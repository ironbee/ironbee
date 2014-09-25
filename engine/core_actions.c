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
 * @brief IronBee --- core actions
 *
 * @author Craig Forbes <cforbes@qualys.com>
 */

#include "ironbee_config_auto.h"

#include "core_private.h"
#include "engine_private.h"

#include <ironbee/action.h>
#include <ironbee/bytestr.h>
#include <ironbee/context.h>
#include <ironbee/escape.h>
#include <ironbee/field.h>
#include <ironbee/flags.h>
#include <ironbee/logevent.h>
#include <ironbee/mm.h>
#include <ironbee/rule_engine.h>
#include <ironbee/rule_logger.h>
#include <ironbee/string.h>
#include <ironbee/transformation.h>
#include <ironbee/types.h>
#include <ironbee/util.h>

#include <assert.h>
#include <inttypes.h>
#include <stdlib.h>
#include <strings.h>

/**
 * Data types for the setvar action.
 */
typedef enum {
    SETVAR_STRSET,                /**< Set to a constant string */
    SETVAR_NUMSET,                /**< Set to a constant number */
    SETVAR_NUMADD,                /**< Add to a value (counter) */
    SETVAR_NUMSUB,                /**< Subtract from a value (counter) */
    SETVAR_NUMMULT,               /**< Multiply to a value (counter) */
    SETVAR_FLOATSET,              /**< Set to a constant float. */
    SETVAR_FLOATADD,              /**< Add to a float. */
    SETVAR_FLOATSUB,              /**< Subtract from a float. */
    SETVAR_FLOATMULT,             /**< Multiply to a float. */
} setvar_op_t;


/**
 * Actual implementation of setvar operators for numbers.
 */
typedef ib_status_t (*setvar_num_op_fn_t)(
    const ib_num_t n1,
    const ib_num_t n2,
    ib_num_t *out);

/**
 * Actual implementation of setvar operators for floats.
 */
typedef ib_status_t (*setvar_float_op_fn_t)(
    const ib_float_t f1,
    const ib_float_t f2,
    ib_float_t *out
);

static ib_status_t setvar_num_sub_op(
    const ib_num_t n1,
    const ib_num_t n2,
    ib_num_t *out)
{

    *out = n1 - n2;

    return IB_OK;
}
static ib_status_t setvar_num_mult_op(
    const ib_num_t n1,
    const ib_num_t n2,
    ib_num_t *out)
{

    *out = n1 * n2;

    return IB_OK;
}
static ib_status_t setvar_num_add_op(
    const ib_num_t n1,
    const ib_num_t n2,
    ib_num_t *out)
{

    *out = n1 + n2;

    return IB_OK;
}

static ib_status_t setvar_float_sub_op(
    const ib_float_t f1,
    const ib_float_t f2,
    ib_float_t *out)
{

    *out = f1 - f2;

    return IB_OK;
}
static ib_status_t setvar_float_mult_op(
    const ib_float_t f1,
    const ib_float_t f2,
    ib_float_t *out)
{

    *out = f1 * f2;

    return IB_OK;
}
static ib_status_t setvar_float_add_op(
    const ib_float_t f1,
    const ib_float_t f2,
    ib_float_t *out)
{

    *out = f1 + f2;

    return IB_OK;
}

struct setvar_value_t {
    ib_field_t *value;           /**< The value to transform. */
    ib_list_t  *transformations; /**< List of transformation names. */
};
struct setvar_value_t setvar_value_t;

/**
 * Structure storing setvar instance data.
 */
struct setvar_data_t {
    setvar_op_t      op;              /**< Setvar operation */
    ib_var_target_t *target;          /**< Target to set. */

    /**
     * Holds a ib_num_t, ib_float_t, or a @ref ib_var_expand_t *.
     *
     * The @ref ib_var_expand_t is stored as a generic pointer type.
     */
    ib_field_t      *argument;        /**< The setvar argument. */
    ib_list_t       *transformations; /**< Names of tfns to apply to value. */
    const char      *target_str;      /**< Used in error logging. */
    size_t           target_str_len;  /**< Used in error logging. */
};
typedef struct setvar_data_t setvar_data_t;

/**
 * Data types for the setflag action.
 */
enum setflag_op_t {
    setflag_op_set,  /**< Set the flag */
    setflag_op_clear /**< Clear the flag */
};
typedef enum setflag_op_t setflag_op_t;

struct setflag_data_t {
    const ib_tx_flag_map_t *flag;
    setflag_op_t            op;
};
typedef struct setflag_data_t setflag_data_t;

/**
 * Data types for the event action.
 */
struct event_data_t {
    ib_logevent_type_t event_type; /**< Type of the event */
};
typedef struct event_data_t event_data_t;

/**
 * Perform float setvar operation.
 *
 * @param[in] tx Current transaction.
 * @param[in] rule_exec Rule execution environment.
 * @param[in] argument The value to pass to setvar.
 * @param[in] op The operator to perform on the two fields.
 *            The value in cur_field is passed as the first argument
 *            to @a op. The second argument to @a op is the
 *            parameter presented at configuration time.
 * @param[out] cur_field The current field being used for the left hand side
 *             and to store the result in.
 *
 * @returns
 *   - IB_OK
 */
static
ib_status_t setvar_float_op(
    ib_tx_t               *tx,
    const ib_rule_exec_t  *rule_exec,
    const ib_field_t      *argument,
    setvar_float_op_fn_t   op,
    ib_field_t           **cur_field
)
{
    assert(tx                 != NULL);
    assert(rule_exec          != NULL);
    assert(cur_field          != NULL);

    ib_status_t rc;
    ib_float_t  flt1;
    ib_float_t  flt2;

    /* If it doesn't exist, create the variable with a value of zero */
    if (*cur_field == NULL) {
        ib_float_t initial = 0;
        rc = ib_field_create(cur_field,
                             tx->mm,
                             "", 0,
                             IB_FTYPE_FLOAT,
                             ib_ftype_float_in(&initial));
        if (rc != IB_OK) {
            return rc;
        }
    }
    else if ((*cur_field)->type != IB_FTYPE_FLOAT) {
        return IB_EINVAL;
    }

    /* Get arg 1. */
    rc = ib_field_value(*cur_field, ib_ftype_float_out(&flt1));
    if (rc != IB_OK) {
        return rc;
    }

    /* Get arg 2. */
    rc = ib_field_value_type(
        argument,
        ib_ftype_float_out(&flt2),
        IB_FTYPE_FLOAT);
    if (rc != IB_OK) {
        return rc;
    }

    /* Finally do the work. */
    op(flt1, flt2, &flt1);

    ib_field_setv(*cur_field, ib_ftype_float_in(&flt1));

    return IB_OK;
}

/**
 * Perform number setvar operation.
 *
 * @param[in] tx Current transaction.
 * @param[in] rule_exec Rule execution environment.
 * @param[in] argument The value to pass to setvar.
 * @param[in] cur_field The current field being used for the left hand side
 *            and to store the result in.
 * @param[in] op The operator to perform on the two fields.
 *            The value in cur_field is passed as the first argument
 *            to @a op. The second argument to @a op is the
 *            parameter presented at configuration time.
 *
 * @returns
 *   - IB_OK
 */
static
ib_status_t setvar_num_op(
    ib_tx_t               *tx,
    const ib_rule_exec_t  *rule_exec,
    const ib_field_t      *argument,
    setvar_num_op_fn_t     op,
    ib_field_t           **cur_field
)
{
    assert(tx                 != NULL);
    assert(rule_exec          != NULL);
    assert(cur_field          != NULL);

    ib_status_t rc;
    ib_num_t    num1;
    ib_num_t    num2;

    /* If it doesn't exist, create the variable with a value of zero */
    if (*cur_field == NULL) {
        ib_num_t initial = 0;
        rc = ib_field_create(cur_field,
                             tx->mm,
                             "", 0,
                             IB_FTYPE_NUM,
                             ib_ftype_num_in(&initial));
        if (rc != IB_OK) {
            return rc;
        }
    }
    else if ((*cur_field)->type != IB_FTYPE_NUM) {
        return IB_EINVAL;
    }

    /* Get arg 1. */
    rc = ib_field_value(*cur_field, ib_ftype_num_out(&num1));
    if (rc != IB_OK) {
        return rc;
    }

    /* Get arg 2. */
    rc = ib_field_value_type(
        argument,
        ib_ftype_num_out(&num2),
        IB_FTYPE_NUM);
    if (rc != IB_OK) {
        return rc;
    }

    op(num1, num2, &num1);

    ib_field_setv(*cur_field, ib_ftype_num_in(&num1));

    return IB_OK;
}


/**
 * Create function for the setflags action.
 *
 * @param[in]  mm            Memory manager.
 * @param[in]  ctx           Context.
 * @param[in]  parameters    Parameters
 * @param[out] instance_data Instance data to pass to execute.
 * @param[in]  cbdata        Callback data.
 *
 * @returns Status code
 */
static ib_status_t act_setflags_create(
    ib_mm_t       mm,
    ib_context_t *ctx,
    const char   *parameters,
    void         *instance_data,
    void         *cbdata
)
{
    const ib_tx_flag_map_t *flag;
    setflag_op_t op;

    if (parameters == NULL) {
        return IB_EINVAL;
    }

    if (*parameters == '!') {
        op = setflag_op_clear;
        ++parameters;
    }
    else {
        op = setflag_op_set;
    }

    for (flag = ib_core_vars_tx_flags();  flag->name != NULL;  ++flag) {
        if (strcasecmp(flag->name, parameters) == 0) {
            setflag_data_t *data;

            if (flag->read_only) {
                return IB_EINVAL;
            }
            data = ib_mm_alloc(mm, sizeof(*data));
            if (data == NULL) {
                return IB_EALLOC;
            }
            data->op = op;
            data->flag = flag;
            *(void **)instance_data = (void *)data;
            return IB_OK;
        }
    }
    return IB_EINVAL;
}

/**
 * Execute function for the "set flag" action
 *
 * @param[in] rule_exec The rule execution object
 * @param[in] data Name of the flag to set
 * @param[in] cbdata Unused.
 *
 * @returns Status code
 */
static ib_status_t act_setflags_execute(
    const ib_rule_exec_t *rule_exec,
    void *data,
    void *cbdata)
{
    /* Data will be a setflag_data_t */
    const setflag_data_t *opdata = (const setflag_data_t *)data;
    ib_status_t           rc;
    ib_tx_t              *tx = rule_exec->tx;

    switch (opdata->op) {

    case setflag_op_set:
        rc = ib_tx_flags_set(tx, opdata->flag->tx_flag);
        break;

    case setflag_op_clear:
        rc = ib_tx_flags_unset(tx, opdata->flag->tx_flag);
        break;

    default:
        return IB_EINVAL;
    }

    return rc;
}

/**
 * Create function for the event action.
 *
 * @param[in]  mm            Memory manager.
 * @param[in]  ctx           Context.
 * @param[in]  parameters    Parameters
 * @param[out] instance_data Instance data to pass to execute.
 * @param[in]  cbdata        Callback data.
 *
 * @returns Status code
 */
static ib_status_t act_event_create(
    ib_mm_t       mm,
    ib_context_t *ctx,
    const char   *parameters,
    void         *instance_data,
    void         *cbdata
)
{
    assert(instance_data != NULL);

    event_data_t *event_data;
    ib_logevent_type_t event_type;

    if (parameters == NULL) {
        event_type = IB_LEVENT_TYPE_OBSERVATION;
    }
    else if (strcasecmp(parameters, "observation") == 0) {
        event_type = IB_LEVENT_TYPE_OBSERVATION;
    }
    else if (strcasecmp(parameters, "alert") == 0) {
        event_type = IB_LEVENT_TYPE_ALERT;
    }
    else {
        return IB_EINVAL;
    }

    /* Allocate an event data object, populate it */
    event_data = ib_mm_alloc(mm, sizeof(*event_data));
    if (event_data == NULL) {
        return IB_EALLOC;
    }
    event_data->event_type = event_type;
    *(void **)instance_data = (void *)event_data;

    return IB_OK;
}

/**
 * Event action execution callback.
 *
 * Create and event and log it.
 *
 * @param[in] rule_exec The rule execution object
 * @param[in] data Instance data needed for execution.
 * @param[in] cbdata Unused.
 *
 * @returns IB_OK if successful.
 */
static ib_status_t act_event_execute(
    const ib_rule_exec_t *rule_exec,
    void *data,
    void *cbdata)
{
    assert(rule_exec != NULL);
    assert(data != NULL);

    ib_status_t  rc;
    ib_logevent_t *event;
    const char *expanded;
    size_t expanded_size;
    const ib_field_t *field;
    const ib_rule_t *rule = rule_exec->rule;
    ib_tx_t *tx = rule_exec->tx;
    const event_data_t *event_data = (const event_data_t *)data;
    ib_core_cfg_t *corecfg;

    ib_rule_log_debug(rule_exec, "Creating event via action");

    rc = ib_core_context_config(ib_context_main(rule_exec->ib), &corecfg);
    if (rc != IB_OK) {
        ib_rule_log_error(rule_exec,
                          "event: Failed to fetch configuration.: %s",
                          ib_status_to_string(rc));
        return rc;
    }

    /* Expand the message string */
    if (rule->meta.msg != NULL) {
        rc = ib_var_expand_execute(
            rule->meta.msg,
            &expanded, &expanded_size,
            tx->mm,
            tx->var_store
        );
        if (rc != IB_OK) {
            ib_rule_log_error(rule_exec,
                              "event: Failed to expand message: %s",
                              ib_status_to_string(rc));
            return rc;
        }
    }
    else {
        expanded = "";
        expanded_size = 0;
    }

    /* Create the event */
    rc = ib_logevent_create(
        &event,
        tx->mm,
        ib_rule_id(rule),
        event_data->event_type,
        IB_LEVENT_ACTION_UNKNOWN,
        rule->meta.confidence,
        rule->meta.severity,
        "%.*s",
        (int)expanded_size, expanded
    );
    if (rc != IB_OK) {
        return rc;
    }

    /* Set the data */
    if (rule->meta.data != NULL) {
        rc = ib_var_expand_execute(
            rule->meta.data,
            &expanded, &expanded_size,
            tx->mm,
            tx->var_store
        );
        if (rc != IB_OK) {
            ib_rule_log_error(rule_exec,
                              "event: Failed to expand logdata: %s",
                              ib_status_to_string(rc));
            return rc;
        }

        rc = ib_logevent_data_set(event, expanded, expanded_size);
        if (rc != IB_OK) {
            ib_rule_log_error(rule_exec, "event: Failed to set data: %s",
                              ib_status_to_string(rc));
            return rc;
        }
    }

    /* Create the destination tag list. */
    rc = ib_list_create(&(event->tags), tx->mm);
    if (rc != IB_OK) {
        return rc;
    }

    /* Copy each tag to the new event tag list.
     * If the tag may be expanded using the ib_tx_t's var_store, it is expanded.
     */
    for (
        const ib_list_node_t *tag_node = ib_list_first_const(rule->meta.tags);
        tag_node != NULL;
        tag_node = ib_list_node_next_const(tag_node)
    )
    {
        const char *tag = (const char *)ib_list_node_data_const(tag_node);
        assert(tag != NULL);

        /* If the tag can be expanded, expand it an assign the result to `tag`. */
        if (ib_var_expand_test(IB_S2SL(tag))) {
            const char      *expanded_tag    = NULL;
            size_t           expanded_tag_sz = 0;
            ib_var_expand_t *expand          = NULL;

            /* Get an expansion for the tag. */
            rc = ib_var_expand_acquire(
                &expand,
                tx->mm,
                IB_S2SL(tag),
                ib_engine_var_config_get(rule_exec->ib));
            if (rc != IB_OK) {
                ib_rule_log_error(
                    rule_exec,
                    "event: Failed acquire tag expansion for %s: %s",
                    tag,
                    ib_status_to_string(rc));
                return rc;
            }

            /* Expand the tag. */
            rc = ib_var_expand_execute(
                expand,
                &expanded_tag,
                &expanded_tag_sz,
                tx->mm,
                tx->var_store
            );

            /* If the expanded tag size is too long, truncate it with an elipses. */
            if (expanded_tag_sz > 64) {
                strcpy((char *)(expanded_tag + 61), "...");
                expanded_tag_sz = 64;
            }

            /* Null terminates the expanded string. */
            tag = ib_mm_memdup_to_str(tx->mm, expanded_tag, expanded_tag_sz);
            if (tag == NULL) {
                return IB_EALLOC;
            }
        }

        rc = ib_list_push(event->tags, (void *)tag);
        if (rc != IB_OK) {
            return rc;
        }
    }

    /* Populate fields */
    if (! ib_flags_any(rule->flags, IB_RULE_FLAG_NO_TGT)) {
        rc = ib_var_source_get_const(
            corecfg->vars->field_name_full,
            &field,
            tx->var_store
        );
        if ( (rc == IB_OK) && (field->type == IB_FTYPE_NULSTR) ) {
            const char *name = NULL;
            rc = ib_field_value(field, ib_ftype_nulstr_out(&name));
            if (rc == IB_OK) {
                ib_logevent_field_add(event, name);
            }
        }
        else if ( (rc == IB_OK) && (field->type == IB_FTYPE_BYTESTR) ) {
            const ib_bytestr_t *bs;
            rc = ib_field_value(field, ib_ftype_bytestr_out(&bs));
            if (rc == IB_OK) {
                ib_logevent_field_add_ex(
                    event,
                    (const char *)ib_bytestr_const_ptr(bs),
                    ib_bytestr_length(bs)
                );
            }
        }
    }

    /* Set the actions if appropriate */
    if (ib_flags_all(tx->flags,
                     (IB_TX_FBLOCK_ADVISORY |
                      IB_TX_FBLOCK_PHASE |
                      IB_TX_FBLOCK_IMMEDIATE)) )
    {
        event->rec_action = IB_LEVENT_ACTION_BLOCK;
    }

    /* Log the event. */
    rc = ib_logevent_add(tx, event);
    if (rc != IB_OK) {
        return rc;
    }

    /* Add the event to the rule execution */
    rc = ib_rule_log_exec_add_event(rule_exec->exec_log, event);
    if (rc != IB_OK) {
        /* todo: Ignore this? */
    }

    return IB_OK;
}

/**
 * Create function for the setvar action.
 *
 * @param[in]  mm            Memory manager.
 * @param[in]  ctx           Context.
 * @param[in]  parameters    Parameters
 * @param[out] instance_data Instance data to pass to execute.
 * @param[in]  cbdata        Callback data.
 *
 * @returns Status code
 */
static ib_status_t act_setvar_create(
    ib_mm_t       mm,
    ib_context_t *ctx,
    const char   *parameters,
    void         *instance_data,
    void         *cbdata
)
{
    size_t nlen;                 /* Name length. */
    const char *eq;              /* '=' character in @a parameters. */
    const char *mod;             /* '+'/'-'/'*' character in parameters. */
    const char *value;           /* Value in parameters. */
    setvar_data_t *setvar_data;  /* Data for the execute function. */
    ib_status_t rc;              /* Status code. */
    ib_list_t   *tfns;           /* List of transformations. */
    ib_engine_t *ib = ctx->ib;

    /* Argument variable. */
    union {
        ib_num_t         num;
        ib_float_t       flt;
        ib_var_expand_t *var_expand; /**< Stored as an IB_FTYPE_GENERIC. */
    } arg_u;

    if (parameters == NULL) {
        return IB_EINVAL;
    }

    /* Simple checks; parameters should look like '<name>=[<value>]' */
    eq = strchr(parameters, '=');
    if ( (eq == NULL) || (eq == parameters) ) {
        return IB_EINVAL;
    }

    /* Calculate name length */
    if (*(eq-1) == '*' || *(eq-1) == '-' || *(eq-1) == '+') {
        mod = eq - 1;
        nlen = (mod - parameters);
    }
    else {
        mod = NULL;
        nlen = (eq - parameters);
    }

    /* Create the setvar_data structure for the execute function */
    setvar_data = ib_mm_alloc(mm, sizeof(*setvar_data));
    if (setvar_data == NULL) {
        return IB_EALLOC;
    }
    setvar_data->target_str = ib_mm_memdup(mm, parameters, nlen);
    setvar_data->target_str_len = nlen;
    setvar_data->transformations = NULL;

    rc = ib_list_create(&tfns, mm);
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_cfg_parse_target_string(
        mm,
        (eq + 1),
        &value,
        tfns);
    if (rc != IB_OK) {
        return rc;
    }

    /* If there are transformation fields, convert them to ib_transformation_inst_t and
     * add them to the setvar action. */
    if (ib_list_elements(tfns) > 0) {
        rc = ib_list_create(&(setvar_data->transformations), mm);
        if (rc != IB_OK) {
            return rc;
        }

        rc = ib_rule_tfn_fields_to_inst(
            ib,
            mm,
            tfns,
            setvar_data->transformations);
        if (rc != IB_OK) {
            return rc;
        }
    }

    /* Construct target. */
    // @todo Record error_message and error_offset and do something with them.
    rc = ib_var_target_acquire_from_string(
        &(setvar_data->target),
        mm,
        ib_engine_var_config_get(ib),
        parameters, nlen
    );
    if (rc != IB_OK) {
        return rc;
    }

    /* Create the value */
    rc = ib_string_to_num_ex(IB_S2SL(value), 0, &(arg_u.num));
    if (rc == IB_OK) {
        rc = ib_field_create(
            &(setvar_data->argument),
            mm,
            "", 0,
            IB_FTYPE_NUM,
            ib_ftype_num_in(&arg_u.num));
        if (rc != IB_OK) {
            return rc;
        }

        if (mod == NULL) {
            setvar_data->op = SETVAR_NUMSET;
        }
        else if (*mod == '+') {
            setvar_data->op = SETVAR_NUMADD;
        }
        else if (*mod == '-') {
            setvar_data->op = SETVAR_NUMSUB;
        }
        else if (*mod == '*') {
            setvar_data->op = SETVAR_NUMMULT;
        }

        goto success;
    }

    rc = ib_string_to_float(value, &(arg_u.flt));
    if (rc == IB_OK) {
        rc = ib_field_create(
            &(setvar_data->argument),
            mm,
            "", 0,
            IB_FTYPE_FLOAT,
            ib_ftype_float_in(&arg_u.flt));
        if (rc != IB_OK) {
            return rc;
        }

        if (mod == NULL) {
            setvar_data->op = SETVAR_FLOATSET;
        }
        else if (*mod == '+') {
            setvar_data->op = SETVAR_FLOATADD;
        }
        else if (*mod == '-') {
            setvar_data->op = SETVAR_FLOATSUB;
        }
        else if (*mod == '*') {
            setvar_data->op = SETVAR_FLOATMULT;
        }

        goto success;
    }

    /* Argument is not a number. Generically handle this. */
    else {
        if (mod != NULL) {
            ib_log_error(
                ib,
                "setvar: Numeric '%c' not supported for strings",
                *mod);
            return IB_EINVAL;
        }

        rc = ib_var_expand_acquire(
            &(arg_u.var_expand),
            mm,
            IB_S2SL(value),
            ib_engine_var_config_get(ib)
        );

        if (rc != IB_OK) {
            if (rc == IB_EINVAL) {
                ib_log_error(
                    ib,
                    "setvar: Error pre-constructing value."
                );
            }
            return rc;
        }

        rc = ib_field_create(
            &(setvar_data->argument),
            mm,
            "", 0,
            IB_FTYPE_GENERIC,
            ib_ftype_generic_in(arg_u.var_expand));
        if (rc != IB_OK) {
            return rc;
        }
        setvar_data->op = SETVAR_STRSET;
    }

success:
    *(void **)instance_data = setvar_data;
    return IB_OK;
}

/**
 * Execute function for the "set variable" action
 *
 * @param[in] rule_exec The rule execution object
 * @param[in] data Setvar data (setvar_data_t *)
 * @param[in] cbdata Unused.
 *
 * @returns Status code
 *    - IB_OK The operator succeeded.
 *    - IB_EINVAL if there was an internal error during expansion.
 *    - IB_EALLOC on memory errors.
 */
static ib_status_t act_setvar_execute(
    const ib_rule_exec_t *rule_exec,
    void *data,
    void *cbdata)
{
    assert(data != NULL);
    assert(rule_exec != NULL);
    assert(rule_exec->tx != NULL);
    assert(rule_exec->tx->ib != NULL);

    ib_status_t       rc;
    ib_field_t       *cur_field = NULL;
    ib_tx_t          *tx        = rule_exec->tx;
    ib_mm_t           mm        = tx->mm;
    const ib_list_t  *result    = NULL; /* List of target fields: ib_field_t* */
    const ib_field_t *argument;
    ib_var_target_t  *expanded_target;

    /* Data should be a setvar_data_t created in our create function */
    const setvar_data_t *setvar_data = (const setvar_data_t *)data;
    const char          *ts          = setvar_data->target_str;
    int                  tslen       = (int)setvar_data->target_str_len;

    /* Expand target. */
    rc = ib_var_target_expand(
        setvar_data->target,
        &expanded_target,
        mm,
        tx->var_store
    );
    if (rc != IB_OK) {
        ib_rule_log_error(
            rule_exec,
            "setvar %.*s: Failed to expand value: %s",
            tslen, ts,
            ib_status_to_string(rc)
        );
        return rc;
    }

    /* Remove target, recording results. */
    rc = ib_var_target_get(
        expanded_target,
        &result,
        mm,
        tx->var_store
    );
    if (rc == IB_OK) {
        if (result != NULL && ib_list_elements(result) > 0) {
            cur_field = (ib_field_t *)ib_list_node_data_const(
                ib_list_first_const(result));
        }
    }
    else if (rc == IB_ENOENT) {
        cur_field = NULL;
    }
    else {
        ib_rule_log_error(
            rule_exec,
            "setvar %.*s: Failed to get value: %s",
            tslen, ts,
            ib_status_to_string(rc)
        );
        return rc;
    }


    /* If the argument is a IB_FTYPE_GENERIC type, then it must be expanded. */
    if (setvar_data->argument->type == IB_FTYPE_GENERIC) {
        ib_var_expand_t *arg_expand;
        const char      *arg_name;
        size_t           arg_name_len;
        ib_bytestr_t    *arg_bs;
        ib_field_t      *arg_tmp;

        rc = ib_field_value(
            setvar_data->argument,
            ib_ftype_generic_mutable_out(&arg_expand));
        if (rc != IB_OK) {
            return rc;
        }

        rc = ib_var_expand_execute(
            arg_expand,
            &arg_name,
            &arg_name_len,
            mm,
            tx->var_store);
        if (rc != IB_OK) {
            return rc;
        }

        rc = ib_bytestr_alias_mem(
            &arg_bs,
            mm,
            (const uint8_t *)arg_name,
            arg_name_len);
        if (rc != IB_OK) {
            return rc;
        }

        rc = ib_field_create(
            &arg_tmp,
            mm,
            IB_S2SL(""),
            IB_FTYPE_BYTESTR,
            ib_ftype_bytestr_in(arg_bs));
        if (rc != IB_OK) {
            return rc;
        }

        argument = arg_tmp;
    }
    /* Pull out the argument to setvar. */
    else {
        argument = setvar_data->argument;
    }

    if (setvar_data->transformations != NULL &&
        ib_list_elements(setvar_data->transformations) > 0)
    {
        const ib_list_node_t *tfn_node;

        IB_LIST_LOOP_CONST(setvar_data->transformations, tfn_node) {
            const ib_field_t    *tmp_field;
            const ib_transformation_inst_t *tfn_inst =
                (const ib_transformation_inst_t *)ib_list_node_data_const(tfn_node);

            rc = ib_transformation_inst_execute(tfn_inst, mm, argument, &tmp_field);
            if (rc != IB_OK) {
                return rc;
            }

            /* Promote the temporary field to the new current field. */
            argument = tmp_field;
        }
    }

    /* At this point cur_field represents the old value (possibly) NULL.
     * Depending on the operation and value, we now need to modify it to
     * represent the new value and then we'll set it back. */

    switch(setvar_data->op) {

    case SETVAR_STRSET:
    {
        rc = ib_field_copy(
            &cur_field,
            mm,
            setvar_data->target_str,
            setvar_data->target_str_len,
            argument);
        if (rc != IB_OK) {
            return rc;
        }
        break;
    }

    case SETVAR_FLOATSET:
    {
        ib_float_t flt;

        rc = ib_field_value_type(
            argument,
            ib_ftype_float_out(&flt),
            IB_FTYPE_FLOAT);
        if (rc != IB_OK) {
            return rc;
        }

        /* Try to re-use the existing field */
        if (cur_field != NULL && cur_field->type == IB_FTYPE_FLOAT) {
            ib_field_setv(cur_field, ib_ftype_float_in(&flt));
        }
        else {
            rc = ib_field_create(
                &cur_field,
                tx->mm,
                "", 0,
                IB_FTYPE_FLOAT,
                ib_ftype_float_in(&flt));
            if (rc != IB_OK) {
                ib_rule_log_error(
                    rule_exec,
                    "setvar %.*s: Failed to create field: %s",
                    tslen, ts,
                    ib_status_to_string(rc)
                );
                return rc;
            }
        }
        break;
    }

    case SETVAR_NUMSET:
    {
        ib_num_t num;

        rc = ib_field_value_type(
            argument,
            ib_ftype_num_out(&num),
            IB_FTYPE_NUM);
        if (rc != IB_OK) {
            return rc;
        }

        /* Try to re-use the existing field */
        if (cur_field != NULL && cur_field->type == IB_FTYPE_NUM) {
            ib_field_setv(cur_field, ib_ftype_num_in(&num));
            return IB_OK;
        }
        else {
            rc = ib_field_create(
                &cur_field,
                tx->mm,
                "", 0,
                IB_FTYPE_NUM,
                ib_ftype_num_in(&num));
            if (rc != IB_OK) {
                ib_rule_log_error(
                    rule_exec,
                    "setvar %.*s: Failed to create field: %s",
                    tslen, ts,
                    ib_status_to_string(rc)
                );
                return rc;
            }
        }

        break;
    }
    /* Numerical operation : Add */
    case SETVAR_FLOATADD:
        rc = setvar_float_op(
            tx,
            rule_exec,
            argument,
            &setvar_float_add_op,
            &cur_field);
        break;

    /* Numerical operation : Sub */
    case SETVAR_FLOATSUB:
        rc = setvar_float_op(
            tx,
            rule_exec,
            argument,
            &setvar_float_sub_op,
            &cur_field);
        break;

    /* Numerical operation : Mult */
    case SETVAR_FLOATMULT:
        rc = setvar_float_op(
            tx,
            rule_exec,
            argument,
            &setvar_float_mult_op,
            &cur_field);
        break;

    /* Numerical operation : Add */
    case SETVAR_NUMADD:
        rc = setvar_num_op(
            tx,
            rule_exec,
            argument,
            &setvar_num_add_op,
            &cur_field);
        break;

    /* Numerical operation : Sub */
    case SETVAR_NUMSUB:
        rc = setvar_num_op(
            tx,
            rule_exec,
            argument,
            &setvar_num_sub_op,
            &cur_field);
        break;

    /* Numerical operation : Mult */
    case SETVAR_NUMMULT:
        rc = setvar_num_op(
            tx,
            rule_exec,
            argument,
            &setvar_num_mult_op,
            &cur_field);
        break;
    }

    if (rc != IB_OK) {
        ib_rule_log_error(
            rule_exec,
            "setvar %.*s: Failed operate on field: %s",
            tslen, ts,
            ib_status_to_string(rc)
        );
        return rc;
    }

    assert(cur_field != NULL);

    rc = ib_var_target_remove_and_set(expanded_target, tx->mm, tx->var_store, cur_field);
    if (rc != IB_OK) {
        ib_rule_log_error(
            rule_exec,
            "setvar %.*s: Failed to set field: %s",
            tslen, ts,
            ib_status_to_string(rc)
        );
        return rc;
    }

    return IB_OK;
}

/**
 * Find event from this rule
 *
 * @param[in] rule_exec The rule execution object
 * @param[out] event Matching event
 *
 * @return
 *   - IB_OK (if found)
 *   - IB_ENOENT if not found
 *   - Errors returned by ib_logevent_get_all()
 */
static ib_status_t get_event(const ib_rule_exec_t *rule_exec,
                             ib_logevent_t **event)
{
    assert(rule_exec != NULL);

    ib_status_t rc;
    ib_list_t *event_list;
    ib_list_node_t *event_node;
    ib_tx_t *tx = rule_exec->tx;

    rc = ib_logevent_get_all(tx, &event_list);
    if (rc != IB_OK) {
        return rc;
    }
    event_node = ib_list_last(event_list);
    if (event_node == NULL) {
        return IB_ENOENT;
    }
    ib_logevent_t *e = (ib_logevent_t *)event_node->data;
    if (strcmp(e->rule_id, ib_rule_id(rule_exec->rule)) == 0) {
        *event = e;
        return IB_OK;
    }

    return IB_ENOENT;
}

/**
 * Set the IB_TX_FBLOCK_ADVISORY flag and set the DPI value @c FLAGS:BLOCK=1.
 *
 * @param[in] rule_exec The rule execution object
 *
 * @return
 *   - IB_OK on success.
 *   - Other if an event exists, but cannot be retrieved for this action.
 */
static ib_status_t act_block_advisory_execute(
    const ib_rule_exec_t *rule_exec)
{
    assert(rule_exec != NULL);

    ib_tx_t *tx = rule_exec->tx;
    ib_status_t rc;
    ib_logevent_t *event;
    ib_core_cfg_t *corecfg;

    rc = ib_core_context_config(ib_context_main(rule_exec->ib), &corecfg);
    if (rc != IB_OK) {
        ib_rule_log_error(rule_exec,
                          "block: Failed to fetch configuration.: %s",
                          ib_status_to_string(rc));
        return rc;
    }

    /* Don't re-set the flag because it bloats the DPI value FLAGS
     * with lots of BLOCK entries. */
    if (!ib_flags_all(tx->flags, IB_TX_FBLOCK_ADVISORY)) {
        /* Set the flag in the transaction. */
        ib_tx_flags_set(tx, IB_TX_FBLOCK_ADVISORY);

        /* Update the event (if required) */
        rc = get_event(rule_exec, &event);
        if (rc == IB_OK) {
            event->rec_action = IB_LEVENT_ACTION_BLOCK;
        }
        else if (rc != IB_ENOENT) {
            ib_rule_log_error(rule_exec,
                              "Failed to fetch event "
                              "associated with this action: %s",
                              ib_status_to_string(rc));
            return rc;
        }
    }

    ib_rule_log_debug(rule_exec, "Advisory block.");

    return IB_OK;
}

/**
 * Set the IB_TX_FBLOCK_PHASE flag in the tx.
 *
 * @param[in] rule_exec The rule execution object
 *
 * @return
 *   - IB_OK on success.
 *   - Other if an event exists, but cannot be retrieved for this action.
 */
static ib_status_t act_block_phase_execute(
    const ib_rule_exec_t *rule_exec)
{
    ib_status_t rc;
    ib_logevent_t *event;
    ib_tx_t *tx = rule_exec->tx;

    ib_tx_flags_set(tx, IB_TX_FBLOCK_ADVISORY|IB_TX_FBLOCK_PHASE);

    /* Update the event (if required) */
    rc = get_event(rule_exec, &event);
    if (rc == IB_OK) {
        event->rec_action = IB_LEVENT_ACTION_BLOCK;
    }
    else if (rc != IB_ENOENT) {
        ib_rule_log_error(rule_exec,
                          "Failed phase block: %s.", ib_status_to_string(rc));
        return rc;
    }

    ib_rule_log_trace(rule_exec, "Phase block.");

    return IB_OK;
}

/**
 * Set the IB_TX_FBLOCK_IMMEDIATE flag in the tx.
 *
 * @param[in] rule_exec The rule execution object
 *
 * @returns
 *   - IB_OK on success.
 *   - Other if an event exists, but cannot be retrieved for this action.
 */
static ib_status_t act_block_immediate_execute(
    const ib_rule_exec_t *rule_exec)
{
    assert(rule_exec != NULL);

    ib_status_t rc;
    ib_logevent_t *event;

    ib_tx_flags_set(rule_exec->tx, IB_TX_FBLOCK_ADVISORY|IB_TX_FBLOCK_IMMEDIATE);

    /* Update the event (if required) */
    rc = get_event(rule_exec, &event);
    if (rc == IB_OK) {
        event->rec_action = IB_LEVENT_ACTION_BLOCK;
    }
    else if (rc != IB_ENOENT) {
        ib_rule_log_error(rule_exec,
                          "Failed immediate block: %s.",
                          ib_status_to_string(rc));
        return rc;
    }

    ib_rule_log_debug(rule_exec, "Immediate block.");

    return IB_OK;
}

/**
 * The function that implements flagging a particular block type.
 *
 * @param[in] rule_exec The rule execution object
 *
 * @return Return code
 */
typedef ib_status_t(*act_block_execution_t)(
    const ib_rule_exec_t *rule_exec
);

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
 * Executes the function stored in @a data.
 *
 * @param[in] rule_exec The rule execution object
 * @param[in] data Cast to an @c act_block_t and the @c execute field is
 *            called on the given @a tx.
 * @param[in] cbdata Callback data. Unused.
 */
static ib_status_t act_block_execute(
    const ib_rule_exec_t *rule_exec,
    void *data,
    void *cbdata)
{
    assert(rule_exec);
    assert(data);

    ib_status_t rc = ((const act_block_t *)data)->execute(rule_exec);

    return rc;
}

/**
 * Create / initialize a new instance of an action.
 *
 * @param[in]  mm            Memory manager.
 * @param[in]  ctx           Context.
 * @param[in]  parameters    Parameters.  These may be "immediate", "phase",
 *                           or "advise". If null, "advisory" is assumed.
 *                           These select the type of block that will be put
 *                           in place by deciding which callback
 *                           (act_block_phase_execute(),
 *                           act_block_immediate_execute(), or
 *                           act_block_advise_execute()) is assigned to the
 *                           rule data object.
 * @param[out] instance_data Instance data to pass to execute.
 * @param[in]  cbdata        Callback data.
 *
 * @return IB_OK on success or IB_EALLOC if the callback data
 *         cannot be initialized for the rule.
 */
static ib_status_t act_block_create(
    ib_mm_t       mm,
    ib_context_t *ctx,
    const char   *parameters,
    void         *instance_data,
    void         *cbdata
)
{
    assert(ctx != NULL);
    assert(instance_data != NULL);
    act_block_t *act_block;

    act_block = (act_block_t *)ib_mm_alloc(mm, sizeof(*act_block));
    if ( act_block == NULL ) {
        return IB_EALLOC;
    }

    /* When parameters are NULL, use advisory blocking by default. */
    if ( parameters == NULL ) {
        act_block->execute = &act_block_advisory_execute;
    }

    /* Just note that a block should be done, according to this rule. */
    else if ( ! strcasecmp("advisory", parameters) ) {
        act_block->execute = &act_block_advisory_execute;
    }

    /* Block at the end of the phase. */
    else if ( ! strcasecmp("phase", parameters) ) {
        act_block->execute = &act_block_phase_execute;
    }

    /* Immediate blocking. Block ASAP. */
    else if ( ! strcasecmp("immediate", parameters) ) {
        act_block->execute = &act_block_immediate_execute;
    }

    /* As with parameters == NULL, the default is to use an advisory block. */
    else {
        act_block->execute = &act_block_advisory_execute;
    }

    /* Assign the built up context object. */
    *(void **)instance_data = act_block;

    return IB_OK;
}

/**
 * Holds the name of the header and the value to set/append/merge.
 */
struct act_header_data_t {
    const ib_var_expand_t *name;  /**< Name. */
    const ib_var_expand_t *value; /**< Value */
};
typedef struct act_header_data_t act_header_data_t;

/**
 * Common create routine for delResponseHeader and delRequestHeader action.
 *
 * @param[in]  mm            Memory manager.
 * @param[in]  ctx           Context.
 * @param[in]  parameters    Parameters: name=&lt;header name&gt;.
 * @param[out] instance_data Instance data to pass to execute.
 * @param[in]  cbdata        Callback data.
 *
 * @return IB_OK on success. IB_EALLOC if a memory allocation fails.
 */
static ib_status_t act_del_header_create(
    ib_mm_t       mm,
    ib_context_t *ctx,
    const char   *parameters,
    void         *instance_data,
    void         *cbdata
)
{
    assert(ctx != NULL);
    assert(parameters != NULL);
    assert(instance_data != NULL);

    ib_engine_t *ib = ctx->ib;
    act_header_data_t *act_data;
    ib_status_t rc;
    ib_var_expand_t *expand;

    act_data = (act_header_data_t *)ib_mm_calloc(mm, 1, sizeof(*act_data));

    if (act_data == NULL) {
        return IB_EALLOC;
    }

    if ( (parameters == NULL) || (strlen(parameters) == 0) ) {
        ib_log_error(ib, "Delete header action requires a parameter.");
        return IB_EINVAL;
    }

    rc = ib_var_expand_acquire(
        &expand,
        mm,
        IB_S2SL(parameters),
        ib_engine_var_config_get(ib)
    );
    if (rc != IB_OK) {
        ib_log_error(ib,
            "Error parsing name %s: %s",
            parameters,
            ib_status_to_string(rc)
        );
        return rc;
    }

    act_data->name = expand;
    *(void **)instance_data = act_data;

    return IB_OK;
}

/**
 * Common create routine for setResponseHeader and setRequestHeader actions.
 *
 * @param[in]  mm            Memory manager.
 * @param[in]  ctx           Context.
 * @param[in]  parameters    Parameters: name=&gt;header name&lt;.
 * @param[out] instance_data Instance data to pass to execute.
 * @param[in]  cbdata        Callback data.
 *
 * @return IB_OK on success. IB_EALLOC if a memory allocation fails.
 */
static ib_status_t act_set_header_create(
    ib_mm_t       mm,
    ib_context_t *ctx,
    const char   *parameters,
    void         *instance_data,
    void         *cbdata
)
{
    assert(ctx != NULL);
    assert(parameters != NULL);
    assert(instance_data != NULL);

    ib_engine_t *ib = ctx->ib;
    size_t name_len;
    size_t value_len;
    size_t parameters_len;
    char *equals_idx;
    act_header_data_t *act_data;
    size_t value_offs = 1;
    const char *value;
    ib_var_expand_t *expand;
    ib_status_t rc;

    act_data = (act_header_data_t *)ib_mm_calloc(mm, 1, sizeof(*act_data));

    if (act_data == NULL) {
        return IB_EALLOC;
    }


    if ( (parameters == NULL) || (strlen(parameters) == 0) ) {
        ib_log_error(ib, "Set header requires a parameter.");
        return IB_EINVAL;
    }

    equals_idx = index(parameters, '=');

    /* If the returned value was NULL it is an error. */
    if (equals_idx == NULL) {
        ib_log_error(ib, "Set header parameter format is name=value: %s", parameters);
        return IB_EINVAL;
    }

    /* Compute string lengths needed for parsing out name and value. */
    parameters_len = strlen(parameters);
    name_len = equals_idx - parameters;
    value_len = parameters_len - name_len - value_offs;

    rc = ib_var_expand_acquire(
        &expand,
        mm,
        parameters, name_len,
        ib_engine_var_config_get(ib)
    );
    if (rc != IB_OK) {
        ib_log_error(ib,
            "Error parsing name %.*s: %s",
            (int)name_len, parameters,
            ib_status_to_string(rc)
        );
        return rc;
    }

    act_data->name = expand;

    if (value_len == 0) {
        value = "";
    }
    else {
        value = equals_idx + value_offs;
    }

    rc = ib_var_expand_acquire(
        &expand,
        mm,
        value, value_len,
        ib_engine_var_config_get(ib)
    );
    if (rc != IB_OK) {
        ib_log_error(ib,
            "Error parsing value %.*s: %s",
            (int)value_len, value,
            ib_status_to_string(rc)
        );
        return rc;
    }
    act_data->value = expand;

    *(void **)instance_data = act_data;
    return IB_OK;
}

/**
 * Set the request header in @c tx->data.
 *
 * @param[in] rule_exec The rule execution object
 * @param[in] data Instance data needed for execution.
 * @param[in] cbdata Unused.
 *
 * @returns IB_OK if successful.
 */
static ib_status_t act_set_request_header_execute(
    const ib_rule_exec_t *rule_exec,
    void *data,
    void *cbdata)
{
    assert(rule_exec);
    assert(rule_exec->tx);
    assert(rule_exec->ib);
    assert(ib_engine_server_get(rule_exec->ib));
    assert(data);

    ib_status_t rc;
    act_header_data_t *act_data = (act_header_data_t *)data;
    const char *value;
    size_t value_len;
    const char *name;
    size_t name_len;
    ib_tx_t *tx = rule_exec->tx;

    rc = ib_var_expand_execute(
        act_data->name,
        &name,
        &name_len,
        tx->mm,
        tx->var_store
    );
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_var_expand_execute(
        act_data->value,
        &value,
        &value_len,
        tx->mm,
        tx->var_store
    );
    if (rc != IB_OK) {
        return rc;
    }

    ib_rule_log_debug(rule_exec, "Setting request header \"%.*s\"=\"%.*s\"",
                      (int)name_len, name, (int)value_len, value);

    rc = ib_server_header(ib_engine_server_get(rule_exec->ib), tx,
                          IB_SERVER_REQUEST, IB_HDR_SET,
                          name, name_len, value, value_len);

    return rc;
}

/**
 * @param[in] rule_exec The rule execution object
 * @param[in] data Instance data needed for execution.
 * @param[in] cbdata Unused.
 *
 * @returns IB_OK if successful.
 */
static ib_status_t act_del_request_header_execute(
    const ib_rule_exec_t *rule_exec,
    void *data,
    void *cbdata)
{
    assert(rule_exec);
    assert(rule_exec->tx);
    assert(rule_exec->ib);
    assert(ib_engine_server_get(rule_exec->ib));
    assert(data);

    ib_status_t rc;
    act_header_data_t *act_data = (act_header_data_t *)data;
    const char *name;
    size_t name_len;

    /* Expand the name (if required) */
    rc = ib_var_expand_execute(
        act_data->name,
        &name,
        &name_len,
        rule_exec->tx->mm,
        rule_exec->tx->var_store
    );
    if (rc != IB_OK) {
        return rc;
    }

    ib_rule_log_debug(rule_exec, "Deleting request header \"%.*s\"",
                      (int)name_len, name);

    rc = ib_server_header(ib_engine_server_get(rule_exec->ib),
                          rule_exec->tx,
                          IB_SERVER_REQUEST,
                          IB_HDR_UNSET,
                          name, name_len,
                          "", 0);

    return rc;
}

/**
 * @param[in] rule_exec The rule execution object
 * @param[in] data Instance data needed for execution.
 * @param[in] cbdata Unused.
 *
 * @returns IB_OK if successful.
 */
static ib_status_t act_set_response_header_execute(
    const ib_rule_exec_t *rule_exec,
    void *data,
    void *cbdata)
{
    assert(rule_exec);
    assert(rule_exec->tx);
    assert(rule_exec->ib);
    assert(ib_engine_server_get(rule_exec->ib));
    assert(data);

    ib_status_t rc;
    act_header_data_t *act_data = (act_header_data_t *)data;
    const char *value;
    size_t value_len;
    const char *name;
    size_t name_len;
    ib_tx_t *tx = rule_exec->tx;

    /* Expand the name (if required) */
    rc = ib_var_expand_execute(
        act_data->name,
        &name,
        &name_len,
        tx->mm,
        tx->var_store
    );
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_var_expand_execute(
        act_data->value,
        &value,
        &value_len,
        tx->mm,
        tx->var_store
    );
    if (rc != IB_OK) {
        return rc;
    }

    ib_rule_log_debug(rule_exec, "Setting response header \"%.*s\"=\"%.*s\"",
                      (int)name_len, name, (int)value_len, value);

    rc = ib_server_header(ib_engine_server_get(tx->ib), tx,
                          IB_SERVER_RESPONSE, IB_HDR_SET,
                          name, name_len, value, value_len);

    return rc;
}

/**
 *
 * @param[in] rule_exec The rule execution object
 * @param[in] data Instance data needed for execution.
 * @param[in] cbdata Unused.
 *
 * @returns IB_OK if successful.
 */
static ib_status_t act_del_response_header_execute(
    const ib_rule_exec_t *rule_exec,
    void *data,
    void *cbdata)
{
    assert(rule_exec);
    assert(rule_exec->tx);
    assert(rule_exec->ib);
    assert(ib_engine_server_get(rule_exec->ib));
    assert(data);

    ib_status_t rc;
    act_header_data_t *act_data = (act_header_data_t *)data;
    const char *name;
    size_t name_len;

    /* Expand the name (if required) */
    rc = ib_var_expand_execute(
        act_data->name,
        &name,
        &name_len,
        rule_exec->tx->mm,
        rule_exec->tx->var_store
    );
    if (rc != IB_OK) {
        return rc;
    }

    ib_rule_log_debug(rule_exec, "Deleting response header \"%.*s\"",
                      (int)name_len, name);

    /* Note: ignores lengths for now */
    rc = ib_server_header(ib_engine_server_get(rule_exec->ib),
                          rule_exec->tx,
                          IB_SERVER_RESPONSE,
                          IB_HDR_UNSET,
                          name, name_len,
                          "", 0);

    return rc;
}

/**
 * Create function for the allow action.
 *
 * @param[in]  mm            Memory manager.
 * @param[in]  ctx           Context.
 * @param[in]  parameters    Parameters
 * @param[out] instance_data Instance data to pass to execute.
 * @param[in]  cbdata        Callback data.
 *
 * @returns Status code
 */
static ib_status_t act_allow_create(
    ib_mm_t       mm,
    ib_context_t *ctx,
    const char   *parameters,
    void         *instance_data,
    void         *cbdata
)
{
    ib_flags_t flags = IB_TX_FNONE;
    ib_flags_t *idata;

    if (parameters == NULL) {
        flags |= IB_TX_FALLOW_ALL;
    }
    else if (strcasecmp(parameters, "phase") == 0) {
        flags |= IB_TX_FALLOW_PHASE;
    }
    else if (strcasecmp(parameters, "request") == 0) {
        flags |= IB_TX_FALLOW_REQUEST;
    }
    else {
        return IB_EINVAL;
    }

    idata = ib_mm_alloc(mm, sizeof(*idata));
    if (idata == NULL) {
        return IB_EALLOC;
    }

    *idata = flags;
    *(void **)instance_data = idata;

    return IB_OK;
}

/**
 * Allow action.
 *
 * @param[in] rule_exec The rule execution object
 * @param[in] data Not used.
 * @param[in] cbdata Unused.
 */
static ib_status_t act_allow_execute(
    const ib_rule_exec_t *rule_exec,
    void *data,
    void *cbdata)
{
    assert(data != NULL);
    assert(rule_exec != NULL);
    assert(rule_exec->tx != NULL);

    const ib_flags_t *pflags = (const ib_flags_t *)data;
    ib_flags_t set_flags = *pflags;

    /* For post process, treat ALLOW_ALL like ALLOW_PHASE */
    if ( (rule_exec->rule->meta.phase == IB_PHASE_POSTPROCESS) &&
         (ib_flags_all(set_flags, IB_TX_FALLOW_ALL)) )
    {
        set_flags |= IB_TX_FALLOW_PHASE;
    }

    /* Set the flags in the TX */
    ib_tx_flags_set(rule_exec->tx, set_flags);

    return IB_OK;
}

/**
 * Audit log parts action data
 */
typedef struct {
    const ib_list_t   *oplist;   /**< Flags operation list */
} act_auditlog_parts_t;

/**
 * Create function for the auditLogParts action
 *
 * @param[in]  mm            Memory manager.
 * @param[in]  ctx           Context.
 * @param[in]  parameters    Parameters
 * @param[out] instance_data Instance data to pass to execute.
 * @param[in]  cbdata        Callback data.
 *
 * @returns Status code
 */
static ib_status_t act_auditlogparts_create(
    ib_mm_t       mm,
    ib_context_t *ctx,
    const char   *parameters,
    void         *instance_data,
    void         *cbdata
)
{
    assert(ctx != NULL);
    assert(instance_data != NULL);

    ib_status_t           rc;
    act_auditlog_parts_t *idata;
    ib_list_t            *oplist;
    const ib_strval_t    *map;

    /* Create the list */
    rc = ib_list_create(&oplist, mm);
    if (rc != IB_OK) {
        return rc;
    }

    /* Get the auditlog parts map */
    rc = ib_core_auditlog_parts_map(&map);
    if (rc != IB_OK) {
        return rc;
    }

    /* Parse the parameter string */
    rc = ib_flags_oplist_parse(map, mm, parameters, ",", oplist);
    if (rc != IB_OK) {
        return rc;
    }

    /* Create and populate the instance data */
    idata = ib_mm_alloc(mm, sizeof(*idata));
    if (idata == NULL) {
        return IB_EALLOC;
    }
    idata->oplist = oplist;

    *(void **)instance_data = idata;
    return IB_OK;
}

/**
 * Execution function for the auditLogParts action
 *
 * @param[in] rule_exec The rule execution object
 * @param[in] data Instance data
 * @param[in] cbdata Unused.
 */
static ib_status_t act_auditlogparts_execute(
    const ib_rule_exec_t *rule_exec,
    void                 *data,
    void                 *cbdata)
{
    assert(data != NULL);
    assert(rule_exec != NULL);
    assert(rule_exec->tx != NULL);

    const act_auditlog_parts_t *idata = (const act_auditlog_parts_t *)data;
    ib_tx_t    *tx = rule_exec->tx;
    ib_num_t    parts = tx->auditlog_parts;
    ib_flags_t  parts_flags = 0;
    ib_flags_t  parts_mask = 0;
    ib_status_t rc;

    rc = ib_flags_oplist_apply(idata->oplist, &parts_flags, &parts_mask);
    if (rc != IB_OK) {
        return rc;
    }

    /* Merge the set flags with the previous value. */
    parts = ( (parts_flags & parts_mask) | (parts & ~parts_mask) );

    ib_rule_log_debug(rule_exec, "Updating auditlog parts from "
                      "0x%08"PRIx64" to 0x%08"PRIx64,
                      (uint64_t)tx->auditlog_parts, (uint64_t)parts);
    tx->auditlog_parts = parts;

    return IB_OK;
}

ib_status_t ib_core_actions_init(ib_engine_t *ib, ib_module_t *mod)
{
    ib_status_t  rc;

    /* Register the set flag action. */
    rc = ib_action_create_and_register(
        NULL, ib,
        "setflag",
        act_setflags_create, NULL,
        NULL, /* no destroy function */ NULL,
        act_setflags_execute, NULL
    );
    if (rc != IB_OK) {
        return rc;
    }

    /* Register the set variable action. */
    rc = ib_action_create_and_register(
        NULL, ib,
        "setvar",
        act_setvar_create, NULL,
        NULL, /* no destroy function */ NULL,
        act_setvar_execute, NULL
    );
    if (rc != IB_OK) {
        return rc;
    }

    /* Register the event action. */
    rc = ib_action_create_and_register(
        NULL, ib,
        "event",
        act_event_create, NULL,
        NULL, /* no destroy function */ NULL,
        act_event_execute, NULL
    );
    if (rc != IB_OK) {
        return rc;
    }

    /* Register the block action. */
    rc = ib_action_create_and_register(
        NULL, ib,
        "block",
        act_block_create, NULL,
        NULL, NULL,
        act_block_execute, NULL
    );
    if (rc != IB_OK) {
        return rc;
    }

    /* Register the allow actions. */
    rc = ib_action_create_and_register(
        NULL, ib,
        "allow",
        act_allow_create, NULL,
        NULL, NULL,
        act_allow_execute, NULL
    );
    if (rc != IB_OK) {
        return rc;
    }

    /* Register the auditLogParts action. */
    rc = ib_action_create_and_register(
        NULL, ib,
        "auditLogParts",
        act_auditlogparts_create, NULL,
        NULL, NULL,
        act_auditlogparts_execute, NULL
    );
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_action_create_and_register(
        NULL, ib,
        "setRequestHeader",
        act_set_header_create, NULL,
        NULL, NULL,
        act_set_request_header_execute, NULL
    );
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_action_create_and_register(
        NULL, ib,
        "delRequestHeader",
        act_del_header_create, NULL,
        NULL, NULL,
        act_del_request_header_execute, NULL
    );
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_action_create_and_register(
        NULL, ib,
        "setResponseHeader",
        act_set_header_create, NULL,
        NULL, NULL,
        act_set_response_header_execute, NULL
    );
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_action_create_and_register(
        NULL, ib,
        "delResponseHeader",
        act_del_header_create, NULL,
        NULL, NULL,
        act_del_response_header_execute, NULL
    );
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_var_source_register(
        NULL,
        ib_engine_var_config_get(ib),
        IB_S2SL("param"),
        IB_PHASE_NONE, IB_PHASE_NONE
    );
    if (rc != IB_OK) {
        ib_log_notice(ib,
            "Core actions failed to register var \"param\": %s",
            ib_status_to_string(rc)
        );
        /* Continue. */
    }

    return IB_OK;
}
