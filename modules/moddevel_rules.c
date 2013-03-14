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
 * @brief IronBee --- Development rules sub-module
 *
 * This is a module that defines some rule operators and actions for
 * development purposes.
 *
 * @note This module is enabled only for builds configured with
 * "--enable-devel".
 *
 * @author Nick LeRoy <nleroy@qualys.com>
 */

#include "moddevel_private.h"

#include <ironbee/action.h>
#include <ironbee/bytestr.h>
#include <ironbee/capture.h>
#include <ironbee/engine.h>
#include <ironbee/field.h>
#include <ironbee/list.h>
#include <ironbee/module.h>
#include <ironbee/mpool.h>
#include <ironbee/operator.h>
#include <ironbee/rule_engine.h>
#include <ironbee/string.h>

#include <assert.h>
#include <stdio.h>
#include <string.h>


/**
 * Rule configuration
 */
struct ib_moddevel_rules_config_t {
    ib_list_t   *injection_list;  /**< List of rules to inject */
};

/**
 * Execute function for the "true" operator
 *
 * @note This operator is enabled only for builds configured with
 * "--enable-devel".
 *
 * @param[in] rule_exec Rule execution object
 * @param[in] data Operator data (unused)
 * @param[in] flags Operator instance flags
 * @param[in] field Field value (unused)
 * @param[out] result Pointer to number in which to store the result
 *
 *
 *
 * @returns Status code
 */
static ib_status_t op_true_execute(const ib_rule_exec_t *rule_exec,
                                   void *data,
                                   ib_flags_t flags,
                                   ib_field_t *field,
                                   ib_num_t *result)
{
    *result = 1;

    if (ib_rule_should_capture(rule_exec, *result)) {
        ib_capture_clear(rule_exec->tx);
        ib_capture_set_item(rule_exec->tx, 0, field);
    }
    return IB_OK;
}

/**
 * Execute function for the "false" operator
 *
 * @note This operator is enabled only for builds configured with
 * "--enable-devel".
 *
 * @param[in] rule_exec Rule execution object
 * @param[in] data Operator data (unused)
 * @param[in] flags Operator instance flags
 * @param[in] field Field value (unused)
 * @param[out] result Pointer to number in which to store the result
 *
 * @returns Status code
 */
static ib_status_t op_false_execute(const ib_rule_exec_t *rule_exec,
                                    void *data,
                                    ib_flags_t flags,
                                    ib_field_t *field,
                                    ib_num_t *result)
{
    *result = 0;
    /* Don't check for capture, because we always return zero */

    return IB_OK;
}

/**
 * Create function for the "assert" operator
 *
 * @param[in] ib The IronBee engine (unused)
 * @param[in] ctx The current IronBee context (unused)
 * @param[in] rule Parent rule to the operator
 * @param[in,out] mp Memory pool to use for allocation
 * @param[in] parameters Constant parameters
 * @param[in,out] op_inst Instance operator
 *
 * @returns Status code
 */
static ib_status_t op_assert_create(ib_engine_t *ib,
                                    ib_context_t *ctx,
                                    const ib_rule_t *rule,
                                    ib_mpool_t *mp,
                                    const char *parameters,
                                    ib_operator_inst_t *op_inst)
{
    ib_status_t rc;
    bool expand;
    char *str;

    if (parameters == NULL) {
        return IB_EINVAL;
    }

    str = ib_mpool_strdup(mp, parameters);
    if (str == NULL) {
        return IB_EALLOC;
    }

    /* Do we need expansion? */
    rc = ib_data_expand_test_str(str, &expand);
    if (rc != IB_OK) {
        return rc;
    }
    else if (expand) {
        op_inst->flags |= IB_ACTINST_FLAG_EXPAND;
    }

    op_inst->data = str;
    return IB_OK;
}

/**
 * Execute function for the "assert" operator
 *
 * @note This operator is enabled only for builds configured with
 * "--enable-devel".
 *
 * @param[in] rule_exec Rule execution object
 * @param[in] data Operator data (unused)
 * @param[in] flags Operator instance flags
 * @param[in] field Field value (unused)
 * @param[out] result Pointer to number in which to store the result
 *
 * @returns Status code
 */
static ib_status_t op_assert_execute(const ib_rule_exec_t *rule_exec,
                                     void *data,
                                     ib_flags_t flags,
                                     ib_field_t *field,
                                     ib_num_t *result)
{
    /* This works on C-style (NUL terminated) strings */
    const char *cstr = (const char *)data;
    char *expanded = NULL;
    ib_status_t rc;

    /* Expand the string */
    if ((flags & IB_ACTINST_FLAG_EXPAND) != 0) {
        rc = ib_data_expand_str(rule_exec->tx->data, cstr, false, &expanded);
        if (rc != IB_OK) {
            ib_rule_log_error(rule_exec,
                              "log_execute: Failed to expand string '%s': %s",
                              cstr, ib_status_to_string(rc));
        }
    }
    else {
        expanded = (char *)cstr;
    }

    ib_rule_log_error(rule_exec, "ASSERT: %s", expanded);
    assert(0 && expanded);
    return IB_OK;
}

/**
 * Execute function for the "exists" operator
 *
 * @param[in] rule_exec Rule execution object
 * @param[in] data Operator data (unused)
 * @param[in] flags Operator instance flags
 * @param[in] field Field value
 * @param[out] result Pointer to number in which to store the result
 *
 * @returns Status code
 */
static ib_status_t op_exists_execute(const ib_rule_exec_t *rule_exec,
                                     void *data,
                                     ib_flags_t flags,
                                     ib_field_t *field,
                                     ib_num_t *result)
{
    /* Return true of field is not NULL */
    *result = (field != NULL);

    if (ib_rule_should_capture(rule_exec, *result)) {
        ib_capture_clear(rule_exec->tx);
        ib_capture_set_item(rule_exec->tx, 0, field);
    }

    return IB_OK;
}

/* IsType operators */
typedef enum
{
    IsTypeStr,
    IsTypeNulStr,
    IsTypeByteStr,
    IsTypeNum,
    IsTypeInt,
    IsTypeFloat,
} istype_t;

/* IsType operator data */
typedef struct
{
    istype_t    istype;
    int         numtypes;
    ib_ftype_t  types[2];
} istype_params_t;

/* IsType operators data */
static istype_params_t istype_params[] = {
    { IsTypeStr,     2, { IB_FTYPE_NULSTR, IB_FTYPE_BYTESTR } },
    { IsTypeNulStr,  1, { IB_FTYPE_NULSTR } },
    { IsTypeByteStr, 1, { IB_FTYPE_BYTESTR } },
    { IsTypeNum,     2, { IB_FTYPE_NUM, IB_FTYPE_FLOAT } },
    { IsTypeInt,     1, { IB_FTYPE_NUM } },
    { IsTypeFloat,   1, { IB_FTYPE_FLOAT } },
};

/**
 * Execute function for the "istype" operator family
 *
 * @note This operator is enabled only for builds configured with
 * "--enable-devel".
 *
 * @param[in] rule_exec Rule execution object
 * @param[in] data Operator data (unused)
 * @param[in] flags Operator instance flags
 * @param[in] field Field value (unused)
 * @param[out] result Pointer to number in which to store the result
 *
 * @returns Status code
 */
static ib_status_t op_istype_execute(const ib_rule_exec_t *rule_exec,
                                     void *data,
                                     ib_flags_t flags,
                                     ib_field_t *field,
                                     ib_num_t *result)
{
    assert(field != NULL);

    /* Ignore data */
    const istype_params_t *params =
        (istype_params_t *)rule_exec->rule->opinst->op->cd_execute;
    int n;

    assert(params != NULL);

    *result = false;
    for (n = 0; n < params->numtypes; ++n) {
        if (params->types[n] == field->type) {
            *result = true;
        }
    }
    return IB_OK;
}

/**
 * Create function for the log action.
 *
 * @param[in] ib IronBee engine (unused)
 * @param[in] ctx Current context.
 * @param[in] mp Memory pool to use for allocation
 * @param[in] parameters Constant parameters from the rule definition
 * @param[in,out] inst Action instance
 * @param[in] cbdata Callback data (unused)
 *
 * @returns Status code
 */
static ib_status_t act_log_create(ib_engine_t *ib,
                                  ib_context_t *ctx,
                                  ib_mpool_t *mp,
                                  const char *parameters,
                                  ib_action_inst_t *inst,
                                  void *cbdata)
{
    ib_status_t rc;
    bool expand;
    char *str;

    if (parameters == NULL) {
        return IB_EINVAL;
    }

    str = ib_mpool_strdup(mp, parameters);
    if (str == NULL) {
        return IB_EALLOC;
    }

    /* Do we need expansion? */
    rc = ib_data_expand_test_str(str, &expand);
    if (rc != IB_OK) {
        return rc;
    }
    else if (expand) {
        inst->flags |= IB_ACTINST_FLAG_EXPAND;
    }

    inst->data = str;
    return IB_OK;
}

/**
 * Execute function for the "debuglog" action
 *
 * @param[in] rule_exec The rule execution object
 * @param[in] data C-style string to log
 * @param[in] flags Action instance flags
 * @param[in] cbdata Callback data (unused)
 *
 * @returns Status code
 */
static ib_status_t act_debuglog_execute(const ib_rule_exec_t *rule_exec,
                                        void *data,
                                        ib_flags_t flags,
                                        void *cbdata)
{
    /* This works on C-style (NUL terminated) strings */
    const char *cstr = (const char *)data;
    char *expanded = NULL;
    ib_status_t rc;

    /* Expand the string */
    if ((flags & IB_ACTINST_FLAG_EXPAND) != 0) {
        rc = ib_data_expand_str(rule_exec->tx->data, cstr, false, &expanded);
        if (rc != IB_OK) {
            ib_rule_log_error(rule_exec,
                              "log_execute: Failed to expand string '%s': %s",
                              cstr, ib_status_to_string(rc));
        }
    }
    else {
        expanded = (char *)cstr;
    }

    ib_rule_log_trace(rule_exec, "LOG: %s", expanded);
    return IB_OK;
}

/**
 * Create function for the assert action.
 *
 * @param[in] ib IronBee engine (unused)
 * @param[in] ctx Current context.
 * @param[in] mp Memory pool to use for allocation
 * @param[in] parameters Constant parameters from the rule definition
 * @param[in,out] inst Action instance
 * @param[in] cbdata Callback data (unused)
 *
 * @returns Status code
 */
static ib_status_t act_assert_create(ib_engine_t *ib,
                                     ib_context_t *ctx,
                                     ib_mpool_t *mp,
                                     const char *parameters,
                                     ib_action_inst_t *inst,
                                     void *cbdata)
{
    ib_status_t rc;
    bool expand;
    char *str;

    if (parameters == NULL) {
        parameters = "";
    }

    str = ib_mpool_strdup(mp, parameters);
    if (str == NULL) {
        return IB_EALLOC;
    }

    /* Do we need expansion? */
    rc = ib_data_expand_test_str(str, &expand);
    if (rc != IB_OK) {
        return rc;
    }
    else if (expand) {
        inst->flags |= IB_ACTINST_FLAG_EXPAND;
    }

    inst->data = str;
    return IB_OK;
}

/**
 * Execute function for the "assert" action
 *
 * @param[in] rule_exec The rule execution object
 * @param[in] data C-style string to log
 * @param[in] flags Action instance flags
 * @param[in] cbdata Callback data (unused)
 *
 * @returns Status code
 */
static ib_status_t act_assert_execute(const ib_rule_exec_t *rule_exec,
                                      void *data,
                                      ib_flags_t flags,
                                      void *cbdata)
{
    /* This works on C-style (NUL terminated) strings */
    const char *cstr = (const char *)data;
    char *expanded = NULL;
    ib_status_t rc;

    /* Expand the string */
    if ((flags & IB_ACTINST_FLAG_EXPAND) != 0) {
        rc = ib_data_expand_str(rule_exec->tx->data, cstr, false, &expanded);
        if (rc != IB_OK) {
            ib_rule_log_error(rule_exec,
                              "log_execute: Failed to expand string '%s': %s",
                              cstr, ib_status_to_string(rc));
        }
    }
    else {
        expanded = (char *)cstr;
    }

    ib_rule_log_fatal(rule_exec, "ASSERT \"%s\"", expanded);
    return IB_OK;
}

/** Injection action functions & related declarations */
static const char *act_inject_name = "inject";

/**
 * Create function for the inject action.
 *
 * @param[in] ib IronBee engine (unused)
 * @param[in] ctx Current context.
 * @param[in] mp Memory pool to use for allocation
 * @param[in] parameters Constant parameters from the rule definition
 * @param[in,out] inst Action instance
 * @param[in] cbdata Callback data (configuration; unused)
 *
 * @returns Status code
 */
static ib_status_t act_inject_create_fn(ib_engine_t *ib,
                                        ib_context_t *ctx,
                                        ib_mpool_t *mp,
                                        const char *parameters,
                                        ib_action_inst_t *inst,
                                        void *cbdata)
{
    inst->data = NULL;
    return IB_OK;
}

/**
 * Inject action rule ownership function
 *
 * @param[in] ib IronBee engine
 * @param[in] rule Rule being registered
 * @param[in] cbdata Registration function callback data (configuration struct)
 *
 * @returns Status code:
 *   - IB_OK All OK, rule managed externally by module
 *   - IB_DECLINE Decline to manage rule
 *   - IB_Exx Other error
 */
static ib_status_t act_inject_ownership_fn(
    const ib_engine_t    *ib,
    const ib_rule_t      *rule,
    void                 *cbdata)
{
    assert(ib != NULL);
    assert(rule != NULL);
    assert(cbdata != NULL);

    ib_status_t rc;
    size_t count = 0;
    ib_moddevel_rules_config_t *config = (ib_moddevel_rules_config_t *)cbdata;

    rc = ib_rule_search_action(ib, rule, RULE_ACTION_TRUE,
                               act_inject_name,
                               NULL, &count);
    if (rc != IB_OK) {
        return rc;
    }
    if (count == 0) {
        return IB_DECLINED;
    }
    rc = ib_list_push(config->injection_list, (ib_rule_t *)rule);
    if (rc != IB_OK) {
        return rc;
    }
    return IB_OK;
}

/**
 * Inject action rule injection function
 *
 * @param[in] ib IronBee engine
 * @param[in] rule_exec Rule execution environment
 * @param[in,out] rule_list List of rules to execute (append-only)
 * @param[in] cbdata Injection function callback data (configuration struct)
 *
 * @returns Status code:
 *   - IB_OK All OK
 *   - IB_Exx Other error
 */
static ib_status_t act_inject_injection_fn(
    const ib_engine_t    *ib,
    const ib_rule_exec_t *rule_exec,
    ib_list_t            *rule_list,
    void                 *cbdata)
{
    assert(ib != NULL);
    assert(rule_exec != NULL);
    assert(rule_list != NULL);
    assert(cbdata != NULL);

    const ib_list_node_t *node;
    const ib_moddevel_rules_config_t *config =
        (const ib_moddevel_rules_config_t *)cbdata;

    IB_LIST_LOOP_CONST(config->injection_list, node) {
        const ib_rule_t *rule = (const ib_rule_t *)node->data;
        if (rule->meta.phase == rule_exec->phase) {
            ib_status_t rc = ib_list_push(rule_list, (ib_rule_t *)rule);
            if (rc != IB_OK) {
                return rc;
            }
        }
    }
    return IB_OK;
}

ib_status_t ib_moddevel_rules_init(
    ib_engine_t                 *ib,
    ib_module_t                 *mod,
    ib_mpool_t                  *mp,
    ib_moddevel_rules_config_t **pconfig)
{
    ib_status_t rc;
    ib_rule_phase_num_t       phase;
    ib_moddevel_rules_config_t *config;
    ib_log_debug(ib, "Initializing rule development module");

    /* Allocate a configuration object */
    config = ib_mpool_calloc(mp, sizeof(*config), 1);
    if (config == NULL) {
        return IB_EALLOC;
    }
    rc = ib_list_create(&(config->injection_list), mp);
    if (rc != IB_OK) {
        return rc;
    }

    /**
     * Simple True / False operators.
     */

    /* Register the true operator */
    rc = ib_operator_register(ib,
                              "true",
                              ( IB_OP_FLAG_ALLOW_NULL |
                                IB_OP_FLAG_PHASE |
                                IB_OP_FLAG_STREAM |
                                IB_OP_FLAG_CAPTURE ),
                              NULL, NULL, /* No create function */
                              NULL, NULL, /* no destroy function */
                              op_true_execute, NULL);
    if (rc != IB_OK) {
        return rc;
    }

    /* Register the false operator */
    rc = ib_operator_register(ib,
                              "false",
                              ( IB_OP_FLAG_ALLOW_NULL |
                                IB_OP_FLAG_PHASE |
                                IB_OP_FLAG_STREAM ),
                              NULL, NULL, /* No create function */
                              NULL, NULL, /* no destroy function */
                              op_false_execute, NULL);
    if (rc != IB_OK) {
        return rc;
    }

    /* Register the field exists operator */
    rc = ib_operator_register(ib,
                              "exists",
                              ( IB_OP_FLAG_ALLOW_NULL |
                                IB_OP_FLAG_PHASE |
                                IB_OP_FLAG_CAPTURE ),
                              NULL, /* No create function */
                              NULL,
                              NULL, /* no destroy function */
                              NULL,
                              op_exists_execute,
                              NULL);
    if (rc != IB_OK) {
        return rc;
    }

    /* Register the false operator */
    rc = ib_operator_register(ib,
                              "assert",
                              ( IB_OP_FLAG_ALLOW_NULL |
                                IB_OP_FLAG_PHASE |
                                IB_OP_FLAG_STREAM ),
                              op_assert_create, NULL,
                              NULL, NULL, /* no destroy function */
                              op_assert_execute, NULL);
    if (rc != IB_OK) {
        return rc;
    }

    /**
     * IsType operators
     */

    /* Register the IsStr operator */
    rc = ib_operator_register(ib,
                              "IsStr",
                              ( IB_OP_FLAG_PHASE |
                                IB_OP_FLAG_STREAM ),
                              NULL, NULL, /* no create function */
                              NULL, NULL, /* no destroy function */
                              op_istype_execute, &istype_params[IsTypeStr]);
    if (rc != IB_OK) {
        return rc;
    }

    /* Register the IsNulStr operator */
    rc = ib_operator_register(ib,
                              "IsNulStr",
                              ( IB_OP_FLAG_PHASE |
                                IB_OP_FLAG_STREAM ),
                              NULL, NULL, /* no create function */
                              NULL, NULL, /* no destroy function */
                              op_istype_execute, &istype_params[IsTypeNulStr]);
    if (rc != IB_OK) {
        return rc;
    }

    /* Register the IsByteStr operator */
    rc = ib_operator_register(ib,
                              "IsByteStr",
                              ( IB_OP_FLAG_PHASE |
                                IB_OP_FLAG_STREAM ),
                              NULL, NULL, /* no create function */
                              NULL, NULL, /* no destroy function */
                              op_istype_execute, &istype_params[IsTypeByteStr]);
    if (rc != IB_OK) {
        return rc;
    }

    /* Register the IsNum operator */
    rc = ib_operator_register(ib,
                              "IsNum",
                              ( IB_OP_FLAG_PHASE |
                                IB_OP_FLAG_STREAM ),
                              NULL, NULL, /* no create function */
                              NULL, NULL, /* no destroy function */
                              op_istype_execute, &istype_params[IsTypeNum]);
    if (rc != IB_OK) {
        return rc;
    }

    /* Register the IsInt operator */
    rc = ib_operator_register(ib,
                              "IsInt",
                              ( IB_OP_FLAG_PHASE |
                                IB_OP_FLAG_STREAM ),
                              NULL, NULL, /* no create function */
                              NULL, NULL, /* no destroy function */
                              op_istype_execute, &istype_params[IsTypeInt]);
    if (rc != IB_OK) {
        return rc;
    }

    /* Register the IsFloat operator */
    rc = ib_operator_register(ib,
                              "IsFloat",
                              ( IB_OP_FLAG_PHASE |
                                IB_OP_FLAG_STREAM ),
                              NULL, NULL, /* no create function */
                              NULL, NULL, /* no destroy function */
                              op_istype_execute, &istype_params[IsTypeFloat]);
    if (rc != IB_OK) {
        return rc;
    }

    /**
     * Debug logging actions
     */

    /* Register the debuglog action */
    rc = ib_action_register(ib,
                            "debuglog",
                            IB_ACT_FLAG_NONE,
                            act_log_create, NULL,
                            NULL, NULL, /* no destroy function */
                            act_debuglog_execute, NULL);
    if (rc != IB_OK) {
        return rc;
    }

    /* dlog is an alias for debuglog */
    rc = ib_action_register(ib,
                            "dlog",
                            IB_ACT_FLAG_NONE,
                            act_log_create, NULL,
                            NULL, NULL, /* no destroy function */
                            act_debuglog_execute, NULL);
    if (rc != IB_OK) {
        return rc;
    }

    /* Register the assert action */
    rc = ib_action_register(ib,
                            "assert",
                            IB_ACT_FLAG_NONE,
                            act_assert_create, NULL,
                            NULL, NULL, /* no destroy function */
                            act_assert_execute, NULL);
    if (rc != IB_OK) {
        return rc;
    }

    /* Register the inject action and related rule engine callbacks */
    rc = ib_action_register(ib,
                            act_inject_name,
                            IB_ACT_FLAG_NONE,
                            act_inject_create_fn, config,
                            NULL, NULL, /* no destroy function */
                            NULL, NULL); /* no execute function */
    if (rc != IB_OK) {
        return rc;
    }

    /* Register the ownership function */
    rc = ib_rule_register_ownership_fn(ib, act_inject_name,
                                       act_inject_ownership_fn, config);
    if (rc != IB_OK) {
        return rc;
    }

    /* Register the injection function */
    for (phase = PHASE_NONE; phase < IB_RULE_PHASE_COUNT; ++phase) {
        rc = ib_rule_register_injection_fn(ib, act_inject_name,
                                           phase,
                                           act_inject_injection_fn, config);
        if (rc != IB_OK) {
            return rc;
        }
    }

    return IB_OK;
}

ib_status_t ib_moddevel_rules_fini(
    ib_engine_t                *ib,
    ib_module_t                *mod,
    ib_moddevel_rules_config_t *config)
{
    return IB_OK;
}
