//////////////////////////////////////////////////////////////////////////////
// Licensed to Qualys, Inc. (QUALYS) under one or more
// contributor license agreements.  See the NOTICE file distributed with
// this work for additional information regarding copyright ownership.
// QUALYS licenses this file to You under the Apache License, Version 2.0
// (the "License"); you may not use this file except in compliance with
// the License.  You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
/// @file
/// @brief IronBee --- String/value parser utility tests
///
/// @author Nick LeRoy <nleroy@qualys.com>
//////////////////////////////////////////////////////////////////////////////

#include <ironbee/flags.h>

#include "ironbee_config_auto.h"

#include "gtest/gtest.h"

#include <ironbee/list.h>
#include <ironbee/mpool.h>
#include <ironbee/strval.h>
#include <ironbee/types.h>

#include <stdexcept>

const ib_flags_t FLAG_01 = (1 <<  0);
const ib_flags_t FLAG_02 = (1 <<  1);
const ib_flags_t FLAG_03 = (1 <<  2);
const ib_flags_t FLAG_04 = (1 <<  3);
const ib_flags_t FLAG_05 = (1 <<  4);
const ib_flags_t FLAG_06 = (1 <<  5);
const ib_flags_t FLAG_07 = (1 <<  6);
const ib_flags_t FLAG_08 = (1 <<  7);
const ib_flags_t FLAG_09 = (1 <<  8);
const ib_flags_t FLAG_10 = (1 <<  9);
const ib_flags_t FLAG_11 = (1 << 10);
const ib_flags_t FLAG_12 = (1 << 11);
const ib_flags_t FLAG_13 = (1 << 12);
const ib_flags_t FLAG_14 = (1 << 13);
const ib_flags_t FLAG_15 = (1 << 14);
const ib_flags_t FLAG_16 = (1 << 15);
const ib_flags_t FLAG_SET_01 = (FLAG_01|FLAG_02|FLAG_03);
const ib_flags_t FLAG_SET_02 = (FLAG_01|FLAG_02|FLAG_10|FLAG_11);
const ib_flags_t FLAGS_ALL = (0xffffffff);

static IB_STRVAL_MAP(flag_map) = {
    IB_STRVAL_PAIR("flag-01", FLAG_01),
    IB_STRVAL_PAIR("flag-02", FLAG_02),
    IB_STRVAL_PAIR("flag-03", FLAG_03),
    IB_STRVAL_PAIR("flag-04", FLAG_04),
    IB_STRVAL_PAIR("flag-05", FLAG_05),
    IB_STRVAL_PAIR("flag-06", FLAG_06),
    IB_STRVAL_PAIR("flag-07", FLAG_07),
    IB_STRVAL_PAIR("flag-08", FLAG_08),
    IB_STRVAL_PAIR("flag-09", FLAG_09),
    IB_STRVAL_PAIR("flag-10", FLAG_10),
    IB_STRVAL_PAIR("flag-11", FLAG_11),
    IB_STRVAL_PAIR("flag-12", FLAG_12),
    IB_STRVAL_PAIR("flag-13", FLAG_13),
    IB_STRVAL_PAIR("flag-14", FLAG_14),
    IB_STRVAL_PAIR("flag-15", FLAG_15),
    IB_STRVAL_PAIR("flag-16", FLAG_16),
    IB_STRVAL_PAIR("flag-set-01", FLAG_SET_01),
    IB_STRVAL_PAIR("flag-set-02", FLAG_SET_02),
    IB_STRVAL_PAIR_LAST
}; 

TEST(TestFlags, test_flags)
{
    ib_flags_t flags;

    flags = 0x0;
    ib_flags_set(flags, FLAG_01);
    ASSERT_EQ(FLAG_01, flags);

    ib_flags_set(flags, FLAG_02);
    ASSERT_EQ(FLAG_01 | FLAG_02, flags);

    ib_flags_set(flags, FLAG_03);
    ASSERT_EQ(FLAG_01 | FLAG_02 | FLAG_03, flags);

    ib_flags_clear(flags, FLAG_01);
    ASSERT_EQ(FLAG_02 | FLAG_03, flags);

    ib_flags_set(flags, FLAG_04 | FLAG_05);
    ASSERT_EQ(FLAG_02 | FLAG_03 | FLAG_04 | FLAG_05, flags);

    ib_flags_clear(flags, FLAG_02 | FLAG_03);
    ASSERT_EQ(FLAG_04 | FLAG_05, flags);

    flags = (FLAG_01 | FLAG_02 | FLAG_03 | FLAG_04);
    ASSERT_TRUE (ib_flags_any(flags, FLAG_01 | FLAG_02));
    ASSERT_TRUE (ib_flags_any(flags, FLAG_01 | FLAG_02 | FLAG_03 | FLAG_04));
    ASSERT_TRUE (ib_flags_any(flags, FLAG_01 | FLAG_05));
    ASSERT_FALSE(ib_flags_any(flags, FLAG_05 | FLAG_06));

    ASSERT_TRUE (ib_flags_all(flags, FLAG_01));
    ASSERT_TRUE (ib_flags_all(flags, FLAG_01 | FLAG_02));
    ASSERT_TRUE (ib_flags_all(flags, FLAG_01 | FLAG_02 | FLAG_03 | FLAG_04));
    ASSERT_FALSE(ib_flags_all(flags, FLAG_01 | FLAG_05));
    ASSERT_FALSE(ib_flags_all(flags, FLAG_05 | FLAG_06));
}

TEST(TestStrVal, test_flags_string)
{
    int         n = 0;
    ib_status_t rc;
    ib_flags_t  flags = 0;
    ib_flags_t  mask = 0;

    rc = ib_flags_string(flag_map, "flag-01", n++, &flags, &mask);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_EQ(FLAG_01, flags);
    ASSERT_EQ(FLAGS_ALL, mask);

    rc = ib_flags_string(flag_map, "+flag-02", n++, &flags, &mask);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_EQ(FLAG_01|FLAG_02, flags);
    ASSERT_EQ(FLAGS_ALL, mask);

    n = 0;
    rc = ib_flags_string(flag_map, "flag-set-01", n++, &flags, &mask);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_EQ(FLAG_SET_01, flags);
    ASSERT_EQ(FLAGS_ALL, mask);

    rc = ib_flags_string(flag_map, "+flag-10", n++, &flags, &mask);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_EQ( (FLAG_SET_01|FLAG_10), flags);
    ASSERT_EQ(FLAGS_ALL, mask);

    rc = ib_flags_string(flag_map, "-flag-01", n++, &flags, &mask);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_EQ( ((FLAG_SET_01|FLAG_10) & (~FLAG_01)), flags);
    ASSERT_EQ(FLAGS_ALL, mask);

    n = 0;
    flags = 0;
    mask = 0;
    rc = ib_flags_string(flag_map, "+flag-set-02", n++, &flags, &mask);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_EQ(FLAG_SET_02, flags);
    ASSERT_EQ(FLAG_SET_02, mask);

    rc = ib_flags_string(flag_map, "-flag-01", n++, &flags, &mask);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_EQ( (FLAG_SET_02 & (~FLAG_01)), flags);
    ASSERT_EQ(FLAG_SET_02, mask);

    rc = ib_flags_string(flag_map, "+flag-04", n++, &flags, &mask);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_EQ( ((FLAG_SET_02 | FLAG_04) & (~FLAG_01)), flags);
    ASSERT_EQ( (FLAG_SET_02|FLAG_04), mask);

    rc = ib_flags_string(flag_map, "+flag-10", n++, &flags, &mask);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_EQ( ((FLAG_SET_02 | FLAG_04 | FLAG_10) & (~FLAG_01)), flags);
    ASSERT_EQ( (FLAG_SET_02|FLAG_04|FLAG_10), mask);
}

TEST(TestStrVal, test_flags_strtok)
{
    ib_status_t  rc;
    ib_flags_t   flags = 0;
    ib_flags_t   mask = 0;
    const char  *str;
    ib_mpool_t  *mp;

    rc = ib_mpool_create(&mp, "test", NULL);
    ASSERT_EQ(IB_OK, rc);

    str = "flag-01,+flag-02";
    rc = ib_flags_strtok(flag_map, mp, str, ",", &flags, &mask);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_EQ(FLAG_01|FLAG_02, flags);
    ASSERT_EQ(FLAGS_ALL, mask);

    str = "flag-set-01,+flag-10,-flag-01";
    rc = ib_flags_strtok(flag_map, mp, str, ",", &flags, &mask);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_EQ( ((FLAG_SET_01|FLAG_10) & (~FLAG_01)), flags);
    ASSERT_EQ(FLAGS_ALL, mask);

    flags = 0;
    mask = 0;
    str = "+flag-set-02;-flag-01;+flag-04;+flag-10";
    rc = ib_flags_strtok(flag_map, mp, str, ";", &flags, &mask);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_EQ( ((FLAG_SET_02 | FLAG_04 | FLAG_10) & (~FLAG_01)), flags);
    ASSERT_EQ( (FLAG_SET_02|FLAG_04|FLAG_10), mask);
}

TEST(TestStrVal, test_flags_strlist)
{
    ib_status_t  rc;
    ib_flags_t   flags = 0;
    ib_flags_t   mask = 0;
    ib_mpool_t  *mp;
    ib_list_t   *strlist;
    const char  *error;

    rc = ib_mpool_create(&mp, "test", NULL);
    ASSERT_EQ(IB_OK, rc);
    rc = ib_list_create(&strlist, mp);
    ASSERT_EQ(IB_OK, rc);

    ib_list_clear(strlist);
    ASSERT_EQ(IB_OK, ib_list_push(strlist,
                                  ib_mpool_strdup(mp, "flag-01")));
    ASSERT_EQ(IB_OK, ib_list_push(strlist,
                                  ib_mpool_strdup(mp, "+flag-02")));
    rc = ib_flags_strlist(flag_map, strlist, &flags, &mask, &error);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_STREQ(NULL, error);
    ASSERT_EQ(FLAG_01|FLAG_02, flags);
    ASSERT_EQ(FLAGS_ALL, mask);

    ib_list_clear(strlist);
    ASSERT_EQ(IB_OK, ib_list_push(strlist,
                                  ib_mpool_strdup(mp, "flag-set-01")));
    ASSERT_EQ(IB_OK, ib_list_push(strlist,
                                  ib_mpool_strdup(mp, "+flag-10")));
    ASSERT_EQ(IB_OK, ib_list_push(strlist,
                                  ib_mpool_strdup(mp, "-flag-01")));
    rc = ib_flags_strlist(flag_map, strlist, &flags, &mask, &error);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_STREQ(NULL, error);
    ASSERT_EQ( ((FLAG_SET_01|FLAG_10) & (~FLAG_01)), flags);
    ASSERT_EQ(FLAGS_ALL, mask);

    flags = 0;
    mask = 0;
    ib_list_clear(strlist);
    ASSERT_EQ(IB_OK, ib_list_push(strlist,
                                  ib_mpool_strdup(mp, "+flag-set-02")));
    ASSERT_EQ(IB_OK, ib_list_push(strlist,
                                  ib_mpool_strdup(mp, "-flag-01")));
    ASSERT_EQ(IB_OK, ib_list_push(strlist,
                                  ib_mpool_strdup(mp, "+flag-04")));
    ASSERT_EQ(IB_OK, ib_list_push(strlist,
                                  ib_mpool_strdup(mp, "+flag-10")));
    rc = ib_flags_strlist(flag_map, strlist, &flags, &mask, &error);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_STREQ(NULL, error);
    ASSERT_EQ( ((FLAG_SET_02 | FLAG_04 | FLAG_10) & (~FLAG_01)), flags);
    ASSERT_EQ( (FLAG_SET_02|FLAG_04|FLAG_10), mask);

    ib_list_clear(strlist);
    ASSERT_EQ(IB_OK, ib_list_push(strlist, ib_mpool_strdup(mp, "+xyzzy")));
    rc = ib_flags_strlist(flag_map, strlist, &flags, &mask, &error);
    ASSERT_EQ(IB_ENOENT, rc);
    ASSERT_STREQ("+xyzzy", error);

    ib_list_clear(strlist);
    ASSERT_EQ(IB_OK, ib_list_push(strlist, ib_mpool_strdup(mp, "+flag-01")));
    ASSERT_EQ(IB_OK, ib_list_push(strlist, ib_mpool_strdup(mp, "+flag-02")));
    ASSERT_EQ(IB_OK, ib_list_push(strlist, ib_mpool_strdup(mp, "+xyzzy")));
    rc = ib_flags_strlist(flag_map, strlist, &flags, &mask, &error);
    ASSERT_EQ(IB_ENOENT, rc);
    ASSERT_STREQ("+xyzzy", error);
}

TEST(TestStrVal, test_flags_oplist)
{
    ib_status_t  rc;
    ib_flags_t   flags = 0;
    ib_flags_t   mask = 0;
    const char  *str;
    ib_mpool_t  *mp;
    ib_list_t   *oplist;

    rc = ib_mpool_create(&mp, "test", NULL);
    ASSERT_EQ(IB_OK, rc);
    rc = ib_list_create(&oplist, mp);
    ASSERT_EQ(IB_OK, rc);

    str = "flag-01,+flag-02";
    rc = ib_flags_oplist_parse(flag_map, mp, str, ",", oplist);
    ASSERT_EQ(IB_OK, rc);
    rc = ib_flags_oplist_apply(oplist, &flags, &mask);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_EQ(FLAG_01|FLAG_02, flags);
    ASSERT_EQ(FLAGS_ALL, mask);

    str = "flag-set-01,+flag-10,-flag-01";
    rc = ib_flags_oplist_parse(flag_map, mp, str, ",", oplist);
    ASSERT_EQ(IB_OK, rc);
    rc = ib_flags_oplist_apply(oplist, &flags, &mask);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_EQ( ((FLAG_SET_01|FLAG_10) & (~FLAG_01)), flags);
    ASSERT_EQ(FLAGS_ALL, mask);

    flags = 0;
    mask = 0;
    str = "+flag-set-02;-flag-01;+flag-04;+flag-10";
    rc = ib_flags_oplist_parse(flag_map, mp, str, ";", oplist);
    ASSERT_EQ(IB_OK, rc);
    rc = ib_flags_oplist_apply(oplist, &flags, &mask);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_EQ( ((FLAG_SET_02 | FLAG_04 | FLAG_10) & (~FLAG_01)), flags);
    ASSERT_EQ( (FLAG_SET_02|FLAG_04|FLAG_10), mask);
}
