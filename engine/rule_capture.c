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
 * @brief IronBee --- Rule capture implementation.
 *
 * @author Nick LeRoy <nleroy@qualys.com>
 */

#include "ironbee_config_auto.h"

#include <ironbee/rule_capture.h>

#include <ironbee/capture.h>
#include <ironbee/flags.h>
#include <ironbee/rule_engine.h>
#include <ironbee/util.h>

#include <assert.h>

bool ib_rule_should_capture(
    const ib_rule_exec_t *rule_exec,
    ib_num_t              result)
{
    assert(rule_exec != NULL);

    if ( (result != 0) &&
         (rule_exec->rule != NULL) &&
         (ib_flags_all(rule_exec->rule->flags, IB_RULE_FLAG_CAPTURE)) )
    {
        return true;
    }
    else {
        return false;
    }
}

const char *ib_rule_capture_name(
    const ib_rule_exec_t *rule_exec,
    int                   num)
{
    assert( (rule_exec != NULL) &&
            (rule_exec->tx != NULL) &&
            (rule_exec->rule != NULL) );
    assert(num >= 0);

    return ib_capture_name(num);
}

const char *ib_rule_capture_fullname(
    const ib_rule_exec_t *rule_exec,
    int                   num)
{
    assert( (rule_exec != NULL) &&
            (rule_exec->tx != NULL) &&
            (rule_exec->rule != NULL) );

    return ib_capture_fullname(rule_exec->tx,
                               rule_exec->rule->capture_collection,
                               num);
}

ib_status_t ib_rule_capture_clear(
    const ib_rule_exec_t *rule_exec)
{
    assert( (rule_exec != NULL) &&
            (rule_exec->tx != NULL) &&
            (rule_exec->rule != NULL) );

    return ib_capture_clear(rule_exec->tx,
                            rule_exec->rule->capture_collection);
}

ib_status_t ib_rule_capture_set_item(
    const ib_rule_exec_t *rule_exec,
    int                   num,
    ib_field_t           *in_field)
{
    assert( (rule_exec != NULL) &&
            (rule_exec->tx != NULL) &&
            (rule_exec->rule != NULL) );

    return ib_capture_set_item(rule_exec->tx,
                               rule_exec->rule->capture_collection,
                               num,
                               in_field);
}

ib_status_t ib_rule_capture_add_item(
    const ib_rule_exec_t *rule_exec,
    ib_field_t           *in_field
)
{
    assert( (rule_exec != NULL) &&
            (rule_exec->tx != NULL) &&
            (rule_exec->rule != NULL) );

    return ib_capture_add_item(rule_exec->tx,
                               rule_exec->rule->capture_collection,
                               in_field);
}
