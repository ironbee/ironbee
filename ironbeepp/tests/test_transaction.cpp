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
 * @brief IronBee++ Internals &mdash; Transaction Tests
 * @internal
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 **/

#include <ironbeepp/transaction.hpp>
#include <ironbeepp/connection.hpp>
#include <ironbeepp/engine.hpp>
#include <ironbeepp/memory_pool.hpp>
#include <ironbeepp/context.hpp>
#include <ironbeepp/clock.hpp>

#include "gtest/gtest.h"

using namespace IronBee;

TEST(TestTransaction, basic)
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

    ib_tx.t.started = 17;
    EXPECT_EQ(ib_tx.t.started, ptime_to_ib(tx.started_time()));

    ib_tx.t.finished = 18;
    EXPECT_EQ(ib_tx.t.finished, ptime_to_ib(tx.finished_time()));

    ib_tx.t.started = 1;
    EXPECT_EQ(ib_tx.t.started, ptime_to_ib(tx.started_time()));

    ib_tx.t.request_started = 2;
    EXPECT_EQ(ib_tx.t.request_started, ptime_to_ib(tx.request_started_time()));

    ib_tx.t.request_headers = 3;
    EXPECT_EQ(ib_tx.t.request_headers, ptime_to_ib(tx.request_headers_time()));

    ib_tx.t.request_body = 4;
    EXPECT_EQ(ib_tx.t.request_body, ptime_to_ib(tx.request_body_time()));

    ib_tx.t.request_finished = 5;
    EXPECT_EQ(ib_tx.t.request_finished, ptime_to_ib(tx.request_finished_time()));

    ib_tx.t.response_started = 6;
    EXPECT_EQ(ib_tx.t.response_started, ptime_to_ib(tx.response_started_time()));

    ib_tx.t.response_headers = 7;
    EXPECT_EQ(ib_tx.t.response_headers, ptime_to_ib(tx.response_headers_time()));

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

    ib_tx.flags = 0;
    EXPECT_EQ(ib_tx.flags, tx.flags());
    ib_tx.flags = IB_TX_FPIPELINED | IB_TX_FSUSPICIOUS;
    EXPECT_EQ(ib_tx.flags, tx.flags());

    EXPECT_FALSE(tx.is_none());
    EXPECT_FALSE(tx.is_error());
    EXPECT_TRUE(tx.is_pipelined());
    EXPECT_FALSE(tx.is_seen_data_in());
    EXPECT_FALSE(tx.is_seen_data_out());
    EXPECT_FALSE(tx.is_request_started());
    EXPECT_FALSE(tx.is_request_seen_headers());
    EXPECT_FALSE(tx.is_request_no_body());
    EXPECT_FALSE(tx.is_request_seen_body());
    EXPECT_FALSE(tx.is_request_finished());
    EXPECT_FALSE(tx.is_response_started());
    EXPECT_FALSE(tx.is_response_seen_headers());
    EXPECT_FALSE(tx.is_response_seen_body());
    EXPECT_FALSE(tx.is_response_finished());
    EXPECT_TRUE(tx.is_suspicious());
}
