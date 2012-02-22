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
#include <ironbee/debug.h>
#include <ironbee/types.h>
#include <ironbee/field.h>
#include <ironbee/mpool.h>
#include <ironbee/action.h>
#include <ironbee/rule_engine.h>

#include "ironbee_core_private.h"


/**
 * Setvar action data.
 */
typedef enum {
    SETVAR_SET,                    /**< Set to a constant value */
    SETVAR_ADD,                    /**< Add to a value (counter) */
    SETVAR_SUB,                    /**< Subtract from a value (counter) */
} setvar_op_t;
typedef struct {
    setvar_op_t       op;          /**< Setvar operation */
    char             *name;        /**< Field name */
    ib_ftype_t        type;        /**< Data type */
    union {
        ib_num_t      num;         /**< Numeric value */
        const char   *str;         /**< String value */
    }                 value;       /**< Value */
} setvar_data_t;

/**
 * @internal
 * Create function for the log action.
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
 * @internal
 * Execute function for the "log" action
 *
 * @param[in] data C-style string to log
 * @param[in] rule The matched rule
 * @param[in] tx IronBee transaction
 *
 * @returns Status code
 */
static ib_status_t act_log_execute(void *data,
                                   ib_rule_t *rule,
                                   ib_tx_t *tx)
{
    IB_FTRACE_INIT();

    /* This works on C-style (NUL terminated) strings */
    const char *cstr = (const char *)data;

    ib_log_debug(tx->ib, 9, "LOG: %s", cstr);
    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * @internal
 * Create function for the setflags action.
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
 * @internal
 * Execute function for the "set flag" action
 *
 * @param[in] data Name of the flag to set
 * @param[in] rule The matched rule
 * @param[in] tx IronBee transaction
 *
 * @returns Status code
 */
static ib_status_t act_setflag_execute(void *data,
                                       ib_rule_t *rule,
                                       ib_tx_t *tx)
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
 *
 * Create and event and log it.
 *
 * @param[in] data Instance data needed for execution.
 * @param[in] rule The rule executing this action.
 * @param[in] tx The transaction for this action.
 *
 * @returns IB_OK if successful.
 */
static ib_status_t act_event_execute(void *data,
                                     ib_rule_t *rule,
                                     ib_tx_t *tx)
{
    IB_FTRACE_INIT();
    ib_status_t  rc;
    ib_logevent_t *e;

    ib_log_debug(tx->ib, 4, "Creating event via action");

    rc = ib_logevent_create(
        &e,
        tx->mp,
        ib_rule_id(rule),
        IB_LEVENT_TYPE_ALERT,
        IB_LEVENT_ACT_UNKNOWN,
        IB_LEVENT_PCLASS_UNKNOWN,
        IB_LEVENT_SCLASS_UNKNOWN,
        IB_LEVENT_SYS_UNKNOWN,
        IB_LEVENT_ACTION_IGNORE,
        IB_LEVENT_ACTION_IGNORE,
        rule->meta.confidence,
        rule->meta.severity,
        (rule->meta.msg ? rule->meta.msg : "")
    );
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Link to rule tags. */
    /// @todo Probably need to copy here
    e->tags = rule->meta.tags;

    /* Log the event. */
    rc = ib_event_add(tx->epi, e);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * @internal
 * Create function for the setvar action.
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
    size_t vlen;                 /* Length of the value */
    const char *eq;              /* '=' character in @a params */
    const char *value;           /* Value in @a params */
    int modifier = 0;            /* Modifier character '+' or '-' */
    ib_ftype_t ftype;            /* Field type */
    setvar_data_t *data;         /* Data for the execute function */

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
    if ( (*value == '+') || (*value == '-') ) {
        size_t digits = strspn(value+1, "0123456789");
        if (vlen == (digits + 1)) {
            modifier = *value;
            ftype = IB_FTYPE_NUM;
            ++value;
        }
        else {
            ftype = IB_FTYPE_NULSTR;
        }
    }
    else {
        size_t digits = strspn(value, "0123456789");
        if (vlen == digits) {
            ftype = IB_FTYPE_NUM;
        }
        else {
            ftype = IB_FTYPE_NULSTR;
        }
    }

    /* Create the data structure for the execute function */
    data = ib_mpool_alloc(mp, sizeof(*data) );
    if (data == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }
    data->type = ftype;

    /* Copy the name */
    data->name = ib_mpool_calloc(mp, nlen+1, 1);
    if (data->name == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }
    memcpy(data->name, params, nlen);

    /* Create the value */
    if (ftype == IB_FTYPE_NUM) {
        data->value.num = (ib_num_t) strtol(value, NULL, 0);
    }
    else {
        data->value.str = ib_mpool_strdup(mp, value);
        if (data->value.str == NULL) {
            IB_FTRACE_RET_STATUS(IB_EALLOC);
        }
    }

    /* Fill in the data structure */
    if (modifier == 0) {
        data->op = SETVAR_SET;
    }
    else {
        data->op = (modifier == '+') ? SETVAR_ADD : SETVAR_SUB;
    }

    inst->data = data;
    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * @internal
 * Execute function for the "set variable" action
 *
 * @param[in] data Name of the flag to set
 * @param[in] rule The matched rule
 * @param[in] tx IronBee transaction
 *
 * @returns Status code
 */
static ib_status_t act_setvar_execute(void *data,
                                      ib_rule_t *rule,
                                      ib_tx_t *tx)
{
    IB_FTRACE_INIT();
    ib_field_t *cur = NULL;
    ib_field_t *new;
    ib_status_t rc;

    /* Data should be a setvar_data_t created in our create function */
    const setvar_data_t *svdata = (const setvar_data_t *)data;

    /* Get the current value */
    ib_data_get(tx->dpi, svdata->name, &cur);

    /* What we depends on the operation */
    if (svdata->op == SETVAR_SET) {
        if (cur != NULL) {
            ib_data_remove(tx->dpi, svdata->name, NULL);
        }

        /* Create the new field */
        rc = ib_field_create(&new, tx->mp, svdata->name, svdata->type,
                             (void *)&(svdata->value) );
        if (rc != IB_OK) {
            ib_log_error(tx->ib, 4,
                         "setvar: Failed to create field %s: %d",
                         svdata->name, rc);
            IB_FTRACE_RET_STATUS(rc);
        }

        /* Add the field to the DPI */
        rc = ib_data_add(tx->dpi, new);
        if (rc != IB_OK) {
            ib_log_error(tx->ib, 4,
                         "setvar: Failed to add field %s: %d",
                         svdata->name, rc);
            IB_FTRACE_RET_STATUS(rc);
        }
    }
    else {
        const char *opname = (svdata->op == SETVAR_ADD) ? "add" : "subtract";
        if (cur == NULL) {
            ib_log_error(tx->ib, 4,
                         "setvar: field %s doesn't exist for %s action",
                         svdata->name, opname);
            IB_FTRACE_RET_STATUS(IB_EINVAL);
        }

        /* Handle num and unum types */
        if (cur->type == IB_FTYPE_NUM) {
            ib_num_t *num = ib_field_value_num(cur);
            if (svdata->op == SETVAR_ADD) {
                *num += svdata->value.num;
            }
            else {
                *num -= svdata->value.num;
            }
        }
        else if (cur->type == IB_FTYPE_UNUM) {
            ib_unum_t *num = ib_field_value_unum(cur);
            if (svdata->op == SETVAR_ADD) {
                *num += (ib_unum_t)svdata->value.num;
            }
            else {
                *num -= (ib_unum_t)svdata->value.num;
            }
        }
        else {
            ib_log_error(tx->ib, 4,
                         "setvar: field %s type %d invalid for %s action",
                         svdata->name, cur->type, opname);
            IB_FTRACE_RET_STATUS(IB_EINVAL);
        }
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t ib_core_actions_init(ib_engine_t *ib, ib_module_t *mod)
{
    IB_FTRACE_INIT();
    ib_status_t  rc;

    /* Register the log action */
    rc = ib_action_register(ib,
                            "log",
                            IB_ACT_FLAG_NONE,
                            act_log_create,
                            NULL, /* no destroy function */
                            act_log_execute);
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
