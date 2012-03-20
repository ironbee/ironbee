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
 *****************************************************************************/

/**
 * @file
 * @brief IronBee Module Code
 *
 * @author Brian Rectanus <brectanus@qualys.com>
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include "ironbee_config_auto.h"

#include <ironbee/module.h>
#include <ironbee/mpool.h>
#include <ironbee/dso.h>

#include "ironbee_private.h"

/// @todo Probably need to load into a given context???
ib_status_t ib_module_init(ib_module_t *m, ib_engine_t *ib)
{
    IB_FTRACE_INIT();
    ib_status_t rc;

    /* Keep track of the module index. */
    m->idx = ib_array_elements(ib->modules);
    m->ib = ib;

    ib_log_debug(ib, 7, "Initializing module %s (%d): %s",
                 m->name, m->idx, m->filename);

    /* Zero the config structure if there is one. */
    if (m->gclen > 0) {
        memset(m->gcdata, 0, m->gclen);
    }

    /* Register directives */
    if (m->dm_init != NULL) {
        ib_config_register_directives(ib, m->dm_init);
    }

    rc = ib_array_setn(ib->modules, m->idx, m);
    if (rc != IB_OK) {
        ib_log_error(ib, 1, "Failed to register module %s %d", m->name, rc);
        IB_FTRACE_RET_STATUS(rc);
    }

    if (ib->ctx != NULL) {
        ib_log_debug(ib, 7, "Registering module \"%s\" with main context %p",
                     m->name, ib->ctx);
        ib_module_register_context(m, ib->ctx);
    }
    else {
        ib_log_error(ib, 4, "No main context to registering module \"%s\"",
                     m->name);
    }

    /* Init and register the module */
    if (m->fn_init != NULL) {
        rc = m->fn_init(ib, m, m->cbdata_init);
        if (rc != IB_OK) {
            ib_log_error(ib, 1, "Failed to initialize module %s %d",
                         m->name, rc);
            /// @todo Need to be able to delete the entry???
            ib_array_setn(ib->modules, m->idx, NULL);
            IB_FTRACE_RET_STATUS(rc);
        }
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t ib_module_create(ib_module_t **pm,
                             ib_engine_t *ib)
{
    IB_FTRACE_INIT();

    *pm = (ib_module_t *)ib_mpool_calloc(ib->config_mp, 1, sizeof(**pm));
    if (*pm == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t ib_module_load(ib_module_t **pm,
                           ib_engine_t *ib,
                           const char *file)
{
    IB_FTRACE_INIT();
    ib_status_t rc;
    ib_dso_t *dso;
    union {
        void              *sym;
        ib_dso_sym_t      *dso;
        ib_module_sym_fn   fn_sym;
    } sym;

    if (ib == NULL) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    /* Load module and fetch the module symbol. */
    ib_log_debug(ib, 7, "Loading module: %s", file);
    rc = ib_dso_open(&dso, file, ib->config_mp);
    if (rc != IB_OK) {
        ib_log_error(ib, 1, "Failed to load module %s: %d", file, rc);
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = ib_dso_sym_find(dso, IB_MODULE_SYM_NAME, &sym.dso);
    if (rc != IB_OK) {
        ib_log_error(ib, 1, "Failed to load module %s: no symbol named %s",
                     file, IB_MODULE_SYM_NAME);
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Fetch the module structure. */
    *pm = sym.fn_sym(ib);
    if (*pm == NULL) {
        ib_log_error(ib, 1, "Failed to load module %s: no module structure",
                     file);
        IB_FTRACE_RET_STATUS(IB_EUNKNOWN);
    }

    /* Check module for ABI compatibility with this engine */
    if ((*pm)->vernum > IB_VERNUM) {
        ib_log_error(ib, 0,
                     "Module %s (built against engine version %s) is not "
                     "compatible with this engine (version %s): "
                     "ABI %d > %d",
                     file, (*pm)->version, IB_VERSION, (*pm)->abinum, IB_ABINUM);
        IB_FTRACE_RET_STATUS(IB_EINCOMPAT);
    }

    ib_log_debug(ib, 9,
                 "Loaded module %s: "
                 "vernum=%d abinum=%d version=%s index=%d filename=%s",
                 (*pm)->name,
                 (*pm)->vernum, (*pm)->abinum, (*pm)->version,
                 (*pm)->idx, (*pm)->filename);

    rc = ib_module_init(*pm, ib);
    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_module_unload(ib_module_t *m)
{
    IB_FTRACE_INIT();
    ib_engine_t *ib;
    ib_status_t rc;

    if (m == NULL) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    ib = m->ib;

    ib_log_debug(ib, 9,
                 "Unloading module %s: "
                 "vernum=%d abinum=%d version=%s index=%d filename=%s",
                 m->name,
                 m->vernum, m->abinum, m->version,
                 m->idx, m->filename);

    /* Finish the module */
    if (m->fn_fini != NULL) {
        rc = m->fn_fini(ib, m, m->cbdata_fini);
        if (rc != IB_OK) {
            ib_log_error(ib, 1, "Failed to finish module %s %d",
                         m->name, rc);
        }
    }

    /// @todo Implement

    /* Unregister directives */

    IB_FTRACE_RET_STATUS(IB_ENOTIMPL);
}

ib_status_t ib_module_register_context(ib_module_t *m,
                                       ib_context_t *ctx)
{
    IB_FTRACE_INIT();
    ib_context_data_t *cfgdata;
    ib_status_t rc;

    /* Create a module context data structure. */
    cfgdata = (ib_context_data_t *)ib_mpool_calloc(ctx->mp, 1, sizeof(*cfgdata));
    if (cfgdata == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }
    cfgdata->module = m;

    /* Set default values from parent values. */

    /* Add module config entries to config context, first copying the
     * parent/global values, then overriding using default values from
     * the configuration mapping.
     *
     * NOTE: Not all configuration data is required to be in the
     * mapping, which is why the initial memcpy is required.
     */
    if (m->gclen > 0) {
        ib_context_t *p_ctx = ctx->parent;
        ib_context_data_t *p_cfgdata;

        cfgdata->data = ib_mpool_alloc(ctx->mp, m->gclen);
        if (cfgdata->data == NULL) {
            IB_FTRACE_RET_STATUS(IB_EALLOC);
        }

        /* Copy values from parent context if available, otherwise
         * use the module global values as defaults.
         */
        if (p_ctx != NULL) {
            rc = ib_array_get(p_ctx->cfgdata, m->idx, &p_cfgdata);
            if (rc == IB_OK) {
                if (m->fn_cfg_copy) {
                    rc = m->fn_cfg_copy(
                        m->ib, m,
                        cfgdata->data,
                        p_cfgdata->data,
                        m->gclen,
                        m->cbdata_cfg_copy
                    );
                    if (rc != IB_OK) {
                        IB_FTRACE_RET_STATUS(rc);
                    }
                }
                else {
                    memcpy(cfgdata->data, p_cfgdata->data, m->gclen);
                }
                ib_context_init_cfg(ctx, cfgdata->data, m->cm_init);
            }
            else {
                /* No parent context config, so use globals. */
                memcpy(cfgdata->data, m->gcdata, m->gclen);
                ib_context_init_cfg(ctx, cfgdata->data, m->cm_init);
            }
        }
        else {
            if (m->fn_cfg_copy) {
                rc = m->fn_cfg_copy(
                    m->ib, m,
                    cfgdata->data,
                    m->gcdata,
                    m->gclen,
                    m->cbdata_cfg_copy
                );
                if (rc != IB_OK) {
                    IB_FTRACE_RET_STATUS(rc);
                }
            }
            else {
                memcpy(cfgdata->data, m->gcdata, m->gclen);
            }
            ib_context_init_cfg(ctx, cfgdata->data, m->cm_init);
        }
    }

    /* Keep track of module specific context data using the
     * module index as the key so that the location is deterministic.
     */
    rc = ib_array_setn(ctx->cfgdata, m->idx, cfgdata);
    IB_FTRACE_RET_STATUS(rc);
}
