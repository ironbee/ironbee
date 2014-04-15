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
 * @brief IronBee --- CLIPP Connection Modifiers Implementation
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include "connection_modifiers.hpp"

#include <boost/foreach.hpp>

namespace IronBee {
namespace CLIPP {

namespace {

class SetLocalIP :
    public Input::ModifierDelegate
{
public:
    SetLocalIP(const std::string& ip) :
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

class SetLocalPort :
    public Input::ModifierDelegate
{
public:
    SetLocalPort(uint32_t port) :
        m_port( port )
    {
        // nop
    }

    void connection_opened(Input::ConnectionEvent& event)
    {
        event.local_port = m_port;
    }

private:
    uint32_t m_port;
};

class SetRemoteIP :
    public Input::ModifierDelegate
{
public:
    SetRemoteIP(const std::string& ip) :
        m_ip(ip)
    {
        // nop
    }

    void connection_opened(Input::ConnectionEvent& event)
    {
        event.remote_ip = Input::Buffer(m_ip);
    }

private:
    const std::string& m_ip;
};

class SetRemotePort :
    public Input::ModifierDelegate
{
public:
    SetRemotePort(uint32_t port) :
        m_port( port )
    {
        // nop
    }

    void connection_opened(Input::ConnectionEvent& event)
    {
        event.remote_port = m_port;
    }

private:
    uint32_t m_port;
};

}

SetLocalIPModifier::SetLocalIPModifier(const std::string& ip) :
    m_ip(ip)
{
    // nop
}

bool SetLocalIPModifier::operator()(Input::input_p& in_out)
{
    if (! in_out) {
        return true;
    }

    SetLocalIP delegate(m_ip);
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

SetLocalPortModifier::SetLocalPortModifier(uint32_t port) :
    m_port(port)
{
    // nop
}

bool SetLocalPortModifier::operator()(Input::input_p& in_out)
{
    if (! in_out) {
        return true;
    }

    SetLocalPort delegate(m_port);
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

SetRemoteIPModifier::SetRemoteIPModifier(const std::string& ip) :
    m_ip(ip)
{
    // nop
}

bool SetRemoteIPModifier::operator()(Input::input_p& in_out)
{
    if (! in_out) {
        return true;
    }

    SetRemoteIP delegate(m_ip);
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

SetRemotePortModifier::SetRemotePortModifier(uint32_t port) :
    m_port(port)
{
    // nop
}

bool SetRemotePortModifier::operator()(Input::input_p& in_out)
{
    if (! in_out) {
        return true;
    }

    SetRemotePort delegate(m_port);
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
