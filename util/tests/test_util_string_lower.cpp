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
 * @brief Predicate --- String Lower Tests
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 **/

#include "ironbee_config_auto.h"

#include <ironbee/string_lower.h>
#include <ironbeepp/memory_manager.hpp>
#include <ironbeepp/memory_pool_lite.hpp>

#include "gtest/gtest.h"

using namespace std;
using namespace IronBee;

namespace {

string strlower(const string& s)
{
    ScopedMemoryPoolLite mpl;

    char *out = NULL;
    ib_status_t rc;

    rc = ib_strlower(
        MemoryManager(mpl).ib(),
        reinterpret_cast<const uint8_t*>(s.data()), s.length(),
        reinterpret_cast<uint8_t**>(&out)
    );
    if (rc != IB_OK) {
        throw runtime_error("ib_strlower() did not return IB_OK");
    }

    return string(out, s.length());
}

}

TEST(TestStringLower, strlower)
{
    EXPECT_EQ("abc", strlower("abc"));
    EXPECT_EQ("abc", strlower("aBc"));
    EXPECT_EQ("abc", strlower("ABC"));
    EXPECT_EQ("", strlower(""));
}
