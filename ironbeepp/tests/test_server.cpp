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
 * @brief IronBee++ Internals --- Server Tests
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 **/

#include <ironbeepp/server.hpp>

#include <boost/bind.hpp>

#include "gtest/gtest.h"

using namespace IronBee;
using namespace std;

TEST(TestServer, basic)
{
    const char* filename = "abc";
    const char* name = "def";
    ServerValue sv(filename, name);
    const ServerValue& csv = sv;

    Server s = sv.get();
    ConstServer cs = csv.get();

    ASSERT_TRUE(s);
    ASSERT_TRUE(cs);

    EXPECT_EQ(cs, s);
    EXPECT_EQ(IB_VERNUM, cs.version_number());
    EXPECT_EQ(static_cast<uint32_t>(IB_ABINUM), cs.abi_number());
    EXPECT_EQ(string(IB_VERSION), cs.version());
    EXPECT_EQ(filename, cs.filename());
    EXPECT_EQ(name, cs.name());
}

namespace {

void error_callback(int& called, Transaction, int)
{
    called = 1;
}

void error_header_callback(int& called, Transaction, const char*, size_t, const char*, size_t)
{
    called = 2;
}

void error_data_callback(int& called, Transaction, const char*, size_t)
{
    called = 3;
}

void header_callback(
    int& called,
    Transaction,
    Server::direction_e,
    Server::header_action_e,
    const char*, size_t,
    const char*, size_t,
    ib_rx_t*
)
{
    called = 4;
}

void close_callback(int& called, Connection, Transaction)
{
    called = 5;
}

}

TEST(TestServer, callbacks)
{
    ServerValue sv("abc", "def");
    Server s = sv.get();
    int callback_called;

    ASSERT_TRUE(s);

    callback_called = 0;
    s.set_error_callback(boost::bind(
        error_callback, boost::ref(callback_called), _1, _2
    ));

    ASSERT_TRUE(s.ib()->err_fn);
    ASSERT_TRUE(s.ib()->err_data);

    s.ib()->err_fn(NULL, 0, s.ib()->err_data);
    ASSERT_EQ(1, callback_called);

    callback_called = 0;
    s.set_error_header_callback(boost::bind(
        error_header_callback, boost::ref(callback_called), _1, _2, _3, _4, _5
    ));

    ASSERT_TRUE(s.ib()->err_hdr_fn);
    ASSERT_TRUE(s.ib()->err_hdr_data);

    s.ib()->err_hdr_fn(NULL, NULL, 0, NULL, 0, s.ib()->err_hdr_data);
    ASSERT_EQ(2, callback_called);

    callback_called = 0;
    s.set_error_data_callback(boost::bind(
        error_data_callback, boost::ref(callback_called), _1, _2, _3
    ));

    ASSERT_TRUE(s.ib()->err_body_fn);
    ASSERT_TRUE(s.ib()->err_body_data);

    s.ib()->err_body_fn(NULL, NULL, 0, s.ib()->err_body_data);
    ASSERT_EQ(3, callback_called);

    callback_called = 0;
    s.set_header_callback(boost::bind(
        header_callback, boost::ref(callback_called), _1, _2, _3, _4, _5, _6, _7, _8
    ));

    ASSERT_TRUE(s.ib()->hdr_fn);
    ASSERT_TRUE(s.ib()->hdr_data);

    s.ib()->hdr_fn(NULL, IB_SERVER_REQUEST, IB_HDR_SET, NULL, 0, NULL, 0, NULL, s.ib()->hdr_data);
    ASSERT_EQ(4, callback_called);

    callback_called = 0;
    s.set_close_callback(boost::bind(
        close_callback, boost::ref(callback_called), _1, _2
    ));

    ASSERT_TRUE(s.ib()->close_fn);
    ASSERT_TRUE(s.ib()->close_data);

    s.ib()->close_fn(NULL, NULL, s.ib()->close_data);
    ASSERT_EQ(5, callback_called);
}
