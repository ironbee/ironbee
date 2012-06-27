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
 * @brief IronBee &mdash; core actions
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
#include <ironbee/util.h>

#include "engine_private.h"

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
 * Create function for the setflags action.
 *
 * @param[in] ib IronBee engine (unused)
 * @param[in] ctx Current context.
 * @param[in] mp Memory pool to use for allocation
 * @param[in] parameters Constant parameters from the rule definition
 * @param[in,out] inst Action instance
 * @param[in] cbdata Unused.
 *
 * @returns Status code
 */
static ib_status_t act_setflags_create(ib_engine_t *ib,
                                       ib_context_t *ctx,
                                       ib_mpool_t *mp,
                                       const char *parameters,
                                       ib_action_inst_t *inst,
                                       void *cbdata)
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
 *
 * @param[in] data Name of the flag to set
 * @param[in] rule The matched rule
 * @param[in] tx IronBee transaction
 * @param[in] flags Action instance flags
 * @param[in] cbdata Unused.
 *
 * @returns Status code
 */
static ib_status_t act_setflag_execute(void *data,
                                       ib_rule_t *rule,
                                       ib_tx_t *tx,
                                       ib_flags_t flags,
                                       void *cbdata)
{
    IB_FTRACE_INIT();

    /* Data will be a C-Style string */
    const char *cstr = (const char *)data;

    /* Handle the suspicious flag */
    if (strcasecmp(cstr, "suspicious") == 0) {
        ib_tx_flags_set(tx, IB_TX_FSUSPICIOUS);
    }
    else {
        ib_log_error_tx(tx,  "Set flag action: invalid flag '%s'", cstr);
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
 * @param[in] flags Action instance flags
 * @param[in] cbdata Unused.
 *
 * @returns IB_OK if successful.
 */
static ib_status_t act_event_execute(void *data,
                                     ib_rule_t *rule,
                                     ib_tx_t *tx,
                                     ib_flags_t flags,
                                     void *cbdata)
{
    IB_FTRACE_INIT();
    ib_status_t  rc;
    ib_logevent_t *event;
    const char *expanded;

    ib_log_debug_tx(tx, "Creating event via action");

    /* Expand the message string */
    if ( (rule->meta.flags & IB_RULEMD_FLAG_EXPAND_MSG) != 0) {
        char *tmp;
        rc = ib_data_expand_str(tx->dpi, rule->meta.msg, &tmp);
        if (rc != IB_OK) {
            ib_log_error_tx(tx,
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
                ib_log_error_tx(tx,
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
            ib_log_error_tx(tx,  "event: Failed to set data: %s",
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
 *
 * @param[in] ib IronBee engine (unused)
 * @param[in] ctx Current context.
 * @param[in] mp Memory pool to use for allocation
 * @param[in] params Constant parameters from the rule definition
 * @param[in,out] inst Action instance
 * @param[in] cbdata Unused.
 *
 * @returns Status code
 */
static ib_status_t act_setvar_create(ib_engine_t *ib,
                                     ib_context_t *ctx,
                                     ib_mpool_t *mp,
                                     const char *params,
                                     ib_action_inst_t *inst,
                                     void *cbdata)
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
        bool expand = false;

        rc = ib_data_expand_test_str_ex(value, vlen, &expand);
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }
        else if (expand == true) {
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
 *
 * @param[in] data Name of the flag to set
 * @param[in] rule The matched rule
 * @param[in] tx IronBee transaction
 * @param[in] flags Action instance flags
 * @param[in] cbdata Unused.
 *
 * @returns Status code
 */
static ib_status_t act_setvar_execute(void *data,
                                      ib_rule_t *rule,
                                      ib_tx_t *tx,
                                      ib_flags_t flags,
                                      void *cbdata)
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
    const setvar_data_t *svdata = (const setvar_data_t *)data;

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
            tx->dpi, bsdata, bslen, false, &expanded, &exlen);
        if (rc != IB_OK) {
            ib_log_error_tx(tx,
                         "setvar: Failed to expand string '%.*s': %s",
                         (int) bslen, bsdata, ib_status_to_string(rc));
            IB_FTRACE_RET_STATUS(rc);
        }
    }
    else if (svdata->type == IB_FTYPE_BYTESTR) {
        expanded = ib_mpool_memdup(tx->mp, bsdata, bslen);
        if (expanded == NULL) {
            ib_log_error_tx(tx,
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
            ib_log_error_tx(tx,
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
            ib_log_error_tx(tx,
                         "setvar: Failed to create field %s: %s",
                         svdata->name, ib_status_to_string(rc));
            IB_FTRACE_RET_STATUS(rc);
        }

        /* Add the field to the DPI */
        rc = ib_data_add(tx->dpi, new);
        if (rc != IB_OK) {
            ib_log_error_tx(tx,
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
            ib_log_error_tx(tx,
                         "setvar: Failed to create field %s: %s",
                         svdata->name, ib_status_to_string(rc));
            IB_FTRACE_RET_STATUS(rc);
        }

        /* Add the field to the DPI */
        rc = ib_data_add(tx->dpi, new);
        if (rc != IB_OK) {
            ib_log_error_tx(tx,
                         "setvar: Failed to add field %s: %s",
                         svdata->name, ib_status_to_string(rc));
            IB_FTRACE_RET_STATUS(rc);
        }
    }

    /* Numerical operation : Add */
    else if (svdata->op == SETVAR_NUMADD) {
        assert(svdata->type == IB_FTYPE_NUM);
        if (cur == NULL) {
            ib_log_error_tx(tx,
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
            ib_log_error_tx(tx,
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

/**
 * Set the IB_TX_BLOCK_ADVISORY flag and set the DPI value @c FLAGS:BLOCK=1.
 *
 * @param[out] tx The transaction we are going to modify.
 *
 * @return IB_DECLINED
 */
static ib_status_t act_block_advisory_execute(ib_tx_t *tx)
{
    IB_FTRACE_INIT();

    ib_status_t rc;
    ib_field_t *ib_flags_field;
    ib_field_t *ib_block_field;
    ib_num_t ib_num_one = 1;

    /* Don't re-set the flag because it bloats the DPI value FLAGS
     * with lots of BLOCK entries. */
    if (!ib_tx_flags_isset(tx, IB_TX_BLOCK_ADVISORY)) {

        /* Set the flag in the transaction. */
        ib_tx_flags_set(tx, IB_TX_BLOCK_ADVISORY);

        /* Get the FLAGS list of values or create it if missing. */
        rc = ib_data_get(tx->dpi, "FLAGS", &ib_flags_field);
        if (rc == IB_ENOENT) {

            rc = ib_data_add_list(tx->dpi, "FLAGS", &ib_flags_field);
            if (rc != IB_OK) {
                ib_log_error_tx(tx, "Could not create FLAGS field: %s",
                                    ib_status_to_string(rc));
                IB_FTRACE_RET_STATUS(rc);
            }

        }
        else if (rc != IB_OK) {
            ib_log_error_tx(tx, "Could not retrieve FLAGS field: %s",
                                ib_status_to_string(rc));
            IB_FTRACE_RET_STATUS(rc);
        }

        /* Create the IB_FTYPE_NUM field to add to FLAGS. */
        rc = ib_field_create(&ib_block_field,
                             tx->mp,
                             "BLOCK",
                             5,
                             IB_FTYPE_NUM,
                             &ib_num_one);
        if ( rc != IB_OK ) {
            ib_log_error_tx(tx, "Failed to create the IB_NUM field BLOCK: %s",
                                ib_status_to_string(rc));
            IB_FTRACE_RET_STATUS(rc);
        }

        /* Add BLOCK=1 to FLAGS. */
        rc = ib_field_list_add(ib_flags_field, ib_block_field);
        if (rc != IB_OK) {
            ib_log_error_tx(tx, "Could not add blocked field to flags: %s",
                                ib_status_to_string(rc));
            IB_FTRACE_RET_STATUS(rc);
        }
    }

    IB_FTRACE_RET_STATUS(IB_DECLINED);
}

/**
 * Set the IB_TX_BLOCK_PHASE flag in the tx.
 *
 * @param[out] tx The transaction we are going to modify.
 *
 * @return IB_DECLINED
 */
static ib_status_t act_block_phase_execute(ib_tx_t *tx)
{
    IB_FTRACE_INIT();

    ib_tx_flags_set(tx, IB_TX_BLOCK_PHASE);

    IB_FTRACE_RET_STATUS(IB_DECLINED);
}

/**
 * Set the IB_TX_BLOCK_IMMEDIATE flag in the tx.
 *
 * @param[out] tx The transaction we are going to modify.
 *
 * @returns IB_DECLINED.
 */
static ib_status_t act_block_immediate_execute(ib_tx_t *tx)
{
    IB_FTRACE_INIT();

    ib_tx_flags_set(tx, IB_TX_BLOCK_IMMEDIATE);

    IB_FTRACE_RET_STATUS(IB_DECLINED);
}

/**
 * The function that implements flagging a particular block type.
 */
typedef ib_status_t(*act_block_execution_t)(ib_tx_t *);

/**
 * Internal block action structure.
 *
 * This holds a pointer to the block callback that will be used.
 */
struct act_block_t {
    act_block_execution_t execute; /**< What block method should be used. */
};
typedef struct act_block_t act_block_t;

/**
 * Executes the function stored in cbdata.
 *
 * @param[in] data Cast to an @c act_block_t and the @c execute field is
 *            called on the given @a tx.
 * @param[in] rule The rule structure.
 * @param[out] tx The transaction we are going to modify.
 * @param[in] flags Flags. Unused.
 */
static ib_status_t act_block_execute(void* data,
                                     ib_rule_t *rule,
                                     ib_tx_t *tx,
                                     ib_flags_t flags,
                                     void *cbdata)
{
    IB_FTRACE_INIT();

    assert(data);
    assert(tx);

    ib_status_t rc = ((const act_block_t *)data)->execute(tx);

    IB_FTRACE_RET_STATUS(rc);
}

/**
 * Create / initialize a new instance of an action.
 *
 * @param[in] ib IronBee engine.
 * @param[in] ctx Context.
 * @param[in] mp Memory pool.
 * @param[in] params Parameters. These may be "immediate", "phase", or
 *            "advise". If null, "advisory" is assumed.
 *            These select the type of block that will be put in place
 *            by deciding which callback (act_block_phase_execute(),
 *            act_block_immediate_execute(), or act_block_advise_execute())
 *            is assigned to the rule data object.
 * @param[out] inst The instance being initialized.
 * @param[in] cbdata Unused.
 *
 * @return IB_OK on success or IB_EALLOC if the callback data
 *         cannot be initialized for the rule.
 */
static ib_status_t act_block_create(ib_engine_t *ib,
                                    ib_context_t *ctx,
                                    ib_mpool_t *mp,
                                    const char *params,
                                    ib_action_inst_t *inst,
                                    void *cbdata)
{
    IB_FTRACE_INIT();

    act_block_t *act_block =
        (act_block_t *)ib_mpool_alloc(mp, sizeof(*act_block));
    if ( act_block == NULL ) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    /* When params are NULL, use advisory blocking by default. */
    if ( params == NULL ) {
        act_block->execute = &act_block_advisory_execute;
    }

    /* Just note that a block should be done, according to this rule. */
    else if ( ! strcasecmp("advisory", params) ) {
        act_block->execute = &act_block_advisory_execute;
    }

    /* Block at the end of the phase. */
    else if ( ! strcasecmp("phase", params) ) {
        act_block->execute = &act_block_phase_execute;
    }

    /* Immediate blocking. Block ASAP. */
    else if ( ! strcasecmp("immediate", params) ) {
        act_block->execute = &act_block_immediate_execute;
    }

    /* As with params == NULL, the default is to use an advisory block. */
    else {
        act_block->execute = &act_block_advisory_execute;
    }

    /* Assign the built up context object. */
    inst->data = act_block;

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * Holds the status code that a @c status action will set in the @c tx.
 */
struct act_status_t {
    int block_status; /**< The status to copy into @c tx->block_status. */
};
typedef struct act_status_t act_status_t;

/**
 * Set the @c block_status value in @a tx.
 *
 * @param[in] data The act_status_t that contains the @c block_status
 *            to assign to @c tx->block_status.
 * @param[in] rule The rule. Unused.
 * @param[out] tx The field in this struct, @c block_status, is set.
 * @param[in] flags The flags used to create this rule. Unused.
 *
 * @returns IB_OK.
 */
static ib_status_t act_status_execute(void* data,
                                      ib_rule_t *rule,
                                      ib_tx_t *tx,
                                      ib_flags_t flags,
                                      void *cbdata)
{
    IB_FTRACE_INIT();

    assert(data);
    assert(tx);

    /* NOTE: Range validation of block_status is done in act_status_create. */
    tx->block_status = ((act_status_t *)data)->block_status;

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * Create an action that sets the TX's block_status value.
 *
 * @param[in] ib The IronBee engine.
 * @param[in] ctx The current context. Unused.
 * @param[in] mp The memory pool that will allocate the act_status_t
 *            holder for the status value.
 * @param[in] params The parameters. This is a string representing
 *            an integer from 200 to 599, inclusive.
 * @param[out] inst The action instance that will be initialized.
 * @param[in] cbdata Unused.
 *
 * @return IB_OK on success. IB_EALLOC on an allocation error from mp.
 *         IB_EINVAL if @a param is NULL or not convertible with
 *         @c atoi(const @c char*) to an integer in the range 200 through 599,
 *         inclusive.
 */
static ib_status_t act_status_create(ib_engine_t *ib,
                                     ib_context_t *ctx,
                                     ib_mpool_t *mp,
                                     const char *params,
                                     ib_action_inst_t *inst,
                                     void *cbdata)
{
    IB_FTRACE_INIT();

    assert(inst);
    assert(mp);

    act_status_t *act_status;
    int block_status;

    act_status = (act_status_t *) ib_mpool_alloc(mp, sizeof(*act_status));
    if (act_status == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    if (params == NULL) {
        ib_log_error(ib, "Action status must be given a parameter "
                         "x where 200 <= x < 600.");
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    block_status = atoi(params);

    if ( block_status < 200 || block_status >= 600 ) {
        ib_log_error(ib, "Action status must be given a parameter "
                         "x where 200 <= x < 600. It was given %s.",
                         params);
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    act_status->block_status = block_status;

    inst->data = act_status;

    IB_FTRACE_RET_STATUS(IB_OK);
}


/**
 * Holds the name of the header and the value to set it to.
 */
struct act_header_set_t {
    const char *name; /**< Name of the header to operate on. */
    const char *value; /**< Value to replace the header with. */
};
typedef struct act_header_set_t act_header_set_t;

/**
 * Holds the name of the header to delete.
 */
struct act_header_del_t {
    const char *name; /**< Name of the header to remove. */
};
typedef struct act_header_del_t act_header_del_t;

/**
 * Common create routine for delResponseHeader and delRequestHeader action.
 *
 * @param[in] ib The IronBee engine.
 * @param[in] ctx The context.
 * @param[in] mp The memory pool this is allocated out of.
 * @param[in] params Parameters of the format name=<header name>.
 * @param[out] inst The action instance being initialized.
 * @param[in] cbdata Unused.
 *
 * @return IB_OK on success. IB_EALLOC if a memory allocation fails.
 */
static ib_status_t act_del_header_create(ib_engine_t *ib,
                                         ib_context_t *ctx,
                                         ib_mpool_t *mp,
                                         const char *params,
                                         ib_action_inst_t *inst,
                                         void *cbdata)
{
    IB_FTRACE_INIT();

    act_header_del_t *act_header_del =
        (act_header_del_t *)ib_mpool_alloc(mp, sizeof(*act_header_del));

    if ( act_header_del == NULL ) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    if ( params == NULL || strlen(params) == 0 ) {
        ib_log_error(ib, "Operation requires a parameter.");
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    act_header_del->name = ib_mpool_strdup(mp, params);

    if (act_header_del->name == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    inst->data = act_header_del;

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * Common create routine for setResponseHeader and setRequestHeader actions.
 *
 * @param[in] ib The IronBee engine.
 * @param[in] ctx The context.
 * @param[in] mp The memory pool this is allocated out of.
 * @param[in] params Parameters of the format name=<header name>.
 * @param[out] inst The action instance being initialized.
 * @param[in] cbdata Unused.
 *
 * @return IB_OK on success. IB_EALLOC if a memory allocation fails.
 */
static ib_status_t act_set_header_create(ib_engine_t *ib,
                                         ib_context_t *ctx,
                                         ib_mpool_t *mp,
                                         const char *params,
                                         ib_action_inst_t *inst,
                                         void *cbdata)
{
    IB_FTRACE_INIT();

    size_t name_len;
    size_t value_len;
    size_t params_len;
    char *equals_idx;
    act_header_set_t *act_header_set =
        (act_header_set_t *)ib_mpool_alloc(mp, sizeof(*act_header_set));

    if (act_header_set == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    if (params == NULL || strlen(params) == 0) {
        ib_log_error(ib, "Operation requires a parameter");
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    equals_idx = index(params, '=');

    /* If the returned value was NULL it is an error. */
    if (equals_idx == NULL) {
        ib_log_error(ib, "Format for parameter is name=value: %s", params);
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    /* Compute string lengths needed for parsing out name and value. */
    params_len = strlen(params);
    name_len = equals_idx - params;
    value_len = params_len - name_len - 1;

    act_header_set->name = (const char *)ib_mpool_memdup(mp,
                                                         params,
                                                         name_len+1);
    if (act_header_set->name == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    /* Terminate name with '\0'. This replaces the '=' that was copied.
     * Notice that we strip the const-ness of this value to make this one
     * assignment. */
    ((char *)act_header_set->name)[name_len] = '\0';

    act_header_set->value = (value_len == 0)?
        ib_mpool_strdup(mp, ""):
        ib_mpool_strdup(mp, equals_idx+1);
    if (act_header_set->value == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    inst->data = act_header_set;
    IB_FTRACE_RET_STATUS(IB_OK);
}

static ib_status_t act_set_request_header_execute(void* data,
                                                  ib_rule_t *rule,
                                                  ib_tx_t *tx,
                                                  ib_flags_t flags,
                                                  void *cbdata)
{
    IB_FTRACE_INIT();

    assert(data);
    assert(tx);
    assert(tx->ib);
    assert(tx->ib->server);

    ib_status_t rc;
    act_header_set_t *act_header_set = (act_header_set_t *)data;
    char *expanded_value;

    rc = ib_data_expand_str(tx->dpi, act_header_set->value, &expanded_value);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    ib_log_debug_tx(tx, "Setting request header %s=%s",
                    act_header_set->name, expanded_value);

    rc = ib_server_header(tx->ib->server,
                          tx,
                          IB_SERVER_REQUEST,
                          IB_HDR_SET,
                          act_header_set->name,
                          expanded_value);

    IB_FTRACE_RET_STATUS(rc);
}

static ib_status_t act_del_request_header_execute(void* data,
                                                  ib_rule_t *rule,
                                                  ib_tx_t *tx,
                                                  ib_flags_t flags,
                                                  void *cbdata)
{
    IB_FTRACE_INIT();

    assert(data);
    assert(tx);
    assert(tx->ib);
    assert(tx->ib->server);

    ib_status_t rc;
    act_header_del_t *act_header_del = (act_header_del_t *)data;

    ib_log_debug_tx(tx, "Deleting request header %s",
                    act_header_del->name);
    rc = ib_server_header(tx->ib->server,
                          tx,
                          IB_SERVER_REQUEST,
                          IB_HDR_UNSET,
                          act_header_del->name,
                          "");

    IB_FTRACE_RET_STATUS(rc);
}

static ib_status_t act_set_response_header_execute(void* data,
                                                  ib_rule_t *rule,
                                                  ib_tx_t *tx,
                                                  ib_flags_t flags,
                                                  void *cbdata)
{
    IB_FTRACE_INIT();

    assert(data);
    assert(tx);
    assert(tx->ib);
    assert(tx->ib->server);

    ib_status_t rc;
    act_header_set_t *act_header_set = (act_header_set_t *)data;
    char *expanded_value;

    rc = ib_data_expand_str(tx->dpi, act_header_set->value, &expanded_value);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    ib_log_debug_tx(tx, "Setting response header %s=%s",
                    act_header_set->name, act_header_set->value);
    rc = ib_server_header(tx->ib->server,
                          tx,
                          IB_SERVER_RESPONSE,
                          IB_HDR_SET,
                          act_header_set->name,
                          expanded_value);

    IB_FTRACE_RET_STATUS(rc);
}

static ib_status_t act_del_response_header_execute(void* data,
                                                  ib_rule_t *rule,
                                                  ib_tx_t *tx,
                                                  ib_flags_t flags,
                                                  void *cbdata)
{
    IB_FTRACE_INIT();

    assert(data);
    assert(tx);
    assert(tx->ib);
    assert(tx->ib->server);

    ib_status_t rc;
    act_header_del_t *act_header_del = (act_header_del_t *)data;

    ib_log_debug_tx(tx, "Deleting response header %s",
                    act_header_del->name);
    rc = ib_server_header(tx->ib->server,
                          tx,
                          IB_SERVER_RESPONSE,
                          IB_HDR_UNSET,
                          act_header_del->name,
                          "");

    IB_FTRACE_RET_STATUS(rc);
}

/**
 * Create function for the allow action.
 *
 * @param[in] ib IronBee engine (unused)
 * @param[in] ctx Current context.
 * @param[in] mp Memory pool to use for allocation
 * @param[in] parameters Constant parameters from the rule definition
 * @param[in,out] inst Action instance
 * @param[in] cbdata Unused.
 *
 * @returns Status code
 */
static ib_status_t act_allow_create(ib_engine_t *ib,
                                    ib_context_t *ctx,
                                    ib_mpool_t *mp,
                                    const char *parameters,
                                    ib_action_inst_t *inst,
                                    void *cbdata)
{
    IB_FTRACE_INIT();
    ib_flags_t flags = IB_TX_FNONE;
    ib_flags_t *idata;

    if (parameters == NULL) {
        flags |= IB_TX_ALLOW_ALL;
    }
    else if (strcasecmp(parameters, "phase") == 0) {
        flags |= IB_TX_ALLOW_PHASE;
    }
    else if (strcasecmp(parameters, "request") == 0) {
        flags |= IB_TX_ALLOW_REQUEST;
    }
    else {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    idata = ib_mpool_alloc(mp, sizeof(*idata));
    if (idata == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    *idata = flags;
    inst->data = idata;

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * Allow action.
 *
 * @param[in] data Not used.
 * @param[in] rule The rule structure.
 * @param[in,out] tx The transaction we are going to modify.
 * @param[in] flags Flags. Unused.
 * @param[in] cbdata Unused.
 */
static ib_status_t act_allow_execute(void *data,
                                     ib_rule_t *rule,
                                     ib_tx_t *tx,
                                     ib_flags_t flags,
                                     void *cbdata)
{
    IB_FTRACE_INIT();

    assert(data != NULL);
    assert(rule != NULL);
    assert(tx != NULL);

    const ib_flags_t *pflags = (const ib_flags_t *)data;
    ib_flags_t set_flags = *pflags;

    /* For post process, treat ALLOW_ALL like ALLOW_PHASE */
    if ( (rule->meta.phase == PHASE_POSTPROCESS) &&
         (ib_flags_all(set_flags, IB_TX_ALLOW_ALL) == true) )
    {
        set_flags |= IB_TX_ALLOW_PHASE;
    }

    /* Set the flags in the TX */
    ib_tx_flags_set(tx, set_flags);

    /* For ALLOW_PHASE, store the current phase */
    if (ib_flags_all(set_flags, IB_TX_ALLOW_PHASE) == true) {
        tx->allow_phase = rule->meta.phase;
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t ib_core_actions_init(ib_engine_t *ib, ib_module_t *mod)
{
    IB_FTRACE_INIT();
    ib_status_t  rc;

    /* Register the set flag action. */
    rc = ib_action_register(ib,
                            "setflag",
                            IB_ACT_FLAG_NONE,
                            act_setflags_create, NULL,
                            NULL, /* no destroy function */ NULL,
                            act_setflag_execute, NULL);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Register the set variable action. */
    rc = ib_action_register(ib,
                            "setvar",
                            IB_ACT_FLAG_NONE,
                            act_setvar_create, NULL,
                            NULL, /* no destroy function */ NULL,
                            act_setvar_execute, NULL);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Register the event action. */
    rc = ib_action_register(ib,
                            "event",
                            IB_ACT_FLAG_NONE,
                            NULL, /* no create function */ NULL,
                            NULL, /* no destroy function */ NULL,
                            act_event_execute, NULL);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Register the block action. */
    rc = ib_action_register(ib,
                            "block",
                            IB_ACT_FLAG_NONE,
                            act_block_create, NULL,
                            NULL, NULL,
                            act_block_execute, NULL);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Register the allow actions. */
    rc = ib_action_register(ib,
                            "allow",
                            IB_ACT_FLAG_NONE,
                            act_allow_create, NULL,
                            NULL, NULL,
                            act_allow_execute, NULL);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Register the status action to modify how block is performed. */
    rc = ib_action_register(ib,
                            "status",
                            IB_ACT_FLAG_NONE,
                            act_status_create, NULL,
                            NULL, NULL,
                            act_status_execute, NULL);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = ib_action_register(ib,
                            "setRequestHeader",
                            IB_ACT_FLAG_NONE,
                            act_set_header_create, NULL,
                            NULL, NULL,
                            act_set_request_header_execute, NULL);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = ib_action_register(ib,
                            "delRequestHeader",
                            IB_ACT_FLAG_NONE,
                            act_del_header_create, NULL,
                            NULL, NULL,
                            act_del_request_header_execute, NULL);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = ib_action_register(ib,
                            "setResponseHeader",
                            IB_ACT_FLAG_NONE,
                            act_set_header_create, NULL,
                            NULL, NULL,
                            act_set_response_header_execute, NULL);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = ib_action_register(ib,
                            "delResponseHeader",
                            IB_ACT_FLAG_NONE,
                            act_del_header_create, NULL,
                            NULL, NULL,
                            act_del_response_header_execute, NULL);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}
