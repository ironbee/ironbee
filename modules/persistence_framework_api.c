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
 * @brief IronBee Engine --- Persistence Framework API Implementation
 *
 * @author Sam Baskinger <sbaskinger@qualys.com>
 */

#include "persistence_framework_private.h"

#include <ironbee/context.h>
#include <ironbee/engine.h>
#include <ironbee/engine_state.h>
#include <ironbee/module.h>
#include <ironbee/string.h>

#include <assert.h>

ib_status_t ib_persist_fw_cfg_create(
    ib_mm_t               mm,
    ib_persist_fw_cfg_t **persist_fw
)
{
    assert(persist_fw != NULL);

    ib_status_t rc;
    ib_persist_fw_cfg_t *persist_fw_out;

    /* Create persist_fw_cfg. */
    persist_fw_out = ib_mm_alloc(mm, sizeof(*persist_fw_out));
    if (persist_fw_out == NULL) {
        return IB_EALLOC;
    }

    /* Init persist_fw_cfg->handlers. */
    rc = ib_hash_create(&(persist_fw_out->handlers), mm);
    if (rc != IB_OK) {
        return rc;
    }

    /* Init persist_fw_cfg->stores. */
    rc = ib_hash_create(&(persist_fw_out->stores), mm);
    if (rc != IB_OK) {
        return rc;
    }

    /* Init persist_fw_cfg->coll_list. */
    rc = ib_list_create(&(persist_fw_out->coll_list), mm);
    if (rc != IB_OK) {
        return rc;
    }

    *persist_fw = persist_fw_out;
    return IB_OK;
}

/**
 * Get the per-context persist_fw_cfg.
 *
 * @a persist_fw_main and @c *persist_fw_cfg may reference the same address.
 * @a persist_fw_cfg will not be written too until the correct @ref
 * ib_persist_fw_cfg_t has been retrieved.
 *
 * @param[in] persist_fw_main Any ib_persist_fw_cfg_t for this module. This is
 *            used to find the correct per-context @ref ib_persist_fw_cfg_t.
 * @param[in] ctx The context that the proper @ref ib_persist_fw_cfg_t is
 *            found in.
 * @param[out] persist_fw_cfg This may be @c &persist_fw_main. This pointer will
 *             not be overwritten until the proper @ref ib_persist_fw_cfg_t
 *             is found.
 * @returns
 * - IB_OK On success.
 * - IB_ENOENT If no module is registered.
 * - Failure of ib_context_module_config() on error. An error is also logged
 *   using ib_persist_fw_cfg_t::ib.
 */
static ib_status_t get_ctx_persist_fw(
    ib_persist_fw_t      *persist_fw_main,
    ib_context_t         *ctx,
    ib_persist_fw_cfg_t **persist_fw_cfg
)
{
    assert(persist_fw_main != NULL);
    assert(persist_fw_main->ib != NULL);
    assert(persist_fw_main->user_module != NULL);
    assert(persist_fw_main->persist_fw_module != NULL);
    assert(persist_fw_cfg != NULL);
    assert(ctx != NULL);

    ib_persist_fw_cfg_t     *persist_fw_tmp = NULL;
    ib_status_t              rc;
    ib_persist_fw_modlist_t *cfg;

    rc = ib_context_module_config(
        ctx,
        persist_fw_main->persist_fw_module,
        &cfg);
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_array_get(
        cfg->configs,
        persist_fw_main->user_module->idx,
        &persist_fw_tmp);
    if (rc != IB_OK) {
        return rc;
    }

    *persist_fw_cfg = persist_fw_tmp;

    return IB_OK;
}

/**
 * Called by ib_psntsfw_create() to add a user's module config.
 *
 * The persistence framework keeps configuration information about
 * a user's module. This function adds space in the persistence
 * framework's configuration space for the user's module.
 *
 * @param[in] mm Memory manager to use.
 * @param[in] persist_fw The configuration handle we will pass back to the user.
 *
 * @returns
 * - IB_OK On success.
 * - Other on error.
 */
static ib_status_t add_module_config(
    ib_mm_t mm,
    ib_persist_fw_t *persist_fw
)
{
    assert(persist_fw != NULL);
    assert(persist_fw->ib != NULL);
    assert(persist_fw->user_module != NULL);
    assert(persist_fw->persist_fw_module != NULL);

    ib_engine_t             *ib             = persist_fw->ib;
    ib_persist_fw_cfg_t     *persist_fw_cfg = NULL;
    ib_persist_fw_modlist_t *cfg            = NULL;
    ib_context_t            *ctx            = ib_context_main(ib);
    ib_status_t              rc;

    rc = ib_persist_fw_cfg_create(mm, &persist_fw_cfg);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to create new persist_fw_cfg.");
        return rc;
    }

    /* Get main configuration context for the persistence framework module. */
    rc = ib_context_module_config(ctx, persist_fw->persist_fw_module, &cfg);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to fetch per-context persistence mappings.");
        return rc;
    }

    /* At the user's module's index in the persistence framework's
     * configuration, insert the empty persistence configuration. */
    rc = ib_array_setn(
        cfg->configs,
        persist_fw->user_module->idx,
        persist_fw_cfg);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to add config to persistence config.");
        return rc;
    }

    return IB_OK;
}


/**
 * When a context is selected, populate the transaction from the handlers.
 *
 * @param[in] ib IronBee engine.
 * @param[in] tx The transaction to populate.
 * @param[in] state The particular state.
 * @param[in] cbdata The original ib_persist_fw_cfg_t instance. We need
 *                   to use this to fetch the per-context
 *                   instance of @ref ib_persist_fw_cfg_t using
 *                   ib_persist_fw_cfg_t::module.
 *
 * @returns
 * - IB_OK On success.
 * - Other on error.
 */
static
ib_status_t populate_data_in_context(
    ib_engine_t *ib,
    ib_tx_t     *tx,
    ib_state_t   state,
    void        *cbdata
)
{
    assert(ib != NULL);
    assert(tx != NULL);
    assert(tx->conn != NULL);
    assert(tx->var_store != NULL);
    assert(state == request_header_finished_state);
    assert(cbdata != NULL);

    ib_persist_fw_t      *persist_fw     = (ib_persist_fw_t *)cbdata;
    ib_persist_fw_cfg_t  *persist_fw_cfg = NULL;
    ib_status_t           rc             = IB_OK;
    ib_context_t         *ctx;
    const ib_list_node_t *list_node;

    ctx = ib_context_get_context(ib, tx->conn, tx);
    if (ctx == NULL) {
        ib_log_error(ib, "There is no context available.");
        return IB_EOTHER;
    }

    rc = get_ctx_persist_fw(persist_fw, ctx, &persist_fw_cfg);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to retrieve persistence store.");
        return rc;
    }

    IB_LIST_LOOP_CONST(persist_fw_cfg->coll_list, list_node) {
        const ib_persist_fw_mapping_t *mapping =
            (const ib_persist_fw_mapping_t *)ib_list_node_data_const(list_node);

        /* Alias some values. */
        const char            *name       = mapping->name;
        ib_persist_fw_store_t *store      = mapping->store;
        const char            *key        = NULL;
        size_t                 key_length = 0;

        rc = ib_var_expand_execute(
            mapping->key_expand,
            &key, &key_length,
            tx->mm,
            tx->var_store
        );
        if (rc != IB_OK) {
            ib_log_error(
                ib,
                "Failed to expand key. "
                "Aborting population of collection %s.",
                name);
            continue;
        }

        if (store->handler->load_fn) {
            ib_list_t *list = NULL;
            ib_field_t *list_field = NULL;

            rc = ib_var_source_initialize(
                mapping->source,
                &list_field,
                tx->var_store,
                IB_FTYPE_LIST
            );
            if (rc != IB_OK) {
                ib_log_error(ib, "Failed to initialize list to populate.");
                continue;
            }

            rc = ib_field_value(list_field, ib_ftype_list_mutable_out(&list));
            if (rc != IB_OK) {
                ib_log_error(ib, "Failed to get list.");
                continue;
            }

            rc = store->handler->load_fn(
                store->impl,
                tx,
                key, key_length,
                list,
                store->handler->load_data);
            if (rc != IB_OK) {
                ib_log_debug(ib, "Failed to load collection %s", name);
            }
        }
    }

    return IB_OK;
}

/**
 * Persist the data written during @a tx in the appropriate context.
 *
 * @param[in] ib IronBee engine.
 * @param[in] tx The transaction to populate.
 * @param[in] state The particular state.
 * @param[in] cbdata The original @ref ib_persist_fw_cfg_t instance. We need
 *                   to use this to fetch the per-context
 *                   instance of @ref ib_persist_fw_cfg_t using
 *                   ib_persist_fw_cfg_t::module.
 * @returns
 * - IB_OK On success.
 * - Other on error.
 */
static
ib_status_t persist_data_in_context(
    ib_engine_t *ib,
    ib_tx_t     *tx,
    ib_state_t   state,
    void        *cbdata
)
{
    assert(ib != NULL);
    assert(tx != NULL);
    assert(tx->ctx != NULL);
    assert(state == handle_postprocess_state);
    assert(cbdata != NULL);

    ib_persist_fw_t     *persist_fw     = (ib_persist_fw_t *)cbdata;
    ib_persist_fw_cfg_t *persist_fw_cfg = NULL;
    const ib_list_node_t *list_node;
    ib_status_t rc = IB_OK;

    rc = get_ctx_persist_fw(persist_fw, tx->ctx, &persist_fw_cfg);
    if (rc != IB_OK) {
        ib_log_warning(ib, "Failed to retrieve persistence store.");
        return rc;
    }

    IB_LIST_LOOP_CONST(persist_fw_cfg->coll_list, list_node) {
        const ib_persist_fw_mapping_t *mapping =
            (const ib_persist_fw_mapping_t *)ib_list_node_data_const(list_node);

        /* Alias some values. */
        const char            *name   = mapping->name;
        ib_persist_fw_store_t *store  = mapping->store;
        const char            *key;
        size_t                 key_length;

        rc = ib_var_expand_execute(
            mapping->key_expand,
            &key, &key_length,
            tx->mm,
            tx->var_store
        );
        if (rc != IB_OK) {
            ib_log_error(
                ib,
                "Failed to expand key. "
                "Aborting persisting of collection %s.",
                name);
            continue;
        }

        if (store->handler->store_fn) {

            const ib_list_t *list = NULL;
            ib_field_t *list_field = NULL;

            rc = ib_var_source_get(
                mapping->source,
                &list_field,
                tx->var_store
            );
            if (rc != IB_OK) {
                ib_log_error(ib, "Failed to get list to store.");
                continue;
            }

            rc = ib_field_value(list_field, ib_ftype_list_out(&list));
            if (rc != IB_OK) {
                ib_log_error(ib, "Failed to get list.");
                continue;
            }

            rc = store->handler->store_fn(
                store->impl,
                tx,
                key, key_length, mapping->expiration,
                list,
                store->handler->store_data);
            if (rc != IB_OK) {
                ib_log_error(ib, "Failed to store collection %s.", name);
            }
        }
    }

    return IB_OK;
}

/**
 * Destroy persistence stores when their enclosing context is destroyed.
 *
 * @param[in] ib IronBee engine.
 * @param[in] ctx Context being destroyed.
 * @param[in] state The specific state. This is @ref context_destroy_state.
 * @param[in] cbdata An @ref ib_persist_fw_t.
 *
 * @returns
 * - IB_OK On success.
 * - Other on error.
 */
static
ib_status_t destroy_stores(
    ib_engine_t  *ib,
    ib_context_t *ctx,
    ib_state_t    state,
    void         *cbdata
)
{
    assert(ib != NULL);
    assert(ctx != NULL);
    assert(state == context_destroy_state);
    assert(cbdata != NULL);

    ib_persist_fw_t     *persist_fw     = (ib_persist_fw_t *)cbdata;
    ib_persist_fw_cfg_t *persist_fw_cfg = NULL;
    ib_mm_t              mm             = ib_engine_mm_main_get(ib);
    ib_status_t          rc;
    ib_hash_iterator_t  *itr            = ib_hash_iterator_create(mm);
    if (itr == NULL) {
        return IB_EALLOC;
    }

    rc = get_ctx_persist_fw(persist_fw, ctx, &persist_fw_cfg);
    if (rc != IB_OK) {
        return IB_OK;
    }

    for (
        ib_hash_iterator_first(itr, persist_fw_cfg->stores);
        ! ib_hash_iterator_at_end(itr);
        ib_hash_iterator_next(itr)
    )
    {
        const char         *key;
        size_t              keysz;
        ib_persist_fw_store_t *store;

        ib_hash_iterator_fetch(&key, &keysz, &store, itr);

        /* When a store is destroyed, the handler is NULLed.
         * Check that this store is not destroyed. */
        if ( (store != NULL) &&
             (store->handler != NULL) &&
             (store->handler->destroy_fn != NULL))
        {
            store->handler->destroy_fn(
                store->impl,
                store->handler->destroy_data);
            store->handler = NULL;
        }
    }

    return IB_OK;
}


ib_status_t ib_persist_fw_register_type(
    ib_persist_fw_t            *persist_fw,
    ib_context_t               *ctx,
    const char                 *type,
    ib_persist_fw_create_fn_t   create_fn,
    void                       *create_data,
    ib_persist_fw_destroy_fn_t  destroy_fn,
    void                       *destroy_data,
    ib_persist_fw_load_fn_t     load_fn,
    void                       *load_data,
    ib_persist_fw_store_fn_t    store_fn,
    void                       *store_data
)
{
    assert(persist_fw != NULL);
    assert(persist_fw->ib != NULL);
    assert(persist_fw->user_module != NULL);
    assert(persist_fw->persist_fw_module != NULL);
    assert(ctx != NULL);
    assert(type != NULL);

    ib_status_t           rc;
    ib_engine_t          *ib = persist_fw->ib;
    ib_persist_fw_cfg_t  *persist_fw_cfg = NULL;
    ib_mm_t               mm = ib_engine_mm_main_get(ib);
    ib_persist_fw_handler_t *handler;

    rc = get_ctx_persist_fw(persist_fw, ctx, &persist_fw_cfg);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to retrieve persistence store.");
        return rc;
    }

    handler = (ib_persist_fw_handler_t *)ib_mm_alloc(mm, sizeof(*handler));
    if (handler == NULL) {
        ib_log_error(ib, "Failed to allocate handler.");
        return IB_EALLOC;
    }

    handler->type         = ib_mm_strdup(mm, type);
    handler->create_fn    = create_fn;
    handler->create_data  = create_data;
    handler->destroy_fn   = destroy_fn;
    handler->destroy_data = destroy_data;
    handler->load_fn      = load_fn;
    handler->load_data    = load_data;
    handler->store_fn     = store_fn;
    handler->store_data   = store_data;

    if (handler->type == NULL) {
        ib_log_error(ib, "Failed to allocate handler type string.");
        return IB_EALLOC;
    }

    rc = ib_hash_get(persist_fw_cfg->handlers, NULL, type);
    if (rc == IB_OK) {
        ib_log_error(ib, "Handler for %s already exists.", type);
        return IB_EEXIST;
    }

    rc = ib_hash_set(persist_fw_cfg->handlers, type, handler);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to register handler for type %s.", type);
        return rc;
    }

    return IB_OK;
}

ib_status_t ib_persist_fw_map_collection(
    ib_persist_fw_t *persist_fw,
    ib_context_t    *ctx,
    const char      *name,
    const char      *key,
    size_t           key_length,
    ib_num_t         expiration,
    const char      *store_name
)
{
    assert(persist_fw != NULL);
    assert(persist_fw->ib != NULL);
    assert(persist_fw->user_module != NULL);
    assert(persist_fw->persist_fw_module != NULL);
    assert(ctx != NULL);
    assert(name != NULL);
    assert(key != NULL);
    assert(store_name != NULL);

    ib_status_t              rc;
    ib_engine_t             *ib             = persist_fw->ib;
    ib_persist_fw_cfg_t     *persist_fw_cfg = NULL;
    ib_mm_t                  mm             = ib_engine_mm_main_get(ib);
    ib_persist_fw_store_t   *store          = NULL;
    ib_persist_fw_mapping_t *mapping        = NULL;
    ib_var_expand_t         *expand         = NULL;
    ib_persist_fw_modlist_t *cfg;

    /* Get main configuration context for the persistence framework module. */
    rc = ib_context_module_config(ctx, persist_fw->persist_fw_module, &cfg);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to fetch per-context persistence mappings.");
        return rc;
    }

    rc = get_ctx_persist_fw(persist_fw, ctx, &persist_fw_cfg);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to retrieve persistence store.");
        return rc;
    }

    mapping = (ib_persist_fw_mapping_t *)ib_mm_alloc(mm, sizeof(*mapping));
    if (mapping == NULL) {
        ib_log_error(ib, "Failed to creating mapping %s.", name);
        return IB_EALLOC;
    }

    mapping->name = ib_mm_strdup(mm, name);
    if (mapping->name == NULL) {
        ib_log_error(ib, "Failed to copy mapping %s.", name);
        return IB_EALLOC;
    }

    /* Convert expiration in seconds to useconds and assign. */
    mapping->expiration = expiration * 1000000;

    rc = ib_var_source_register(
        &(mapping->source),
        ib_engine_var_config_get(ib),
        IB_S2SL(name),
        IB_PHASE_NONE, IB_PHASE_NONE
    );
    if (rc == IB_EEXIST) {
        rc = ib_var_source_acquire(
            &(mapping->source),
            mm,
            ib_engine_var_config_get(ib),
            IB_S2SL(name));
        if (rc != IB_OK) {
            ib_log_error(
                ib,
                "Failed to acquire previously registered source \"%s\"",
                name);
            return rc;
        }
    }
    /* Many sites may all be registering a var. EEXIST is OK. */
    else if (rc != IB_OK) {
        ib_log_error(ib, "Failed to register source for %s: %s",
                     name, ib_status_to_string(rc));
        return rc;
    }

    rc = ib_var_expand_acquire(
        &expand,
        mm,
        key,
        key_length,
        ib_engine_var_config_get(ib)
    );
    if (rc == IB_EINVAL) {
        ib_log_error(
            ib,
            "Failed to create expand for %s's key name %s",
            name,
            key);
        return rc;
    }
    else if (rc != IB_OK) {
        ib_log_error(
            ib,
            "Failed to create expansion for %s's key name %s.",
            name,
            key);
        return rc;
    }
    mapping->key_expand = expand;

    rc = ib_hash_get(persist_fw_cfg->stores, &store, store_name);
    if (rc != IB_OK) {
        ib_log_error(
            ib,
            "Failed to retrieve store %s for mapping %s.",
            store_name,
            name);
        return rc;
    }
    mapping->store = store;

    rc = ib_list_push(persist_fw_cfg->coll_list, mapping);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to store mapping in persistence config.");
        return rc;
    }

    return IB_OK;
}

ib_status_t ib_persist_fw_create(
    ib_engine_t      *ib,
    ib_module_t      *user_module,
    ib_persist_fw_t **persist_fw
)
{
    assert(ib != NULL);
    assert(user_module != NULL);
    assert(persist_fw != NULL);

    ib_persist_fw_t *persist_fw_out;
    ib_mm_t          mm = ib_engine_mm_main_get(ib);
    ib_status_t      rc;

    persist_fw_out = ib_mm_alloc(mm, sizeof(*persist_fw_out));
    if (persist_fw_out == NULL) {
        return IB_EALLOC;
    }

    persist_fw_out->ib          = ib;
    persist_fw_out->user_module = user_module;

    /* Assign persist_fw_out->persist_fw_module. */
    rc = ib_engine_module_get(
        ib,
        PERSISTENCE_FRAMEWORK_MODULE_NAME_STR,
        &(persist_fw_out->persist_fw_module));
    if (rc == IB_ENOENT) {
        ib_log_error(ib, "Persistence framework not loaded into engine.");
        return rc;
    }
    else if (rc != IB_OK) {
        ib_log_error(ib, "Failed to fetch persistence module information.");
        return rc;
    }

    /* Add the user's module to the persistence module's config. */
    rc = add_module_config(mm, persist_fw_out);
    if (rc != IB_OK) {
        return rc;
    }

    /* Register the callback for when the context is selected. */
    rc = ib_hook_tx_register(
        ib,
        request_header_finished_state,
        populate_data_in_context,
        persist_fw_out);
    if (rc != IB_OK) {
        return rc;
    }

    /* Register the callback for when the context is to be cleaned up. */
    rc = ib_hook_tx_register(
        ib,
        handle_postprocess_state,
        persist_data_in_context,
        persist_fw_out);
    if (rc != IB_OK) {
        return rc;
    }

    /* Register a callback that destroys stores in a context. */
    rc = ib_hook_context_register(
        ib,
        context_destroy_state,
        destroy_stores,
        persist_fw_out);
    if (rc != IB_OK) {
        return rc;
    }

    *persist_fw = persist_fw_out;

    return IB_OK;
}

ib_status_t ib_persist_fw_create_store(
    ib_persist_fw_t *persist_fw,
    ib_context_t    *ctx,
    const char      *type,
    const char      *name,
    const ib_list_t *params
)
{
    assert(persist_fw != NULL);
    assert(persist_fw->ib != NULL);
    assert(persist_fw->user_module != NULL);
    assert(persist_fw->persist_fw_module != NULL);
    assert(ctx != NULL);
    assert(type != NULL);
    assert(name != NULL);

    ib_status_t           rc;
    ib_engine_t          *ib = persist_fw->ib;
    ib_persist_fw_cfg_t  *persist_fw_cfg = NULL;
    ib_mm_t               mm = ib_engine_mm_main_get(ib);
    ib_persist_fw_store_t   *store;
    ib_persist_fw_handler_t *handler = NULL;

    rc = get_ctx_persist_fw(persist_fw, ctx, &persist_fw_cfg);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to retrieve persistence store.");
        return rc;
    }

    rc = ib_hash_get(persist_fw_cfg->handlers, &handler, type);
    if (rc != IB_OK) {
        ib_log_error(
            ib,
            "Failed to get handler for type %s to instantiate store %s.",
            type,
            name);
        return rc;
    }

    store = (ib_persist_fw_store_t *)ib_mm_alloc(mm, sizeof(*store));
    if (store == NULL) {
        ib_log_error(ib, "Failed to allocate store.");
        return IB_EALLOC;
    }

    store->handler = handler;
    store->impl = NULL;
    store->name = ib_mm_strdup(mm, name);
    if (store->name == NULL) {
        ib_log_error(ib, "Failed to copy store name %s", name);
        return IB_EALLOC;
    }

    if (handler->create_fn != NULL) {
        rc = handler->create_fn(
            ib,
            params,
            &(store->impl),
            handler->create_data);
        if (rc != IB_OK) {
            ib_log_error(
                ib, "Failed to instantiate store %s of type %s.", name, type);
            return rc;
        }
    }

    rc = ib_hash_get(persist_fw_cfg->stores, NULL, name);
    if (rc == IB_OK) {
        ib_log_error(ib, "Store %s already exists.", name);
        return IB_EEXIST;
    }

    rc = ib_hash_set(persist_fw_cfg->stores, name, store);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to record store %s.", name);
        return rc;
    }

    return IB_OK;
}
