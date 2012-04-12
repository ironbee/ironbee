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
 * @brief IronBee++ Internals -- Test Fixture Implementation
 * @internal
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 **/

#include "fixture.hpp"

#include <stdexcept>

IBPPTestFixture::IBPPTestFixture()
{
    m_ib_server.vernum   = IB_VERNUM;
    m_ib_server.abinum   = IB_ABINUM;
    m_ib_server.version  = IB_VERSION;
    m_ib_server.filename = __FILE__;
    m_ib_server.name     = "IBPPTest";

    ib_initialize();

    ib_status_t rc = ib_engine_create(&m_ib_engine, &m_ib_server);
    if (rc != IB_OK) {
        throw std::runtime_error("ib_engine_create failed.");
    }
    rc = ib_engine_init(m_ib_engine);
    if (rc != IB_OK) {
        throw std::runtime_error("ib_engine_init failed.");
    }
}

IBPPTestFixture::~IBPPTestFixture()
{
    ib_shutdown();
}

IBPPTXTestFixture::IBPPTXTestFixture()
{
    ib_conn_create(m_ib_engine, &m_ib_connection, NULL);
    m_ib_connection->local_ipstr = "1.0.0.1";
    m_ib_connection->remote_ipstr = "1.0.0.2";
    m_ib_connection->remote_port = 65534;
    m_ib_connection->local_port = 80;

    ib_tx_create(m_ib_engine, &m_ib_transaction, m_ib_connection, NULL);
}

IBPPTXTestFixture::~IBPPTXTestFixture()
{
}
