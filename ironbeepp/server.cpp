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
 * @brief IronBee++ &mdash; Server Implementation
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <ironbeepp/server.hpp>

namespace IronBee {

// ConstServer

ConstServer::ConstServer() :
    m_ib(NULL)
{
    // nop
}

ConstServer::ConstServer(ib_type ib_server) :
    m_ib(ib_server)
{
    // nop
}


uint32_t ConstServer::version_number() const
{
    return ib()->vernum;
}

uint32_t ConstServer::abi_number() const
{
    return ib()->abinum;
}

const char* ConstServer::version() const
{
    return ib()->version;
}

const char* ConstServer::filename() const
{
    return ib()->filename;
}

const char* ConstServer::name() const
{
    return ib()->name;
}

// Server

Server Server::remove_const(ConstServer server)
{
    return Server(const_cast<ib_type>(server.ib()));
}

Server::Server() :
    m_ib(NULL)
{
    // nop
}

Server::Server(ib_type ib_server) :
    ConstServer(ib_server),
    m_ib(ib_server)
{
    // nop
}

std::ostream& operator<<(std::ostream& o, const ConstServer& server)
{
    if (! server) {
        o << "IronBee::Server[!singular!]";
    } else {
        o << "IronBee::Server[" << server.name() << "]";
    }
    return o;
}

//! ServerValue

ServerValue::ServerValue(
    const char* filename,
    const char* name
)
{
    m_value.vernum = IB_VERNUM;
    m_value.abinum = IB_ABINUM;
    m_value.version = IB_VERSION;
    m_value.filename = filename;
    m_value.name = name;
}

Server ServerValue::get()
{
    return Server(&m_value);
}

ConstServer ServerValue::get() const
{
    return ConstServer(&m_value);
}

} // IronBee
