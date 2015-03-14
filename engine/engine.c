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
 * @brief IronBee
 *
 * @author Brian Rectanus <brectanus@qualys.com>
 * @author Christopher Alfeld <calfeld@qualys.com>
 * @author Nick LeRoy <nleroy@qualys.com>
 */

#include "ironbee_config_auto.h"

#include <ironbee/engine.h>
#include "engine_private.h"

#include "core_private.h"
#include "module_private.h"
#include "rule_engine_private.h"
#include "state_notify_private.h"

#include <ironbee/array.h>
#include <ironbee/cfgmap.h>
#include <ironbee/context.h>
#include <ironbee/context_selection.h>
#include <ironbee/core.h>
#include <ironbee/engine_state.h>
#include <ironbee/flags.h>
#include <ironbee/hash.h>
#include <ironbee/ip.h>
#include <ironbee/mm_mpool.h>
#include <ironbee/module.h>
#include <ironbee/server.h>
#include <ironbee/state_notify.h>
#include <ironbee/stream_processor.h>
#include <ironbee/stream_pump.h>
#include <ironbee/string.h>
#include <ironbee/util.h>

#include <assert.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


/* -- Constants -- */

/** Constant max path used in calls to getcwd() */
static const size_t maxpath = 512;

/** Constant String Values */
const ib_default_string_t ib_default_string = {
    "",          /* empty */
    "unknown",   /* unknown */
    "core",      /* core */
    "/",         /* root_path */
    "/",         /* uri_root_path */
};

static const char *default_auditlog_index = "ironbee-index.log";

/**
 * Mapping of valid rule logging names to flag values.
 */
static IB_STRVAL_MAP(ib_tx_flags_map) = {
    IB_STRVAL_PAIR("None", IB_TX_FNONE),
    IB_STRVAL_PAIR("HTTP/0.9", IB_TX_FHTTP09),
    IB_STRVAL_PAIR("Pipelined", IB_TX_FPIPELINED),

    IB_STRVAL_PAIR("Request Started", IB_TX_FREQ_STARTED),
    IB_STRVAL_PAIR("Request Line", IB_TX_FREQ_LINE),
    IB_STRVAL_PAIR("Request Header", IB_TX_FREQ_HEADER),
    IB_STRVAL_PAIR("Request Body", IB_TX_FREQ_BODY),
    IB_STRVAL_PAIR("Request Trailer", IB_TX_FREQ_TRAILER),
    IB_STRVAL_PAIR("Request Finished", IB_TX_FREQ_FINISHED),
    IB_STRVAL_PAIR("Request Has Data", IB_TX_FREQ_HAS_DATA),

    IB_STRVAL_PAIR("Response Started", IB_TX_FRES_STARTED),
    IB_STRVAL_PAIR("Response Line", IB_TX_FRES_LINE),
    IB_STRVAL_PAIR("Response Header", IB_TX_FRES_HEADER),
    IB_STRVAL_PAIR("Response Body", IB_TX_FRES_BODY),
    IB_STRVAL_PAIR("Response Trailer", IB_TX_FRES_TRAILER),
    IB_STRVAL_PAIR("Response Finished", IB_TX_FRES_FINISHED),
    IB_STRVAL_PAIR("Response Has Data", IB_TX_FRES_HAS_DATA),

    IB_STRVAL_PAIR("Logging", IB_TX_FLOGGING),
    IB_STRVAL_PAIR("Post-Process", IB_TX_FPOSTPROCESS),

    IB_STRVAL_PAIR("Error", IB_TX_FERROR),
    IB_STRVAL_PAIR("Suspicious", IB_TX_FSUSPICIOUS),

    IB_STRVAL_PAIR("Inspect Request URI", IB_TX_FINSPECT_REQURI),
    IB_STRVAL_PAIR("Inspect Request Parameters", IB_TX_FINSPECT_REQPARAMS),
    IB_STRVAL_PAIR("Inspect Request Header", IB_TX_FINSPECT_REQHDR),
    IB_STRVAL_PAIR("Inspect Request Body", IB_TX_FINSPECT_REQBODY),
    IB_STRVAL_PAIR("Inspect Response Header", IB_TX_FINSPECT_RESHDR),
    IB_STRVAL_PAIR("Inspect Response Body", IB_TX_FINSPECT_RESBODY),

    IB_STRVAL_PAIR("Blocking Mode", IB_TX_FBLOCKING_MODE),
    IB_STRVAL_PAIR("Block: Advisory", IB_TX_FBLOCK_ADVISORY),
    IB_STRVAL_PAIR("Block: Phase", IB_TX_FBLOCK_PHASE),
    IB_STRVAL_PAIR("Block: Immediate", IB_TX_FBLOCK_IMMEDIATE),
    IB_STRVAL_PAIR("Allow: Phase", IB_TX_FALLOW_PHASE),
    IB_STRVAL_PAIR("Allow: Request", IB_TX_FALLOW_REQUEST),
    IB_STRVAL_PAIR("Allow: All", IB_TX_FALLOW_ALL),

    /* End */
    IB_STRVAL_PAIR_LAST
};

/* -- Internal Structures -- */
typedef struct {
    ib_state_t            state;      /**< State type */
    const char           *state_name; /**< State name */
    ib_state_hook_type_t  hook_type;  /**< Hook's type */
} ib_state_data_t;

/**
 * List of callback data types for state id to type lookups.
 */
static ib_state_data_t ib_state_table[IB_STATE_NUM];

/**
 * Initialize the state table entry for @a state
 *
 * @note This is done as a macro take advantage of IB_STRINGIFY(),
 * and invokes init_state_table_entry().
 *
 * @param[in] state State (ib_state_t) to initialize
 * @param[in] hook_type The associated hook type
 */
#define INIT_STATE_TABLE_ENT(state, hook_type)              \
    init_state_table_entry((state), IB_STRINGIFY(state), hook_type)

/* -- Internal Routines -- */
/**
 * Initialize the state table entry for @a state.
 *
 * @param[in] state State to initialize
 * @param[in] state_name Name of @a state
 * @param[in] hook_type The associated hook type
 */
static
void init_state_table_entry(
    ib_state_t            state,
    const char           *state_name,
    ib_state_hook_type_t  hook_type
)
{
    assert(state < IB_STATE_NUM);
    assert(state_name != NULL);

    ib_state_data_t *ent = &ib_state_table[state];
    ent->state = state;
    ent->state_name = state_name;
    ent->hook_type = hook_type;
}

ib_status_t ib_hook_check(
    ib_engine_t* ib,
    ib_state_t state,
    ib_state_hook_type_t hook_type
)
{
    static const size_t num_states =
        sizeof(ib_state_table) / sizeof(ib_state_data_t);
    ib_state_hook_type_t expected_hook_type;

    if (state >= num_states) {
        ib_log_error( ib,
            "State/hook mismatch: Unknown state: %d", state
        );
        return IB_EINVAL;
    }

    expected_hook_type = ib_state_table[state].hook_type;
    if (expected_hook_type != hook_type) {
        ib_log_debug(ib,
                     "State/hook mismatch: "
                     "State type %s expected %d but received %d",
                     ib_state_name(state),
                     expected_hook_type, hook_type);
        return IB_EINVAL;
    }

    return IB_OK;
}

static ib_status_t ib_hook_register(
    ib_engine_t *ib,
    ib_state_t state,
    ib_hook_t *hook
)
{
    assert(ib != NULL);

    ib_status_t           rc;
    ib_list_t            *list;

    list = ib->hooks[state];
    assert(list != NULL);

    /* Insert the hook at the end of the list */
    rc = ib_list_push(list, hook);
    if (rc != IB_OK) {
        return rc;
    }

    return IB_OK;
}

/**
 * Initialize the IronBee state table.
 *
 * @note Asserts if error detected.
 *
 * @returns Status code:
 *    - IB_OK All OK
 */
static ib_status_t ib_state_table_init(void)
{
    ib_state_t state;
    static bool           table_initialized = false;

    if (table_initialized) {
        goto validate;
    };

    memset(ib_state_table, 0, sizeof(ib_state_table));

    /* Engine States */
    INIT_STATE_TABLE_ENT(conn_started_state, IB_STATE_HOOK_CONN);
    INIT_STATE_TABLE_ENT(conn_finished_state, IB_STATE_HOOK_CONN);
    INIT_STATE_TABLE_ENT(tx_started_state, IB_STATE_HOOK_TX);
    INIT_STATE_TABLE_ENT(tx_process_state, IB_STATE_HOOK_TX);
    INIT_STATE_TABLE_ENT(tx_finished_state, IB_STATE_HOOK_TX);

    /* Handler States */
    INIT_STATE_TABLE_ENT(handle_context_conn_state, IB_STATE_HOOK_CONN);
    INIT_STATE_TABLE_ENT(handle_connect_state, IB_STATE_HOOK_CONN);
    INIT_STATE_TABLE_ENT(handle_context_tx_state, IB_STATE_HOOK_TX);
    INIT_STATE_TABLE_ENT(handle_request_header_state, IB_STATE_HOOK_TX);
    INIT_STATE_TABLE_ENT(handle_request_state, IB_STATE_HOOK_TX);
    INIT_STATE_TABLE_ENT(handle_response_header_state, IB_STATE_HOOK_TX);
    INIT_STATE_TABLE_ENT(handle_response_state, IB_STATE_HOOK_TX);
    INIT_STATE_TABLE_ENT(handle_disconnect_state, IB_STATE_HOOK_CONN);
    INIT_STATE_TABLE_ENT(handle_postprocess_state, IB_STATE_HOOK_TX);
    INIT_STATE_TABLE_ENT(handle_logging_state, IB_STATE_HOOK_TX);

    /* Server States */
    INIT_STATE_TABLE_ENT(conn_opened_state, IB_STATE_HOOK_CONN);
    INIT_STATE_TABLE_ENT(conn_closed_state, IB_STATE_HOOK_CONN);

    /* Parser States */
    INIT_STATE_TABLE_ENT(request_started_state, IB_STATE_HOOK_REQLINE);
    INIT_STATE_TABLE_ENT(request_header_data_state, IB_STATE_HOOK_HEADER);
    INIT_STATE_TABLE_ENT(request_header_process_state, IB_STATE_HOOK_TX);
    INIT_STATE_TABLE_ENT(request_header_finished_state, IB_STATE_HOOK_TX);
    INIT_STATE_TABLE_ENT(request_body_data_state, IB_STATE_HOOK_TXDATA);
    INIT_STATE_TABLE_ENT(request_finished_state, IB_STATE_HOOK_TX);
    INIT_STATE_TABLE_ENT(response_started_state, IB_STATE_HOOK_RESPLINE);
    INIT_STATE_TABLE_ENT(response_header_data_state, IB_STATE_HOOK_HEADER);
    INIT_STATE_TABLE_ENT(response_header_finished_state, IB_STATE_HOOK_TX);
    INIT_STATE_TABLE_ENT(response_body_data_state, IB_STATE_HOOK_TXDATA);
    INIT_STATE_TABLE_ENT(response_finished_state, IB_STATE_HOOK_TX);

    /* Context States */
    INIT_STATE_TABLE_ENT(context_open_state, IB_STATE_HOOK_CTX);
    INIT_STATE_TABLE_ENT(context_close_state, IB_STATE_HOOK_CTX);
    INIT_STATE_TABLE_ENT(context_destroy_state, IB_STATE_HOOK_CTX);

    /* Engine States */
    INIT_STATE_TABLE_ENT(engine_shutdown_initiated_state, IB_STATE_HOOK_NULL);

    /* Sanity check the table, make sure all states are initialized */
validate:
    for(state = conn_started_state; state < IB_STATE_NUM; ++state) {
        assert(ib_state_table[state].state == state);
        assert(ib_state_table[state].hook_type != IB_STATE_HOOK_INVALID);
        assert(ib_state_table[state].state_name != NULL);
    }

    table_initialized = true;
    return IB_OK;
}

/* -- Main Engine Routines -- */
ib_status_t ib_initialize(void)
{
    ib_status_t rc;

    /* Initialize the utility library */
    rc = ib_util_initialize();
    if (rc != IB_OK) {
        return rc;
    }

    /* Initialize the state table */
    rc = ib_state_table_init();
    if (rc != IB_OK) {
        return rc;
    }

    return IB_OK;

}

ib_status_t ib_shutdown(void)
{
    /* Shut down the utility library */
    ib_util_shutdown();

    return IB_OK;
}

const char *ib_engine_version(void) {
    return IB_VERSION;
}

const char *ib_engine_product_name(void) {
    return IB_PRODUCT_VERSION_NAME;
}

uint32_t ib_engine_version_number(void) {
    return IB_VERNUM;
}

uint32_t ib_engine_abi_number(void) {
    return IB_ABINUM;
}

ib_status_t ib_engine_create(ib_engine_t **pib,
                             const ib_server_t *server)
{
    ib_mpool_t *pool;
    ib_status_t rc;
    ib_state_t state;
    ib_engine_t *ib = NULL;
    ib_mm_t mm;

    /* Create primary memory pool */
    rc = ib_mpool_create(&pool, "engine", NULL);
    if (rc != IB_OK) {
        rc = IB_EALLOC;
        goto failed;
    }
    mm = ib_mm_mpool(pool);

    /* Create the main structure in the primary memory pool */
    ib = (ib_engine_t *)ib_mm_calloc(mm, 1, sizeof(*ib));
    if (ib == NULL) {
        rc = IB_EALLOC;
        goto failed;
    }
    ib->mp = pool;

    /* Set the logger. */
    rc = ib_logger_create(&(ib->logger), IB_LOG_INFO, mm);
    if (rc != IB_OK) {
        return rc;
    }

    /* Create temporary memory pool */
    rc = ib_mpool_create(&(ib->temp_mp),
                         "temp",
                         ib->mp);
    if (rc != IB_OK) {
        goto failed;
    }

    /* Create the config memory pool */
    rc = ib_mpool_create(&(ib->config_mp),
                         "config",
                         ib->mp);
    if (rc != IB_OK) {
        goto failed;
    }

    /* Create a list to hold config contexts */
    rc = ib_list_create(&(ib->contexts), ib_engine_mm_main_get(ib));
    if (rc != IB_OK) {
        goto failed;
    }

    /* Create an engine config context and use it as the
     * main context until the engine can be configured.
     */
    rc = ib_context_create(ib, NULL, IB_CTYPE_ENGINE,
                           "engine", "engine", &(ib->ectx));
    if (rc != IB_OK) {
        goto failed;
    }
    /* Set the context's CWD */
    rc = ib_context_set_cwd(ib->ectx, NULL);
    if (rc != IB_OK) {
        goto failed;
    }

    ib->ctx = ib->ectx;
    ib->cfg_state = CFG_NOT_STARTED;

    /* Check server for ABI compatibility with this engine */
    if (server == NULL) {
        ib_log_error(ib,  "Error creating engine: server info required");
        rc = IB_EINVAL;
        goto failed;
    }
    if (server->vernum > IB_VERNUM) {
        ib_log_alert(ib,
                     "Server %s (built against engine version %s) is not "
                     "compatible with this engine (version %s): "
                     "ABI %d > %d",
                     server->filename, server->version, IB_VERSION,
                     server->abinum, IB_ABINUM);
        rc = IB_EINCOMPAT;
        goto failed;
    }
    ib->server = server;

    /* Sensor info. */
    ib->sensor_name = IB_DSTR_UNKNOWN;
    ib->sensor_version = IB_PRODUCT_VERSION_NAME;
    ib->sensor_hostname = IB_DSTR_UNKNOWN;

    /* Create the instance UUID */
    rc = ib_uuid_create_v4(ib->instance_id);
    if (rc != IB_OK) {
        return rc;
    }

    /* Create an array to hold loaded modules */
    /// @todo Need good defaults here
    rc = ib_array_create(&(ib->modules), mm, 16, 8);
    if (rc != IB_OK) {
        goto failed;
    }

    /* Create a hash to hold configuration directive mappings by name */
    rc = ib_hash_create_nocase(&(ib->dirmap), mm);
    if (rc != IB_OK) {
        goto failed;
    }

    /* Create a hash to hold transformations by name */
    rc = ib_hash_create_nocase(&(ib->tfns), mm);
    if (rc != IB_OK) {
        goto failed;
    }

    /* Create a hash to hold operators by name */
    rc = ib_hash_create_nocase(&(ib->operators), mm);
    if (rc != IB_OK) {
        goto failed;
    }

    /* Create a hash to hold stream operators by name */
    rc = ib_hash_create_nocase(&(ib->stream_operators), mm);
    if (rc != IB_OK) {
        goto failed;
    }

    /* Create a hash to hold actions by name */
    rc = ib_hash_create_nocase(&(ib->actions), mm);
    if (rc != IB_OK) {
        goto failed;
    }

    /* Create the stream processor registry. */
    rc = ib_stream_processor_registry_create(
        &ib->stream_processor_registry,
        mm);
    if (rc != IB_OK) {
        return rc;
    }

    /* Initialize the hook lists */
    for (state = conn_started_state; state < IB_STATE_NUM; ++state) {
        rc = ib_list_create(&(ib->hooks[state]), mm);
        if (rc != IB_OK) {
            goto failed;
        }
    }

    /* Initialize the data configuration. */
    rc = ib_var_config_acquire(&ib->var_config, mm);
    if (rc != IB_OK) {
        ib_log_alert(ib, "Error creating var configuration: %s",
                     ib_status_to_string(rc));
        goto failed;
    }

    /* Initialize logevent callback list. */
    rc = ib_list_create(&ib->logevent_handlers, mm);
    if (rc != IB_OK) {
        goto failed;
    }

    /* Initialize block pre-hooks list. */
    rc = ib_list_create(&(ib->block_pre_hooks), mm);
    if (rc != IB_OK) {
        goto failed;
    }

    /* Initialize block post-hooks list. */
    rc = ib_list_create(&(ib->block_post_hooks), mm);
    if (rc != IB_OK) {
        goto failed;
    }

    /* Initialize the core static module. */
    /// @todo Probably want to do this in a less hard-coded manner.
    rc = ib_module_register(ib_core_module_sym(), ib);
    if (rc != IB_OK) {
        ib_log_alert(ib, "Error registering core module: %s",
                     ib_status_to_string(rc));
        goto failed;
    }

    /* Initialize the rule engine */
    rc = ib_rule_engine_init(ib);
    if (rc != IB_OK) {
        ib_log_alert(ib, "Error initializing rule engine: %s",
                     ib_status_to_string(rc));
        goto failed;
    }



    /* Kick off internal configuration of then engine context. */
    rc = ib_context_open(ib->ectx);
    if (rc != IB_OK) {
        goto failed;
    }
    rc = ib_context_close(ib->ectx);
    if (rc != IB_OK) {
        goto failed;
    }

    *pib = ib;

    ib_log_ex(ib, IB_LOG_INFO, NULL, NULL, 0,
              "%s: Starting", IB_PRODUCT_VERSION_NAME);

    return rc;

failed:
    /* Make sure everything is cleaned up on failure */
    if (ib != NULL) {
        ib_engine_pool_destroy(ib, ib->mp);
    }
    ib = NULL;

    return rc;
}

const ib_server_t *ib_engine_server_get(const ib_engine_t *ib)
{
    assert(ib != NULL);

    return ib->server;
}

ib_logger_t* ib_engine_logger_get(const ib_engine_t *ib)
{
    assert(ib != NULL);
    assert(ib->logger != NULL);

    return ib->logger;
}

/* Create a main context to operate in. */
ib_status_t ib_engine_context_create_main(ib_engine_t *ib)
{
    ib_context_t *ctx;
    ib_status_t rc;

    rc = ib_context_create(ib, ib->ectx, IB_CTYPE_MAIN, "main", "main", &ctx);
    if (rc != IB_OK) {
        return rc;
    }

    /* Set the context's CWD */
    rc = ib_context_set_cwd(ctx, NULL);
    if (rc != IB_OK) {
        return rc;
    }

    ib->ctx = ctx;

    return IB_OK;
}

ib_stream_processor_registry_t *ib_engine_stream_processor_registry(
    ib_engine_t *ib
)
{
    assert(ib != NULL);
    assert(ib->stream_processor_registry != NULL);

    return ib->stream_processor_registry;
}

ib_status_t ib_engine_config_started(ib_engine_t *ib,
                                     ib_cfgparser_t *cp)
{
    assert(ib != NULL);
    assert(cp != NULL);
    assert(ib->cfg_state == CFG_NOT_STARTED);
    ib_status_t rc;

    /* Store the configuration parser in the engine */
    ib->cfgparser = cp;
    ib->cfg_state = CFG_STARTED;

    /* Create and configure the main configuration context. */
    rc = ib_engine_context_create_main(ib);
    if (rc != IB_OK) {
        return rc;
    }

    /* Open the main context */
    rc = ib_context_open(ib->ctx);
    if (rc != IB_OK) {
        return rc;
    }

    ib_log_ex(ib, IB_LOG_INFO, NULL, NULL, 0,
              "%s: Configuring", IB_PRODUCT_VERSION_NAME);

    return IB_OK;
}

ib_status_t ib_engine_config_finished(ib_engine_t *ib)
{
    assert(ib != NULL);
    assert(ib->cfg_state == CFG_STARTED);
    ib_status_t rc;

    /* Apply the configuration. */
    rc = ib_cfgparser_apply(ib->cfgparser, ib);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to configure the IronBee engine.");
    }

    /* Initialize (and close) the main configuration context.
     * Note: The context can be NULL for unit tests. */
    if (ib->ctx != NULL) {
        ib_status_t tmp = ib_context_close(ib->ctx);
        if (tmp != IB_OK) {
            return tmp;
        }
    }

    /* Clear config parser pointer */
    ib->cfgparser = NULL;
    ib->cfg_state = CFG_FINISHED;

    /* Destroy the temporary memory pool. */
    ib_engine_pool_temp_destroy(ib);

    if (rc == IB_OK) {
        ib_log_ex(ib, IB_LOG_INFO, NULL, NULL, 0,
                  "%s: Ready", IB_PRODUCT_VERSION_NAME);
    }

    return rc;
}

ib_status_t ib_engine_module_get(const ib_engine_t *ib,
                                 const char * name,
                                 ib_module_t **pm)
{
    size_t n;
    size_t i;
    ib_module_t *m;

    /* Return the first module matching the name. */
    IB_ARRAY_LOOP(ib->modules, n, i, m) {
        if (m == NULL) {
            continue;
        }
        if (strcmp(name, m->name) == 0) {
            *pm = m;
            return IB_OK;
        }
    }

    *pm = NULL;

    return IB_ENOENT;
}

ib_status_t ib_engine_cfgparser_get(const ib_engine_t *ib,
                                    const ib_cfgparser_t **pparser)
{
    assert(ib != NULL);
    assert(pparser != NULL);

    *pparser = ib->cfgparser;
    return IB_OK;
}


ib_mm_t ib_engine_mm_main_get(const ib_engine_t *ib)
{
    return ib_mm_mpool(ib->mp);
}

ib_mm_t ib_engine_mm_config_get(const ib_engine_t *ib)
{
    return ib_mm_mpool(ib->config_mp);
}

ib_mm_t ib_engine_mm_temp_get(const ib_engine_t *ib)
{
    return ib_mm_mpool(ib->temp_mp);
}

void ib_engine_pool_temp_destroy(ib_engine_t *ib)
{
    ib_engine_pool_destroy(ib, ib->temp_mp);
    ib->temp_mp = NULL;
    return;
}

void ib_engine_pool_destroy(ib_engine_t *ib, ib_mpool_t *mp)
{
    assert(ib != NULL);


    if (mp == NULL) {
        return;
    }

#ifdef IB_DEBUG_MEMORY
    {
        ib_status_t rc;
        char *message = NULL;
        char *path = ib_mpool_path(mp);

        if (path == NULL) {
            /* This will probably also fail... */
            ib_log_emergency(ib, "Allocation error.");
            goto finish;
        }

        rc = ib_mpool_validate(mp, &message);
        if (rc != IB_OK) {
            ib_log_error(ib, "Memory pool %s failed to validate: %s",
                path, (message ? message : "no message")
            );
        }

        if (message != NULL) {
            free(message);
            message = NULL;
        }

        message = ib_mpool_analyze(mp);
        if (message == NULL) {
            ib_log_emergency(ib, "Allocation error.");
            goto finish;
        }

        /* We use printf to coincide with the final memory debug which
         * can't be logged because it occurs too late in engine destruction.
         */
        printf("Memory Pool Analysis of %s:\n%s", path, message);

    finish:
        if (path != NULL) {
            free(path);
        }
        if (message != NULL) {
            free(message);
        }
    }
#endif

    ib_mpool_destroy(mp);

    return;
}

ib_var_config_t *ib_engine_var_config_get(
    ib_engine_t *ib
)
{
    return ib->var_config;
}

const ib_var_config_t *ib_engine_var_config_get_const(
    const ib_engine_t *ib
)
{
    return ib->var_config;
}

struct engine_notify_logevent_t {
    ib_engine_notify_logevent_fn  fn;
    void                         *cbdata;
};

typedef struct engine_notify_logevent_t engine_notify_logevent_t;

/**
 * Register a callback function to handle newly created events.
 *
 * @param[in] ib IronBee engine.
 * @param[in] fn The function to call on @ref ib_logevent_t generation.
 * @param[in] cbdata Callback data for @a fn.
 *
 * @returns
 * - IB_OK On succes.
 * - IB_EALLOC On allocation error.
 * - Other on unexpected failure.
 */
ib_status_t ib_engine_notify_logevent_register(
    ib_engine_t                  *ib,
    ib_engine_notify_logevent_fn  fn,
    void                         *cbdata
)
{
    assert(ib != NULL);
    assert(ib->logevent_handlers != NULL);
    assert(fn != NULL);

    ib_status_t               rc;
    ib_mm_t                   mm = ib_engine_mm_main_get(ib);
    engine_notify_logevent_t *handler =
        (engine_notify_logevent_t *)ib_mm_alloc(mm, sizeof(*handler));

    handler->fn     = fn;
    handler->cbdata = cbdata;

    /* Add callback to the list of handlers. */
    rc = ib_list_push(ib->logevent_handlers, handler);
    if (rc != IB_OK) {
        return rc;
    }

    return IB_OK;
}

ib_status_t ib_engine_notify_logevent(
    ib_engine_t   *ib,
    ib_tx_t       *tx,
    ib_logevent_t *logevent
)
{
    assert(ib != NULL);
    assert(ib->cfg_state == CFG_FINISHED);
    assert(ib->logevent_handlers != NULL);
    assert(tx != NULL);
    assert(logevent != NULL);

    const ib_list_node_t *node;

    IB_LIST_LOOP_CONST(ib->logevent_handlers, node) {
        ib_status_t rc;
        const engine_notify_logevent_t *handler =
            (const engine_notify_logevent_t *)ib_list_node_data_const(node);

        rc = handler->fn(ib, tx, logevent, handler->cbdata);
        if (rc == IB_DECLINED) {
            ib_log_debug(ib, "Logevent handler declined.");
        }
        else if (rc != IB_OK) {
            ib_log_error(
                ib, "Error handling logevent: %s", ib_status_to_string(rc));
        }
    }

    return IB_OK;
}

void ib_engine_destroy(ib_engine_t *ib)
{
    size_t ne;
    size_t idx;
    ib_list_node_t *node;
    ib_module_t    *m;

    if (ib == NULL) {
        return;
    }

    /// @todo Destroy filters

    IB_LIST_LOOP_REVERSE(ib->contexts, node) {
        ib_context_t *ctx = (ib_context_t *)ib_list_node_data(node);
        if ( (ctx != ib->ctx) && (ctx != ib->ectx) ) {
            ib_context_destroy(ctx);
        }
    }

    if (ib->ctx != ib->ectx) {
        ib_context_destroy(ib->ctx);
        ib->ctx = NULL;
    }
    ib_context_destroy(ib->ectx);
    ib->ectx = NULL;

    /* Important: Logging does not work after this point! */
    IB_ARRAY_LOOP_REVERSE(ib->modules, ne, idx, m) {
        if (m != NULL) {
            ib_module_unload(m);
        }
    }

    /* No logging from here on out. */

    /* Close the loggers. */
    ib_logger_close(ib->logger);

#ifdef IB_DEBUG_MEMORY
    /* We can't use ib_engine_pool_destroy here as too little of the
     * the engine is left.
     *
     * But always output memory usage stats.
     *
     * Also can't use logging anymore.
     */
    {
        char *report = ib_mpool_analyze(ib->mp);
        if (report != NULL) {
            printf("Engine Memory Use:\n%s\n", report);
            free(report);
        }
    }
#endif
    ib_mpool_destroy(ib->mp);

    return;
}

const char *ib_engine_instance_id(
    const ib_engine_t *ib)
{
    return ib->instance_id;
}

ib_status_t ib_conn_generate_id(ib_conn_t *conn)
{
    return ib_uuid_create_v4(conn->id);
}

ib_status_t ib_conn_create(ib_engine_t *ib,
                           ib_conn_t **pconn,
                           void *server_ctx)
{
    ib_mpool_t *pool;
    ib_status_t rc;
    char namebuf[64];
    ib_conn_t *conn = NULL;
    ib_mm_t mm;

    /* Create a sub-pool for each connection and allocate from it */
    /// @todo Need to tune the pool size
    rc = ib_mpool_create(&pool, "conn", NULL);
    if (rc != IB_OK) {
        rc = IB_EALLOC;
        goto failed;
    }
    mm = ib_mm_mpool(pool);

    conn = (ib_conn_t *)ib_mm_calloc(mm, 1, sizeof(*conn));
    if (conn == NULL) {
        rc = IB_EALLOC;
        goto failed;
    }

    /* Mark time. */
    ib_clock_gettimeofday(&(conn->tv_created));
    conn->t.started = ib_clock_get_time();

    /* Name the connection pool */
    snprintf(namebuf, sizeof(namebuf), "conn[%p]", (void *)conn);
    ib_mpool_setname(pool, namebuf);

    conn->ib = ib;
    conn->mp = pool;
    conn->mm = mm;
    conn->ctx = ib->ctx;
    conn->server_ctx = server_ctx;

    ib_conn_generate_id(conn);

    /* Create the per-module data data store. */
    rc = ib_array_create(&(conn->module_data), conn->mm, 16, 8);
    if (rc != IB_OK) {
        rc = IB_EALLOC;
        goto failed;
    }

    *pconn = conn;

    return IB_OK;

failed:
    /* Make sure everything is cleaned up on failure */
    if (conn != NULL) {
        ib_engine_pool_destroy(ib, conn->mp);
    }
    conn = NULL;

    return rc;
}

ib_status_t ib_conn_get_module_data(
    const ib_conn_t   *conn,
    const ib_module_t *m,
    void              *data
)
{
  assert(conn != NULL);
  assert(m != NULL);
  assert(data != NULL);

  void *local_data;

  ib_status_t rc = ib_array_get(conn->module_data, m->idx, &local_data);
  if (rc != IB_OK || local_data == NULL) {
      return IB_ENOENT;
  }

  *(void **)data = local_data;

  return IB_OK;
}

ib_status_t ib_conn_set_module_data(
    ib_conn_t         *conn,
    const ib_module_t *m,
    void              *data
)
{
  assert(conn != NULL);
  assert(m != NULL);

  ib_status_t rc = ib_array_setn(conn->module_data, m->idx, data);
  return rc;
}

void ib_conn_destroy(ib_conn_t *conn)
{
    /// @todo Probably need to update state???
    if ( conn != NULL && conn->mp != NULL ) {
        ib_engine_pool_destroy(conn->ib, conn->mp);
        /* Don't do this: conn->mp = NULL; conn is now freed memory! */
    }
}

ib_status_t ib_tx_generate_id(ib_tx_t *tx)
{
    return ib_uuid_create_v4(tx->id);
}

ib_status_t ib_tx_create(ib_tx_t **ptx,
                         ib_conn_t *conn,
                         void *sctx)
{
    ib_mpool_t *pool;
    ib_mm_t mm;
    ib_status_t rc;
    char namebuf[64];
    ib_tx_t *tx = NULL;
    ib_core_cfg_t *corecfg;

    ib_engine_t *ib = conn->ib;

    rc = ib_core_context_config(ib->ctx, &corecfg);
    if (rc != IB_OK) {
        ib_log_alert(ib, "Failed to retrieve core module configuration.");
    }

    assert(corecfg != NULL);

    /* Create a sub-pool from the connection memory pool for each
     * transaction and allocate from it
     */
    rc = ib_mpool_create(&pool, "tx", conn->mp);
    if (rc != IB_OK) {
        rc = IB_EALLOC;
        goto failed;
    }
    mm = ib_mm_mpool(pool);
    tx = (ib_tx_t *)ib_mm_calloc(mm, 1, sizeof(*tx));
    if (tx == NULL) {
        rc = IB_EALLOC;
        goto failed;
    }

    /* Name the transaction pool */
    snprintf(namebuf, sizeof(namebuf), "tx[%p]", (void *)tx);
    ib_mpool_setname(pool, namebuf);

    /* Mark time. */
    ib_clock_gettimeofday(&tx->tv_created);
    tx->t.started = ib_clock_get_time();

    tx->ib = ib;
    tx->mp = pool;
    tx->mm = mm;
    tx->ctx = ib->ctx;
    tx->sctx = sctx;
    tx->conn = conn;
    tx->remote_ipstr = conn->remote_ipstr;
    tx->hostname = IB_DSTR_EMPTY;
    tx->path = IB_DSTR_URI_ROOT_PATH;
    tx->auditlog_parts = corecfg->auditlog_parts;

    ++conn->tx_count;
    ib_tx_generate_id(tx);

    /* Create data */
    rc = ib_var_store_acquire(&tx->var_store, tx->mm, tx->ib->var_config);
    if (rc != IB_OK) {
        ib_log_alert_tx(tx,
                        "Error creating tx var store: %s",
                        ib_status_to_string(rc));
        return rc;
    }

    /* Create logevents */
    rc = ib_list_create(&tx->logevents, tx->mm);
    if (rc != IB_OK) {
        return rc;
    }

    /* Create the per-module data data store. */
    rc = ib_array_create(&(tx->module_data), tx->mm, 16, 8);
    if (rc != IB_OK) {
        rc = IB_EALLOC;
        goto failed;
    }

    /* Create the body buffers. */
    rc = ib_stream_create(&tx->request_body, tx->mm);
    if (rc != IB_OK) {
        goto failed;
    }
    rc = ib_stream_create(&tx->response_body, tx->mm);
    if (rc != IB_OK) {
        goto failed;
    }

    /* Create the request body stream. */
    rc = ib_stream_pump_create(
        &tx->request_body_pump,
        ib_engine_stream_processor_registry(ib),
        tx);
    if (rc != IB_OK) {
        goto failed;
    }

    /* Create the response body stream. */
    rc = ib_stream_pump_create(
        &tx->response_body_pump,
        ib_engine_stream_processor_registry(ib),
        tx);
    if (rc != IB_OK) {
        goto failed;
    }

    /**
     * After this, we have generally succeeded and are now outputting
     * the transaction to the conn object and the ptx pointer.
     */

    /* Add transaction to the connection list */
    if (conn->tx_first == NULL) {
        conn->tx_first = tx;
        conn->tx = tx;
        conn->tx_last = tx;
    }
    else {
        conn->tx = tx;
        conn->tx_last->next = tx;
        conn->tx_last = tx;

        /* If there are more than one transactions, then this is a pipeline
         * request and needs to be marked as such.
         */
        if (conn->tx_first->next == tx) {
            ib_tx_flags_set(conn->tx_first, IB_TX_FPIPELINED);
        }
        ib_tx_flags_set(tx, IB_TX_FPIPELINED);
    }

    /* Only when we are successful, commit changes to output variable. */
    *ptx = tx;

    return IB_OK;

failed:
    /* Make sure everything is cleaned up on failure */
    if (tx != NULL) {
        ib_engine_pool_destroy(ib, tx->mp);
    }
    tx = NULL;

    return rc;
}

ib_status_t ib_tx_get_module_data(
    const ib_tx_t *tx,
    const ib_module_t *m,
    void *pdata
)
{
  assert(tx != NULL);
  assert(m != NULL);
  assert(pdata != NULL);

  void *local_data;

  ib_status_t rc = ib_array_get(tx->module_data, m->idx, &local_data);
  if (rc != IB_OK || local_data == NULL) {
      return IB_ENOENT;
  }

  *(void **)pdata = local_data;

  return IB_OK;
}


ib_status_t ib_tx_set_module_data(
    ib_tx_t *tx,
    const ib_module_t *m,
    void *data
)
{
  assert(tx != NULL);
  assert(m != NULL);

  ib_status_t rc = ib_array_setn(tx->module_data, m->idx, data);
  return rc;
}

ib_stream_pump_t *ib_tx_response_body_pump(ib_tx_t *tx) {
    assert(tx != NULL);
    assert(tx->response_body_pump != NULL);

    return tx->response_body_pump;
}

ib_stream_pump_t *ib_tx_request_body_pump(ib_tx_t *tx) {
    assert(tx != NULL);
    assert(tx->request_body_pump != NULL);

    return tx->request_body_pump;
}

ib_status_t ib_tx_server_error(
    ib_tx_t *tx,
    int status
)
{
    assert(tx != NULL);
    assert(tx->ib != NULL);
    assert(ib_engine_server_get(tx->ib) != NULL);

    return ib_server_error_response(ib_engine_server_get(tx->ib), tx, status);
}

ib_status_t ib_tx_server_error_header(
    ib_tx_t *tx,
    const char *name, size_t name_len,
    const char *value, size_t value_len
)
{
    assert(tx != NULL);
    assert(tx->ib != NULL);
    assert(ib_engine_server_get(tx->ib) != NULL);
    assert(name != NULL);
    assert(value != NULL);

    return ib_server_error_header(ib_engine_server_get(tx->ib), tx, name, name_len, value, value_len);
}

ib_status_t ib_tx_server_error_data(
    ib_tx_t *tx,
    const char *data,
    size_t dlen
)
{
    assert(tx != NULL);
    assert(tx->ib != NULL);
    assert(ib_engine_server_get(tx->ib) != NULL);
    assert(data != NULL);

    return ib_server_error_body(ib_engine_server_get(tx->ib), tx, data, dlen);
}

ib_status_t ib_tx_server_header(
    ib_tx_t                   *tx,
    ib_server_direction_t      dir,
    ib_server_header_action_t  action,
    const char                *name,
    size_t                     name_length,
    const char                *value,
    size_t                     value_length
)
{
    assert(tx != NULL);
    assert(tx->ib != NULL);
    assert(ib_engine_server_get(tx->ib) != NULL);
    assert(name != NULL);
    assert(value != NULL);

    return ib_server_header(ib_engine_server_get(tx->ib), tx, dir, action, name, name_length, value, value_length);
}

void ib_tx_destroy(ib_tx_t *tx)
{
    assert(tx != NULL);
    assert(tx->conn != NULL);
    assert(tx->conn->tx_first != NULL);

    ib_conn_t *conn = tx->conn;
    ib_tx_t *curr;
    ib_tx_t *prev = NULL;
    bool found = false;

    if (   ib_flags_all(tx->flags, IB_TX_FREQ_HAS_DATA)
        || ib_flags_all(tx->flags, IB_TX_FRES_HAS_DATA) )
    {
        /* Make sure that the post processing state was notified. */
        // TODO: Remove the need for this
        if (! ib_flags_all(tx->flags, IB_TX_FPOSTPROCESS)) {
            ib_log_warning_tx(tx,
                              "Failed to run post processing on transaction.");
        }

        /* Make sure that the post processing state was notified. */
        // TODO: Remove the need for this
        if (! ib_flags_all(tx->flags, IB_TX_FLOGGING)) {
            ib_log_warning_tx(tx,
                              "Failed to run logging on transaction.");
        }
    }

    /* Find the tx in the list */
    for (curr = conn->tx_first; curr != NULL; curr = curr->next) {
        if (curr == tx) {
            found = true;
            break;
        }
        prev = curr;
    }
    assert(found);

    /* Update the first and last pointers */
    if (conn->tx_first == tx) {
        conn->tx_first = tx->next;
        conn->tx = tx->next;
    }
    if (conn->tx_last == tx) {
        assert(tx->next == NULL);
        conn->tx_last = prev;
    }

    /* Remove tx from the list */
    if (prev != NULL) {
        prev->next = tx->next;
    }

    /// @todo Probably need to update state???
    ib_engine_pool_destroy(tx->ib, tx->mp);
}

/* -- Blocking -- */

static
ib_status_t default_block_handler(
    ib_tx_t         *tx,
    ib_block_info_t *info
)
{
    info->method = IB_BLOCK_METHOD_STATUS;
    info->status = 403;

    return IB_OK;
}

static
ib_status_t block_with_status(
    ib_tx_t               *tx,
    const ib_block_info_t *block_info
)
{
    assert(tx != NULL);
    assert(block_info != NULL);
    assert(block_info->method == IB_BLOCK_METHOD_STATUS);

    const ib_server_t *server = ib_engine_server_get(tx->ib);
    ib_status_t rc;

    rc = ib_server_error_response(server, tx, block_info->status);
    if (rc == IB_ENOTIMPL || rc == IB_DECLINED) {
        return rc;
    }
    else if (rc != IB_OK) {
        ib_log_notice_tx(
            tx,
            "Server failed to set HTTP error response: %s",
            ib_status_to_string(rc)
        );
        return rc;
    }

    assert(rc == IB_OK);
    return IB_OK;
}

static
ib_status_t block_with_close(
    ib_tx_t *tx
)
{
    assert(tx != NULL);

    const ib_server_t *server = ib_engine_server_get(tx->ib);
    ib_status_t rc;

    rc = ib_server_close(server, tx->conn, tx);
    if (rc == IB_ENOTIMPL || rc == IB_DECLINED) {
        return rc;
    }
    else if (rc != IB_OK) {
        ib_log_notice_tx(
            tx,
            "Server failed to close connection: %s.",
            ib_status_to_string(rc)
        );
    }

    assert(rc == IB_OK);
    return IB_OK;
}

ib_status_t ib_tx_block(ib_tx_t *tx)
{
    assert(tx != NULL);

    const ib_list_node_t *node;
    ib_status_t rc;
    ib_block_info_t block_info;

    if (ib_tx_is_blocked(tx)) {
        return IB_OK;
    }
    tx->is_blocked = true;

    /* Call all pre-block hooks. */
    IB_LIST_LOOP_CONST(tx->ib->block_pre_hooks, node) {
        const ib_block_pre_hook_t *info = ib_list_node_data_const(node);
        rc = info->hook(tx, info->cbdata);
        if (rc != IB_OK) {
            ib_log_error_tx(tx,
                "Block pre-hook %s failed: %s",
                info->name,
                ib_status_to_string(rc)
            );
            return rc;
        }
    }

    /* If a block handler is registered, call it to get the blocking
     * info. */
    if (tx->ib->block_handler.handler != NULL) {
        rc = tx->ib->block_handler.handler(
            tx,
            &block_info,
            tx->ib->block_handler.cbdata
        );
        if (rc == IB_DECLINED) {
            return IB_DECLINED;
        }
        if (rc != IB_OK) {
            ib_log_error_tx(tx,
                "Block handler %s failed: %s",
                tx->ib->block_handler.name,
                ib_status_to_string(rc)
            );
            return rc;
        }
    }
    else {
        /* If no block handler is registered, call a default blocking
         * handler to get the blocking info. */
        rc = default_block_handler(tx, &block_info);
        if (rc == IB_DECLINED) {
            return IB_DECLINED;
        }
        if (rc != IB_OK) {
            ib_log_error_tx(tx,
                "Default block handler failed: %s",
                ib_status_to_string(rc)
            );
            return rc;
        }
    }

    /* Record block info. */
    tx->block_info = block_info;

    /* If blocking is not enabled, the function returns IB_DECLINED. */
    if (! ib_tx_is_blocking_enabled(tx)) {
        return IB_DECLINED;
    }

    /* Communicate the blocking info to the server. */
    switch(block_info.method) {
        case IB_BLOCK_METHOD_STATUS:
            block_with_status(tx, &block_info);
            break;
        case IB_BLOCK_METHOD_CLOSE:
            block_with_close(tx);
            break;
        default:
            assert(! "Invalid block method.");
    }

    /* Call all post-block hooks. */
    IB_LIST_LOOP_CONST(tx->ib->block_post_hooks, node) {
        const ib_block_post_hook_t *info = ib_list_node_data_const(node);
        rc = info->hook(tx, &block_info, info->cbdata);
        if (rc != IB_OK) {
            ib_log_error_tx(tx,
                "Block post-hook %s failed: %s",
                info->name,
                ib_status_to_string(rc)
            );
            return rc;
        }
    }

    return IB_OK;
}

void ib_tx_enable_blocking(ib_tx_t *tx)
{
    ib_tx_flags_set(tx, IB_TX_FBLOCKING_MODE);
}

void ib_tx_disable_blocking(ib_tx_t *tx)
{
    ib_tx_flags_unset(tx, IB_TX_FBLOCKING_MODE);
}

bool ib_tx_is_blocking_enabled(const ib_tx_t *tx)
{
    return ib_flags_any(tx->flags, IB_TX_FBLOCKING_MODE);
}

bool ib_tx_is_blocked(const ib_tx_t *tx)
{
    return tx->is_blocked;
}

ib_block_info_t ib_tx_block_info(const ib_tx_t *tx)
{
    return tx->block_info;
}

ib_status_t ib_register_block_handler(
    ib_engine_t           *ib,
    const char            *name,
    ib_block_handler_fn_t  handler,
    void                  *cbdata
)
{
    assert(ib != NULL);
    assert(handler != NULL);
    assert(name != NULL);

    ib_mm_t mm = ib_engine_mm_main_get(ib);

    if (ib->block_handler.handler != NULL) {
        return IB_EINVAL;
    }

    ib->block_handler.name = ib_mm_strdup(mm, name);
    if (ib->block_handler.name == NULL) {
        return IB_EALLOC;
    }
    ib->block_handler.handler = handler;
    ib->block_handler.cbdata = cbdata;

    return IB_OK;
}

ib_status_t ib_register_block_pre_hook(
    ib_engine_t            *ib,
    const char             *name,
    ib_block_pre_hook_fn_t  hook,
    void                   *cbdata
)
{
    assert(ib != NULL);
    assert(hook != NULL);
    assert(name != NULL);

    ib_status_t rc;
    ib_mm_t mm = ib_engine_mm_main_get(ib);
    ib_block_pre_hook_t *block_pre_hook;

    block_pre_hook = ib_mm_calloc(mm, 1, sizeof(*block_pre_hook));
    if (block_pre_hook == NULL) {
        return IB_EALLOC;
    }


    block_pre_hook->name = ib_mm_strdup(mm, name);
    if (block_pre_hook->name == NULL) {
        return IB_EALLOC;
    }
    block_pre_hook->hook = hook;
    block_pre_hook->cbdata = cbdata;

    rc = ib_list_push(ib->block_pre_hooks, block_pre_hook);
    if (rc != IB_OK) {
        return rc;
    }

    return IB_OK;
}

ib_status_t ib_register_block_post_hook(
    ib_engine_t             *ib,
    const char              *name,
    ib_block_post_hook_fn_t  hook,
    void                    *cbdata
)
{
    assert(ib != NULL);
    assert(hook != NULL);
    assert(name != NULL);

    ib_status_t rc;
    ib_mm_t mm = ib_engine_mm_main_get(ib);
    ib_block_post_hook_t *block_post_hook;

    block_post_hook = ib_mm_calloc(mm, 1, sizeof(*block_post_hook));
    if (block_post_hook == NULL) {
        return IB_EALLOC;
    }

    block_post_hook->name = ib_mm_strdup(mm, name);
    if (block_post_hook->name == NULL) {
        return IB_EALLOC;
    }
    block_post_hook->hook = hook;
    block_post_hook->cbdata = cbdata;

    rc = ib_list_push(ib->block_post_hooks, block_post_hook);
    if (rc != IB_OK) {
        return rc;
    }

    return IB_OK;
}

/* -- State Routines -- */

const char *ib_state_name(ib_state_t state)
{
    return ib_state_table[state].state_name;
}
/* -- Hook Routines -- */

ib_state_hook_type_t ib_state_hook_type(ib_state_t state)
{
    static const size_t num_states =
        sizeof(ib_state_table) / sizeof(ib_state_data_t);

    if (state >= num_states) {
        return IB_STATE_HOOK_INVALID;
    }

    return ib_state_table[state].hook_type;
}

ib_status_t ib_hook_null_register(
    ib_engine_t *ib,
    ib_state_t state,
    ib_state_null_hook_fn_t cb,
    void *cbdata
)
{
    ib_status_t rc;

    rc = ib_hook_check(ib, state, IB_STATE_HOOK_NULL);
    if (rc != IB_OK) {
        return rc;
    }

    ib_hook_t *hook = (ib_hook_t *)ib_mm_alloc(ib_engine_mm_main_get(ib), sizeof(*hook));
    if (hook == NULL) {
        return IB_EALLOC;
    }

    hook->callback.null = cb;
    hook->cbdata = cbdata;

    rc = ib_hook_register(ib, state, hook);

    return rc;
}

ib_status_t ib_hook_conn_register(
    ib_engine_t *ib,
    ib_state_t state,
    ib_state_conn_hook_fn_t cb,
    void *cbdata
)
{
    ib_status_t rc;

    rc = ib_hook_check(ib, state, IB_STATE_HOOK_CONN);
    if (rc != IB_OK) {
        return rc;
    }

    ib_hook_t *hook = (ib_hook_t *)ib_mm_alloc(ib_engine_mm_main_get(ib), sizeof(*hook));
    if (hook == NULL) {
        return IB_EALLOC;
    }

    hook->callback.conn = cb;
    hook->cbdata = cbdata;

    rc = ib_hook_register(ib, state, hook);

    return rc;
}

ib_status_t ib_hook_tx_register(
    ib_engine_t *ib,
    ib_state_t state,
    ib_state_tx_hook_fn_t cb,
    void *cbdata
) {
    ib_status_t rc;

    rc = ib_hook_check(ib, state, IB_STATE_HOOK_TX);
    if (rc != IB_OK) {
        return rc;
    }

    ib_hook_t *hook = (ib_hook_t *)ib_mm_alloc(ib_engine_mm_main_get(ib), sizeof(*hook));
    if (hook == NULL) {
        return IB_EALLOC;
    }

    hook->callback.tx = cb;
    hook->cbdata = cbdata;

    rc = ib_hook_register(ib, state, hook);

    return rc;
}

ib_status_t ib_hook_txdata_register(
    ib_engine_t *ib,
    ib_state_t state,
    ib_state_txdata_hook_fn_t cb,
    void *cbdata
) {
    ib_status_t rc;

    rc = ib_hook_check(ib, state, IB_STATE_HOOK_TXDATA);
    if (rc != IB_OK) {
        return rc;
    }

    ib_hook_t *hook = (ib_hook_t *)ib_mm_alloc(ib_engine_mm_main_get(ib), sizeof(*hook));
    if (hook == NULL) {
        return IB_EALLOC;
    }

    hook->callback.txdata = cb;
    hook->cbdata = cbdata;

    rc = ib_hook_register(ib, state, hook);

    return rc;
}

ib_status_t ib_hook_parsed_header_data_register(
    ib_engine_t *ib,
    ib_state_t state,
    ib_state_header_data_fn_t cb,
    void *cbdata)
{
    ib_status_t rc;

    rc = ib_hook_check(ib, state, IB_STATE_HOOK_HEADER);
    if (rc != IB_OK) {
        return rc;
    }

    ib_hook_t *hook = (ib_hook_t *)ib_mm_alloc(ib_engine_mm_main_get(ib), sizeof(*hook));
    if (hook == NULL) {
        return IB_EALLOC;
    }

    hook->callback.headerdata = cb;
    hook->cbdata = cbdata;

    rc = ib_hook_register(ib, state, hook);

    return rc;
}

ib_status_t ib_hook_parsed_req_line_register(
    ib_engine_t *ib,
    ib_state_t state,
    ib_state_request_line_fn_t cb,
    void *cbdata)
{
    ib_status_t rc;

    rc = ib_hook_check(ib, state, IB_STATE_HOOK_REQLINE);
    if (rc != IB_OK) {
        return rc;
    }

    ib_hook_t *hook = (ib_hook_t *)ib_mm_alloc(ib_engine_mm_main_get(ib), sizeof(*hook));
    if (hook == NULL) {
        return IB_EALLOC;
    }

    hook->callback.requestline = cb;
    hook->cbdata = cbdata;

    rc = ib_hook_register(ib, state, hook);

    return rc;
}

ib_status_t ib_hook_parsed_resp_line_register(
    ib_engine_t *ib,
    ib_state_t state,
    ib_state_response_line_fn_t cb,
    void *cbdata)
{
    ib_status_t rc;

    rc = ib_hook_check(ib, state, IB_STATE_HOOK_RESPLINE);
    if (rc != IB_OK) {
        return rc;
    }

    ib_hook_t *hook = (ib_hook_t *)ib_mm_alloc(ib_engine_mm_main_get(ib), sizeof(*hook));
    if (hook == NULL) {
        return IB_EALLOC;
    }

    hook->callback.responseline = cb;
    hook->cbdata = cbdata;

    rc = ib_hook_register(ib, state, hook);

    return rc;
}

ib_status_t ib_hook_context_register(
    ib_engine_t *ib,
    ib_state_t state,
    ib_state_ctx_hook_fn_t cb,
    void *cbdata
)
{
    ib_status_t rc;

    rc = ib_hook_check(ib, state, IB_STATE_HOOK_CTX);
    if (rc != IB_OK) {
        return rc;
    }

    ib_hook_t *hook = (ib_hook_t *)ib_mm_alloc(ib_engine_mm_main_get(ib), sizeof(*hook));
    if (hook == NULL) {
        return IB_EALLOC;
    }

    hook->callback.ctx = cb;
    hook->cbdata = cbdata;

    rc = ib_hook_register(ib, state, hook);

    return rc;
}


/* -- Configuration Contexts -- */

const ib_list_t *ib_context_get_all(const ib_engine_t *ib)
{
    assert(ib != NULL);

    return ib->contexts;
}

ib_context_t DLL_PUBLIC *ib_context_get_context(
  ib_engine_t *ib,
  ib_conn_t   *conn,
  ib_tx_t     *tx
)
{
    assert(ib != NULL);

    if (tx != NULL && tx->ctx != NULL) {
        return tx->ctx;
    }

    if (conn != NULL && conn->ctx != NULL) {
        return conn->ctx;
    }

    return ib_context_main(ib);
}

ib_status_t ib_context_create(ib_engine_t *ib,
                              ib_context_t *parent,
                              ib_ctype_t ctype,
                              const char *ctx_type,
                              const char *ctx_name,
                              ib_context_t **pctx)
{
    ib_mpool_t *ppool;
    ib_mpool_t *pool;
    ib_mm_t mm;
    ib_status_t rc;
    ib_context_t *ctx = NULL;
    char *full;
    size_t full_len;

    /* Create memory subpool */
    ppool = (parent == NULL) ? ib->mp : parent->mp;
    rc = ib_mpool_create(&pool, "context", ppool);
    if (rc != IB_OK) {
        rc = IB_EALLOC;
        goto failed;
    }
    mm = ib_mm_mpool(pool);

    /* Create the main structure */
    ctx = (ib_context_t *)ib_mm_calloc(mm, 1, sizeof(*ctx));
    if (ctx == NULL) {
        rc = IB_EALLOC;
        goto failed;
    }

    ctx->ib = ib;
    ctx->mp = pool;
    ctx->mm = mm;
    ctx->parent = parent;
    ctx->ctype = ctype;
    ctx->ctx_type = ctx_type;
    ctx->ctx_name = ctx_name;
    ctx->state = CTX_CREATED;

    /* Generate the full name of the context */
    full_len = 2;
    if (parent != NULL) {
        full_len += strlen(parent->ctx_name) + 1;
    }
    if (ctx_type != NULL) {
        full_len += strlen(ctx_type);
    }
    if (ctx_name != NULL) {
        full_len += strlen(ctx_name);
    }
    full = (char *)ib_mm_alloc(ctx->mm, full_len);
    if (full == NULL) {
        rc = IB_EALLOC;
        goto failed;
    }
    *full = '\0';
    if (parent != NULL) {
        strcat(full, parent->ctx_name);
        strcat(full, ":");
    }
    if (ctx_type != NULL) {
        strcat(full, ctx_type);
    }
    strcat(full, ":");
    if (ctx_name != NULL) {
        strcat(full, ctx_name);
    }
    ctx->ctx_full = full;

    /* Create a cfgmap to hold the configuration */
    rc = ib_cfgmap_create(&(ctx->cfg), ctx->mm);
    if (rc != IB_OK) {
        goto failed;
    }

    /* Create an array to hold the module config data */
    rc = ib_array_create(&(ctx->cfgdata), ctx->mm, 16, 8);
    if (rc != IB_OK) {
        goto failed;
    }

    /* Create a list to hold the enabled filters */
    rc = ib_list_create(&(ctx->filters), ctx->mm);
    if (rc != IB_OK) {
        goto failed;
    }

    /* Create a list to hold child contexts */
    rc = ib_list_create(&(ctx->children), ctx->mm);
    if (rc != IB_OK) {
        goto failed;
    }

    /* Create a hash to hold the module-specific data */
    rc = ib_array_create(&(ctx->cfgdata), ctx->mm, 16, 8);
    if (rc != IB_OK) {
        goto failed;
    }

    /* Add myself to my parent's child list */
    if (parent != NULL) {
        rc = ib_list_push(parent->children, ctx);
        if (rc != IB_OK) {
            goto failed;
        }
    }

    /* Add to the engine's list of all contexts */
    rc = ib_list_push(ib->contexts, ctx);
    if (rc != IB_OK) {
        goto failed;
    }

    if (parent != NULL) {
        rc = ib_context_set_auditlog_index(
            ctx,
            parent->auditlog->index_enabled,
            parent->auditlog->index_default ? NULL : parent->auditlog->index);
    }
    else {
        rc = ib_context_set_auditlog_index(ctx, true, NULL);
    }
    if (rc != IB_OK) {
        goto failed;
    }

    /* Register the modules. */
    /// @todo Later on this needs to be triggered by ActivateModule or similar
    if (ctype != IB_CTYPE_ENGINE) {
        ib_module_t *m;
        size_t n;
        size_t i;
        IB_ARRAY_LOOP(ib->modules, n, i, m) {
            if (m == NULL) {
                ib_log_notice(ib, "Not registering NULL module idx=%zd", i);
                continue;
            }
            rc = ib_module_register_context(m, ctx);
            if (rc != IB_OK) {
                goto failed;
            }
        }
    }

    /* Commit the new ctx to pctx. */
    *pctx = ctx;

    return IB_OK;


failed:
    /* Make sure everything is cleaned up on failure */
    if (ctx != NULL) {
        ib_engine_pool_destroy(ib, ctx->mp);
    }

    return rc;
}

ib_status_t ib_context_open(ib_context_t *ctx)
{
    ib_engine_t *ib = ctx->ib;
    ib_status_t rc;

    if (ctx->state != CTX_CREATED) {
        return IB_EINVAL;
    }

    if (ctx->ctype != IB_CTYPE_ENGINE) {
        rc = ib_cfgparser_context_push(ib->cfgparser, ctx);
        if (rc != IB_OK) {
            return rc;
        }
        rc = ib_context_set_cwd(ctx, ib->cfgparser->cur_cwd);
        if (rc != IB_OK) {
            return rc;
        }
    }

    rc = ib_state_notify_context_open(ib, ctx);
    if (rc != IB_OK) {
        return rc;
    }

    ctx->state = CTX_OPEN;
    return IB_OK;
}

ib_status_t ib_context_close(ib_context_t *ctx)
{
    ib_engine_t *ib = ctx->ib;
    ib_status_t rc;

    if (ctx->state != CTX_OPEN) {
        return IB_EINVAL;
    }

    rc = ib_state_notify_context_close(ib, ctx);
    if (rc != IB_OK) {
        return rc;
    }

    if (ctx->ctype != IB_CTYPE_ENGINE) {
        rc = ib_cfgparser_context_pop(ib->cfgparser, NULL, NULL);
        if (rc != IB_OK) {
            return rc;
        }
    }

    ctx->state = CTX_CLOSED;
    return IB_OK;
}

ib_status_t ib_context_set_cwd(ib_context_t *ctx, const char *dir)
{
    assert(ctx != NULL);

    /* For special cases (i.e. tests), allow handle NULL directory */
    if (dir == NULL) {
        char *buf = (char *)ib_mm_alloc(ctx->mm, maxpath);
        if (buf == NULL) {
            return IB_EALLOC;
        }
        ctx->ctx_cwd = getcwd(buf, maxpath);
        if (ctx->ctx_cwd == NULL) {
            return IB_EALLOC;
        }
        return IB_OK;
    }

    /* Copy it */
    ctx->ctx_cwd = ib_mm_strdup(ctx->mm, dir);
    if (ctx->ctx_cwd == NULL) {
        return IB_EALLOC;
    }
    return IB_OK;
}

const char *ib_context_config_cwd(const ib_context_t *ctx)
{
    assert(ctx != NULL);

    if (ctx->ib->cfgparser == NULL) {
         return ctx->ctx_cwd;
    }
    else {
         return ctx->ib->cfgparser->cur_cwd;
    }
}

ib_status_t ib_context_set_auditlog_index(ib_context_t *ctx,
                                          bool enable,
                                          const char *idx)
{
    ib_status_t rc;

    assert(ctx != NULL);
    assert(ctx->ib != NULL);
    assert(ctx->mp != NULL);

    /* Check if a new audit log structure must be allocated:
     *   1. if auditlog == NULL or
     *   2. if the allocated audit log belongs to another context we may
     *      not change its auditlog->index value (or auditlog->index_fp).
     *      We must make a new auditlog that the passed in ib_context_t
     *      ctx owns.  */
    if (ctx->auditlog == NULL || ctx->auditlog->owner != ctx) {

        ctx->auditlog = (ib_auditlog_cfg_t *)
            ib_mm_calloc(ctx->mm, 1, sizeof(*ctx->auditlog));

        if (ctx->auditlog == NULL) {
            return IB_EALLOC;
        }

        /* Set owner. */
        ctx->auditlog->owner = ctx;

        /* Set index_fp_lock. */
        if (enable == true) {
            rc = ib_lock_create(&(ctx->auditlog->index_fp_lock), ctx->mm);
            if (rc != IB_OK) {
                ib_log_notice(ctx->ib,
                              "Failed to initialize lock "
                              "for audit index %s: %s",
                              idx, ib_status_to_string(rc));
                return rc;
            }

            /* Set index. */
            if (idx == NULL) {
                ctx->auditlog->index_default = true;
                idx = default_auditlog_index;
            }
            else {
                ctx->auditlog->index_default = false;
            }
            ctx->auditlog->index = ib_mm_strdup(ctx->mm, idx);
            if (ctx->auditlog->index == NULL) {
                return IB_EALLOC;
            }
            ctx->auditlog->index_enabled = true;
        }
        else {
            ctx->auditlog->index = NULL;
            ctx->auditlog->index_fp = NULL;
        }
    }
    /* Else the auditlog struct is initialized and owned by this ctx. */
    else {
        bool unlock = false;
        if (ctx->auditlog->index_enabled == true) {
            const char *cidx = ctx->auditlog->index; /* Current index */

            rc = ib_lock_lock(ctx->auditlog->index_fp_lock);
            if (rc != IB_OK) {
                ib_log_notice(ctx->ib, "Failed lock to audit index %s",
                              ctx->auditlog->index);
                return rc;
            }
            unlock = true;

            /* Check that we aren't re-setting a value in the same context. */
            if ( (enable && ctx->auditlog->index_enabled) &&
                 ( ((idx == NULL) && ctx->auditlog->index_default) ||
                   ((idx != NULL) && (cidx != NULL) &&
                    (strcmp(idx, cidx) == 0)) ) )
            {
                if (unlock) {
                    ib_lock_unlock(ctx->auditlog->index_fp_lock);
                }

                return IB_OK;
            }
        }

        /* Replace the old index value with the new index value. */
        if (enable == false) {
            ctx->auditlog->index_enabled = false;
            ctx->auditlog->index_default = false;
            ctx->auditlog->index = NULL;
        }
        else {
            if (idx == NULL) {
                idx = default_auditlog_index;
                ctx->auditlog->index_default = true;
            }
            else {
                ctx->auditlog->index_default = false;
            }
            ctx->auditlog->index = ib_mm_strdup(ctx->mm, idx);
            if (ctx->auditlog->index == NULL) {
                if (unlock) {
                    ib_lock_unlock(ctx->auditlog->index_fp_lock);
                }
                return IB_EALLOC;
            }
            ctx->auditlog->index_enabled = true;
        }

        /* Close the audit log file if it is open. */
        if (ctx->auditlog->index_fp != NULL) {
            fclose(ctx->auditlog->index_fp);
            ctx->auditlog->index_fp = NULL;
        }

        if (unlock) {
            ib_lock_unlock(ctx->auditlog->index_fp_lock);
        }
    }

    return IB_OK;
}

ib_status_t ib_context_site_set(ib_context_t *ctx,
                                const ib_site_t *site)
{
    assert(ctx != NULL);

    if (ctx->state == CTX_CLOSED) {
        return IB_EINVAL;
    }
    if ( (ctx->ctype != IB_CTYPE_SITE) && (ctx->ctype != IB_CTYPE_LOCATION) ) {
        return IB_EINVAL;
    }

    ctx->site = site;
    return IB_OK;
}

ib_status_t ib_context_site_get(const ib_context_t *ctx,
                                const ib_site_t **psite)
{
    assert(ctx != NULL);
    assert(psite != NULL);

    if ( (ctx->ctype != IB_CTYPE_SITE) && (ctx->ctype != IB_CTYPE_LOCATION) ) {
        return IB_EINVAL;
    }

    *psite = ctx->site;
    return IB_OK;
}

ib_status_t ib_context_location_set(ib_context_t *ctx,
                                    const ib_site_location_t *location)
{
    assert(ctx != NULL);

    if (ctx->state == CTX_CLOSED) {
        return IB_EINVAL;
    }
    if (ctx->ctype != IB_CTYPE_LOCATION) {
        return IB_EINVAL;
    }

    ctx->location = location;
    return IB_OK;
}

ib_status_t ib_context_location_get(const ib_context_t *ctx,
                                    const ib_site_location_t **plocation)
{
    assert(ctx != NULL);
    assert(plocation != NULL);

    if (ctx->ctype != IB_CTYPE_LOCATION) {
        return IB_EINVAL;
    }

    *plocation = ctx->location;
    return IB_OK;
}

ib_context_t *ib_context_parent_get(const ib_context_t *ctx)
{
    return ctx->parent;
}

void ib_context_parent_set(ib_context_t *ctx,
                           ib_context_t *parent)
{
    ctx->parent = parent;
    return;
}

ib_ctype_t ib_context_type(const ib_context_t *ctx)
{
    assert(ctx != NULL);

    return ctx->ctype;
}

bool ib_context_type_check(const ib_context_t *ctx, ib_ctype_t ctype)
{
    assert(ctx != NULL);

    return ctx->ctype == ctype;
}

const char *ib_context_type_get(const ib_context_t *ctx)
{
    assert(ctx != NULL);

    if (ctx->ctx_type == NULL) {
        return "";
    }
    else {
        return ctx->ctx_type;
    }
}

const char *ib_context_name_get(const ib_context_t *ctx)
{
    assert(ctx != NULL);

    if (ctx->ctx_name == NULL) {
        return "";
    }
    else {
        return ctx->ctx_name;
    }
}

const char *ib_context_full_get(const ib_context_t *ctx)
{
    assert(ctx != NULL);

    return ctx->ctx_full;
}

void ib_context_destroy(ib_context_t *ctx)
{
    if (ctx == NULL) {
        return;
    }

    /* Run through the context modules to call any ctx_fini functions. */
    ib_state_notify_context_destroy(ctx->ib, ctx);

    ib_engine_pool_destroy(ctx->ib, ctx->mp);
}

ib_context_t *ib_context_engine(const ib_engine_t *ib)
{
    return ib->ectx;
}

ib_context_t *ib_context_main(const ib_engine_t *ib)
{
    return ib->ctx;
}

ib_engine_t *ib_context_get_engine(const ib_context_t *ctx)
{
    return ctx->ib;
}

ib_mm_t ib_context_get_mm(const ib_context_t *ctx)
{
    return ctx->mm;
}

ib_status_t ib_context_init_cfg(ib_context_t *ctx,
                                void *base,
                                const ib_cfgmap_init_t *init)
{
    ib_status_t rc;

    if (init == NULL) {
        return IB_OK;
    }

    rc = ib_cfgmap_init(ctx->cfg, base, init);

    return rc;
}

ib_status_t ib_context_module_config(const ib_context_t *ctx,
                                     const ib_module_t *m,
                                     void *pcfg)
{
    ib_context_data_t *cfgdata;
    ib_status_t rc;

    rc = ib_array_get(ctx->cfgdata, m->idx, (void *)&cfgdata);
    if (rc != IB_OK) {
        *(void **)pcfg = NULL;
        return rc;
    }

    if (cfgdata == NULL) {
        *(void **)pcfg = NULL;
        return IB_EINVAL;
    }

    *(void **)pcfg = cfgdata->data;

    return IB_OK;
}

ib_status_t ib_context_set(ib_context_t *ctx,
                           const char *name,
                           void *val)
{
    ib_status_t rc = ib_cfgmap_set(ctx->cfg, name, val);
    return rc;
}

ib_status_t ib_context_set_num(ib_context_t *ctx,
                               const char *name,
                               ib_num_t val)
{
    ib_status_t rc = ib_cfgmap_set(ctx->cfg, name, ib_ftype_num_in(&val));
    return rc;
}

ib_status_t ib_context_set_string(ib_context_t *ctx,
                                  const char *name,
                                  const char *val)
{
    ib_status_t rc = ib_cfgmap_set(ctx->cfg, name, ib_ftype_nulstr_in(val));
    return rc;
}


ib_status_t ib_context_get(ib_context_t *ctx,
                           const char *name,
                           void *pval, ib_ftype_t *ptype)
{
    ib_status_t rc = ib_cfgmap_get(ctx->cfg, name, pval, ptype);
    return rc;
}

/**
 * Set the given tx var flag to the given value (1 or 0).
 */
static ib_status_t tx_var_flags_set(
    ib_tx_t    *tx,
    ib_flags_t  flag,
    ib_num_t    flag_value
)
{
    assert(tx != NULL);

    for (
        const ib_tx_flag_map_t *flagmap = ib_core_vars_tx_flags();
        flagmap->name != NULL;
        ++flagmap
    )
    {
        /* If this flag is being set, set the var value. */
        if (flagmap->tx_flag & flag) {
            ib_field_t      *field;
            ib_status_t      rc;

            /* Create a field to use to set the value. */
            rc = ib_field_create(
                &field,
                tx->mm,
                IB_S2SL(flagmap->tx_name),
                IB_FTYPE_NUM,
                ib_ftype_num_in(&flag_value));
            if (rc != IB_OK) {
                return rc;
            }

            /* Remove and set the value. */
            rc = ib_var_target_remove_and_set(
                flagmap->target,
                tx->mm,
                tx->var_store,
                field);
            if (rc != IB_OK) {
                return rc;
            }
        }
    }

    return IB_OK;
}

ib_status_t ib_tx_flags_set(ib_tx_t *tx, ib_flags_t flag)
{
    tx->flags |= flag;
    return tx_var_flags_set(tx, flag, 1);
}

ib_status_t ib_tx_flags_unset(ib_tx_t *tx, ib_flags_t flag)
{
    tx->flags &= (~flag);
    return tx_var_flags_set(tx, flag, 0);
}

const ib_strval_t *ib_tx_flags_strval_first()
{
    return ib_tx_flags_map;
}

const char *ib_engine_sensor_id(const ib_engine_t *ib) {
    assert(ib != NULL);

    return ib->sensor_id;
}
