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
#include <ironbee/mpool.h>
#include <ironbee/action.h>
#include <ironbee/rule_engine.h>

#include "ironbee_core_private.h"


/**
 * @internal
 * Create function for the log action.
 *
 * @param[in] mp Memory pool to use for allocation
 * @param[in] parameters Constant parameters from the rule definition
 * @param[in,out] inst Action instance
 *
 * @returns Status code
 */
static ib_status_t act_log_create(ib_mpool_t *mp,
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
 * @param[in] mp Memory pool to use for allocation
 * @param[in] parameters Constant parameters from the rule definition
 * @param[in,out] inst Action instance
 *
 * @returns Status code
 */
static ib_status_t act_setflags_create(ib_mpool_t *mp,
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

typedef struct act_event_data_t act_event_data_t;

struct act_event_data_t {
    char *msg;
};

/**
 * Event action creation callback.
 *
 * @param[in] pool Memory pool to be used for allocating needed memory.
 * @param[in] parameters Unparsed string with the parameters to
 *                       initialize the action instance.
 * @param[in,out] act_inst Pointer to the operator instance to be initialized.
 *
 * @returns IB_OK if successful.
 */
static ib_status_t act_event_create_inst(ib_mpool_t *pool,
                                         const char *data,
                                         ib_action_inst_t *act_inst)
{
    IB_FTRACE_INIT();
    act_event_data_t *ev_data;
    ev_data = (act_event_data_t *)ib_mpool_alloc(pool, sizeof(*ev_data));

    ev_data->msg = ib_mpool_strdup(pool, data);

    act_inst->data = ev_data;

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
    act_event_data_t *ev_data = data;
    ib_logevent_t *e;

    rc = ib_logevent_create(&e,
                            tx->mp,
                            ib_rule_id(rule),
                            IB_LEVENT_TYPE_ALERT,
                            IB_LEVENT_ACT_UNKNOWN,
                            IB_LEVENT_PCLASS_UNKNOWN,
                            IB_LEVENT_SCLASS_UNKNOWN,
                            IB_LEVENT_SYS_UNKNOWN,
                            IB_LEVENT_ACTION_IGNORE,
                            IB_LEVENT_ACTION_IGNORE,
                            90,
                            80,
                            ev_data->msg
                            );

    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Log the event. */
    rc = ib_event_add(tx->epi, e);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
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

    rc = ib_action_register(ib,
                            "event",
                            IB_ACT_FLAG_NONE,
                            act_event_create_inst,
                            NULL, /* no destroy function */
                            act_event_execute);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}
