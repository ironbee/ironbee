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
 * @brief IronBee
 *
 * @author Nick LeRoy <nleroy@qualys.com>
 */

#include "ironbee_config_auto.h"

#include "rule_engine_private.h"

#include <ironbee/action.h>
#include <ironbee/bytestr.h>
#include <ironbee/core.h>
#include <ironbee/debug.h>
#include <ironbee/field.h>
#include <ironbee/mpool.h>
#include <ironbee/operator.h>
#include <ironbee/rule_engine.h>
#include <ironbee/transformation.h>
#include <ironbee/util.h>

#include <assert.h>
#include <inttypes.h>

/**
 * Prefix used for all rule engine log entries.
 */
//static const char *LOG_PREFIX = "RULE_ENG";

/**
 * Length of buffer used for formatting fields
 */
static const size_t MAX_FIELD_BUF = 64;
static const size_t PREFIX_BUFSIZE = 32;
static const size_t REV_BUFSIZE = 16;

/**
 * If any of these flags are set, enable rule logging
 */
#define RULE_LOG_FLAG_RULE_ENABLE                            \
    ( IB_RULE_LOG_FLAG_RULE |                                \
      IB_RULE_LOG_FLAG_RULE_DATA |                           \
      IB_RULE_LOG_FLAG_TFN |                                 \
      IB_RULE_LOG_FLAG_OPERATOR |                            \
      IB_RULE_LOG_FLAG_ACTION |                              \
      IB_RULE_LOG_FLAG_EVENT |                               \
      IB_RULE_LOG_FLAG_AUDIT |                               \
      IB_RULE_LOG_FLAG_TIMING )

/**
 * If any of these flags are set, enable target gathering
 */
#define RULE_LOG_FLAG_TARGET_ENABLE                  \
    ( IB_RULE_LOG_FLAG_RULE_DATA |                   \
      IB_RULE_LOG_FLAG_TFN |                         \
      IB_RULE_LOG_FLAG_OPERATOR |                    \
      IB_RULE_LOG_FLAG_ACTION )

/**
 * Mapping of valid rule logging names to flag values.
 */
static IB_STRVAL_MAP(flags_map) = {
    IB_STRVAL_PAIR("tx", IB_RULE_LOG_FLAG_TX),
    IB_STRVAL_PAIR("requestLine", IB_RULE_LOG_FLAG_REQ_LINE),
    IB_STRVAL_PAIR("requestHeader", IB_RULE_LOG_FLAG_REQ_HEADER),
    IB_STRVAL_PAIR("requestBody", IB_RULE_LOG_FLAG_REQ_BODY),
    IB_STRVAL_PAIR("responseLine", IB_RULE_LOG_FLAG_RSP_LINE),
    IB_STRVAL_PAIR("responseHeader", IB_RULE_LOG_FLAG_RSP_HEADER),
    IB_STRVAL_PAIR("responseBody", IB_RULE_LOG_FLAG_RSP_BODY),
    IB_STRVAL_PAIR("phase", IB_RULE_LOG_FLAG_PHASE),
    IB_STRVAL_PAIR("rule", IB_RULE_LOG_FLAG_RULE),
    IB_STRVAL_PAIR("ruleData", IB_RULE_LOG_FLAG_RULE_DATA),
    IB_STRVAL_PAIR("transformation", IB_RULE_LOG_FLAG_TFN),
    IB_STRVAL_PAIR("operator", IB_RULE_LOG_FLAG_OPERATOR),
    IB_STRVAL_PAIR("action", IB_RULE_LOG_FLAG_ACTION),
    IB_STRVAL_PAIR("event", IB_RULE_LOG_FLAG_EVENT),
    IB_STRVAL_PAIR("audit", IB_RULE_LOG_FLAG_AUDIT),
    IB_STRVAL_PAIR("timing", IB_RULE_LOG_FLAG_TIMING),

    IB_STRVAL_PAIR("allRules", IB_RULE_LOG_FLAG_MODE_ALL),
    IB_STRVAL_PAIR("actionableRulesOnly", IB_RULE_LOG_FLAG_MODE_ACT),
    IB_STRVAL_PAIR("operatorErrorOnly", IB_RULE_LOG_FLAG_MODE_ERROR),
    IB_STRVAL_PAIR("returnedTrueOnly", IB_RULE_LOG_FLAG_MODE_TRUE),
    IB_STRVAL_PAIR("returnedFalseOnly", IB_RULE_LOG_FLAG_MODE_FALSE),

    /* End */
    IB_STRVAL_PAIR_LAST
};

/**
 * Low level rule logging
 *
 * @param[in] log_tx Rule logging transaction information
 * @param[in] log_exec  Rule logging execution information
 * @param[in] fmt Printf-like format string
 * @param[in] ap Argument list
 */
static void rule_vlog(const ib_rule_log_tx_t *log_tx,
                      const ib_rule_log_exec_t *log_exec,
                      const char *fmt,
                      va_list ap) VPRINTF_ATTRIBUTE(3);

/**
 * Log a rule result
 *
 * @param[in] log_tx Rule logging transaction information
 * @param[in] log_exec  Rule logging execution information
 * @param[in] fmt Printf-like format string
 */
static void rule_log(const ib_rule_log_tx_t *log_tx,
                     const ib_rule_log_exec_t *log_exec,
                     const char *fmt, ...) PRINTF_ATTRIBUTE(3, 4);


void ib_rule_log_flags_dump(ib_engine_t *ib,
                            ib_context_t *ctx)
{
    IB_FTRACE_INIT();
    ib_core_cfg_t *corecfg = NULL;
    ib_strval_t *rec;

    ib_context_module_config(ctx, ib_core_module(), (void *)&corecfg);
    if (corecfg->rule_debug_log_level < IB_RULE_LOG_LEVEL_DEBUG) {
        IB_FTRACE_RET_VOID();
    }

    rec = (ib_strval_t *)flags_map;
    while (rec->str != NULL) {
        bool enabled = ib_flags_all(corecfg->rule_log_flags, rec->val);
        ib_log(ib, corecfg->rule_log_level,
               "Rule logging flag %s [0x%08x]: %s",
               rec->str,
               (unsigned int)rec->val,
               enabled ? "enabled" : "disabled");
        ++rec;
    }
    IB_FTRACE_RET_VOID();
}

ib_flags_t ib_rule_log_flags(ib_context_t *ctx)
{
    IB_FTRACE_INIT();
    ib_core_cfg_t *corecfg = NULL;
    ib_context_module_config(ctx, ib_core_module(), (void *)&corecfg);
    IB_FTRACE_RET_UINT(corecfg->rule_log_flags);
}

ib_log_level_t ib_rule_log_level(ib_context_t *ctx)
{
    IB_FTRACE_INIT();
    ib_core_cfg_t *corecfg = NULL;
    ib_context_module_config(ctx, ib_core_module(), (void *)&corecfg);
    IB_FTRACE_RET_INT(corecfg->rule_log_level);
}

ib_rule_log_level_t ib_rule_debug_log_level(ib_context_t *ctx)
{
    IB_FTRACE_INIT();
    ib_core_cfg_t *corecfg = NULL;
    ib_context_module_config(ctx, ib_core_module(), (void *)&corecfg);
    IB_FTRACE_RET_INT(corecfg->rule_debug_log_level);
}

/* Log TX start */
ib_status_t ib_rule_log_tx_create(
    ib_tx_t *tx,
    ib_rule_log_tx_t **log_tx
)
{
    IB_FTRACE_INIT();
    ib_flags_t flags;
    ib_rule_log_tx_t *new;
    ib_rule_log_mode_t mode;

    assert(tx != NULL);
    assert(log_tx != NULL);

    *log_tx = NULL;

    /* Get the flags from the transaction */
    flags = (ib_flags_t)ib_rule_log_flags(tx->ctx);

    /* Allocate the object */
    new = ib_mpool_calloc(tx->mp, sizeof(*new), 1);
    if (new == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    /* Get the start time */
    if (ib_flags_all(flags, IB_RULE_LOG_FLAG_TIMING)) {
        ib_clock_gettimeofday(&new->start_time);
    }

    /* Extract the mode */
    if (ib_flags_all(flags, IB_RULE_LOG_FLAG_MODE_ACT)) {
        mode = IB_RULE_LOG_MODE_ACT;
    }
    else if (ib_flags_all(flags, IB_RULE_LOG_FLAG_MODE_ERROR)) {
        mode = IB_RULE_LOG_MODE_ERROR;
    }
    else if (ib_flags_all(flags, IB_RULE_LOG_FLAG_MODE_TRUE)) {
        mode = IB_RULE_LOG_MODE_TRUE;
    }
    else if (ib_flags_all(flags, IB_RULE_LOG_FLAG_MODE_FALSE)) {
        mode = IB_RULE_LOG_MODE_FALSE;
    }
    else {
        mode = IB_RULE_LOG_MODE_ALL;
    }

    /* Complete the new object, store pointer to it */
    new->level = ib_rule_log_level(tx->ctx);
    new->flags = flags;
    new->mode = mode;
    new->tx = tx;
    *log_tx = new;
    tx->rule_log_tx = new;

    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t ib_rule_log_exec_create(ib_rule_log_tx_t *log_tx,
                                    const ib_rule_t *rule,
                                    ib_rule_log_exec_t **log_exec)
{
    IB_FTRACE_INIT();
    ib_status_t rc;
    ib_rule_log_exec_t *new;

    assert(rule != NULL);
    assert(log_exec != NULL);

    *log_exec = NULL;

    /* If no transaction, nothing to do */
    if (log_tx == NULL) {
        IB_FTRACE_RET_STATUS(IB_OK);
    }

    /* Flag check */
    if (ib_flags_any(log_tx->flags, RULE_LOG_FLAG_RULE_ENABLE) == false) {
        IB_FTRACE_RET_STATUS(IB_OK);
    }

    /* Allocate the object */
    new = ib_mpool_calloc(log_tx->tx->mp, sizeof(*new), 1);
    if (new == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    /* Create the list of target fields */
    if (ib_flags_any(log_tx->flags, RULE_LOG_FLAG_TARGET_ENABLE)) {
        rc = ib_list_create(&(new->tgt_list), log_tx->tx->mp);
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }
    }

    /* Complete the new object, store pointer to it */
    new->log_tx = log_tx;
    new->rule = rule;
    /* Don't need to initialize num_xxx because we use calloc() above */
    *log_exec = new;

    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t ib_rule_log_exec_add_target(
    ib_rule_log_exec_t *log_exec,
    const ib_rule_target_t *target,
    const ib_field_t *original
)
{
    IB_FTRACE_INIT();
    ib_status_t rc = IB_OK;
    ib_rule_log_tgt_t *tgt;

    if (log_exec == NULL) {
        IB_FTRACE_RET_STATUS(IB_OK);
    }
    ++(log_exec->num_tgt);
    if (log_exec->tgt_list == NULL) {
        IB_FTRACE_RET_STATUS(IB_OK);
    }

    tgt = (ib_rule_log_tgt_t *)
        ib_mpool_calloc(log_exec->log_tx->tx->mp, sizeof(*tgt), 1);
    if (tgt == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }
    tgt->target = target;
    tgt->original = original;
    tgt->transformed = NULL;

    /* Initialize the result list */
    if (ib_flags_all(log_exec->log_tx->flags, IB_RULE_LOG_FLAG_OPERATOR) ) {
        rc = ib_list_create(&tgt->rslt_list, log_exec->log_tx->tx->mp);
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }
    }
    tgt->num_results = 0;

    /* Initialize the transformation list */
    if (ib_flags_all(log_exec->log_tx->flags, IB_RULE_LOG_FLAG_TFN) ) {
        rc = ib_list_create(&tgt->tfn_list, log_exec->log_tx->tx->mp);
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }
    }
    tgt->num_tfn = 0;

    rc = ib_list_push(log_exec->tgt_list, tgt);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }
    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_rule_log_exec_set_tgt_final(ib_rule_log_exec_t *log_exec,
                                           const ib_field_t *final)
{
    IB_FTRACE_INIT();
    ib_list_node_t *node;
    ib_rule_log_tgt_t *tgt;

    if (log_exec == NULL) {
        IB_FTRACE_RET_STATUS(IB_OK);
    }

    node = ib_list_last(log_exec->tgt_list);
    if ( (node == NULL) || (node->data == NULL) ) {
        IB_FTRACE_RET_STATUS(IB_OK);
    }
    tgt = node->data;
    tgt->transformed = final;

    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t ib_rule_log_exec_add_stream_tgt(ib_rule_log_exec_t *log_exec,
                                            const ib_field_t *field)
{
    IB_FTRACE_INIT();
    ib_status_t rc = IB_OK;
    ib_rule_target_t *target;
    char *fname;

    if (log_exec == NULL) {
        IB_FTRACE_RET_STATUS(IB_OK);
    }
    if (log_exec->tgt_list == NULL) {
        ++(log_exec->num_tgt);  /* Normally done by add_target() call below */
        IB_FTRACE_RET_STATUS(IB_OK);
    }

    target = ib_mpool_alloc(log_exec->log_tx->tx->mp, sizeof(*target));
    if (target == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    fname = ib_mpool_alloc(log_exec->log_tx->tx->mp, field->nlen+1);
    if (fname == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }
    strncpy(fname, field->name, field->nlen);
    *(fname + field->nlen) = '\0';
    target->field_name = fname;
    target->tfn_list = NULL;

    rc = ib_rule_log_exec_add_target(log_exec, target, field);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_rule_log_exec_add_tfn(ib_rule_log_exec_t *log_exec,
                                     const ib_tfn_t *tfn,
                                     const ib_field_t *in,
                                     const ib_field_t *out,
                                     ib_status_t status)
{
    IB_FTRACE_INIT();
    ib_status_t rc = IB_OK;
    ib_rule_log_tfn_t *object;
    ib_list_node_t *node;
    ib_rule_log_tgt_t *tgt;

    if (log_exec == NULL) {
        IB_FTRACE_RET_STATUS(IB_OK);
    }

    node = ib_list_last(log_exec->tgt_list);
    if ( (node == NULL) || (node->data == NULL) ) {
        IB_FTRACE_RET_STATUS(IB_OK);
    }
    tgt = node->data;
    if (tgt->tfn_list) {
        IB_FTRACE_RET_STATUS(IB_OK);
    }
    if (tgt->tfn_list == NULL) {
        IB_FTRACE_RET_STATUS(IB_OK);
    }

    object = (ib_rule_log_tfn_t *)
        ib_mpool_alloc(log_exec->log_tx->tx->mp, sizeof(*object));
    if (object == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }
    object->tfn = tfn;
    object->in = in;
    object->out = out;
    object->status = status;

    rc = ib_list_push(tgt->tfn_list, object);
    IB_FTRACE_RET_STATUS(rc);
}

static void count_result(ib_rule_log_count_t *counts,
                         ib_num_t result,
                         ib_status_t status)
{
    IB_FTRACE_INIT();

    if (status != IB_OK) {
        ++counts->num_errors;
    }
    else if (result) {
        ++counts->num_true;
    }
    else {
        ++counts->num_false;
    }

    IB_FTRACE_RET_VOID();
}

ib_status_t ib_rule_log_exec_add_result(ib_rule_log_exec_t *log_exec,
                                        const ib_field_t *value,
                                        ib_num_t result,
                                        ib_status_t status)
{
    IB_FTRACE_INIT();
    ib_status_t rc = IB_OK;
    ib_rule_log_rslt_t *object;
    ib_list_node_t *node;
    ib_rule_log_tgt_t *tgt;

    if (log_exec == NULL) {
        IB_FTRACE_RET_STATUS(IB_OK);
    }
    count_result(&log_exec->counts, result, status);

    node = ib_list_last(log_exec->tgt_list);
    if ( (node == NULL) || (node->data == NULL) ) {
        IB_FTRACE_RET_STATUS(IB_OK);
    }
    tgt = node->data;
    count_result(&tgt->counts, result, status);
    if (tgt->rslt_list == NULL) {
        IB_FTRACE_RET_STATUS(IB_OK);
    }

    object = (ib_rule_log_rslt_t *)
        ib_mpool_alloc(log_exec->log_tx->tx->mp, sizeof(*object));
    if (object == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }
    object->value = value;
    object->result = result;
    object->status = status;
    object->num_actions = 0;

    if (ib_flags_all(log_exec->log_tx->flags, IB_RULE_LOG_FLAG_ACTION) ) {
        rc = ib_list_create(&object->act_list, log_exec->log_tx->tx->mp);
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }
    }
    else {
        object->act_list = NULL;
    }

    rc = ib_list_push(tgt->rslt_list, object);
    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_rule_log_exec_add_action(ib_rule_log_exec_t *log_exec,
                                        const ib_action_inst_t *act_inst,
                                        ib_status_t status)
{
    IB_FTRACE_INIT();
    ib_status_t rc = IB_OK;
    ib_list_node_t *node;
    ib_rule_log_tgt_t *tgt;
    ib_rule_log_rslt_t *rslt;
    ib_rule_log_act_t *object;

    if (log_exec == NULL) {
        IB_FTRACE_RET_STATUS(IB_OK);
    }
    ++log_exec->counts.num_actions;

    node = ib_list_last(log_exec->tgt_list);
    if ( (node == NULL) || (node->data == NULL) ) {
        IB_FTRACE_RET_STATUS(IB_OK);
    }
    tgt = node->data;
    ++tgt->counts.num_actions;

    if (tgt->rslt_list == NULL) {
        IB_FTRACE_RET_STATUS(IB_OK);
    }
    ++(tgt->num_results);

    node = ib_list_last(tgt->rslt_list);
    if ( (node == NULL) || (node->data == NULL) ) {
        IB_FTRACE_RET_STATUS(IB_OK);
    }
    rslt = node->data;

    object = (ib_rule_log_act_t *)
        ib_mpool_calloc(log_exec->log_tx->tx->mp, sizeof(*object), 1);
    if (object == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }
    object->status = status;
    object->act_inst = act_inst;

    rc = ib_list_push(rslt->act_list, object);
    IB_FTRACE_RET_STATUS(rc);
}

/* Log a field's value for the rule engine */
void ib_rule_log_field(const ib_tx_t *tx,
                       const ib_rule_t *rule,
                       const ib_rule_target_t *target,
                       const ib_tfn_t *tfn,
                       const char *label,
                       const ib_field_t *f)
{
    IB_FTRACE_INIT();
    ib_status_t rc;

    if (ib_rule_debug_log_level(tx->ctx) < IB_RULE_LOG_LEVEL_TRACE) {
        IB_FTRACE_RET_VOID();
    }

    if (f->type == IB_FTYPE_NULSTR) {
        const char *s;
        rc = ib_field_value(f, ib_ftype_nulstr_out(&s));
        if (rc != IB_OK) {
            IB_FTRACE_RET_VOID();
        }
        ib_rule_log_debug(tx, rule, target, tfn, "\"%s\": \"%s\"", label, s);
    }
    else if (f->type == IB_FTYPE_BYTESTR) {
        const ib_bytestr_t *bs;
        rc = ib_field_value(f, ib_ftype_bytestr_out(&bs));
        if (rc != IB_OK) {
            IB_FTRACE_RET_VOID();
        }
        ib_rule_log_debug(tx, rule, target, tfn, "\"%s\": \"%.*s\"",
                          label,
                          (int)ib_bytestr_length(bs),
                          (const char *)ib_bytestr_const_ptr(bs));
    }
    else {
        ib_rule_log_debug(tx, rule, target, tfn,
                          "%s type = %d",
                          label, f->type);
    }
    IB_FTRACE_RET_VOID();
}

static void log_tx_start(
    const ib_rule_log_tx_t *log_tx,
    ib_tx_t *tx
)
{
    IB_FTRACE_INIT();
    if (ib_flags_all(log_tx->flags, IB_RULE_LOG_FLAG_TX)) {
        ib_log_tx_ex(tx, log_tx->level, NULL, 0,
                     "TX_START %s:%d %s",
                     tx->conn->remote_ipstr,
                     tx->conn->remote_port,
                     tx->hostname);
    }
    IB_FTRACE_RET_VOID();
}

static void log_tx_end(
    const ib_rule_log_tx_t *log_tx,
    ib_tx_t *tx
)
{
    IB_FTRACE_INIT();
    if (ib_flags_all(log_tx->flags, IB_RULE_LOG_FLAG_TX)) {
        ib_log_tx_ex(tx, log_tx->level, NULL, 0, "TX_END");
    }
    IB_FTRACE_RET_VOID();
}

static void log_tx_request_line(
    const ib_rule_log_tx_t *log_tx,
    ib_tx_t *tx
)
{
    IB_FTRACE_INIT();

    if (ib_flags_all(log_tx->flags, IB_RULE_LOG_FLAG_REQ_LINE)) {
        if (ib_bytestr_length(tx->request_line->protocol) == 0) {
            ib_log_tx_ex(tx, log_tx->level, NULL, 0,
                         "REQ_LINE %.*s %.*s",
                         IB_BYTESTR_FMT_PARAM(tx->request_line->method),
                         IB_BYTESTR_FMT_PARAM(tx->request_line->uri));
        }
        else {
            ib_log_tx_ex(tx, log_tx->level, NULL, 0,
                         "REQ_LINE %.*s %.*s %.*s",
                         IB_BYTESTR_FMT_PARAM(tx->request_line->method),
                         IB_BYTESTR_FMT_PARAM(tx->request_line->uri),
                         IB_BYTESTR_FMT_PARAM(tx->request_line->protocol));
        }
    }
    IB_FTRACE_RET_VOID();
}

static void log_tx_response_line(
    const ib_rule_log_tx_t *log_tx,
    ib_tx_t *tx
)
{
    IB_FTRACE_INIT();

    if (ib_flags_all(log_tx->flags, IB_RULE_LOG_FLAG_RSP_LINE)) {
        ib_log_tx_ex(tx, log_tx->level, NULL, 0,
                     "RES_LINE %.*s %.*s %.*s",
                     IB_BYTESTR_FMT_PARAM(tx->response_line->protocol),
                     IB_BYTESTR_FMT_PARAM(tx->response_line->status),
                     IB_BYTESTR_FMT_PARAM(tx->response_line->msg));
    }
    IB_FTRACE_RET_VOID();
}

static void log_tx_header(
    const ib_rule_log_tx_t *log_tx,
    const char *label,
    const ib_parsed_name_value_pair_list_wrapper_t *wrap
)
{
    IB_FTRACE_INIT();
    ib_parsed_name_value_pair_list_t *nvpair;

    for (nvpair = wrap->head;  nvpair != NULL;  nvpair = nvpair->next) {
        ib_log_tx_ex(log_tx->tx, log_tx->level, NULL, 0,
                     "%s %.*s %.*s",
                     label,
                     IB_BYTESTR_FMT_PARAM(nvpair->name),
                     IB_BYTESTR_FMT_PARAM(nvpair->value));
    }
}

static void log_tx_request_header(
    const ib_rule_log_tx_t *log_tx,
    ib_tx_t *tx
)
{
    IB_FTRACE_INIT();

    if (ib_flags_all(log_tx->flags, IB_RULE_LOG_FLAG_REQ_HEADER)) {
        log_tx_header(log_tx, "REQ_HEADER", tx->request_header);
    }
    IB_FTRACE_RET_VOID();
}

static void log_tx_response_header(
    const ib_rule_log_tx_t *log_tx,
    ib_tx_t *tx
)
{
    IB_FTRACE_INIT();

    if (ib_flags_all(log_tx->flags, IB_RULE_LOG_FLAG_RSP_HEADER)) {
        log_tx_header(log_tx, "RES_HEADER", tx->response_header);
    }
    IB_FTRACE_RET_VOID();
}

static void log_tx_body(
    const ib_rule_log_tx_t *log_tx,
    const char *label,
    const ib_stream_t *body
)
{
    IB_FTRACE_INIT();

    // todo
    ib_log_tx_ex(log_tx->tx, log_tx->level, NULL, 0, "%s", label);
    IB_FTRACE_RET_VOID();
}

static void log_tx_request_body(
    const ib_rule_log_tx_t *log_tx,
    ib_tx_t *tx
)
{
    IB_FTRACE_INIT();

    if (ib_flags_all(log_tx->flags, IB_RULE_LOG_FLAG_REQ_BODY)) {
        log_tx_body(log_tx, "REQ_BODY", tx->request_body);
    }
    IB_FTRACE_RET_VOID();
}

static void log_tx_response_body(
    const ib_rule_log_tx_t *log_tx,
    ib_tx_t *tx
)
{
    IB_FTRACE_INIT();

    if (ib_flags_all(log_tx->flags, IB_RULE_LOG_FLAG_RSP_BODY)) {
        log_tx_body(log_tx, "RES_BODY", tx->response_body);
    }
    IB_FTRACE_RET_VOID();
}

/* Log TX events (start of phase) */
void ib_rule_log_tx_event_start(
    const ib_rule_log_tx_t *log_tx,
    ib_state_event_type_t event
)
{
    IB_FTRACE_INIT();

    if (log_tx == NULL) {
        IB_FTRACE_RET_VOID();
    }

    switch(event) {

    case handle_request_header_event :
        log_tx_start(log_tx, log_tx->tx);
        log_tx_request_line(log_tx, log_tx->tx);
        log_tx_request_header(log_tx, log_tx->tx);
        break;

    case handle_request_event :
        log_tx_request_body(log_tx, log_tx->tx);
        break;

    case handle_response_header_event :
        log_tx_response_line(log_tx, log_tx->tx);
        log_tx_response_header(log_tx, log_tx->tx);
        break;

    case handle_response_event :
        log_tx_response_body(log_tx, log_tx->tx);
        break;

    default :
        break;       /* Do nothing */
    }
    IB_FTRACE_RET_VOID();
}

/* Log TX events (end of phase) */
void ib_rule_log_tx_event_end(
    const ib_rule_log_tx_t *log_tx,
    ib_state_event_type_t event
)
{
    IB_FTRACE_INIT();

    if (log_tx == NULL) {
        IB_FTRACE_RET_VOID();
    }

    switch(event) {
    case handle_postprocess_event :
        log_tx_end(log_tx, log_tx->tx);
        break;

    default:
        break;       /* Do nothing */
    }
    IB_FTRACE_RET_VOID();
}

/* Log phase start */
void ib_rule_log_phase(
    ib_rule_log_tx_t *log_tx,
    ib_rule_phase_num_t phase_num,
    const char *phase_name
)
{
    IB_FTRACE_INIT();

    if (log_tx == NULL) {
        IB_FTRACE_RET_VOID();
    }

    if (ib_flags_all(log_tx->flags, IB_RULE_LOG_FLAG_PHASE)) {
        if (phase_num != log_tx->last_phase) {
            ib_log_tx_ex(log_tx->tx, log_tx->level,
                         NULL, 0, "PHASE %s", phase_name);
            log_tx->last_phase = phase_num;
        }
    }
    IB_FTRACE_RET_VOID();
}

static void rule_vlog(
    const ib_rule_log_tx_t *log_tx,
    const ib_rule_log_exec_t *log_exec,
    const char *fmt,
    va_list ap
)
{
    IB_FTRACE_INIT();
    ib_rule_vlog(IB_RULE_LOG_LEVEL_ALWAYS, log_tx->tx, log_exec->rule,
                 NULL, NULL, NULL, NULL, 0, fmt, ap);

    IB_FTRACE_RET_VOID();
}

static void rule_log(
    const ib_rule_log_tx_t *log_tx,
    const ib_rule_log_exec_t *log_exec,
    const char *fmt, ...
)
{
    IB_FTRACE_INIT();

    va_list ap;

    va_start(ap, fmt);
    rule_vlog(log_tx, log_exec, fmt, ap);
    va_end(ap);

    IB_FTRACE_RET_VOID();
}

/**
 * Log a rule's transformations
 *
 * @param[in] log_tx Rule transaction logging object
 * @param[in] log_exec Rule execution logging object
 * @param[in] tgt Rule target logging object
 */
static void log_tfns(
    const ib_rule_log_tx_t *log_tx,
    const ib_rule_log_exec_t *log_exec,
    const ib_rule_log_tgt_t *tgt
)
{
    IB_FTRACE_INIT();

    assert(log_tx != NULL);
    assert(log_exec != NULL);
    assert(log_exec->rule != NULL);
    assert(tgt != NULL);
    assert(tgt->tfn_list != NULL);
    assert(log_tx->tx != NULL);

    const ib_list_node_t *tfn_node;
    char buf[MAX_FIELD_BUF+1];

    if (ib_flags_all(log_tx->flags, IB_RULE_LOG_FLAG_TFN) == false) {
        IB_FTRACE_RET_VOID();
    }

    IB_LIST_LOOP_CONST(tgt->tfn_list, tfn_node) {
        const ib_rule_log_tfn_t *tfn =
            (const ib_rule_log_tfn_t *)ib_list_node_data_const(tfn_node);

        rule_log(log_tx, log_exec, "TFN %s %.*s %s %s %s",
                 tfn->tfn->name,
                 (int)tgt->original->nlen, tgt->original->name,
                 ib_field_type_name(tgt->original->type),
                 ib_field_format(tfn->out, true, true, NULL,
                                 buf, MAX_FIELD_BUF),
                 tfn->status == IB_OK ? "" : ib_status_to_string(tfn->status));
    }

    IB_FTRACE_RET_VOID();
}

/**
 * Log a rule result
 *
 * @param[in] log_tx Rule transaction logging object
 * @param[in] log_exec Rule execution logging object
 * @param[in] rslt Rule result logging object
 */
static void log_actions(
    const ib_rule_log_tx_t *log_tx,
    const ib_rule_log_exec_t *log_exec,
    const ib_rule_log_rslt_t *rslt
)
{
    IB_FTRACE_INIT();
    const ib_list_node_t *act_node;

    assert(log_tx != NULL);
    assert(log_exec != NULL);
    assert(rslt != NULL);
    assert(rslt->act_list != NULL);

    if (ib_flags_all(log_tx->flags, IB_RULE_LOG_FLAG_ACTION) == false) {
        IB_FTRACE_RET_VOID();
    }

    IB_LIST_LOOP_CONST(rslt->act_list, act_node) {
        const ib_rule_log_act_t *act =
            (const ib_rule_log_act_t *)ib_list_node_data_const(act_node);

        rule_log(log_tx, log_exec,
                 "ACTION %s \"%s\" %s",
                 act->act_inst->action->name,
                 act->act_inst->params,
                 act->status == IB_OK ?"" : ib_status_to_string(act->status));
    }

    IB_FTRACE_RET_VOID();
}

/**
 * Log a rule result
 *
 * @param[in] log_tx Rule transaction logging object
 * @param[in] log_exec Rule execution logging object
 * @param[in] tgt Rule target logging object
 * @param[in] rslt Rule result logging object
 */
static void log_result(
    const ib_rule_log_tx_t *log_tx,
    const ib_rule_log_exec_t *log_exec,
    const ib_rule_log_tgt_t *tgt,
    const ib_rule_log_rslt_t *rslt
)
{
    IB_FTRACE_INIT();

    assert(log_tx != NULL);
    assert(log_exec != NULL);
    assert(tgt != NULL);
    assert(rslt != NULL);
    char buf[MAX_FIELD_BUF+1];

    if(tgt->tfn_list != NULL) {
        log_tfns(log_tx, log_exec, tgt);
    }

    if (rslt->value == NULL) {
        rule_log(log_tx, log_exec,
                 "RULE_DATA %.*s %s %s",
                 tgt->original == NULL ? 4 : (int)tgt->original->nlen,
                 tgt->original == NULL ? "None" : tgt->original->name,
                 "NULL", "NULL");
    }
    else {
        rule_log(log_tx, log_exec,
                 "RULE_DATA %.*s %s %s",
                 (int)rslt->value->nlen, rslt->value->name,
                 ib_field_type_name(rslt->value->type),
                 ib_field_format(rslt->value, true, true, NULL,
                                 buf, MAX_FIELD_BUF));
    }

    if (ib_flags_all(log_tx->flags, IB_RULE_LOG_FLAG_OPERATOR) ) {
        rule_log(log_tx, log_exec, "OP %s \"%s\" %s %ld %s",
                 log_exec->rule->opinst->op->name,
                 log_exec->rule->opinst->params,
                 ib_field_format(rslt->value, true, true, NULL,
                                 buf, MAX_FIELD_BUF),
                 (long int)rslt->result,
                 (rslt->status == IB_OK ? "" :
                  ib_status_to_string(rslt->status)));
    }

    if (rslt->act_list != NULL) {
        log_actions(log_tx, log_exec, rslt);
    }

    IB_FTRACE_RET_VOID();
}

static int get_count(
    const ib_rule_log_tx_t *log_tx,
    const ib_rule_log_count_t *counts
)
{
    IB_FTRACE_INIT();

    switch(log_tx->mode) {
    case IB_RULE_LOG_MODE_ACT:
        IB_FTRACE_RET_INT(counts->num_actions);
    case IB_RULE_LOG_MODE_ERROR:
        IB_FTRACE_RET_INT(counts->num_errors);
    case IB_RULE_LOG_MODE_TRUE:
        IB_FTRACE_RET_INT(counts->num_true);
    case IB_RULE_LOG_MODE_FALSE:
        IB_FTRACE_RET_INT(counts->num_false);
    case IB_RULE_LOG_MODE_ALL:
    default:
        IB_FTRACE_RET_INT(1);
    }
}

/* Log rule execution: exec. */
void ib_rule_log_exec_ex(
    const ib_rule_log_exec_t *log_exec
)
{
    IB_FTRACE_INIT();
    const ib_list_node_t *tgt_node;
    const ib_rule_log_tx_t *log_tx;
    const ib_rule_t *rule;

    if ( (log_exec == NULL) || (log_exec->rule == NULL) ) {
        IB_FTRACE_RET_VOID();
    }

    log_tx = log_exec->log_tx;
    if (get_count(log_tx, &log_exec->counts) == 0) {
        IB_FTRACE_RET_VOID();
    }

    rule = log_exec->rule;

    /*
     * Log the rule start and/or data
     */
    if (ib_flags_all(log_tx->flags, IB_RULE_LOG_FLAG_RULE)) {
        const char *rule_type;
        if (ib_flags_all(log_exec->rule->flags, IB_RULE_FLAG_STREAM)) {
            rule_type = "STREAM";
        }
        else if (ib_flags_all(log_exec->rule->flags, IB_RULE_FLAG_EXTERNAL)) {
            rule_type = "EXTERNAL";
        }
        else {
            rule_type = "PHASE";
        }

        rule_log(log_tx, log_exec, "RULE_START %s", rule_type);
    }

    /*
     * Log all of the targets whose result that matched the result type.
     */
    if (log_exec->tgt_list != NULL) {
        IB_LIST_LOOP_CONST(log_exec->tgt_list, tgt_node) {
            const ib_rule_log_tgt_t *tgt =
                (const ib_rule_log_tgt_t *)tgt_node->data;
            const ib_list_node_t *rslt_node;
            assert(tgt != NULL);

            if (get_count(log_tx, &tgt->counts) == 0) {
                continue;
            }

            if (ib_flags_all(log_tx->flags, IB_RULE_LOG_FLAG_RULE_DATA)) {
                bool allow_null = ib_flags_all(rule->opinst->op->flags,
                                               IB_OP_FLAG_ALLOW_NULL);
                if ( (tgt->original == NULL) && (allow_null == false) ) {
                    rule_log(log_tx, log_exec,
                             "RULE_DATA %s NOT_FOUND",
                             tgt->target->field_name);
                }
            }

            if (tgt->rslt_list != NULL) {
                IB_LIST_LOOP_CONST(tgt->rslt_list, rslt_node) {
                    const ib_rule_log_rslt_t *rslt =
                        (const ib_rule_log_rslt_t *)rslt_node->data;
                    log_result(log_tx, log_exec, tgt, rslt);
                }
            }
        }
    }

    if (ib_flags_all(log_tx->flags, IB_RULE_LOG_FLAG_RULE)) {
        rule_log(log_tx, log_exec, "RULE_END");
    }

    IB_FTRACE_RET_VOID();
}

/* Generic Logger for rules. */
void ib_rule_vlog(
    ib_rule_log_level_t level,
    const ib_tx_t *tx,
    const ib_rule_t *rule,
    const ib_rule_target_t *target,
    const ib_tfn_t *tfn,
    const char *prefix,
    const char *file,
    int line,
    const char *fmt,
    va_list ap
)
{
    IB_FTRACE_INIT();
    char *fmtbuf = NULL;
    size_t fmtlen = PREFIX_BUFSIZE + 1;
    void *freeptr = NULL;
    bool log_opinst = false;

    /* Ignore this message? */
    if (level > ib_rule_debug_log_level(tx->ctx)) {
        IB_FTRACE_RET_VOID();
    }

    /* Build a new format buffer with rule ID and target field name */

    /* Calculate the prefix length */
    if (rule != NULL) {
        fmtlen += strlen(rule->meta.id) + 10;
        if (level >= IB_RULE_LOG_LEVEL_DEBUG) {
            log_opinst = true;
            fmtlen += strlen(rule->opinst->op->name) + 12;
        }
    }
    if (target != NULL) {
        fmtlen += strlen(target->field_name) + 10;
    }
    if (tfn != NULL) {
        fmtlen += strlen(tfn->name) + 10;
    }

    /* Using the length, build a new prefix buffer */
    fmtlen += strlen(fmt) + 4;
    fmtbuf = malloc(fmtlen);

    if (fmtbuf != NULL) {
        bool first = true;

        strcpy(fmtbuf, "[");

        /* Add the rule and operator name */
        if (rule != NULL) {
            char revbuf[REV_BUFSIZE+1];

            snprintf(revbuf, REV_BUFSIZE, "%d", rule->meta.revision);

            strcat(fmtbuf, "rule:\"");
            strcat(fmtbuf, rule->meta.id);
            strcat(fmtbuf, "\" rev:");
            strcat(fmtbuf, revbuf);

            if (log_opinst) {
                strcat(fmtbuf, " operator:\"");
                strcat(fmtbuf, rule->opinst->op->name);
                strcat(fmtbuf, "\"");
            }
            first = false;
        }

        /* Add the target field name */
        if (target != NULL) {
            if (! first) {
                strcat(fmtbuf, " ");
            }
            else {
                first = false;
            }
            strcat(fmtbuf, "target:\"");
            strcat(fmtbuf, target->field_name);
            strcat(fmtbuf, "\"");
        }

        /* Add the transformation name */
        if (tfn != NULL) {
            if (! first) {
                strcat(fmtbuf, " ");
            }
            strcat(fmtbuf, "tfn:\"");
            strcat(fmtbuf, tfn->name);
            strcat(fmtbuf, "\"");
        }
        strcat(fmtbuf, "] ");
        strcat(fmtbuf, fmt);
        fmt = fmtbuf;
        freeptr = fmtbuf;
    }

    ib_vlog_tx_ex(tx, ib_rule_log_level(tx->ctx), file, line, fmt, ap);

    if (freeptr != NULL) {
        free(freeptr);
    }

    IB_FTRACE_RET_VOID();
}

/* Generic Logger for rules. */
void ib_rule_log(
    ib_rule_log_level_t level,
    const ib_tx_t *tx,
    const ib_rule_t *rule,
    const ib_rule_target_t *target,
    const ib_tfn_t *tfn,
    const char *prefix,
    const char *file,
    int line,
    const char *fmt, ...
)
{
    IB_FTRACE_INIT();

    va_list ap;

    va_start(ap, fmt);
    ib_rule_vlog(level, tx, rule, target, tfn, prefix, file, line, fmt, ap);
    va_end(ap);

    IB_FTRACE_RET_VOID();
}
