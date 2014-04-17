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
 * @brief IronBee Modules --- XRule Exception
 *
 * An XRule that logically ands several checks together to create exceptions.
 *
 * An XRuleException is comprised of many FactAction s and one
 * ConclusionAction. At various points in an IronBee::Transaction
 * XRules will run and, on success, fire the associated FactAction.
 * The FactAction, after recording that it has fired, in turn, fires
 * the ConclusionAction. The ConclusionAction checks if all FactAction
 * results have been set, meaning that all FactAction s have fired. If all
 * FactAction s have fired, the ConclusionAction fires the user's Action, eg
 * block, raise the threat level, etc.
 *
 * Action priority is not meaningful in the context of an XRuleException
 * as FactAction and ConclusionAction objects are all unique.
 *
 * @author Sam Baskinger <sbaskinger@qualys.com>
 */

#ifndef __MODULES__XRULES_EXCEPTION_HPP
#define __MODULES__XRULES_EXCEPTION_HPP

#include "xrules.hpp"

#include <ironbeepp/configuration_parser.hpp>

/**
 * An XRule that combines several checks into a single logical predicate.
 *
 * Because an XRuleException is best implemented as Action s associated
 * with existing XRules, the XRuleException class cannot be instantiated and
 * only serves to hold a factory method (xrule_directive()) to
 * construct the appropriate actions.
 *
 * @sa ConclusionAction
 * @sa FactAction
 */
class XRuleException  {
public:
    /**
     * Construct and add an XRuleException during IronBee configuration.
     *
     * This function implements the configuration parsing and
     * object construction for an XRuleException.
     *
     * @param[in] module XRule module.
     * @param[in] cp The configuration parser.
     * @param[in] name The directive name used.
     * @param[in] all_params The configuration parameters.
     */
    static void xrule_directive(
        XRulesModule                     module,
        IronBee::ConfigurationParser     cp,
        const char *                     name,
        IronBee::ConstList<const char *> all_params
    );
private:
    XRuleException();
};

#endif // __MODULES__XRULES_EXCEPTION_HPP
