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
 * @brief IronBee++ Internals &mdash; ParsedRequestLine Tests
 * @internal
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

    ib_prl.method = ByteString::create(mp, "foo").ib();
    EXPECT_EQ(ib_prl.method, prl.method().ib());

    ib_prl.path = ByteString::create(mp, "bar").ib();
    EXPECT_EQ(ib_prl.path, prl.path().ib());

    ib_prl.version = ByteString::create(mp, "baz").ib();
    EXPECT_EQ(ib_prl.version, prl.version().ib());
}
