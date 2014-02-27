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
#include <ironbee/mm_mpool_lite.h>
#include <ironbee/module.h>

#include <assert.h>

/* Module boiler plate */
#define MODULE_NAME     PERSISTENCE_FRAMEWORK_MODULE_NAME
#define MODULE_NAME_STR PERSISTENCE_FRAMEWORK_MODULE_NAME_STR
IB_MODULE_DECLARE();

static ib_status_t cpy_psntsfw_cfg(
    ib_engine_t                *ib,
    ib_mm_t                     mm,
    ib_mm_t                     local_mm,
    const ib_persist_fw_cfg_t  *persist_fw_src,
    ib_persist_fw_cfg_t       **persist_fw_dst
)
{
    assert(ib != NULL);
    assert(persist_fw_src != NULL);
    assert(persist_fw_dst != NULL);

    ib_list_t      *list = NULL;
    ib_list_node_t *list_node;
    ib_status_t     rc;
    ib_persist_fw_cfg_t *persist_fw_out;

    rc = ib_persist_fw_cfg_create(mm, &persist_fw_out);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to create new persist_fw_cfg.");
        return rc;
    }

    rc = ib_list_create(&list, local_mm);
    if (rc != IB_OK) {
        return rc;
    }

    /* Copy the src store hash to the dst store hash. */
    if (ib_hash_size(persist_fw_src->stores) > 0) {
        rc = ib_hash_get_all(persist_fw_src->stores, list);
        if (rc != IB_OK) {
            ib_log_error(ib, "Failed to get entries from hash.");
            return rc;
        }
        IB_LIST_LOOP(list, list_node) {
            ib_persist_fw_store_t *store =
                (ib_persist_fw_store_t *)ib_list_node_data_const(list_node);
            rc = ib_hash_set(persist_fw_out->stores, store->name, store);
            if (rc != IB_OK) {
                ib_log_error(ib, "Failed to set store %s", store->name);
                return rc;
            }
        }
        ib_list_clear(list);
    }

    if (ib_hash_size(persist_fw_src->handlers) > 0) {
        /* Copy the src handlers hash to the dst handlers hash. */
        rc = ib_hash_get_all(persist_fw_src->handlers, list);
        if (rc != IB_OK) {
            ib_log_error(ib, "Failed to get entries from hash.");
            return rc;
        }
        IB_LIST_LOOP(list, list_node) {
            ib_persist_fw_handler_t *handler =
                (ib_persist_fw_handler_t *)ib_list_node_data_const(list_node);
            rc = ib_hash_set(persist_fw_out->handlers, handler->type, handler);
            if (rc != IB_OK) {
                ib_log_error(ib, "Failed to set handler %s.", handler->type);
                return rc;
            }
        }
        ib_list_clear(list);
    }

    /* Copy the list of mappings. */
    IB_LIST_LOOP(persist_fw_src->coll_list, list_node) {
        rc = ib_list_push(
            persist_fw_out->coll_list,
            ib_list_node_data(list_node));
        if (rc != IB_OK) {
            ib_log_error(ib, "Failed to append to collection list.");
            return rc;
        }
    }

    *persist_fw_dst = persist_fw_out;
    return IB_OK;
}

/**
 * Copy a @ref ib_persist_fw_cfg_t.
 *
 * Because the persistence framework must be configuration context aware,
 * it registers every instance of itself as a module.
 * That modules knows how to copy its configuration information.
 *
 * @returns
 * - IB_OK On success.
 * - Other on error.
 */
static ib_status_t cpy_persist_fw(
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
    assert(length == sizeof(ib_persist_fw_modlist_t));

    const ib_persist_fw_modlist_t *src_cfg = (ib_persist_fw_modlist_t *)src;
    ib_persist_fw_modlist_t       *dst_cfg = (ib_persist_fw_modlist_t *)dst;
    ib_mm_t                        mm = ib_engine_mm_main_get(ib);
    ib_status_t                    rc;
    ib_mpool_lite_t               *local_mp;
    ib_mm_t                        local_mm;
    size_t                         ne;
    size_t                         idx;
    ib_persist_fw_cfg_t           *persist_fw_src = NULL;

    /* Shallow copy. Now we overwrite bits we need to manually duplicate. */
    memcpy(dst, src, length);

    /* Create a local memory pool, cleared at the end of this function. */
    rc = ib_mpool_lite_create(&local_mp);
    if (rc != IB_OK) {
        return rc;
    }
    local_mm = ib_mm_mpool_lite(local_mp);

    rc = ib_array_create(&(dst_cfg->configs), mm, 1, 2);
    if (rc != IB_OK) {
        return rc;
    }

    IB_ARRAY_LOOP(src_cfg->configs, ne, idx, persist_fw_src) {
        ib_persist_fw_cfg_t *persist_fw_dst = NULL;

        /* Skip unregistered modules. They have a NULL configuration. */
        if (persist_fw_src == NULL) {
            continue;
        }

        rc = cpy_psntsfw_cfg(ib, mm, local_mm, persist_fw_src, &persist_fw_dst);
        if (rc != IB_OK) {
            ib_log_error(ib, "Failed to copy configuration into new context.");
            goto exit;
        }

        rc = ib_array_setn(dst_cfg->configs, idx, persist_fw_dst);
        if (rc != IB_OK) {
            ib_log_error(ib, "Failed to copy configuration into new context.");
            goto exit;
        }
    }

exit:
    ib_mpool_lite_destroy(local_mp);
    return rc;
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
    ib_mm_t       mm = ib_engine_mm_main_get(ib);
    ib_persist_fw_modlist_t *cfg;

    cfg = ib_mm_alloc(mm, sizeof(*cfg));
    if (cfg == NULL) {
        ib_log_error(ib, "Failed to allocate module configuration.");
        return IB_EALLOC;
    }

    rc = ib_array_create(&(cfg->configs), mm, 1, 2);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to create configs hash.");
        return rc;
    }

    /* Set the main context module configuration. */
    rc = ib_module_config_initialize(module, cfg, sizeof(*cfg));
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to set module configuration.");
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
    IB_MODULE_HEADER_DEFAULTS,    /* Header defaults. */
    MODULE_NAME_STR,              /* Module name. */
    NULL,                         /* Configuration. Dynamically set in init. */
    0,                            /* Config length is 0. */
    cpy_persist_fw,                  /* Configuration copy function. */
    NULL,                         /* Callback data. */
    NULL,                         /* Config map. */
    NULL,                         /* Directive map. */
    persistence_framework_init,   /* Initialization. */
    NULL,                         /* Callback data. */
    persistence_framework_fini,   /* Finalization. */
    NULL,                         /* Callback data. */
);
