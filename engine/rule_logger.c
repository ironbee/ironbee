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
 * @brief IronBee
 *
 * @author Nick LeRoy <nleroy@qualys.com>
 */

#include "ironbee_config_auto.h"

#include <assert.h>
#include <inttypes.h>

#include <ironbee/bytestr.h>
#include <ironbee/core.h>
#include <ironbee/util.h>
#include <ironbee/field.h>
#include <ironbee/debug.h>
#include <ironbee/mpool.h>
#include <ironbee/operator.h>
#include <ironbee/action.h>
#include <ironbee/transformation.h>
#include <ironbee/rule_engine.h>

#include <ironbee/debug.h>
#include <ironbee/mpool.h>

#include "rule_engine_private.h"

/**
 * Prefix used for all rule engine log entries.
 */
static const char *LOG_PREFIX = "RULE_ENG";

/**
 * Length of buffer used for formatting fields
 */
static size_t MAX_FIELD_BUF = 64;

/**
 * Format a field into a string buffer
 *
 * @param field Input field
 * @param buf Buffer for output
 * @param bufsize Size of @a buf
 *
 * @returns @a buf
 */
static const char *format_field(const ib_field_t *field,
                                char *buf,
                                size_t bufsize)
{
    IB_FTRACE_INIT();
    ib_status_t rc;

    assert(buf != NULL);
    assert(bufsize > 0);

    *buf = '\0';
    if (field != NULL) {
        switch (field->type) {

        case IB_FTYPE_NULSTR :
        {
            const char *s;
            rc = ib_field_value(field, ib_ftype_nulstr_out(&s));
            if (rc != IB_OK) {
                break;
            }
            strncpy(buf, s, bufsize-1);
            *(buf+bufsize-1) = '\0';
            break;
        }

        case IB_FTYPE_BYTESTR:
        {
            const ib_bytestr_t *bs;
            size_t len;

            rc = ib_field_value(field, ib_ftype_bytestr_out(&bs));
            if (rc != IB_OK) {
                break;
            }
            len = ib_bytestr_length(bs);
            if (len > (bufsize - 1) ) {
                len = bufsize - 1;
            }
            strncpy(buf, (const char *)ib_bytestr_const_ptr(bs), len);
            *(buf+len) = '\0';
            break;
        }

        case IB_FTYPE_NUM :          /**< Numeric value */
        {
            ib_num_t n;
            rc = ib_field_value(field, ib_ftype_num_out(&n));
            if (rc != IB_OK) {
                break;
            }
            snprintf(buf, bufsize, "%"PRId64, n);
            break;
        }

        case IB_FTYPE_UNUM :         /**< Unsigned numeric value */
        {
            ib_unum_t u;
            rc = ib_field_value(field, ib_ftype_unum_out(&u));
            if (rc != IB_OK) {
                break;
            }
            snprintf(buf, bufsize, "%"PRIu64, u);
            break;
        }

        case IB_FTYPE_LIST :         /**< List */
        {
            const ib_list_t *lst;
            size_t len;
            rc = ib_field_value(field, ib_ftype_list_out(&lst));
            if (rc != IB_OK) {
                break;
            }
            len = IB_LIST_ELEMENTS(lst);
            if (len == 0) {
                snprintf(buf, bufsize, "list[%zd]", len);
            }
            else {
                const ib_list_node_t *node;
                node = ib_list_last_const(lst);
                if (node == NULL) {
                    snprintf(buf, bufsize, "list[%zd]", len);
                }
                else {
                    format_field((const ib_field_t *)node->data, buf, bufsize);
                }
            }
            break;
        }

        default:
            snprintf(buf, bufsize, "type = %d", field->type);
            break;
        }
    }
    IB_FTRACE_RET_CONSTSTR(buf);
}

const char *ib_rule_log_mode_str(ib_rule_log_mode_t mode)
{
    IB_FTRACE_INIT();
    switch (mode) {
    case IB_RULE_LOG_MODE_OFF :
        IB_FTRACE_RET_CONSTSTR("None");
    case IB_RULE_LOG_MODE_FAST :
        IB_FTRACE_RET_CONSTSTR("Fast");
    case IB_RULE_LOG_MODE_EXEC :
        IB_FTRACE_RET_CONSTSTR("RuleExec");
    }
    IB_FTRACE_RET_CONSTSTR("<Invalid>");
}

ib_rule_log_mode_t ib_rule_log_mode(const ib_engine_t *ib)
{
    IB_FTRACE_INIT();
    ib_core_cfg_t *corecfg = NULL;
    ib_context_module_config(ib_context_main(ib),
                             ib_core_module(),
                             (void *)&corecfg);
    IB_FTRACE_RET_INT(corecfg->rule_log_mode);
}

ib_flags_t ib_rule_log_flags(const ib_engine_t *ib)
{
    IB_FTRACE_INIT();
    ib_core_cfg_t *corecfg = NULL;
    ib_context_module_config(ib_context_main(ib),
                             ib_core_module(),
                             (void *)&corecfg);
    IB_FTRACE_RET_INT(corecfg->rule_log_flags);
}

ib_rule_log_level_t ib_rule_log_level(const ib_engine_t *ib)
{
    IB_FTRACE_INIT();
    ib_core_cfg_t *corecfg = NULL;
    ib_context_module_config(ib_context_main(ib),
                             ib_core_module(),
                             (void *)&corecfg);
    IB_FTRACE_RET_INT(corecfg->rule_log_level);
}

ib_status_t ib_rule_log_exec_create(ib_tx_t *tx,
                                    const ib_rule_t *rule,
                                    ib_rule_log_exec_t **log_exec)
{
    IB_FTRACE_INIT();
    ib_status_t rc;
    ib_rule_log_mode_t mode;
    ib_flags_t flags;
    ib_rule_log_exec_t *new;

    assert(tx != NULL);
    assert(rule != NULL);
    assert(log_exec != NULL);

    *log_exec = NULL;

    /* Get the mode & flags */
    mode = (ib_rule_log_mode_t)ib_rule_log_mode(tx->ib);
    if (mode == IB_RULE_LOG_MODE_OFF) {
        IB_FTRACE_RET_STATUS(IB_OK);
    }
    flags = (ib_flags_t)ib_rule_log_flags(tx->ib);

    /* Allocate the object */
    new = ib_mpool_calloc(tx->mp, sizeof(*new), 1);
    if (new == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    /* Create the target list */
    rc = ib_list_create(&(new->tgt_list), tx->mp);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Complete the new object, store pointer to it */
    new->flags = flags;
    new->mode = mode;
    new->tx = tx;
    new->rule = rule;
    *log_exec = new;

    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t ib_rule_log_exec_add_tgt(ib_rule_log_exec_t *log_exec,
                                     const ib_rule_target_t *target,
                                     const ib_field_t *original)
{
    IB_FTRACE_INIT();
    ib_status_t rc = IB_OK;
    ib_rule_log_tgt_t *tgt;

    if ( (log_exec == NULL) || (log_exec->tgt_list == NULL) ) {
        IB_FTRACE_RET_STATUS(IB_OK);
    }

    tgt = (ib_rule_log_tgt_t *)
        ib_mpool_calloc(log_exec->tx->mp, sizeof(*tgt), 1);
    if (tgt == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }
    tgt->target = target;
    tgt->original = original;
    tgt->transformed = NULL;

    /* Initialize the result list */
    rc = ib_list_create(&tgt->rslt_list, log_exec->tx->mp);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Initialize the transformation list */
    rc = ib_list_create(&tgt->tfn_list, log_exec->tx->mp);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = ib_list_push(log_exec->tgt_list, tgt);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }
    log_exec->tgt_cur = tgt;
    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_rule_log_exec_set_tgt_final(ib_rule_log_exec_t *log_exec,
                                           const ib_field_t *final)
{
    IB_FTRACE_INIT();

    if ( (log_exec == NULL) || (log_exec->tgt_cur == NULL) ) {
        IB_FTRACE_RET_STATUS(IB_OK);
    }
    log_exec->tgt_cur->transformed = final;

    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t ib_rule_log_exec_add_stream_tgt(ib_rule_log_exec_t *log_exec,
                                            const ib_field_t *field)
{
    IB_FTRACE_INIT();
    ib_status_t rc = IB_OK;
    ib_rule_target_t *target;
    char *fname;

    if ( (log_exec == NULL) || (log_exec->tgt_list == NULL) ) {
        IB_FTRACE_RET_STATUS(IB_OK);
    }

    target = ib_mpool_alloc(log_exec->tx->mp, sizeof(*target));
    if (target == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    target = ib_mpool_alloc(log_exec->tx->mp, sizeof(*target));
    if (target == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }
    fname = ib_mpool_alloc(log_exec->tx->mp, field->nlen+1);
    if (fname == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }
    strncpy(fname, field->name, field->nlen);
    *(fname + field->nlen) = '\0';
    target->field_name = fname;
    target->tfn_list = NULL;

    rc = ib_rule_log_exec_add_tgt(log_exec, target, field);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_rule_log_exec_add_tfn(ib_rule_log_exec_t *log_exec,
                                     const ib_tfn_t *tfn,
                                     const ib_field_t *in,
                                     const ib_field_t *out)
{
    IB_FTRACE_INIT();
    ib_status_t rc = IB_OK;
    ib_rule_log_tfn_t *object;

    if ( (log_exec == NULL) ||
         (log_exec->tgt_cur == NULL) ||
         (log_exec->tgt_cur->tfn_list == NULL) )
    {
        IB_FTRACE_RET_STATUS(IB_OK);
    }

    object = (ib_rule_log_tfn_t *)
        ib_mpool_alloc(log_exec->tx->mp, sizeof(*object));
    if (object == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }
    object->tfn = tfn;
    object->in = in;
    object->out = out;

    rc = ib_list_push(log_exec->tgt_cur->tfn_list, object);
    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_rule_log_exec_add_result(ib_rule_log_exec_t *log_exec,
                                        const ib_field_t *value,
                                        ib_num_t result,
                                        const ib_list_t *actions)
{
    IB_FTRACE_INIT();
    ib_status_t rc = IB_OK;
    ib_rule_log_rslt_t *object;

    if ( (log_exec == NULL) ||
         (log_exec->tgt_cur == NULL) ||
         (log_exec->tgt_cur->rslt_list == NULL) )
    {
        IB_FTRACE_RET_STATUS(IB_OK);
    }

    object = (ib_rule_log_rslt_t *)
        ib_mpool_alloc(log_exec->tx->mp, sizeof(*object));
    if (object == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }
    object->value = value;
    object->result = result;
    object->act_list = actions;

    rc = ib_list_push(log_exec->tgt_cur->rslt_list, object);
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

    if (ib_rule_log_level(tx->ib) < IB_RULE_LOG_LEVEL_TRACE) {
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

/* Generic Logger for rules. */
static const size_t prebuf_size = 32;
void ib_rule_vlog(ib_rule_log_level_t level,
                  const ib_tx_t *tx,
                  const ib_rule_t *rule,
                  const ib_rule_target_t *target,
                  const ib_tfn_t *tfn,
                  const char *prefix,
                  const char *file,
                  int line,
                  const char *fmt,
                  va_list ap)
{
    IB_FTRACE_INIT();
    char *fmtbuf = NULL;
    size_t fmtlen = prebuf_size + 1;
    void *freeptr = NULL;
    bool log_opinst = false;

    /* Ignore this message? */
    if (level > ib_rule_log_level(tx->ib)) {
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

    snprintf(fmtbuf, fmtlen, "%s/%s", LOG_PREFIX, prefix);

    if (fmtbuf != NULL) {
        bool first = true;

        strcpy(fmtbuf, "[");

        /* Add the rule and operator name */
        if (rule != NULL) {
            strcat(fmtbuf, "rule:\"");
            strcat(fmtbuf, rule->meta.id);
            strcat(fmtbuf, "\"");
            if (log_opinst == true) {
                strcat(fmtbuf, " operator:\"");
                strcat(fmtbuf, rule->opinst->op->name);
                strcat(fmtbuf, "\"");
            }
            first = false;
        }

        /* Add the target field name */
        if (target != NULL) {
            if (first != true) {
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
            if (first != true) {
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

    ib_vlog_tx_ex(tx, IB_LOG_INFO, file, line, fmt, ap);

    if (freeptr != NULL) {
        free(freeptr);
    }

    IB_FTRACE_RET_VOID();
}

/* Generic Logger for rules. */
void ib_rule_log(ib_rule_log_level_t level,
                 const ib_tx_t *tx,
                 const ib_rule_t *rule,
                 const ib_rule_target_t *target,
                 const ib_tfn_t *tfn,
                 const char *prefix,
                 const char *file,
                 int line,
                 const char *fmt, ...)
{
    IB_FTRACE_INIT();

    va_list ap;

    va_start(ap, fmt);
    ib_rule_vlog(level, tx, rule, target, tfn, prefix, file, line, fmt, ap);
    va_end(ap);

    IB_FTRACE_RET_VOID();
}

/**
 * Log rule execution: Full flag check
 *
 * @param[in] log_exec Execution logging data
 *
 * @returns true / false
 */
static bool log_exec_flag_full(const ib_rule_log_exec_t *log_exec)
{
    return ib_flags_all(log_exec->flags, IB_RULE_LOG_FLAG_FULL);
}

/**
 * Log rule execution: Debug flag check
 *
 * @param[in] log_exec Execution logging data
 *
 * @returns true / false
 */
static bool log_exec_flag_debug(const ib_rule_log_exec_t *log_exec)
{
    return ib_flags_all(log_exec->flags, IB_RULE_LOG_FLAG_DEBUG);
}

/**
 * Log rule execution: Trace flag check
 *
 * @param[in] log_exec Execution logging data
 *
 * @returns true / false
 */
static bool log_exec_flag_trace(const ib_rule_log_exec_t *log_exec)
{
    return ib_flags_all(log_exec->flags, IB_RULE_LOG_FLAG_TRACE);
}

/**
 * Build action list buffer
 *
 * @param[in] act_list Action list
 * @param[in,out] buf Buffer to fill (must be initialized with '\\0')
 * @param[in] bufsize of @a buf
 */
static void build_act_buf(const ib_rule_log_rslt_t *rslt,
                          char *buf,
                          size_t bufsize)
{
    IB_FTRACE_INIT();

    assert(rslt != NULL);
    assert(rslt->act_list != NULL);

    const ib_list_node_t *node;
    char *cur = buf;
    size_t remain = bufsize - strlen(buf);
    bool first = true;

    /*
     * Make a string out of the action list (think Perl's "join").
     */
    IB_LIST_LOOP_CONST(rslt->act_list, node) {
        const ib_action_inst_t *action =
            (const ib_action_inst_t *)ib_list_node_data_const(node);

        /* For the second and following actions, add a comma to the string */
        if (first == false) {
            strncpy(cur, ",", remain);
            ++cur;
            --remain;
        }
        else {
            first = false;
        }

        /* Add the name of the action, with an optional "!" prefix */
        if (remain >= 2) {
            size_t len = strlen(action->action->name);
            if (rslt->result == 0) {
                strncpy(cur, "!", remain);
                ++cur;
                --remain;
            }
            strncpy(cur, action->action->name, remain);
            cur += len;
            remain -= len;
        }

        /* If we've filled our string, get out */
        if (remain < 2) {
            break;
        }
    }
    if (*buf == '\0') {
        strcpy(buf, "<NONE>");
    }

    IB_FTRACE_RET_VOID();
}

/**
 * Max action buffer
 */
const size_t MAX_ACTBUF = 128;

/**
 * Log rule execution: fast mode + full.
 *
 * @param[in] log_exec Execution logging data
 * @param[in] file Source file name (or NULL)
 * @param[in] line Source file line number (or 0)
 *
 * @note: Format: site-id rIP:rPort tx-time-delta ruleid: target="target-name"
 *                op="op-name" actions=actionname1,action-name2,...
 */
static void log_exec_fast_full(const ib_rule_log_exec_t *log_exec,
                               const char *file,
                               int line)
{
    IB_FTRACE_INIT();

    assert(log_exec != NULL);
    assert(log_exec->mode == IB_RULE_LOG_MODE_FAST);
    assert(log_exec->rule != NULL);
    assert(log_exec->tgt_list != NULL);

    const ib_list_node_t *tgt_node;
    ib_tx_t *tx = log_exec->tx;
    const ib_rule_t *rule = log_exec->rule;
    ib_time_t now = ib_clock_get_time();
    char actbuf[MAX_ACTBUF + 1];

    /*
     * Log all of the targets whose result that matched the result type.
     */
    IB_LIST_LOOP_CONST(log_exec->tgt_list, tgt_node) {
        const ib_rule_log_tgt_t *tgt =
            (const ib_rule_log_tgt_t *)tgt_node->data;
        const ib_list_node_t *rslt_node;
        assert(tgt != NULL);

        IB_LIST_LOOP_CONST(tgt->rslt_list, rslt_node) {
            const ib_rule_log_rslt_t *rslt =
                (const ib_rule_log_rslt_t *)rslt_node->data;
            assert(rslt != NULL);
            const char *name;
            size_t nlen;

            /* Only log rule targets that caused an action to execute */
            if ( (rslt->act_list == NULL) ||
                 (ib_list_elements(rslt->act_list) == 0) )
            {
                continue;
            }
            if (rslt->value == NULL) {
                name = "";
                nlen = 0;
            }
            else {
                name = rslt->value->name;
                nlen = rslt->value->nlen;
            }

            actbuf[0] = '\0';
            build_act_buf(rslt, actbuf, MAX_ACTBUF);
            ib_log_tx_ex(tx, IB_LOG_INFO, file, line,
                         "%s %s:%d %"PRIu64"us %s target=\"%s\" "
                         "op=\"%s\" "
                         "field=\"%.*s\" "
                         "actions=%s",
                         LOG_PREFIX,
                         tx->er_ipstr, tx->conn->remote_port,
                         now - tx->t.started,
                         rule->meta.id,
                         tgt->target->field_name,
                         rule->opinst->op->name,
                         (int)nlen, name,
                         actbuf);
        }
    }

    IB_FTRACE_RET_VOID();
}

/**
 * Log rule execution: fast mode.
 *
 * @param[in] log_exec Execution logging data
 * @param[in] file Source file name (or NULL)
 * @param[in] line Source file line number (or 0)
 *
 * @note: Format: site-id rIP:rPort tx-time-delta ruleid:
 *                actions=actionname1,action-name2,...
 */
static void log_exec_fast(const ib_rule_log_exec_t *log_exec,
                          const char *file,
                          int line)
{
    IB_FTRACE_INIT();

    assert(log_exec != NULL);
    assert(log_exec->tx != NULL);
    assert(log_exec->rule != NULL);
    assert(log_exec->mode == IB_RULE_LOG_MODE_FAST);
    assert(log_exec->tgt_list != NULL);

    ib_tx_t *tx = log_exec->tx;
    const ib_rule_t *rule = log_exec->rule;
    ib_time_t now;
    char actbuf[MAX_ACTBUF + 1];
    size_t num_actions = 0;
    const ib_list_node_t *tgt_node;

    /* If no actions, just return */
    actbuf[0] = '\0';
    IB_LIST_LOOP_CONST(log_exec->tgt_list, tgt_node) {
        const ib_rule_log_tgt_t *tgt =
            (const ib_rule_log_tgt_t *)tgt_node->data;
        const ib_list_node_t *rslt_node;
        assert(tgt != NULL);

        IB_LIST_LOOP_CONST(tgt->rslt_list, rslt_node) {
            const ib_rule_log_rslt_t *rslt =
                (const ib_rule_log_rslt_t *)rslt_node->data;
            assert(rslt != NULL);

            size_t n = ib_list_elements(rslt->act_list);
            if (n != 0) {
                num_actions += n;
                build_act_buf(rslt, actbuf, MAX_ACTBUF);
            }
        }
    }
    if (num_actions == 0) {
        IB_FTRACE_RET_VOID();
    }

    now = ib_clock_get_time();
    ib_log_tx_ex(tx, IB_LOG_INFO, file, line,
                   "%s %s:%d %"PRIu64"us %s actions=%s",
                   LOG_PREFIX,
                   tx->er_ipstr, tx->conn->remote_port,
                   now - tx->t.started,
                   rule->meta.id,
                   actbuf);

    IB_FTRACE_RET_VOID();
}

static void log_tfns(const ib_rule_log_exec_t *log_exec,
                     const ib_rule_log_tgt_t *tgt,
                     const ib_rule_log_rslt_t *rslt,
                     const char *file,
                     int line)
{
    IB_FTRACE_INIT();

    assert(log_exec != NULL);
    assert(log_exec->tx != NULL);
    assert(log_exec->rule != NULL);
    assert(tgt != NULL);
    assert(tgt->tfn_list != NULL);
    assert(rslt != NULL);

    ib_tx_t *tx = log_exec->tx;
    const ib_list_node_t *tfn_node;
    char outbuf[MAX_FIELD_BUF];

    /* If the debug flag is set, log all of the transformations */
    if (log_exec_flag_debug(log_exec) != true) {
        IB_FTRACE_RET_VOID();
    }

    if (ib_list_elements(tgt->tfn_list) == 0) {
        ib_log_tx_ex(tx, IB_LOG_INFO, file, line,
                     "%s %s:%d \"%s\" target=\"%s\" \"%s\" No tranformations",
                     LOG_PREFIX,
                     tx->er_ipstr,
                     tx->conn->remote_port,
                     log_exec->rule->meta.id,
                     tgt->target->field_name,
                     format_field(tgt->original, outbuf, MAX_FIELD_BUF));
        IB_FTRACE_RET_VOID();
    }

    IB_LIST_LOOP_CONST(tgt->tfn_list, tfn_node) {
        char inbuf[MAX_FIELD_BUF];
        const ib_rule_log_tfn_t *tfn =
            (const ib_rule_log_tfn_t *)ib_list_node_data_const(tfn_node);

        ib_log_tx_ex(tx, IB_LOG_INFO, file, line,
                     "%s %s:%d \"%s\" target=\"%s\" tfn=\"%s\" "
                     "\"%s\" -> \"%s\"",
                     LOG_PREFIX,
                     tx->er_ipstr,
                     tx->conn->remote_port,
                     log_exec->rule->meta.id,
                     tgt->target->field_name,
                     tfn->tfn->name,
                     format_field(tfn->in, inbuf, MAX_FIELD_BUF),
                     format_field(tfn->out, outbuf, MAX_FIELD_BUF));
    }

    IB_FTRACE_RET_VOID();
}

static void log_actions(const ib_rule_log_exec_t *log_exec,
                        const ib_rule_log_tgt_t *tgt,
                        const ib_rule_log_rslt_t *rslt,
                        const char *file,
                        int line)
{
    IB_FTRACE_INIT();

    assert(log_exec != NULL);
    assert(log_exec->tx != NULL);
    assert(log_exec->rule != NULL);
    assert(tgt != NULL);
    assert(rslt != NULL);

    ib_tx_t *tx = log_exec->tx;
    const ib_list_node_t *act_node;

    if (IB_LIST_ELEMENTS(rslt->act_list) == 0) {
        ib_log_tx_ex(tx, IB_LOG_INFO, file, line,
                     "%s %s:%d \"%s\" target=\"%s\" op=\"%s\" "
                     "result %" PRIu64 "; "
                     "no actions executed",
                     LOG_PREFIX,
                     tx->er_ipstr,
                     tx->conn->remote_port,
                     log_exec->rule->meta.id,
                     tgt->target->field_name,
                     log_exec->rule->opinst->op->name,
                     rslt->result);
        IB_FTRACE_RET_VOID();
    }

    IB_LIST_LOOP_CONST(rslt->act_list, act_node) {
        const ib_action_inst_t *action =
            (const ib_action_inst_t *)ib_list_node_data_const(act_node);

        ib_log_tx_ex(tx, IB_LOG_INFO, file, line,
                     "%s %s:%d \"%s\" target=\"%s\" op=\"%s\" "
                     "result %" PRIu64 "; action \"%s%s\" executed",
                     LOG_PREFIX,
                     tx->er_ipstr,
                     tx->conn->remote_port,
                     log_exec->rule->meta.id,
                     tgt->target->field_name,
                     log_exec->rule->opinst->op->name,
                     rslt->result,
                     rslt->result == 0 ? "!" : "",
                     action->action->name);
    }

    IB_FTRACE_RET_VOID();
}

/* Log rule execution: Normal mode +full. */
static void log_exec_normal_full(const ib_rule_log_exec_t *log_exec,
                                 const char *file,
                                 int line)
{
    IB_FTRACE_INIT();

    assert(log_exec != NULL);
    assert(log_exec->rule != NULL);
    assert(log_exec->mode == IB_RULE_LOG_MODE_EXEC);
    assert(log_exec->tgt_list != NULL);

    const ib_list_node_t *tgt_node;

    /*
     * Log all of the targets whose result that matched the result type.
     */
    IB_LIST_LOOP_CONST(log_exec->tgt_list, tgt_node) {
        const ib_rule_log_tgt_t *tgt =
            (const ib_rule_log_tgt_t *)tgt_node->data;
        const ib_list_node_t *rslt_node;
        assert(tgt != NULL);

        IB_LIST_LOOP_CONST(tgt->rslt_list, rslt_node) {
            const ib_rule_log_rslt_t *rslt =
                (const ib_rule_log_rslt_t *)rslt_node->data;
            assert(rslt != NULL);

            log_tfns(log_exec, tgt, rslt, file, line);
            log_actions(log_exec, tgt, rslt, file, line);
        }
    }

    IB_FTRACE_RET_VOID();
}

/* Log rule execution: Normal mode. */
static void log_exec_normal(const ib_rule_log_exec_t *log_exec,
                            const char *file,
                            int line)
{
    IB_FTRACE_INIT();

    assert(log_exec != NULL);
    assert(log_exec->mode == IB_RULE_LOG_MODE_EXEC);
    assert(log_exec->tgt_list != NULL);

    ib_tx_t *tx = log_exec->tx;
    const ib_rule_t *rule = log_exec->rule;
    const ib_list_node_t *tgt_node;

    /*
     * Log all of the targets whose result that matched the result type.
     */
    IB_LIST_LOOP_CONST(log_exec->tgt_list, tgt_node) {
        const ib_rule_log_tgt_t *tgt =
            (const ib_rule_log_tgt_t *)tgt_node->data;
        const ib_list_node_t *rslt_node;
        assert(tgt != NULL);

        IB_LIST_LOOP_CONST(tgt->rslt_list, rslt_node) {
            const ib_list_node_t *act_node;
            const ib_rule_log_rslt_t *rslt =
                (const ib_rule_log_rslt_t *)rslt_node->data;
            assert(rslt != NULL);

            IB_LIST_LOOP_CONST(rslt->act_list, act_node) {
                const ib_action_inst_t *action =
                    (const ib_action_inst_t *)ib_list_node_data_const(act_node);

                ib_log_tx_ex(tx, IB_LOG_INFO, file, line,
                             "%s %s:%d \"%s\" result %" PRIu64 "; "
                             "action \"%s%s\" executed",
                             LOG_PREFIX,
                             tx->er_ipstr,
                             tx->conn->remote_port,
                             rule->meta.id,
                             log_exec->result,
                             log_exec->result == 0 ? "!" : "",
                             action->action->name);
            }
        }
    }

    IB_FTRACE_RET_VOID();
}

/* Log rule execution: exec. */
void ib_rule_log_exec_ex(const ib_rule_log_exec_t *log_exec,
                         const char *file,
                         int line)
{
    IB_FTRACE_INIT();

    if ( (log_exec == NULL) ||
         (log_exec->rule == NULL) ||
         (log_exec->tgt_list == NULL) )
    {
        IB_FTRACE_RET_VOID();
    }

    /* Remove source file info if Trace isn't enabled */
    if (log_exec_flag_trace(log_exec) == false) {
        file = NULL;
        line = 0;
    }

    switch (log_exec->mode) {
    case IB_RULE_LOG_MODE_OFF:
        break;

    case IB_RULE_LOG_MODE_FAST:
        if (log_exec_flag_full(log_exec) == true) {
            log_exec_fast_full(log_exec, file, line);
        }
        else {
            log_exec_fast(log_exec, file, line);
        }
        break;

    case IB_RULE_LOG_MODE_EXEC:
        if (log_exec_flag_full(log_exec) == true) {
            log_exec_normal_full(log_exec, file, line);
        }
        else {
            log_exec_normal(log_exec, file, line);
        }
        break;

    default:
        assert(0 && "Invalid rule log level");
        break;
    }

    IB_FTRACE_RET_VOID();
}
