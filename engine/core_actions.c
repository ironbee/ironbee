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

#include "ironbee_core_private.h"


/**
 * @internal
 * Create function for the log operator.
 *
 * @param mp Memory pool to use for allocation
 * @param parameters Constant parameters
 * @param op_inst Instance operator
 *
 * @returns Status code
 */
static ib_status_t act_log_create(ib_mpool_t *mp,
                                  const char *parameters,
                                  ib_action_inst_t *inst)
{
    IB_FTRACE_INIT(act_log_create);
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
 * Execute function for the "@streq" operator
 *
 * @param data C-style string to compare to
 * @param field Field value
 * @param result Pointer to number in which to store the result
 *
 * @returns Status code
 */
static ib_status_t act_log_execute(void *data,
                                   ib_rule_t *rule,
                                   ib_tx_t *tx)
{
    IB_FTRACE_INIT(act_log_execute);

    /* This works on C-style (NUL terminated) and byte strings */
    const char *cstr = (const char *)data;

    ib_log_debug(tx->ib, 9, "LOG: %s", cstr);
    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t ib_core_actions_init(ib_engine_t *ib, ib_module_t *mod)
{
    IB_FTRACE_INIT(ib_core_actions_init);
    ib_status_t  rc;

    rc = ib_action_register(ib,
                            "log",
                            IB_ACT_FLAG_NONE,
                            act_log_create,
                            NULL, /* no destroy function */
                            act_log_execute);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}
