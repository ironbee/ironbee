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
 * XRules that allow for IronBee Actions.
 *
 * @author Sam Baskinger <sbaskinger@qualys.com>
 */
#ifndef __MODULES__XRULES_IBACTION_HPP
#define __MODULES__XRULES_IBACTION_HPP

#include "xrules.hpp"

#include <ironbeepp/transaction.hpp>
#include <ironbeepp/var.hpp>
#include <ironbeepp/action.hpp>


/**
 * Defines how to call an IronBee action.
 */
class IbAction : public Action {

public:

    /**
     * Construct a new IbAction.
     *
     * @param[in] action_name The IronBee action to instantiate.
     * @param[in] actin_param The parameter to hand the IronBee action.
     * @param[in] priority The priority of this action.
     *
     */
    IbAction(
        IronBee::MemoryManager mm,
        IronBee::Context ctx,
        const char *action_name,
        const char *action_param,
        int         priority
    );

private:

    IronBee::ActionInstance m_action_inst;

    /**
     * Execute a given action.
     *
     * @sa Action::apply_impl().
     */
    void apply_impl(
        const XRulesModuleConfig& config,
        xrules_module_tx_data_ptr mdata,
        IronBee::Transaction tx
    ) const;
};

#endif /* __MODULES__XRULES_IBACTION_HPP */
