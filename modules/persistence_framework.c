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
 * @brief IronBee Engine --- Persistence Framework Implementation
 *
 * @author Sam Baskinger <sbaskinger@qualys.com>
 */

#include "persistence_framework_private.h"

#include <ironbee/context.h>
#include <ironbee/engine.h>
#include <ironbee/engine_state.h>
#include <ironbee/module.h>

#include <assert.h>

/* Module boiler plate */
#define MODULE_NAME persistence_framework
#define MODULE_NAME_STR IB_XSTRINGIFY(MODULE_NAME)
IB_MODULE_DECLARE();

/**
 * Get the per-context pstnsfw_cfg.
 *
 * @a pstnsfw_main and @c *pstnsfw_cfg may referece the same address.
 * @a pstnsfw_cfg will not be written too until the correct @ref ib_pstnsfw_cfg_t
 * has been retrieved.
 *
 * @param[in] pstnsfw_main Any ib_pstnsfw_cfg_t for this module. This is
 *            used to find the correct per-context @ref ib_pstnsfw_cfg_t.
 * @param[in] ctx The context that the proper @ref ib_pstnsfw_cfg_t is
 *            found in.
 * @param[out] pstnsfw_cfg This may be @c &pstnsfw_main. This pointer will
 *             not be overwritten until the proper @ref ib_pstnsfw_cfg_t is found.
 * @returns
 * - IB_OK On success.
 * - IB_ENOENT If no module is registered.
 * - Failure of ib_context_module_config() on error. An error is also logged
 *   using ib_pstnsfw_cfg_t::ib.
 */
static ib_status_t get_ctx_pstnsfw(
    ib_pstnsfw_t  *pstnsfw_main,
    ib_context_t  *ctx,
    ib_pstnsfw_cfg_t **pstnsfw_cfg
)
{
    assert(pstnsfw_main != NULL);
    assert(pstnsfw_main->ib != NULL);
    assert(pstnsfw_main->user_module != NULL);
    assert(pstnsfw_main->pstnsfw_module != NULL);
    assert(pstnsfw_cfg != NULL);
    assert(ctx != NULL);

    ib_pstnsfw_cfg_t     *pstnsfw_tmp = NULL;
    ib_status_t           rc;
    ib_pstnsfw_modlist_t *configs;

    rc = ib_context_module_config(
        ctx,
        pstnsfw_main->pstnsfw_module,
        &configs);
    if (rc != IB_OK) {
        ib_log_error(
            pstnsfw_main->ib,
            "Failed to fetch per-context persistence mappings.");
        return rc;
    }

    rc = ib_array_get(
        configs->configs,
        pstnsfw_main->user_module->idx,
        &pstnsfw_tmp);
    if (rc == IB_EINVAL || pstnsfw_tmp == NULL) {
        ib_log_error(
            pstnsfw_main->ib,
            "No module registration in persistence framework.");
        return IB_ENOENT;
    }
    if (rc != IB_OK) {
        ib_log_error(
            pstnsfw_main->ib,
            "Failed to fetch per-context persistence mappings.");
        return rc;
    }

    *pstnsfw_cfg = pstnsfw_tmp;

    return IB_OK;
}

/**
 * When a context is selected, populate the transaction from the handlers.
 *
 * @param[in] ib IronBee engine.
 * @param[in] tx The transaction to populate.
 * @param[in] event The particular event.
 * @param[in] cbdata The original ib_pstnsfw_cfg_t instance. We need
 *                   to use this to fetch the per-context
 *                   instance of @ref ib_pstnsfw_cfg_t using
 *                   ib_pstnsfw_cfg_t::module.
 *
 * @returns
 * - IB_OK On success.
 * - Other on error.
 */
static ib_status_t populate_context(
    ib_engine_t           *ib,
    ib_tx_t               *tx,
    ib_state_event_type_t  event,
    void                  *cbdata
)
{
    assert(ib != NULL);
    assert(tx != NULL);
    assert(tx->ctx != NULL);
    assert(tx->data != NULL);
    assert(tx->mp != NULL);
    assert(event == handle_context_tx_event);
    assert(cbdata != NULL);

    ib_pstnsfw_t     *pstnsfw = (ib_pstnsfw_t *)cbdata;
    ib_pstnsfw_cfg_t *pstnsfw_cfg = NULL;
    const ib_list_node_t *list_node;
    ib_status_t rc = IB_OK;

    rc = get_ctx_pstnsfw(pstnsfw, tx->ctx, &pstnsfw_cfg);
    if (rc != IB_OK) {
        return rc;
    }

    IB_LIST_LOOP_CONST(pstnsfw_cfg->coll_list, list_node) {
        const ib_pstnsfw_mapping_t *mapping =
            (const ib_pstnsfw_mapping_t *)ib_list_node_data_const(list_node);

        /* Alias some values. */
        const char         *name   = mapping->name;
        ib_pstnsfw_store_t *store  = mapping->store;
        const char         *key    = NULL;
        bool                expand = false;

        ib_data_expand_test_str(key, &expand);
        if (expand) {
            char *ex_key = NULL;
            rc = ib_data_expand_str(
                tx->data,
                mapping->key,
                true,
                &ex_key);
            if (rc != IB_OK) {
                ib_log_error(
                    ib,
                    "Failed to expand key. "
                    "Aborting population of collection %s.",
                    name);
                continue;
            }
            key = ex_key;
        }
        else {
            key = mapping->key;
        }

        if (store->handler->load_fn) {
            ib_list_t *list = NULL;
            ib_field_t *list_field = NULL;

            rc = ib_data_add_list(tx->data, name, &list_field);
            if (rc != IB_OK) {
                ib_log_error(ib, "Failed to create list to populate.");
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
                key,
                list,
                store->handler->load_data);
            if (rc != IB_OK) {
                ib_log_error(ib, "Failed to load collection %s", name);
            }
        }
        else {
            ib_log_debug(
                ib,
                "Mapping for collection %s has no load handler. Skipping.",
                name);
        }
    }

    return IB_OK;
}

/**
 * Persist the data written during @a tx in the appropriate context.
 *
 * @param[in] ib IronBee engine.
 * @param[in] tx The transaction to populate.
 * @param[in] event The particular event.
 * @param[in] cbdata The original ib_pstnsfw_cfg_t instance. We need
 *                   to use this to fetch the per-context
 *                   instance of @ref ib_pstnsfw_cfg_t using
 *                   ib_pstnsfw_cfg_t::module.
 * @returns
 * - IB_OK On success.
 * - Other on error.
 */
static ib_status_t persist_context(
    ib_engine_t           *ib,
    ib_tx_t               *tx,
    ib_state_event_type_t  event,
    void                  *cbdata
)
{
    assert(ib != NULL);
    assert(tx != NULL);
    assert(tx->ctx != NULL);
    assert(event == handle_postprocess_event);
    assert(cbdata != NULL);

    ib_pstnsfw_t     *pstnsfw     = (ib_pstnsfw_t *)cbdata;
    ib_pstnsfw_cfg_t *pstnsfw_cfg = NULL;
    const ib_list_node_t *list_node;
    ib_status_t rc = IB_OK;

    rc = get_ctx_pstnsfw(pstnsfw, tx->ctx, &pstnsfw_cfg);
    if (rc != IB_OK) {
        return rc;
    }

    IB_LIST_LOOP_CONST(pstnsfw_cfg->coll_list, list_node) {
        const ib_pstnsfw_mapping_t *mapping =
            (const ib_pstnsfw_mapping_t *)ib_list_node_data_const(list_node);

        /* Alias some values. */
        const char         *name   = mapping->name;
        ib_pstnsfw_store_t *store  = mapping->store;
        const char         *key    = NULL;
        bool                expand = false;

        ib_data_expand_test_str(key, &expand);
        if (expand) {
            char *ex_key = NULL;
            rc = ib_data_expand_str(
                tx->data,
                mapping->key,
                true,
                &ex_key);
            if (rc != IB_OK) {
                ib_log_error(
                    ib,
                    "Failed to expand key. "
                    "Aborting persisting of collection %s.",
                    name);
                continue;
            }
            key = ex_key;
        }
        else {
            key = mapping->key;
        }

        if (store->handler->store_fn) {

            const ib_list_t *list = NULL;
            ib_field_t *list_field = NULL;

            rc = ib_data_get(tx->data, name, &list_field);
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
                key,
                list,
                store->handler->store_data);
            if (rc != IB_OK) {
                ib_log_error(ib, "Failed to store collection %s", name);
            }
        }
        else {
            ib_log_debug(
                ib,
                "Mapping for collection %s has no store handler. Skipping.",
                name);
        }
    }

    return IB_OK;
}

static ib_status_t pstnsfw_cfg_create(
    ib_mpool_t       *mp,
    ib_pstnsfw_cfg_t **pstnsfw
)
{
    assert(mp != NULL);
    assert(pstnsfw != NULL);

    ib_status_t rc;
    ib_pstnsfw_cfg_t *pstnsfw_out;

    /* Create pstnsfw_cfg. */
    pstnsfw_out = ib_mpool_alloc(mp, sizeof(*pstnsfw_out));
    if (pstnsfw_out == NULL) {
        return IB_EALLOC;
    }

    /* Init pstnsfw_cfg->handlers. */
    rc = ib_hash_create(&(pstnsfw_out->handlers), mp);
    if (rc != IB_OK) {
        return rc;
    }

    /* Init pstnsfw_cfg->stores. */
    rc = ib_hash_create(&(pstnsfw_out->stores), mp);
    if (rc != IB_OK) {
        return rc;
    }

    /* Init pstnsfw_cfg->coll_list. */
    rc = ib_list_create(&(pstnsfw_out->coll_list), mp);
    if (rc != IB_OK) {
        return rc;
    }

    *pstnsfw = pstnsfw_out;
    return IB_OK;
}


static ib_status_t cpy_psntsfw_cfg(
    ib_engine_t             *ib,
    ib_mpool_t              *mp,
    ib_mpool_t              *local_mp,
    const ib_pstnsfw_cfg_t  *pstnsfw_src,
    ib_pstnsfw_cfg_t       **pstnsfw_dst
)
{
    assert(ib != NULL);
    assert(mp != NULL);
    assert(local_mp != NULL);
    assert(pstnsfw_src != NULL);
    assert(pstnsfw_dst != NULL);

    ib_list_t      *list = NULL;
    ib_list_node_t *list_node;
    ib_status_t     rc;
    ib_pstnsfw_cfg_t *pstnsfw_out;

    rc = pstnsfw_cfg_create(mp, &pstnsfw_out);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to create new pstnsfw_cfg.");
        return rc;
    }

    rc = ib_list_create(&list, local_mp);
    if (rc != IB_OK) {
        return rc;
    }

    /* Copy the src store hash to the dst store hash. */
    rc = ib_hash_get_all(pstnsfw_src->stores, list);
    if (rc != IB_OK) {
        return rc;
    }
    IB_LIST_LOOP(list, list_node) {
        ib_pstnsfw_store_t *store =
            (ib_pstnsfw_store_t *)ib_list_node_data_const(list_node);
        rc = ib_hash_set(pstnsfw_out->stores, store->name, store);
        if (rc != IB_OK) {
            return rc;
        }
    }
    ib_list_clear(list);

    /* Copy the src handlers hash to the dst handlers hash. */
    rc = ib_hash_get_all(pstnsfw_src->handlers, list);
    if (rc != IB_OK) {
        return rc;
    }
    IB_LIST_LOOP(list, list_node) {
        ib_pstnsfw_handler_t *handler =
            (ib_pstnsfw_handler_t *)ib_list_node_data_const(list_node);
        rc = ib_hash_set(pstnsfw_out->handlers, handler->type, handler);
        if (rc != IB_OK) {
            return rc;
        }
    }
    ib_list_clear(list);

    /* Copy the list of mappings. */
    IB_LIST_LOOP(pstnsfw_src->coll_list, list_node) {
        rc = ib_list_push(pstnsfw_out->coll_list, ib_list_node_data(list_node));
        if (rc != IB_OK) {
            return rc;
        }
    }

    *pstnsfw_dst = pstnsfw_out;
    return IB_OK;
}

/**
 * Copy a @ref ib_pstnsfw_cfg_t.
 *
 * Because the persistence framework must be configuration context aware,
 * it registers every instance of itself as a module.
 * That modules knows how to copy its configuration information.
 *
 * @returns
 * - IB_OK On success.
 * - Other on error.
 */
static ib_status_t cpy_pstnsfw(
    ib_engine_t *ib,
    ib_module_t *module,
    void *dst,
    const void *src,
    size_t length,
    void *cbdata
)
{
    assert(ib != NULL);
    assert(module != NULL);
    assert(src != NULL);
    assert(dst != NULL);
    assert(length == sizeof(ib_pstnsfw_modlist_t));

    const ib_pstnsfw_modlist_t *src_cfg = (ib_pstnsfw_modlist_t *)src;
    ib_pstnsfw_modlist_t       *dst_cfg = (ib_pstnsfw_modlist_t *)dst;
    ib_mpool_t                 *mp = ib_engine_pool_main_get(ib);
    ib_status_t                 rc;
    ib_mpool_t                 *local_mp;
    size_t                      ne;
    size_t                      idx;
    ib_pstnsfw_cfg_t           *pstnsfw_src = NULL;


    /* Shallow copy. Now we overwrite bits we need to manually duplicate. */
    memcpy(dst, src, length);

    /* Create a local memory pool, cleared at the end of this function. */
    rc = ib_mpool_create(&local_mp, "local mp", mp);
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_array_create(&(dst_cfg->configs), mp, 1, 2);
    if (rc != IB_OK) {
        return rc;
    }

    IB_ARRAY_LOOP(src_cfg->configs, ne, idx, pstnsfw_src) {
        ib_pstnsfw_cfg_t *pstnsfw_dst = NULL;

        rc = cpy_psntsfw_cfg(ib, mp, local_mp, pstnsfw_src, &pstnsfw_dst);
        if (rc != IB_OK) {
            ib_log_error(ib, "Failed to copy configuration into new context.");
            goto exit;
        }

        rc = ib_array_setn(dst_cfg->configs, idx, pstnsfw_dst);
        if (rc != IB_OK) {
            ib_log_error(ib, "Failed to copy configuration into new context.");
            goto exit;
        }
    }

exit:
    ib_mpool_release(local_mp);
    return rc;
}

static ib_status_t destroy_stores(
    ib_engine_t           *ib,
    ib_context_t          *ctx,
    ib_state_event_type_t  event,
    void                  *cbdata
)
{
    assert(ib != NULL);
    assert(ctx != NULL);
    assert(event == context_destroy_event);
    assert(cbdata != NULL);

    ib_pstnsfw_t       *pstnsfw     = (ib_pstnsfw_t *)cbdata;
    ib_pstnsfw_cfg_t   *pstnsfw_cfg = NULL;
    ib_mpool_t         *mp          = ib_engine_pool_main_get(ib);
    ib_status_t         rc;
    ib_hash_iterator_t *itr         = ib_hash_iterator_create(mp);
    if (itr == NULL) {
        return IB_EALLOC;
    }

    rc = get_ctx_pstnsfw(pstnsfw, ctx, &pstnsfw_cfg);
    if (rc != IB_OK) {
        return rc;
    }

    for (
        ib_hash_iterator_first(itr, pstnsfw_cfg->stores);
        ! ib_hash_iterator_at_end(itr);
        ib_hash_iterator_next(itr)
    )
    {
        const char         *key;
        size_t              keysz;
        ib_pstnsfw_store_t *store;

        ib_hash_iterator_fetch(&key, &keysz, &store, itr);

        /* When a store is destroyed, the handler is NULLed.
         * Check that this store is not destroyed. */
        if (store != NULL && store->handler != NULL) {
            store->handler->destroy_fn(
                store->impl,
                store->handler->destroy_data);
            store->handler = NULL;
        }
    }

    return IB_OK;
}

/**
 * Called by ib_psntsfw_create() to add a user's module config.
 *
 * The persistence framework keeps configuration information about
 * a user's module. This function adds space in the persistence
 * framework's configuration space for the user's module.
 *
 * @param[in] mp Memory pool to use.
 * @param[in] pstnsfw The configuration handle we will pass back to the user.
 *
 * @returns
 * - IB_OK On success.
 * - Other on error.
 */
static ib_status_t add_module_config(
    ib_mpool_t *mp,
    ib_pstnsfw_t *pstnsfw
)
{
    assert(mp != NULL);
    assert(pstnsfw != NULL);
    assert(pstnsfw->ib != NULL);
    assert(pstnsfw->user_module != NULL);
    assert(pstnsfw->pstnsfw_module != NULL);

    ib_engine_t          *ib          = pstnsfw->ib;
    ib_pstnsfw_cfg_t     *pstnsfw_cfg = NULL;
    ib_pstnsfw_modlist_t *cfg         = NULL;
    ib_context_t         *ctx         = ib_context_main(ib);
    ib_status_t           rc;

    rc = pstnsfw_cfg_create(mp, &pstnsfw_cfg);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to create new pstnsfw_cfg.");
        return rc;
    }

    /* Get main configuration context for the persistence framework module. */
    rc = ib_context_module_config(ctx, pstnsfw->pstnsfw_module, &cfg);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to fetch per-context persistence mappings.");
        return rc;
    }

    /* At the user's module's index in the persistence framework's
     * configuration, insert the empty persistence configuration. */
    rc = ib_array_setn(cfg->configs, pstnsfw->user_module->idx, pstnsfw_cfg);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to add config to persistence config.");
        return rc;
    }

    return IB_OK;
}

ib_status_t ib_pstnsfw_create(
    ib_engine_t   *ib,
    ib_module_t   *user_module,
    ib_pstnsfw_t **pstnsfw
)
{
    assert(ib != NULL);
    assert(user_module != NULL);
    assert(pstnsfw != NULL);

    ib_pstnsfw_t *pstnsfw_out;
    ib_mpool_t   *mp = ib_engine_pool_main_get(ib);
    ib_status_t   rc;

    pstnsfw_out = ib_mpool_alloc(mp, sizeof(*pstnsfw_out));
    if (pstnsfw_out == NULL) {
        return IB_EALLOC;
    }

    pstnsfw_out->user_module = user_module;

    /* Assign pstnsfw_out->pstnsfw_module. */
    rc = ib_engine_module_get(
        ib,
        MODULE_NAME_STR,
        &(pstnsfw_out->pstnsfw_module));
    if (rc == IB_ENOENT) {
        ib_log_error(ib, "Persistence framework not loaded into engine.");
        return rc;
    }
    else if (rc != IB_OK) {
        ib_log_error(ib, "Failed to fetch persistence module information.");
        return rc;
    }

    /* Add the user's module to the persistence module's config. */
    rc = add_module_config(mp, pstnsfw_out);

    /* Register the callback for when the context is selected. */
    rc = ib_hook_tx_register(
        ib,
        handle_context_tx_event,
        populate_context,
        pstnsfw_out);
    if (rc != IB_OK) {
        return rc;
    }

    /* Register the callback for when the context is to be cleaned up. */
    rc = ib_hook_tx_register(
        ib,
        handle_postprocess_event,
        persist_context,
        pstnsfw_out);
    if (rc != IB_OK) {
        return rc;
    }

    /* Register a callback that destroys stores in a context. */
    rc = ib_hook_context_register(
        ib,
        context_destroy_event,
        destroy_stores,
        pstnsfw_out);
    if (rc != IB_OK) {
        return rc;
    }

    *pstnsfw = pstnsfw_out;

    return IB_OK;
}

ib_status_t ib_pstnsfw_register_type(
    ib_pstnsfw_t            *pstnsfw,
    ib_context_t            *ctx,
    const char              *type,
    ib_pstnsfw_create_fn_t   create_fn,
    void                    *create_data,
    ib_pstnsfw_destroy_fn_t  destroy_fn,
    void                    *destroy_data,
    ib_pstnsfw_load_fn_t     load_fn,
    void                    *load_data,
    ib_pstnsfw_store_fn_t    store_fn,
    void                    *store_data
)
{
    assert(pstnsfw != NULL);
    assert(pstnsfw->ib != NULL);
    assert(pstnsfw->user_module != NULL);
    assert(pstnsfw->pstnsfw_module != NULL);
    assert(ctx != NULL);
    assert(type != NULL);

    ib_status_t           rc;
    ib_engine_t          *ib = pstnsfw->ib;
    ib_pstnsfw_cfg_t     *pstnsfw_cfg = NULL;
    ib_mpool_t           *mp = ib_engine_pool_main_get(ib);
    ib_pstnsfw_handler_t *handler;

    rc = get_ctx_pstnsfw(pstnsfw, ctx, &pstnsfw_cfg);
    if (rc != IB_OK) {
        return rc;
    }

    handler = (ib_pstnsfw_handler_t *)ib_mpool_alloc(mp, sizeof(*handler));
    if (handler == NULL) {
        ib_log_error(ib, "Failed to allocate handler.");
        return IB_EALLOC;
    }

    handler->type         = ib_mpool_strdup(mp, type);
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

    rc = ib_hash_get(pstnsfw_cfg->handlers, NULL, type);
    if (rc == IB_OK) {
        ib_log_error(ib, "Handler for %s already exists.", type);
        return IB_EEXIST;
    }

    rc = ib_hash_set(pstnsfw_cfg->handlers, type, handler);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to register handler for type %s.", type);
        return rc;
    }

    return IB_OK;
}

ib_status_t ib_pstnsfw_create_store(
    ib_pstnsfw_t *pstnsfw,
    ib_context_t *ctx,
    const char   *type,
    const char   *name,
    ib_list_t    *params
)
{
    assert(pstnsfw != NULL);
    assert(pstnsfw->ib != NULL);
    assert(pstnsfw->user_module != NULL);
    assert(pstnsfw->pstnsfw_module != NULL);
    assert(ctx != NULL);
    assert(type != NULL);
    assert(name != NULL);

    ib_status_t           rc;
    ib_engine_t          *ib = pstnsfw->ib;
    ib_pstnsfw_cfg_t     *pstnsfw_cfg = NULL;
    ib_mpool_t           *mp = ib_engine_pool_main_get(ib);
    ib_pstnsfw_store_t   *store;
    ib_pstnsfw_handler_t *handler = NULL;

    rc = get_ctx_pstnsfw(pstnsfw, ctx, &pstnsfw_cfg);
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_hash_get(pstnsfw_cfg->handlers, &handler, type);
    if (rc != IB_OK) {
        ib_log_error(
            ib,
            "Failed to get handler for type %s to instantiate store %s.",
            type,
            name);
        return rc;
    }

    store = (ib_pstnsfw_store_t *)ib_mpool_alloc(mp, sizeof(*store));
    if (store == NULL) {
        ib_log_error(ib, "Failed to allocate store.");
        return IB_EALLOC;
    }

    store->handler = handler;
    store->impl = NULL;
    store->name = ib_mpool_strdup(mp, name);
    if (store->name == NULL) {
        ib_log_error(ib, "Failed to cpy store name %s", name);
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
        }
    }

    rc = ib_hash_get(pstnsfw_cfg->stores, NULL, name);
    if (rc == IB_OK) {
        ib_log_error(ib, "Store %s already exists.", name);
        return IB_EEXIST;
    }

    rc = ib_hash_set(pstnsfw_cfg->stores, name, store);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to record store %s.", name);
        return rc;
    }

    return IB_OK;
}

ib_status_t DLL_PUBLIC ib_pstnsfw_map_collection(
    ib_pstnsfw_t *pstnsfw,
    ib_context_t *ctx,
    const char   *name,
    const char   *key,
    const char   *store_name
)
{
    assert(pstnsfw != NULL);
    assert(pstnsfw->ib != NULL);
    assert(pstnsfw->user_module != NULL);
    assert(pstnsfw->pstnsfw_module != NULL);
    assert(ctx != NULL);
    assert(name != NULL);
    assert(key != NULL);
    assert(store_name != NULL);

    ib_status_t           rc;
    ib_engine_t          *ib = pstnsfw->ib;
    ib_pstnsfw_cfg_t     *pstnsfw_cfg = NULL;
    ib_mpool_t           *mp = ib_engine_pool_main_get(ib);
    ib_pstnsfw_store_t   *store = NULL;
    ib_pstnsfw_mapping_t *mapping;

    rc = get_ctx_pstnsfw(pstnsfw, ctx, &pstnsfw_cfg);
    if (rc != IB_OK) {
        return rc;
    }

    mapping = (ib_pstnsfw_mapping_t *)ib_mpool_alloc(mp, sizeof(*mapping));
    if (mapping == NULL) {
        ib_log_error(ib, "Failed to creating mapping %s.", name);
        return IB_EALLOC;
    }

    mapping->name = ib_mpool_strdup(mp, name);
    if (mapping->name == NULL) {
        ib_log_error(ib, "Failed to copy mapping %s.", name);
        return IB_EALLOC;
    }

    mapping->key = ib_mpool_strdup(mp, key);
    if (mapping->key == NULL) {
        ib_log_error(ib, "Failed to copy mapping %s's key name %s.", name, key);
        return IB_EALLOC;
    }

    rc = ib_hash_get(pstnsfw_cfg->stores, &store, store_name);
    if (rc != IB_OK) {
        ib_log_error(
            ib,
            "Failed to retrieve store %s for mapping %s.",
            store_name,
            name);
        return rc;
    }
    mapping->store = store;

    rc = ib_list_push(pstnsfw_cfg->coll_list, mapping);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to store mapping in persistence config.");
        return rc;
    }

    return IB_OK;
}

/**
 * Module initialization.
 *
 * @param[in] ib IronBee engine.
 * @param[in] module Module structure.
 * @param[in] cbdata Callback data.
 *
 * @returns
 * - IB_OK On success.
 */
static ib_status_t persistence_framework_init(
    ib_engine_t *ib,
    ib_module_t *module,
    void *cbdata
)
{
    assert(ib != NULL);
    assert(module != NULL);

    ib_status_t   rc;
    ib_mpool_t   *mp = ib_engine_pool_main_get(ib);
    ib_pstnsfw_modlist_t *cfg;

    cfg = ib_mpool_alloc(mp, sizeof(*cfg));
    if (cfg == NULL) {
        ib_log_error(ib, "Failed to allocate module configuration.");
        return IB_EALLOC;
    }

    rc = ib_array_create(&(cfg->configs), mp, 1, 2);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to create configs hash.");
        return rc;
    }

    /* Set the main context module configuration. */
    rc = ib_module_config_initialize(module, cfg, sizeof(*cfg));
    if (rc != IB_OK) {
        ib_log_error(ib, "Cannot set module configuration.");
        return rc;
    }

    return IB_OK;
}

/**
 * Module destruction.
 *
 * @param[in] ib IronBee engine.
 * @param[in] module Module structure.
 * @param[in] cbdata Callback data.
 *
 * @returns
 * - IB_OK On success.
 */
static ib_status_t persistence_framework_fini(
    ib_engine_t *ib,
    ib_module_t *module,
    void *cbdata
)
{
    return IB_OK;
}


IB_MODULE_INIT(
    IB_MODULE_HEADER_DEFAULTS,    /* Headeer defaults. */
    MODULE_NAME_STR,              /* Module name. */
    NULL,                         /* Configuration. Dyanmically set in init. */
    0,                            /* Config length is 0. */
    cpy_pstnsfw,                  /* Configuration copy function. */
    NULL,                         /* Callback data. */
    NULL,                         /* Config map. */
    NULL,                         /* Directive map. */
    persistence_framework_init,   /* Initialization. */
    NULL,                         /* Callback data. */
    persistence_framework_fini,   /* Finalization. */
    NULL,                         /* Callback data. */
);
