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
 * @brief IronBee &mdash; CLIPP Connection Modifiers Implementation
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include "connection_modifiers.hpp"

#include <boost/foreach.hpp>

namespace IronBee {
namespace CLIPP {

namespace  {

class SetLocalIp :
    public Input::ModifierDelegate
{
public:
    SetLocalIp(const std::string& ip) :
        m_ip(ip)
    {
        // nop
    }

    void connection_opened(Input::ConnectionEvent& event)
    {
        event.local_ip = Input::Buffer(m_ip);
    }

private:
    const std::string& m_ip;
};

}

SetLocalIPModifier::SetLocalIPModifier(const std::string& ip) :
    m_ip(ip)
{
    // nop
}

bool SetLocalIPModifier::operator()(Input::input_p& in_out)
{
    SetLocalIp delegate(m_ip);
    // ConnectionOpened events only occur in pre-transaction
    BOOST_FOREACH(
        const Input::event_p& event,
        in_out->connection.pre_transaction_events
    )
    {
        event->dispatch(delegate);
    }

    return true;
}

} // CLI
} // IronBee
