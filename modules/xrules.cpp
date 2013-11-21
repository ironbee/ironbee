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

#include <ironbeepp/configuration_directives.hpp>
#include <ironbeepp/configuration_parser.hpp>
#include <ironbeepp/connection.hpp>
#include <ironbeepp/engine.hpp>
#include <ironbeepp/hooks.hpp>
#include <ironbeepp/list.hpp>
#include <ironbeepp/module_bootstrap.hpp>
#include <ironbeepp/module_delegate.hpp>
#include <ironbeepp/parsed_name_value.hpp>
#include <ironbeepp/transaction.hpp>

/* C includes. */
#include <ironbee/data.h>
#include <ironbee/engine.h>
#include <ironbee/flags.h>
#include <ironbee/ip.h>
#include <ironbee/ipset.h>
#include <ironbee/log.h>
#include <ironbee/server.h>
#include <ironbee/string.h>

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/date_time/gregorian/gregorian_types.hpp>
#include <boost/date_time/local_time/local_time.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/time_zone_base.hpp>
#include <boost/foreach.hpp>
#include <boost/regex.hpp>
#include <boost/shared_ptr.hpp>

#include <list>
#include <vector>

namespace {

    struct XRulesModuleTxData;
    typedef boost::shared_ptr<XRulesModuleTxData> xrules_module_tx_data_ptr;

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
    typedef boost::shared_ptr<Action> action_ptr;

    /* Action Impl. */
    Action::Action(std::string id, int priority) : m_priority(priority)
    {}

    Action::~Action()
    {}

    void Action::operator()(
        xrules_module_tx_data_ptr mdata,
        IronBee::Transaction tx
    )
    {
        apply_impl(mdata, tx);
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
        xrules_module_tx_data_ptr mdata,
        IronBee::Transaction tx
    )
    {
        for(
            std::map<Action, action_ptr>::iterator itr = m_actions.begin();
            itr != m_actions.end();
            ++itr
        ) {
            (*(itr->second))(mdata, tx);
        }

        /* After applying the TX, set the value. */
        IronBee::Field f = IronBee::Field::create_float(
            tx.memory_pool(),
            IB_S2SL("XRULES:SCALE_THREAT"),
            mdata->scale_threat);

        IronBee::throw_if_error(
            ib_data_add(tx.ib()->data, f.ib()),
            "Failed to add Scale Threat field to tx.");
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
         * @param[in] Priority of this action.
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

    /* BlockAllow Impl */
    BlockAllow::BlockAllow(bool block, int priority)
    :
        Action("BlockAllow", priority),
        m_block(block)
    {}

    void BlockAllow::apply_impl(
        xrules_module_tx_data_ptr mdata,
        IronBee::Transaction tx
    ) const
    {
        if (m_block) {
            ib_log_debug_tx(tx.ib(), "Blocking Transaction");
            tx.ib()->flags |= IB_TX_BLOCK_IMMEDIATE;
            tx.ib()->flags &= ~(IB_TX_ALLOW_ALL);
        }
        else {
            ib_log_debug_tx(tx.ib(), "Allowing Transaction");
            tx.ib()->flags &= ~(IB_TX_BLOCK_IMMEDIATE | IB_TX_BLOCK_PHASE | IB_TX_BLOCK_ADVISORY);
            tx.ib()->flags |= IB_TX_ALLOW_ALL;
        }
    }
    /* End BlockAllow Impl */

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
         * @param[in] field_name The name of the var to set to 1 or 0
         *            depending on if the flag is turned on or off.
         * @param[in] flag A field of bits to be set or cleared.
         *            Typical usage should be to set a single
         *            bit in the bit field (a single flag),
         *            but treating multiple bits being set or cleared
         *            is not prevented from working.
         * @param[in] clear If true, then the flag is cleared. If false, the
         *            flag is set.
         * @param[in] priority Sets the priority of this action to control
         *            if it may be overridden.
         */
        SetFlag(
            const std::string& field_name,
            ib_flags_t flag,
            bool clear,
            int priority
        );

    private:
        //! The name of the field to set.
        std::string m_field_name;

        //! The flag to set in ib_tx_t::flags.
        ib_flags_t m_flag;

        //! Should the flag be cleared, rather than set.
        bool m_clear;

        /**
         * Set the field to 0 or 1 and set or clear the flag in @ref ib_tx_t.
         *
         * @param[in] mdata The module.
         * @param[in] tx The transaction to be modified.
         */
        virtual void apply_impl(
            xrules_module_tx_data_ptr mdata,
            IronBee::Transaction tx
        ) const;
    };

    /* SetFlag Impl */
    SetFlag::SetFlag(
        const std::string& field_name,
        ib_flags_t flag,
        bool clear,
        int priority
    )
    :
        Action("SetFlag_" + field_name, priority),
        m_field_name(field_name),
        m_flag(flag),
        m_clear(clear)
    {}

    void SetFlag::apply_impl(
        xrules_module_tx_data_ptr mdata,
        IronBee::Transaction tx
    ) const
    {
        ib_num_t    val;
        ib_list_t  *clist;
        ib_field_t *cfield;
        ib_status_t rc;

        if (m_clear) {
            val = 0;
            ib_tx_flags_set(tx.ib(), m_flag);
        }
        else {
            val = 1;
            ib_tx_flags_set(tx.ib(), m_flag);
        }

        rc = ib_data_get(tx.ib()->data, m_field_name.c_str(), &cfield);
        if (cfield->type != IB_FTYPE_LIST) {
            throw IronBee::einval() << IronBee::errinfo_what(
                "ib_data_get failed to return a list for field "+m_field_name);
        }

        /* If there is no FLAGS field, add the flag as a new field. */
        if (rc == IB_ENOENT) {
            IronBee::throw_if_error(
                ib_data_add_num(tx.ib()->data, m_field_name.c_str(), val, NULL)
            );
        }
        else {

            /* Otherwise, get the first entry. */
            IronBee::throw_if_error(
                ib_field_value_type(
                    cfield,
                    ib_ftype_list_mutable_out(&clist),
                    IB_FTYPE_LIST)
            );

            /* If the flag is not found in the list, add it as a new field. */
            if (ib_list_elements(clist) == 0U) {
                IronBee::throw_if_error(
                    ib_data_add_num(
                        tx.ib()->data,
                        m_field_name.c_str(),
                        val,
                        NULL)
                );
            }
            else {
                /* If the field is found, set its value in-place. */
                IronBee::Field(
                    IronBee::List<ib_field_t *>(clist).back()
                ).set_number(val);
            }
        }
    }
    /* End SetFlag Impl */

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
         * @param[in] results The calling application
         *            must provide a pointer to a float that
         *            this action may manipulate when it is executed.
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

    /* ScaleThreat Impl */
    ScaleThreat::ScaleThreat(
        std::string unique_id,
        ib_float_t fnum,
        int priority
    )
    :
        Action(std::string("ScaleThreat_") + unique_id, priority),
        m_fnum(fnum)
    {}

    void ScaleThreat::apply_impl(
        xrules_module_tx_data_ptr mdata,
        IronBee::Transaction tx
    ) const
    {
        (mdata->scale_threat) += m_fnum;
    }
    /* End ScaleThreat Impl */

    /**
     * Sets XRULE:BLOCKING_MODE = ENABLED | DISABLED.
     */
    class SetBlockingMode : public Action {

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

    private:
        //! Set the field to enabled or disabled.
        bool m_enabled;

        //! The name of the field to mark as blocked.
        static const std::string BLOCKING_MODE_FIELD_NAME;

        /**
         * Set @c XRULES:BLOCKING_MODE = @c ENABLED or @c DISABLED.
         *
         * @param[in] mdata The module.
         * @param[in] tx The transaction to be modified.
         */
        virtual void apply_impl(
            xrules_module_tx_data_ptr mdata,
            IronBee::Transaction tx
        ) const;
    };

    /* SetBlockingMode Impl */
    SetBlockingMode::SetBlockingMode(bool enabled, int priority)
    :
        Action("SetBlockingMode", priority),
        m_enabled(enabled)
    {}

    void SetBlockingMode::apply_impl(
        xrules_module_tx_data_ptr mdata,
        IronBee::Transaction tx
    ) const
    {
        typedef IronBee::ByteString BS;

        BS bs;

        if (m_enabled) {
            ib_tx_flags_set(tx.ib(), IB_TX_FBLOCKING_MODE);
            bs = BS::create(tx.memory_pool(), "ENABLED");
        }
        else {
            ib_tx_flags_unset(tx.ib(), IB_TX_FBLOCKING_MODE);
            bs = BS::create(tx.memory_pool(), "DISABLED");
        }

        IronBee::Field f = IronBee::Field::create_byte_string(
            tx.memory_pool(),
            BLOCKING_MODE_FIELD_NAME.c_str(),
            BLOCKING_MODE_FIELD_NAME.length(),
            bs);

        IronBee::throw_if_error(
            ib_data_add(tx.ib()->data, f.ib()),
            "Failed to set XRULES:BLOCKING_MODE.");
    }
    const std::string SetBlockingMode::BLOCKING_MODE_FIELD_NAME =
        "XRULES:BLOCKING_MODE";
    /* End SetBlockingMode Impl */


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
         * @param[out] action A concrete action derived from Action.
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

        ib_log_debug(m_ib.ib(), "Building action %*.s",
            (int)(mr[2].first - mr[1].first),
            mr[1].first);

        if (has_action(ACTION_BLOCK, mr)) {
            return action_ptr(new BlockAllow(true, priority));
        }
        else if (has_action(ACTION_ALLOW, mr)) {
            return action_ptr(new BlockAllow(false, priority));
        }
        else if (has_action(ACTION_ENABLEBLOCKINGMODE, mr)) {
            return action_ptr(new SetBlockingMode(true, priority));
        }
        else if (has_action(ACTION_DISABLEBLOCKINGMODE, mr)) {
            return action_ptr(new SetBlockingMode(false, priority));
        }
        else if (has_action(ACTION_SCALETHREAT, mr)) {
            ib_uuid_t uuid;
            std::vector<char> uuid_str(37);
            ib_float_t fnum;

            IronBee::throw_if_error(
                ib_uuid_create_v4(&uuid),
                "Cannot initialize v4 UUID.");
            IronBee::throw_if_error(
                ib_uuid_bin_to_ascii(&(uuid_str[0]), &uuid),
                "Cannot generate v4 UUID.");
            IronBee::throw_if_error(
                ib_string_to_float(std::string(mr[2]).c_str(), &fnum),
                "Cannot convert string to float.");

            return action_ptr(
                new ScaleThreat(
                    std::string(&(uuid_str[0]), 37),
                    fnum,
                    priority));
        }
        else if (has_action(ACTION_ENABLEREQUESTHEADERINSPECTION, mr)) {
            return action_ptr(
                new SetFlag(
                    "FLAGS:inspectRequestHeader",
                    IB_TX_FINSPECT_REQHDR,
                    false,
                    priority));
        }
        else if (has_action(ACTION_DISABLEREQUESTHEADERINSPECTION, mr)) {
            return action_ptr(
                new SetFlag(
                    "FLAGS:inspectRequestHeader",
                    IB_TX_FINSPECT_REQHDR,
                    true,
                    priority));
        }
        else if (has_action(ACTION_ENABLEREQUESTURIINSPECTION, mr)) {
            return action_ptr(
                new SetFlag(
                    "FLAGS:inspectRequestUri",
                    IB_TX_FINSPECT_REQURI,
                    false,
                    priority));
        }
        else if (has_action(ACTION_DISABLEREQUESTURIINSPECTION, mr)) {
            return action_ptr(
                new SetFlag(
                    "FLAGS:inspectRequestUri",
                    IB_TX_FINSPECT_REQURI,
                    true,
                    priority));
        }
        else if (has_action(ACTION_ENABLEREQUESTPARAMINSPECTION, mr)) {
            return action_ptr(
                new SetFlag(
                    "FLAGS:inspectRequestParams",
                    IB_TX_FINSPECT_REQPARAMS,
                    false,
                    priority));
        }
        else if (has_action(ACTION_DISABLEREQUESTPARAMINSPECTION, mr)) {
            return action_ptr(
                new SetFlag(
                    "FLAGS:inspectRequestParams",
                    IB_TX_FINSPECT_REQPARAMS,
                    true,
                    priority));
        }
        else if (has_action(ACTION_ENABLEREQUESTBODYINSPECTION, mr)) {
            return action_ptr(
                new SetFlag(
                    "FLAGS:inspectRequestBody",
                    IB_TX_FINSPECT_REQBODY,
                    false,
                    priority));
        }
        else if (has_action(ACTION_DISABLEREQUESTBODYINSPECTION, mr)) {
            return action_ptr(
                new SetFlag(
                    "FLAGS:inspectRequestBody",
                    IB_TX_FINSPECT_REQBODY,
                    true,
                    priority));
        }
        else if (has_action(ACTION_ENABLERESPONSEHEADERINSPECTION, mr)) {
            return action_ptr(
                new SetFlag(
                    "FLAGS:inspectResponseHeader",
                    IB_TX_FINSPECT_RSPHDR,
                    false,
                    priority));
        }
        else if (has_action(ACTION_DISABLERESPONSEHEADERINSPECTION, mr)) {
            return action_ptr(
                new SetFlag(
                    "FLAGS:inspectResponseHeader",
                    IB_TX_FINSPECT_RSPHDR,
                    true,
                    priority));
        }
        else if (has_action(ACTION_ENABLERESPONSEBODYINSPECTION, mr)) {
            return action_ptr(
                new SetFlag(
                    "FLAGS:inspectResponseHeader",
                    IB_TX_FINSPECT_RSPBODY,
                    false,
                    priority));
        }
        else if (has_action(ACTION_DISABLERESPONSEBODYINSPECTION, mr)) {
            return action_ptr(
                new SetFlag(
                    "FLAGS:inspectResponseHeader",
                    IB_TX_FINSPECT_RSPBODY,
                    true,
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

    typedef boost::shared_ptr<XRule> xrule_ptr;
    /* End XRule Impl */

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
    };

    /**
     * An XRule that checks the two-character country code.
     */
    class XRuleGeo : public XRule {

    public:

        /**
         * Constructor.
         *
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

    /* XRuleGeo Impl */
    XRuleGeo::XRuleGeo(const char *country, action_ptr action)
    :
        XRule(action),
        m_country(country)
    {}

    void XRuleGeo::xrule_impl(IronBee::Transaction tx, ActionSet& actions)
    {
        if (actions.overrides(m_action)) {
            ib_field_t *cfield;

            ib_log_debug_tx(
                tx.ib(),
                "Running GEO Check for %s",
                m_country.c_str());

            IronBee::throw_if_error(
                ib_data_get_ex(
                    tx.ib()->data,
                    GEOIP_FIELD,
                    strlen(GEOIP_FIELD),
                    &cfield),
                "Failed to retrieve GeoIP field.");

            IronBee::ConstField field(cfield);
            IronBee::ConstList<const ib_field_t *> ls(
                field.value_as_list<const ib_field_t *>());

            if (ls.size() < 1) {
                ib_log_info_tx(
                    tx.ib(),
                    "No GeoIP fields. Not filtering on GeoIP.");
            }
            else {
                IronBee::ConstByteString bs(
                    IronBee::ConstField(ls.front()).
                        value_as_byte_string());

                if (bs.index_of(m_country.c_str()) == 0) {
                    actions.set(m_action);
                }
            }
        }
        else {
            ib_log_debug_tx(
                tx.ib(),
                "Skipping rule as action does not override tx actions.");
        }
    }

    const char *XRuleGeo::GEOIP_FIELD = "GEOIP:country_code";
    /* End XRuleGeo Impl */

    /**
     * Check if a content type matches.
     */
    class XRuleContentType : public XRule {

    public:

        /**
         * @param[in] field REQUEST_CONTENT_TYPE or RESPONSE_CONTENT_TYPE.
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

    /* XRuleContentType Impl */
    XRuleContentType::XRuleContentType(
        const char*       content_type,
        action_ptr        action,
        const std::string content_type_field,
        const std::string content_length_field,
        const std::string transport_encoding_field
    )
    :
        XRule(action),
        m_any(false),
        m_none(false),
        m_content_type_field(content_type_field),
        m_content_length_field(content_length_field),
        m_transport_encoding_field(transport_encoding_field)
    {
        std::string content_type_str(content_type);
        std::list<std::string> result;
        boost::algorithm::split(
            result,
            content_type_str,
            boost::algorithm::is_any_of("|"));

        BOOST_FOREACH(const std::string& s, result) {
            if (s ==   "*" ||  s == "\"*\"") {
                m_any = true;
            }
            else if (s == "" || s == "\"\"") {
                m_none = true;
            }
            else {
                m_content_types.insert(s);
            }
        }
    }

    bool XRuleContentType::has_field(
        IronBee::Transaction tx,
        std::string &field
    )
    {
        ib_field_t *cfield;

        ib_status_t rc = ib_data_get(tx.ib()->data, field.c_str(), &cfield);

        if (rc == IB_ENOENT) {
            return false;
        }

        if (rc != IB_OK) {
            ib_log_error_tx(
                tx.ib(),
                "Failed to retrieve field %s.", field.c_str());
            IronBee::throw_if_error(rc, "Failed to retrieve field.");
        }

        if (cfield->type == IB_FTYPE_LIST) {
            const ib_list_t *clist;

            IronBee::throw_if_error(
                ib_field_value(cfield, ib_ftype_list_out(&clist)),
                "Failed to extract byte string from content type "
                "field.");

            return (ib_list_elements(clist) >= 1U);
        }

        return true;
    }

    void XRuleContentType::xrule_impl(
        IronBee::Transaction tx,
        ActionSet&           actions
    )
    {
        if (actions.overrides(m_action)) {
            if (m_any) {
                if
                (
                    has_field(tx, m_content_type_field) &&
                    !has_field(tx, m_content_length_field) &&
                    !has_field(tx, m_transport_encoding_field)
                )
                {
                    actions.set(m_action);
                }
            }
            else if (m_none) {
                if
                (
                    !has_field(tx, m_content_type_field) &&
                    (
                        has_field(tx, m_content_length_field) ||
                        has_field(tx, m_transport_encoding_field)
                    )
                )
                {
                    actions.set(m_action);
                }
            } else {
                ib_field_t         *cfield;
                const ib_list_t    *clist = NULL;

                // Fetch list of fields.
                IronBee::throw_if_error(
                    ib_data_get(
                        tx.ib()->data,
                        m_content_type_field.c_str(),
                        &cfield),
                    "Failed to retrieve content type field.");

                if (cfield->type != IB_FTYPE_LIST) {
                    ib_log_error_tx(
                        tx.ib(),
                        "Expected type list(%d) but field type was %d.",
                        IB_FTYPE_LIST,
                        cfield->type);

                    BOOST_THROW_EXCEPTION(
                        IronBee::eother()
                            << IronBee::errinfo_what("Expected list."));
                }

                // Extract bs from field.
                IronBee::throw_if_error(
                    ib_field_value(cfield, ib_ftype_list_out(&clist)),
                    "Failed to extract byte string from content type "
                    "field.");

                IronBee::ConstList<ib_field_t *> list(clist);

                const std::string content_type =
                    IronBee::ConstField(list.front()).to_s();

                ib_log_debug_tx(
                    tx.ib(),
                    "Got string %s",
                    content_type.c_str());

                // Is the content type in the set.
                if (m_content_types.count(content_type) > 0)
                {
                    actions.set(m_action);
                }
            }
        }
        else {
            ib_log_debug_tx(
                tx.ib(),
                "Skipping rule as action does not override tx actions.");
        }
    }
    /* End XRuleContentType Impl */

    /**
     * Check that the request path prefix starts with a known string.
     */
    class XRulePath : public XRule {

    public:

        /**
         * Constructor.
         *
         * @param[in] path The path prefix to match.
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
    /* XRulePath Impl */
    XRulePath::XRulePath(const char *path, action_ptr action)
    :
        XRule(action),
        m_path(path)
    {}

    void XRulePath::xrule_impl(
        IronBee::Transaction tx,
        ActionSet&            actions
    )
    {
        if (actions.overrides(m_action)) {
            const std::string tx_path(tx.ib()->path);

            if (tx_path.length() >= m_path.length() &&
                tx_path.compare(0, m_path.length(), m_path) == 0)
            {
                actions.set(m_action);
            }
        }
        else {
            ib_log_debug_tx(
                tx.ib(),
                "Skipping rule as action does not override tx actions.");
        }
    }
    /* End XRulePath Impl */

    /**
     * Check if the start time of a tx falls in (or out of) a time window.
     */
    class XRuleTime : public XRule {

    public:

        /**
         * Constructor.
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

    /* XRuleTime Impl */
    XRuleTime::XRuleTime(
        IronBee::ConfigurationParser cp,
        const char *time,
        action_ptr action
    ) :
        XRule(action),
        m_invert(false)
    {
        assert(time);

        boost::cmatch mr;
        static const boost::regex re(
            "(!?)([\\d,]+@)?"
            "(\\d\\d:\\d\\d)-(\\d\\d:\\d\\d)([+-]\\d\\d\\d\\d)");

        ib_cfg_log_debug(cp.ib(), "Parsing time %s", time);

        if (!boost::regex_match(time, mr, re)) {
            BOOST_THROW_EXCEPTION(
                IronBee::einval()
                    << IronBee::errinfo_what("Cannot parse time.")
            );
        }

        if (mr[1] == "!") {
            m_invert = true;
        }

        /* Parse comma-separated days. */
        if (mr[2].length() > 0) {
            boost::cmatch days_mr;
            static const boost::regex days_re("(\\d+)[,\\@].*");
            const char *c = mr[2].first;
            const int   l = mr[2].length();
            ib_cfg_log_debug(cp.ib(), "Parsing day string \"%.*s\"", l, c);
            while (boost::regex_match(c, days_mr, days_re)) {

                /* Convert the front of the match to a digit. */
                m_days.insert(atoi(days_mr[1].first));

                /* Advance c past it's match and the , or @ character. */
                c += days_mr[1].length() + 1;
            }
        }

        parse_date_time(cp, mr[3].first, m_start_time);
        parse_date_time(cp, mr[4].first, m_end_time);
        parse_time_zone(cp, mr[5].first, m_zone_info);
    }

    void XRuleTime::parse_time_zone(
        IronBee::ConfigurationParser      cp,
        const char*                       str,
        boost::local_time::time_zone_ptr& zone
    )
    {
        using namespace std;
        using namespace boost::local_time;

        string tzstr(str);

        tzstr.insert(
            ((str[0] == '-' || str[0] == '+') ? 3 : 2),
            ":"
        );

        try {
            zone.reset(new posix_time_zone(tzstr));
        }
        catch (const boost::local_time::bad_offset& e) {
            BOOST_THROW_EXCEPTION(
                IronBee::einval()
                    << IronBee::errinfo_level(IB_LOG_ERROR)
                    << IronBee::errinfo_what(
                        " Zone offset out of range. "
                        "Valid values are -1200 <= tz <= +1400.")
            );
        }
        catch (const boost::local_time::bad_adjustment& e) {
            BOOST_THROW_EXCEPTION(
                IronBee::einval()
                    << IronBee::errinfo_what(e.what())
                    << IronBee::errinfo_level(IB_LOG_ERROR)
            );
        }
        catch (const std::out_of_range& e) {
            BOOST_THROW_EXCEPTION(
                IronBee::einval()
                    << IronBee::errinfo_what(e.what())
                    << IronBee::errinfo_level(IB_LOG_ERROR)
            );
        }
    }

    void XRuleTime::parse_date_time(
        IronBee::ConfigurationParser  cp,
        const char                   *str,
        boost::posix_time::ptime&     p
    )
    {
        boost::posix_time::time_input_facet *facet =
            new boost::posix_time::time_input_facet("%H:%M");
        std::string time_str(str, 5);
        std::istringstream is(time_str);
        std::locale loc(is.getloc(), facet);
        is.imbue(loc);
        is.exceptions(std::ios_base::failbit); /* Enable exceptions. */

        ib_cfg_log_debug(cp.ib(), "Parsing time string \"%.*s\"", 5, str);

        try {
            is>>p;
        }
        catch (...) {
            const std::string msg = 
                std::string("Unable to parse time string: ") + str;
            BOOST_THROW_EXCEPTION(
                IronBee::einval()
                    << IronBee::errinfo_what(msg)
                    << IronBee::errinfo_level(IB_LOG_ERROR)
            );
        }
    }

    void XRuleTime::xrule_impl(
        IronBee::Transaction  tx,
        ActionSet&            actions
    )
    {
        if (actions.overrides(m_action)) {

            /* Get tx start time, shifted into the local time zone. */
            boost::posix_time::ptime tx_start =
                tx.started_time() + m_zone_info->base_utc_offset();

            std::ostringstream os;
            std::locale loc(
                os.getloc(), 
                new boost::posix_time::time_facet("%H:%M:%S"));
            os.imbue(loc);
            os << "Checking current time "
               << tx_start
               << " against window "
               << m_start_time
               << "-"
               << m_end_time
               << ".";
            ib_log_debug_tx(tx.ib(), "%s", os.str().c_str());

            bool in_window = (
                m_start_time.time_of_day() <= tx_start.time_of_day() &&
                tx_start.time_of_day()     <  m_end_time.time_of_day()
            );

            // If any days of the week are specified in our window...
            if (m_days.size() > 0) {
                // ...get the day of the week...
                short dow =
                    boost::gregorian::gregorian_calendar::day_of_week(
                        tx_start.date().year_month_day());

                // ...and update the in_window boolean.
                in_window &= (m_days.find(dow) != m_days.end());
            }

            // If we are in the window specified (considering the
            // m_invert member) then execute the associated action.
            if (in_window ^ m_invert) {
                ib_log_debug_tx(tx.ib(), "XRuleTime was matched.");
                actions.set(m_action);
            }
            else {
                ib_log_debug_tx(tx.ib(), "XRuleTime was not matched.");
            }
        }
        else {
            ib_log_debug_tx(
                tx.ib(),
                "Skipping rule as action does not override tx actions.");
        }
    }

    /* End XRuleTime Impl */

    /**
     * Map the client IP address of an IronBee::Transaction to an Action.
     *
     * Unlike a normal XRule that maps a single check to a single action,
     * for efficient evaluation, this XRule is constructed
     * after the IronBee configuration phase and wraps all IPs into
     * a @ref ib_ipset4_t or @ref ib_set6_t and does a single check
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

    /* RuleIP Impl */
    XRuleIP::XRuleIP(XRulesModuleConfig& cfg)
    {
        ib_ipset4_init(
            &m_ipset4,
            NULL,
            0,
            &(cfg.ipv4_list[0]),
            cfg.ipv4_list.size());

        ib_ipset6_init(
            &m_ipset6,
            NULL,
            0,
            &(cfg.ipv6_list[0]),
            cfg.ipv6_list.size());
    }

    void XRuleIP::xrule_impl(
        IronBee::Transaction tx,
        ActionSet&            actions
    )
    {
        const char *remote_ip = tx.effective_remote_ip_string();
        ib_ip4_t    ipv4;
        ib_ip6_t    ipv6;

        ib_log_debug_tx(tx.ib(), "Checking IP Access for %s", remote_ip);

        // Check IP lists.
        if (remote_ip == NULL) {
            BOOST_THROW_EXCEPTION(
                IronBee::einval()
                    << IronBee::errinfo_what("No remote IP available.")
            );
        }
        else if (IB_OK == ib_ip4_str_to_ip(remote_ip, &ipv4)) {
            const ib_ipset4_entry_t *entry;
            ib_status_t rc;
            rc = ib_ipset4_query(&(m_ipset4), ipv4, NULL, &entry, NULL);
            if (rc == IB_OK) {
                ib_log_debug_tx(tx.ib(), "IP matched %s", remote_ip);
                action_ptr action =
                    IronBee::data_to_value<action_ptr>(entry->data);
                actions.set(action);
            }
            else {
                ib_log_debug_tx(
                    tx.ib(),
                    "IP set is empty or does not include %s",
                    remote_ip);
            }
        }
        else if (IB_OK == ib_ip6_str_to_ip(remote_ip, &ipv6)) {
            const ib_ipset6_entry_t *entry;
            ib_status_t rc;
            rc = ib_ipset6_query(&(m_ipset6), ipv6, NULL, &entry, NULL);
            if (rc == IB_OK) {
                ib_log_debug_tx(tx.ib(), "IP matched %s", remote_ip);
                action_ptr action =
                    IronBee::data_to_value<action_ptr>(entry->data);
                actions.set(action);
            }
            else {
                ib_log_debug_tx(
                    tx.ib(),
                    "IP set is empty or does not include %s",
                    remote_ip);
            }
        }
        else {
            BOOST_THROW_EXCEPTION(
                IronBee::enoent()
                    << IronBee::errinfo_what("Cannot convert IP to v4 or v6.")
            );
        }
    }
    /* End RuleIP Impl */
} /* Close anonymous namespace. */


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
     * @param[in] cp Configuration parser.
     * @param[in] list Parameter list.
     * @param[out] action The action the user chose.
     * @throws IronBee::einval if priority or action is missing.
     */
    action_ptr parse_action(
        IronBee::ConfigurationParser     cp,
        IronBee::ConstList<const char *> list
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
     * Run resp_xrules against responses.
     *
     * @param[in] ib IronBee engine.
     * @param[in] tx The current transaction.
     *
     * @throws
     * - IB_OK On success.
     * - Other on error.
     */
    void on_response_header_finished(
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
    void on_handle_context_transaction(
        IronBee::Engine ib,
        IronBee::Transaction tx
    );

    bool is_tx_empty(IronBee::ConstTransaction tx) const;
};

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
        .handle_context_transaction(
            boost::bind(
                &XRulesModule::on_handle_context_transaction,
                this,
                _1,
                _2
            )
        )
        .response_header_finished(
            boost::bind(
                &XRulesModule::on_response_header_finished,
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
                &XRulesModule::xrule_directive, this, _1, _2, _3));

    module.set_configuration_data<XRulesModuleConfig>();
}

void XRulesModule::build_ip_xrule(IronBee::Engine ib, IronBee::Context ctx) {
    XRulesModuleConfig &cfg =
        module().configuration_data<XRulesModuleConfig>(ctx);

    cfg.req_xrules.push_back(xrule_ptr(new XRuleIP(cfg)));
}

action_ptr XRulesModule::parse_action(
    IronBee::ConfigurationParser     cp,
    IronBee::ConstList<const char *> list
)
{
    int  priority = 10;
    const char *action_text = NULL;

    IronBee::ConstList<const char *>::iterator itr = list.begin();

    ++itr; // Skip the argument to the directive.

    action_text = *itr;

    for (++itr; itr != list.end(); ++itr)
    {
        ib_cfg_log_debug(cp.ib(), "Parsing arg %s.", *itr);
        if (boost::istarts_with(*itr, "priority="))
        {
            priority = atoi((*itr) + sizeof("priority=")-1);
        }
    }

    if (action_text == NULL)
    {
        BOOST_THROW_EXCEPTION(
            IronBee::einval()
                << IronBee::errinfo_what("No action text.")
        );
    }

    ib_cfg_log_debug(
        cp.ib(),
        "Building action \"%s\" with priority %d.",
        action_text,
        priority);

    return m_action_factory.build(action_text, priority);
}

void XRulesModule::xrule_directive(
    IronBee::ConfigurationParser     cp,
    const char *                     name,
    IronBee::ConstList<const char *> params
)
{
    std::string        name_str(name);
    IronBee::Context   ctx = cp.current_context();
    XRulesModuleConfig &cfg =
        module().configuration_data<XRulesModuleConfig>(ctx);

    if (name_str == "XRuleIpv4") {
        // Copy in an empty, uninitialized ipset entry.
        ib_ipset4_entry_t entry;
        action_ptr action = parse_action(cp, params);

        IronBee::throw_if_error(
            ib_ip4_str_to_net(params.front(), &(entry.network)),
            "Failed to get net from string.");

        /* Put that action in the ip set. */
        entry.data = IronBee::value_to_data<action_ptr>(
            action,
            cp.engine().main_memory_pool().ib());

        cfg.ipv4_list.push_back(entry);
    }
    else if (name_str =="XRuleIpv6") {
        // Copy in an empty, uninitialized ipset entry.
        ib_ipset6_entry_t entry;
        action_ptr action = parse_action(cp, params);

        IronBee::throw_if_error(
            ib_ip6_str_to_net(params.front(), &(entry.network)),
            "Failed to get net from string.");

        /* Put that action in the ip set. */
        entry.data = IronBee::value_to_data<action_ptr>(
            action,
            cp.engine().main_memory_pool().ib());

        cfg.ipv6_list.push_back(entry);
    }
    else if (name_str =="XRuleGeo") {
        cfg.req_xrules.push_back(
            xrule_ptr(
                new XRuleGeo(params.front(), parse_action(cp, params))));
    }
    else if (name_str =="XRulePath") {
        cfg.req_xrules.push_back(
            xrule_ptr(
                new XRulePath(params.front(), parse_action(cp, params))));
    }
    else if (name_str =="XRuleTime") {
        cfg.req_xrules.push_back(
            xrule_ptr(
                new XRuleTime(cp, params.front(), parse_action(cp, params))));
    }
    else if (name_str =="XRuleRequestContentType") {
        cfg.req_xrules.push_back(
            xrule_ptr(
                new XRuleContentType(
                    params.front(),
                    parse_action(cp, params),
                    "request_headers:Content-Type",
                    "request_headers:Content-Length",
                    "request_headers:Transport-Encoding")));
    }
    else if (name_str =="XRuleResponseContentType") {
        cfg.resp_xrules.push_back(
            xrule_ptr(
                new XRuleContentType(
                    params.front(),
                    parse_action(cp, params),
                    "response_headers:Content-Type",
                    "response_headers:Content-Length",
                    "response_headers:Transport-Encoding")));
    }
    else {
        ib_cfg_log_error(
            cp.ib(),
            "Unknown directive: %s",
            name);
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

void XRulesModule::on_response_header_finished(
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

    for (
        std::list<xrule_ptr>::iterator itr = cfg.resp_xrules.begin();
        itr != cfg.resp_xrules.end();
        ++itr)
    {
        (**itr)(tx, actions);
    }

    actions.apply(mdata, tx);
}

void XRulesModule::on_handle_context_transaction(
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

    for (
        std::list<xrule_ptr>::iterator itr = cfg.req_xrules.begin();
        itr != cfg.req_xrules.end();
        ++itr)
    {
        (**itr)(tx, actions);
    }

    actions.apply(mdata, tx);
}

/* End XRulesModule Impl */

IBPP_BOOTSTRAP_MODULE_DELEGATE("XRulesModule", XRulesModule);
