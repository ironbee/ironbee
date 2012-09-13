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
