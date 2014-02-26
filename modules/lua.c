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
 * @brief IronBee --- LUA Module
 *
 * This module provides Lua functionality to IronBee.
 *
 * @author Brian Rectanus <brectanus@qualys.com>
 * @author Sam Baskinger <sbaskinger@qualys.com>
 */

#include "lua_private.h"

#include "lua/ironbee.h"

#include "lua_modules_private.h"
#include "lua_rules_private.h"
#include "lua_runtime_private.h"

#include <ironbee/array.h>
#include <ironbee/cfgmap.h>
#include <ironbee/context.h>
#include <ironbee/engine.h>
#include <ironbee/engine_state.h>
#include <ironbee/escape.h>
#include <ironbee/field.h>
#include <ironbee/hash.h>
#include <ironbee/mm_mpool_lite.h>
#include <ironbee/path.h>
#include <ironbee/queue.h>

#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>

#include <assert.h>
#include <ctype.h>
#include <errno.h>

#if defined(__cplusplus) && !defined(__STDC_FORMAT_MACROS)
/* C99 requires that inttypes.h only exposes PRI* macros
 *  * for C++ implementations if this is defined: */
#define __STDC_FORMAT_MACROS
#endif
#include <inttypes.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

/* -- Module Setup -- */

/* Define the module name as well as a string version of it. */
#define MODULE_NAME lua
#define MODULE_NAME_STR IB_XSTRINGIFY(MODULE_NAME)

/* Define the public module symbol. */
IB_MODULE_DECLARE();

//! Name of an action that does nothing but tag a rule.
static const char *g_modlua_waggle_action_name = "waggle";


ib_status_t modlua_push_config_path(
    ib_engine_t  *ib,
    ib_context_t *ctx,
    lua_State    *L
)
{
    assert(ib  != NULL);
    assert(ctx != NULL);
    assert(L   != NULL);

    /* Index in the Lua Stack where the target table is located. */
    int table_idx;

    lua_createtable(L, 10, 0); /* Make a list to store our context names in. */
    table_idx = lua_gettop(L); /* Store where in the stack it is. */

    /* Push all config contexts onto the Lua stack until and including MAIN. */
    while (ctx != ib_context_main(ib)) {
        lua_pushstring(L, ib_context_name_get(ctx));
        ctx = ib_context_parent_get(ctx);
    }

    /* Push the main context's name. */
    lua_pushstring(L, ib_context_name_get(ctx));

    /* Insert previously pushed contexts into the table at table_idx. */
    for (int i = 1; lua_isstring(L, -1); ++i) {
        lua_pushinteger(L, i);      /* Insert k. */
        lua_insert(L, -2);          /* Make the stack [table, ..., k, v]. */
        lua_settable(L, table_idx); /* Set t[k] = v. */
    }

    return IB_OK;
}


/**
 * Commit any pending configuration items, such as rules.
 *
 * @param[in] ib IronBee engine.
 * @param[in] cfg Module configuration.
 *
 * @returns
 *   - IB_OK
 *   - IB_EOTHER on Rule adding errors. See log file.
 */
static ib_status_t modlua_commit_configuration(
    ib_engine_t *ib,
    modlua_cfg_t *cfg)
{
    assert(ib != NULL);
    assert(cfg != NULL);
    assert(cfg->L != NULL);

    ib_status_t rc;
    int lua_rc;
    lua_State *L = cfg->L;

    lua_getglobal(L, "ibconfig");
    if ( ! lua_istable(L, -1) ) {
        ib_log_error(ib, "ibconfig is not a module table.");
        lua_pop(L, lua_gettop(L));
        return IB_EOTHER;
    }

    lua_getfield(L, -1, "build_rules");
    if ( ! lua_isfunction(L, -1) ) {
        ib_log_error(ib, "ibconfig.include is not a function.");
        lua_pop(L, lua_gettop(L));
        return IB_EOTHER;
    }

    lua_pushlightuserdata(L, ib);
    lua_rc = lua_pcall(L, 1, 1, 0);
    if (lua_rc == LUA_ERRFILE) {
        ib_log_error(ib, "Configuration Error: %s", lua_tostring(L, -1));
        lua_pop(L, lua_gettop(L));
        return IB_EOTHER;
    }
    else if (lua_rc != 0) {
        ib_log_error(ib, "Configuration Error: %s", lua_tostring(L, -1));
        lua_pop(L, lua_gettop(L));
        return IB_EOTHER;
    }
    else if (lua_tonumber(L, -1) != IB_OK) {
        rc = lua_tonumber(L, -1);
        lua_pop(L, lua_gettop(L));
        ib_log_error(
            ib,
            "Configuration error reported: %d:%s",
            rc,
            ib_status_to_string(rc));
        return IB_EOTHER;
    }

    /* Clear stack. */
    lua_pop(L, lua_gettop(L));

    return IB_OK;
}


/* -- Event Handlers -- */

/**
 * Using only the context, fetch the module configuration.
 *
 * @param[in] ib IronBee engine.
 * @param[in] ctx The current configuration context.
 * @param[out] cfg Where to store the configuration. **cfg must be NULL.
 *
 * @returns
 *   - IB_OK on success.
 *   - IB_EEXIST if the module cannot be found in the engine.
 */
ib_status_t modlua_cfg_get(
    ib_engine_t   *ib,
    ib_context_t  *ctx,
    modlua_cfg_t **cfg
)
{
    assert(ib  != NULL);
    assert(ctx != NULL);

    ib_status_t rc;
    ib_module_t *module = NULL;

    rc = ib_engine_module_get(ib, MODULE_NAME_STR, &module);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to find module \"" MODULE_NAME_STR ".\"");
        return rc;
    }

    rc = ib_context_module_config(ctx, module, cfg);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to retrieve modlua configuration.");
        return rc;
    }

    return IB_OK;
}

/**
 * Make an empty reloads list for the configuration for @a ctx.
 *
 * param[in] ib IronBee engine.
 * param[in] ctx The configuration context.
 * param[in] event The type of event. This must always be a
 *           @ref context_close_event.
 * param[in] cbdata Callback data. The @ref ib_module_t of ibmod_lua.
 *
 * @returns
 *   - IB_OK on success.
 *   - Non-IB_OK on an unexpected internal engine failure.
 */
static ib_status_t modlua_context_open(
    ib_engine_t           *ib,
    ib_context_t          *ctx,
    ib_state_event_type_t  event,
    void                  *cbdata
)
{
    assert(ib     != NULL);
    assert(ctx    != NULL);
    assert(event  == context_open_event);
    assert(cbdata != NULL);

    ib_status_t     rc;
    modlua_cfg_t   *cfg    = NULL;
    ib_mm_t         mm     = ib_engine_mm_main_get(ib);
    ib_module_t    *module = (ib_module_t *)cbdata;

    /* In the case where we open the main context, we're done. */
    if (ctx == ib_context_main(ib)) {
        return IB_OK;
    }

    rc = ib_context_module_config(ctx, module, &cfg);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to retrieve modlua configuration.");
        return rc;
    }

    rc = ib_list_create(&(cfg->reloads), mm);
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_list_create(&(cfg->waggle_rules), mm);
    if (rc != IB_OK) {
        return rc;
    }

    return IB_OK;
}

/**
 * Context close callback. Registers outstanding rule configurations
 * if the context being closed in the main context.
 *
 * param[in] ib IronBee engine.
 * param[in] ctx The configuration context.
 * param[in] event The type of event. This must always be a
 *           @ref context_close_event.
 * param[in] cbdata Callback data. The ib_module_t for modlua.
 *
 * @returns
 *   - IB_OK on success.
 *   - Non-IB_OK on an unexpected internal engine failure.
 */
static ib_status_t modlua_context_close(
    ib_engine_t           *ib,
    ib_context_t          *ctx,
    ib_state_event_type_t  event,
    void                  *cbdata
)
{
    assert(ib     != NULL);
    assert(ctx    != NULL);
    assert(event  == context_close_event);
    assert(cbdata != NULL);


    /* Close of the main context signifies configuration finished. */
    if (ib_context_type(ctx) == IB_CTYPE_MAIN) {
        ib_status_t   rc;
        modlua_cfg_t *cfg    = NULL;
        ib_module_t  *module = (ib_module_t *)cbdata;

        rc = ib_context_module_config(ctx, module, &cfg);
        if (rc != IB_OK) {
            ib_log_error(ib, "Failed to retrieve modlua configuration.");
            return rc;
        }

        /* Commit any pending configuration items. */
        rc = modlua_commit_configuration(ib, cfg);
        if (rc != IB_OK) {
            return rc;
        }

        ib_resource_release(cfg->lua_resource);
        cfg->lua_resource = NULL;
        cfg->L = NULL;

        rc = ib_resource_pool_flush(cfg->lua_pool);
        if (rc != IB_OK) {
            return rc;
        }
    }

    return IB_OK;
}

/**
 * Context destroy callback.
 *
 * Destroys Lua stack and pointer when the main context is destroyed.
 *
 * param[in] ib IronBee engine.
 * param[in] ctx The configuration context.
 * param[in] event The type of event. This must always be a
 *           @ref context_close_event.
 * param[in] cbdata Callback data. The ib_module_t of ibmod_lua.
 *
 * @returns
 *   - IB_OK on success.
 *   - Non-IB_OK on an unexpected internal engine failure.
 */
static ib_status_t modlua_context_destroy(
    ib_engine_t           *ib,
    ib_context_t          *ctx,
    ib_state_event_type_t  event,
    void                  *cbdata
)
{
    assert(ib     != NULL);
    assert(ctx    != NULL);
    assert(event  == context_destroy_event);
    assert(cbdata != NULL);

    /* Close of the main context signifies configuration finished. */
    if (ib_context_type(ctx) == IB_CTYPE_MAIN) {
        ib_status_t   rc;
        modlua_cfg_t *cfg = NULL;
        ib_module_t  *module = (ib_module_t *)cbdata;

        rc = ib_context_module_config(ctx, module, &cfg);
        if (rc != IB_OK) {
            ib_log_error(ib, "Failed to retrieve modlua configuration.");
            return rc;
        }

        ib_lock_destroy(&(cfg->lua_pool_lock));
    }

    return IB_OK;
}

/**
 * Report if @c ibmod_lua claims ownership over the rule as a Waggle rule.
 *
 * @param[in] ib IronBee engine.
 * @param[in] rule The rule to consider.
 * @param[in] ctx The context in which the rule is enabled.
 * @param[in] cbdata A pointer to this module's
 *            @ref ib_module_t registered with @a ib.
 *
 * @returns
 * - IB_OK On success.
 * - IB_DECLINED To not take ownership of the rule.
 * - Other on error.
 */
static ib_status_t modlua_ownership_fn(
    const ib_engine_t  *ib,
    const ib_rule_t    *rule,
    const ib_context_t *ctx,
    void               *cbdata
) NONNULL_ATTRIBUTE(1, 2, 3, 4);

static ib_status_t modlua_ownership_fn(
    const ib_engine_t  *ib,
    const ib_rule_t    *rule,
    const ib_context_t *ctx,
    void               *cbdata
)
{
    assert(ib != NULL);
    assert(rule != NULL);
    assert(rule->ctx != NULL);
    assert(cbdata != NULL);

    ib_status_t       rc;
    ib_mpool_lite_t  *tmpmp;
    ib_mm_t           tmpmm;
    ib_list_t        *actions;

    rc = ib_mpool_lite_create(&tmpmp);
    if (rc != IB_OK) {
        return rc;
    }
    tmpmm = ib_mm_mpool_lite(tmpmp);

    rc = ib_list_create(&actions, tmpmm);
    if (rc != IB_OK) {
        goto cleanup;
    }

    rc = ib_rule_search_action(
        ib,
        rule,
        IB_RULE_ACTION_TRUE,
        g_modlua_waggle_action_name,
        actions,
        NULL
    );
    if (rc != IB_OK) {
        ib_log_notice(
            ib,
            "Cannot find action %s.",
            g_modlua_waggle_action_name);
        goto cleanup;
    }

    if (ib_list_elements(actions) > 0) {

        const modlua_cfg_t *cfg    = NULL;
        const ib_module_t  *module = (const ib_module_t *)cbdata;

        /* Fetch the module configuration. */
        rc = ib_context_module_config(ctx, module, &cfg);
        if (rc != IB_OK) {
            ib_log_error(ib, "Cannot retrieve module configuration.");
            rc = IB_OK;
            goto cleanup;
        }

        /* Copy the rule into the waggle list for this context. */
        rc = ib_list_push(cfg->waggle_rules, (void *)rule);
        if (rc != IB_OK) {
            goto cleanup;
        }

        rc = IB_OK;
        goto cleanup;
    }

    rc = IB_DECLINED;
cleanup:
    ib_mpool_lite_destroy(tmpmp);
    return rc;
}

/**
 * Inject a set of Waggle rules for execution.
 *
 * @param[in] ib IronBee engine.
 * @param[in] rule_exec The rule execution environment.
 * @param[out] rule_list The list of rules to append to.
 * @param[in] cbdata A pointer to @a ib 's registered @ref ib_module_t.
 *
 * @returns
 * - IB_OK On success.
 * - Other on error.
 */
static ib_status_t modlua_injection_fn(
    const ib_engine_t    *ib,
    const ib_rule_exec_t *rule_exec,
    ib_list_t            *rule_list,
    void                 *cbdata
) NONNULL_ATTRIBUTE(1, 2, 3, 4);

static ib_status_t modlua_injection_fn(
    const ib_engine_t    *ib,
    const ib_rule_exec_t *rule_exec,
    ib_list_t            *rule_list,
    void                 *cbdata
)
{
    assert(ib != NULL);
    assert(rule_exec != NULL);
    assert(rule_exec->tx != NULL);
    assert(rule_exec->tx->ctx != NULL);
    assert(rule_list != NULL);

    ib_module_t          *module = (ib_module_t *)cbdata;
    ib_context_t         *ctx    = rule_exec->tx->ctx;
    ib_status_t           rc;
    modlua_cfg_t   *cfg = NULL;
    ib_list_node_t *node;

    assert(ib != NULL);
    assert(ctx != NULL);

    rc = ib_context_module_config(ctx, module, &cfg);
    if (rc != IB_OK) {
        ib_log_error(ib, "Cannot retrieve module configuration.");
        return rc;
    }

    /* Copy all Waggle rules that match the current rule phase
     * into the rule_list which the rule_engine will execute for us. */
    IB_LIST_LOOP(cfg->waggle_rules, node) {
        ib_rule_t *rule = (ib_rule_t *)ib_list_node_data(node);
        if (rule_exec->phase == rule->meta.phase) {
            rc = ib_list_push(rule_list, rule);
            if (rc != IB_OK) {
                return rc;
            }
        }
    }

    return IB_OK;
}

/**
 * Initialize the ModLua Module.
 *
 * This will create a common "global" runtime into which various APIs
 * will be loaded.
 */
static ib_status_t modlua_init(
    ib_engine_t *ib,
    ib_module_t *module,
    void        *cbdata
)
{
    assert(ib     != NULL);
    assert(module != NULL);

    ib_mm_t       mm = ib_engine_mm_main_get(ib);
    ib_status_t   rc;
    modlua_cfg_t *cfg = NULL;

    cfg = ib_mm_calloc(mm, 1, sizeof(*cfg));
    if (cfg == NULL) {
        ib_log_error(ib, "Failed to allocate lua module configuration.");
        return IB_EALLOC;
    }

    rc = ib_list_create(&(cfg->reloads), mm);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to allocate reloads list.");
        return rc;
    }

    rc = ib_list_create(&(cfg->waggle_rules), mm);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to allocate waggle rules list.");
        return rc;
    }

    rc = ib_module_config_initialize(module, cfg, sizeof(*cfg));
    if (rc != IB_OK) {
        ib_log_error(ib, "Module already has configuration data?");
        return rc;
    }

    rc = ib_action_register(
        ib,
        g_modlua_waggle_action_name,
        NULL, NULL,
        NULL, NULL,
        NULL, NULL
    );
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to register waggle injection action.");
        return rc;
    }

    rc = ib_lock_init(&(cfg->lua_pool_lock));
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to configure Lua resource pool lock.");
        return rc;
    }

    rc = modlua_runtime_resource_pool_create(&(cfg->lua_pool), ib, module, mm);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to create Lua resource pool.");
        return rc;
    }

    /* Set up defaults */
    rc = ib_resource_acquire(cfg->lua_pool, &(cfg->lua_resource));
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to create Lua stack.");
        return rc;
    }
    cfg->L = ((modlua_runtime_t *)ib_resource_get(cfg->lua_resource))->L;

    /* Hook the context close event.
     * New contexts must copy their parent context's reload list. */
    rc = ib_hook_context_register(
        ib,
        context_open_event,
        modlua_context_open,
        module);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to register context_open_event hook: %s",
                     ib_status_to_string(rc));
        return rc;
    }

    /* Hook the context close event. */
    rc = ib_hook_context_register(
        ib,
        context_close_event,
        modlua_context_close,
        module);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to register context_close_event hook: %s",
                     ib_status_to_string(rc));
        return rc;
    }

    /* Hook the context destroy event to deallocate the Lua stack and lock. */
    rc = ib_hook_context_register(
        ib,
        context_destroy_event,
        modlua_context_destroy,
        module);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to register context_destroy_event hook: %s",
                     ib_status_to_string(rc));
        return rc;
    }

    /* Register an ownership function to take ownership of all waggle rules. */
    rc = ib_rule_register_ownership_fn(
        ib,
        "LuaRuleOwnership",
        modlua_ownership_fn,
        module);
    if (rc != IB_OK) {
        return rc;
    }

    /* Register an injection function for all phases. */
    for (int phase = 0; phase < IB_RULE_PHASE_COUNT; ++phase) {
        rc = ib_rule_register_injection_fn(
            ib,
            "LuaRuleInjection",
            phase,
            modlua_injection_fn,
            module);
        if (rc != IB_OK) {
            return rc;
        }
    }

    /* Set up rule support. */
    rc = rules_lua_init(ib, module);
    if (rc != IB_OK) {
        return rc;
    }

    return IB_OK;
}

static IB_CFGMAP_INIT_STRUCTURE(modlua_config_map) = {
    IB_CFGMAP_INIT_ENTRY(
        MODULE_NAME_STR ".pkg_path",
        IB_FTYPE_NULSTR,
        modlua_cfg_t,
        pkg_path
    ),
    IB_CFGMAP_INIT_ENTRY(
        MODULE_NAME_STR ".pkg_cpath",
        IB_FTYPE_NULSTR,
        modlua_cfg_t,
        pkg_cpath
    ),

    IB_CFGMAP_INIT_LAST
};


/* -- Configuration Directives -- */

/**
 * Implements the LuaInclude directive.
 *
 * Use the common Lua Configuration stack to configure IronBee using Lua.
 *
 * @param[in] cp Configuration parser and state.
 * @param[in] name The directive.
 * @param[in] p1 The file to include.
 * @param[in] cbdata The callback data. NULL. None is needed.
 *
 * @returns
 *   - IB_OK on success.
 *   - IB_EALLOC if an allocation cannot be performed, such as a Lua Stack.
 *   - IB_EOTHER if any other error is encountered.
 *   - IB_EINVAL if there is a Lua interpretation problem. This
 *               will almost always indicate a problem with the user's code
 *               and the user should examine their script.
 */
static ib_status_t modlua_dir_lua_include(
    ib_cfgparser_t *cp,
    const char     *name,
    const char     *p1,
    void           *cbdata
)
{
    assert(cp != NULL);
    assert(cp->ib != NULL);
    assert(name != NULL);
    assert(p1 != NULL);

    ib_status_t    rc;
    int            lua_rc;
    ib_engine_t   *ib      = cp->ib;
    ib_core_cfg_t *corecfg = NULL;
    modlua_cfg_t  *cfg     = NULL;
    lua_State     *L       = NULL;
    ib_context_t  *ctx;

    rc = ib_cfgparser_context_current(cp, &ctx);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp, "Failed to retrieve current context.");
        return rc;
    }

    if (ctx != ib_context_main(ib)) {
        ib_cfg_log_error(
            cp,
            "Directive %s may only be used in the main context.",
            name);
        return IB_EOTHER;
    }

    rc = modlua_cfg_get(ib, ctx, &cfg);
    if (rc != IB_OK) {
        return rc;
    }

    L = cfg->L;

    rc = ib_core_context_config(ib_context_main(ib), &corecfg);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to retrieve core configuration.");
        lua_pop(L, lua_gettop(L));
        return rc;
    }

    /* If the path is relative, get the absolute path, but relative to the
     * current configuration file. */
    p1 = ib_util_relative_file(
        ib_engine_mm_config_get(ib),
        ib_cfgparser_curr_file(cp),
        p1);


    lua_getglobal(L, "ibconfig");
    if ( ! lua_istable(L, -1) ) {
        ib_log_error(ib, "ibconfig is not a module table.");
        lua_pop(L, lua_gettop(L));
        return IB_EOTHER;
    }

    lua_getfield(L, -1, "include");
    if ( ! lua_isfunction(L, -1) ) {
        ib_log_error(ib, "ibconfig.include is not a function.");
        lua_pop(L, lua_gettop(L));
        return IB_EOTHER;
    }

    lua_pushlightuserdata(L, cp);
    lua_pushstring(L, p1);
    lua_rc = lua_pcall(L, 2, 1, 0);
    if (lua_rc == LUA_ERRFILE) {
        ib_log_error(ib, "Failed to access file %s.", p1);
        ib_log_error(ib, "Configuration Error: %s", lua_tostring(L, -1));
        lua_pop(L, lua_gettop(L));
        return IB_EOTHER;
    }
    else if (lua_rc) {
        ib_log_error(ib, "Configuration Error: %s", lua_tostring(L, -1));
        lua_pop(L, lua_gettop(L));
        return IB_EOTHER;
    }
    else if (lua_tonumber(L, -1) != IB_OK) {
        rc = lua_tonumber(L, -1);
        lua_pop(L, lua_gettop(L));
        ib_log_error(
            ib,
            "Configuration error reported: %d:%s",
            rc,
            ib_status_to_string(rc));
        return IB_EOTHER;
    }

    lua_pop(L, lua_gettop(L));

    rc = modlua_commit_configuration(ib, cfg);
    if (rc != IB_OK) {
        ib_log_error(
            ib,
            "Failed to commit LuaInclude configurations from file %s.",
            p1);
        return rc;
    }

    return IB_OK;
}

/**
 * Implement the LuaSet directive.
 *
 * This will set a value in a loaded lua module's context configuration.
 */
static ib_status_t modlua_dir_lua_set(
    ib_cfgparser_t  *cp,
    const char      *name,
    const ib_list_t *params,
    void            *cbdata
)
{
    assert(cp != NULL);
    assert(cp->ib != NULL);
    assert(name != NULL);
    assert(params != NULL);

    ib_status_t           rc;
    int                   lua_rc;
    lua_State            *L;
    ib_engine_t          *ib = cp->ib;
    const ib_list_node_t *node;
    ib_context_t         *ctx;                /* Current context. */
    modlua_cfg_t         *cfg;                /* Ibmod_lua config. Holds L. */
    ib_module_t          *lua_module;         /* Lua module (not ibmod_lua). */
    const char           *lua_module_name;
    const char           *lua_module_setting;
    const char           *lua_module_value;

    if (ib_list_elements(params) != 3) {
        ib_cfg_log_error(
            cp,
            "Expected 3 arguments to directive %s but %d given.",
            name,
            (int)ib_list_elements(params));
        return IB_EINVAL;
    }

    node = ib_list_first_const(params);
    lua_module_name = (const char *)ib_list_node_data_const(node);

    node = ib_list_node_next_const(node);
    lua_module_setting = (const char *)ib_list_node_data_const(node);

    node = ib_list_node_next_const(node);
    lua_module_value = (const char *)ib_list_node_data_const(node);

    rc = ib_cfgparser_context_current(cp, &ctx);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp, "Failed to retrieve current context.");
        return rc;
    }

    rc = ib_engine_module_get(ib, lua_module_name, &lua_module);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp, "Failed to find module \"%s\".", lua_module_name);
        return rc;
    }

    rc = modlua_cfg_get(ib, ctx, &cfg);
    if (rc != IB_OK) {
        return rc;
    }
    L = cfg->L;

    lua_getglobal(L, "modlua");
    if ( ! lua_istable(L, -1) ) {
        ib_log_error(ib, "ibconfig is not a module table.");
        lua_pop(L, lua_gettop(L));
        return IB_EOTHER;
    }

    lua_getfield(L, -1, "set");
    if ( ! lua_isfunction(L, -1) ) {
        ib_log_error(ib, "modlua.set is not a function.");
        lua_pop(L, lua_gettop(L));
        return IB_EOTHER;
    }

    lua_pushlightuserdata(L, cp);
    lua_pushlightuserdata(L, ctx);
    lua_pushlightuserdata(L, lua_module);
    lua_pushstring(L, lua_module_setting);
    lua_pushstring(L, lua_module_value);

    lua_rc = lua_pcall(L, 5, 1, 0);
    if (lua_rc != 0) {
        ib_log_error(ib, "Configuration Error: %s", lua_tostring(L, -1));
        rc = IB_EOTHER;
        goto cleanup;
    }
    else if (lua_tonumber(L, -1) != IB_OK) {
        rc = lua_tonumber(L, -1);
        goto cleanup;
    }

    rc = IB_OK;
cleanup:
    /* Clear the stack. */
    lua_pop(L, lua_gettop(L));
    return rc;
}

/**
 * @param[in] cp Configuration parser.
 * @param[in] name The name of the directive.
 * @param[in] p1 The argument to the directive parameter.
 * @param[in,out] cbdata Unused callback data.
 *
 * @returns
 *   - IB_OK
 *   - IB_EALLOC on memory error.
 *   - Others if engine registration or util functions fail.
 */
static ib_status_t modlua_dir_param1(
    ib_cfgparser_t *cp,
    const char     *name,
    const char     *p1,
    void           *cbdata
)
{
    assert(cp     != NULL);
    assert(cp->ib != NULL);
    assert(p1     != NULL);

    ib_status_t    rc;
    ib_module_t   *module;
    ib_mm_t        mm;
    size_t         p1_unescaped_len;
    char          *p1_unescaped;
    ib_engine_t   *ib      = cp->ib;
    ib_core_cfg_t *corecfg = NULL;
    size_t         p1_len  = strlen(p1);
    ib_context_t  *ctx     = NULL;
    modlua_cfg_t  *cfg     = NULL;

    mm = ib_engine_mm_config_get(ib);

    rc = ib_cfgparser_context_current(cp, &ctx);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp, "Cannot get current configuration context.");
        return rc;
    }

    rc = modlua_cfg_get(ib, ctx, &cfg);
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_core_context_config(ib_context_main(ib), &corecfg);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp, "Failed to retrieve core configuration.");
        return rc;
    }

    rc = ib_engine_module_get(ib, MODULE_NAME_STR, &module);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to find module \"" MODULE_NAME_STR ".\"");
        return rc;
    }

    p1_unescaped = ib_mm_alloc(mm, p1_len+1);
    if ( p1_unescaped == NULL ) {
        return IB_EALLOC;
    }

    rc = ib_util_unescape_string(
        p1_unescaped,
        &p1_unescaped_len,
        p1,
        p1_len);
    if (rc != IB_OK) {
        if (rc == IB_EBADVAL) {
            ib_cfg_log_error(cp, "Value for parameter \"%s\" may not contain NULL bytes: %s", name, p1);
        }
        else {
            ib_cfg_log_error(cp, "Value for parameter \"%s\" could not be unescaped: %s", name, p1);
        }
        free(p1_unescaped);
        return rc;
    }

    assert(p1_unescaped_len <= p1_len);

    /* Null-terminate the result. */
    p1_unescaped[p1_unescaped_len] = '\0';

    if (strcasecmp("LuaLoadModule", name) == 0) {
        const char *mod_name = p1_unescaped;

        /* Absolute path. */
        if (p1_unescaped[0] == '/') {
            rc = modlua_module_load(ib, module, mod_name, p1_unescaped, cfg);
            if (rc != IB_OK) {
                ib_cfg_log_error(
                    cp,
                    "Failed to load Lua module with error %s: %s",
                    ib_status_to_string(rc),
                    p1_unescaped);
                return rc;
            }
        }
        else {
            char *path;
            size_t path_len =
                strlen(corecfg->module_base_path) +
                strlen(p1_unescaped) +
                1;

            path = ib_mm_alloc(ib_engine_mm_config_get(ib), path_len);
            if (path == NULL) {
                return IB_EALLOC;
            }

            snprintf(
                path,
                path_len+1,
                "%s/%s",
                corecfg->module_base_path,
                p1_unescaped);

            /* Try a path relative to the modules directory. */
            rc = modlua_module_load(ib, module, mod_name, path, cfg);
            if (rc == IB_OK) {
                return IB_OK;
            }

            /* If the above fails, try relative to the current config file. */
            path = ib_util_relative_file(
                ib_engine_mm_config_get(ib),
                ib_cfgparser_curr_file(cp),
                p1_unescaped);
            rc = modlua_module_load(ib, module, mod_name, path, cfg);
            if (rc == IB_OK) {
                return IB_OK;
            }

            ib_log_error(
                ib,
                "Failed to load Lua module with error %s: %s",
                ib_status_to_string(rc),
                path);
            return rc;
        }
    }
    else if (strcasecmp("LuaPackagePath", name) == 0) {
        rc = ib_context_set_string(ctx, MODULE_NAME_STR ".pkg_path", p1_unescaped);
        return rc;
    }
    else if (strcasecmp("LuaPackageCPath", name) == 0) {
        rc = ib_context_set_string(ctx, MODULE_NAME_STR ".pkg_cpath", p1_unescaped);
        return rc;
    }
    else {
        ib_log_error(ib, "Unhandled directive: %s %s", name, p1_unescaped);
        return IB_EINVAL;
    }

    return IB_OK;
}

static ib_status_t modlua_dir_commit_rules(
    ib_cfgparser_t *cp,
    const char *name,
    const ib_list_t *list,
    void *cbdata)
{
    assert(cp != NULL);
    assert(cp->ib != NULL);

    ib_cfg_log_warning(
        cp,
        "LuaCommitRules is deprecated and should not be used.");

    return IB_OK;
}

static IB_DIRMAP_INIT_STRUCTURE(modlua_directive_map) = {
    IB_DIRMAP_INIT_PARAM1(
        "LuaLoadModule",
        modlua_dir_param1,
        NULL
    ),
    IB_DIRMAP_INIT_PARAM1(
        "LuaPackagePath",
        modlua_dir_param1,
        NULL
    ),
    IB_DIRMAP_INIT_PARAM1(
        "LuaPackageCPath",
        modlua_dir_param1,
        NULL
    ),
    IB_DIRMAP_INIT_PARAM1(
        "LuaInclude",
        modlua_dir_lua_include,
        NULL
    ),
    IB_DIRMAP_INIT_LIST(
        "LuaCommitRules",
        modlua_dir_commit_rules,
        NULL
    ),
    IB_DIRMAP_INIT_LIST(
        "LuaSet",
        modlua_dir_lua_set,
        NULL
    ),

    /* End */
    IB_DIRMAP_INIT_LAST
};

ib_status_t modlua_cfg_copy(
    ib_engine_t *ib,
    ib_module_t *module,
    void        *dst,
    const void  *src,
    size_t       length,
    void        *cbdata
)
{
    assert(ib != NULL);
    assert(module != NULL);
    assert(dst != NULL);
    assert(src != NULL);

    modlua_cfg_t *dstcfg = (modlua_cfg_t *)dst;
    ib_status_t   rc;

    /* Base copy. */
    memcpy(dst, src, length);

    /* The list has to be different in each context to
     * separately append. */
    rc = ib_list_create(&(dstcfg->waggle_rules), ib_engine_mm_main_get(ib));
    if (rc != IB_OK) {
        return rc;
    }

    return IB_OK;
}

/* -- Module Definition -- */

/**
 * Module structure.
 *
 * This structure defines some metadata, config data and various functions.
 */
IB_MODULE_INIT(
    IB_MODULE_HEADER_DEFAULTS,           /**< Default metadata */
    MODULE_NAME_STR,                     /**< Module name */
    NULL,                                /**< NULL config. Init sets this. */
    0,                                   /**< Zero length config (it's null).*/
    modlua_cfg_copy,                     /**< Copy modlua configs. */
    NULL,                                /**< Callback for modlua_cfg_copy. */
    modlua_config_map,                   /**< Configuration field map */
    modlua_directive_map,                /**< Config directive map */
    modlua_init,                         /**< Initialize function */
    NULL,                                /**< Callback data */
    NULL,                                /**< Finish function */
    NULL,                                /**< Callback data */
);
