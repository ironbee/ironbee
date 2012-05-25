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
#include <ironbee/rule_engine.h>

#include <ironbee/debug.h>
#include <ironbee/mpool.h>

#include "ironbee_private.h"

/**
 * Prefix used for all rule engine log entries.
 */
#define LOG_PREFIX "RULE_ENG"

ib_rule_log_level_t ib_rule_log_level(ib_engine_t *ib)
{
    IB_FTRACE_INIT();
    ib_core_cfg_t *corecfg = NULL;
    ib_context_module_config(ib_context_main(ib),
                             ib_core_module(),
                             (void *)&corecfg);
    IB_FTRACE_RET_INT(corecfg->rule_log_level);
}

/* Log a field's value for the rule engine */
void ib_log_rule_field(ib_engine_t *ib,
                       const char *label,
                       const ib_field_t *f)
{
    IB_FTRACE_INIT();
    ib_status_t rc;

    if (f->type == IB_FTYPE_NULSTR) {
        const char *s;
        rc = ib_field_value(f, ib_ftype_nulstr_out(&s));
        if (rc != IB_OK) {
            IB_FTRACE_RET_VOID();
        }
        ib_log_debug3(ib, "%s = '%s'", label, s);
    }
    else if (f->type == IB_FTYPE_BYTESTR) {
        const ib_bytestr_t *bs;
        rc = ib_field_value(f, ib_ftype_bytestr_out(&bs));
        if (rc != IB_OK) {
            IB_FTRACE_RET_VOID();
        }
        ib_log_debug3(ib, "%s = '%.*s'",
                     label,
                     (int)ib_bytestr_length(bs),
                     (const char *)ib_bytestr_const_ptr(bs));
    }
    else {
        ib_log_debug3(ib, "%s type = %d", label, f->type);
    }
    IB_FTRACE_RET_VOID();
}

/* Generic Logger for rules. */
void ib_rule_vlog(ib_rule_log_level_t level,
                  ib_tx_t *tx,
                  const ib_rule_t *rule,
                  const ib_rule_target_t *target,
                  const char *prefix,
                  const char *file,
                  int line,
                  const char *fmt,
                  va_list ap)
{
    IB_FTRACE_INIT();

    IB_FTRACE_RET_VOID();
}

/* Generic Logger for rules. */
void ib_rule_log(ib_rule_log_level_t level,
                 ib_tx_t *tx,
                 const ib_rule_t *rule,
                 const ib_rule_target_t *target,
                 const char *prefix,
                 const char *file,
                 int line,
                 const char *fmt, ...)
{
    IB_FTRACE_INIT();

    va_list ap;
    va_start(ap, fmt);

    ib_rule_vlog(level, tx, rule, target, prefix, file, line, fmt, ap);

    va_end(ap);

    IB_FTRACE_RET_VOID();
}

/* Generic Logger for rules. */
/* Format: site-id rIP:rPort tx-time-delta ruleid: op=op-name target="target-name" actions=actionname1,action-name2,... */
void ib_rule_log_fast_ex(ib_tx_t *tx,
                         const ib_rule_t *rule,
                         ib_bool_t result_type,
                         const ib_list_t *results,
                         const ib_list_t *actions,
                         const char *file,
                         int line)
{
    IB_FTRACE_INIT();
    const size_t MAX_ACTBUF = 128;
    const ib_list_node_t *node;
    ib_time_t now = ib_clock_get_time();
    char actbuf[MAX_ACTBUF + 1];
    char *cur = actbuf;
    size_t remain = MAX_ACTBUF;
    ib_bool_t first = IB_TRUE;

    if (ib_rule_log_level(tx->ib) != IB_RULE_LOG_FAST) {
        IB_FTRACE_RET_VOID();
    }

    /*
     * Make a string out of the action list (think Perl's "join").
     */
    *cur = '\0';
    IB_LIST_LOOP_CONST(actions, node) {
        const ib_action_inst_t *action = (const ib_action_inst_t *)node->data;
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
            ib_log_ex(tx->ib, IB_LOG_EMERGENCY, tx, LOG_PREFIX, file, line,
                      "%s:%d %"PRIu64"us %s op=%s target=\"%s\" actions=%s",
                      tx->er_ipstr, tx->conn->remote_port,
                      now - tx->t.started,
                      rule->meta.id, rule->opinst->op->name,
                      result->target->field_name, actbuf);
        }
    }

    IB_FTRACE_RET_VOID();
}
