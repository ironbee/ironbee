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
 * @brief IronBee --- logMsg module
 *
 * This module defines the logMsg action, useful for development purposes.
 *
 * The logMsg action is used to log a message to the IronBee log.  It supports
 * var expansion.
 *
 * Examples:
 * - <tt>rule x \@eq 1 id:1 "logMsg:x is 1"</tt>
 *
 * @author Nick LeRoy <nleroy@qualys.com>
 */

#include <ironbee/action.h>
#include <ironbee/context.h>
#include <ironbee/engine.h>
#include <ironbee/engine_state.h>
#include <ironbee/module.h>
#include <ironbee/mpool.h>
#include <ironbee/rule_engine.h>
#include <ironbee/string.h>

#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <strings.h>


/* Define the module name as well as a string version of it. */
#define MODULE_NAME        logmsg
#define MODULE_NAME_STR    IB_XSTRINGIFY(MODULE_NAME)

/* Declare the public module symbol. */
IB_MODULE_DECLARE();

/**
 * Create function for the logMsg action.
 *
 * @param[in] ib IronBee engine (unused)
 * @param[in] parameters logMsg parameters
 * @param[in] inst Action instance
 * @param[in] cbdata Callback data (unused)
 *
 * @returns Status code
 */
static ib_status_t logmsg_create(
    ib_engine_t      *ib,
    const char       *parameters,
    ib_action_inst_t *inst,
    void             *cbdata)
{
    ib_var_expand_t *expand;
    ib_mpool_t *mp = ib_engine_pool_main_get(ib);
    ib_status_t rc;

    assert(mp != NULL);

    if (parameters == NULL) {
        return IB_EINVAL;
    }

    rc = ib_var_expand_acquire(
        &expand,
        mp,
        IB_S2SL(parameters),
        ib_engine_var_config_get(ib),
        NULL, NULL
    );
    if (rc != IB_OK) {
        return rc;
    }

    inst->data = expand;
    return IB_OK;
}

/**
 * Execute function for the "logMsg" action
 *
 * @param[in] rule_exec The rule execution object
 * @param[in] data Instance data (var expansion)
 * @param[in] cbdata Callback data (@ref ib_module_t)
 *
 * @returns Status code
 */
static ib_status_t logmsg_execute(
    const ib_rule_exec_t *rule_exec,
    void                 *data,
    void                 *cbdata)
{
    assert(rule_exec != NULL);
    assert(data != NULL);
    assert(cbdata != NULL);

    const ib_var_expand_t *expand = data;
    ib_module_t           *module = cbdata;
    const char            *expanded = NULL;
    size_t                 expanded_length;
    ib_status_t            rc;

    /* Expand the string */
    rc = ib_var_expand_execute(
        expand,
        &expanded, &expanded_length,
        rule_exec->tx->mp,
        rule_exec->tx->var_store
    );
    if (rc != IB_OK) {
        ib_rule_log_error(rule_exec,
                          "%s: Failed to expand string: %s",
                          module->name, ib_status_to_string(rc));
        return rc;
    }

    /* Log the message */
    ib_rule_log_debug(rule_exec, "%s: %.*s",
                      module->name,
                      (int)expanded_length, expanded);

    return IB_OK;
}

/**
 * Initialize the development logmsg module
 *
 * @param[in] ib IronBee object
 * @param[in] module Module object
 * @param[in] cbdata (unused)
 *
 * @returns Status code
 */
static ib_status_t logmsg_init(
    ib_engine_t *ib,
    ib_module_t *module,
    void        *cbdata)
{
    ib_status_t rc;

    ib_log_debug(ib, "Initializing development/logmsg module");

    /* Register the logMsg action */
    rc = ib_action_register(ib,
                            "logMsg",
                            logmsg_create, NULL,
                            NULL, NULL, /* no destroy function */
                            logmsg_execute, NULL);
    if (rc != IB_OK) {
        return rc;
    }

    return IB_OK;
}

/**
 * Module structure.
 *
 * This structure defines some metadata, config data and various functions.
 */
IB_MODULE_INIT(
    IB_MODULE_HEADER_DEFAULTS,                 /* Default metadata */
    MODULE_NAME_STR,                           /* Module name */
    IB_MODULE_CONFIG_NULL,                     /* Global config data */
    NULL,                                      /* Module config map */
    NULL,                                      /* Module directive map */
    logmsg_init,                               /* Initialize function */
    NULL,                                      /* Callback data */
    NULL,                                      /* Finish function */
    NULL,                                      /* Callback data */
);
