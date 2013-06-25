/*****************************************************************************
 * Licensed to Qualys, Inc. (QUALYS) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * QUALYS licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 ****************************************************************************/

/**
 * @file
 * @brief IronBee --- Engine Manager
 *
 * @author Nick LeRoy <nleroy@qualys.com>
 */

#include "ironbee_config_auto.h"

#include <ironbee/engine_manager.h>

#include "engine_manager_private.h"
#include "engine_manager_log_private.h"

#include <ironbee/config.h>
#include <ironbee/context.h>
#include <ironbee/engine.h>
#include <ironbee/engine_manager_testapi.h>
#include <ironbee/list.h>
#include <ironbee/lock.h>
#include <ironbee/log.h>
#include <ironbee/mpool.h>
#include <ironbee/server.h>

#include <assert.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>

/* Local definitions */
static const size_t DEFAULT_MAX_ENGINES = 8;  /**< Default max # of engines */

/**
 * The Engine Manager engine wrapper.
 *
 * There is one wrapper per engine instance.
 */
struct ib_manager_engine_t {
    ib_engine_t  *engine;           /**< The IronBee engine itself */
    uint64_t      ref_count;        /**< Engine's reference count */
};

/**
 * Initialize the locks
 *
 * @param[in] manager Engine manager object
 *
 * @returns Status code:
 * - IB_EALLOC for allocation errors
 * - Any errors returned by ib_lock_init()
 */
static ib_status_t create_locks(
    ib_manager_t *manager
)
{
    assert(manager != NULL);
    ib_status_t rc;

    /* Create the engine list lock */
    rc = ib_lock_init(&manager->engines_lock);
    if (rc != IB_OK) {
        return rc;
    }

    /* Create the engine creation serialization lock */
    rc = ib_lock_init(&manager->creation_lock);
    if (rc != IB_OK) {
        ib_lock_destroy(&manager->engines_lock);
        return rc;
    }

    return IB_OK;
}

/**
 * Destroy the the manager's locks
 *
 * All locks must be unlocked before destroying
 *
 * @param[in] manager Engine manager object
 */
static void destroy_locks(
    ib_manager_t *manager
)
{
    assert(manager != NULL);

    ib_lock_destroy(&manager->engines_lock);
    ib_lock_destroy(&manager->creation_lock);
}

/**
 * Memory pool cleanup function to destroy the locks attached to a manager.
 *
 * @param[in] cbdata Callback data (manager)
 */
void cleanup_locks(
    void *cbdata
)
{
    assert(cbdata != NULL);
    ib_manager_t *manager = (ib_manager_t *)cbdata;
    destroy_locks(manager);
}

/**
 * Set the engine manager's logger functions.
 *
 * Caller must assure exclusive access (e.g. call from manager initialization
 * or with the engine lock held).
 *
 * See ib_manager_create() for documentation on the callback function
 * arguments.
 *
 * @param[in] manager IronBee manager object
 * @param[in] logger_va_fn Logger function (@c va_list version)
 * @param[in] logger_buf_fn Logger function (Formatted buffer version)
 * @param[in] logger_flush_fn Logger flush function (or NULL)
 * @param[in] logger_cbdata Data to pass to logger function
 */
static void set_logger(
    ib_manager_t              *manager,
    ib_manager_log_va_fn_t     logger_va_fn,
    ib_manager_log_buf_fn_t    logger_buf_fn,
    ib_manager_log_flush_fn_t  logger_flush_fn,
    void                      *logger_cbdata
)
{
    assert(manager != NULL);
    assert( (logger_va_fn != NULL) || (logger_buf_fn != NULL) );
    assert( (logger_va_fn == NULL) || (logger_buf_fn == NULL) );

    /* Set the logger info */
    manager->log_va_fn    = logger_va_fn;
    manager->log_buf_fn   = logger_buf_fn;
    manager->log_flush_fn = logger_flush_fn;
    manager->log_cbdata   = logger_cbdata;
}

/**
 * Log the current engine list for debugging.
 *
 * @param[in] manager IronBee engine manager
 * @param[in] level1 Logging level for the count line
 * @param[in] level2 Logging level for individual engines
 * @param[in] label Label for logging
 */
static void log_engines(
    const ib_manager_t *manager,
    ib_log_level_t      level1,
    ib_log_level_t      level2,
    const char         *label
)
{
    /* Log the engine count. */
    if (manager->log_level >= level1) {
        ib_manager_log(manager, level1,
                       "ENGINE MANAGER[%d,%p]: "
                       "%s; engine list (count=%zu, max=%zd)%c",
                       getpid(), manager, label,
                       manager->engine_count, manager->max_engines,
                       (manager->log_level >= level2) ? ':' : ' ');
    }

    /* Log the individual engines. */
    if (manager->log_level >= level2) {
        for (size_t n = 0;  n < manager->engine_count;  ++n) {
            ib_manager_engine_t *engine = manager->engine_list[n];
            if (engine == NULL) {
                ib_manager_log(manager, level2,
                               "ENGINE MANAGER[%d,%p]:  #%zd: %p",
                               getpid(), manager, n, NULL);
            }
            else {
                ib_manager_log(manager, level2,
                               "ENGINE MANAGER[%d,%p]:  #%zd: %p (%sref=%zd)",
                               getpid(), manager, n,
                               engine->engine,
                               ( (engine == manager->engine_current) ?
                                 "current " : "" ),
                               engine->ref_count);
            }
        }
    }
}

/**
 * Destroy IronBee engines
 *
 * This function assumes that the engine list lock has been locked by the
 * caller.
 *
 * Destroy all IronBee engines managed by @a manager.
 *
 * If @a op is IB_ENGINE_DESTROY_INACTIVE, only inactive, non-current engines
 * will be destroyed.  If @a op is IB_ENGINE_DESTROY_NON_CURRENT, all
 * non-current engines will be destroyed.  If @a op is IB_ENGINE_DESTROY_ALL,
 * all engines will be destroyed.
 *
 * @param[in,out] manager IronBee engine manager
 * @param[in] op Destroy operation
 * @param[in] opstr Destroy operation string (for logging)
 *
 * @returns Status code
 *  - IB_OK
 */
static ib_status_t destroy_engines(
    ib_manager_t           *manager,
    ib_manager_destroy_ops  op,
    const char             *opstr
)
{
    assert(manager != NULL);
    size_t destroyed = 0;
    size_t num;
    size_t count;

    /* Log a message */
    ib_manager_log(manager, IB_LOG_INFO,
                   "ENGINE MANAGER[%d,%p]: Destroying engines (%s)",
                   getpid(), manager, opstr);
    log_engines(manager, IB_LOG_INFO, IB_LOG_DEBUG3, "Before destroy");

    /* Destroy all non-current engines with zero reference count */
    for (num = 0;  num < manager->engine_count;  ++num) {
        const ib_manager_engine_t *wrapper = manager->engine_list[num];
        ib_engine_t         *engine = wrapper->engine;
        bool                 is_current = (wrapper == manager->engine_current);
        bool                 is_active = (wrapper->ref_count != 0);
        bool                 destroy = false;

        /* Should we destroy this engine? */
        switch(op) {
        case IB_MANAGER_DESTROY_INACTIVE:
            destroy = ((!is_current) && (!is_active));
            break;
        case IB_MANAGER_DESTROY_ALL:
            destroy = true;
            break;
        default:
            assert(0);
        }
        ib_manager_log(manager, IB_LOG_DEBUG,
                       "ENGINE MANAGER[%d,%p]: "
                       "%s engine %p (%s, %s, ref=%zd)",
                       getpid(),
                       manager,
                       destroy ? "Destroying" : "Not destroying",
                       engine,
                       is_current ? "current" : "non-current",
                       is_active ? "active" : "inactive",
                       wrapper->ref_count);

        /* Destroy the engine */
        if (destroy) {
            ++destroyed;

            /* If it's current, NULL out the current pointer */
            if (wrapper == manager->engine_current) {
                ib_manager_log(manager, IB_LOG_INFO,
                               "ENGINE MANAGER[%d,%p]: Current engine now NULL",
                               getpid(), manager);
                manager->engine_current = NULL;
            }

            /* Note: This will destroy the engine wrapper object, too */
            ib_engine_destroy(engine);
            ib_manager_log(manager, IB_LOG_TRACE,
                           "ENGINE MANAGER[%d,%p]: Destroyed engine %p",
                           getpid(), manager, engine);

            /* NULL out it's place in the list -- we'll consolidate the
             * list at the bottom */
            manager->engine_list[num] = NULL;
        }
    }

    /* Consolidate the list */
    log_engines(manager, IB_LOG_DEBUG3, IB_LOG_TRACE, "Before consolidation");
    count = manager->engine_count;
    for (num = 0;  num < count;  ++num) {
        while ( (manager->engine_count) &&
                (manager->engine_list[num] == NULL) )
        {
            size_t n2;
            for (n2 = 0;  n2 < (count - num - 1);  ++n2) {
                manager->engine_list[num+n2] = manager->engine_list[num+n2+1];
            }
            --(manager->engine_count);
        }
    }
    log_engines(manager, IB_LOG_DEBUG3, IB_LOG_TRACE, "After consolidation");

    /* Update the engine count */
    ib_manager_log(manager, IB_LOG_INFO,
                   "ENGINE MANAGER[%d,%p]: Finished destroying engines "
                   "(op=%s, destroyed=%zu, count=%zu)",
                   getpid(), manager, opstr, destroyed,
                   manager->engine_count);
    log_engines(manager, IB_LOG_DEBUG2, IB_LOG_DEBUG3, "Finished destroy");

    /* Confirm that all were destroyed */
    if ( (op == IB_MANAGER_DESTROY_ALL) && (manager->engine_count != 0) ) {
        log_engines(manager, IB_LOG_ERROR, IB_LOG_WARNING, "Destroy all");
        ib_manager_log_flush(manager);
        assert(manager->engine_count == 0);
    }

    /* By definition, we have no inactive engines now. */
    manager->inactive_count = 0;
    return IB_OK;
}

ib_status_t ib_manager_create(
    const ib_server_t         *server,
    size_t                     max_engines,
    ib_manager_log_va_fn_t     logger_va_fn,
    ib_manager_log_buf_fn_t    logger_buf_fn,
    ib_manager_log_flush_fn_t  logger_flush_fn,
    void                      *logger_cbdata,
    ib_log_level_t             logger_level,
    ib_manager_t             **pmanager
)
{
    assert(server != NULL);
    assert(pmanager != NULL);

    ib_status_t           rc;
    ib_mpool_t           *mpool;
    ib_manager_t         *manager;
    ib_manager_engine_t **engine_list;

    /* Create our memory pool. */
    rc = ib_mpool_create(&mpool, "Engine Manager", NULL);
    if (rc != IB_OK) {
        goto cleanup;
    }

    /* Create the manager object. */
    manager = ib_mpool_calloc(mpool, sizeof(*manager), 1);
    if (manager == NULL) {
        rc = IB_EALLOC;
        goto cleanup;
    }

    /* Create the engine list. */
    if (max_engines == 0) {
        max_engines = DEFAULT_MAX_ENGINES;
    }
    engine_list = ib_mpool_calloc(mpool,
                                  max_engines,
                                  sizeof(ib_manager_engine_t *));
    if (engine_list == NULL) {
        rc = IB_EALLOC;
        goto cleanup;
    }

    /* Create the locks */
    rc = create_locks(manager);
    if (rc != IB_OK) {
        goto cleanup;
    }

    /* Cleanup locks when our memory pool is destroyed */
    rc = ib_mpool_cleanup_register(mpool, cleanup_locks, manager);
    if (rc != IB_OK) {
        destroy_locks(manager);
        goto cleanup;
    }

    /* Populate the manager object. */
    manager->server         = server;
    manager->mpool          = mpool;
    manager->engine_list    = engine_list;
    manager->max_engines    = max_engines;
    manager->log_level      = logger_level;

    /* Set the logger */
    set_logger(manager,
               logger_va_fn, logger_buf_fn, logger_flush_fn,
               logger_cbdata);

    /* Log */
    ib_manager_log(manager, IB_LOG_INFO,
                   "ENGINE MANAGER[%d,%p]: Manager created",
                   getpid(), manager);

    /* Hand the new manager off to the caller. */
    *pmanager = manager;

    /* We haven't acquired any locks, so we can just return. */
    return IB_OK;

cleanup:
    if (mpool != NULL) {
        ib_mpool_destroy(mpool);
    }

    return rc;
}

ib_status_t ib_manager_destroy(
    ib_manager_t *manager
)
{
    ib_status_t  rc;

    ib_manager_log(manager, IB_LOG_INFO,
                   "ENGINE MANAGER[%d,%p]: Destroy manager",
                   getpid(), manager);

    /* Destroy engines */
    rc = destroy_engines(manager, IB_MANAGER_DESTROY_ALL, "all");
    if (rc != IB_OK) {
        goto cleanup;
    }

    /* Any engines not destroyed? */
    if (manager->engine_count != 0) {
        log_engines(manager, IB_LOG_ERROR, IB_LOG_WARNING, "Destroy manager");
        ib_manager_log_flush(manager);
        assert(manager->engine_count == 0);
    }

    /* Done */
    ib_manager_log(manager, IB_LOG_INFO,
                   "ENGINE MANAGER[%d,%p]: Destroying manager",
                   getpid(), manager);

    /* Destroy the manager by destroying it's memory pool. */
    ib_mpool_destroy(manager->mpool);

    /* Note: Locks are destroyed by the memory pool cleanup */

    /* Done */
    return IB_OK;

cleanup:
    ib_manager_log(manager, IB_LOG_NOTICE,
                   "ENGINE MANAGER[%d,%p]: Not destroying manager: %s",
                   getpid(), manager, ib_status_to_string(rc));
    return rc;
}

/**
 * Register an engine.
 *
 * Add @a engine to @a manager's engine list.  The caller is required to have
 * acquired the creation lock to serialize engine creation.
 *
 * @note This function acquires and releases engine list lock.
 *
 * @param[in] manager Engine manager
 * @param[in] engine Engine wrapper object
 *
 */
static ib_status_t register_engine(
    ib_manager_t        *manager,
    ib_manager_engine_t *engine
)
{
    assert(manager != NULL);
    assert(engine != NULL);

    ib_status_t rc;

    /*
     * We need the engine list lock to manipulate engine list, etc.
     */
    rc = ib_lock_lock(&manager->engines_lock);
    if (rc != IB_OK) {
        return rc;
    }

    /* Because of the creation lock, we should always have another slot */
    if (manager->engine_count >= manager->max_engines) {
        log_engines(manager, IB_LOG_ERROR, IB_LOG_WARNING, "Register engine");
        ib_manager_log_flush(manager);
        assert(manager->engine_count < manager->max_engines);
    }

    /* Store it in the list */
    manager->engine_list[manager->engine_count] = engine;
    ++(manager->engine_count);

    /* Make this engine current */
    manager->engine_current = engine;
    ib_manager_log(manager, IB_LOG_INFO,
                   "ENGINE MANAGER[%d,%p]: Current IronBee engine -> %p",
                   getpid(), manager, engine->engine);
    log_engines(manager, IB_LOG_DEBUG, IB_LOG_DEBUG2, "Created engine");

    /* Destroy all non-current engines with zero reference count */
    if (manager->engine_count > 1) {
        destroy_engines(manager, IB_MANAGER_DESTROY_INACTIVE, "INACTIVE");
    }

    ib_lock_unlock(&manager->engines_lock);
    return rc;
}

ib_status_t ib_manager_engine_create(
    ib_manager_t  *manager,
    const char    *config_file
)
{
    assert(manager != NULL);
    assert(config_file != NULL);

    ib_status_t          rc;
    ib_status_t          rc2;
    ib_cfgparser_t      *parser = NULL;
    ib_context_t        *ctx;
    ib_engine_t         *engine = NULL;
    ib_manager_engine_t *wrapper;

    /* Grab the engine creation lock to serialize engine creation. */
    rc = ib_lock_lock(&manager->creation_lock);
    if (rc != IB_OK) {
        goto cleanup;
    }

    /* Are we already at the max # of engines? */
    if (manager->engine_count == manager->max_engines) {
        rc = IB_DECLINED;
        ib_manager_log(manager, IB_LOG_DEBUG,
                       "ENGINE MANAGER[%d,%p]: "
                       "Not creating engine because limit reached "
                       "(count=%zd, limit=%zd)",
                       getpid(), manager,
                       manager->engine_count, manager->max_engines);
        log_engines(manager, IB_LOG_DEBUG3, IB_LOG_TRACE, "Limit encountered");
        goto cleanup;
    }

    /* Sanity check */
    if (manager->engine_count >= manager->max_engines) {
        log_engines(manager, IB_LOG_ERROR, IB_LOG_WARNING, "Engine create");
        ib_manager_log_flush(manager);
        assert(manager->engine_count < manager->max_engines);
    }

    ib_manager_log(manager, IB_LOG_INFO,
                   "ENGINE MANAGER[%d,%p]: Creating IronBee engine "
                   "with configuration file \"%s\"",
                   getpid(), manager, config_file);

    /* Create the engine */
    rc = ib_engine_create(&engine, manager->server);
    if (rc != IB_OK) {
        goto cleanup;
    }

    /* Allocate an engine wrapper from the new engine's memory pool */
    wrapper = ib_mpool_calloc(ib_engine_pool_main_get(engine),
                              sizeof(*wrapper), 1);
    if (wrapper == NULL) {
        goto cleanup;
    }

    /* The basic engine is created */
    ib_manager_log(manager, IB_LOG_INFO,
                   "ENGINE MANAGER[%d,%p]: Created IronBee engine %p",
                   getpid(), manager, engine);

    /* Set the engine's logger function */
    ib_log_set_logger_fn(engine, ib_engine_manager_logger, manager);

    /* This creates the main context */
    rc = ib_cfgparser_create(&parser, engine);
    if (rc != IB_OK) {
        ib_manager_log(manager, IB_LOG_ERROR,
                       "ENGINE MANAGER[%d,%p]: "
                       "Failed to create parser for engine %p: %s",
                       getpid(), manager, engine, ib_status_to_string(rc));
        goto cleanup;
    }

    /* Tell the engine about the new parser */
    rc = ib_engine_config_started(engine, parser);
    if (rc != IB_OK) {
        ib_manager_log(manager, IB_LOG_ERROR,
                       "ENGINE MANAGER[%d,%p]: "
                       "Failed to start configuration for engine %p: %s",
                       getpid(), manager, engine, ib_status_to_string(rc));
        goto cleanup;
    }

    /* Get the main context, set some defaults */
    ctx = ib_context_main(engine);
    ib_context_set_num(ctx, "logger.log_level", (ib_num_t)IB_LOG_WARNING);

    /* Parse the configuration */
    rc = ib_cfgparser_parse(parser, config_file);
    if (rc != IB_OK) {
        ib_manager_log(manager, IB_LOG_ERROR,
                       "ENGINE MANAGER[%d,%p]: "
                       "Failed to parse configuration \"%s\" for engine %p: %s",
                       getpid(), manager,
                       config_file, engine, ib_status_to_string(rc));
    }

    /* Report the status to the engine */
    rc2 = ib_engine_config_finished(engine);
    if (rc2 != IB_OK) {
        ib_manager_log(manager, IB_LOG_ERROR,
                       "ENGINE MANAGER[%d,%p]: "
                       "Failed to finish configuration for engine %p: %s",
                       getpid(), manager, engine, ib_status_to_string(rc));
    }
    if ( (rc2 != IB_OK) && (rc == IB_OK) ) {
        rc = rc2;
    }

    /* All ok? */
    if (rc != IB_OK) {
        goto cleanup;
    }

    /* Fill in the wrapper */
    wrapper->engine = engine;
    wrapper->ref_count = 0;

    /* Register the engine */
    rc = register_engine(manager, wrapper);

cleanup:
    /* If a parser was created, destroy it */
    if (parser != NULL) {
        ib_cfgparser_destroy(parser);
    }

    /* If something failed, destroy the engine that may have been created */
    if (rc != IB_OK) {
        ib_manager_log(manager, IB_LOG_ERROR,
                       "ENGINE MANAGER[%d,%p]: "
                       "Failed to create IronBee engine: %s",
                       getpid(), manager, ib_status_to_string(rc));
        if (engine != NULL) {
            ib_engine_destroy(engine);
            engine = NULL;
        }
    }

    /* Release any locks. */
    ib_lock_unlock(&manager->creation_lock);

    return rc;
}

ib_status_t ib_manager_engine_acquire(
    ib_manager_t  *manager,
    ib_engine_t  **pengine
)
{
    assert(manager != NULL);
    assert(pengine != NULL);

    ib_status_t          rc;
    ib_manager_engine_t *engine = NULL;

    /* Grab the engine list lock */
    rc = ib_lock_lock(&manager->engines_lock);
    if (rc != IB_OK) {
        return rc;
    }

    /* Get the current engine; If there is no current engine, decline. */
    engine = manager->engine_current;
    if (engine == NULL) {
        rc = IB_DECLINED;
        goto cleanup;
    }

    /* Increment the reference count */
    ++(engine->ref_count);
    rc = IB_OK;

    /* No need to update the inactive count; the current engine is
     * never inactive */
    *pengine = engine->engine;

cleanup:
    ib_manager_log(manager, IB_LOG_TRACE,
                   "ENGINE MANAGER[%d,%p]: "
                   "Acquire engine %p [ref=%"PRIu64"]: %s",
                   getpid(),
                   manager,
                   (engine == NULL) ? NULL : engine->engine,
                   (engine == NULL) ? 0 : engine->ref_count,
                   ib_status_to_string(rc));

    /* Release any locks */
    ib_lock_unlock(&manager->engines_lock);
    return rc;
}

ib_status_t ib_manager_engine_release(
    ib_manager_t *manager,
    ib_engine_t  *engine
)
{
    assert(manager != NULL);
    assert(engine != NULL);

    ib_status_t           rc;
    ib_manager_engine_t  *engptr = NULL;
    size_t                num;
    size_t                inactive = 0;

    /* Grab the engine list lock */
    rc = ib_lock_lock(&manager->engines_lock);
    if (rc != IB_OK) {
        return rc;
    }

    /* Is this the current engine? */
    if ( (manager->engine_current != NULL) &&
         (engine == manager->engine_current->engine) )
    {
        if (manager->engine_current->ref_count == 0) {
            log_engines(manager, IB_LOG_ERROR, IB_LOG_WARNING,
                        "Engine release");
            ib_manager_log_flush(manager);
            assert(manager->engine_current->ref_count > 0);
        }
        engptr = manager->engine_current;
        --(engptr->ref_count);
        goto cleanup;
    }

    /*
     * This engine being released is not the current engine.  Walk through the
     * list of engines of known engines, searching for a match.  While
     * iterating through the list, count the number of inactive engines that
     * we encounter.
     */
    for (num = 0;  num < manager->engine_count;  ++num) {
        ib_manager_engine_t *cur = manager->engine_list[num];

        /* If this is the engine we're looking for, save the pointer. */
        if (engine == cur->engine) {
            engptr = cur;
        }

        /* Otherwise, check to see if it's active (current is never inactive) */
        else if ( (cur != manager->engine_current) &&
                  (cur->ref_count == 0) )
        {
            ++inactive;
        }
    }

    /* Something is *very* wrong if we don't have this engine in our list! */
    if(engptr == NULL) {
        ib_manager_log(manager, IB_LOG_CRITICAL,
                       "ENGINE MANAGER[%d,%p]: "
                       "Release engine %p: engine not found",
                       getpid(), manager, engine);
        log_engines(manager, IB_LOG_ERROR, IB_LOG_WARNING, "Can't find engine");
        ib_manager_log_flush(manager);
        assert(engptr != NULL);
    }

    /* Decrement the reference count. */
    if (engptr->ref_count == 0) {
        log_engines(manager, IB_LOG_ERROR, IB_LOG_WARNING, "Engine release");
        ib_manager_log_flush(manager);
        assert(engptr->ref_count > 0);
    }
    --(engptr->ref_count);

    /* If we hit zero, update the inactive count. */
    if (engptr->ref_count == 0) {
        ++inactive;
    }

    /* Store off the inactive count now that we're done walking */
    manager->inactive_count = inactive;

cleanup:
    /* Log while we have the log */
    ib_manager_log(manager, IB_LOG_TRACE,
                   "ENGINE MANAGER[%d,%p]: "
                   "Release engine %p [ref=%"PRIu64"] inactive=%zd: %s",
                   getpid(),
                   manager,
                   engine,
                   engptr == NULL ? 0 : engptr->ref_count,
                   manager->inactive_count,
                   ib_status_to_string(rc));

    /* Release any locks */
    ib_lock_unlock(&manager->engines_lock);
    return rc;
}

ib_status_t ib_manager_disable_current(
    ib_manager_t *manager
)
{
    assert(manager != NULL);

    ib_status_t rc;

    /* Grab the engine list lock */
    rc = ib_lock_lock(&manager->engines_lock);
    if (rc != IB_OK) {
        return rc;
    }

    /* Log what we're doing. */
    ib_manager_log(manager, IB_LOG_DEBUG,
                   "ENGINE MANAGER[%d]: Disabling current engine %p",
                   getpid(), manager->engine_current);

    /* If this makes an otherwise active engine become inactive, increment the
     * manager's inactive count */
    if ( (manager->engine_current != NULL) &&
         (manager->engine_current->ref_count == 0) )
    {
        ++(manager->inactive_count);
    }

    /* NULL the current engine */
    manager->engine_current = NULL;

    /* Done */
    ib_lock_unlock(&manager->engines_lock);
    return rc;
}

ib_status_t ib_manager_destroy_engines(
    ib_manager_t           *manager,
    ib_manager_destroy_ops  op,
    size_t                 *pcount
)
{
    assert(manager != NULL);
    assert( (op == IB_MANAGER_DESTROY_INACTIVE) ||
            (op == IB_MANAGER_DESTROY_ALL) );
    ib_status_t  rc = IB_OK;
    const char  *opstr;

    /* Grab the engine list lock */
    rc = ib_lock_lock(&manager->engines_lock);
    if (rc != IB_OK) {
        return rc;
    }

    /*
     * If this is a cleanup, and we have no inactive engines to cleanup,
     * do nothing.  Otherwise, grab the engine list lock, and go to town.
     */
    if ( (op == IB_MANAGER_DESTROY_INACTIVE) &&
         (manager->inactive_count == 0) )
    {
        goto cleanup;
    }

    /* Get the operation string */
    opstr = (op == IB_MANAGER_DESTROY_ALL) ? "ALL" : "INACTIVE";
    ib_manager_log(manager, IB_LOG_DEBUG,
                   "ENGINE MANAGER[%d,%p]: Destroy engines (op=%s)",
                   getpid(), manager, opstr);

    /* Destroy engines */
    rc = destroy_engines(manager, op, opstr);

    /* Done */
    ib_manager_log(manager, IB_LOG_DEBUG,
                   "ENGINE MANAGER[%d,%p]: "
                   "Destroy engines (op=%s count=%zd): %s",
                   getpid(),
                   manager,
                   opstr,
                   manager->engine_count,
                   ib_status_to_string(rc));

cleanup:
    /* Return the count to the caller if requested */
    if (pcount != NULL) {
        *pcount = manager->engine_count;
    }

    /* Release the engine list lock */
    ib_lock_unlock(&manager->engines_lock);

    return rc;
}

ib_status_t ib_manager_engine_cleanup(
    ib_manager_t  *manager
)
{
    assert(manager != NULL);
    ib_status_t rc;

    /* If the no inactive engines, do nothing */
    if (manager->inactive_count == 0) {
        return IB_OK;
    }

    rc = ib_manager_destroy_engines(manager, IB_MANAGER_DESTROY_INACTIVE, NULL);
    ib_manager_log(manager, IB_LOG_INFO,
                   "ENGINE MANAGER[%d,%p]: Cleanup engines [count=%zu]: %s",
                   getpid(), manager,
                   manager->engine_count,
                   ib_status_to_string(rc));
    return rc;
}

size_t ib_manager_engine_count(
    ib_manager_t *manager
)
{
    assert(manager != NULL);
    return manager->engine_count;
}

size_t ib_manager_engine_count_inactive(
    ib_manager_t *manager
)
{
    assert(manager != NULL);
    return manager->inactive_count;
}

void ib_manager_set_logger(
    ib_manager_t              *manager,
    ib_manager_log_va_fn_t     logger_va_fn,
    ib_manager_log_buf_fn_t    logger_buf_fn,
    ib_manager_log_flush_fn_t  logger_flush_fn,
    void                      *logger_cbdata
)
{
    assert(manager != NULL);

    /* set_logger() does the real work */
    set_logger(manager,
               logger_va_fn, logger_buf_fn, logger_flush_fn,
               logger_cbdata);
}
