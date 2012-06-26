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
/// @brief IronBee &mdash; List Test Functions
/// 
/// @author Brian Rectanus <brectanus@qualys.com>
//////////////////////////////////////////////////////////////////////////////

#include "ironbee_config_auto.h"

#include <ironbee/mpool.h>
#include <ironbee/util.h>
#include <ironbee/list.h>

#include "gtest/gtest.h"
#include "gtest/gtest-spi.h"

#include <stdexcept>

class TestIBUtilList : public ::testing::Test
{
public:
    TestIBUtilList()
    {
        ib_status_t rc;
        
        ib_initialize();
        rc = ib_mpool_create(&m_pool, NULL, NULL);
        if (rc != IB_OK) {
            throw std::runtime_error("Could not create mpool.");
        }
    }
    
    ~TestIBUtilList()
    {
        ib_shutdown();        
    }
    
protected:
    ib_mpool_t* m_pool;
};

/* -- Tests -- */

/// @test Test util list library - ib_htree_list() and ib_list_destroy()
TEST_F(TestIBUtilList, test_list_create_and_destroy)
{
    ib_list_t *list;
    ib_status_t rc;
        
    rc = ib_list_create(&list, m_pool);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_TRUE(list);
    ASSERT_EQ(0UL, ib_list_elements(list));
}

/// @test Test util list library - ib_list_push() and ib_list_pop()
TEST_F(TestIBUtilList, test_list_push_and_pop)
{
    ib_list_t *list;
    ib_status_t rc;
    int v0 = 0;
    int v1 = 1;
    int v2 = 2;
    int v3 = 3;
    int v4 = 4;
    int *val;
    
    rc = ib_list_create(&list, m_pool);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_TRUE(list);
    ASSERT_EQ(0UL, ib_list_elements(list));

    /* Pop invalid. */
    rc = ib_list_pop(list,(void *)&val);
    ASSERT_EQ(IB_ENOENT, rc);
    ASSERT_FALSE(val);
    ASSERT_EQ(0UL, ib_list_elements(list));

    /* Simple pushes followed by pops. */
    rc = ib_list_push(list, &v0);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_EQ(1UL, ib_list_elements(list));
    rc = ib_list_push(list, &v1);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_EQ(2UL, ib_list_elements(list));
    rc = ib_list_push(list, &v2);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_EQ(3UL, ib_list_elements(list));
    rc = ib_list_push(list, &v3);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_EQ(4UL, ib_list_elements(list));
    rc = ib_list_push(list, &v4);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_EQ(5UL, ib_list_elements(list));
    ASSERT_EQ(v0, *(int *)(ib_list_node_data(ib_list_first(list))));
    ASSERT_EQ(v4, *(int *)(ib_list_node_data(ib_list_last(list))));
    rc = ib_list_pop(list, (void *)&val);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_TRUE(val);
    ASSERT_EQ(v4, *val);
    ASSERT_EQ(4UL, ib_list_elements(list));
    rc = ib_list_pop(list, (void *)&val);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_TRUE(val);
    ASSERT_EQ(v3, *val);
    ASSERT_EQ(3UL, ib_list_elements(list));
    rc = ib_list_pop(list, (void *)&val);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_TRUE(val);
    ASSERT_EQ(v2, *val);
    ASSERT_EQ(2UL, ib_list_elements(list));
    rc = ib_list_pop(list, (void *)&val);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_TRUE(val);
    ASSERT_EQ(v1, *val);
    ASSERT_EQ(1UL, ib_list_elements(list));
    rc = ib_list_pop(list, (void *)&val);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_TRUE(val);
    ASSERT_EQ(v0, *val);
    ASSERT_EQ(0UL, ib_list_elements(list));
}

/// @test Test util list library - ib_list_unshift() and ib_list_shift()
TEST_F(TestIBUtilList, test_list_unshift_and_shift)
{
    ib_list_t *list;
    ib_status_t rc;
    int v0 = 0;
    int v1 = 1;
    int v2 = 2;
    int v3 = 3;
    int v4 = 4;
    int *val;
    
    rc = ib_list_create(&list, m_pool);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_TRUE(list);
    ASSERT_EQ(0UL, ib_list_elements(list));

    /* shift invalid. */
    rc = ib_list_shift(list,(void *)&val);
    ASSERT_EQ(IB_ENOENT, rc);
    ASSERT_FALSE(val);
    ASSERT_EQ(0UL, ib_list_elements(list));

    /* Simple unshifts followed by shifts. */
    rc = ib_list_unshift(list, &v0);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_EQ(1UL, ib_list_elements(list));
    rc = ib_list_unshift(list, &v1);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_EQ(2UL, ib_list_elements(list));
    rc = ib_list_unshift(list, &v2);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_EQ(3UL, ib_list_elements(list));
    rc = ib_list_unshift(list, &v3);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_EQ(4UL, ib_list_elements(list));
    rc = ib_list_unshift(list, &v4);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_EQ(5UL, ib_list_elements(list));
    ASSERT_EQ(v4, *(int *)(ib_list_node_data(ib_list_first(list))));
    ASSERT_EQ(v0, *(int *)(ib_list_node_data(ib_list_last(list))));
    rc = ib_list_shift(list, (void *)&val);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_TRUE(val);
    ASSERT_EQ(v4, *val);
    ASSERT_EQ(4UL, ib_list_elements(list));
    rc = ib_list_shift(list, (void *)&val);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_TRUE(val);
    ASSERT_EQ(v3, *val);
    ASSERT_EQ(3UL, ib_list_elements(list));
    rc = ib_list_shift(list, (void *)&val);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_TRUE(val);
    ASSERT_EQ(v2, *val);
    ASSERT_EQ(2UL, ib_list_elements(list));
    rc = ib_list_shift(list, (void *)&val);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_TRUE(val);
    ASSERT_EQ(v1, *val);
    ASSERT_EQ(1UL, ib_list_elements(list));
    rc = ib_list_shift(list, (void *)&val);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_TRUE(val);
    ASSERT_EQ(v0, *val);
    ASSERT_EQ(0UL, ib_list_elements(list));
}

/// @test Test util list library - IB_LIST_LOOP()
TEST_F(TestIBUtilList, test_list_loop)
{
    ib_list_t *list;
    ib_list_node_t *node;
    ib_status_t rc;
    int init[] = { 0, 1, 2, 3, 4 };
    int *val;
    int i;
    
    rc = ib_list_create(&list, m_pool);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_TRUE(list);
    ASSERT_EQ(0UL, ib_list_elements(list));

    for (i = 0; i < 5; i++) {
        rc = ib_list_push(list, &init[i]);
        ASSERT_EQ(IB_OK, rc);
    }
    ASSERT_EQ(5UL, ib_list_elements(list));

    i = 0;
    IB_LIST_LOOP(list, node) {
        val = (int *)ib_list_node_data(node);
        ASSERT_EQ(init[i], *val);
        i++;
    }
    ASSERT_EQ(5UL, ib_list_elements(list));

    ib_mpool_destroy(m_pool);
}

/// @test Test util list library - IB_LIST_LOOP_SAFE()
TEST_F(TestIBUtilList, test_list_loop_safe)
{
    ib_list_t *list;
    ib_list_node_t *node;
    ib_list_node_t *node_next;
    ib_status_t rc;
    int init[] = { 0, 1, 2, 3, 4 };
    int *val;
    int i;
    
    rc = ib_initialize();
    ASSERT_EQ(IB_OK, rc);
    rc = ib_mpool_create(&m_pool, NULL, NULL);
    ASSERT_EQ(IB_OK, rc);
    
    rc = ib_list_create(&list, m_pool);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_TRUE(list);
    ASSERT_EQ(0UL, ib_list_elements(list));

    for (i = 0; i < 5; i++) {
        rc = ib_list_push(list, &init[i]);
        ASSERT_EQ(IB_OK, rc);
    }
    ASSERT_EQ(5UL, ib_list_elements(list));

    i = 0;
    IB_LIST_LOOP_SAFE(list, node, node_next) {
        val = (int *)ib_list_node_data(node);
        ASSERT_EQ(init[i], *val);
        i++;
    }
    ASSERT_EQ(5UL, ib_list_elements(list));
}

/// @test Test util list library - IB_LIST_LOOP_REVERSE()
TEST_F(TestIBUtilList, test_list_loop_reverse)
{
    ib_list_t *list;
    ib_list_node_t *node;
    ib_status_t rc;
    int init[] = { 0, 1, 2, 3, 4 };
    int *val;
    int i;

    rc = ib_list_create(&list, m_pool);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_TRUE(list);
    ASSERT_EQ(0UL, ib_list_elements(list));

    for (i = 0; i < 5; i++) {
        rc = ib_list_push(list, &init[i]);
        ASSERT_EQ(IB_OK, rc);
    }
    ASSERT_EQ(5UL, ib_list_elements(list));

    IB_LIST_LOOP_REVERSE(list, node) {
        --i;
        val = (int *)ib_list_node_data(node);
        ASSERT_EQ(init[i], *val);
    }
    ASSERT_EQ(5UL, ib_list_elements(list));
}

/// @test Test util list library - IB_LIST_LOOP_REVERSE_SAFE()
TEST_F(TestIBUtilList, test_list_loop_reverse_safe)
{
    ib_list_t *list;
    ib_list_node_t *node;
    ib_list_node_t *node_next;
    ib_status_t rc;
    int init[] = { 0, 1, 2, 3, 4 };
    int *val;
    int i;
    
    rc = ib_list_create(&list, m_pool);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_TRUE(list);
    ASSERT_EQ(0UL, ib_list_elements(list));

    for (i = 0; i < 5; i++) {
        rc = ib_list_push(list, &init[i]);
        ASSERT_EQ(IB_OK, rc);
    }
    ASSERT_EQ(5UL, ib_list_elements(list));

    IB_LIST_LOOP_REVERSE_SAFE(list, node, node_next) {
        i--;
        val = (int *)ib_list_node_data(node);
        ASSERT_EQ(init[i], *val);
    }
    ASSERT_EQ(5UL, ib_list_elements(list));
}
