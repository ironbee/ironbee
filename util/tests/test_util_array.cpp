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
/// @brief IronBee --- Array Test Functions
///
/// @author Brian Rectanus <brectanus@qualys.com>
//////////////////////////////////////////////////////////////////////////////

#include "ironbee_config_auto.h"

#include <ironbee/array.h>
#include <ironbee/util.h>

#include "gtest/gtest.h"
#include "simple_fixture.hpp"

#include <stdexcept>

class TestIBUtilArray : public SimpleFixture
{
};

/* -- Tests -- */

/// @test Test util array library - ib_htree_array() and ib_array_destroy()
TEST_F(TestIBUtilArray, test_array_create_and_destroy)
{
    ib_array_t *arr;
    ib_status_t rc;

    rc = ib_array_create(&arr, MM(), 10, 10);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_TRUE(arr);
    ASSERT_EQ(10UL, ib_array_size(arr));
    ASSERT_EQ(0UL, ib_array_elements(arr));
}

/// @test Test util array library - ib_htree_setn() and ib_array_get()
TEST_F(TestIBUtilArray, test_array_set_and_get)
{
    ib_array_t *arr;
    ib_status_t rc;
    int v0 = 0;
    int v9 = 9;
    int v10 = 10;
    int v99 = 99;
    int v100 = 100;
    int v1000 = 1000;
    int v1000000 = 1000000;
    int *val;

    rc = ib_array_create(&arr, MM(), 10, 10);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_TRUE(arr);
    ASSERT_EQ(10UL, ib_array_size(arr));
    ASSERT_EQ(0UL, ib_array_elements(arr));

    /* Get invalid. */
    rc = ib_array_get(arr, 10, &val);
    ASSERT_EQ(IB_ENOENT, rc);
    ASSERT_FALSE(val);
    ASSERT_EQ(10UL, ib_array_size(arr));
    ASSERT_EQ(0UL, ib_array_elements(arr));

    /* Simple set. */
    rc = ib_array_setn(arr, 0, &v0);
    ASSERT_EQ(IB_OK, rc);
    rc = ib_array_get(arr, 0, &val);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_TRUE(val);
    ASSERT_EQ(v0, *val);
    ASSERT_EQ(10UL, ib_array_size(arr));
    ASSERT_EQ(1UL, ib_array_elements(arr));

    /* Should not extend. */
    rc = ib_array_setn(arr, 9, &v9);
    ASSERT_EQ(IB_OK, rc);
    rc = ib_array_get(arr, 9, &val);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_TRUE(val);
    ASSERT_EQ(v9, *val);
    ASSERT_EQ(10UL, ib_array_size(arr));
    ASSERT_EQ(10UL, ib_array_elements(arr));

    /* Should be null if unset. */
    rc = ib_array_get(arr, 5, &val);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_FALSE(val);
    ASSERT_EQ(10UL, ib_array_size(arr));
    ASSERT_EQ(10UL, ib_array_elements(arr));

    /* Should extend once. */
    rc = ib_array_setn(arr, 10, &v10);
    ASSERT_EQ(IB_OK, rc);
    rc = ib_array_get(arr, 10, &val);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_TRUE(val);
    ASSERT_EQ(v10, *val);
    ASSERT_EQ(20UL, ib_array_size(arr));
    ASSERT_EQ(11UL, ib_array_elements(arr));

    /* Should extend to max. */
    rc = ib_array_setn(arr, 99, &v99);
    ASSERT_EQ(IB_OK, rc);
    rc = ib_array_get(arr, 99, &val);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_TRUE(val);
    ASSERT_EQ(v99, *val);
    ASSERT_EQ(100UL, ib_array_size(arr));
    ASSERT_EQ(100UL, ib_array_elements(arr));

    /* Should reallocate extents. */
    rc = ib_array_setn(arr, 100, &v100);
    ASSERT_EQ(IB_OK, rc);
    rc = ib_array_get(arr, 100, &val);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_TRUE(val);
    ASSERT_EQ(v100, *val);
    ASSERT_EQ(110UL, ib_array_size(arr));
    ASSERT_EQ(101UL, ib_array_elements(arr));

    /* Should reallocate extents 2 more times. */
    rc = ib_array_setn(arr, 1000, &v1000);
    ASSERT_EQ(IB_OK, rc);
    rc = ib_array_get(arr, 1000, &val);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_TRUE(val);
    ASSERT_EQ(v1000, *val);
    ASSERT_EQ(1010UL, ib_array_size(arr));
    ASSERT_EQ(1001UL, ib_array_elements(arr));

    /* Should reallocate extents many more times. */
    rc = ib_array_setn(arr, 1000000, &v1000000);
    ASSERT_EQ(IB_OK, rc);
    rc = ib_array_get(arr, 1000000, &val);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_TRUE(val);
    ASSERT_EQ(v1000000, *val);
    ASSERT_EQ(1000010UL, ib_array_size(arr));
    ASSERT_EQ(1000001UL, ib_array_elements(arr));
}

/// @test Test util array library - IB_ARRAY_LOOP()
TEST_F(TestIBUtilArray, test_array_loop)
{
    ib_array_t *arr;
    size_t nelts;
    size_t i;
    int *val;
    ib_status_t rc;
    int init[20] = {
         0,  1,  2,  3,  4,  5,  6,  7,  8,  9,
        10, 11, 12, 13, 14, 15, 16, 17, 18, 19
    };
    const size_t count = (sizeof(init)/sizeof(int));
    size_t prev;

    rc = ib_array_create(&arr, MM(), 16, 8);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_TRUE(arr);
    ASSERT_EQ(16UL, ib_array_size(arr));
    ASSERT_EQ(0UL, ib_array_elements(arr));

    for (i = 0; i < count; i++) {
        rc = ib_array_setn(arr, i, init + i);
        ASSERT_EQ(IB_OK, rc);
    }
    ASSERT_EQ(32UL, ib_array_size(arr));
    ASSERT_EQ(20UL, ib_array_elements(arr));
    rc = ib_array_get(arr, 1, &val);
    ASSERT_EQ(IB_OK, rc);

    prev = -1;
    IB_ARRAY_LOOP(arr, nelts, i, val) {
        ASSERT_EQ(i, prev+1);
        prev = i;
        ASSERT_EQ(init[i], *val);
    }

    prev = count;
    IB_ARRAY_LOOP_REVERSE(arr, nelts, i, val) {
        ASSERT_EQ(i, prev-1);
        prev = i;
        ASSERT_EQ(init[i], *val);
    }
}
