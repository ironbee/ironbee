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

#include "ironbee_private.h"

/**
 * Prefix used for all rule engine log entries.
 */
static const char *LOG_PREFIX = "RULE_ENG";

ib_rule_log_level_t ib_rule_log_level(const ib_engine_t *ib)
{
    IB_FTRACE_INIT();
    ib_core_cfg_t *corecfg = NULL;
    ib_context_module_config(ib_context_main(ib),
                             ib_core_module(),
                             (void *)&corecfg);
    IB_FTRACE_RET_INT(corecfg->rule_log_level);
}

ib_rule_log_exec_t ib_rule_log_exec_level(const ib_engine_t *ib)
{
    IB_FTRACE_INIT();
    ib_core_cfg_t *corecfg = NULL;
    ib_context_module_config(ib_context_main(ib),
                             ib_core_module(),
                             (void *)&corecfg);
    IB_FTRACE_RET_INT(corecfg->rule_log_exec);
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

    if (ib_rule_log_level(tx->ib) < IB_RULE_LOG_TRACE) {
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
    char prebuf[prebuf_size+1];
    char *fmtbuf = NULL;
    size_t fmtlen = 0;
    void *freeptr = NULL;
    ib_bool_t log_opinst = IB_FALSE;

    /* Ignore this message? */
    if ( (level <= IB_RULE_LOG_ERROR) ||
         (level > ib_rule_log_level(tx->ib)) )
    {
        IB_FTRACE_RET_VOID();
    }

    /* Build a new format buffer with rule ID and target field name */

    /* Calculate the prefix length */
    if (rule != NULL) {
        fmtlen += strlen(rule->meta.id) + 10;
        if (level >= IB_RULE_LOG_DEBUG) {
            log_opinst = IB_TRUE;
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
    if (fmtlen != 0) {
        fmtlen += strlen(fmt) + 4;
        fmtbuf = malloc(fmtlen);

        if (fmtbuf != NULL) {
            ib_bool_t first = IB_TRUE;

            strcpy(fmtbuf, "[");

            /* Add the rule and operator name */
            if (rule != NULL) {
                strcat(fmtbuf, "rule:\"");
                strcat(fmtbuf, rule->meta.id);
                strcat(fmtbuf, "\"");
                if (log_opinst == IB_TRUE) {
                    strcat(fmtbuf, " operator:\"");
                    strcat(fmtbuf, rule->opinst->op->name);
                    strcat(fmtbuf, "\"");
                }
                first = IB_FALSE;
            }

            /* Add the target field name */
            if (target != NULL) {
                if (first != IB_TRUE) {
                    strcat(fmtbuf, " ");
                }
                else {
                    first = IB_FALSE;
                }
                strcat(fmtbuf, "target:\"");
                strcat(fmtbuf, target->field_name);
                strcat(fmtbuf, "\"");
            }

            /* Add the transformation name */
            if (tfn != NULL) {
                if (first != IB_TRUE) {
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
    }

    snprintf(prebuf, prebuf_size, "%s/%s", LOG_PREFIX, prefix);

    ib_vlog_ex(tx->ib, IB_LOG_ALWAYS, tx, prebuf, file, line, fmt, ap);

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

/* Log rule execution: fast mode. */
/* Format: site-id rIP:rPort tx-time-delta ruleid: op=op-name target="target-name" actions=actionname1,action-name2,... */
static void log_exec_fast(const ib_tx_t *tx,
                          const ib_rule_t *rule,
                          ib_bool_t result_type,
                          const ib_list_t *results,
                          const ib_list_t *actions,
                          const char *file,
                          int line)
{
    IB_FTRACE_INIT();

    assert(tx != NULL);
    assert(rule != NULL);
    assert(results != NULL);
    assert(actions != NULL);
    assert(ib_rule_log_exec_level(tx->ib) == IB_RULE_LOG_EXEC_FAST);

    const size_t MAX_ACTBUF = 128;
    const ib_list_node_t *node;
    ib_time_t now = ib_clock_get_time();
    char actbuf[MAX_ACTBUF + 1];
    char *cur = actbuf;
    size_t remain = MAX_ACTBUF;
    ib_bool_t first = IB_TRUE;

    /*
     * Make a string out of the action list (think Perl's "join").
     */
    *cur = '\0';
    IB_LIST_LOOP_CONST(actions, node) {
        const ib_action_inst_t *action =
            (const ib_action_inst_t *)ib_list_node_data_const(node);

        /* For the second and following actions, add a comma to the string */
        if (first == IB_FALSE) {
            strncpy(cur, ",", remain);
            ++cur;
            --remain;
        }
        else {
            first = IB_FALSE;
        }

        /* Add the name of the action, with an optional "!" prefix */
        if (remain >= 2) {
            size_t len = strlen(action->action->name);
            if (result_type == IB_FALSE) {
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
    if (*actbuf == '\0') {
        strcpy(actbuf, "<NONE>");
    }

    /*
     * Log all of the targets whose result that matched the result type.
     */
    IB_LIST_LOOP_CONST(results, node) {
        const ib_rule_target_result_t *result =
            (const ib_rule_target_result_t *)node->data;

        /* Only add rule targets that match the result type */
        if ( ((result_type == IB_TRUE) && (result->result != 0)) ||
             ((result_type == IB_FALSE) && (result->result == 0)) )
        {
            ib_log_ex(tx->ib, IB_LOG_ALWAYS, tx, LOG_PREFIX, file, line,
                      "%s:%d %"PRIu64"us %s op=%s target=\"%s\" actions=%s",
                      tx->er_ipstr, tx->conn->remote_port,
                      now - tx->t.started,
                      rule->meta.id, rule->opinst->op->name,
                      result->target->field_name, actbuf);
        }
    }

    IB_FTRACE_RET_VOID();
}

/* Log rule execution: Normal mode. */
static void log_exec(const ib_tx_t *tx,
                     const ib_rule_t *rule,
                     ib_bool_t result_type,
                     const ib_list_t *results,
                     const ib_list_t *actions,
                     const char *file,
                     int line)
{
    IB_FTRACE_INIT();

    assert(tx != NULL);
    assert(rule != NULL);
    assert(results != NULL);
    assert(actions != NULL);
    assert(ib_rule_log_exec_level(tx->ib) == IB_RULE_LOG_EXEC_FULL);

    const ib_list_node_t *resnode;

    /*
     * Log all of the targets whose result that matched the result type.
     */
    IB_LIST_LOOP_CONST(results, resnode) {
        const ib_rule_target_result_t *result =
            (const ib_rule_target_result_t *)ib_list_node_data_const(resnode);
        const ib_list_node_t *actnode;

        IB_LIST_LOOP_CONST(actions, actnode) {
            const ib_action_inst_t *action =
                (const ib_action_inst_t *)ib_list_node_data_const(actnode);

            ib_log_ex(tx->ib, IB_LOG_ALWAYS, tx, LOG_PREFIX, file, line,
                      "%s:%d \"%s\" operator \"%s\" target \"%s\" result %u; "
                      "action \"%s%s\" executed",
                      tx->er_ipstr,
                      tx->conn->remote_port,
                      rule->meta.id,
                      rule->opinst->op->name,
                      result->target->field_name,
                      result->result,
                      result_type == IB_FALSE ? "!" : "",
                      action->action->name);
        }
    }

    IB_FTRACE_RET_VOID();
}

/* Log rule execution: exec. */
/* Format: site-id rIP:rPort tx-time-delta ruleid: op=op-name target="target-name" actions=actionname1,action-name2,... */
void ib_rule_log_exec_ex(const ib_tx_t *tx,
                         const ib_rule_t *rule,
                         ib_bool_t result_type,
                         const ib_list_t *results,
                         const ib_list_t *actions,
                         const char *file,
                         int line)
{
    IB_FTRACE_INIT();

    assert(tx != NULL);
    assert(rule != NULL);

    switch (ib_rule_log_exec_level(tx->ib)) {
    case IB_RULE_LOG_EXEC_OFF:
        IB_FTRACE_RET_VOID();

    case IB_RULE_LOG_EXEC_FAST:
        assert(results != NULL);
        assert(actions != NULL);
        log_exec_fast(tx, rule, result_type, results, actions, file, line);
        IB_FTRACE_RET_VOID();

    case IB_RULE_LOG_EXEC_FULL:
        assert(results != NULL);
        assert(actions != NULL);
        log_exec(tx, rule, result_type, results, actions, file, line);
        IB_FTRACE_RET_VOID();

    default:
        assert(0 && "Invalid rule log exec level");
        IB_FTRACE_RET_VOID();
    }
}
