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
#include <ironbeepp/list.hpp>
#include <ironbee/type_convert.h>

#include "gtest/gtest.h"

using namespace std;
using namespace IronBee;

TEST(TestString, string_to_num)
{
    ib_num_t n;

    EXPECT_EQ(IB_OK, ib_type_atoi_ex(IB_S2SL("1234"), 10, &n));
    EXPECT_EQ(1234L, n);
    EXPECT_EQ(IB_OK, ib_type_atoi_ex(IB_S2SL("-1234"), 10, &n));
    EXPECT_EQ(-1234L, n);
    EXPECT_EQ(IB_EINVAL, ib_type_atoi_ex(IB_S2SL(""), 10, &n));
    EXPECT_EQ(IB_OK, ib_type_atoi_ex(IB_S2SL("1234"), 0, &n));
    EXPECT_EQ(1234L, n);
    EXPECT_EQ(IB_OK, ib_type_atoi_ex(IB_S2SL("0x1234"), 0, &n));
    EXPECT_EQ(0x1234L, n);

    EXPECT_EQ(IB_OK, ib_type_atoi("1234", 10, &n));
    EXPECT_EQ(1234L, n);
    EXPECT_EQ(IB_OK, ib_type_atoi("-1234", 10, &n));
    EXPECT_EQ(-1234L, n);
    EXPECT_EQ(IB_EINVAL, ib_type_atoi("", 10, &n));
    EXPECT_EQ(IB_OK, ib_type_atoi("1234", 0, &n));
    EXPECT_EQ(1234L, n);
    EXPECT_EQ(IB_OK, ib_type_atoi("0x1234", 0, &n));
    EXPECT_EQ(0x1234L, n);
}

TEST(TestString, string_to_time)
{
    ib_time_t t;

    EXPECT_EQ(IB_OK, ib_type_atot_ex(IB_S2SL("1234"), &t));
    EXPECT_EQ(1234UL, t);
    EXPECT_EQ(IB_EINVAL, ib_type_atot_ex(IB_S2SL(""), &t));

    EXPECT_EQ(IB_OK, ib_type_atot("1234", &t));
    EXPECT_EQ(1234UL, t);
    EXPECT_EQ(IB_EINVAL, ib_type_atot("", &t));
}

TEST(TestString, string_to_float)
{
    ib_float_t f;

    EXPECT_EQ(IB_OK, ib_type_atof_ex(IB_S2SL("1234"), &f));
    EXPECT_FLOAT_EQ(1234, f);
    EXPECT_EQ(IB_OK, ib_type_atof_ex(IB_S2SL("12.34"), &f));
    EXPECT_FLOAT_EQ(12.34, f);
    EXPECT_EQ(IB_EINVAL, ib_type_atof_ex(IB_S2SL(""), &f));

    EXPECT_EQ(IB_OK, ib_type_atof("1234", &f));
    EXPECT_FLOAT_EQ(1234, f);
    EXPECT_EQ(IB_OK, ib_type_atof("12.34", &f));
    EXPECT_FLOAT_EQ(12.34, f);
    EXPECT_EQ(IB_EINVAL, ib_type_atof("", &f));
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
    EXPECT_EQ(haystack, result);
    result = ib_strstr(haystack, strlen(haystack), "xx", 2);
    EXPECT_FALSE(result);
    result = ib_strstr(haystack, strlen(haystack), "hello world and more", 20);
    EXPECT_FALSE(result);
}

TEST(TestString, num_to_string)
{
    IronBee::ScopedMemoryPoolLite mpl;
    ib_mm_t mm = MemoryManager(mpl).ib();

    EXPECT_EQ(string("1234"), ib_type_itoa(mm, 1234));
    EXPECT_EQ(string("-1234"), ib_type_itoa(mm, -1234));
}

TEST(TestString, time_to_string)
{
    IronBee::ScopedMemoryPoolLite mpl;
    ib_mm_t mm = MemoryManager(mpl).ib();

    EXPECT_EQ(string("1234"), ib_type_ttoa(mm, 1234));
}

TEST(TestString, float_to_string)
{
    IronBee::ScopedMemoryPoolLite mpl;
    ib_mm_t mm = MemoryManager(mpl).ib();

    EXPECT_EQ(string("12.340000"), ib_type_ftoa(mm, 12.34));
}

TEST(TestString, string_join) {
    using namespace IronBee;

    ScopedMemoryPoolLite mp;
    MemoryManager        mm(mp);
    List<const char *> l = List<const char *>::create(mp);
    const char *str;
    size_t      len;

    l.push_back("hi");
    l.push_back("bye");

    ASSERT_EQ(
        IB_OK,
        ib_string_join(
            ",",
            l.ib(),
            mm.ib(),
            &str,
            &len
        )
    );

    ASSERT_EQ(6UL, len);
    ASSERT_STREQ("hi,bye", str);
}

TEST(TestString, string_join_zero_len) {
    using namespace IronBee;

    ScopedMemoryPoolLite mp;
    MemoryManager        mm(mp);
    List<const char *> l = List<const char *>::create(mp);
    const char *str;
    size_t      len;

    ASSERT_EQ(
        IB_OK,
        ib_string_join(
            ",",
            l.ib(),
            mm.ib(),
            &str,
            &len
        )
    );

    ASSERT_EQ(0UL, len);
    ASSERT_STREQ("", str);
}
