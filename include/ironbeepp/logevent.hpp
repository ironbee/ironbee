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
 * @brief IronBee++ --- LogEvent
 *
 * This file defines LogEvent wrapper for @ref ib_logevent_t.
 *
 * @author Sam Baskinger <sbaskinger@qualys.com>
 */

#include <ironbeepp/abi_compatibility.hpp>
#include <ironbeepp/engine.hpp>

#include <ironbee/logevent.h>

#include <boost/function.hpp>

#ifndef __IBPP__LOGEVENT__
#define __IBPP__LOGEVENT__

namespace IronBee {


/**
 * LogEvent; equivalent to a pointer to ib_logevent_t.
 *
 * Provides operators ==, !=, <, >, <=, >= and evaluation as a boolean for
 * singularity via CommonSemantics.
 *
 * @sa ib_logevent_t
 * @nosubgrouping
 **/
class LogEvent : public CommonSemantics<LogEvent>
{
public:
    //! C Type.
    typedef ib_logevent_t* ib_type;

    enum type_e {
        TYPE_UNKNOWN     = IB_LEVENT_TYPE_UNKNOWN,
        TYPE_OBSERVATION = IB_LEVENT_TYPE_OBSERVATION,
        TYPE_ALERT       = IB_LEVENT_TYPE_ALERT
    };

    enum action_e {
        ACTION_UNKNOWN = IB_LEVENT_ACTION_UNKNOWN,
        ACTION_LOG = IB_LEVENT_ACTION_LOG,
        ACTION_BLOCK = IB_LEVENT_ACTION_BLOCK,
        ACTION_IGNORE = IB_LEVENT_ACTION_IGNORE,
        ACTION_ALLOW = IB_LEVENT_ACTION_ALLOW
    };

    enum suppress_e {
        SUPPRESS_NONE = IB_LEVENT_SUPPRESS_NONE,
        SUPPRESS_FPOS = IB_LEVENT_SUPPRESS_FPOS,
        SUPPRESS_REPLACED = IB_LEVENT_SUPPRESS_REPLACED,
        SUPPRESS_INC = IB_LEVENT_SUPPRESS_INC,
        SUPPRESS_OTHER = IB_LEVENT_SUPPRESS_OTHER
    };

    /**
     * @name C Interoperability
     * Methods to access underlying C types.
     **/
    ///@{

    //! const ib_engine_t accessor.
    // Intentionally inlined.
    ib_logevent_t* ib() const
    {
        return m_ib;
    }

    LogEvent();
    explicit LogEvent(ib_logevent_t *logevent);

    //! Add a tag.
    void tag_add(const std::string& tag);

    //! Create a log event. See ib_logevent_create().
    static LogEvent create(
        MemoryManager      mm,
        const std::string& rule_id,
        type_e             type,
        action_e           rec_action,
        uint8_t            confidence,
        uint8_t            severity,
        const std::string& msg
    );

    ///@}
private:
    ib_logevent_t* m_ib;

};

} // namespace IronBee


#endif // __IBPP__LOGEVENT__
