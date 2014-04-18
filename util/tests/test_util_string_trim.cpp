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
 * @brief Predicate --- String Trim Tests
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 **/

#include "ironbee_config_auto.h"

#include <ironbee/string_trim.h>

#include <boost/function.hpp>

#include "gtest/gtest.h"

using namespace std;

namespace {

string strtrim(
    boost::function<
        ib_status_t(
            const uint8_t  *data_in,
            size_t          dlen_in,
            const uint8_t **data_out,
            size_t         *dlen_out
        )
    > which,
    const string& s
)
{
    const char *out = NULL;
    size_t out_len = 0;
    ib_status_t rc;

    rc = which(
        reinterpret_cast<const uint8_t*>(s.data()), s.length(),
        reinterpret_cast<const uint8_t**>(&out), &out_len
    );
    if (rc != IB_OK) {
        throw runtime_error("Did not return IB_OK");
    }

    return string(out, out_len);
}

}

TEST(TestStringTrim, strtrim_left)
{
    EXPECT_EQ("a b c", strtrim(ib_strtrim_left, "a b c"));
    EXPECT_EQ("a b c", strtrim(ib_strtrim_left, "   a b c"));
    EXPECT_EQ("a b c   ", strtrim(ib_strtrim_left, "a b c   "));
    EXPECT_EQ("a b c   ", strtrim(ib_strtrim_left, "   a b c   "));
    EXPECT_EQ("", strtrim(ib_strtrim_left, ""));
}

TEST(TestStringTrim, strtrim_right)
{
    EXPECT_EQ("a b c", strtrim(ib_strtrim_right, "a b c"));
    EXPECT_EQ("   a b c", strtrim(ib_strtrim_right, "   a b c"));
    EXPECT_EQ("a b c", strtrim(ib_strtrim_right, "a b c   "));
    EXPECT_EQ("   a b c", strtrim(ib_strtrim_right, "   a b c   "));
    EXPECT_EQ("", strtrim(ib_strtrim_right, ""));
}

TEST(TestStringTrim, strtrim_lr)
{
    EXPECT_EQ("a b c", strtrim(ib_strtrim_lr, "a b c"));
    EXPECT_EQ("a b c", strtrim(ib_strtrim_lr, "   a b c"));
    EXPECT_EQ("a b c", strtrim(ib_strtrim_lr, "a b c   "));
    EXPECT_EQ("a b c", strtrim(ib_strtrim_lr, "   a b c   "));
    EXPECT_EQ("", strtrim(ib_strtrim_lr, ""));
}
