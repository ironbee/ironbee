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
 * @brief Predicate --- String Tests
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 **/

#include "ironbee_config_auto.h"

#include <ironbee/string.h>
#include <ironbeepp/memory_manager.hpp>
#include <ironbeepp/memory_pool_lite.hpp>

#include "gtest/gtest.h"

using namespace std;
using namespace IronBee;

TEST(TestString, string_to_num)
{
    ib_num_t n;

    EXPECT_EQ(IB_OK, ib_string_to_num_ex(IB_S2SL("1234"), 10, &n));
    EXPECT_EQ(1234L, n);
    EXPECT_EQ(IB_OK, ib_string_to_num_ex(IB_S2SL("-1234"), 10, &n));
    EXPECT_EQ(-1234L, n);
    EXPECT_EQ(IB_EINVAL, ib_string_to_num_ex(IB_S2SL(""), 10, &n));
    EXPECT_EQ(IB_OK, ib_string_to_num_ex(IB_S2SL("1234"), 0, &n));
    EXPECT_EQ(1234L, n);
    EXPECT_EQ(IB_OK, ib_string_to_num_ex(IB_S2SL("0x1234"), 0, &n));
    EXPECT_EQ(0x1234L, n);

    EXPECT_EQ(IB_OK, ib_string_to_num("1234", 10, &n));
    EXPECT_EQ(1234L, n);
    EXPECT_EQ(IB_OK, ib_string_to_num("-1234", 10, &n));
    EXPECT_EQ(-1234L, n);
    EXPECT_EQ(IB_EINVAL, ib_string_to_num("", 10, &n));
    EXPECT_EQ(IB_OK, ib_string_to_num("1234", 0, &n));
    EXPECT_EQ(1234L, n);
    EXPECT_EQ(IB_OK, ib_string_to_num("0x1234", 0, &n));
    EXPECT_EQ(0x1234L, n);
}

TEST(TestString, string_to_time)
{
    ib_time_t t;

    EXPECT_EQ(IB_OK, ib_string_to_time_ex(IB_S2SL("1234"), &t));
    EXPECT_EQ(1234UL, t);
    EXPECT_EQ(IB_EINVAL, ib_string_to_time_ex(IB_S2SL(""), &t));

    EXPECT_EQ(IB_OK, ib_string_to_time("1234", &t));
    EXPECT_EQ(1234UL, t);
    EXPECT_EQ(IB_EINVAL, ib_string_to_time("", &t));
}

TEST(TestString, string_to_float)
{
    ib_float_t f;

    EXPECT_EQ(IB_OK, ib_string_to_float_ex(IB_S2SL("1234"), &f));
    EXPECT_FLOAT_EQ(1234, f);
    EXPECT_EQ(IB_OK, ib_string_to_float_ex(IB_S2SL("12.34"), &f));
    EXPECT_FLOAT_EQ(12.34, f);
    EXPECT_EQ(IB_EINVAL, ib_string_to_float_ex(IB_S2SL(""), &f));

    EXPECT_EQ(IB_OK, ib_string_to_float("1234", &f));
    EXPECT_FLOAT_EQ(1234, f);
    EXPECT_EQ(IB_OK, ib_string_to_float("12.34", &f));
    EXPECT_FLOAT_EQ(12.34, f);
    EXPECT_EQ(IB_EINVAL, ib_string_to_float("", &f));
}

TEST(TestString, strstr)
{
    const char *haystack;
    const char *result;

    haystack = "hello world";
    result = ib_strstr(haystack, strlen(haystack), "el", 2);
    EXPECT_EQ(haystack + 1, result);
    result = ib_strstr(haystack, strlen(haystack), "ld", 2);
    EXPECT_EQ(haystack + 9, result);
    result = ib_strstr(haystack, strlen(haystack), "he", 2);
    EXPECT_EQ(haystack, result);
    result = ib_strstr(haystack, strlen(haystack), "", 0);
    EXPECT_FALSE(result);
    result = ib_strstr(haystack, strlen(haystack), "xx", 2);
    EXPECT_FALSE(result);
}

TEST(TestString, num_to_string)
{
    IronBee::ScopedMemoryPoolLite mpl;
    ib_mm_t mm = MemoryManager(mpl).ib();

    EXPECT_EQ(string("1234"), ib_num_to_string(mm, 1234));
    EXPECT_EQ(string("-1234"), ib_num_to_string(mm, -1234));
}

TEST(TestString, time_to_string)
{
    IronBee::ScopedMemoryPoolLite mpl;
    ib_mm_t mm = MemoryManager(mpl).ib();

    EXPECT_EQ(string("1234"), ib_time_to_string(mm, 1234));
}

TEST(TestString, float_to_string)
{
    IronBee::ScopedMemoryPoolLite mpl;
    ib_mm_t mm = MemoryManager(mpl).ib();

    EXPECT_EQ(string("12.340000"), ib_float_to_string(mm, 12.34));
}
