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
 * @brief IronBee &mdash; core actions
 *
 * @author Craig Forbes <cforbes@qualys.com>
 */

#include "ironbee_config_auto.h"

#include "core_private.h"
#include "engine_private.h"
#include "rule_engine_private.h"

#include <ironbee/action.h>
#include <ironbee/bytestr.h>
#include <ironbee/debug.h>
#include <ironbee/field.h>
#include <ironbee/mpool.h>
#include <ironbee/rule_engine.h>
#include <ironbee/string.h>
#include <ironbee/types.h>
#include <ironbee/escape.h>
#include <ironbee/util.h>

#include <assert.h>
#include <stdlib.h>
#include <strings.h>

/**
 * Setvar action data.
 */
typedef enum {
    SETVAR_STRSET,                /**< Set to a constant string */
    SETVAR_NUMSET,                /**< Set to a constant number */
    SETVAR_NUMADD,                /**< Add to a value (counter) */
} setvar_op_t;

typedef union {
    ib_num_t         num;         /**< Numeric value */
    ib_bytestr_t    *bstr;        /**< String value */
} setvar_value_t;

typedef struct {
    setvar_op_t      op;          /**< Setvar operation */
    char            *name;        /**< Field name */
    bool             name_expand; /**< Field name should be expanded */
    ib_ftype_t       type;        /**< Data type */
    setvar_value_t   value;       /**< Value */
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
                                       const ib_rule_t *rule,
                                       ib_tx_t *tx,
                                       ib_flags_t flags,
                                       void *cbdata)
{
    IB_FTRACE_INIT();

    /* Data will be a C-Style string */
    const char *cstr = (const char *)data;
    int remove_flag = 0;

    if (*cstr == '!') {
        remove_flag = 1;
        ++cstr;
    }

    /* Handle the suspicious flag */
    if (strcasecmp(cstr, "suspicious") == 0) {
        // FIXME: Expose via FLAGS collection
        if (remove_flag) {
            ib_tx_flags_unset(tx, IB_TX_FSUSPICIOUS);
        }
        else {
            ib_tx_flags_set(tx, IB_TX_FSUSPICIOUS);
        }
    }
    else if (strcasecmp(cstr, "block") == 0) {
        if (remove_flag) {
            // FIXME: Remove in FLAGS collection
            ib_tx_flags_unset(tx,
                IB_TX_BLOCK_ADVISORY |
                IB_TX_BLOCK_PHASE    |
                IB_TX_BLOCK_IMMEDIATE
            );
        }
        else {
            // FIXME: Set in FLAGS collection
            ib_tx_flags_set(tx, IB_TX_BLOCK_ADVISORY);
        }
    }
    else {
        ib_log_notice_tx(tx,  "Set flag action: invalid flag '%s'", cstr);
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
                                     const ib_rule_t *rule,
                                     ib_tx_t *tx,
                                     ib_flags_t flags,
                                     void *cbdata)
{
    IB_FTRACE_INIT();
    ib_status_t  rc;
    ib_logevent_t *event;
    const char *expanded;
    ib_field_t *field;

    ib_log_debug_tx(tx, "Creating event via action");

    /* Expand the message string */
    if ( (rule->meta.flags & IB_RULEMD_FLAG_EXPAND_MSG) != 0) {
        char *tmp;
        rc = ib_data_expand_str(tx->dpi, rule->meta.msg, false, &tmp);
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
            rc = ib_data_expand_str(tx->dpi, rule->meta.data, false, &tmp);
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

    /* Populate fields */
    if (! ib_flags_any(rule->flags, IB_RULE_FLAG_NO_TGT)) {
        rc = ib_data_get(tx->dpi, "FIELD_NAME_FULL", &field);
        if ( (rc == IB_OK) && (field->type == IB_FTYPE_NULSTR) ) {
            const char *name = NULL;
            rc = ib_field_value(field, ib_ftype_nulstr_out(&name));
            if ( (rc != IB_OK) || (name != NULL) ) {
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
    if (ib_tx_flags_isset(tx,
                          (IB_TX_BLOCK_ADVISORY |
                           IB_TX_BLOCK_PHASE |
                           IB_TX_BLOCK_IMMEDIATE)) )
    {
        event->rec_action = IB_LEVENT_ACTION_BLOCK;
    }
    if (ib_tx_flags_isset(tx, (IB_TX_BLOCK_PHASE | IB_TX_BLOCK_IMMEDIATE)) ) {
        event->action = IB_LEVENT_ACTION_BLOCK;
    }

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

    /* Simple checks; params should look like '<name>=[<value>]' */
    eq = strchr(params, '=');
    if ( (eq == NULL) || (eq == params) ) {
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

    /* Does the name need to be expanded? */
    rc = ib_data_expand_test_str_ex(params, nlen, &(data->name_expand));
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Copy the name */
    data->name = ib_mpool_memdup_to_str(mp, params, nlen);
    if (data->name == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

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
        else if (expand) {
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
 * Expand a name from the DPI
 *
 * @param[in] rule The rule executing this action
 * @param[in] tx Transaction to get the value from
 * @param[in] label Label to use for debug / error messages
 * @param[in] setvar_data Setvar parameters
 * @param[out] exname Expanded name
 * @param[out] exnlen Length of @a exname
 *
 * @returns Status code
 */
static ib_status_t expand_name(const ib_rule_t *rule,
                               ib_tx_t *tx,
                               const char *label,
                               const setvar_data_t *setvar_data,
                               const char **exname,
                               size_t *exnlen)
{
    IB_FTRACE_INIT();
    assert(tx);
    assert(rule);
    assert(label);
    assert(setvar_data);
    assert(exname);
    assert(exnlen);

    /* Readability: Alias a common field. */
    const char *name = setvar_data->name;

    /* If it's expandable, expand it */
    if (setvar_data->name_expand) {
        char *tmp;
        size_t len;
        ib_status_t rc;

        rc = ib_data_expand_str_ex(tx->dpi,
                                   name, strlen(name),
                                   false, false,
                                   &tmp, &len);
        if (rc != IB_OK) {
            ib_rule_log_error(
                tx,
                rule,
                NULL,
                NULL,
                "%s: Failed to expand name \"%s\": %s",
                label,
                name,
                ib_status_to_string(rc));
            IB_FTRACE_RET_STATUS(rc);
        }
        *exname = tmp;
        *exnlen = len;
        ib_rule_log_debug(
            tx,
            rule,
            NULL,
            NULL,
            "%s: Expanded variable name from \"%s\" to \"%.*s\"",
            label,
            name,
            (int)len,
            tmp);
    }
    else {
        *exname = name;
        *exnlen = strlen(name);

        ib_rule_log_debug(
            tx,
            rule,
            NULL,
            NULL,
            "%s: No expansion of %s",
            label,
            name);
    }


    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * Get a field from the DPI
 *
 * @param[in] tx Transaction to get the value from
 * @param[in] name Name of the value
 * @param[in] namelen Length of @a name
 * @param[out] field The field from the DPI
 *
 * @returns Status code
 */
static ib_status_t get_data_value(ib_tx_t *tx,
                                  const char *name,
                                  size_t namelen,
                                  ib_field_t **field)
{
    IB_FTRACE_INIT();

    assert(tx != NULL);
    assert(name != NULL);
    assert(field != NULL);

    ib_field_t *cur = NULL;
    ib_status_t rc;
    ib_list_t *list;
    ib_list_node_t *first;
    size_t elements;

    rc = ib_data_get_ex(tx->dpi, name, namelen, &cur);
    if ( (rc == IB_ENOENT) || (cur == NULL) ) {
        *field = NULL;
        IB_FTRACE_RET_STATUS(IB_OK);
    }
    else if (rc != IB_OK) {
        *field = NULL;
        IB_FTRACE_RET_STATUS(rc);
    }

    /* If we got back something other than a list, or it's name matches
     * what we asked for, we're done */
    if ( (cur->type != IB_FTYPE_LIST) ||
         ((cur->nlen == namelen) && (memcmp(name, cur->name, namelen) == 0)) )
    {
        *field = cur;
        IB_FTRACE_RET_STATUS(IB_OK);
    }

    /*
     * If we got back a list and the field name doesn't match the name we
     * requested, assume that we got back a filtered list.
     */
    rc = ib_field_value(cur, ib_ftype_list_mutable_out(&list) );
    if (rc != IB_OK) {
        ib_log_error_tx(tx,
                        "setvar: Failed to get list from \"%.*s\": %s",
                        (int)namelen, name, ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }

    /* No elements?  Filtered list with no values.  Return NULL. */
    elements = ib_list_elements(list);
    if (elements == 0) {
        *field = NULL;
        IB_FTRACE_RET_STATUS(IB_OK);
    }

    if (elements != 1) {
        ib_log_notice_tx(tx,
                         "setvar:Got back list with %zd elements", elements);
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    /* Use the first (only) element in the list as our field */
    first = ib_list_first(list);
    if (first == NULL) {
        ib_log_error_tx(tx,
                        "setvar: Failed to get first list element "
                        "from \"%.*s\": %s",
                        (int)namelen, name, ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(IB_EUNKNOWN);
    }

    /* Finally, take the data from the first node.  Check and mate. */
    *field = (ib_field_t *)first->data;
    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * Contains logic for expanding act_setvar_execute field names for assignment.
 *
 *  @param[in] rule
 *  @param[in] tx
 *  @param[in] label Label (used for log messages)
 *  @param[in] setvar_data
 *  @param[in] flags
 *  @param[out] expanded The expanded string, possibly allocated from
 *              the memory pool or the result of
 *              an ib_mpool_alloc(mp, 0) call.
 *  @param[out] exlen The length of @a expanded.
 *
 *  @return
 *    - IB_OK @a expanded and @a exlen are populated.
 *    - IB_EINVAL if there was an internal error during expansion.
 *    - IB_EALLOC on memory errors.
 *
 */
static ib_status_t expand_data(
    const ib_rule_t *rule,
    ib_tx_t *tx,
    const char *label,
    const setvar_data_t *setvar_data,
    ib_flags_t flags,
    char** expanded,
    size_t *exlen)
{
    IB_FTRACE_INIT();

    assert(tx);
    assert(rule);
    assert(label);
    assert(setvar_data);
    assert(expanded);
    assert(exlen);

    ib_status_t rc;

    /* If setvar_data contains a byte string, we might expand it. */
    if (setvar_data->type == IB_FTYPE_BYTESTR) {

        /* Pull the data out of the bytestr */
        const char *bsdata =
            (const char *)ib_bytestr_ptr(setvar_data->value.bstr);
        size_t bslen = ib_bytestr_length(setvar_data->value.bstr);

        /* Expand the string */
        if (flags & IB_ACTINST_FLAG_EXPAND) {

            rc = ib_data_expand_str_ex(
                tx->dpi, bsdata, bslen, false, false, expanded, exlen);
            if (rc != IB_OK) {
                ib_rule_log_debug(
                    tx,
                    rule,
                    NULL,
                    NULL,
                    "%s: Failed to expand string \"%.*s\": %s",
                    label, (int) bslen, bsdata, ib_status_to_string(rc));
                IB_FTRACE_RET_STATUS(rc);
            }

            ib_rule_log_debug(
                tx,
                rule,
                NULL,
                NULL,
                "%s: Field \"%s\" was expanded.",
                label,
                setvar_data->name);

            if (ib_rule_log_level(tx->ib) >= IB_RULE_LOG_LEVEL_TRACE) {
                const char* hex_coded = ib_util_hex_escape(bsdata, bslen);
                if (hex_coded != NULL) {
                    ib_rule_log_debug(
                        tx,
                        rule,
                        NULL,
                        NULL,
                        "%s: Field \"%s\" has value: %s",
                        label,
                        setvar_data->name,
                        hex_coded);
                    free((void *)hex_coded);
                }
            }
        }
        else if (bsdata == NULL) {
            assert(bslen == 0);

            /* Get a non-null pointer that should never be dereferenced. */
            *expanded = ib_mpool_alloc(tx->mp, 0);

            ib_rule_log_debug(
                tx,
                rule,
                NULL,
                NULL,
                "%s: Field \"%s\" is null.",
                label,
                setvar_data->name);
        }
        else {
            *expanded = ib_mpool_memdup(tx->mp, bsdata, bslen);
            if (*expanded == NULL) {
                ib_rule_log_debug(
                    tx,
                    rule,
                    NULL,
                    NULL,
                    "%s: Failed to copy string \"%.*s\"",
                    label,
                    (int)bslen,
                    bsdata);
                IB_FTRACE_RET_STATUS(IB_EALLOC);
            }
        }
        *exlen = bslen;
    }
    else {
        ib_rule_log_debug(
            tx,
            rule,
            NULL,
            NULL,
            "%s: Did not expand \"%s\" because it is not a byte string.",
            label,
            setvar_data->name
            );

    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * Execute function for the "set variable" action
 *
 * @param[in] data Setvar data (setvar_data_t *)
 * @param[in] rule The matched rule
 * @param[in] tx IronBee transaction
 * @param[in] flags Action instance flags
 * @param[in] cbdata Unused.
 *
 * @returns Status code
 *    - IB_OK The operator succeeded.
 *    - IB_EINVAL if there was an internal error during expansion.
 *    - IB_EALLOC on memory errors.
 */
static ib_status_t act_setvar_execute(void *data,
                                      const ib_rule_t *rule,
                                      ib_tx_t *tx,
                                      ib_flags_t flags,
                                      void *cbdata)
{
    IB_FTRACE_INIT();

    assert(data != NULL);
    assert(rule != NULL);
    assert(tx != NULL);

    ib_status_t rc;
    ib_field_t *cur_field = NULL;
    ib_field_t *new_field;
    const char *name = NULL; /* Name of the field we are setting. */
    size_t nlen;             /* Name length. */
    char *value = NULL;      /* Value we are setting the field name to. */
    size_t vlen;             /* Value length. */

    /* Data should be a setvar_data_t created in our create function */
    const setvar_data_t *setvar_data = (const setvar_data_t *)data;

    /* Expand the name (if required) */
    rc = expand_name(rule, tx, "setvar", setvar_data, &name, &nlen);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Get the current value */
    rc = get_data_value(tx, name, nlen, &cur_field);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* If setvar_data contains a byte string, we might expand it. */
    rc = expand_data(rule, tx, "setvar", setvar_data, flags, &value, &vlen);
    if ( rc != IB_OK ) {
        IB_FTRACE_RET_STATUS(rc);
    }

    switch(setvar_data->op) {
        /* Handle bytestr operations (cur_fieldrently only set) */
        case SETVAR_STRSET:
            assert(setvar_data->type == IB_FTYPE_BYTESTR);
            ib_bytestr_t *bs = NULL;

            if (cur_field != NULL) {
                ib_data_remove_ex(tx->dpi, name, nlen, NULL);
            }

            /* Create a bytestr to hold it. */
            rc = ib_bytestr_alias_mem(&bs, tx->mp, (uint8_t *)value, vlen);
            if (rc != IB_OK) {
                ib_log_error_tx(tx,
                                "setvar: Failed to create bytestring "
                                "for field \"%.*s\": %s",
                                (int)nlen, name, ib_status_to_string(rc));
                IB_FTRACE_RET_STATUS(rc);
            }

            /* Create the new_field field */
            rc = ib_field_create(&new_field,
                                 tx->mp,
                                 name, nlen,
                                 setvar_data->type,
                                 ib_ftype_bytestr_in(bs));
            if (rc != IB_OK) {
                ib_log_error_tx(tx,
                                "setvar: Failed to create field \"%.*s\": %s",
                                (int)nlen, name, ib_status_to_string(rc));
                IB_FTRACE_RET_STATUS(rc);
            }

            /* Add the field to the DPI */
            rc = ib_data_add(tx->dpi, new_field);
            if (rc != IB_OK) {
                ib_log_error_tx(tx,
                                "setvar: Failed to add field \"%.*s\": %s",
                                (int)nlen, name, ib_status_to_string(rc));
                IB_FTRACE_RET_STATUS(rc);
            }
            break;
        /* Numerical operation : Set */
        case SETVAR_NUMSET:
            assert(setvar_data->type == IB_FTYPE_NUM);

            if (cur_field != NULL) {
                ib_data_remove_ex(tx->dpi, name, nlen, NULL);
            }

            /* Create the new_field field */
            rc = ib_field_create(&new_field,
                                 tx->mp,
                                 name, nlen,
                                 setvar_data->type,
                                 ib_ftype_num_in(&setvar_data->value.num));
            if (rc != IB_OK) {
                ib_log_error_tx(tx,
                                "setvar: Failed to create field \"%.*s\": %s",
                                (int)nlen, name, ib_status_to_string(rc));
                IB_FTRACE_RET_STATUS(rc);
            }

            /* Add the field to the DPI */
            rc = ib_data_add(tx->dpi, new_field);
            if (rc != IB_OK) {
                ib_log_error_tx(tx,
                                "setvar: Failed to add field \"%.*s\": %s",
                                (int)nlen, name, ib_status_to_string(rc));
                IB_FTRACE_RET_STATUS(rc);
            }

            break;
        /* Numerical operation : Add */
        case SETVAR_NUMADD:
            assert(setvar_data->type == IB_FTYPE_NUM);

            /* If it doesn't exist, create the variable with a value of zero */
            if (cur_field == NULL) {

                /* Create the new_field field */
                rc = ib_data_add_num_ex(tx->dpi, name, nlen, 0, &cur_field);
                if (rc != IB_OK) {
                    ib_log_error_tx(
                        tx,
                        "setvar: Failed to add field \"%.*s\": %s",
                        (int)nlen, name, ib_status_to_string(rc));
                    IB_FTRACE_RET_STATUS(rc);
                }
            }

            /* Handle num and unum types */
            if (cur_field->type == IB_FTYPE_NUM) {
                ib_num_t num;
                rc = ib_field_value(cur_field, ib_ftype_num_out(&num));
                if (rc != IB_OK) {
                    IB_FTRACE_RET_STATUS(rc);
                }

                num += setvar_data->value.num;
                ib_field_setv(cur_field, ib_ftype_num_in(&num));
            }
            else if (cur_field->type == IB_FTYPE_UNUM) {
                ib_unum_t num;
                rc = ib_field_setv(cur_field, ib_ftype_unum_out(&num));
                if (rc != IB_OK) {
                    IB_FTRACE_RET_STATUS(rc);
                }

                num += (ib_unum_t)setvar_data->value.num;
                ib_field_setv(cur_field, ib_ftype_unum_in(&num));
            }
            else {
                ib_log_error_tx(
                    tx,
                    "setvar: field \"%.*s\" type %d invalid for NUMADD",
                     (int)nlen, name, cur_field->type);
                IB_FTRACE_RET_STATUS(IB_EINVAL);
            }
            break;
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * Find event from this rule
 *
 * @param[in] tx The transaction to search
 * @param[in] rule The rule that fired the action
 * @param[out] event Matching event
 *
 * @return
 *   - IB_OK (if found)
 *   - IB_ENOENT if not found
 *   - Errors returned by ib_event_get_all()
 */
static ib_status_t get_event(ib_tx_t *tx,
                             const ib_rule_t *rule,
                             ib_logevent_t **event)
{
    IB_FTRACE_INIT();

    assert(tx != NULL);
    assert(tx->epi != NULL);
    assert(rule != NULL);

    ib_status_t rc;
    ib_list_t *event_list;
    ib_list_node_t *event_node;

    rc = ib_event_get_all(tx->epi, &event_list);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }
    event_node = ib_list_last(event_list);
    if (event_node == NULL) {
        IB_FTRACE_RET_STATUS(IB_ENOENT);
    }
    ib_logevent_t *e = (ib_logevent_t *)event_node->data;
    if (strcmp(e->rule_id, ib_rule_id(rule)) == 0) {
        *event = e;
        IB_FTRACE_RET_STATUS(IB_OK);
    }

    IB_FTRACE_RET_STATUS(IB_ENOENT);
}

/**
 * Set the IB_TX_BLOCK_ADVISORY flag and set the DPI value @c FLAGS:BLOCK=1.
 *
 * @param[in,out] tx The transaction we are going to modify.
 * @param[in] rule The rule that fired the action
 *
 * @return
 *   - IB_OK on success.
 *   - Errors by ib_data_add_num.
 *   - Other if an event exists, but cannot be retrieved for this action.
 */
static ib_status_t act_block_advisory_execute(ib_tx_t *tx,
                                              const ib_rule_t *rule)
{
    IB_FTRACE_INIT();

    assert(tx != NULL);
    assert(rule != NULL);

    ib_status_t rc;
    ib_num_t ib_num_one = 1;
    ib_logevent_t *event;

    /* Don't re-set the flag because it bloats the DPI value FLAGS
     * with lots of BLOCK entries. */
    if (!ib_tx_flags_isset(tx, IB_TX_BLOCK_ADVISORY)) {

        /* Set the flag in the transaction. */
        ib_tx_flags_set(tx, IB_TX_BLOCK_ADVISORY);

        /* When doing an advisory block, mark the DPI with FLAGS:BLOCK=1. */
        rc = ib_data_add_num(tx->dpi, "FLAGS:BLOCK", ib_num_one, NULL);
        if (rc != IB_OK) {
            ib_rule_log_error(
                tx,
                rule,
                NULL,
                NULL,
                "Could not set value FLAGS:BLOCK=1: %s",
                ib_status_to_string(rc));
            IB_FTRACE_RET_STATUS(rc);
        }

        /* Update the event (if required) */
        rc = get_event(tx, rule, &event);
        if (rc == IB_OK) {
            event->rec_action = IB_LEVENT_ACTION_BLOCK;
        }
        else if (rc != IB_ENOENT) {
            ib_rule_log_error(
                tx,
                rule,
                NULL,
                NULL,
                "Failed to fetch event associated with this action: %s",
                ib_status_to_string(rc));
            IB_FTRACE_RET_STATUS(rc);
        }
    }

    ib_rule_log_debug(
        tx,
        rule,
        NULL,
        NULL,
        "Advisory block.");

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * Set the IB_TX_BLOCK_PHASE flag in the tx.
 *
 * @param[in,out] tx The transaction we are going to modify.
 * @param[in] rule The rule that fired the action
 *
 * @return
 *   - IB_OK on success.
 *   - Other if an event exists, but cannot be retrieved for this action.
 */
static ib_status_t act_block_phase_execute(ib_tx_t *tx,
                                           const ib_rule_t *rule)
{
    IB_FTRACE_INIT();

    ib_status_t rc;
    ib_logevent_t *event;

    ib_tx_flags_set(tx, IB_TX_BLOCK_PHASE);

    /* Update the event (if required) */
    rc = get_event(tx, rule, &event);
    if (rc == IB_OK) {
        event->rec_action = IB_LEVENT_ACTION_BLOCK;
        event->action = IB_LEVENT_ACTION_BLOCK;
    }
    else if (rc != IB_ENOENT) {
        ib_rule_log_error(
            tx,
            rule,
            NULL,
            NULL,
            "Failed phase block: %s.",
            ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }

    ib_rule_log_debug(
        tx,
        rule,
        NULL,
        NULL,
        "Phase block.");

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * Set the IB_TX_BLOCK_IMMEDIATE flag in the tx.
 *
 * @param[in,out] tx The transaction we are going to modify.
 * @param[in] rule The rule that fired the action
 *
 * @returns
 *   - IB_OK on success.
 *   - Other if an event exists, but cannot be retrieved for this action.
 */
static ib_status_t act_block_immediate_execute(ib_tx_t *tx,
                                               const ib_rule_t *rule)
{
    IB_FTRACE_INIT();
    assert(tx != NULL);
    assert(rule != NULL);

    ib_status_t rc;
    ib_logevent_t *event;

    ib_tx_flags_set(tx, IB_TX_BLOCK_IMMEDIATE);

    /* Update the event (if required) */
    rc = get_event(tx, rule, &event);
    if (rc == IB_OK) {
        event->rec_action = IB_LEVENT_ACTION_BLOCK;
        event->action = IB_LEVENT_ACTION_BLOCK;
    }
    else if (rc != IB_ENOENT) {
        ib_rule_log_error(
            tx,
            rule,
            NULL,
            NULL,
            "Failed immediate block: %s.",
            ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }

    ib_rule_log_debug(
        tx,
        rule,
        NULL,
        NULL,
        "Immediate block.");

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * The function that implements flagging a particular block type.
 */
typedef ib_status_t(*act_block_execution_t)(
    ib_tx_t         *tx,
    const ib_rule_t *rule
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
 * @param[in] data Cast to an @c act_block_t and the @c execute field is
 *            called on the given @a tx.
 * @param[in] rule The rule structure.
 * @param[out] tx The transaction we are going to modify.
 * @param[in] flags Flags. Unused.
 * @param[in] cbdata Callback data. Unused.
 */
static ib_status_t act_block_execute(void* data,
                                     const ib_rule_t *rule,
                                     ib_tx_t *tx,
                                     ib_flags_t flags,
                                     void *cbdata)
{
    IB_FTRACE_INIT();

    assert(data);
    assert(tx);

    ib_status_t rc = ((const act_block_t *)data)->execute(tx, rule);

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
 * @param[in] cbdata Callback data. Unused.
 *
 * @returns IB_OK.
 */
static ib_status_t act_status_execute(void* data,
                                      const ib_rule_t *rule,
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
 * @return
 *   - IB_OK on success.
 *   - IB_EALLOC on an allocation error from mp.
 *   - IB_EINVAL if @a param is NULL or not convertible with
 *               @c atoi(const @c char*) to an integer in the range 200
 *               through 599 inclusive.
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
        ib_log_error(ib,
                     "Action status must be given a parameter "
                     "x where 200 <= x < 600. It was given %s.",
                     params);
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    act_status->block_status = block_status;

    inst->data = act_status;

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * Expand a header name from the DPI
 *
 * @todo This should be removed, and expand_name should be used
 *
 * @param[in] tx Transaction to get the value from
 * @param[in] label Label to use for debug / error messages
 * @param[in] name Name to expand
 * @param[in] expandable Is @a expandable?
 * @param[out] exname Expanded name
 * @param[out] exnlen Length of @a exname
 *
 * @returns Status code
 */
static ib_status_t expand_name_hdr(ib_tx_t *tx,
                                   const char *label,
                                   const char *name,
                                   bool expandable,
                                   const char **exname,
                                   size_t *exnlen)
{
    IB_FTRACE_INIT();
    assert(tx != NULL);
    assert(label != NULL);
    assert(name != NULL);
    assert(exname != NULL);
    assert(exnlen != NULL);

    /* If it's expandable, expand it */
    if (expandable) {
        char *tmp;
        size_t len;
        ib_status_t rc;

        rc = ib_data_expand_str(tx->dpi, name, false, &tmp);
        if (rc != IB_OK) {
            ib_log_error_tx(tx,
                            "%s: Failed to expand name \"%s\": %s",
                            label, name, ib_status_to_string(rc));
            IB_FTRACE_RET_STATUS(rc);
        }
        len = strlen(tmp);
        *exname = tmp;
        *exnlen = len;
        ib_log_debug_tx(tx,
                        "%s: Expanded variable name from "
                        "\"%s\" to \"%.*s\"",
                        label, name, (int)len, tmp);
    }
    else {
        *exname = name;
        *exnlen = strlen(name);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * Expand a string from the DPI
 *
 * @todo Should call ib_data_expand_str_ex()
 *
 * @param[in] tx Transaction to get the value from
 * @param[in] label Label to use for debug / error messages
 * @param[in] str String to expand
 * @param[in] flags Action flags
 * @param[out] expanded Expanded string
 * @param[out] exlen Length of @a expanded
 *
 * @returns Status code
 */
static ib_status_t expand_str(ib_tx_t *tx,
                              const char *label,
                              const char *str,
                              ib_flags_t flags,
                              const char **expanded,
                              size_t *exlen)
{
    IB_FTRACE_INIT();
    assert(tx != NULL);
    assert(label != NULL);
    assert(str != NULL);
    assert(expanded != NULL);
    assert(exlen != NULL);

    /* If it's expandable, expand it */
    if ( (flags & IB_ACTINST_FLAG_EXPAND) != 0) {
        char *tmp;
        size_t len;
        ib_status_t rc;

        rc = ib_data_expand_str(tx->dpi, str, false, &tmp);
        if (rc != IB_OK) {
            ib_log_error_tx(tx,
                            "%s: Failed to expand \"%s\": %s",
                            label, str, ib_status_to_string(rc));
            IB_FTRACE_RET_STATUS(rc);
        }
        len = strlen(tmp);
        *expanded = tmp;
        *exlen = len;
        ib_log_debug_tx(tx,
                        "%s: Expanded \"%s\" to \"%.*s\"",
                        label, str, (int)len, tmp);
    }
    else {
        *expanded = str;
        *exlen = strlen(str);
    }
    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * Holds the name of the header and the value to set it to.
 */
struct act_header_data_t {
    const char *name;        /**< Name of the header to operate on. */
    bool        name_expand; /**< Is name expandable? */
    const char *value;       /**< Value to replace the header with. */
};
typedef struct act_header_data_t act_header_data_t;

/**
 * Common create routine for delResponseHeader and delRequestHeader action.
 *
 * @param[in] ib The IronBee engine.
 * @param[in] ctx The context.
 * @param[in] mp The memory pool this is allocated out of.
 * @param[in] params Parameters of the format name=&lt;header name&gt;.
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

    assert(ib != NULL);
    assert(ctx != NULL);
    assert(mp != NULL);
    assert(params != NULL);
    assert(inst != NULL);

    act_header_data_t *act_data =
        (act_header_data_t *)ib_mpool_alloc(mp, sizeof(*act_data));
    ib_status_t rc;

    if (act_data == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    if ( (params == NULL) || (strlen(params) == 0) ) {
        ib_log_error(ib, "Operation requires a parameter.");
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    act_data->name = ib_mpool_strdup(mp, params);

    if (act_data->name == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    /* Does the name need to be expanded? */
    rc = ib_data_expand_test_str_ex(params, strlen(params),
                                    &(act_data->name_expand));
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    inst->data = act_data;

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * Common create routine for setResponseHeader and setRequestHeader actions.
 *
 * @param[in] ib The IronBee engine.
 * @param[in] ctx The context.
 * @param[in] mp The memory pool this is allocated out of.
 * @param[in] params Parameters of the format name=&gt;header name&lt;.
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

    assert(ib != NULL);
    assert(ctx != NULL);
    assert(mp != NULL);
    assert(params != NULL);
    assert(inst != NULL);

    size_t name_len;
    size_t value_len;
    size_t params_len;
    char *equals_idx;
    act_header_data_t *act_data =
        (act_header_data_t *)ib_mpool_alloc(mp, sizeof(*act_data));
    bool expand = false;
    ib_status_t rc;

    if (act_data == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    if ( (params == NULL) || (strlen(params) == 0) ) {
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

    act_data->name = (const char *)ib_mpool_memdup(mp, params, name_len+1);
    if (act_data->name == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    /* Terminate name with '\0'. This replaces the '=' that was copied.
     * Notice that we strip the const-ness of this value to make this one
     * assignment. */
    ((char *)act_data->name)[name_len] = '\0';

    /* Does the name need to be expanded? */
    rc = ib_data_expand_test_str_ex(act_data->name, name_len,
                                    &(act_data->name_expand));
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    act_data->value = (value_len == 0)?
        ib_mpool_strdup(mp, ""):
        ib_mpool_strdup(mp, equals_idx+1);
    if (act_data->value == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    rc = ib_data_expand_test_str_ex(act_data->value, value_len, &expand);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }
    else if (expand) {
        inst->flags |= IB_ACTINST_FLAG_EXPAND;
    }

    inst->data = act_data;
    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * Set the request header in @c tx->dpi.
 */
static ib_status_t act_set_request_header_execute(void* data,
                                                  const ib_rule_t *rule,
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
    act_header_data_t *act_data = (act_header_data_t *)data;
    const char *value;
    size_t value_len;
    const char *name;
    size_t name_len;

    /* Expand the name (if required) */
    rc = expand_name_hdr(tx,
                         "setRequestHeader",
                         act_data->name,
                         act_data->name_expand,
                         &name,
                         &name_len);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = expand_str(tx,
                    "setRequestHeader",
                    act_data->value,
                    flags,
                    &value,
                    &value_len);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    ib_rule_log_debug(tx,
                      rule,
                      NULL,
                      NULL,
                      "Setting request header \"%.*s\"=\"%.*s\"",
                      (int)name_len,
                      name,
                      (int)value_len,
                      value);

    /* Note: ignores lengths for now */
    rc = ib_server_header(tx->ib->server,
                          tx,
                          IB_SERVER_REQUEST,
                          IB_HDR_SET,
                          name,
                          value);

    IB_FTRACE_RET_STATUS(rc);
}

static ib_status_t act_del_request_header_execute(void *data,
                                                  const ib_rule_t *rule,
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
    act_header_data_t *act_data = (act_header_data_t *)data;
    const char *name;
    size_t name_len;

    /* Expand the name (if required) */
    rc = expand_name_hdr(tx, "delRequestHeader",
                         act_data->name, act_data->name_expand,
                         &name, &name_len);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    ib_log_debug_tx(tx, "Deleting request header \"%.*s\"",
                    (int)name_len, name);
    /* Note: ignores lengths for now */
    rc = ib_server_header(tx->ib->server,
                          tx,
                          IB_SERVER_REQUEST,
                          IB_HDR_UNSET,
                          name,
                          "");

    IB_FTRACE_RET_STATUS(rc);
}

static ib_status_t act_set_response_header_execute(void* data,
                                                   const ib_rule_t *rule,
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
    act_header_data_t *act_data = (act_header_data_t *)data;
    const char *value;
    size_t value_len;
    const char *name;
    size_t name_len;

    /* Expand the name (if required) */
    rc = expand_name_hdr(tx, "setResponseHeader",
                         act_data->name, act_data->name_expand,
                         &name, &name_len);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = expand_str(tx, "setResponseHeader", act_data->value, flags,
                    &value, &value_len);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    ib_log_debug_tx(tx, "Setting response header \"%.*s\"=\"%.*s\"",
                    (int)name_len, name, (int)value_len, value);

    /* Note: ignores lengths for now */
    rc = ib_server_header(tx->ib->server,
                          tx,
                          IB_SERVER_RESPONSE,
                          IB_HDR_SET,
                          name,
                          value);

    IB_FTRACE_RET_STATUS(rc);
}

static ib_status_t act_del_response_header_execute(void* data,
                                                   const ib_rule_t *rule,
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
    act_header_data_t *act_data = (act_header_data_t *)data;
    const char *name;
    size_t name_len;

    /* Expand the name (if required) */
    rc = expand_name_hdr(tx, "delResponseHeader",
                         act_data->name, act_data->name_expand,
                         &name, &name_len);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    ib_log_debug_tx(tx, "Deleting response header \"%.*s\"",
                    (int)name_len, name);

    /* Note: ignores lengths for now */
    rc = ib_server_header(tx->ib->server,
                          tx,
                          IB_SERVER_RESPONSE,
                          IB_HDR_UNSET,
                          name,
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
                                     const ib_rule_t *rule,
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
         (ib_flags_all(set_flags, IB_TX_ALLOW_ALL)) )
    {
        set_flags |= IB_TX_ALLOW_PHASE;
    }

    /* Set the flags in the TX */
    ib_tx_flags_set(tx, set_flags);

    /* For ALLOW_PHASE, store the current phase */
    if (ib_flags_all(set_flags, IB_TX_ALLOW_PHASE)) {
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
