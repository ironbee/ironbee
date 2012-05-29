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
 * @brief IronBee++ Internals &mdash; Test Fixture Implementation
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 **/

#include "fixture.hpp"

#include <ironbeepp/ironbee.hpp>

#include <stdexcept>

using namespace IronBee;

IBPPTestFixture::IBPPTestFixture() :
    m_server_value("filename", "name")
{
    IronBee::initialize();

    m_server = m_server_value.get();

    m_engine = Engine::create(m_server);
    m_engine.initialize();

    m_connection = Connection::create(m_engine);

    m_connection.set_local_ip_string("1.0.0.1");
    m_connection.set_remote_ip_string("1.0.0.2");
    m_connection.set_remote_port(65534);
    m_connection.set_local_port(80);

    m_transaction = Transaction::create(m_connection);
}

IBPPTestFixture::~IBPPTestFixture()
{
    m_engine.destroy();
    IronBee::shutdown();
}

