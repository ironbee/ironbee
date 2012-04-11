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
 * @brief IronBee - core actions
 *
 * @author Craig Forbes <cforbes@qualys.com>
 */

#include "ironbee_config_auto.h"

#include <stdlib.h>
#include <strings.h>
#include <assert.h>

#include <ironbee/debug.h>
#include <ironbee/types.h>
#include <ironbee/string.h>
#include <ironbee/field.h>
#include <ironbee/bytestr.h>
#include <ironbee/mpool.h>
#include <ironbee/action.h>
#include <ironbee/rule_engine.h>
#include <ironbee_private.h>


/**
 * Setvar action data.
 */
typedef enum {
    SETVAR_STRSET,               /**< Set to a constant string */
    SETVAR_NUMSET,               /**< Set to a constant number */
    SETVAR_NUMADD,               /**< Add to a value (counter) */
} setvar_op_t;

typedef union {
    ib_num_t         num;        /**< Numeric value */
    ib_bytestr_t    *bstr;       /**< String value */
} setvar_value_t;

typedef struct {
    setvar_op_t      op;         /**< Setvar operation */
    char            *name;       /**< Field name */
    ib_ftype_t       type;       /**< Data type */
    setvar_value_t   value;      /**< Value */
} setvar_data_t;

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
            ib_log_error(tx->ib, 4,
                         "log_execute: Failed to expand string '%s': %s",
                         cstr, ib_status_to_string(rc));
        }
    }
    else {
        expanded = (char *)cstr;
    }

    ib_log_debug(tx->ib, 9, "LOG: %s", expanded);
    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * Create function for the setflags action.
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
static ib_status_t act_setflags_create(ib_engine_t *ib,
                                       ib_context_t *ctx,
                                       ib_mpool_t *mp,
                                       const char *parameters,
                                       ib_action_inst_t *inst)
{
    IB_FTRACE_INIT();
    char *str;

    if (parameters == NULL) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    str = ib_mpool_strdup(mp, parameters);
    if (str == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    inst->data = str;
    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * Execute function for the "set flag" action
 * @internal
 *
 * @param[in] data Name of the flag to set
 * @param[in] rule The matched rule
 * @param[in] tx IronBee transaction
 * @param[in] flags Action instance flags
 *
 * @returns Status code
 */
static ib_status_t act_setflag_execute(void *data,
                                       ib_rule_t *rule,
                                       ib_tx_t *tx,
                                       ib_flags_t flags)
{
    IB_FTRACE_INIT();

    /* Data will be a C-Style string */
    const char *cstr = (const char *)data;

    /* Handle the suspicious flag */
    if (strcasecmp(cstr, "suspicious") == 0) {
        ib_tx_flags_set(tx, IB_TX_FSUSPICIOUS);
    }
    else {
        ib_log_error(tx->ib, 4, "Set flag action: invalid flag '%s'", cstr);
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * Event action execution callback.
 * @internal
 *
 * Create and event and log it.
 *
 * @param[in] data Instance data needed for execution.
 * @param[in] rule The rule executing this action.
 * @param[in] tx The transaction for this action.
 * @param[in] flags Action instance flags
 *
 * @returns IB_OK if successful.
 */
static ib_status_t act_event_execute(void *data,
                                     ib_rule_t *rule,
                                     ib_tx_t *tx,
                                     ib_flags_t flags)
{
    IB_FTRACE_INIT();
    ib_status_t  rc;
    ib_logevent_t *event;
    const char *expanded;

    ib_log_debug(tx->ib, 4, "Creating event via action");

    /* Expand the message string */
    if ( (rule->meta.flags & IB_RULEMD_FLAG_EXPAND_MSG) != 0) {
        char *tmp;
        rc = ib_data_expand_str(tx->dpi, rule->meta.msg, &tmp);
        if (rc != IB_OK) {
            ib_log_error(tx->ib, 4,
                         "event: Failed to expand string '%s': %s",
                         rule->meta.msg, ib_status_to_string(rc));
            IB_FTRACE_RET_STATUS(rc);
        }
        expanded = tmp;
    }
    else if (rule->meta.msg != NULL) {
        expanded = rule->meta.msg;
    }
    else {
        expanded = "";
    }

    /* Create the event */
    rc = ib_logevent_create(
        &event,
        tx->mp,
        ib_rule_id(rule),
        IB_LEVENT_TYPE_OBSERVATION,
        IB_LEVENT_ACTION_UNKNOWN,
        IB_LEVENT_ACTION_UNKNOWN,
        rule->meta.confidence,
        rule->meta.severity,
        expanded
    );
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Set the data */
    if (rule->meta.data != NULL) {
        if ( (rule->meta.flags & IB_RULEMD_FLAG_EXPAND_DATA) != 0) {
            char *tmp;
            rc = ib_data_expand_str(tx->dpi, rule->meta.data, &tmp);
            if (rc != IB_OK) {
                ib_log_error(tx->ib, 4,
                             "event: Failed to expand data '%s': %s",
                             rule->meta.data, ib_status_to_string(rc));
                IB_FTRACE_RET_STATUS(rc);
            }
            expanded = tmp;
        }
        else {
            expanded = rule->meta.data;
        }
        rc = ib_logevent_data_set(event, expanded, strlen(expanded));
        if (rc != IB_OK) {
            ib_log_error(tx->ib, 4, "event: Failed to set data: %s",
                         ib_status_to_string(rc));
            IB_FTRACE_RET_STATUS(rc);
        }
    }

    /* Link to rule tags. */
    /// @todo Probably need to copy here
    event->tags = rule->meta.tags;

    /* Log the event. */
    rc = ib_event_add(tx->epi, event);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * Create function for the setvar action.
 * @internal
 *
 * @param[in] ib IronBee engine (unused)
 * @param[in] ctx Current context.
 * @param[in] mp Memory pool to use for allocation
 * @param[in] params Constant parameters from the rule definition
 * @param[in,out] inst Action instance
 *
 * @returns Status code
 */
static ib_status_t act_setvar_create(ib_engine_t *ib,
                                     ib_context_t *ctx,
                                     ib_mpool_t *mp,
                                     const char *params,
                                     ib_action_inst_t *inst)
{
    IB_FTRACE_INIT();
    size_t nlen;                 /* Name length */
    const char *eq;              /* '=' character in @a params */
    const char *value;           /* Value in params */
    size_t vlen;                 /* Length of value */
    setvar_data_t *data;         /* Data for the execute function */
    ib_status_t rc;              /* Status code */

    if (params == NULL) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    /* Simple checks; params should look like '<name>=<value>' */
    eq = strchr(params, '=');
    if ( (eq == NULL) || (eq == params) || (*(eq+1) == '\0') ) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    /* Calculate name length */
    nlen = (eq - params);

    /* Determine the type of the value */
    value = (eq + 1);
    vlen = strlen(value);

    /* Create the data structure for the execute function */
    data = ib_mpool_alloc(mp, sizeof(*data) );
    if (data == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    /* Copy the name */
    data->name = ib_mpool_calloc(mp, nlen+1, 1);
    if (data->name == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }
    memcpy(data->name, params, nlen);

    /* Create the value */
    rc = ib_string_to_num_ex(value, vlen, 0, &(data->value.num));
    if (rc == IB_OK) {
        data->type = IB_FTYPE_NUM;
        if ( (*value == '+') || (*value == '-') ) {
            data->op = SETVAR_NUMADD;
        }
        else {
            data->op = SETVAR_NUMSET;
        }
    }
    else {
        ib_bool_t expand = IB_FALSE;

        rc = ib_data_expand_test_str_ex(value, vlen, &expand);
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }
        else if (expand == IB_TRUE) {
            inst->flags |= IB_ACTINST_FLAG_EXPAND;
        }

        rc = ib_bytestr_dup_nulstr(&(data->value.bstr), mp, value);
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }
        data->type = IB_FTYPE_BYTESTR;
        data->op = SETVAR_STRSET;
    }

    inst->data = data;
    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * Execute function for the "set variable" action
 * @internal
 *
 * @param[in] data Name of the flag to set
 * @param[in] rule The matched rule
 * @param[in] tx IronBee transaction
 * @param[in] flags Action instance flags
 *
 * @returns Status code
 */
static ib_status_t act_setvar_execute(void *cbdata,
                                      ib_rule_t *rule,
                                      ib_tx_t *tx,
                                      ib_flags_t flags)
{
    IB_FTRACE_INIT();
    ib_field_t *cur = NULL;
    ib_field_t *new;
    char *expanded = NULL;
    size_t exlen;
    ib_status_t rc;
    const char *bsdata = NULL;
    size_t bslen = 0;

    /* Data should be a setvar_data_t created in our create function */
    const setvar_data_t *svdata = (const setvar_data_t *)cbdata;

    /* Pull the data out of the bytestr */
    if (svdata->type == IB_FTYPE_BYTESTR) {
        bsdata = (const char *)ib_bytestr_ptr(svdata->value.bstr);
        bslen = ib_bytestr_length(svdata->value.bstr);
    }

    /* Get the current value */
    ib_data_get(tx->dpi, svdata->name, &cur);

    /* Expand the string */
    if ( (flags & IB_ACTINST_FLAG_EXPAND) != 0) {
        assert(svdata->type == IB_FTYPE_BYTESTR);

        rc = ib_data_expand_str_ex(
            tx->dpi, bsdata, bslen, IB_FALSE, &expanded, &exlen);
        if (rc != IB_OK) {
            ib_log_error(tx->ib, 4,
                         "setvar: Failed to expand string '%.*s': %s",
                         (int) bslen, bsdata, ib_status_to_string(rc));
            IB_FTRACE_RET_STATUS(rc);
        }
    }
    else if (svdata->type == IB_FTYPE_BYTESTR) {
        expanded = ib_mpool_memdup(tx->mp, bsdata, bslen);
        if (expanded == NULL) {
            ib_log_error(tx->ib, 4,
                         "setvar: Failed to copy string '%.*s'",
                         (int)bslen, bsdata);
            IB_FTRACE_RET_STATUS(IB_EALLOC);
        }
        exlen = bslen;
    }

    /* Handle bytestr operations (currently only set) */
    if (svdata->op == SETVAR_STRSET) {
        assert(svdata->type == IB_FTYPE_BYTESTR);
        ib_bytestr_t *bs = NULL;

        if (cur != NULL) {
            ib_data_remove(tx->dpi, svdata->name, NULL);
        }

        /* Create a bytestr to hold it. */
        rc = ib_bytestr_alias_mem(&bs, tx->mp, (uint8_t *)expanded, exlen);
        if (rc != IB_OK) {
            ib_log_error(tx->ib, 4,
                         "setvar: Failed to bytestring for field %s: %s",
                         svdata->name, ib_status_to_string(rc));
            IB_FTRACE_RET_STATUS(rc);
        }

        /* Create the new field */
        rc = ib_field_create(
            &new,
            tx->mp,
            IB_FIELD_NAME(svdata->name),
            svdata->type,
            ib_ftype_bytestr_in(bs)
        );
        if (rc != IB_OK) {
            ib_log_error(tx->ib, 4,
                         "setvar: Failed to create field %s: %s",
                         svdata->name, ib_status_to_string(rc));
            IB_FTRACE_RET_STATUS(rc);
        }

        /* Add the field to the DPI */
        rc = ib_data_add(tx->dpi, new);
        if (rc != IB_OK) {
            ib_log_error(tx->ib, 4,
                         "setvar: Failed to add field %s: %s",
                         svdata->name, ib_status_to_string(rc));
            IB_FTRACE_RET_STATUS(rc);
        }
    }

    /* Numerical operation : Set */
    else if (svdata->op == SETVAR_NUMSET) {
        assert(svdata->type == IB_FTYPE_NUM);

        if (cur != NULL) {
            ib_data_remove(tx->dpi, svdata->name, NULL);
        }

        /* Create the new field */
        rc = ib_field_create(
            &new,
            tx->mp,
            IB_FIELD_NAME(svdata->name),
            svdata->type,
            ib_ftype_num_in(&svdata->value.num)
        );
        if (rc != IB_OK) {
            ib_log_error(tx->ib, 4,
                         "setvar: Failed to create field %s: %s",
                         svdata->name, ib_status_to_string(rc));
            IB_FTRACE_RET_STATUS(rc);
        }

        /* Add the field to the DPI */
        rc = ib_data_add(tx->dpi, new);
        if (rc != IB_OK) {
            ib_log_error(tx->ib, 4,
                         "setvar: Failed to add field %s: %s",
                         svdata->name, ib_status_to_string(rc));
            IB_FTRACE_RET_STATUS(rc);
        }
    }

    /* Numerical operation : Add */
    else if (svdata->op == SETVAR_NUMADD) {
        assert(svdata->type == IB_FTYPE_NUM);
        if (cur == NULL) {
            ib_log_error(tx->ib, 4,
                         "setvar: field %s does not exist for NUMADD action",
                         svdata->name);
            IB_FTRACE_RET_STATUS(IB_EINVAL);
        }

        /* Handle num and unum types */
        if (cur->type == IB_FTYPE_NUM) {
            ib_num_t num;
            rc = ib_field_value(cur, ib_ftype_num_out(&num));
            if (rc != IB_OK) {
                IB_FTRACE_RET_STATUS(rc);
            }

            num += svdata->value.num;
            ib_field_setv(cur, ib_ftype_num_in(&num));
        }
        else if (cur->type == IB_FTYPE_UNUM) {
            ib_unum_t num;
            rc = ib_field_setv(cur, ib_ftype_unum_out(&num));
            if (rc != IB_OK) {
                IB_FTRACE_RET_STATUS(rc);
            }

            num += (ib_unum_t)svdata->value.num;
            ib_field_setv(cur, ib_ftype_unum_in(&num));
        }
        else {
            ib_log_error(tx->ib, 4,
                         "setvar: field %s type %d invalid for NUMADD action",
                         svdata->name, cur->type);
            IB_FTRACE_RET_STATUS(IB_EINVAL);
        }
    }

    /* Should never get here. */
    else {
        assert(0 && "Invalid setvar operator");
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t ib_core_actions_init(ib_engine_t *ib, ib_module_t *mod)
{
    IB_FTRACE_INIT();
    ib_status_t  rc;

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
    rc = ib_action_register(ib,
                            "dlog",
                            IB_ACT_FLAG_NONE,
                            act_log_create,
                            NULL, /* no destroy function */
                            act_debuglog_execute);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Register the set flag action */
    rc = ib_action_register(ib,
                            "setflag",
                            IB_ACT_FLAG_NONE,
                            act_setflags_create,
                            NULL, /* no destroy function */
                            act_setflag_execute);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Register the set variable action */
    rc = ib_action_register(ib,
                            "setvar",
                            IB_ACT_FLAG_NONE,
                            act_setvar_create,
                            NULL, /* no destroy function */
                            act_setvar_execute);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Register the event action */
    rc = ib_action_register(ib,
                            "event",
                            IB_ACT_FLAG_NONE,
                            NULL, /* no create function */
                            NULL, /* no destroy function */
                            act_event_execute);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}
