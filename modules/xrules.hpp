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
 * @brief IronBee Modules --- XRule
 *
 * XRules that implement basic ACL functionality.
 *
 * These XRules implement complex logic for the purposes of
 * doing checks that are beyond a general rule or can take advantage of
 * optimizations that normal Rules may not.
 *
 * @author Sam Baskinger <sbaskinger@qualys.com>
 */

#ifndef __MODULES__XRULES_HPP
#define __MODULES__XRULES_HPP

#include <ironbeepp/module_bootstrap.hpp>
#include <ironbeepp/module_delegate.hpp>
#include <ironbeepp/transaction.hpp>

#include <ironbee/ipset.h>

#include <boost/regex.hpp>
#include <boost/shared_ptr.hpp>

#include <string>
#include <vector>

class Action;

class XRule;

class XRulesModuleConfig;

struct XRulesModuleTxData;

typedef boost::shared_ptr<XRule> xrule_ptr;

typedef boost::shared_ptr<XRulesModuleTxData> xrules_module_tx_data_ptr;

typedef boost::shared_ptr<Action> action_ptr;

/**
 * An action is a change to a transaction object.
 *
 * Actions have priorities and ids. If two actions have the same ID, then
 * they have conflicting effects and only one should be executed. The
 * one with the lower value for m_priority (or, the higher priority)
 * is preferred. The lower priority action should be discarded.
 */
class Action {

private:

    //! Optional message to log with an Observation event when this fires.
    std::string m_logevent_msg;

    //! Tag to use when logging events.
    std::string m_tag;

    /**
     * Apply this action to the transaction.
     *
     * This function must be overridden.
     *
     * @param[in] config Module configuration.
     * @param[in] mdata The module.
     * @param[in] tx The current transaction.
     */
    virtual void apply_impl(
        const XRulesModuleConfig& config,
        xrules_module_tx_data_ptr mdata,
        IronBee::Transaction tx
    ) const;

    //! Default argument to constructor.
    static const std::string DEFAULT_LOG_MESSAGE;

    //! The tag to use in a generated event.
    static const std::string DEFAULT_TAG;

protected:
    /**
     * The priority of this action.
     *
     * The priority, if a lower number, (a higher priority), allows
     * actions with the same Action::m_id to override others.
     */
    const int m_priority;

    /**
     * Unique identifier of what this action affects.
     *
     * Actions that have conflicting results should have conflicting IDs.
     *
     * Actions with unrelated results should not different IDs.
     */
    const std::string m_id;

    /**
     * Constructor of an action.
     *
     * @param[in] id ID under which actions of this type are stored.
     *            Descendant class name is sufficient.
     * @param[in] priority The priority of this action.
     * @param[in] logevent_msg If this string is non-empty the Action
     *            will create an Observation event with this as the message.
     * @param[in] tag Tag to attach to Observation event.
     */
    Action(
        const std::string& id,
        int priority,
        const std::string& logevent_msg = DEFAULT_LOG_MESSAGE,
        const std::string& tag = DEFAULT_TAG
    );

public:

    /**
     * Return if the given Action overrides @c *this action.
     *
     * Actions of the same id and equal priority may override each
     * other. That is, it is possible to have two actions,
     * {{action1}} and {{action2}} with the same ID and equal priority:
     *
     * @code
     * action1.overrides(action2) == true;
     * action2.overrides(action1) == true;
     * @endcode
     *
     * @param[in] that The Action we would like to use to
     *            update this action.
     *
     * @returns True if @a that should override @c *this.
     */
    bool overrides(const Action& that);

    /**
     * Apply this action to the transaction.
     *
     * Descendants must implement this.
     *
     * @param[in] config Module configuration.
     * @param[in] mdata The module data.
     * @param[in] tx Transaction.
     */
    void operator()(
        const XRulesModuleConfig& config,
        xrules_module_tx_data_ptr mdata,
        IronBee::Transaction tx
    );

    /**
     * Return the reference to the message.
     *
     * @returns A reference to the message.
     */
    std::string& logevent_msg();

    /**
     * Return the reference to the tag.
     *
     * @returns The reference to the tag.
     */
    std::string& logevent_tag();

    /**
     * Defined comparison of Actions to allow use in std::map.
     *
     * Compare Action::m_id of the two objects to find the result.
     *
     * @param[in] that The other Action to compare.
     *
     * @returns True if @c *this is less than @a that.
     */
    bool operator<(const Action &that) const;

    //! Destructor.
    virtual ~Action();
};

/**
 * A collection of actions to be applied.
 *
 * Since Actions can override each other if their IDs match,
 * this container of Actions is provided.
 */
class ActionSet {

public:

    /**
     * Set an action in this ActionSet.
     *
     * If @a action has a higher or equal priority than an existing
     * action with the same ID, it is added. Otherwise, it is
     * not added.
     *
     * @param[in] action The action to add.
     */
    void set(action_ptr& action);

    /**
     * Apply all actions in this ActionSet to @a tx.
     *
     * @param[in] config Module configuration.
     * @param[in] mdata The module data.
     * @param[in] tx The transaction to affect.
     */
    void apply(
        const XRulesModuleConfig& config,
        xrules_module_tx_data_ptr mdata,
        IronBee::Transaction tx
    );

    /**
     * Check if a given action overrides an action in this set.
     * This may be used to avoid executing a rule if it will not
     * override an action.
     *
     * @param[in] action The action to check.
     */
    bool overrides(action_ptr action);

private:
    std::map<Action, action_ptr> m_actions;
};

/**
 * This class stores the current transaction-level data for this module.
 *
 * This class is mostly responsible for managing Actions that may
 * or may not be applied to a transaction.
 */
struct XRulesModuleTxData {
    /**
     * Actions executed at the beginning of a request.
     */
    ActionSet request_actions;

    /**
     * Actions executed at the beginning of a response.
     */
    ActionSet response_actions;

    /**
     * ScaleThreat actions manipulates this when they are executed.
     */
    ib_float_t scale_threat;

    //! Enable or disable generating events when XRules fire actions.
    bool generate_events;

    /**
     * XRuleExcepton facts indexed by a pointer to the ConclusionAction.
     */
    std::map<const Action*, std::vector<int> > exception_facts;

    /**
     * Constructor to set defaults.
     */
    XRulesModuleTxData() : scale_threat(0.0), generate_events(true) {}
};

/**
 * Parses and builds appropriate actions.
 */
class ActionFactory {

public:

    //! Constructor.
    ActionFactory(IronBee::Engine ib);

    /**
     * Build an action object based on the @a arg.
     *
     * The action will have its message and tag set to the default
     * of the action type's constructor. The caller should modify these.
     *
     * @param[in] arg Argument to parse.
     * @param[in] priority The priority the resultant @a action should have.
     */
    action_ptr build(const char *arg, int priority);

private:
    //! The regular expression used to parse out action name and param.
    static boost::regex name_val_re;

    //! Engine. Used primarily for logging.
    IronBee::Engine m_ib;

    /**
     * Check if the given match has an action.
     *
     * @param[in] action The name of the actions as a C string.
     * @param[in] The match where sub-match 1 is the action name.
     *
     * @returns True if the action names matches @c m[1]. False otherwise.
     */
    static bool has_action(const char action[], boost::cmatch& m);
};

/**
 * A rule is a check (defined by the XRule class) and an action.
 */
class XRule {

private:

    /**
     * Throws IronBee::enotimpl() and must be overridden by child classes.
     *
     * @param[in] tx Transaction to check.
     * @param[in] actions The set of current actions.
     *            Implementors may inspect this (such as using
     *            ActionSet::overrides()) to check if
     *            a XRule should even execute.
     */
    virtual void xrule_impl(IronBee::Transaction tx, ActionSet& actions);

protected:

    /**
     * An action that this XRule may choose to use if a XRule succeeds.
     *
     * Child classes of XRule should use this if they carry a single
     * action as a result of the XRule succeeding.
     *
     * Other classes, such as XRuleIP, do not use this filed,
     * and it remains empty.
     *
     * This action is not implicitly added to any ActionSet.
     * An descendant of XRule must explicitly add m_action
     * to an ActionSet in XRule::xrule_impl().
     */
    action_ptr m_action;

    /**
     * Constructor only descendants can call.
     *
     * @param[in] action The action to execute.
     */
    explicit XRule(action_ptr action);

    //! Empty constructor. The action is invalid.
    XRule();

public:

    /**
     * Evaluate the rule against the given transaction.
     *
     * @param[in] tx The Transaction.
     * @param[in] actions The set of actions to update according to
     *            the rule results.
     *
     * @returns The action to execute.
     */
    void operator()(IronBee::Transaction tx, ActionSet &actions);

    //! Destructor.
    virtual ~XRule();
};

/**
 * Module configuration.
 */
class XRulesModuleConfig {

public:
    //! Are XRules enabled for this context.
    bool generate_events;

    //! List of IPv4 configurations.
    std::vector<ib_ipset4_entry_t> ipv4_list;

    //! List of IPv6 configurations.
    std::vector<ib_ipset6_entry_t> ipv6_list;

    //! List of xrules to execute for the request.
    std::list<xrule_ptr> req_xrules;

    //! List of xrules to execute for the response.
    std::list<xrule_ptr> resp_xrules;

    //! List of xrules to execute for event creation.
    std::list<xrule_ptr> event_xrules;

    XRulesModuleConfig() : generate_events(false)
    {}
};

/**
 * Implement simple policy changes when the IronBee engines is to shutdown.
 */
class XRulesModule : public IronBee::ModuleDelegate
{

public:

    /**
     * Constructor.
     *
     * @param[in] module The IronBee++ module.
     */
    explicit XRulesModule(IronBee::Module module);

    /**
     * Parse the @a list of parameters and assign @a action.
     *
     * As a side-effect all unparsed parameters are placed in @a unparsed.
     *
     * @param[in] cp Configuration parser.
     * @param[in] list Parameter list.
     * @param[out] unparsed Unparsed parameters are placed here.
     *
     * @returns[out] action The action the user chose.
     * @throws IronBee::einval if priority or action is missing.
     */
    action_ptr parse_action(
        IronBee::ConfigurationParser     cp,
        IronBee::ConstList<const char *> list,
        IronBee::List<const char *>      unparsed
    );

private:
    //! Parse and build actions for each check.
    ActionFactory m_action_factory;

    /**
     * Context close callback.
     *
     * Currently this will only roll-up IP checks into a single IP check rule.
     *
     * @param[in] ib IronBee engine.
     * @param[in] ctx IronBee configuration context.
     */
    void build_ip_xrule(IronBee::Engine ib, IronBee::Context ctx);

    /**
     * Disable XRules event generation.
     *
     * This is wired into the logging phase.
     *
     * @param[in] ib IronBee engine.
     * @param[in] tx IronBee transaction.
     */
    void disable_xrule_events(IronBee::Engine ib, IronBee::Transaction tx);

    /**
     * @param[in] cp Configuration parser.
     * @param[in] name Directive name.
     * @param[in] params List of const char * parameters.
     */
    void xrule_directive(
        IronBee::ConfigurationParser      cp,
        const char *                      name,
        IronBee::ConstList<const char *>  params
    );

    /**
     * Create transaction data for the given transaction.
     *
     * @param[in] ib IronBee engine.
     * @param[in] tx The transaction being started.
     */
    void on_transaction_started(
        IronBee::Engine      ib,
        IronBee::Transaction tx
    );

    /**
     * Run any checks schedule for when logging events are created.
     *
     * @param[in] ib IronBee engine.
     * @param[in] tx The transaction being started.
     */
    void on_logging_event(
        IronBee::Engine      ib,
        IronBee::Transaction tx
    );

    /**
     * Run resp_xrules against responses.
     *
     * @param[in] ib IronBee engine.
     * @param[in] tx The current transaction.
     *
     * @throws
     * - IB_OK On success.
     * - Other on error.
     */
    void on_handle_response_header(
        IronBee::Engine      ib,
        IronBee::Transaction tx
    );

    /**
     * Run req_xrules against requests.
     *
     * @param[in] ib IronBee engine.
     * @param[in] tx The current transaction.
     *
     * @throws
     * - IB_OK On success.
     * - Other on error.
     */
    void on_request_header_finished(
        IronBee::Engine ib,
        IronBee::Transaction tx
    );

    /**
     * Handle the directive XRuleGenerateEvent.
     *
     * @param[in] cp Configuration parser.
     * @param[in] name The name of the directive.
     * @param[in] on The value of the parameter. On or off.
     */
    void xrule_gen_event_directive(
        IronBee::ConfigurationParser cp,
        const char *                 name,
        bool                         on
    );

    bool is_tx_empty(IronBee::ConstTransaction tx) const;
};

#endif /* __MODULES__XRULES_HPP */
