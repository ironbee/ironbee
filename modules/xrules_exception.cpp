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
 * @author Sam Baskinger <sbaskinger@qualys.com>
 */

#include "xrules.hpp"
#include "xrules_acls.hpp"
#include "xrules_exception.hpp"

#include <strings.h>

#include <boost/foreach.hpp>
#include <boost/make_shared.hpp>

namespace {

/**
 * Utility function to parse an argument for XRuleException configurations.
 *
 * This takes an XRuleException argument, checks the provided prefix,
 * and if it matches returns IB_OK and assigns the `char *` following the
 * prefix in @a arg to @a rest. Thus, the caller is provided with
 * the value following the prefix and may continue parsing it.
 *
 * @param[in] expected The expected value. A case insensitive comparison
 *            is done between @a expected and @a arg.
 * @param[in] arg The argument to check.
 * @param[in] rest The rest of @a arg following the prefix @a exected.
 *
 * @returns
 * - IB_OK On success and a match.
 * - IB_ENOENT If @a expected does not match @a arg.
 */
ib_status_t parse_arg(
    const char  *expected,
    const char  *arg,
    const char **rest
)
{
    size_t len = strlen(expected);
    if (strncasecmp(expected, arg, len) == 0) {
        (*rest) = arg+len;
        return IB_OK;
    }
    else {
        return IB_ENOENT;
    }
}

/**
 * Use ib_uuid_create_v4() to generate a name.
 *
 * This is used to name Actions when the name is not important.
 */
std::string random_name()
{
    std::vector<char> uuid(IB_UUID_LENGTH);

    IronBee::throw_if_error(ib_uuid_create_v4(&(uuid[0])));

    return std::string(&(uuid[0]));
}

/**
 * A ConclusionAction is an action that conditionally fires when it is applied.
 *
 * To implement an XRuleException we actually wire together many
 * FactActions which, after each is called, attempts to apply the
 * ConclusionAction.
 *
 * The ConclusionAction will only fire if all its facts are true (results=1).
 */
class ConclusionAction : public Action
{
public:
    /**
     * Construct a conclusion action that depends on @a results.
     *
     * @param[in] action The user's action to execute when all FactAction s
     *            are true.
     * @param[in] results The number of results this ConclusionAction should
     *            create to be given to FactActions.
     */
    ConclusionAction(action_ptr action, int results);

    /**
     * Return a reference to the boolean that denotes a particular result.
     *
     * @param[in] mdata Module data.
     * @param[in] i The index of the result to fetch. This must be in
     * the range 0 through the number of results specified when this
     * object was created.
     *
     * @returns The int refernce to assign to.
     */
    int& result(
        XRulesModuleTxData& mdata,
        int i
    );

private:
    //! The action that is applied if all results are true.
    action_ptr m_action;

    /**
     * A vector of results (0=failed/not executed, 1=success).
     *
     * Only when all of these are equal to 1 will ConclusionAction::m_action
     * be executed.
     */
    size_t m_results;

    /**
     * Apply this action to the transaction if all results are non-zero.
     *
     * @param[in] mdata The module.
     * @param[in] tx The current transaction.
     */
    virtual void apply_impl(
        xrules_module_tx_data_ptr mdata,
        IronBee::Transaction      tx
    ) const;
};

ConclusionAction::ConclusionAction(action_ptr action, int results):
    Action(random_name(), 10),
    m_action(action),
    m_results(results)
{}

int& ConclusionAction::result(
    XRulesModuleTxData& mdata,
    int i
)
{
    std::vector<int>& v = mdata.exception_facts[this];

    if (v.size() <= m_results) {
        v.resize(m_results);
    }

    return v[i];
}

void ConclusionAction::apply_impl(
    xrules_module_tx_data_ptr mdata,
    IronBee::Transaction      tx
) const
{
    std::vector<int>& v = (*mdata).exception_facts[this];

    BOOST_FOREACH(int r, v) {
        if (r != 1) {
            return;
        }
    }

    /* If we reach this code, all results were = 1. Execute. */
    (*m_action)(mdata, tx);
}

/**
 * This action initializes facts to false (0) and sets them to true (1).
 *
 * To implement an XRuleException we must collect various facts, such as
 * is a Path equal to some prefix, is a source IP in a subnet, etc.
 * To collect these facts XRules are created and given FactActions
 * which record that a given XRule was true and fired its action.
 *
 * When any FactAction fires, as its last action, it calls its
 * associated ConclusionAction. A ConclusionAction will check if all
 * FactActions have set their facts to true (1). Only if all facts are
 * true does the ConclusionAction then fire its own associated Action.
 * The ConclusionAction's Action is the action the user requested
 * in the configuration language.
 */
class FactAction : public Action
{
public:

    /**
     * Construct a new action.
     *
     * @param[in] conclusion The conclusion action that this Fact is
     *            gateing the execution of.
     * @param[in] result_idx The index of the result in the result
     *            vector of the ConclusionAction.
     *
     * @note This takes a reference to a boolean because actions may
     *       not modify themselves.
     */
    FactAction(action_ptr conclusion, int result_idx);

private:
    action_ptr  m_conclusion;
    int         m_result_idx;

    /**
     * Apply this action to the transaction.
     *
     * @param[in] mdata The module.
     * @param[in] tx The current transaction.
     */
    virtual void apply_impl(
        xrules_module_tx_data_ptr mdata,
        IronBee::Transaction      tx
    ) const;
};

FactAction::FactAction(action_ptr conclusion, int result_idx) :
    Action(random_name(), 10),
    m_conclusion(conclusion),
    m_result_idx(result_idx)
{}

void FactAction::apply_impl(
    xrules_module_tx_data_ptr mdata,
    IronBee::Transaction      tx
) const
{

    /* Update the results. */
    static_cast<ConclusionAction&>(*m_conclusion).result(*mdata, m_result_idx) = 1;

    /* Having updated the results, try the conclusion. */
    (*m_conclusion)(mdata, tx);
}

} /* Anonymous Namespace */

void XRuleException::xrule_directive(
    XRulesModule                     module,
    IronBee::ConfigurationParser     cp,
    const char *                     name,
    IronBee::ConstList<const char *> all_params
)
{
    IronBee::Context    ctx = cp.current_context();
    XRulesModuleConfig& cfg =
        module.module().configuration_data<XRulesModuleConfig>(ctx);

    /* The unparsed bits from parsing an action out of the params arg. */
    IronBee::List<const char *> params =
        IronBee::List<const char *>::create(cp.memory_manager());

    /* Parse the action and put the remaining tokens in `params`. */
    action_ptr user_action = module.parse_action(cp, all_params, params);

    /* Construct a conclusion action that will fire the user's action. */
    action_ptr conclusion(
        boost::make_shared<ConclusionAction>(
            user_action,
            params.size()));

    if (params.empty()) {
        BOOST_THROW_EXCEPTION(
            IronBee::einval()
                << IronBee::errinfo_what(
                    "XRuleException require at least 1 argument.")
        );
    }

    int result_idx = 0;
    BOOST_FOREACH(const char* param, params) {
        const char* val;

        /* Build a new FactAction to
         * - Set the result `result_idx` to 1.
         * - Fire the conclusion action. */
        action_ptr action(
            boost::make_shared<FactAction>(
                conclusion,
                result_idx));

        ++result_idx;

        if (IB_OK == parse_arg("EventTag:", param, &val)) {
            IronBee::List<const char *> l =
                IronBee::List<const char *>::create(cp.memory_manager());
            l.push_back(val);
            cfg.event_xrules.push_back(
                xrule_ptr(
                    new XRuleEventTag(l, action)));
        }
        else if (IB_OK == parse_arg("IPv4:", param, &val)) {
            // Copy in an empty, uninitialized ipset entry.
            ib_ipset4_entry_t entry;

            memset(&entry, 0, sizeof(entry));

            val = XRuleIP::normalize_ipv4(cp.memory_manager(), val);

            IronBee::throw_if_error(
                ib_ip4_str_to_net(val, &(entry.network)),
                (std::string("Failed to get net from string: ")+val).c_str()
            );

            // Put that action in the ip set.
            entry.data = IronBee::value_to_data<action_ptr>(
                action,
                cp.engine().main_memory_mm().ib());

            cfg.ipv4_list.push_back(entry);
        }
        else if (IB_OK == parse_arg("IPv6:", param, &val)) {
            // Copy in an empty, uninitialized ipset entry.
            ib_ipset6_entry_t entry;

            memset(&entry, 0, sizeof(entry));

            val = XRuleIP::normalize_ipv6(cp.memory_manager(), val);

            IronBee::throw_if_error(
                ib_ip6_str_to_net(val, &(entry.network)),
                (std::string("Failed to get net from string: ")+val).c_str()
            );

            // Put that action in the ip set.
            entry.data = IronBee::value_to_data<action_ptr>(
                action,
                cp.engine().main_memory_mm().ib());

            cfg.ipv6_list.push_back(entry);
        }
        else if (IB_OK == parse_arg("Geo:", param, &val)) {
            cfg.req_xrules.push_back(
                xrule_ptr(
                    new XRuleGeo(val, action)));
        }
        else if (IB_OK == parse_arg("Path:", param, &val)) {
            cfg.req_xrules.push_back(
                xrule_ptr(
                    new XRulePath(val, action)));
        }
        else {
            BOOST_THROW_EXCEPTION(
                IronBee::enoent()
                    << IronBee::errinfo_what(
                        std::string("Unknown XRuleException: ")+param)
            );
        }
    }
}
