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
 * @brief IronBee++ Internals --- ParsedRequestLine Tests
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 **/

#include <ironbeepp/parsed_request_line.hpp>
#include <ironbeepp/transaction.hpp>
#include <ironbeepp/byte_string.hpp>
#include <ironbeepp/memory_pool.hpp>

#include "gtest/gtest.h"

using namespace IronBee;

TEST(TestParsedRequestLine, basic)
{
    MemoryPool mp = MemoryPool::create();

    ib_parsed_req_line_t ib_prl;

    ParsedRequestLine prl(&ib_prl);

    ASSERT_TRUE(prl);

    ib_prl.raw = ByteString::create(mp, "raw").ib();
    EXPECT_EQ(ib_prl.raw, prl.raw().ib());

    ib_prl.method = ByteString::create(mp, "foo").ib();
    EXPECT_EQ(ib_prl.method, prl.method().ib());

    ib_prl.uri = ByteString::create(mp, "bar").ib();
    EXPECT_EQ(ib_prl.uri, prl.uri().ib());

    ib_prl.protocol = ByteString::create(mp, "baz").ib();
    EXPECT_EQ(ib_prl.protocol, prl.protocol().ib());
}

TEST(TestParsedRequestLine, create)
{
    MemoryPool mp = MemoryPool::create();

    const char* raw      = "raw";
    const char* method   = "foo";
    const char* uri      = "bar";
    const char* protocol = "baz";

    ConstParsedRequestLine prl = ParsedRequestLine::create_alias(
        mp,
        raw,      3,
        method,   3,
        uri,      3,
        protocol, 3
    );

    ASSERT_TRUE(prl);

    EXPECT_EQ(raw,      prl.raw().to_s());
    EXPECT_EQ(method,   prl.method().to_s());
    EXPECT_EQ(uri,      prl.uri().to_s());
    EXPECT_EQ(protocol, prl.protocol().to_s());
}
