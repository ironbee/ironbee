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
#include <ironbee/mm_mpool.h>
#include <ironbee/strval.h>
#include <ironbee/types.h>

#include <stdexcept>

const ib_flags_t FLAG_01 = ((ib_flags_t)1 <<  0);
const ib_flags_t FLAG_02 = ((ib_flags_t)1 <<  1);
const ib_flags_t FLAG_03 = ((ib_flags_t)1 <<  2);
const ib_flags_t FLAG_04 = ((ib_flags_t)1 <<  3);
const ib_flags_t FLAG_05 = ((ib_flags_t)1 <<  4);
const ib_flags_t FLAG_06 = ((ib_flags_t)1 <<  5);
const ib_flags_t FLAG_07 = ((ib_flags_t)1 <<  6);
const ib_flags_t FLAG_08 = ((ib_flags_t)1 <<  7);
const ib_flags_t FLAG_09 = ((ib_flags_t)1 <<  8);
const ib_flags_t FLAG_10 = ((ib_flags_t)1 <<  9);
const ib_flags_t FLAG_11 = ((ib_flags_t)1 << 10);
const ib_flags_t FLAG_12 = ((ib_flags_t)1 << 11);
const ib_flags_t FLAG_13 = ((ib_flags_t)1 << 12);
const ib_flags_t FLAG_14 = ((ib_flags_t)1 << 13);
const ib_flags_t FLAG_15 = ((ib_flags_t)1 << 14);
const ib_flags_t FLAG_16 = ((ib_flags_t)1 << 15);
const ib_flags_t FLAG_17 = ((ib_flags_t)1 << 16);
const ib_flags_t FLAG_18 = ((ib_flags_t)1 << 17);
const ib_flags_t FLAG_19 = ((ib_flags_t)1 << 18);
const ib_flags_t FLAG_20 = ((ib_flags_t)1 << 19);
const ib_flags_t FLAG_21 = ((ib_flags_t)1 << 20);
const ib_flags_t FLAG_22 = ((ib_flags_t)1 << 21);
const ib_flags_t FLAG_23 = ((ib_flags_t)1 << 22);
const ib_flags_t FLAG_24 = ((ib_flags_t)1 << 23);
const ib_flags_t FLAG_25 = ((ib_flags_t)1 << 24);
const ib_flags_t FLAG_26 = ((ib_flags_t)1 << 25);
const ib_flags_t FLAG_27 = ((ib_flags_t)1 << 26);
const ib_flags_t FLAG_28 = ((ib_flags_t)1 << 27);
const ib_flags_t FLAG_29 = ((ib_flags_t)1 << 28);
const ib_flags_t FLAG_30 = ((ib_flags_t)1 << 29);
const ib_flags_t FLAG_31 = ((ib_flags_t)1 << 30);
const ib_flags_t FLAG_32 = ((ib_flags_t)1 << 31);
const ib_flags_t FLAG_33 = ((ib_flags_t)1 << 32);
const ib_flags_t FLAG_34 = ((ib_flags_t)1 << 33);
const ib_flags_t FLAG_35 = ((ib_flags_t)1 << 34);
const ib_flags_t FLAG_36 = ((ib_flags_t)1 << 35);
const ib_flags_t FLAG_37 = ((ib_flags_t)1 << 36);
const ib_flags_t FLAG_38 = ((ib_flags_t)1 << 37);
const ib_flags_t FLAG_39 = ((ib_flags_t)1 << 38);
const ib_flags_t FLAG_40 = ((ib_flags_t)1 << 39);
const ib_flags_t FLAG_41 = ((ib_flags_t)1 << 40);
const ib_flags_t FLAG_42 = ((ib_flags_t)1 << 41);
const ib_flags_t FLAG_43 = ((ib_flags_t)1 << 42);
const ib_flags_t FLAG_44 = ((ib_flags_t)1 << 43);
const ib_flags_t FLAG_45 = ((ib_flags_t)1 << 44);
const ib_flags_t FLAG_46 = ((ib_flags_t)1 << 45);
const ib_flags_t FLAG_47 = ((ib_flags_t)1 << 46);
const ib_flags_t FLAG_48 = ((ib_flags_t)1 << 47);
const ib_flags_t FLAG_49 = ((ib_flags_t)1 << 48);
const ib_flags_t FLAG_50 = ((ib_flags_t)1 << 49);
const ib_flags_t FLAG_51 = ((ib_flags_t)1 << 50);
const ib_flags_t FLAG_52 = ((ib_flags_t)1 << 51);
const ib_flags_t FLAG_53 = ((ib_flags_t)1 << 52);
const ib_flags_t FLAG_54 = ((ib_flags_t)1 << 53);
const ib_flags_t FLAG_55 = ((ib_flags_t)1 << 54);
const ib_flags_t FLAG_56 = ((ib_flags_t)1 << 55);
const ib_flags_t FLAG_57 = ((ib_flags_t)1 << 56);
const ib_flags_t FLAG_58 = ((ib_flags_t)1 << 57);
const ib_flags_t FLAG_59 = ((ib_flags_t)1 << 58);
const ib_flags_t FLAG_60 = ((ib_flags_t)1 << 59);
const ib_flags_t FLAG_61 = ((ib_flags_t)1 << 60);
const ib_flags_t FLAG_62 = ((ib_flags_t)1 << 61);
const ib_flags_t FLAG_63 = ((ib_flags_t)1 << 62);
const ib_flags_t FLAG_64 = ((ib_flags_t)1 << 63);
const ib_flags_t FLAG_SET_01 = (FLAG_01|FLAG_02|FLAG_03);
const ib_flags_t FLAG_SET_02 = (FLAG_01|FLAG_02|FLAG_10|FLAG_11);
const ib_flags_t FLAG_SET_03 = (FLAG_16|FLAG_17|FLAG_18);
const ib_flags_t FLAG_SET_04 = (FLAG_16|FLAG_17|FLAG_18|FLAG_19);
const ib_flags_t FLAG_SET_05 = (FLAG_32|FLAG_33|FLAG_34);
const ib_flags_t FLAG_SET_06 = (FLAG_32|FLAG_33|FLAG_34|FLAG_35);
const ib_flags_t FLAG_SET_07 = (FLAG_61|FLAG_62|FLAG_63);
const ib_flags_t FLAG_SET_08 = (FLAG_61|FLAG_62|FLAG_63|FLAG_64);
const ib_flags_t FLAG_SET_09 = (FLAG_01|FLAG_16|FLAG_31|FLAG_63);
const ib_flags_t FLAG_SET_10 = (FLAG_01|FLAG_16|FLAG_31|FLAG_63|FLAG_64);
const ib_flags_t FLAGS_ALL = ~(0x0);

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
    IB_STRVAL_PAIR("flag-17", FLAG_17),
    IB_STRVAL_PAIR("flag-18", FLAG_18),
    IB_STRVAL_PAIR("flag-19", FLAG_19),
    IB_STRVAL_PAIR("flag-20", FLAG_20),
    IB_STRVAL_PAIR("flag-21", FLAG_21),
    IB_STRVAL_PAIR("flag-22", FLAG_22),
    IB_STRVAL_PAIR("flag-23", FLAG_23),
    IB_STRVAL_PAIR("flag-24", FLAG_24),
    IB_STRVAL_PAIR("flag-25", FLAG_25),
    IB_STRVAL_PAIR("flag-26", FLAG_26),
    IB_STRVAL_PAIR("flag-27", FLAG_27),
    IB_STRVAL_PAIR("flag-28", FLAG_28),
    IB_STRVAL_PAIR("flag-29", FLAG_29),
    IB_STRVAL_PAIR("flag-30", FLAG_30),
    IB_STRVAL_PAIR("flag-31", FLAG_31),
    IB_STRVAL_PAIR("flag-32", FLAG_32),
    IB_STRVAL_PAIR("flag-33", FLAG_33),
    IB_STRVAL_PAIR("flag-34", FLAG_34),
    IB_STRVAL_PAIR("flag-35", FLAG_35),
    IB_STRVAL_PAIR("flag-36", FLAG_36),
    IB_STRVAL_PAIR("flag-37", FLAG_37),
    IB_STRVAL_PAIR("flag-38", FLAG_38),
    IB_STRVAL_PAIR("flag-39", FLAG_39),
    IB_STRVAL_PAIR("flag-40", FLAG_40),
    IB_STRVAL_PAIR("flag-41", FLAG_41),
    IB_STRVAL_PAIR("flag-42", FLAG_42),
    IB_STRVAL_PAIR("flag-43", FLAG_43),
    IB_STRVAL_PAIR("flag-44", FLAG_44),
    IB_STRVAL_PAIR("flag-45", FLAG_45),
    IB_STRVAL_PAIR("flag-46", FLAG_46),
    IB_STRVAL_PAIR("flag-47", FLAG_47),
    IB_STRVAL_PAIR("flag-48", FLAG_48),
    IB_STRVAL_PAIR("flag-49", FLAG_49),
    IB_STRVAL_PAIR("flag-50", FLAG_50),
    IB_STRVAL_PAIR("flag-51", FLAG_51),
    IB_STRVAL_PAIR("flag-52", FLAG_52),
    IB_STRVAL_PAIR("flag-53", FLAG_53),
    IB_STRVAL_PAIR("flag-54", FLAG_54),
    IB_STRVAL_PAIR("flag-55", FLAG_55),
    IB_STRVAL_PAIR("flag-56", FLAG_56),
    IB_STRVAL_PAIR("flag-57", FLAG_57),
    IB_STRVAL_PAIR("flag-58", FLAG_58),
    IB_STRVAL_PAIR("flag-59", FLAG_59),
    IB_STRVAL_PAIR("flag-60", FLAG_60),
    IB_STRVAL_PAIR("flag-61", FLAG_61),
    IB_STRVAL_PAIR("flag-62", FLAG_62),
    IB_STRVAL_PAIR("flag-63", FLAG_63),
    IB_STRVAL_PAIR("flag-64", FLAG_64),
    IB_STRVAL_PAIR("flag-set-01", FLAG_SET_01),
    IB_STRVAL_PAIR("flag-set-02", FLAG_SET_02),
    IB_STRVAL_PAIR("flag-set-03", FLAG_SET_03),
    IB_STRVAL_PAIR("flag-set-04", FLAG_SET_04),
    IB_STRVAL_PAIR("flag-set-05", FLAG_SET_05),
    IB_STRVAL_PAIR("flag-set-06", FLAG_SET_06),
    IB_STRVAL_PAIR("flag-set-07", FLAG_SET_07),
    IB_STRVAL_PAIR("flag-set-08", FLAG_SET_08),
    IB_STRVAL_PAIR("flag-set-09", FLAG_SET_09),
    IB_STRVAL_PAIR("flag-set-10", FLAG_SET_10),
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

    ib_flags_set(flags, FLAG_15 | FLAG_16);
    ASSERT_EQ(FLAG_04 | FLAG_05 | FLAG_15 | FLAG_16, flags);
    ib_flags_clear(flags, FLAG_15);
    ASSERT_EQ(FLAG_04 | FLAG_05 | FLAG_16, flags);

    ib_flags_set(flags, FLAG_31 | FLAG_32);
    ASSERT_EQ(FLAG_04 | FLAG_05 | FLAG_16 | FLAG_31 | FLAG_32, flags);
    ib_flags_clear(flags, FLAG_31);
    ASSERT_EQ(FLAG_04 | FLAG_05 | FLAG_16 | FLAG_32, flags);

    ib_flags_set(flags, FLAG_63 | FLAG_64);
    ASSERT_EQ(FLAG_04 | FLAG_05 | FLAG_16 | FLAG_32 | FLAG_63 | FLAG_64, flags);
    ib_flags_clear(flags, FLAG_63);
    ASSERT_EQ(FLAG_04 | FLAG_05 | FLAG_16 | FLAG_32 | FLAG_64, flags);

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

    n = 0;
    flags = 0;
    mask = 0;
    rc = ib_flags_string(flag_map, "+flag-set-03", n++, &flags, &mask);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_EQ(FLAG_SET_03, flags);
    ASSERT_EQ(FLAG_SET_03, mask);

    n = 0;
    flags = 0;
    mask = 0;
    rc = ib_flags_string(flag_map, "+flag-set-04", n++, &flags, &mask);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_EQ(FLAG_SET_04, flags);
    ASSERT_EQ(FLAG_SET_04, mask);

    n = 0;
    flags = 0;
    mask = 0;
    rc = ib_flags_string(flag_map, "+flag-set-05", n++, &flags, &mask);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_EQ(FLAG_SET_05, flags);
    ASSERT_EQ(FLAG_SET_05, mask);

    n = 0;
    flags = 0;
    mask = 0;
    rc = ib_flags_string(flag_map, "+flag-set-06", n++, &flags, &mask);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_EQ(FLAG_SET_06, flags);
    ASSERT_EQ(FLAG_SET_06, mask);

    n = 0;
    flags = 0;
    mask = 0;
    rc = ib_flags_string(flag_map, "+flag-set-07", n++, &flags, &mask);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_EQ(FLAG_SET_07, flags);
    ASSERT_EQ(FLAG_SET_07, mask);

    n = 0;
    flags = 0;
    mask = 0;
    rc = ib_flags_string(flag_map, "+flag-set-08", n++, &flags, &mask);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_EQ(FLAG_SET_08, flags);
    ASSERT_EQ(FLAG_SET_08, mask);

    n = 0;
    flags = 0;
    mask = 0;
    rc = ib_flags_string(flag_map, "+flag-set-09", n++, &flags, &mask);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_EQ(FLAG_SET_09, flags);
    ASSERT_EQ(FLAG_SET_09, mask);

    n = 0;
    flags = 0;
    mask = 0;
    rc = ib_flags_string(flag_map, "+flag-set-10", n++, &flags, &mask);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_EQ(FLAG_SET_10, flags);
    ASSERT_EQ(FLAG_SET_10, mask);
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
    rc = ib_flags_strtok(flag_map, ib_mm_mpool(mp), str, ",", &flags, &mask);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_EQ(FLAG_01|FLAG_02, flags);
    ASSERT_EQ(FLAGS_ALL, mask);

    str = "flag-set-01,+flag-10,-flag-01";
    rc = ib_flags_strtok(flag_map, ib_mm_mpool(mp), str, ",", &flags, &mask);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_EQ( ((FLAG_SET_01|FLAG_10) & (~FLAG_01)), flags);
    ASSERT_EQ(FLAGS_ALL, mask);

    flags = 0;
    mask = 0;
    str = "+flag-set-02;-flag-01;+flag-04;+flag-10";
    rc = ib_flags_strtok(flag_map, ib_mm_mpool(mp), str, ";", &flags, &mask);
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
    rc = ib_list_create(&strlist, ib_mm_mpool(mp));
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
    rc = ib_list_create(&oplist, ib_mm_mpool(mp));
    ASSERT_EQ(IB_OK, rc);

    str = "flag-01,+flag-02";
    rc = ib_flags_oplist_parse(flag_map, ib_mm_mpool(mp), str, ",", oplist);
    ASSERT_EQ(IB_OK, rc);
    rc = ib_flags_oplist_apply(oplist, &flags, &mask);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_EQ(FLAG_01|FLAG_02, flags);
    ASSERT_EQ(FLAGS_ALL, mask);

    str = "flag-set-01,+flag-10,-flag-01";
    rc = ib_flags_oplist_parse(flag_map, ib_mm_mpool(mp), str, ",", oplist);
    ASSERT_EQ(IB_OK, rc);
    rc = ib_flags_oplist_apply(oplist, &flags, &mask);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_EQ( ((FLAG_SET_01|FLAG_10) & (~FLAG_01)), flags);
    ASSERT_EQ(FLAGS_ALL, mask);

    flags = 0;
    mask = 0;
    str = "+flag-set-02;-flag-01;+flag-04;+flag-10";
    rc = ib_flags_oplist_parse(flag_map, ib_mm_mpool(mp), str, ";", oplist);
    ASSERT_EQ(IB_OK, rc);
    rc = ib_flags_oplist_apply(oplist, &flags, &mask);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_EQ( ((FLAG_SET_02 | FLAG_04 | FLAG_10) & (~FLAG_01)), flags);
    ASSERT_EQ( (FLAG_SET_02|FLAG_04|FLAG_10), mask);
}
