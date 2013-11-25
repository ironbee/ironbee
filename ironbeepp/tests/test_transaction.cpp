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
 * @brief IronBee++ Internals --- Transaction Tests
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 **/

#include <ironbeepp/transaction.hpp>
#include <ironbeepp/connection.hpp>
#include <ironbeepp/engine.hpp>
#include <ironbeepp/memory_pool.hpp>
#include <ironbeepp/context.hpp>
#include <ironbeepp/clock.hpp>
#include <ironbeepp/parsed_request_line.hpp>
#include <ironbeepp/parsed_name_value.hpp>

// For ugly workaround.
#include "engine/engine_private.h"

#include <ironbeepp/test_fixture.hpp>
#include "gtest/gtest.h"

using namespace IronBee;

class TestTransaction : public ::testing::Test, public TestFixture
{
};

TEST_F(TestTransaction, basic)
{
    ib_tx_t ib_tx;

    Transaction tx(&ib_tx);

    ASSERT_TRUE(tx);

    ib_tx.ib = (ib_engine_t*)1234;
    EXPECT_EQ(ib_tx.ib, tx.engine().ib());

    ib_tx.mp = (ib_mpool_t*)1235;
    EXPECT_EQ(ib_tx.mp, tx.memory_pool().ib());

    ib_tx.conn = (ib_conn_t*)1236;
    EXPECT_EQ(ib_tx.conn, tx.connection().ib());

    ib_tx.ctx = (ib_context_t*)1237;
    EXPECT_EQ(ib_tx.ctx, tx.context().ib());

    ib_tx.tv_created.tv_sec = 0;
    ib_tx.tv_created.tv_usec = 0;

    ib_tx.t.started = 0;
    EXPECT_EQ(ib_tx.t.started, ptime_to_ib(tx.started_time()));

    ib_tx.t.finished = 18;
    EXPECT_EQ(ib_tx.t.finished, ptime_to_ib(tx.finished_time()));

    ib_tx.t.request_started = 2;
    EXPECT_EQ(ib_tx.t.request_started, ptime_to_ib(tx.request_started_time()));

    ib_tx.t.request_header = 3;
    EXPECT_EQ(ib_tx.t.request_header, ptime_to_ib(tx.request_header_time()));

    ib_tx.t.request_body = 4;
    EXPECT_EQ(ib_tx.t.request_body, ptime_to_ib(tx.request_body_time()));

    ib_tx.t.request_finished = 5;
    EXPECT_EQ(ib_tx.t.request_finished, ptime_to_ib(tx.request_finished_time()));

    ib_tx.t.response_started = 6;
    EXPECT_EQ(ib_tx.t.response_started, ptime_to_ib(tx.response_started_time()));

    ib_tx.t.response_header = 7;
    EXPECT_EQ(ib_tx.t.response_header, ptime_to_ib(tx.response_header_time()));

    ib_tx.t.response_body = 8;
    EXPECT_EQ(ib_tx.t.response_body, ptime_to_ib(tx.response_body_time()));

    ib_tx.t.response_finished = 9;
    EXPECT_EQ(ib_tx.t.response_finished, ptime_to_ib(tx.response_finished_time()));

    ib_tx.t.postprocess = 10;
    EXPECT_EQ(ib_tx.t.postprocess, ptime_to_ib(tx.postprocess_time()));

    ib_tx.t.logtime = 11;
    EXPECT_EQ(ib_tx.t.logtime, ptime_to_ib(tx.logtime_time()));

    ib_tx.t.finished = 12;
    EXPECT_EQ(ib_tx.t.finished, ptime_to_ib(tx.finished_time()));

    ib_tx.hostname = "foo";
    EXPECT_EQ(ib_tx.hostname, tx.hostname());

    ib_tx.er_ipstr = "bar";
    EXPECT_EQ(ib_tx.er_ipstr, tx.effective_remote_ip_string());

    ib_tx.path = "baz";
    EXPECT_EQ(ib_tx.path, tx.path());

    ib_tx.request_line = (ib_parsed_req_line_t*)1238;
    EXPECT_EQ(ib_tx.request_line, tx.request_line().ib());

    ib_parsed_headers_t plw;
    plw.head = (ib_parsed_header_t*)1239;
    ib_tx.request_header = &plw;
    EXPECT_EQ(ib_tx.request_header->head, tx.request_header().ib());

    ib_tx.flags = 0;
    EXPECT_EQ(ib_tx.flags, tx.flags());
    ib_tx.flags = IB_TX_FPIPELINED | IB_TX_FSUSPICIOUS;
    EXPECT_EQ(ib_tx.flags, tx.flags());

    EXPECT_FALSE(tx.is_none());
    EXPECT_FALSE(tx.is_error());
    EXPECT_TRUE(tx.is_pipelined());
    EXPECT_FALSE(tx.is_request_started());
    EXPECT_FALSE(tx.is_request_seen_header());
    EXPECT_FALSE(tx.is_request_no_body());
    EXPECT_FALSE(tx.is_request_seen_body());
    EXPECT_FALSE(tx.is_request_finished());
    EXPECT_FALSE(tx.is_response_started());
    EXPECT_FALSE(tx.is_response_seen_header());
    EXPECT_FALSE(tx.is_response_seen_body());
    EXPECT_FALSE(tx.is_response_finished());
    EXPECT_TRUE(tx.is_suspicious());
}

TEST_F(TestTransaction, create)
{
    Connection c = Connection::create(m_engine);
    Transaction tx = Transaction::create(c);

    ASSERT_TRUE(tx);
    EXPECT_EQ(c, tx.connection());

    // State transition logic is currently stored in transaction destruction
    // logic (!).  This is considered a known bug.  The following line is a
    // work around.
    m_engine.ib()->cfg_state = CFG_FINISHED;
    tx.destroy();
}
