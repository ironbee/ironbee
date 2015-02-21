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
 * @brief IronBee Modules --- XRules
 *
 * @author Sam Baskinger <sbaskinger@qualys.com>
 */

#include "xrules.hpp"
#include "xrules_acls.hpp"
#include "xrules_exception.hpp"

#include <ironbeepp/configuration_directives.hpp>
#include <ironbeepp/configuration_parser.hpp>
#include <ironbeepp/connection.hpp>
#include <ironbeepp/engine.hpp>
#include <ironbeepp/hooks.hpp>
#include <ironbeepp/list.hpp>
#include <ironbeepp/module_bootstrap.hpp>
#include <ironbeepp/module_delegate.hpp>
#include <ironbeepp/parsed_header.hpp>
#include <ironbeepp/transaction.hpp>

/* C includes. */
#include <ironbee/engine.h>
#include <ironbee/flags.h>
#include <ironbee/ip.h>
#include <ironbee/ipset.h>
#include <ironbee/log.h>
#include <ironbee/logevent.h>
#include <ironbee/server.h>
#include <ironbee/string.h>
#include <ironbee/type_convert.h>
#include <ironbee/var.h>

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/foreach.hpp>
#include <boost/regex.hpp>
#include <boost/shared_ptr.hpp>

#include <list>
#include <vector>

//! Block action text.
const char ACTION_BLOCK[] = "Block";

//! Allow action text.
const char ACTION_ALLOW[] = "Allow";

//! Enable Blocking Mode action text.
const char ACTION_ENABLEBLOCKINGMODE[] = "EnableBlockingMode";

//! Disable Blocking Mode action text.
const char ACTION_DISABLEBLOCKINGMODE[] = "DisableBlockingMode";

//! Scale Threat action text.
const char ACTION_SCALETHREAT[] = "ScaleThreat";

//! Enable Request Header Inspect action text.
const char ACTION_ENABLEREQUESTHEADERINSPECTION[] =
    "EnableRequestHeaderInspection";

//! Disable Request Header Inspection action text.
const char ACTION_DISABLEREQUESTHEADERINSPECTION[] =
    "DisableRequestHeaderInspection";

//! Enable Request URI Inspection action text.
const char ACTION_ENABLEREQUESTURIINSPECTION[] =
    "EnableRequestURIInspection";

//! Disable request URI Inspection action text.
const char ACTION_DISABLEREQUESTURIINSPECTION[] =
    "DisableRequestURIInspection";

//! Enable Request Param Inspection action text.
const char ACTION_ENABLEREQUESTPARAMINSPECTION[] =
    "EnableRequestParamInspection";

//! Disable Request Param Inspection action text.
const char ACTION_DISABLEREQUESTPARAMINSPECTION[] =
    "DisableRequestParamInspection";

//! Enable Request Body Inspection action text.
const char ACTION_ENABLEREQUESTBODYINSPECTION[] =
    "EnableRequestBodyInspection";

//! Disable Request Body Inspection action text.
const char ACTION_DISABLEREQUESTBODYINSPECTION[] =
    "DisableRequestBodyInspection";

//! Enable Response Header Inspection action text.
const char ACTION_ENABLERESPONSEHEADERINSPECTION[] =
    "EnableResponseHeaderInspection";

//! Disable Response Header Inspection action text.
const char ACTION_DISABLERESPONSEHEADERINSPECTION[] =
    "DisableResponseHeaderInspection";

//! Enable Response Body Inspection action text.
const char ACTION_ENABLERESPONSEBODYINSPECTION[] =
    "EnableResponseBodyInspection";

//! Disable Response Body Inspection action text.
const char ACTION_DISABLERESPONSEBODYINSPECTION[] =
    "DisableResponseBodyInspection";


const std::string Action::DEFAULT_LOG_MESSAGE("");
const std::string Action::DEFAULT_TAG("");

/* Action Impl. */
Action::Action(
    const std::string& id,
    int priority,
    const std::string& logevent_msg,
    const std::string& tag
)
:
    m_logevent_msg(logevent_msg),
    m_tag(tag),
    m_priority(priority),
    m_id(id)
{}

Action::~Action()
{}

void Action::operator()(
    const XRulesModuleConfig& config,
    xrules_module_tx_data_ptr mdata,
    IronBee::Transaction tx
)
{
    if (
        config.generate_events &&    /* Does the module allow events? */
        mdata->generate_events &&    /* Does the tx allow events? */
        m_logevent_msg.length() > 0  /* Finally, is there a message? */
    )
    {
        ib_logevent_t *logevent;

        IronBee::throw_if_error(
            ib_logevent_create(
                &logevent,
                tx.memory_manager().ib(),
                m_tag.c_str(),              /* Use the tag for the rule id. */
                IB_LEVENT_TYPE_OBSERVATION,
                IB_LEVENT_ACTION_UNKNOWN,
                0,                          /* Confidence. */
                0,                          /* Severity. */
                "%s",
                m_logevent_msg.c_str()
            )
        );

        IronBee::throw_if_error(
            ib_logevent_tag_add(logevent, m_tag.c_str())
        );

        IronBee::throw_if_error(
            ib_logevent_add(tx.ib(), logevent)
        );
    }

    apply_impl(config, mdata, tx);

}

std::string& Action::logevent_msg() {
    return m_logevent_msg;
}

std::string& Action::logevent_tag() {
    return m_tag;
}

bool Action::operator<(const Action &that) const
{
    return m_id < that.m_id;
}

bool Action::overrides(const Action& that)
{
    return (m_priority >= that.m_priority && m_id == that.m_id);
}

void Action::apply_impl(
    const XRulesModuleConfig& config,
    xrules_module_tx_data_ptr mdata,
    IronBee::Transaction tx
) const
{
    BOOST_THROW_EXCEPTION(
        IronBee::enotimpl() <<
            IronBee::errinfo_what("Action::apply_impl must be overridden.")
    );
}
/* End Action Impl. */

/* ActionSet Impl */
void ActionSet::set(action_ptr& action)
{
    std::map<Action, action_ptr>::iterator itr =
        m_actions.find(*action);

    if (itr == m_actions.end()) {
        m_actions[*action] = action;
    }
    else if (itr->second->overrides(*action)) {
        itr->second = action;
    }
}

void ActionSet::apply(
    const XRulesModuleConfig& config,
    xrules_module_tx_data_ptr mdata,
    IronBee::Transaction tx
)
{
    /* Just a debug message. */
    if (m_actions.size() == 0) {
        ib_log_debug_tx(tx.ib(), "No actions to run.");
    }
    else {
        ib_log_debug_tx(
            tx.ib(),
            "Running %d actions.",
            static_cast<int>(m_actions.size()));
    }

    for(
        std::map<Action, action_ptr>::iterator itr = m_actions.begin();
        itr != m_actions.end();
        ++itr
    ) {
        (*(itr->second))(config, mdata, tx);
    }

    /* After applying the TX, set the value. */
    IronBee::Field f = IronBee::Field::create_float(
        tx.memory_manager(),
        "", 0,
        mdata->scale_threat);

    IronBee::VarTarget target = IronBee::VarTarget::acquire(
        tx.memory_manager(),
        config.xrules_collection,
        IronBee::VarExpand(),
        config.xrules_scale_threat
    );

    target.remove_and_set(
        tx.memory_manager(),
        tx.var_store(),
        f
    );
}

bool ActionSet::overrides(action_ptr action)
{
    std::map<Action, action_ptr>::iterator itr = m_actions.find(*action);

    if (itr == m_actions.end()) {
        return true;
    }
    else {
        return itr->second->overrides(*action);
    }
}
/* End ActionSet Impl */


/* ActionFactory Impl */
boost::regex ActionFactory::name_val_re("\\s*([^\\s=]+)(?:=([^\\s]*))?\\s*");

ActionFactory::ActionFactory(IronBee::Engine ib) : m_ib(ib) {}

action_ptr ActionFactory::build(const char *arg, int priority)
{
    boost::cmatch mr;
    if (!boost::regex_match(arg, mr, name_val_re)) {
        BOOST_THROW_EXCEPTION(
            IronBee::einval()
                << IronBee::errinfo_what("Cannot parse action.")
        );
    }

    ib_log_debug(m_ib.ib(), "Building action %.*s",
        (int)(mr[2].first - mr[1].first),
        mr[1].first);

    if (has_action(ACTION_BLOCK, mr)) {
        return action_ptr(new BlockAllow(true, priority));
    }
    else if (has_action(ACTION_ALLOW, mr)) {
        return action_ptr(new BlockAllow(false, priority));
    }
    else if (has_action(ACTION_ENABLEBLOCKINGMODE, mr)) {
        return action_ptr(
            new SetFlag(
                "FLAGS:blockingMode", IB_TX_FBLOCKING_MODE, priority));
    }
    else if (has_action(ACTION_DISABLEBLOCKINGMODE, mr)) {
        return action_ptr(
            new UnsetFlag(
                "FLAGS:blockingMode", IB_TX_FBLOCKING_MODE, priority));
    }
    else if (has_action(ACTION_SCALETHREAT, mr)) {
        std::vector<char> uuid(IB_UUID_LENGTH);
        ib_float_t fnum;

        IronBee::throw_if_error(
            ib_uuid_create_v4(uuid.data()),
            "Cannot initialize v4 UUID.");
        IronBee::throw_if_error(
            ib_type_atof(std::string(mr[2]).c_str(), &fnum),
            "Cannot convert string to float.");

        return action_ptr(
            new ScaleThreat(
                std::string(uuid.data(), IB_UUID_LENGTH - 1),
                fnum,
                priority));
    }
    else if (has_action(ACTION_ENABLEREQUESTHEADERINSPECTION, mr)) {
        return action_ptr(
            new SetFlag(
                "FLAGS:inspectRequestHeader",
                IB_TX_FINSPECT_REQHDR,
                priority));
    }
    else if (has_action(ACTION_DISABLEREQUESTHEADERINSPECTION, mr)) {
        return action_ptr(
            new UnsetFlag(
                "FLAGS:inspectRequestHeader",
                IB_TX_FINSPECT_REQHDR,
                priority));
    }
    else if (has_action(ACTION_ENABLEREQUESTURIINSPECTION, mr)) {
        return action_ptr(
            new SetFlag(
                "FLAGS:inspectRequestUri",
                IB_TX_FINSPECT_REQURI,
                priority));
    }
    else if (has_action(ACTION_DISABLEREQUESTURIINSPECTION, mr)) {
        return action_ptr(
            new UnsetFlag(
                "FLAGS:inspectRequestUri",
                IB_TX_FINSPECT_REQURI,
                priority));
    }
    else if (has_action(ACTION_ENABLEREQUESTPARAMINSPECTION, mr)) {
        return action_ptr(
            new SetFlag(
                "FLAGS:inspectRequestParams",
                IB_TX_FINSPECT_REQPARAMS,
                priority));
    }
    else if (has_action(ACTION_DISABLEREQUESTPARAMINSPECTION, mr)) {
        return action_ptr(
            new UnsetFlag(
                "FLAGS:inspectRequestParams",
                IB_TX_FINSPECT_REQPARAMS,
                priority));
    }
    else if (has_action(ACTION_ENABLEREQUESTBODYINSPECTION, mr)) {
        return action_ptr(
            new SetFlag(
                "FLAGS:inspectRequestBody",
                IB_TX_FINSPECT_REQBODY,
                priority));
    }
    else if (has_action(ACTION_DISABLEREQUESTBODYINSPECTION, mr)) {
        return action_ptr(
            new UnsetFlag(
                "FLAGS:inspectRequestBody",
                IB_TX_FINSPECT_REQBODY,
                priority));
    }
    else if (has_action(ACTION_ENABLERESPONSEHEADERINSPECTION, mr)) {
        return action_ptr(
            new SetFlag(
                "FLAGS:inspectResponseHeader",
                IB_TX_FINSPECT_RESHDR,
                priority));
    }
    else if (has_action(ACTION_DISABLERESPONSEHEADERINSPECTION, mr)) {
        return action_ptr(
            new UnsetFlag(
                "FLAGS:inspectResponseHeader",
                IB_TX_FINSPECT_RESHDR,
                priority));
    }
    else if (has_action(ACTION_ENABLERESPONSEBODYINSPECTION, mr)) {
        return action_ptr(
            new SetFlag(
                "FLAGS:inspectResponseBody",
                IB_TX_FINSPECT_RESBODY,
                priority));
    }
    else if (has_action(ACTION_DISABLERESPONSEBODYINSPECTION, mr)) {
        return action_ptr(
            new UnsetFlag(
                "FLAGS:inspectResponseBody",
                IB_TX_FINSPECT_RESBODY,
                priority));
    }

    BOOST_THROW_EXCEPTION(
        IronBee::einval()
            << IronBee::errinfo_what(
                "Unknown action: "+std::string(mr[1]))
    );
}

bool ActionFactory::has_action(const char action[], boost::cmatch& m)
{
    return boost::iequals(std::string(action), std::string(m[1]));
}
/* End ActionFactory Impl */

/* XRule Impl */
XRule::XRule(action_ptr action)
:
    m_action(action)
{
    assert(m_action.get());
}

XRule::XRule(){}

XRule::~XRule() {}

void XRule::operator()(IronBee::Transaction tx, ActionSet &actions)
{
    xrule_impl(tx, actions);
}

void XRule::xrule_impl(IronBee::Transaction tx, ActionSet& actions)
{
    BOOST_THROW_EXCEPTION(
        IronBee::enotimpl()
            << IronBee::errinfo_what("XRules must implement xrule_impl.")
    );
}

/* End XRule Impl */


/* XRulesModuleConfig Impl */
XRulesModuleConfig::XRulesModuleConfig(IronBee::Module module)
:
    generate_events(false),
    xrules_collection(
        IronBee::VarSource::acquire(
            module.engine().main_memory_mm(),
            module.engine().var_config(),
            "XRULES"
        )
    ),
    xrules_scale_threat(
        IronBee::VarFilter::acquire(
            module.engine().main_memory_mm(),
            "SCALE_THREAT"
        )
    )
{
}
/* End XRulesModuleConfig Impl */


/* XRulesModule */

bool XRulesModule::is_tx_empty(IronBee::ConstTransaction tx) const {
    return (! (tx.flags() & (IB_TX_FREQ_HAS_DATA | IB_TX_FRES_HAS_DATA)));
}

/* XRulesModule Impl */
XRulesModule::XRulesModule(IronBee::Module module) :
    IronBee::ModuleDelegate(module),
    m_action_factory(module.engine())
{
    assert(module);

    module.engine().register_hooks()
        .request_header_finished(
            boost::bind(
                &XRulesModule::on_request_header_finished,
                this,
                _1,
                _2
            )
        )
        .handle_response_header(
            boost::bind(
                &XRulesModule::on_handle_response_header,
                this,
                _1,
                _2
            )
        )
        .transaction_started(
            boost::bind(
                &XRulesModule::on_transaction_started,
                this,
                _1,
                _2
            )
        )
        .context_close(
            boost::bind(
                &XRulesModule::build_ip_xrule,
                this,
                _1,
                _2
            )
        )
        .handle_logevent(
            boost::bind(
                &XRulesModule::on_logging_event,
                this,
                _1,
                _2,
                _3
            )
        )
        .handle_response(
            boost::bind(
                &XRulesModule::disable_xrule_events,
                this,
                _1,
                _2
            )
        );

    /* Register configuration directives. */
    module.engine().register_configuration_directives().
        list(
            "XRuleIpv4",
            boost::bind(
                &XRulesModule::xrule_directive, this, _1, _2, _3)).
        list(
            "XRuleIpv6",
            boost::bind(
                &XRulesModule::xrule_directive, this, _1, _2, _3)).
        list(
            "XRuleGeo",
            boost::bind(
                &XRulesModule::xrule_directive, this, _1, _2, _3)).
        list(
            "XRulePath",
            boost::bind(
                &XRulesModule::xrule_directive, this, _1, _2, _3)).
        list(
            "XRuleTime",
            boost::bind(
                &XRulesModule::xrule_directive, this, _1, _2, _3)).
        list(
            "XRuleRequestContentType",
            boost::bind(
                &XRulesModule::xrule_directive, this, _1, _2, _3)).
        list(
            "XRuleResponseContentType",
            boost::bind(
                &XRulesModule::xrule_directive, this, _1, _2, _3)).
        list(
            "XRuleEventTag",
            boost::bind(
                &XRulesModule::xrule_directive, this, _1, _2, _3)).
        list(
            "XRuleParam",
            boost::bind(
                &XRulesModule::xrule_directive, this, _1, _2, _3)).
        list(
            "XRuleCookie",
            boost::bind(
                &XRulesModule::xrule_directive, this, _1, _2, _3)).
        list(
            "XRuleRequestHeader",
            boost::bind(
                &XRulesModule::xrule_directive, this, _1, _2, _3)).
        list(
            "XRuleMethod",
            boost::bind(
                &XRulesModule::xrule_directive, this, _1, _2, _3)).
        list(
            "XRuleHostname",
            boost::bind(
                &XRulesModule::xrule_directive, this, _1, _2, _3)).
        list(
            "XRuleException",
            boost::bind(
                &XRuleException::xrule_directive, *this, _1, _2, _3)).
        on_off(
            "XRuleGenerateEvent",
            boost::bind(
                &XRulesModule::xrule_gen_event_directive, *this, _1, _2, _3));

    module.set_configuration_data<XRulesModuleConfig>(module);
}

void XRulesModule::build_ip_xrule(IronBee::Engine ib, IronBee::Context ctx) {
    XRulesModuleConfig &cfg =
        module().configuration_data<XRulesModuleConfig>(ctx);

    cfg.req_xrules.push_back(xrule_ptr(new XRuleIP(cfg)));
}

void XRulesModule::disable_xrule_events(IronBee::Engine ib, IronBee::Transaction tx) {
    XRulesModuleConfig &cfg =
        module().configuration_data<XRulesModuleConfig>(tx.context());

    cfg.generate_events = false;
}

action_ptr XRulesModule::parse_action(
    IronBee::ConfigurationParser     cp,
    IronBee::ConstList<const char *> list,
    IronBee::List<const char *>      unparsed
)
{
    int  priority = 10;
    const char *action_text = NULL;

    IronBee::ConstList<const char *>::reverse_iterator itr = list.rbegin();

    for (; itr != list.rend(); ++itr)
    {
        ib_cfg_log_debug(cp.ib(), "Parsing arg %s.", *itr);
        if (boost::istarts_with(*itr, "priority="))
        {
            priority = atoi((*itr) + sizeof("priority=")-1);
        }
        /* When we match no assignment, break the loop.
         * We are pointing at an action. */
        else {
            action_text = *itr;
            break;
        }
    }

    if (action_text == NULL)
    {
        BOOST_THROW_EXCEPTION(
            IronBee::einval()
                << IronBee::errinfo_what("No action text.")
        );
    }

    /* Push remaining arguments to the out-list. */
    for (++itr; itr != list.rend(); ++itr) {
        unparsed.push_front(*itr);
    }

    ib_cfg_log_debug(
        cp.ib(),
        "Building action \"%s\" with priority %d.",
        action_text,
        priority);

    return m_action_factory.build(action_text, priority);
}

void XRulesModule::xrule_gen_event_directive(
    IronBee::ConfigurationParser     cp,
    const char *                     name,
    bool                             on
)
{
    IronBee::Context   ctx = cp.current_context();
    XRulesModuleConfig &cfg =
        module().configuration_data<XRulesModuleConfig>(ctx);
    cfg.generate_events = on;
}

void XRulesModule::xrule_directive(
    IronBee::ConfigurationParser     cp,
    const char *                     name,
    IronBee::ConstList<const char *> all_params
)
{
    const static std::string EVENT_TAG("event-tag:");

    std::string        name_str(name);
    IronBee::Context   ctx = cp.current_context();
    XRulesModuleConfig &cfg =
        module().configuration_data<XRulesModuleConfig>(ctx);

    /* The unparsed bits from parsing an action out of the params arg. */
    IronBee::List<const char *> params =
        IronBee::List<const char *>::create(cp.memory_manager());

    /* Parse the action and put the remaining tokens in `params`. */
    action_ptr action = parse_action(cp, all_params, params);

    if (params.empty()) {
        BOOST_THROW_EXCEPTION(
            IronBee::einval()
                << IronBee::errinfo_what("XRules require at least 1 argument.")
        );
    }

    if (boost::iequals(name_str, "XRuleIpv4")) {
        /* Copy in an empty, uninitialized ipset entry. */
        ib_ipset4_entry_t entry;

        memset(&entry, 0, sizeof(entry));

        const char *net = XRuleIP::normalize_ipv4(
            cp.memory_manager(), params.front());

        IronBee::throw_if_error(
            ib_ip4_str_to_net(net, &(entry.network)),
            (std::string("Failed to get net from string: ")+net).c_str()
        );

        action->logevent_msg() =
            std::string("IPv4 ") + net + ": "+action->logevent_msg();
        action->logevent_tag() = "xrule/ipv4";

        /* Put that action in the ip set. */
        entry.data = IronBee::value_to_data<action_ptr>(
            action,
            cp.engine().main_memory_mm().ib());

        cfg.ipv4_list.push_back(entry);
    }
    else if (boost::iequals(name_str, "XRuleIpv6")) {
        // Copy in an empty, uninitialized ipset entry.
        ib_ipset6_entry_t entry;

        memset(&entry, 0, sizeof(entry));

        const char *net = XRuleIP::normalize_ipv6(
            cp.memory_manager(), params.front());

        IronBee::throw_if_error(
            ib_ip6_str_to_net(net, &(entry.network)),
            (std::string("Failed to get net from string: ")+net).c_str()
        );

        action->logevent_msg() =
            std::string("IPv6 ") + net + ": "+action->logevent_msg();
        action->logevent_tag() = "xrule/ipv6";

        /* Put that action in the ip set. */
        entry.data = IronBee::value_to_data<action_ptr>(
            action,
            cp.engine().main_memory_mm().ib());

        cfg.ipv6_list.push_back(entry);
    }
    else if (boost::iequals(name_str, "XRuleGeo")) {
        action->logevent_msg() =
            std::string("Geo ") +
            params.front()+
            ": "+
            action->logevent_msg();
        action->logevent_tag() = "xrule/geo";
        cfg.req_xrules.push_back(
            xrule_ptr(
                new XRuleGeo(params.front(), action)));
    }
    else if (boost::iequals(name_str, "XRulePath")) {
        action->logevent_msg() =
            std::string("Path ") +
            params.front()+
            ": "+
            action->logevent_msg();
        action->logevent_tag() = "xrule/path";
        cfg.req_xrules.push_back(
            xrule_ptr(
                new XRulePath(params.front(), action)));
    }
    else if (boost::iequals(name_str, "XRuleTime")) {
        action->logevent_msg() =
            std::string("Time ") +
            params.front()+
            ": "+
            action->logevent_msg();
        action->logevent_tag() = "xrule/time";
        cfg.req_xrules.push_back(
            xrule_ptr(
                new XRuleTime(cp, params.front(), action)));
    }
    else if (boost::iequals(name_str, "XRuleRequestContentType")) {
        action->logevent_msg() =
            std::string("RequestContentType ") +
            params.front()+
            ": "+
            action->logevent_msg();
        action->logevent_tag() = "xrule/content_type/request";
        cfg.req_xrules.push_back(
            xrule_ptr(
                new XRuleContentType(
                    params.front(),
                    action,
                    "request_headers:Content-Type",
                    "request_headers:Content-Length",
                    "request_headers:Transport-Encoding")));
    }
    else if (boost::iequals(name_str, "XRuleResponseContentType")) {
        action->logevent_msg() =
            std::string("ResponseContentType ") +
            params.front()+
            ": "+
            action->logevent_msg();
        action->logevent_tag() = "xrule/content_type/response";
        cfg.resp_xrules.push_back(
            xrule_ptr(
                new XRuleContentType(
                    params.front(),
                    action,
                    "response_headers:Content-Type",
                    "response_headers:Content-Length",
                    "response_headers:Transport-Encoding")));
    }
    else if (boost::iequals(name_str, "XRuleEventTag")) {
        action->logevent_msg() =
            std::string("EventTag ") +
            params.front()+
            ": "+
            action->logevent_msg();
        action->logevent_tag() = "xrule/event_tag";
        cfg.event_xrules.push_back(
            xrule_ptr(
                new XRuleEventTag(params, action)));
    }
    else if (boost::iequals(name_str, "XRuleParam")) {
        action->logevent_msg() =
            std::string("Param ") +
            ": "+
            action->logevent_msg();
        action->logevent_tag() = "xrule/param";
        cfg.req_xrules.push_back(
            xrule_ptr(
                new XRuleParam(params.front(), cp.engine(), action)));
    }
    else if (boost::iequals(name_str, "XRuleCookie")) {
        action->logevent_msg() =
            std::string("Cookie ") +
            ": "+
            action->logevent_msg();
        action->logevent_tag() = "xrule/cookie";
        cfg.req_xrules.push_back(
            xrule_ptr(
                new XRuleCookie(params.front(), cp.engine(), action)));
    }
    else if (boost::iequals(name_str, "XRuleRequestHeader")) {
        action->logevent_msg() =
            std::string("RequestHeader ") +
            ": "+
            action->logevent_msg();
        action->logevent_tag() = "xrule/requestheader";
        cfg.req_xrules.push_back(
            xrule_ptr(
                new XRuleRequestHeader(params.front(), action)));
    }
    else if (boost::iequals(name_str, "XRuleMethod")) {
        action->logevent_msg() =
            std::string("Method ") +
            ": "+
            action->logevent_msg();
        action->logevent_tag() = "xrule/method";
        cfg.req_xrules.push_back(
            xrule_ptr(
                new XRuleMethod(params.front(), action)));
    }
    else if (boost::iequals(name_str, "XRuleHostname")) {
        action->logevent_msg() =
            std::string("Hostname ") +
            ": "+
            action->logevent_msg();
        action->logevent_tag() = "xrule/hostname";
        cfg.req_xrules.push_back(
            xrule_ptr(
                new XRuleHostname(params.front(), action)));
    }

    else {
        ib_cfg_log_error(cp.ib(), "Unknown directive: %s", name);
        BOOST_THROW_EXCEPTION(
            IronBee::einval() <<
                IronBee::errinfo_what("Unknown directive.")
        );
    }
}

void XRulesModule::on_transaction_started(
    IronBee::Engine      ib,
    IronBee::Transaction tx
)
{
    xrules_module_tx_data_ptr mdata(new XRulesModuleTxData());

    tx.set_module_data(module(), mdata);
}

void XRulesModule::on_logging_event(
    IronBee::Engine      ib,
    IronBee::Transaction tx,
    IronBee::LogEvent    logevent
)
{
    ActionSet actions;

    IronBee::Context ctx = tx.context();

    XRulesModuleConfig &cfg =
        module().configuration_data<XRulesModuleConfig>(ctx);

    xrules_module_tx_data_ptr mdata =
        tx.get_module_data<xrules_module_tx_data_ptr>(module());

    BOOST_FOREACH(xrule_ptr xrule, cfg.event_xrules) {
        (*xrule)(tx, actions);
    }

    actions.apply(cfg, mdata, tx);
}

void XRulesModule::on_handle_response_header(
    IronBee::Engine      ib,
    IronBee::Transaction tx
)
{
    if (is_tx_empty(tx)) {
        ib_log_debug_tx(tx.ib(), "Empty tx. Skipping response XRules.");
        return;
    }

    IronBee::Context ctx = tx.context();
    XRulesModuleConfig &cfg =
        module().configuration_data<XRulesModuleConfig>(ctx);
    xrules_module_tx_data_ptr mdata =
        tx.get_module_data<xrules_module_tx_data_ptr>(module());
    ActionSet &actions = mdata->response_actions;

    BOOST_FOREACH(xrule_ptr xrule, cfg.resp_xrules) {
        (*xrule)(tx, actions);
    }

    actions.apply(cfg, mdata, tx);
}

void XRulesModule::on_request_header_finished(
    IronBee::Engine      ib,
    IronBee::Transaction tx
)
{
    if (is_tx_empty(tx)) {
        ib_log_debug_tx(tx.ib(), "Empty tx. Skipping request XRules.");
        return;
    }

    IronBee::Context ctx = tx.context();
    XRulesModuleConfig &cfg =
        module().configuration_data<XRulesModuleConfig>(ctx);
    xrules_module_tx_data_ptr mdata =
        tx.get_module_data<xrules_module_tx_data_ptr>(module());
    ActionSet &actions = mdata->request_actions;

    BOOST_FOREACH(xrule_ptr xrule, cfg.req_xrules) {
        (*xrule)(tx, actions);
    }

    actions.apply(cfg, mdata, tx);
}

/* End XRulesModule Impl */

IBPP_BOOTSTRAP_MODULE_DELEGATE("XRulesModule", XRulesModule);
