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
/// @brief IronBee - Array Test Functions
/// 
/// @author Brian Rectanus <brectanus@qualys.com>
//////////////////////////////////////////////////////////////////////////////

#include "ironbee_config_auto.h"

#include "gtest/gtest.h"
#include "gtest/gtest-spi.h"

#define TESTING

#include "util/util.c"
#include "util/array.c"
#include "util/mpool.c"
#include "util/debug.c"


/* -- Tests -- */

/// @test Test util array library - ib_htree_array() and ib_array_destroy()
TEST(TestIBUtilArray, test_array_create_and_destroy)
{
    ib_mpool_t *mp;
    ib_array_t *arr;
    ib_status_t rc;

    atexit(ib_shutdown);
    rc = ib_initialize();
    ASSERT_TRUE(rc == IB_OK) << "ib_initialize() failed - rc != IB_OK";
    rc = ib_mpool_create(&mp, NULL);
    ASSERT_TRUE(rc == IB_OK) << "ib_mpool_create() failed - rc != IB_OK";
    
    rc = ib_array_create(&arr, mp, 10, 10);
    ASSERT_TRUE(rc == IB_OK) << "ib_array_create() failed - rc != IB_OK";
    ASSERT_TRUE(arr != NULL) << "ib_array_create() failed - NULL value";
    ASSERT_TRUE(ib_array_size(arr) == 10) << "ib_array_create() failed - wrong size";
    ASSERT_TRUE(ib_array_elements(arr) == 0) << "ib_array_create() failed - wrong number of elements";

    ib_mpool_destroy(mp);
}

/// @test Test util array library - ib_htree_setn() and ib_array_get()
TEST(TestIBUtilArray, test_array_set_and_get)
{
    ib_mpool_t *mp;
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
    
    atexit(ib_shutdown);
    rc = ib_initialize();
    ASSERT_TRUE(rc == IB_OK) << "ib_initialize() failed - rc != IB_OK";
    rc = ib_mpool_create(&mp, NULL);
    ASSERT_TRUE(rc == IB_OK) << "ib_mpool_create() failed - rc != IB_OK";
    
    rc = ib_array_create(&arr, mp, 10, 10);
    ASSERT_TRUE(rc == IB_OK) << "ib_array_create() failed - rc != IB_OK";
    ASSERT_TRUE(arr != NULL) << "ib_array_create() failed - NULL value";
    ASSERT_TRUE(ib_array_size(arr) == 10) << "ib_array_create() failed - wrong size";
    ASSERT_TRUE(ib_array_elements(arr) == 0) << "ib_array_create() failed - wrong number of elements";

    /* Get invalid. */
    rc = ib_array_get(arr, 10, &val);
    ASSERT_TRUE(rc == IB_EINVAL) << "ib_array_get() failed - rc != IB_INVAL";
    ASSERT_TRUE(val == NULL) << "ib_array_get() failed - not NULL value";
    ASSERT_TRUE(ib_array_size(arr) == 10) << "ib_array_create() failed - wrong size";
    ASSERT_TRUE(ib_array_elements(arr) == 0) << "ib_array_create() failed - wrong number of elements";

    /* Simple set. */
    rc = ib_array_setn(arr, 0, &v0);
    ASSERT_TRUE(rc == IB_OK) << "ib_array_get() failed - rc != IB_INVAL";
    rc = ib_array_get(arr, 0, &val);
    ASSERT_TRUE(rc == IB_OK) << "ib_array_get() failed - rc != IB_INVAL";
    ASSERT_TRUE(val != NULL) << "ib_array_get() failed - NULL value";
    ASSERT_TRUE(*val == v0) << "ib_array_get() failed - wrong value";
    ASSERT_TRUE(ib_array_size(arr) == 10) << "ib_array_create() failed - wrong size";
    ASSERT_TRUE(ib_array_elements(arr) == 1) << "ib_array_create() failed - wrong number of elements";

    /* Should not extend. */
    rc = ib_array_setn(arr, 9, &v9);
    ASSERT_TRUE(rc == IB_OK) << "ib_array_get() failed - rc != IB_INVAL";
    rc = ib_array_get(arr, 9, &val);
    ASSERT_TRUE(rc == IB_OK) << "ib_array_get() failed - rc != IB_INVAL";
    ASSERT_TRUE(val != NULL) << "ib_array_get() failed - NULL value";
    ASSERT_TRUE(*val == v9) << "ib_array_get() failed - wrong value";
    ASSERT_TRUE(ib_array_size(arr) == 10) << "ib_array_create() failed - wrong size";
    ASSERT_TRUE(ib_array_elements(arr) == 10) << "ib_array_create() failed - wrong number of elements";

    /* Should be null if unset. */
    rc = ib_array_get(arr, 5, &val);
    ASSERT_TRUE(rc == IB_OK) << "ib_array_get() failed - rc != IB_INVAL";
    ASSERT_TRUE(val == NULL) << "ib_array_get() failed - not NULL value";
    ASSERT_TRUE(ib_array_size(arr) == 10) << "ib_array_create() failed - wrong size";
    ASSERT_TRUE(ib_array_elements(arr) == 10) << "ib_array_create() failed - wrong number of elements";

    /* Should extend once. */
    rc = ib_array_setn(arr, 10, &v10);
    ASSERT_TRUE(rc == IB_OK) << "ib_array_get() failed - rc != IB_INVAL";
    rc = ib_array_get(arr, 10, &val);
    ASSERT_TRUE(rc == IB_OK) << "ib_array_get() failed - rc != IB_INVAL";
    ASSERT_TRUE(val != NULL) << "ib_array_get() failed - NULL value";
    ASSERT_TRUE(*val == v10) << "ib_array_get() failed - wrong value";
    ASSERT_TRUE(ib_array_size(arr) == 20) << "ib_array_create() failed - wrong size";
    ASSERT_TRUE(ib_array_elements(arr) == 11) << "ib_array_create() failed - wrong number of elements";

    /* Should extend to max. */
    rc = ib_array_setn(arr, 99, &v99);
    ASSERT_TRUE(rc == IB_OK) << "ib_array_get() failed - rc != IB_INVAL";
    rc = ib_array_get(arr, 99, &val);
    ASSERT_TRUE(rc == IB_OK) << "ib_array_get() failed - rc != IB_INVAL";
    ASSERT_TRUE(val != NULL) << "ib_array_get() failed - NULL value";
    ASSERT_TRUE(*val == v99) << "ib_array_get() failed - wrong value";
    ASSERT_TRUE(ib_array_size(arr) == 100) << "ib_array_create() failed - wrong size";
    ASSERT_TRUE(ib_array_elements(arr) == 100) << "ib_array_create() failed - wrong number of elements";

    /* Should reallocate extents. */
    rc = ib_array_setn(arr, 100, &v100);
    ASSERT_TRUE(rc == IB_OK) << "ib_array_get() failed - rc != IB_INVAL";
    rc = ib_array_get(arr, 100, &val);
    ASSERT_TRUE(rc == IB_OK) << "ib_array_get() failed - rc != IB_INVAL";
    ASSERT_TRUE(val != NULL) << "ib_array_get() failed - NULL value";
    ASSERT_TRUE(*val == v100) << "ib_array_get() failed - wrong value";
    ASSERT_TRUE(ib_array_size(arr) == 110) << "ib_array_create() failed - wrong size";
    ASSERT_TRUE(ib_array_elements(arr) == 101) << "ib_array_create() failed - wrong number of elements";

    /* Should reallocate extents 2 more times. */
    rc = ib_array_setn(arr, 1000, &v1000);
    ASSERT_TRUE(rc == IB_OK) << "ib_array_get() failed - rc != IB_INVAL";
    rc = ib_array_get(arr, 1000, &val);
    ASSERT_TRUE(rc == IB_OK) << "ib_array_get() failed - rc != IB_INVAL";
    ASSERT_TRUE(val != NULL) << "ib_array_get() failed - NULL value";
    ASSERT_TRUE(*val == v1000) << "ib_array_get() failed - wrong value";
    ASSERT_TRUE(ib_array_size(arr) == 1010) << "ib_array_create() failed - wrong size";
    ASSERT_TRUE(ib_array_elements(arr) == 1001) << "ib_array_create() failed - wrong number of elements";

    /* Should reallocate extents many more times. */
    rc = ib_array_setn(arr, 1000000, &v1000000);
    ASSERT_TRUE(rc == IB_OK) << "ib_array_get() failed - rc != IB_INVAL";
    rc = ib_array_get(arr, 1000000, &val);
    ASSERT_TRUE(rc == IB_OK) << "ib_array_get() failed - rc != IB_INVAL";
    ASSERT_TRUE(val != NULL) << "ib_array_get() failed - NULL value";
    ASSERT_TRUE(*val == v1000000) << "ib_array_get() failed - wrong value";
    ASSERT_TRUE(ib_array_size(arr) == 1000010) << "ib_array_create() failed - wrong size";
    ASSERT_TRUE(ib_array_elements(arr) == 1000001) << "ib_array_create() failed - wrong number of elements";

    ib_mpool_destroy(mp);
}

/// @test Test util array library - IB_ARRAY_LOOP()
TEST(TestIBUtilArray, test_array_loop)
{
    ib_mpool_t *mp;
    ib_array_t *arr;
    size_t nelts;
    size_t i;
    int *val;
    ib_status_t rc;
    int init[20] = {
         0,  1,  2,  3,  4,  5,  6,  7,  8,  9,
        10, 11, 12, 13, 14, 15, 16, 17, 18, 19
    };
    
    atexit(ib_shutdown);
    rc = ib_initialize();
    ASSERT_TRUE(rc == IB_OK) << "ib_initialize() failed - rc != IB_OK";
    rc = ib_mpool_create(&mp, NULL);
    ASSERT_TRUE(rc == IB_OK) << "ib_mpool_create() failed - rc != IB_OK";
    
    rc = ib_array_create(&arr, mp, 16, 8);
    ASSERT_TRUE(rc == IB_OK) << "ib_array_create() failed - rc != IB_OK";
    ASSERT_TRUE(arr != NULL) << "ib_array_create() failed - NULL value";
    ASSERT_TRUE(ib_array_size(arr) == 16) << "ib_array_create() failed - wrong size";
    ASSERT_TRUE(ib_array_elements(arr) == 0) << "ib_array_create() failed - wrong number of elements";

    for (i = 0; i < (sizeof(init)/sizeof(int)); i++) {
        rc = ib_array_setn(arr, i, init + i);
        ASSERT_TRUE(rc == IB_OK) << "ib_array_setn() failed - rc != IB_OK";
    }
    ASSERT_TRUE(ib_array_size(arr) == 32) << "ib_array_setn() failed - wrong size";
    ASSERT_TRUE(ib_array_elements(arr) == 20) << "ib_array_setn() failed - wrong number of elements";
    rc = ib_array_get(arr, 1, &val);
    ASSERT_TRUE(rc == IB_OK) << "ib_array_get() failed - rc != IB_OK";

    IB_ARRAY_LOOP(arr, nelts, i, val) {
        //ASSERT_TRUE(*val == init[i]) << "IB_ARRAY_LOOP() failed - wrong value at index=" << (int)i "/" << (int)(nelts - 1);
        ASSERT_TRUE(*val == init[i]) << "IB_ARRAY_LOOP() failed - wrong value";
    }

    ib_mpool_destroy(mp);
}
