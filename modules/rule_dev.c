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
 * @brief IronBee &mdash; Rule development module
 *
 * This is a module that defines some rule operators and actions for
 * development purposes.
 *
 * @note This module is enabled only for builds configured with
 * "--enable-devel".
 *
 * @author Nick LeRoy <nleroy@qualys.com>
 */
#include <stdio.h>
#include <string.h>

#include <ironbee/engine.h>
#include <ironbee/mpool.h>
#include <ironbee/module.h>
#include <ironbee/bytestr.h>
#include <ironbee/debug.h>
#include <ironbee/field.h>
#include <ironbee/string.h>
#include <ironbee/rule_engine.h>
#include <ironbee/action.h>
#include <ironbee/operator.h>

/* Define the module name as well as a string version of it. */
#define MODULE_NAME        rule_dev
#define MODULE_NAME_STR    IB_XSTRINGIFY(MODULE_NAME)

/* Declare the public module symbol. */
IB_MODULE_DECLARE();


/**
 * Execute function for the "true" operator
 * @internal
 *
 * @note This operator is enabled only for builds configured with
 * "--enable-devel".
 *
 * @param[in] ib Ironbee engine (unused)
 * @param[in] tx The transaction for this operator (unused)
 * @param[in] data Operator data (unused)
 * @param[in] flags Operator instance flags
 * @param[in] field Field value (unused)
 * @param[out] result Pointer to number in which to store the result
 *
 *
 *
 * @returns Status code
 */
static ib_status_t op_true_execute(ib_engine_t *ib,
                                   ib_tx_t *tx,
                                   void *data,
                                   ib_flags_t flags,
                                   ib_field_t *field,
                                   ib_num_t *result)
{
    IB_FTRACE_INIT();
    ib_log_debug_tx(tx, "True operator returning 1");
    *result = 1;
    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * Execute function for the "false" operator
 * @internal
 *
 * @note This operator is enabled only for builds configured with
 * "--enable-devel".
 *
 * @param[in] ib Ironbee engine (unused)
 * @param[in] tx The transaction for this operator (unused)
 * @param[in] data Operator data (unused)
 * @param[in] flags Operator instance flags
 * @param[in] field Field value (unused)
 * @param[out] result Pointer to number in which to store the result
 *
 * @returns Status code
 */
static ib_status_t op_false_execute(ib_engine_t *ib,
                                    ib_tx_t *tx,
                                    void *data,
                                    ib_flags_t flags,
                                    ib_field_t *field,
                                    ib_num_t *result)
{
    IB_FTRACE_INIT();
    *result = 0;
    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * Create function for the log action.
 * @internal
 *
 * @param[in] ib IronBee engine (unused)
 * @param[in] ctx Current context.
 * @param[in] mp Memory pool to use for allocation
 * @param[in] parameters Constant parameters from the rule definition
 * @param[in,out] inst Action instance
 *
 * @returns Status code
 */
static ib_status_t act_log_create(ib_engine_t *ib,
                                  ib_context_t *ctx,
                                  ib_mpool_t *mp,
                                  const char *parameters,
                                  ib_action_inst_t *inst)
{
    IB_FTRACE_INIT();
    ib_status_t rc;
    ib_bool_t expand;
    char *str;

    if (parameters == NULL) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    str = ib_mpool_strdup(mp, parameters);
    if (str == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    /* Do we need expansion? */
    rc = ib_data_expand_test_str(str, &expand);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }
    else if (expand == IB_TRUE) {
        inst->flags |= IB_ACTINST_FLAG_EXPAND;
    }

    inst->data = str;
    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * Execute function for the "debuglog" action
 * @internal
 *
 * @param[in] data C-style string to log
 * @param[in] rule The matched rule
 * @param[in] tx IronBee transaction
 * @param[in] flags Action instance flags
 *
 * @returns Status code
 */
static ib_status_t act_debuglog_execute(void *data,
                                        ib_rule_t *rule,
                                        ib_tx_t *tx,
                                        ib_flags_t flags)
{
    IB_FTRACE_INIT();

    /* This works on C-style (NUL terminated) strings */
    const char *cstr = (const char *)data;
    char *expanded = NULL;
    ib_status_t rc;

    /* Expand the string */
    if ((flags & IB_ACTINST_FLAG_EXPAND) != 0) {
        rc = ib_data_expand_str(tx->dpi, cstr, &expanded);
        if (rc != IB_OK) {
            ib_log_error_tx(tx,
                         "log_execute: Failed to expand string '%s': %s",
                         cstr, ib_status_to_string(rc));
        }
    }
    else {
        expanded = (char *)cstr;
    }

    ib_log_debug3_tx(tx, "LOG: %s", expanded);
    IB_FTRACE_RET_STATUS(IB_OK);
}


/**
 * Called to initialize the rule development module
 * @internal
 *
 * Registers rule development operators and actions.
 *
 * @param[in,out] ib IronBee object
 * @param[in] m Module object
 * @param[in] cbdata (unused)
 *
 * @returns Status code
 */
static ib_status_t ruledev_init(ib_engine_t *ib, ib_module_t *m, void *cbdata)
{
    IB_FTRACE_INIT();
    ib_status_t rc;

    /**
     * Simple True / False operators.
     */
    ib_log_debug(ib, "Initializing rule development module");

    /* Register the true operator */
    rc = ib_operator_register(ib,
                              "true",
                              IB_OP_FLAG_ALLOW_NULL|IB_OP_FLAG_PHASE,
                              NULL, /* No create function */
                              NULL, /* no destroy function */
                              op_true_execute);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Register the false operator */
    rc = ib_operator_register(ib,
                              "false",
                              IB_OP_FLAG_ALLOW_NULL|IB_OP_FLAG_PHASE,
                              NULL, /* No create function */
                              NULL, /* no destroy function */
                              op_false_execute);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /**
     * Debug logging actions
     */

    /* Register the debuglog action */
    rc = ib_action_register(ib,
                            "debuglog",
                            IB_ACT_FLAG_NONE,
                            act_log_create,
                            NULL, /* no destroy function */
                            act_debuglog_execute);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* dlog is an alias for debuglog */
    rc = ib_action_register(ib,
                            "dlog",
                            IB_ACT_FLAG_NONE,
                            act_log_create,
                            NULL, /* no destroy function */
                            act_debuglog_execute);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

IB_MODULE_INIT(
    IB_MODULE_HEADER_DEFAULTS,      /* Default metadata */
    MODULE_NAME_STR,                /* Module name */
    IB_MODULE_CONFIG_NULL,          /* Global config data */
    NULL,                           /* Module config map */
    NULL,                           /* Module directive map */
    ruledev_init,                   /* Initialize function */
    NULL,                           /* Callback data */
    NULL,                           /* Finish function */
    NULL,                           /* Callback data */
    NULL,                           /* Context open function */
    NULL,                           /* Callback data */
    NULL,                           /* Context close function */
    NULL,                           /* Callback data */
    NULL,                           /* Context destroy function */
    NULL                            /* Callback data */
);
