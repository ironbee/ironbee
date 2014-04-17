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
 * @brief IronBee Modules --- XRule ACL Implementations
 *
 * @author Sam Baskinger <sbaskinger@qualys.com>
 */

#include "xrules_acls.hpp"

#include <ironbee/string.h>
#include <ironbee/logevent.h>

#include <ironbeepp/configuration_parser.hpp>

#include <boost/date_time/local_time/local_time.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/time_zone_base.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/foreach.hpp>


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
        IronBee::throw_if_error(
            ib_tx_flags_set(
                tx.ib(),
                IB_TX_FBLOCK_IMMEDIATE | IB_TX_FBLOCK_ADVISORY
            )
        );
        IronBee::throw_if_error(
            ib_tx_flags_unset(
                tx.ib(),
                IB_TX_FALLOW_ALL
            )
        );
    }
    else {
        ib_log_debug_tx(tx.ib(), "Allowing Transaction");
        tx.ib()->flags |= IB_TX_FALLOW_ALL;
        IronBee::throw_if_error(
            ib_tx_flags_unset(
                tx.ib(),
                IB_TX_FBLOCK_IMMEDIATE | IB_TX_FBLOCK_PHASE | IB_TX_FBLOCK_ADVISORY
            )
        );
        IronBee::throw_if_error(
            ib_tx_flags_set(
                tx.ib(),
                IB_TX_FALLOW_ALL
            )
        );
    }
}
/* End BlockAllow Impl */

/* SetFlag Impl */
SetFlag::SetFlag(
    const std::string& field_name,
    ib_flags_t flag,
    int priority
)
:
    Action("SetFlag_" + field_name, priority),
    m_field_name(field_name),
    m_flag(flag)
{}

UnsetFlag::UnsetFlag(
        const std::string& field_name,
        ib_flags_t         flag,
        int                priority)
:
    SetFlag(field_name, flag, priority)
{}

void UnsetFlag::apply_impl(
    xrules_module_tx_data_ptr mdata,
    IronBee::Transaction tx
) const
{
    IronBee::throw_if_error(ib_tx_flags_unset(tx.ib(), m_flag));
}

void SetFlag::apply_impl(
    xrules_module_tx_data_ptr mdata,
    IronBee::Transaction tx
) const
{
    IronBee::throw_if_error(ib_tx_flags_set(tx.ib(), m_flag));
}
/* End SetFlag Impl */

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

/* SetBlockingMode Impl */
SetBlockingMode::SetBlockingMode(bool enabled, int priority)
:
    SetFlag(
        "FLAGS:blockingMode",
        IB_TX_FBLOCKING_MODE,
        priority)
{}

/* UnsetBlockingMode Impl */
UnsetBlockingMode::UnsetBlockingMode(bool enabled, int priority)
:
    UnsetFlag(
        "FLAGS:blockingMode",
        IB_TX_FBLOCKING_MODE,
        priority)
{}

/* End SetBlockingMode Impl */

/* XRuleGeo Impl */
XRuleGeo::XRuleGeo(const char *country, action_ptr action)
:
    XRule(action),
    m_country(country)
{}

void XRuleGeo::xrule_impl(IronBee::Transaction tx, ActionSet& actions)
{
    if (actions.overrides(m_action)) {
        ib_var_target_t *target;
        const ib_list_t *clist;

        ib_log_debug_tx(
            tx.ib(),
            "Running GeoIP check for %s",
            m_country.c_str());

        IronBee::throw_if_error(
            ib_var_target_acquire_from_string(
                &target,
                tx.memory_manager().ib(),
                ib_engine_var_config_get(tx.engine().ib()),
                IB_S2SL(GEOIP_FIELD),
                NULL,
                NULL
            ),
            "Failed to acquire GeoIP source."
        );

        IronBee::throw_if_error(
            ib_var_target_get_const(
                target,
                &clist,
                tx.memory_manager().ib(),
                tx.ib()->var_store
            ),
            "Failed to retrieve GeoIP field."
        );

        IronBee::ConstList<const ib_field_t *> ls(clist);

        if (ls.size() < 1) {
            ib_log_info_tx(
                tx.ib(),
                "No GeoIP fields. Not filtering on GeoIP.");
        }
        else {
            try {
                IronBee::ConstByteString bs(
                    IronBee::ConstField(ls.front()).
                        value_as_byte_string());

                ib_log_debug_tx(
                    tx.ib(),
                    "Matching GeoIP input %.*s against country %.*s.",
                    static_cast<int>(bs.length()),
                    bs.const_data(),
                    static_cast<int>(m_country.length()),
                    m_country.data());
                if (boost::iequals(bs.to_s(), m_country)) {
                    ib_log_debug_tx(tx.ib(), "GeoIP match.");
                    actions.set(m_action);
                }
                else {
                    ib_log_debug_tx(tx.ib(), "No GeoIP match.");
                }
            }
            catch (const IronBee::einval& e) {
                ib_log_error_tx(
                    tx.ib(),
                    "GeoIP field is not a byte string field. "
                    "This XRule cannot run."
                );
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
    ib_var_target_t *target;
    const ib_list_t *clist;

    IronBee::throw_if_error(
        ib_var_target_acquire_from_string(
            &target,
            tx.memory_manager().ib(),
            ib_engine_var_config_get(tx.engine().ib()),
            field.data(),
            field.length(),
            NULL,
            NULL)
    );

    IronBee::throw_if_error(
        ib_var_target_get_const(
            target,
            &clist,
            tx.memory_manager().ib(),
            tx.ib()->var_store)
    );

    return (ib_list_elements(clist) >= 1U);
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
        }
        else {
            const ib_list_t          *clist = NULL;
            ib_var_target_t *target;

            // Fetch list of fields.
            IronBee::throw_if_error(
                ib_var_target_acquire_from_string(
                    &target,
                    tx.memory_manager().ib(),
                    ib_engine_var_config_get(tx.engine().ib()),
                    m_content_type_field.data(),
                    m_content_type_field.length(),
                    NULL, NULL
                ),
                "Failed to acquire content type target.");

            IronBee::throw_if_error(
                ib_var_target_get(
                    target,
                    &clist,
                    tx.memory_manager().ib(),
                    tx.ib()->var_store
                ),
                "Failed to retrieve content type field.");

            IronBee::ConstList<ib_field_t *> list(clist);

            if (list.size() > 0) {
                const std::string content_type =
                    IronBee::ConstField(list.front()).to_s();

                ib_log_debug_tx(
                    tx.ib(),
                    "Checking content type value \"%s\".",
                    content_type.c_str());

                // Is the content type in the set.
                if (m_content_types.count(content_type) > 0)
                {
                    ib_log_debug_tx(
                        tx.ib(),
                        "Content type matched.");
                    actions.set(m_action);
                }
            }
            else {
                ib_log_debug_tx(
                    tx.ib(),
                    "No Content-Type header values. Rule not evaluated.");
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

/* RuleIP Impl */
XRuleIP::XRuleIP(XRulesModuleConfig& cfg)
{
    IronBee::throw_if_error(
        ib_ipset4_init(
            &m_ipset4,
            NULL,
            0,
            &(cfg.ipv4_list[0]),
            cfg.ipv4_list.size()),
        "Failed to initialize IPv4 set."
    );

    IronBee::throw_if_error(
        ib_ipset6_init(
            &m_ipset6,
            NULL,
            0,
            &(cfg.ipv6_list[0]),
            cfg.ipv6_list.size()),
        "Failed to initialize IPv6 set."
    );
}

const char* XRuleIP::normalize_ipv6(IronBee::MemoryManager mm, const char *str)
{
    /* Check if network_param ends w/ pattern '/d+'. If not, add /32. */
    if (!boost::regex_match(str, boost::regex(".*\\/\\d+$"))) {
        str = ib_mm_strdup(
                mm.ib(),
                (std::string(str) + "/128").c_str());
        if (!str) {
            BOOST_THROW_EXCEPTION(IronBee::ealloc());
        }
    }

    return str;
}

const char* XRuleIP::normalize_ipv4(IronBee::MemoryManager mm, const char *str)
{
    /* Check if network_param ends with pattern '/d+'. If not, add /32. */
    if (!boost::regex_match(str, boost::regex(".*\\/\\d+$"))) {
        str = ib_mm_strdup(
                mm.ib(),
                (std::string(str) + "/32").c_str());
        if (!str) {
            BOOST_THROW_EXCEPTION(IronBee::ealloc());
        }
    }

    return str;
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

/* XRuleEventTag Impl */

XRuleEventTag::XRuleEventTag(
    IronBee::ConstList<const char *> tags,
    action_ptr                       action
)
:
    XRule(action)
{
    BOOST_FOREACH(const char *tag, tags) {
        m_tags.push_back(tag);
    }
}

void XRuleEventTag::xrule_impl(
    IronBee::Transaction tx,
    ActionSet&           actions
)
{
    if (actions.overrides(m_action))
    {
        IronBee::ConstList<const ib_logevent_t*> logevents(tx.ib()->logevents);

        BOOST_FOREACH(const ib_logevent_t* logevent, logevents) {
            /* Do not consider suppressed events. */
            if (logevent->suppress == IB_LEVENT_SUPPRESS_NONE) {

                IronBee::ConstList<const char *> event_tags(logevent->tags);

                /* ... every tag in the events. */
                BOOST_FOREACH(const char *event_tag, event_tags)
                {
                    /* Every tag in our class. */
                    BOOST_FOREACH(const std::string& tag, m_tags)
                    {
                        ib_log_debug_tx(
                            tx.ib(),
                            "Comparing event tag %s to tag %.*s.",
                            event_tag,
                            static_cast<int>(tag.length()),
                            tag.data());
                        if (strncmp(tag.data(), event_tag, tag.length()) == 0)
                        {
                            actions.set(m_action);
                            return;
                        }
                    }
                }
            }
        }
    }
}
/* End XRuleEventTag Impl */
