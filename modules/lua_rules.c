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

#include "lua_rules_private.h"

#include "lua_runtime_private.h"

#include <ironbee/engine.h>

#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>

#include <assert.h>
#include <inttypes.h>

/**
 * @file
 * @brief IronBee --- Lua Rules
 *
 * IronBee Rules as Lua scripts.
 *
 * @author Sam Baskinger <sbaskinger@qualys.com>
 */

static ib_status_t lua_operator_execute(
    ib_tx_t *tx,
    void *instance_data,
    const ib_field_t *field,
    ib_field_t *capture,
    ib_num_t *result,
    void *cbdata
)
{
    assert(tx != NULL);
    assert(tx->ib != NULL);
    assert(tx->ctx != NULL);
    assert(tx->conn != NULL);
    assert(result != NULL);
    assert(instance_data != NULL);
    assert(cbdata != NULL);

    ib_status_t            rc;
    ib_status_t            rc2;
    ib_module_t           *module;
    int                    result_int;
    ib_engine_t           *ib                  = tx->ib;
    ib_context_t          *ctx                 = tx->ctx;
    modlua_cfg_t          *cfg                 = NULL;
    modlua_runtime_t      *luart               = NULL;
    const char            *func_name           = (const char *)instance_data;
    modlua_rules_cbdata_t *modlua_rules_cbdata =
        (modlua_rules_cbdata_t *)cbdata;

    assert(modlua_rules_cbdata->module != NULL);

    /* Get the lua module configuration for this context. */
    rc = modlua_cfg_get(ib, ctx, &cfg);
    if (rc != IB_OK) {
        return rc;
    }

    /* Get the lua module configuration for this context. */
    rc = modlua_acquirestate(ib, cfg, &luart);
    if (rc != IB_OK) {
        return rc;
    }

    module = modlua_rules_cbdata->module;

    rc = modlua_reload_ctx_except_main(ib, module, ctx, luart->L);
    if (rc != IB_OK) {
        ib_log_error_tx(tx, "Failed to reload Lua stack.");
        goto exit;
    }

    /* Call the rule. */
    rc = ib_lua_func_eval_int(ib, tx, luart->L, func_name, &result_int);
    if (rc != IB_OK) {
        *result = 0;
        goto exit;
    }

    /* Convert the passed in integer type to an ib_num_t. */
    *result = result_int;

exit:
    rc2 = modlua_releasestate(ib, cfg, luart);
    if (rc2 != IB_OK) {
        ib_log_error_tx(tx, "Failed to return Lua stack.");
        if (rc == IB_OK) {
            return rc2;
        }
    }

    return rc;
}

static ib_status_t lua_operator_create(
    ib_context_t  *ctx,
    const char    *parameters,
    void          *instance_data,
    void          *cbdata
)
{
    assert(parameters    != NULL);
    assert(instance_data != NULL);

    *(const char **)instance_data = parameters;
    return IB_OK;
}

/**
 * Called by RuleExt lua:.
 *
 * @param[in] cp       Configuration parser.
 * @param[in] rule     Rule under construction.
 * @param[in] tag      Should be "lua".
 * @param[in] location What comes after "lua:".
 * @param[in] cbdata   Callback data; unused.
 * @return
 * - IB_OK on success.
 * - IB_EINVAL if Lua not available or not called for "lua" tag.
 * - Other error code if loading or registration fails.
 */
static ib_status_t modlua_rule_driver(
    ib_cfgparser_t *cp,
    ib_rule_t *rule,
    const char *tag,
    const char *location,
    void *cbdata
)
{
    assert(cp != NULL);
    assert(cp->ib != NULL);
    assert(tag != NULL);
    assert(location != NULL);

    ib_status_t            rc;
    const char            *slash;
    const char            *name;
    ib_operator_t         *op;
    void                  *instance_data;
    ib_engine_t           *ib                 = cp->ib;
    modlua_cfg_t          *cfg                = NULL;
    ib_context_t          *ctx                = NULL;
    modlua_rules_cbdata_t *modlua_rules_cbdata =
        (modlua_rules_cbdata_t *)cbdata;

    /* This cbdata is passed on to the implementation. Validate it here. */
    assert(modlua_rules_cbdata != NULL);
    assert(modlua_rules_cbdata->module != NULL);

    if (strncmp(tag, "lua", 3) != 0) {
        ib_cfg_log_error(cp, "Lua rule driver called for non-lua tag.");
        return IB_EINVAL;
    }

    rc = ib_cfgparser_context_current(cp, &ctx);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp, "Failed to retrieve current context.");
        return rc;
    }

    rc = modlua_cfg_get(ib, ctx, &cfg);
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_lua_load_func(
        cp->ib,
        cfg->L,
        location,
        ib_rule_id(rule));
    if (rc != IB_OK) {
        ib_cfg_log_error(cp, "Failed to load lua file \"%s\"", location);
        return rc;
    }

    /* Record that we need to reload this rule in each TX. */
    rc = modlua_record_reload(
        cp->ib,
        cfg,
        MODLUA_RELOAD_RULE,
        NULL,
        ib_rule_id(rule),
        location);
    if (rc != IB_OK) {
        ib_cfg_log_error(
            cp,
            "Failed to record lua file \"%s\" to reload",
            location);
        return rc;
    }

    slash = strrchr(location, '/');
    if (slash == NULL) {
        name = location;
    }
    else {
        name = slash + 1;
    }

    rc = ib_operator_create_and_register(
        &op,
        cp->ib,
        name,
        IB_OP_CAPABILITY_NONE,
        &lua_operator_create,
        modlua_rules_cbdata,
        NULL,
        NULL,
        &lua_operator_execute,
        modlua_rules_cbdata
    );
    if (rc != IB_OK) {
        ib_cfg_log_error(cp, "Error registering lua operator \"%s\": %s",
                         name, ib_status_to_string(rc));
        return rc;
    }

    rc = ib_operator_inst_create(op,
                                 ctx,
                                 ib_rule_required_op_flags(rule),
                                 ib_rule_id(rule), /* becomes instance_data */
                                 &instance_data);

    if (rc != IB_OK) {
        ib_cfg_log_error(cp,
                         "Error instantiating lua operator "
                         "for rule \"%s\": %s",
                         name, ib_status_to_string(rc));
        return rc;
    }

    rc = ib_rule_set_operator(cp->ib, rule, op, instance_data);

    if (rc != IB_OK) {
        ib_cfg_log_error(cp,
                         "Error associating lua operator \"%s\" "
                         "with rule \"%s\": %s",
                         name, ib_rule_id(rule), ib_status_to_string(rc));
        return rc;
    }

    return IB_OK;
}

ib_status_t rules_lua_init(ib_engine_t *ib, ib_module_t *module)
{
    assert(ib != NULL);
    assert(module != NULL);

    ib_status_t            rc;
    modlua_rules_cbdata_t *modlua_rules_cbdata = NULL;
    ib_mm_t                mm                  = ib_engine_mm_main_get(ib);

    /* Build an initialize callback struct for Lua Rules. */
    modlua_rules_cbdata = ib_mm_calloc(mm, 1, sizeof(*modlua_rules_cbdata));
    if (modlua_rules_cbdata == NULL) {
        ib_log_error(ib, "Failed to create Lua Rules callback data.");
        return IB_EALLOC;
    }
    modlua_rules_cbdata->module = module;

    /* Register driver. */
    rc = ib_rule_register_external_driver(
        ib,
        "lua",
        modlua_rule_driver,
        modlua_rules_cbdata
    );
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to register lua rule driver.");
        return rc;
    }

    return IB_OK;
}
