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

#include "mock_module.h"

#include <assert.h>
#include <ironbee/context.h>

#define MODULE_NAME mock_module
#define MODULE_NAME_STR IB_XSTRINGIFY(MODULE_NAME)

IB_MODULE_DECLARE();


static mock_module_conf_t *getconf(ib_cfgparser_t *cp)
{
    assert(cp != NULL);
    assert(cp->ib != NULL);

    ib_status_t rc;
    ib_module_t *module;
    ib_context_t *ctx;
    ib_engine_t *ib = cp->ib;
    mock_module_conf_t *conf;

    rc = ib_engine_module_get(ib, MODULE_NAME_STR, &module);
    if (rc != IB_OK) {
        return NULL;
    }

    rc = ib_cfgparser_context_current(cp, &ctx);
    if (rc != IB_OK) {
        return NULL;
    }

    rc = ib_context_module_config(ctx, module, &conf);
    if (rc != IB_OK) {
        return NULL;
    }

    assert(conf != NULL);

    ib_cfg_log_info(
        cp,
        "Returning "MODULE_NAME_STR" configuration for context %s",
        ib_context_full_get(ctx));

    return conf;
}

static ib_status_t blkend(ib_cfgparser_t *cp, const char *name, void *cbdata)
{
    mock_module_conf_t *conf = getconf(cp);

    ib_cfg_log_info(cp, "/%s", name);

    conf->blkend_called = true;

    return IB_OK;
}

static ib_status_t
onoff(ib_cfgparser_t* cp, const char *name, int onoff, void *cbdata)
{
    mock_module_conf_t *conf = getconf(cp);

    ib_cfg_log_info(cp, "%s: %d", name, onoff);

    conf->onoff_onoff = onoff;
    return IB_OK;
}

static ib_status_t
param1(ib_cfgparser_t* cp, const char *name, const char *p1, void *cbdata)
{
    mock_module_conf_t *conf = getconf(cp);

    ib_cfg_log_info(cp, "%s: %s", name, p1);

    conf->param1_p1 = p1;

    return IB_OK;
}

static ib_status_t
param2(
    ib_cfgparser_t* cp,
    const char *name,
    const char *p1,
    const char *p2,
    void *cbdata
) {
    mock_module_conf_t *conf = getconf(cp);

    ib_cfg_log_info(cp, "%s: %s, %s", name, p1, p2);

    conf->param2_p1 = p1;
    conf->param2_p2 = p2;

    return IB_OK;
}

static ib_status_t
list(
    ib_cfgparser_t* cp,
    const char *name,
    const ib_list_t *params,
    void *cbdata
) {
    mock_module_conf_t *conf = getconf(cp);

    ib_cfg_log_info(cp, "%s: [list...]", name);

    conf->list_params = params;

    return IB_OK;
}

static ib_status_t
opflags(
    ib_cfgparser_t* cp,
    const char *name,
    ib_flags_t val,
    ib_flags_t mask,
    void *cbdata
) {
    mock_module_conf_t *conf = getconf(cp);

    ib_cfg_log_info(cp, "%s: %x&%x", name, val, mask);

    conf->opflags_val = val;
    conf->opflags_mask = mask;

    return IB_OK;
}

static ib_status_t
sblk1(ib_cfgparser_t* cp, const char *name, const char *p1, void *cbdata)
{
    mock_module_conf_t *conf = getconf(cp);

    ib_cfg_log_info(cp, "%s: %s", name, p1);

    conf->sblk1_p1 = p1;

    return IB_OK;
}

static ib_strval_t mock_module_flags[] = {
    { "Flag1", 1 },
    { "Flag2", 2 },
    { "OFF",   0 }
};

IB_DIRMAP_INIT_STRUCTURE(mock_module_directives) = {
    IB_DIRMAP_INIT_ONOFF("OnOff", onoff, NULL),
    IB_DIRMAP_INIT_PARAM1("Param1", param1, NULL),
    IB_DIRMAP_INIT_PARAM2("Param2", param2, NULL),
    IB_DIRMAP_INIT_LIST("List", list, NULL),
    IB_DIRMAP_INIT_OPFLAGS("OpFlags", opflags, NULL, mock_module_flags),
    IB_DIRMAP_INIT_SBLK1("Sblk1", sblk1, blkend, NULL, NULL),
    IB_DIRMAP_INIT_LAST
};

static mock_module_conf_t g_conf = {
    NULL,
    NULL,
    NULL,
    NULL,
    false,
    0,
    NULL,
    0,
    0
};

IB_MODULE_INIT(
    IB_MODULE_HEADER_DEFAULTS,           /* Default metadata */
    MODULE_NAME_STR,                     /* Module name */
    IB_MODULE_CONFIG(&g_conf),           /* Global config data */
    NULL,                                /* Configuration field map */
    mock_module_directives,              /* Config directive map */
    NULL,                                /* Initialize function */
    NULL,                                /* Callback data */
    NULL,                                /* Finish function */
    NULL,                                /* Callback data */
);

const char *mock_module_name() {
    return MODULE_NAME_STR;
}

ib_status_t mock_module_register(ib_engine_t *ib) {
    ib_status_t rc;

    rc = ib_module_init(IB_MODULE_STRUCT_PTR, ib);
    if (rc != IB_OK){
        return rc;
    }

    return IB_OK;
}
