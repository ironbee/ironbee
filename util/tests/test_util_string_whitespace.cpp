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
 * @brief Predicate --- String Whitespace Tests
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 **/

#include "ironbee_config_auto.h"

#include <ironbee/string_whitespace.h>
#include <ironbeepp/memory_manager.hpp>
#include <ironbeepp/memory_pool_lite.hpp>

#include "gtest/gtest.h"

using namespace std;
using namespace IronBee;

namespace {

string strws(
    boost::function<
        ib_status_t(
            ib_mm_t         mm,
            const uint8_t  *data_in,
            size_t          dlen_in,
            uint8_t       **data_out,
            size_t         *dlen_out
        )
    > which,
    const string& s
)
{
    ScopedMemoryPoolLite mpl;

    char *out = NULL;
    size_t out_len = 0;
    ib_status_t rc;

    rc = which(
        MemoryManager(mpl).ib(),
        reinterpret_cast<const uint8_t*>(s.data()), s.length(),
        reinterpret_cast<uint8_t**>(&out), &out_len
    );
    if (rc != IB_OK) {
        throw runtime_error("Did not return IB_OK");
    }

    return string(out, out_len);
}

}

TEST(TestStringWhitespace, str_whitespace_remove)
{
    EXPECT_EQ("abc", strws(ib_str_whitespace_remove, "  a  b   c  "));
    EXPECT_EQ("abc", strws(ib_str_whitespace_remove, "abc"));
    EXPECT_EQ("abc", strws(ib_str_whitespace_remove, "a b c"));
    EXPECT_EQ("", strws(ib_str_whitespace_remove, ""));
}

TEST(TestStringWhitespace, str_whitespace_compress)
{
    EXPECT_EQ(" a b c ", strws(ib_str_whitespace_compress, "  a  b   c  "));
    EXPECT_EQ("abc", strws(ib_str_whitespace_compress, "abc"));
    EXPECT_EQ("a b c", strws(ib_str_whitespace_compress, "a b c"));
    EXPECT_EQ("", strws(ib_str_whitespace_compress, ""));
}
