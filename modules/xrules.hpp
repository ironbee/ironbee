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

#include <string>
#include <vector>

#include <ironbee/ipset.h>

#include <ironbeepp/module_bootstrap.hpp>
#include <ironbeepp/module_delegate.hpp>
#include <ironbeepp/transaction.hpp>

#include <boost/date_time/gregorian/gregorian_types.hpp>
#include <boost/date_time/local_time/local_time.hpp>
#include <boost/regex.hpp>
#include <boost/shared_ptr.hpp>

class Action;

class XRule;

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

    /**
     * Apply the transaction.
     *
     * This function must be overridden.
     *
     * @param[in] mdata The module.
     * @param[in] tx The current transaction.
     */
    virtual void apply_impl(
        xrules_module_tx_data_ptr mdata,
        IronBee::Transaction tx
    ) const;

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
     */
    Action(std::string id, int priority);

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
     * @param[in] mdata The module data.
     * @param[in] tx Transaction.
     */
    void operator()(
        xrules_module_tx_data_ptr mdata,
        IronBee::Transaction tx
    );

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
     * @param[in] mdata The module data.
     * @param[in] tx The transaction to affect.
     */
    void apply(xrules_module_tx_data_ptr mdata, IronBee::Transaction tx);

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

    /**
     * Constructor to set defaults.
     */
    XRulesModuleTxData() : scale_threat(0.0) {}
};

/**
 * Defines how to block a transaction.
 */
class BlockAllow : public Action {

public:

    /**
     * Construct a new BlockAllow.
     *
     * @param[in] block If true, BlockAllow will block a transaction.
     *            If false, BlockAllow will allow a transaction.
     * @param[in] priority Priority of this action.
     */
    BlockAllow(bool block, int priority);

private:
    //! Block or allow the transaction.
    bool m_block;

    /**
     * Set the block or allow flags in the @a tx.
     *
     * @param[in] mdata The module.
     * @param[in] tx The transaction to modify.
     */
    virtual void apply_impl(
        xrules_module_tx_data_ptr mdata,
        IronBee::Transaction tx
    ) const;
};

/**
 * Set a particular flag in ib_tx_t::flags.
 *
 * This is done by setting a flag in ib_tx_t::flags and setting
 * a numeric value in ib_tx_t::data to 1 or 0.
 */
class SetFlag : public Action {

public:

    /**
     * Constructor.
     *
     * @param[in] field_name The flag name. This is used to identify this
     *            action.
     * @param[in] flag A field of bits to be set.
     * @param[in] priority Sets the priority of this action to control
     *            if it may be overridden.
     */
    SetFlag(
        const std::string& field_name,
        ib_flags_t         flag,
        int                priority
    );

protected:
    //! The name of the field to set.
    std::string m_field_name;

    //! The flag to set in ib_tx_t::flags.
    ib_flags_t m_flag;

    /**
     * Set the field to 1 and set the flag in @ref ib_tx_t.
     *
     * @param[in] mdata The module.
     * @param[in] tx The transaction to be modified.
     */
    virtual void apply_impl(
        xrules_module_tx_data_ptr mdata,
        IronBee::Transaction tx
    ) const;
};

/**
 * Almost identical to SetFlag except the apply_impl method.
 */
class UnsetFlag : public SetFlag {
public:

    /**
     * Constructor.
     *
     * @param[in] field_name The flag name. This is used to identify
     *            this action.
     * @param[in] flag A field of bits to be cleared.
     * @param[in] priority Sets the priority of this action to control
     *            if it may be overridden.
     */
    UnsetFlag(
        const std::string& field_name,
        ib_flags_t         flag,
        int                priority);
protected:
    /**
     * Set the field to 0 and clear the flag in @ref ib_tx_t.
     *
     * @param[in] mdata The module.
     * @param[in] tx The transaction to be modified.
     */
    virtual void apply_impl(
        xrules_module_tx_data_ptr mdata,
        IronBee::Transaction tx
    ) const;
};

/**
 * Set @c XRULES:SCALE_THREAT in tx.
 */
class ScaleThreat : public Action {

public:

    /**
     * Constructor.
     *
     * @param[in] unique_id To ensure that each ScaleThreat
     *            action is always executed, they
     *            need unique ids.
     * @param[in] fnum The floating point number to be store in tx.
     * @param[in] priority The priority of this action.
     */
    ScaleThreat(
        std::string unique_id,
        ib_float_t fnum,
        int priority
    );

private:

    //! The value set in the transaction.
    ib_float_t m_fnum;

    /**
     * Scale the threat value in the tx data.
     *
     * @param[in] mdata Module data.
     * @param[in] tx The transaction to modify.
     */
    virtual void apply_impl(
        xrules_module_tx_data_ptr mdata,
        IronBee::Transaction tx
    ) const;
};

//! Set the blocking mode flag.
class SetBlockingMode : public SetFlag {

public:

    /**
     * Constructor.
     *
     * @param[in] enabled If true, set the XRULES:BLOCKING_MODE to
     *            ENABLED. Set it it to DISABLED otherwise.
     * @param[in] priority Sets the priority of this action to control
     *            if it may be overridden.
     */
    SetBlockingMode(bool enabled, int priority);
};


class UnsetBlockingMode : public UnsetFlag {

public:

    /**
     * Constructor.
     *
     * @param[in] enabled If true, set the XRULES:BLOCKING_MODE to
     *            ENABLED. Set it it to DISABLED otherwise.
     * @param[in] priority Sets the priority of this action to control
     *            if it may be overridden.
     */
    UnsetBlockingMode(bool enabled, int priority);
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

    /**
     * Empty constructor. The action is invalid.
     */
    XRule();

    //! Empty constructor.

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

    //! List of IPv4 configurations.
    std::vector<ib_ipset4_entry_t> ipv4_list;

    //! List of IPv6 configurations.
    std::vector<ib_ipset6_entry_t> ipv6_list;

    //! List of xrules to execute for the request.
    std::list<xrule_ptr> req_xrules;

    //! List of xrules to execute for the response.
    std::list<xrule_ptr> resp_xrules;

    //! List of xrules toexecute for event creation.
    std::list<xrule_ptr> event_xrules;
};

/**
 * An XRule that checks the two-character country code.
 */
class XRuleGeo : public XRule {

public:

    /**
     * Constructor.
     *
     * @param[in] country The country to match.
     * @param[in] action The action to perform if this rule matches.
     */
    XRuleGeo(const char *country, action_ptr action);

    //! The field that is set.
    static const char *GEOIP_FIELD;

private:

    //! The country that will cause this rule to succeed if it matches.
    std::string m_country;

    /**
     * Check if GEOIP_FIELD is set to the value in m_country.
     *
     * If it is, the XRule succeeds and m_action is added to @a actions.
     *
     * @param[in] tx The transaction to check.
     * @param[in] actions The current set of actions.
     */
    virtual void xrule_impl(IronBee::Transaction tx, ActionSet& actions);
};

/**
 * Check if a content type matches.
 */
class XRuleContentType : public XRule {

public:

    /**
     * Constructor.
     *
     * @param[in] content_type Content type to match.
     * @param[in] action Action to take on match.
     * @param[in] content_type_field Target holding content type.
     * @param[in] content_length_field Target holding content length.
     * @param[in] transport_encoding_field Target holding
     *            transport encoding.
     */
    XRuleContentType(
        const char* content_type,
        action_ptr action,
        const std::string content_type_field,
        const std::string content_length_field,
        const std::string transport_encoding_field
    );

private:
    bool                  m_any;
    bool                  m_none;
    std::string           m_content_type_field;
    std::string           m_content_length_field;
    std::string           m_transport_encoding_field;
    std::set<std::string> m_content_types;

    bool has_field(IronBee::Transaction tx, std::string &field);

    virtual void xrule_impl(
        IronBee::Transaction tx,
        ActionSet&            actions
    );
};

/**
 * Check that the request path prefix starts with a known string.
 */
class XRulePath : public XRule {

public:

    /**
     * Constructor.
     *
     * @param[in] path The path prefix to match.
     * @param[in] action Action to take on match.
     */
    XRulePath(const char *path, action_ptr action);

private:
    //! Path to check the HTTP request for.
    std::string m_path;

    /**
     * Check if the request path in @a tx is prefixed with m_path.
     *
     * @param[in] tx The transaction.
     * @param[in] actions The action set to modify if a match is found.
     */
    virtual void xrule_impl(
        IronBee::Transaction tx,
        ActionSet&            actions
    );
};

/**
 * Check if the start time of a tx falls in (or out of) a time window.
 */
class XRuleTime : public XRule {

public:

    /**
     * Constructor.
     * @param[in] cp Configuration parser.
     * @param[in] time Time window string. This string has a
     *            specific format.
     *            - 08:00-17:00-600 This is the most simple format,
     *              8am to 5pm offset from GMT by -6 hours.
     *            - !08:00-17:00-600 Prefixing the string with a !
     *              will invert the window; "Not in this time window."
     *            - !1,2,3,4,5@08:00-17:00-0600 A list of days
     *            (as integers where 0 is Sunday) may be added
     *            to denote a particular day that the time window should
     *            apply to.
     * @param[in] action The action executed if a given
     *            IronBee::Transaction started in the @a time window.
     */
    XRuleTime(
        IronBee::ConfigurationParser cp,
        const char *time,
        action_ptr action);

private:
    //! A set of days of the week (0 through 6 where 0 is Sunday).
    std::set<int> m_days;

    //! The posix start time for this filter.
    boost::posix_time::ptime m_start_time;

    //! The posix end time for this filter.
    boost::posix_time::ptime m_end_time;

    //! Should the rule check result be inverted (outside the window).
    bool m_invert;

    //! The amount incoming times to be checked are shifted.
    boost::local_time::time_zone_ptr m_zone_info;

    /**
     * Parse a time value.
     *
     * @param[in] str The time zone to parse. This
     *            must be of the format @c [+-]dddd.
     * @param[out] zone The zone object to output.
     */
    static void parse_time_zone(
        IronBee::ConfigurationParser      cp,
        const char*                       str,
        boost::local_time::time_zone_ptr& zone
    );

    /**
     * Parse a time value.
     *
     * @param[in] str The time to parse.
     * @param[out] p The posix time it is stored in.
     */
    static void parse_date_time(
        IronBee::ConfigurationParser  cp,
        const char                   *str,
        boost::posix_time::ptime&     p
    );

    /**
     * Check if @a tx is in the time window.
     *
     * The rule check is true if IronBee::Transaction::started_time()
     * - occurs in any specified days in XRuleTime::m_days
     * - and occurs at or after the XRuleTime::m_start_time
     * - and occurs before the XRuleTime::m_end_time.
     *
     * If XRuleTime::m_invert is true, then the rule check is inverted.
     *
     * If the rule check is true, then the associated action is executed.
     * Otherwise the action is not executed.
     */
    virtual void xrule_impl(
        IronBee::Transaction tx,
        ActionSet&            actions
    );
};

/**
 * Map the client IP address of an IronBee::Transaction to an Action.
 *
 * Unlike a normal XRule that maps a single check to a single action,
 * for efficient evaluation, this XRule is constructed
 * after the IronBee configuration phase and wraps all IPs into
 * a @ref ib_ipset4_t or @ref ib_ipset6_t and does a single check
 * for the most precise match. That match results in a pointer
 * to an Action wrapped by IronBee::value_to_data().
 */
class XRuleIP : public XRule {

public:

    /**
     * Build a single rule check for the configuration context.
     *
     * @param[in] cfg The configuration for the closing configuration c
     *            context. The IPv4 and IPv6 lists are used from
     *            this configuration context to build the final rule.
     */
    explicit XRuleIP(XRulesModuleConfig& cfg);

private:

    //! IPv4 set holding pointers to Actions.
    ib_ipset4_t m_ipset4;

    //! IPv6 set holding pointers to Actions.
    ib_ipset6_t m_ipset6;

    /**
     * Check if @a tx's remote ip is mapped to an action.
     *
     * This is done by taking
     * IronBee::Transaction::effective_remote_ip_string()
     * and checking if it can be converted to a @ref ib_ip4_t or
     * an @ref ib_ip6_t and checking if that value is in
     * XRuleIP::m_ipset4 or XRuleIP::m_ipset6, respectively.
     *
     * @param[in] tx The transaction to check.
     * @param[in] actions The ActionSet to edit.
     */
    virtual void xrule_impl(
        IronBee::Transaction tx,
        ActionSet&            actions
    );
};

class XRuleEventTag : public XRule {

public:
    XRuleEventTag(IronBee::ConstList<const char *> tags, action_ptr action);

private:

    /**
     * List of tags to check.
     */
    std::vector<std::string> m_tags;

    /**
     * @param[in] tx The transaction to check.
     * @param[in] actions The ActionSet to edit.
     */
    virtual void xrule_impl(
        IronBee::Transaction tx,
        ActionSet&            actions
    );

};

/**
 * Implement simple policy changes when the IronBee engines is to shutdown.
 */
class XRulesModule : public IronBee::ModuleDelegate
{

public:

    /**
     * Constructor.
     * @param[in] module The IronBee++ module.
     */
    explicit XRulesModule(IronBee::Module module);

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

    bool is_tx_empty(IronBee::ConstTransaction tx) const;
};

#endif /* __MODULES__XRULES_HPP */