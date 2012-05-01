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
 * @brief IronBee++ Internals &mdash; Connection Data Tests
 * @internal
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 **/

#include <ironbeepp/connection_data.hpp>
#include <ironbeepp/connection.hpp>
#include <ironbeepp/engine.hpp>
#include <ironbeepp/memory_pool.hpp>

#include "fixture.hpp"

#include "gtest/gtest.h"

using namespace IronBee;

class TestConnectionData : public ::testing::Test, public IBPPTestFixture
{
};

TEST_F(TestConnectionData, basic)
{
    ib_conndata_t ib_conndata;

    ConnectionData conndata(&ib_conndata);

    ASSERT_TRUE(conndata);

    ib_conndata.conn = (ib_conn_t*)1236;
    EXPECT_EQ(ib_conndata.conn, conndata.connection().ib());

    ib_conndata.dlen = 14;
    EXPECT_EQ(ib_conndata.dlen, conndata.length());
    ib_conndata.data = (uint8_t*)15;
    EXPECT_EQ((char*)ib_conndata.data, conndata.data());
}

TEST_F(TestConnectionData, create)
{
    Connection c = Connection::create(Engine(m_ib_engine));
    ConnectionData cd = ConnectionData::create(c, 100);

    ASSERT_TRUE(cd);
    EXPECT_EQ(c, cd.connection());
    EXPECT_TRUE(cd.data());
}
