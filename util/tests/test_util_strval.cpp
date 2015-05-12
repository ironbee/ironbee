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

#include <ironbee/strval.h>

#include "ironbee_config_auto.h"

#include "gtest/gtest.h"

#include <ironbee/list.h>
#include <ironbee/mm.h>
#include <ironbee/string.h>
#include <ironbee/types.h>
#include <ironbee/type_convert.h>

#include <stdexcept>

typedef enum {
    VALUE_01,
    VALUE_02,
    VALUE_03,
    VALUE_04,
    VALUE_05,
    VALUE_06,
    VALUE_07,
    VALUE_08,
    VALUE_09,
    VALUE_10,
    VALUE_11,
    VALUE_12,
    VALUE_13,
    VALUE_14,
    VALUE_15,
    VALUE_16,
    VALUE_17,
    VALUE_18,
    VALUE_19,
    VALUE_MAX=VALUE_19
} test_values_t;
#define NUM_VALUES (VALUE_MAX + 1)

static IB_STRVAL_MAP(value_map) = {
    IB_STRVAL_PAIR("value-01", VALUE_01),
    IB_STRVAL_PAIR("value-02", VALUE_02),
    IB_STRVAL_PAIR("value-03", VALUE_03),
    IB_STRVAL_PAIR("value-04", VALUE_04),
    IB_STRVAL_PAIR("value-05", VALUE_05),
    IB_STRVAL_PAIR("value-06", VALUE_06),
    IB_STRVAL_PAIR("value-07", VALUE_07),
    IB_STRVAL_PAIR("value-08", VALUE_08),
    IB_STRVAL_PAIR("value-09", VALUE_09),
    IB_STRVAL_PAIR("value-10", VALUE_10),
    IB_STRVAL_PAIR("value-11", VALUE_11),
    IB_STRVAL_PAIR("value-12", VALUE_12),
    IB_STRVAL_PAIR("value-13", VALUE_13),
    IB_STRVAL_PAIR("value-14", VALUE_14),
    IB_STRVAL_PAIR("value-15", VALUE_15),
    IB_STRVAL_PAIR("value-16", VALUE_16),
    IB_STRVAL_PAIR("value-17", VALUE_17),
    IB_STRVAL_PAIR("value-18", VALUE_18),
    IB_STRVAL_PAIR("value-19", VALUE_19),
    IB_STRVAL_PAIR_LAST
};

TEST(TestStrVal, test_lookup)
{
    uint64_t   value;

    ASSERT_EQ(IB_ENOENT, ib_strval_lookup(value_map, "value-00", &value));

    ASSERT_EQ(IB_OK, ib_strval_lookup(value_map, "value-01", &value));
    ASSERT_EQ(VALUE_01, value);

    ASSERT_EQ(IB_OK, ib_strval_lookup(value_map, "value-02", &value));
    ASSERT_EQ(VALUE_02, value);

    ASSERT_EQ(IB_OK, ib_strval_lookup(value_map, "value-10", &value));
    ASSERT_EQ(VALUE_10, value);

    ASSERT_EQ(IB_OK, ib_strval_lookup(value_map, "value-19", &value));
    ASSERT_EQ(VALUE_19, value);

    ASSERT_EQ(IB_EINVAL, ib_strval_lookup(NULL, "value-00", &value));
    ASSERT_EQ(IB_EINVAL, ib_strval_lookup(value_map, NULL, &value));
    ASSERT_EQ(IB_EINVAL, ib_strval_lookup(value_map, "value-19", NULL));
}

TEST(TestStrVal, test_loop)
{
    size_t             count = 0;
    bool               found[NUM_VALUES];
    const ib_strval_t *rec;

    memset(found, 0, sizeof(found));
    IB_STRVAL_LOOP(value_map, rec) {
        ++count;
        ASSERT_TRUE(rec->val <= VALUE_MAX);
        found[rec->val] = true;
    }

    ASSERT_EQ((size_t)NUM_VALUES, count);
    for(size_t i = 0;  i < NUM_VALUES;  ++i) {
        ASSERT_TRUE(found[i]);
    }
}

static IB_STRVAL_PTR_MAP(ptr_map) = {
    IB_STRVAL_PAIR("value-01", "01"),
    IB_STRVAL_PAIR("value-02", "02"),
    IB_STRVAL_PAIR("value-03", "03"),
    IB_STRVAL_PAIR("value-04", "04"),
    IB_STRVAL_PAIR("value-05", "05"),
    IB_STRVAL_PAIR("value-06", "06"),
    IB_STRVAL_PAIR("value-07", "07"),
    IB_STRVAL_PAIR("value-08", "08"),
    IB_STRVAL_PAIR("value-09", "09"),
    IB_STRVAL_PAIR("value-10", "10"),
    IB_STRVAL_PAIR("value-11", "11"),
    IB_STRVAL_PAIR("value-12", "12"),
    IB_STRVAL_PAIR("value-13", "13"),
    IB_STRVAL_PAIR("value-14", "14"),
    IB_STRVAL_PAIR("value-15", "15"),
    IB_STRVAL_PAIR("value-16", "16"),
    IB_STRVAL_PAIR("value-17", "17"),
    IB_STRVAL_PAIR("value-18", "18"),
    IB_STRVAL_PAIR("value-19", "19"),
    IB_STRVAL_PAIR_LAST
};

TEST(TestStrVal, test_ptr_lookup)
{
    const void *value;

    ASSERT_EQ(IB_ENOENT, ib_strval_ptr_lookup(ptr_map, "value-00", &value));

    ASSERT_EQ(IB_OK, ib_strval_ptr_lookup(ptr_map, "value-01", &value));
    ASSERT_STREQ("01", (const char *)value);

    ASSERT_EQ(IB_OK, ib_strval_ptr_lookup(ptr_map, "value-02", &value));
    ASSERT_STREQ("02", (const char *)value);

    ASSERT_EQ(IB_OK, ib_strval_ptr_lookup(ptr_map, "value-10", &value));
    ASSERT_STREQ("10", (const char *)value);

    ASSERT_EQ(IB_OK, ib_strval_ptr_lookup(ptr_map, "value-19", &value));
    ASSERT_STREQ("19", (const char *)value);

    ASSERT_EQ(IB_EINVAL, ib_strval_ptr_lookup(NULL, "value-00", &value));
    ASSERT_EQ(IB_EINVAL, ib_strval_ptr_lookup(ptr_map, NULL, &value));
    ASSERT_EQ(IB_EINVAL, ib_strval_ptr_lookup(ptr_map, "value-19", NULL));
}

TEST(TestStrVal, test_ptr_loop)
{
    size_t                 count = 0;
    const ib_strval_ptr_t *rec;
    bool                   found[NUM_VALUES];

    memset(found, 0, sizeof(found));
    IB_STRVAL_LOOP(ptr_map, rec) {
        ib_status_t rc;
        ib_num_t    num;

        ++count;
        rc = ib_type_atoi( (const char *)(rec->val), 10, &num);
        found[num-1] = true;
        ASSERT_EQ(IB_OK, rc);
    }

    ASSERT_EQ((size_t)NUM_VALUES, count);
    for(size_t i = 0;  i < NUM_VALUES;  ++i) {
        ASSERT_TRUE(found[i]);
    }
}

typedef struct {
    test_values_t      value;
    const char        *str;
} test_data_t;
typedef struct {
    const char        *str;
    const test_data_t  data;
} test_strval_data_t;
static IB_STRVAL_DATA_MAP(test_strval_data_t, data_map) = {
    IB_STRVAL_DATA_PAIR("value-01", VALUE_01, "01"),
    IB_STRVAL_DATA_PAIR("value-02", VALUE_02, "02"),
    IB_STRVAL_DATA_PAIR("value-03", VALUE_03, "03"),
    IB_STRVAL_DATA_PAIR("value-04", VALUE_04, "04"),
    IB_STRVAL_DATA_PAIR("value-05", VALUE_05, "05"),
    IB_STRVAL_DATA_PAIR("value-06", VALUE_06, "06"),
    IB_STRVAL_DATA_PAIR("value-07", VALUE_07, "07"),
    IB_STRVAL_DATA_PAIR("value-08", VALUE_08, "08"),
    IB_STRVAL_DATA_PAIR("value-09", VALUE_09, "09"),
    IB_STRVAL_DATA_PAIR("value-10", VALUE_10, "10"),
    IB_STRVAL_DATA_PAIR("value-11", VALUE_11, "11"),
    IB_STRVAL_DATA_PAIR("value-12", VALUE_12, "12"),
    IB_STRVAL_DATA_PAIR("value-13", VALUE_13, "13"),
    IB_STRVAL_DATA_PAIR("value-14", VALUE_14, "14"),
    IB_STRVAL_DATA_PAIR("value-15", VALUE_15, "15"),
    IB_STRVAL_DATA_PAIR("value-16", VALUE_16, "16"),
    IB_STRVAL_DATA_PAIR("value-17", VALUE_17, "17"),
    IB_STRVAL_DATA_PAIR("value-18", VALUE_18, "18"),
    IB_STRVAL_DATA_PAIR("value-19", VALUE_19, "19"),
    IB_STRVAL_DATA_PAIR_LAST((test_values_t)0, NULL)
};

TEST(TestStrVal, test_data_lookup)
{
    ib_status_t        rc;
    const test_data_t *value = NULL;

    rc = IB_STRVAL_DATA_LOOKUP(data_map, test_strval_data_t,
                               "value-00", &value);
    ASSERT_EQ(IB_ENOENT, rc);

    rc = IB_STRVAL_DATA_LOOKUP(data_map, test_strval_data_t,
                               "value-01", &value);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_EQ(VALUE_01, value->value);
    ASSERT_STREQ("01", value->str);

    rc = IB_STRVAL_DATA_LOOKUP(data_map, test_strval_data_t,
                               "value-02", &value);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_EQ(VALUE_02, value->value);
    ASSERT_STREQ("02", value->str);

    rc = IB_STRVAL_DATA_LOOKUP(data_map, test_strval_data_t,
                               "value-10", &value);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_EQ(VALUE_10, value->value);
    ASSERT_STREQ("10", value->str);

    rc = IB_STRVAL_DATA_LOOKUP(data_map, test_strval_data_t,
                               "value-19", &value);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_EQ(VALUE_19, value->value);
    ASSERT_STREQ("19", value->str);
}

TEST(TestStrVal, test_data_loop)
{
    size_t                    count = 0;
    const test_strval_data_t *rec;
    bool                      found[NUM_VALUES];

    memset(found, 0, sizeof(found));

    IB_STRVAL_LOOP(data_map, rec) {

        ++count;
        found[rec->data.value] = true;
    }

    ASSERT_EQ((size_t)NUM_VALUES, count);
    for(size_t i = 0;  i < NUM_VALUES;  ++i) {
        ASSERT_TRUE(found[i]);
    }
}
