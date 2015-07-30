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
 * @brief IronBee Modules --- XRule IB Action
 *
 * @author Sam Baskinger <sbaskinger@qualys.com>
 */

#include "xrules_ibaction.hpp"

/* IbAction Impl */
IbAction::IbAction(
    IronBee::MemoryManager  mm,
    IronBee::Context        ctx,
    const char             *action_name,
    const char             *action_param,
    int                     priority
) :
    Action(
        (std::string(action_name)+action_param).c_str(),
        priority,
        (std::string(action_name) + "(" +action_param+")"),
        "xrule/acl"
    ),
    m_action_inst(
        IronBee::ActionInstance::create(mm, ctx, action_name, action_param)
    )
{
}

void IbAction::apply_impl(
    const XRulesModuleConfig& config,
    xrules_module_tx_data_ptr mdata,
    IronBee::Transaction      tx
) const
{
    m_action_inst.execute(mdata->rule_exec);
}

