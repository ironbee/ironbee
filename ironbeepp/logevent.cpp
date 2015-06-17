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
 * @brief IronBee++ Internals -- LogEvent Implementation
 *
 * @author Sam Baskinger <sbaskinger@qualys.com>
 **/

#include <ironbeepp/logevent.hpp>

#ifdef __clang__
#pragma clang diagnostic push
#if __has_warning("-Wunused-local-typedef")
#pragma clang diagnostic ignored "-Wunused-local-typedef"
#endif
#endif
#include <boost/algorithm/string/predicate.hpp>
#ifdef __clang__
#pragma clang diagnostic pop
#endif

namespace IronBee {

LogEvent::LogEvent() : m_ib(NULL) {}
LogEvent::LogEvent(ib_logevent_t *logevent) : m_ib(logevent) {}

LogEvent LogEvent::create(
        MemoryManager      mm,
        const std::string& rule_id,
        type_e             type,
        action_e           rec_action,
        uint8_t            confidence,
        uint8_t            severity,
        const std::string& msg
    )
{
    ib_logevent_t *event;

    throw_if_error(
        ::ib_logevent_create(
            &event,
            mm.ib(),
            rule_id.c_str(),
            static_cast<ib_logevent_type_t>(type),
            static_cast<ib_logevent_action_t>(rec_action),
            confidence,
            severity,
            "%s",
            msg.c_str()
        )
    );

    return LogEvent(event);
}

void LogEvent::tag_add(const std::string& tag)
{
    throw_if_error(
        ::ib_logevent_tag_add(ib(), tag.c_str())
    );
}

LogEvent::type_e LogEvent::type_from_string(const std::string& val)
{
    if (boost::algorithm::iequals(val, "OBSERVATION")) {
        return LogEvent::TYPE_OBSERVATION;
    }

    if (boost::algorithm::iequals(val, "ALERT")) {
        return LogEvent::TYPE_ALERT;
    }

    return LogEvent::TYPE_UNKNOWN;
}

LogEvent::action_e LogEvent::action_from_string(const std::string& val)
{
    if (boost::algorithm::iequals(val, "LOG")) {
        return LogEvent::ACTION_LOG;
    }

    if (boost::algorithm::iequals(val, "BLOCK")) {
        return LogEvent::ACTION_BLOCK;
    }

    if (boost::algorithm::iequals(val, "IGNORE")) {
        return LogEvent::ACTION_IGNORE;
    }

    if (boost::algorithm::iequals(val, "ALLOW")) {
        return LogEvent::ACTION_ALLOW;
    }

    return LogEvent::ACTION_UNKNOWN;

}

LogEvent::suppress_e LogEvent::suppress_from_string(const std::string& val)
{

    if (boost::algorithm::iequals(val, "NONE")) {
        return LogEvent::SUPPRESS_NONE;
    }

    if (boost::algorithm::iequals(val, "FPOS")) {
        return LogEvent::SUPPRESS_FPOS;
    }

    if (boost::algorithm::iequals(val, "REPLACED")) {
        return LogEvent::SUPPRESS_REPLACED;
    }

    if (boost::algorithm::iequals(val, "INC")) {
        return LogEvent::SUPPRESS_INC;
    }

    return LogEvent::SUPPRESS_OTHER;
}


}