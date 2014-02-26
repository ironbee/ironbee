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
 * @brief IronBee++ Internals --- Connection Tests
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 **/

#include <ironbeepp/connection.hpp>
#include <ironbeepp/engine.hpp>
#include <ironbeepp/transaction.hpp>
#include <ironbeepp/memory_pool.hpp>
#include <ironbeepp/context.hpp>
#include <ironbeepp/clock.hpp>

#include <ironbeepp/test_fixture.hpp>

#include "gtest/gtest.h"

using namespace std;

class TestConnection : public ::testing::Test, public IronBee::TestFixture
{
};

TEST_F(TestConnection, basic)
{
    ib_conn_t ib_conn;

    IronBee::Connection conn(&ib_conn);

    ASSERT_TRUE(conn);

    ib_conn.ib = (ib_engine_t*)1234;
    EXPECT_EQ(ib_conn.ib, conn.engine().ib());

    ib_conn.ctx = (ib_context_t*)1236;
    EXPECT_EQ(ib_conn.ctx, conn.context().ib());

    ib_conn.tv_created.tv_sec = 0;
    ib_conn.tv_created.tv_usec = 0;

    ib_conn.t.started = 0;
    EXPECT_EQ(ib_conn.t.started, IronBee::ptime_to_ib(conn.started_time()));

    ib_conn.t.finished = 18;
    EXPECT_EQ(ib_conn.t.finished, IronBee::ptime_to_ib(conn.finished_time()));

    ib_conn.remote_ipstr = "foo";
    EXPECT_EQ(ib_conn.remote_ipstr, conn.remote_ip_string());

    ib_conn.remote_port = 19;
    EXPECT_EQ(ib_conn.remote_port, conn.remote_port());

    ib_conn.local_ipstr = "bar";
    EXPECT_EQ(ib_conn.local_ipstr, conn.local_ip_string());

    ib_conn.local_port = 20;
    EXPECT_EQ(ib_conn.local_port, conn.local_port());

    ib_conn.tx_count = 21;
    EXPECT_EQ(ib_conn.tx_count, conn.transaction_count());

    ib_tx_t tx1;
    ib_conn.tx_first = &tx1;
    EXPECT_EQ(ib_conn.tx_first, conn.first_transaction().ib());

    ib_tx_t tx2;
    ib_conn.tx_last = &tx2;
    EXPECT_EQ(ib_conn.tx_last, conn.last_transaction().ib());

    ib_tx_t tx3;
    ib_conn.tx = &tx3;
    EXPECT_EQ(ib_conn.tx, conn.transaction().ib());

    ib_conn.flags = 0;
    EXPECT_EQ(ib_conn.flags, conn.flags());
    ib_conn.flags = IB_CONN_FTX | IB_CONN_FCLOSED;
    EXPECT_EQ(ib_conn.flags, conn.flags());
    EXPECT_FALSE(conn.is_none());
    EXPECT_FALSE(conn.is_error());
    EXPECT_TRUE(conn.is_transaction());
    EXPECT_FALSE(conn.is_data_in());
    EXPECT_FALSE(conn.is_data_out());
    EXPECT_FALSE(conn.is_opened());
    EXPECT_TRUE(conn.is_closed());
}

TEST_F(TestConnection, create)
{
    IronBee::Connection conn = IronBee::Connection::create(
        m_engine
    );

    ASSERT_TRUE(conn);
    ASSERT_EQ(m_engine.ib(), conn.engine().ib());

    conn.destroy();
}

TEST_F(TestConnection, set)
{
    ib_conn_t ib_conn;

    IronBee::Connection conn(&ib_conn);

    conn.set_remote_ip_string("foo");
    EXPECT_EQ(string("foo"), ib_conn.remote_ipstr);
    conn.set_remote_port(12);
    EXPECT_EQ(12, ib_conn.remote_port);
    conn.set_local_ip_string("bar");
    EXPECT_EQ(string("bar"), ib_conn.local_ipstr);
    conn.set_local_port(13);
    EXPECT_EQ(13, ib_conn.local_port);
}
