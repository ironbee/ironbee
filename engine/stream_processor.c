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
 * @brief IronBee --- Stream Pump Implementation
 *
 * @author Sam Baskinger <sbaskinger@qualys.com>
 */

#include "ironbee_config_auto.h"

#include <ironbee/hash.h>
#include <ironbee/list.h>
#include <ironbee/mm_mpool_lite.h>
#include <ironbee/mpool_freeable.h>
#include <ironbee/mpool_lite.h>
#include <ironbee/stream_processor.h>

#include <assert.h>

/**
 * The definition used to create an @ref ib_stream_processor_t.
 *
 * This contains the name, types, and callback functions.
 */
struct ib_stream_processor_def_t {
    const char                     *name;           /**< Unique name. */
    ib_list_t                      *types;          /**< const char * types. */
    ib_stream_processor_create_fn   create_fn;      /**< Create function. */
    void                           *create_cbdata;  /**< Callback data. */
    ib_stream_processor_execute_fn  execute_fn;     /**< Execute function. */
    void                           *execute_cbdata; /**< Callback data. */
    ib_stream_processor_destroy_fn  destroy_fn;     /**< Destroy function. */
    void                           *destroy_cbdata; /**< Callback data. */
};
typedef struct ib_stream_processor_def_t ib_stream_processor_def_t;

/* An instance of a processor is just instance data and the definition. */
struct ib_stream_processor_t {
    void                            *instance_data; /**< Instance data. */
    const ib_stream_processor_def_t *def;           /**< Definition of this. */
};

ib_status_t ib_stream_processor_execute(
    ib_stream_processor_t *processor,
    ib_tx_t               *tx,
    ib_mm_t                mm_eval,
    ib_stream_io_tx_t     *io_tx
)
{
    assert(processor != NULL);
    assert(processor->def != NULL);
    assert(io_tx != NULL);

    ib_status_t rc;

    /* Execute. */
    rc = processor->def->execute_fn(
        processor->instance_data,
        tx,
        mm_eval,
        io_tx,
        processor->def->execute_cbdata
    );

    return rc;
}

const char * ib_stream_processor_name(
    ib_stream_processor_t *processor
)
{
    assert(processor != NULL);
    assert(processor->def != NULL);
    assert(processor->def->name != NULL);

    return processor->def->name;
}

const ib_list_t * ib_stream_processor_types(
    ib_stream_processor_t *processor
)
{
    assert(processor != NULL);
    assert(processor->def != NULL);
    assert(processor->def->types != NULL);

    return processor->def->types;
}

/* Store stream processor definitions by name and types. */
struct ib_stream_processor_registry_t {
    /**
     * Memory manager for this registry.
     */
    ib_mm_t    mm;

    /**
     * Map types to @ref ib_list_t of @ref ib_stream_processor_def_t.
     */
    ib_hash_t *processors_by_type;

    /**
     * Map names of @ref ib_stream_processor_def_t.
     */
    ib_hash_t *processor_by_name;
};

ib_status_t ib_stream_processor_registry_create(
    ib_stream_processor_registry_t **registry,
    ib_mm_t                          mm
)
{
    assert(registry != NULL);

    ib_stream_processor_registry_t *reg;
    ib_status_t                     rc;

    reg = (ib_stream_processor_registry_t *)ib_mm_alloc(mm, sizeof(*reg));
    if (reg == NULL) {
        return IB_EALLOC;
    }

    reg->mm = mm;
    rc = ib_hash_create_nocase(&reg->processors_by_type, mm);
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_hash_create_nocase(&reg->processor_by_name, mm);
    if (rc != IB_OK) {
        return rc;
    }

    *registry = reg;
    return IB_OK;
}

ib_status_t ib_stream_processor_registry_register(
    ib_stream_processor_registry_t *registry,
    const char                     *name,
    const ib_list_t                *types,
    ib_stream_processor_create_fn   create_fn,
    void                           *create_cbdata,
    ib_stream_processor_execute_fn  execute_fn,
    void                           *execute_cbdata,
    ib_stream_processor_destroy_fn  destroy_fn,
    void                           *destroy_cbdata
)
{
    assert(registry != NULL);
    assert(name != NULL);
    assert(types != NULL);
    assert(execute_fn != NULL);

    ib_stream_processor_def_t *def;
    ib_status_t                rc;
    const ib_list_node_t      *node;

    def = ib_mm_alloc(registry->mm, sizeof(*def));
    if (def == NULL) {
        return IB_EALLOC;
    }

    def->create_fn      = create_fn;
    def->create_cbdata  = create_cbdata;
    def->execute_fn     = execute_fn;
    def->execute_cbdata = execute_cbdata;
    def->destroy_fn     = destroy_fn;
    def->destroy_cbdata = destroy_cbdata;
    def->name           = ib_mm_strdup(registry->mm, name);
    if (def->name == NULL) {
        return IB_EALLOC;
    }
    rc = ib_list_create(&def->types, registry->mm);
    if (rc != IB_OK) {
        return rc;
    }

    /* Make sure that we aren't redefining a previously defined name. */
    rc = ib_hash_get(registry->processor_by_name, NULL, name);
    if (rc == IB_OK || rc != IB_ENOENT) {
        return IB_EINVAL;
    }

    /* Bind name to this definition. */
    rc = ib_hash_set(registry->processor_by_name, name, def);
    if (rc != IB_OK) {
        return rc;
    }

    /* Register this definition under all types listed in @a types.*/
    IB_LIST_LOOP_CONST(types, node) {
        ib_list_t  *processors;

        /* Copy the type because we will be storing it in def->types. */
        const char *type = ib_mm_strdup(
            registry->mm,
            (const char *)ib_list_node_data_const(node));
        if (type == NULL) {
            return IB_EALLOC;
        }

        /* Push copied type string to an internal list. */
        rc = ib_list_push(def->types, (void *)type);
        if (rc != IB_OK) {
            return rc;
        }

        /* Find the types list. */
        rc = ib_hash_get(registry->processors_by_type, &processors, type);
        if (rc == IB_ENOENT) {
            /* If the type list is not found, make it. */
            rc = ib_list_create(&processors, registry->mm);
            if (rc != IB_OK) {
                return rc;
            }

            /* Add the new type list into the registry. */
            rc = ib_hash_set(registry->processors_by_type, type, processors);
            if (rc != IB_OK) {
                return rc;
            }
        }
        /* Fail on errors that aren't IB_ENOENT. */
        else if (rc != IB_OK) {
            return rc;
        }

        /* Finally, push the processor to the list. */
        rc = ib_list_push(processors, def);
        if (rc != IB_OK) {
            return rc;
        }
    }

    return IB_OK;
}

static void stream_processor_destroy(void *cbdata) {

    ib_stream_processor_t *inst = (ib_stream_processor_t *)cbdata;

    assert(inst != NULL);
    assert(inst->def != NULL);

    if (inst->def->destroy_fn != NULL) {
        inst->def->destroy_fn(
            inst->instance_data,
            inst->def->destroy_cbdata
        );
    }
}

ib_status_t ib_stream_processor_registry_processor_create(
    ib_stream_processor_registry_t  *registry,
    const char                      *name,
    ib_stream_processor_t          **processor,
    ib_tx_t                         *tx
)
{
    ib_stream_processor_def_t *def;
    ib_stream_processor_t     *inst;
    ib_status_t                rc;

    rc = ib_hash_get(registry->processor_by_name, &def, name);
    if (rc != IB_OK) {
        return rc;
    }

    inst = ib_mm_alloc(tx->mm, sizeof(*inst));
    if (inst == NULL) {
        return IB_EALLOC;
    }

    /* If there is user create function, call it. */
    if (def->create_fn != NULL) {
        rc = def->create_fn(&inst->instance_data, tx, def->create_cbdata);
        if (rc != IB_OK) {
            return rc;
        }
    }

    rc = ib_mm_register_cleanup(tx->mm, stream_processor_destroy, inst);
    if (rc != IB_OK) {
        return rc;
    }

    inst->def = def;

    *processor = inst;
    return IB_OK;
}

ib_status_t ib_stream_processor_registry_names_find(
    ib_stream_processor_registry_t  *registry,
    const char                      *type,
    ib_list_t                       *names
)
{
    assert(registry != NULL);
    assert(type != NULL);
    assert(names != NULL);

    ib_list_t      *list;
    ib_list_node_t *node;
    ib_status_t     rc;

    /* Get the list of definitions under a type. */
    rc = ib_hash_get(registry->processors_by_type, &list, type);
    if (rc != IB_OK) {
        return rc;
    }

    /* For each stream processor definition... */
    IB_LIST_LOOP(list, node) {
        ib_stream_processor_def_t *def;

        def = (ib_stream_processor_def_t *)ib_list_node_data(node);

        rc = ib_list_push(names, (void *)def->name);
        if (rc != IB_OK) {
            return rc;
        }
    }

    return IB_OK;
}
