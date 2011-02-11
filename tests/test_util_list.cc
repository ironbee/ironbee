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
/// @brief IronBee - List Test Functions
/// 
/// @author Brian Rectanus <brectanus@qualys.com>
//////////////////////////////////////////////////////////////////////////////

#include "ironbee_config_auto.h"

#include "gtest/gtest.h"
#include "gtest/gtest-spi.h"

#define TESTING

#include "util/util.c"
#include "util/list.c"
#include "util/mpool.c"
#include "util/debug.c"


/* -- Tests -- */

/// @test Test util list library - ib_htree_list() and ib_list_destroy()
TEST(TestIBUtilList, test_list_create_and_destroy)
{
    ib_mpool_t *mp;
    ib_list_t *list;
    ib_status_t rc;
    
    atexit(ib_shutdown);
    rc = ib_initialize();
    ASSERT_TRUE(rc == IB_OK) << "ib_initialize() failed - rc != IB_OK";
    rc = ib_mpool_create(&mp, NULL);
    ASSERT_TRUE(rc == IB_OK) << "ib_mpool_create() failed - rc != IB_OK";
    
    rc = ib_list_create(&list, mp);
    ASSERT_TRUE(rc == IB_OK) << "ib_list_create() failed - rc != IB_OK";
    ASSERT_TRUE(list != NULL) << "ib_list_create() failed - NULL value";
    ASSERT_TRUE(ib_list_elements(list) == 0) << "ib_list_create() failed - wrong number of elements";

    ib_mpool_destroy(mp);
}

/// @test Test util list library - ib_list_push() and ib_list_pop()
TEST(TestIBUtilList, test_list_push_and_pop)
{
    ib_mpool_t *mp;
    ib_list_t *list;
    ib_status_t rc;
    int v0 = 0;
    int v1 = 1;
    int v2 = 2;
    int v3 = 3;
    int v4 = 4;
    int *val;
    
    atexit(ib_shutdown);
    rc = ib_initialize();
    ASSERT_TRUE(rc == IB_OK) << "ib_initialize() failed - rc != IB_OK";
    rc = ib_mpool_create(&mp, NULL);
    ASSERT_TRUE(rc == IB_OK) << "ib_mpool_create() failed - rc != IB_OK";
    
    rc = ib_list_create(&list, mp);
    ASSERT_TRUE(rc == IB_OK) << "ib_list_create() failed - rc != IB_OK";
    ASSERT_TRUE(list != NULL) << "ib_list_create() failed - NULL value";
    ASSERT_TRUE(ib_list_elements(list) == 0) << "ib_list_create() failed - wrong number of elements";

    /* Pop invalid. */
    rc = ib_list_pop(list,(void *)&val);
    ASSERT_TRUE(rc == IB_ENOENT) << "ib_list_pop() failed - rc != IB_INVAL";
    ASSERT_TRUE(val == NULL) << "ib_list_pop() failed - not NULL value";
    ASSERT_TRUE(ib_list_elements(list) == 0) << "ib_list_create() failed - wrong number of elements";

    /* Simple pushes followed by pops. */
    rc = ib_list_push(list, &v0);
    ASSERT_TRUE(rc == IB_OK) << "ib_list_push() failed - rc != IB_INVAL";
    ASSERT_TRUE(ib_list_elements(list) == 1) << "ib_list_push() failed - wrong number of elements";
    rc = ib_list_push(list, &v1);
    ASSERT_TRUE(rc == IB_OK) << "ib_list_push() failed - rc != IB_INVAL";
    ASSERT_TRUE(ib_list_elements(list) == 2) << "ib_list_push() failed - wrong number of elements";
    rc = ib_list_push(list, &v2);
    ASSERT_TRUE(rc == IB_OK) << "ib_list_push() failed - rc != IB_INVAL";
    ASSERT_TRUE(ib_list_elements(list) == 3) << "ib_list_push() failed - wrong number of elements";
    rc = ib_list_push(list, &v3);
    ASSERT_TRUE(rc == IB_OK) << "ib_list_push() failed - rc != IB_INVAL";
    ASSERT_TRUE(ib_list_elements(list) == 4) << "ib_list_push() failed - wrong number of elements";
    rc = ib_list_push(list, &v4);
    ASSERT_TRUE(rc == IB_OK) << "ib_list_push() failed - rc != IB_INVAL";
    ASSERT_TRUE(ib_list_elements(list) == 5) << "ib_list_push() failed - wrong number of elements";
    ASSERT_TRUE(*(int *)(ib_list_node_data(ib_list_first(list))) == v0) << "ib_list_push() failed - wrong first element";
    ASSERT_TRUE(*(int *)(ib_list_node_data(ib_list_last(list))) == v4) << "ib_list_push() failed - wrong last element";
    rc = ib_list_pop(list, (void *)&val);
    ASSERT_TRUE(rc == IB_OK) << "ib_list_pop() failed - rc != IB_INVAL";
    ASSERT_TRUE(val != NULL) << "ib_list_pop() failed - NULL value";
    ASSERT_TRUE(*val == v4) << "ib_list_pop() failed - wrong value";
    ASSERT_TRUE(ib_list_elements(list) == 4) << "ib_list_pop() failed - wrong number of elements";
    rc = ib_list_pop(list, (void *)&val);
    ASSERT_TRUE(rc == IB_OK) << "ib_list_pop() failed - rc != IB_INVAL";
    ASSERT_TRUE(val != NULL) << "ib_list_pop() failed - NULL value";
    ASSERT_TRUE(*val == v3) << "ib_list_pop() failed - wrong value";
    ASSERT_TRUE(ib_list_elements(list) == 3) << "ib_list_pop() failed - wrong number of elements";
    rc = ib_list_pop(list, (void *)&val);
    ASSERT_TRUE(rc == IB_OK) << "ib_list_pop() failed - rc != IB_INVAL";
    ASSERT_TRUE(val != NULL) << "ib_list_pop() failed - NULL value";
    ASSERT_TRUE(*val == v2) << "ib_list_pop() failed - wrong value";
    ASSERT_TRUE(ib_list_elements(list) == 2) << "ib_list_pop() failed - wrong number of elements";
    rc = ib_list_pop(list, (void *)&val);
    ASSERT_TRUE(rc == IB_OK) << "ib_list_pop() failed - rc != IB_INVAL";
    ASSERT_TRUE(val != NULL) << "ib_list_pop() failed - NULL value";
    ASSERT_TRUE(*val == v1) << "ib_list_pop() failed - wrong value";
    ASSERT_TRUE(ib_list_elements(list) == 1) << "ib_list_pop() failed - wrong number of elements";
    rc = ib_list_pop(list, (void *)&val);
    ASSERT_TRUE(rc == IB_OK) << "ib_list_pop() failed - rc != IB_INVAL";
    ASSERT_TRUE(val != NULL) << "ib_list_pop() failed - NULL value";
    ASSERT_TRUE(*val == v0) << "ib_list_pop() failed - wrong value";
    ASSERT_TRUE(ib_list_elements(list) == 0) << "ib_list_pop() failed - wrong number of elements";

    ib_mpool_destroy(mp);
}

/// @test Test util list library - ib_list_unshift() and ib_list_shift()
TEST(TestIBUtilList, test_list_unshift_and_shift)
{
    ib_mpool_t *mp;
    ib_list_t *list;
    ib_status_t rc;
    int v0 = 0;
    int v1 = 1;
    int v2 = 2;
    int v3 = 3;
    int v4 = 4;
    int *val;
    
    atexit(ib_shutdown);
    rc = ib_initialize();
    ASSERT_TRUE(rc == IB_OK) << "ib_initialize() failed - rc != IB_OK";
    rc = ib_mpool_create(&mp, NULL);
    ASSERT_TRUE(rc == IB_OK) << "ib_mpool_create() failed - rc != IB_OK";
    
    rc = ib_list_create(&list, mp);
    ASSERT_TRUE(rc == IB_OK) << "ib_list_create() failed - rc != IB_OK";
    ASSERT_TRUE(list != NULL) << "ib_list_create() failed - NULL value";
    ASSERT_TRUE(ib_list_elements(list) == 0) << "ib_list_create() failed - wrong number of elements";

    /* shift invalid. */
    rc = ib_list_shift(list,(void *)&val);
    ASSERT_TRUE(rc == IB_ENOENT) << "ib_list_shift() failed - rc != IB_INVAL";
    ASSERT_TRUE(val == NULL) << "ib_list_shift() failed - not NULL value";
    ASSERT_TRUE(ib_list_elements(list) == 0) << "ib_list_create() failed - wrong number of elements";

    /* Simple unshiftes followed by shifts. */
    rc = ib_list_unshift(list, &v0);
    ASSERT_TRUE(rc == IB_OK) << "ib_list_unshift() failed - rc != IB_INVAL";
    ASSERT_TRUE(ib_list_elements(list) == 1) << "ib_list_unshift() failed - wrong number of elements";
    rc = ib_list_unshift(list, &v1);
    ASSERT_TRUE(rc == IB_OK) << "ib_list_unshift() failed - rc != IB_INVAL";
    ASSERT_TRUE(ib_list_elements(list) == 2) << "ib_list_unshift() failed - wrong number of elements";
    rc = ib_list_unshift(list, &v2);
    ASSERT_TRUE(rc == IB_OK) << "ib_list_unshift() failed - rc != IB_INVAL";
    ASSERT_TRUE(ib_list_elements(list) == 3) << "ib_list_unshift() failed - wrong number of elements";
    rc = ib_list_unshift(list, &v3);
    ASSERT_TRUE(rc == IB_OK) << "ib_list_unshift() failed - rc != IB_INVAL";
    ASSERT_TRUE(ib_list_elements(list) == 4) << "ib_list_unshift() failed - wrong number of elements";
    rc = ib_list_unshift(list, &v4);
    ASSERT_TRUE(rc == IB_OK) << "ib_list_unshift() failed - rc != IB_INVAL";
    ASSERT_TRUE(ib_list_elements(list) == 5) << "ib_list_unshift() failed - wrong number of elements";
    ASSERT_TRUE(*(int *)(ib_list_node_data(ib_list_first(list))) == v4) << "ib_list_unshift() failed - wrong first element";
    ASSERT_TRUE(*(int *)(ib_list_node_data(ib_list_last(list))) == v0) << "ib_list_unshift() failed - wrong last element";
    rc = ib_list_shift(list, (void *)&val);
    ASSERT_TRUE(rc == IB_OK) << "ib_list_shift() failed - rc != IB_INVAL";
    ASSERT_TRUE(val != NULL) << "ib_list_shift() failed - NULL value";
    ASSERT_TRUE(*val == v4) << "ib_list_shift() failed - wrong value";
    ASSERT_TRUE(ib_list_elements(list) == 4) << "ib_list_shift() failed - wrong number of elements";
    rc = ib_list_shift(list, (void *)&val);
    ASSERT_TRUE(rc == IB_OK) << "ib_list_shift() failed - rc != IB_INVAL";
    ASSERT_TRUE(val != NULL) << "ib_list_shift() failed - NULL value";
    ASSERT_TRUE(*val == v3) << "ib_list_shift() failed - wrong value";
    ASSERT_TRUE(ib_list_elements(list) == 3) << "ib_list_shift() failed - wrong number of elements";
    rc = ib_list_shift(list, (void *)&val);
    ASSERT_TRUE(rc == IB_OK) << "ib_list_shift() failed - rc != IB_INVAL";
    ASSERT_TRUE(val != NULL) << "ib_list_shift() failed - NULL value";
    ASSERT_TRUE(*val == v2) << "ib_list_shift() failed - wrong value";
    ASSERT_TRUE(ib_list_elements(list) == 2) << "ib_list_shift() failed - wrong number of elements";
    rc = ib_list_shift(list, (void *)&val);
    ASSERT_TRUE(rc == IB_OK) << "ib_list_shift() failed - rc != IB_INVAL";
    ASSERT_TRUE(val != NULL) << "ib_list_shift() failed - NULL value";
    ASSERT_TRUE(*val == v1) << "ib_list_shift() failed - wrong value";
    ASSERT_TRUE(ib_list_elements(list) == 1) << "ib_list_shift() failed - wrong number of elements";
    rc = ib_list_shift(list, (void *)&val);
    ASSERT_TRUE(rc == IB_OK) << "ib_list_shift() failed - rc != IB_INVAL";
    ASSERT_TRUE(val != NULL) << "ib_list_shift() failed - NULL value";
    ASSERT_TRUE(*val == v0) << "ib_list_shift() failed - wrong value";
    ASSERT_TRUE(ib_list_elements(list) == 0) << "ib_list_shift() failed - wrong number of elements";

    ib_mpool_destroy(mp);
}

/// @test Test util list library - IB_LIST_LOOP()
TEST(TestIBUtilList, test_list_loop)
{
    ib_mpool_t *mp;
    ib_list_t *list;
    ib_list_node_t *node;
    ib_status_t rc;
    int init[] = { 0, 1, 2, 3, 4 };
    int *val;
    int i;
    
    atexit(ib_shutdown);
    rc = ib_initialize();
    ASSERT_TRUE(rc == IB_OK) << "ib_initialize() failed - rc != IB_OK";
    rc = ib_mpool_create(&mp, NULL);
    ASSERT_TRUE(rc == IB_OK) << "ib_mpool_create() failed - rc != IB_OK";
    
    rc = ib_list_create(&list, mp);
    ASSERT_TRUE(rc == IB_OK) << "ib_list_create() failed - rc != IB_OK";
    ASSERT_TRUE(list != NULL) << "ib_list_create() failed - NULL value";
    ASSERT_TRUE(ib_list_elements(list) == 0) << "ib_list_create() failed - wrong number of elements";

    for (i = 0; i < 5; i++) {
        rc = ib_list_push(list, &init[i]);
        ASSERT_TRUE(rc == IB_OK) << "ib_list_push() failed - rc != IB_INVAL";
    }
    ASSERT_TRUE(ib_list_elements(list) == 5) << "ib_list_push() failed - wrong number of elements";

    i = 0;
    IB_LIST_LOOP(list, node) {
        val = (int *)ib_list_node_data(node);
        ASSERT_TRUE(*val == init[i]) << "IB_LIST_LOOP() failed - wrong value";
        i++;
    }
    ASSERT_TRUE(ib_list_elements(list) == 5) << "IB_LIST_LOOP() failed - wrong number of elements";

    ib_mpool_destroy(mp);
}

/// @test Test util list library - IB_LIST_LOOP_SAFE()
TEST(TestIBUtilList, test_list_loop_safe)
{
    ib_mpool_t *mp;
    ib_list_t *list;
    ib_list_node_t *node;
    ib_list_node_t *node_next;
    ib_status_t rc;
    int init[] = { 0, 1, 2, 3, 4 };
    int *val;
    int i;
    
    atexit(ib_shutdown);
    rc = ib_initialize();
    ASSERT_TRUE(rc == IB_OK) << "ib_initialize() failed - rc != IB_OK";
    rc = ib_mpool_create(&mp, NULL);
    ASSERT_TRUE(rc == IB_OK) << "ib_mpool_create() failed - rc != IB_OK";
    
    rc = ib_list_create(&list, mp);
    ASSERT_TRUE(rc == IB_OK) << "ib_list_create() failed - rc != IB_OK";
    ASSERT_TRUE(list != NULL) << "ib_list_create() failed - NULL value";
    ASSERT_TRUE(ib_list_elements(list) == 0) << "ib_list_create() failed - wrong number of elements";

    for (i = 0; i < 5; i++) {
        rc = ib_list_push(list, &init[i]);
        ASSERT_TRUE(rc == IB_OK) << "ib_list_push() failed - rc != IB_INVAL";
    }
    ASSERT_TRUE(ib_list_elements(list) == 5) << "ib_list_push() failed - wrong number of elements";

    i = 0;
    IB_LIST_LOOP_SAFE(list, node, node_next) {
        val = (int *)ib_list_node_data(node);
        ASSERT_TRUE(*val == init[i]) << "IB_LIST_LOOP_SAFE() failed - wrong value";
        i++;
    }
    ASSERT_TRUE(ib_list_elements(list) == 5) << "IB_LIST_LOOP_SAFE() failed - wrong number of elements";

    ib_mpool_destroy(mp);
}

/// @test Test util list library - IB_LIST_LOOP_REVERSE()
TEST(TestIBUtilList, test_list_loop_reverse)
{
    ib_mpool_t *mp;
    ib_list_t *list;
    ib_list_node_t *node;
    ib_status_t rc;
    int init[] = { 0, 1, 2, 3, 4 };
    int *val;
    int i;
    
    atexit(ib_shutdown);
    rc = ib_initialize();
    ASSERT_TRUE(rc == IB_OK) << "ib_initialize() failed - rc != IB_OK";
    rc = ib_mpool_create(&mp, NULL);
    ASSERT_TRUE(rc == IB_OK) << "ib_mpool_create() failed - rc != IB_OK";
    
    rc = ib_list_create(&list, mp);
    ASSERT_TRUE(rc == IB_OK) << "ib_list_create() failed - rc != IB_OK";
    ASSERT_TRUE(list != NULL) << "ib_list_create() failed - NULL value";
    ASSERT_TRUE(ib_list_elements(list) == 0) << "ib_list_create() failed - wrong number of elements";

    for (i = 0; i < 5; i++) {
        rc = ib_list_push(list, &init[i]);
        ASSERT_TRUE(rc == IB_OK) << "ib_list_push() failed - rc != IB_INVAL";
    }
    ASSERT_TRUE(ib_list_elements(list) == 5) << "ib_list_push() failed - wrong number of elements";

    IB_LIST_LOOP_REVERSE(list, node) {
        i--;
        val = (int *)ib_list_node_data(node);
        ASSERT_TRUE(*val == init[i]) << "IB_LIST_LOOP_REVERSE() failed - wrong value";
    }
    ASSERT_TRUE(ib_list_elements(list) == 5) << "IB_LIST_LOOP_REVERSE() failed - wrong number of elements";

    ib_mpool_destroy(mp);
}

/// @test Test util list library - IB_LIST_LOOP_REVERSE_SAFE()
TEST(TestIBUtilList, test_list_loop_reverse_safe)
{
    ib_mpool_t *mp;
    ib_list_t *list;
    ib_list_node_t *node;
    ib_list_node_t *node_next;
    ib_status_t rc;
    int init[] = { 0, 1, 2, 3, 4 };
    int *val;
    int i;
    
    atexit(ib_shutdown);
    rc = ib_initialize();
    ASSERT_TRUE(rc == IB_OK) << "ib_initialize() failed - rc != IB_OK";
    rc = ib_mpool_create(&mp, NULL);
    ASSERT_TRUE(rc == IB_OK) << "ib_mpool_create() failed - rc != IB_OK";
    
    rc = ib_list_create(&list, mp);
    ASSERT_TRUE(rc == IB_OK) << "ib_list_create() failed - rc != IB_OK";
    ASSERT_TRUE(list != NULL) << "ib_list_create() failed - NULL value";
    ASSERT_TRUE(ib_list_elements(list) == 0) << "ib_list_create() failed - wrong number of elements";

    for (i = 0; i < 5; i++) {
        rc = ib_list_push(list, &init[i]);
        ASSERT_TRUE(rc == IB_OK) << "ib_list_push() failed - rc != IB_INVAL";
    }
    ASSERT_TRUE(ib_list_elements(list) == 5) << "ib_list_push() failed - wrong number of elements";

    IB_LIST_LOOP_REVERSE_SAFE(list, node, node_next) {
        i--;
        val = (int *)ib_list_node_data(node);
        ASSERT_TRUE(*val == init[i]) << "IB_LIST_LOOP_REVERSE_SAFE() failed - wrong value";
    }
    ASSERT_TRUE(ib_list_elements(list) == 5) << "IB_LIST_LOOP_REVERSE_SAFE() failed - wrong number of elements";

    ib_mpool_destroy(mp);
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    ib_trace_init(NULL);
    return RUN_ALL_TESTS();
}
