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
 * @brief IronBee --- Module Code
 *
 * @author Brian Rectanus <brectanus@qualys.com>
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include "ironbee_config_auto.h"

#include <ironbee/module.h>

#include "module_private.h"
#include "engine_private.h"

#include <ironbee/context.h>
#include <ironbee/dso.h>
#include <ironbee/mm.h>
#include <ironbee/rule_engine.h>

#include <assert.h>
#include <inttypes.h>
#include <stdlib.h>

/**
 * Context open hook
 *
 * @param[in] ib IronBee engine
 * @param[in] ctx Context
 * @param[in] state State
 * @param[in] cbdata Callback data (module pointer)
 *
 * @returns Status code
 */
static ib_status_t module_context_open(
    ib_engine_t *ib,
    ib_context_t *ctx,
    ib_state_t state,
    void *cbdata
)
{
    assert(ib != NULL);
    assert(ctx != NULL);
    assert(state == context_open_state);
    assert(cbdata != NULL);

    /* We only care about the main context */
    if (ib_context_type(ctx) != IB_CTYPE_MAIN) {
        return IB_OK;
    }

    return IB_OK;
}

ib_status_t ib_module_register(const ib_module_t *mod, ib_engine_t *ib)
{
    assert(ib != NULL);
    assert(mod != NULL);

    /* Validate module */
    if (mod->vernum != IB_VERNUM) {
        ib_log_notice(ib,
            "Module was written for IronBee version %d but this is IronBee"
            " version %d.  Please ask module author to update.",
            mod->vernum, IB_VERNUM
        );
    }
    if (mod->abinum != IB_ABINUM) {
        ib_log_error(ib,
            "Module was written for IronBee ABI %d but this is IronBee"
            " ABI %d.  Cannot load incompatible module.  Ask module author "
            " to update.",
            mod->abinum, IB_ABINUM
        );
        return IB_EINVAL;
    }

    ib_status_t rc;
    ib_module_t *m = ib_mm_memdup(
        ib_engine_mm_main_get(ib), mod, sizeof(*mod));

    if (m == NULL) {
        return IB_EALLOC;
    }

    /* Keep track of the module index. */
    m->idx = ib_array_elements(ib->modules);
    m->ib = ib;

    /* Register our own context open callback */
    rc = ib_hook_context_register(ib, context_open_state,
                                  module_context_open, m);
    if (rc != IB_OK) {
        return rc;
    }

    /* Register directives */
    if (m->dm_init != NULL) {
        ib_config_register_directives(ib, m->dm_init);
    }

    rc = ib_array_setn(ib->modules, m->idx, m);
    if (rc != IB_OK) {
        ib_log_error(ib,
            "Error registering module %s: %s",
            m->name, ib_status_to_string(rc)
        );
        return rc;
    }

    if (ib->ctx != NULL) {
        ib_module_register_context(m, ib->ctx);
    }
    else {
        ib_log_error(ib, "Error registering module \"%s\": No main context",
                     m->name);
        return IB_EINVAL;
    }

    /* Init and register the module */
    if (m->fn_init != NULL) {
        rc = m->fn_init(ib, m, m->cbdata_init);
        if (rc != IB_OK) {
            ib_log_error(ib, "Error initializing module %s: %s",
                         m->name, ib_status_to_string(rc));
            /// @todo Need to be able to delete the entry???
            ib_array_setn(ib->modules, m->idx, NULL);
            return rc;
        }
    }

    return IB_OK;
}

ib_status_t ib_module_create(ib_module_t **pm,
                             ib_engine_t *ib)
{
    *pm = (ib_module_t *)ib_mm_calloc(
        ib_engine_mm_config_get(ib), 1, sizeof(**pm)
    );
    if (*pm == NULL) {
        return IB_EALLOC;
    }

    return IB_OK;
}


/*
 * ib_dso_sym_find will search beyond the specified file if IB_MODULE_SYM is
 * not found in it.  In order to detect this situation, the resulting symbol
 * is compared against the statically linked IB_MODULE_SYM, i.e., the one
 * that core defines.  This extern allows the code to access its address.
 */
extern const ib_module_t *IB_MODULE_SYM(ib_engine_t *);

ib_status_t ib_module_file_to_sym(
    ib_module_sym_fn *psym,
    ib_engine_t      *ib,
    const char       *file
)
{
    assert(psym != NULL);
    assert(file != NULL);

    ib_status_t rc;
    ib_dso_t *dso;
    union {
        void              *sym;
        ib_dso_sym_t      *dso;
        ib_module_sym_fn   fn_sym;
    } sym;

    if (ib == NULL) {
        return IB_EINVAL;
    }

    /* Load module and fetch the module symbol. */

    rc = ib_dso_open(&dso, file, ib_engine_mm_config_get(ib));
    if (rc != IB_OK) {
        ib_log_error(ib,
            "Error loading module %s: %s", file, ib_status_to_string(rc)
        );
        return rc;
    }

    rc = ib_dso_sym_find(&sym.dso, dso, IB_MODULE_SYM_NAME);
    if (rc != IB_OK || &IB_MODULE_SYM == sym.fn_sym) {
        ib_log_error(ib, "Error loading module %s: no symbol named %s",
                     file, IB_MODULE_SYM_NAME);
        return IB_EINVAL;
    }

    *psym = sym.fn_sym;
    return IB_OK;
}

ib_status_t DLL_PUBLIC ib_module_load_from_sym(
    ib_engine_t       *ib,
    ib_module_sym_fn   sym
)
{
    assert(ib != NULL);
    assert(sym != NULL);

    ib_status_t        rc;
    const ib_module_t *m;

    /* Load module and fetch the module symbol. */

    /* Fetch and copy the module structure. */
    m = sym(ib);
    if (m == NULL) {
        ib_log_error(ib, "Error loading module: no module structure");
        return IB_EUNKNOWN;
    }

    /* Check module for ABI compatibility with this engine */
    if (m->vernum > IB_VERNUM) {
        ib_log_alert(ib,
                     "Module (built against engine version %s) is not "
                     "compatible with this engine (version %s): "
                     "ABI %d > %d",
                     m->version, IB_VERSION, m->abinum, IB_ABINUM);
        return IB_EINCOMPAT;
    }

    ib_log_debug3(ib,
                  "Loaded module %s: "
                  "vernum=%d abinum=%d version=%s index=%zd filename=%s",
                  m->name,
                  m->vernum, m->abinum, m->version,
                  m->idx, m->filename);

    rc = ib_module_register(m, ib);
    return rc;
}

ib_status_t ib_module_load(ib_engine_t *ib, const char *file)
{
    assert(ib != NULL);
    assert(file != NULL);

    ib_status_t rc;
    ib_module_sym_fn sym;

    rc = ib_module_file_to_sym(&sym, ib, file);
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_module_load_from_sym(ib, sym);
    if (rc != IB_OK) {
        return rc;
    }

    return IB_OK;
}

void ib_module_unload(ib_module_t *m)
{
    assert(m != NULL);

    ib_status_t rc;

    if (m->fn_fini != NULL) {
        rc = m->fn_fini(m->ib, m, m->cbdata_fini);
        /* If something goes wrong here, we are in trouble.  We can't log it
         * as logging is not supported during module unloading.  We settle
         * for panic. */
        if (rc != IB_OK) {
            fprintf(
                stderr,
                "PANIC! Module %s failed to unload: %s",
                m->name, ib_status_to_string(rc)
            );
            abort();
        }
    }
}

ib_status_t ib_module_register_context(ib_module_t *m,
                                       ib_context_t *ctx)
{
    ib_context_data_t *cfgdata;
    ib_status_t rc;
    ib_context_t *p_ctx = ctx->parent;
    ib_context_data_t *p_cfgdata;

    void *src_data = NULL;
    size_t src_length;

    /* Create a module context data structure. */
    cfgdata =
        (ib_context_data_t *)ib_mm_calloc(ctx->mm, 1, sizeof(*cfgdata));
    if (cfgdata == NULL) {
        return IB_EALLOC;
    }
    cfgdata->module = m;

    if (p_ctx != NULL) {
        rc = ib_array_get(p_ctx->cfgdata, m->idx, &p_cfgdata);
        if (rc == IB_OK) {
            src_data = p_cfgdata->data;
            src_length = p_cfgdata->data_length;
        }
    }
    if (src_data == NULL) {
        src_data = m->gcdata;
        src_length = m->gclen;
    }

    if (src_length > 0) {
        cfgdata->data = ib_mm_calloc(ctx->mm, 1, src_length);
        if (cfgdata->data == NULL) {
            return IB_EALLOC;
        }
        cfgdata->data_length = src_length;
        if (m->fn_cfg_copy) {
            rc = m->fn_cfg_copy(
                m->ib, m,
                cfgdata->data,
                src_data, src_length,
                m->cbdata_cfg_copy
            );
            if (rc != IB_OK) {
                return rc;
            }
        }
        else {
            memcpy(
                cfgdata->data,
                src_data, src_length
            );
        }
        ib_context_init_cfg(ctx, cfgdata->data, m->cm_init);
    }

    /* Keep track of module specific context data using the
     * module index as the key so that the location is deterministic.
     */
    rc = ib_array_setn(ctx->cfgdata, m->idx, cfgdata);
    return rc;
}

ib_status_t ib_module_config_initialize(
    ib_module_t *module,
    void *cfg,
    size_t cfg_length)
{
    assert(module);
    assert(module->ib);

    ib_status_t rc;
    ib_context_t *main_context = ib_context_main(module->ib);
    ib_context_data_t *main_cfgdata = NULL;

    assert(main_context);

    rc = ib_array_get(main_context->cfgdata, module->idx, &main_cfgdata);
    if (rc != IB_OK || main_cfgdata->data != NULL) {
        return IB_EINVAL;
    }
    main_cfgdata->data        = cfg;
    main_cfgdata->data_length = cfg_length;
    module->gcdata            = cfg;
    module->gclen             = cfg_length;

    return IB_OK;
}
