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
 * @brief IronBee++ String Set Tests
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 **/

#include "ironbee_config_auto.h"
#include "gtest/gtest.h"

#include <ironbee/stringset.h>
#include <ironbee/string.h>

using namespace std;

TEST(TestStringSet, Empty)
{
    ib_stringset_t set;
    ib_stringset_entry_t entries;

    ASSERT_EQ(IB_OK, ib_stringset_init(&set, &entries, 0));

    EXPECT_EQ(IB_ENOENT, ib_stringset_query(&set, IB_S2SL("foo"), NULL));
}

TEST(TestStringSet, Easy)
{
    int a = 1;
    ib_stringset_t set;
    ib_stringset_entry_t entries[3] = {
        {"foo", 3, &a},
        {"bar", 3, NULL},
        {"baz", 3, NULL}
    };

    ASSERT_EQ(IB_OK, ib_stringset_init(&set, entries, 3));

    EXPECT_EQ(IB_ENOENT, ib_stringset_query(&set, IB_S2SL("hello"), NULL));

    const ib_stringset_entry_t* result;

    ASSERT_EQ(IB_OK, ib_stringset_query(&set, IB_S2SL("foo"), &result));
    EXPECT_EQ(&a, result->data);
}

TEST(TestStringSet, Prefixed)
{
    int a = 1;
    ib_stringset_t set;
    ib_stringset_entry_t entries[4] = {
        {"bar", 3, NULL},
        {"a", 1, NULL},
        {"aaa", 3, &a},
        {"aa", 2, NULL}
    };

    ASSERT_EQ(IB_OK, ib_stringset_init(&set, entries, 4));

    EXPECT_EQ(IB_ENOENT, ib_stringset_query(&set, IB_S2SL("hello"), NULL));

    const ib_stringset_entry_t* result;

    ASSERT_EQ(IB_OK, ib_stringset_query(&set, IB_S2SL("aaaaaa"), &result));
    EXPECT_EQ("aaa", string(result->string, result->length));
    EXPECT_EQ(&a, result->data);
}

TEST(TestStringSet, BeforeStart)
{
    ib_stringset_t set;
    ib_stringset_entry_t entries[4] = {
        {"d", 1, NULL},
        {"e", 1, NULL},
        {"f", 1, NULL},
    };

    ASSERT_EQ(IB_OK, ib_stringset_init(&set, entries, 3));

    EXPECT_EQ(IB_ENOENT, ib_stringset_query(&set, IB_S2SL("c"), NULL));
}

TEST(TestStringSet, AfterEnd)
{
    ib_stringset_t set;
    ib_stringset_entry_t entries[4] = {
        {"d", 1, NULL},
        {"e", 1, NULL},
        {"f", 1, NULL},
    };

    ASSERT_EQ(IB_OK, ib_stringset_init(&set, entries, 3));

    EXPECT_EQ(IB_ENOENT, ib_stringset_query(&set, IB_S2SL("g"), NULL));
}
