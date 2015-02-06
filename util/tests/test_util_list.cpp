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
/// @brief IronBee --- List Test Functions
///
/// @author Brian Rectanus <brectanus@qualys.com>
//////////////////////////////////////////////////////////////////////////////

#include "ironbee_config_auto.h"

#include <ironbee/mm_mpool.h>
#include <ironbee/util.h>
#include <ironbee/list.h>

#include "gtest/gtest.h"
#include "simple_fixture.hpp"

#include <stdexcept>


class TestIBUtilList : public SimpleFixture
{

public:
    /* Populate a list from an array of integers */
    static void populate_list(ib_list_t *list,
                              const int *ints,
                              size_t count)
    {
        size_t i;
        ib_status_t rc;
        for (i = 0; i < count; i++) {
            rc = ib_list_push(list, (int *)&ints[i]);
            ASSERT_EQ(IB_OK, rc);
        }
        ASSERT_EQ(count, ib_list_elements(list));
    }

    /* Check a list against an array of integers */
    static void check_list(const ib_list_t *list,
                           const int *ints,
                           size_t count)
    {
        const ib_list_node_t *node;
        const int *val;
        size_t i = 0;

        ASSERT_EQ(count, ib_list_elements(list));
        IB_LIST_LOOP_CONST(list, node) {
            val = (const int *)ib_list_node_data_const(node);
            ASSERT_EQ(ints[i], *val);
            i++;
        }
    }

};

/* -- Tests -- */

/// @test Test util list library - ib_htree_list() and ib_list_destroy()
TEST_F(TestIBUtilList, test_list_create_and_destroy)
{
    ib_list_t *list;
    ib_status_t rc;

    rc = ib_list_create(&list, MM());
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

    rc = ib_list_create(&list, MM());
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

/// @test Test util list library - IB_LIST_REMOVE()
TEST_F(TestIBUtilList, test_list_remove_head)
{
    ib_list_t *list;
    ib_list_node_t *node;
    ib_status_t rc;
    int init[] = { 0, 1, 2, 3, 4 };
    int *val;

    rc = ib_list_create(&list, MM());
    ASSERT_EQ(IB_OK, rc);
    ASSERT_TRUE(list);
    ASSERT_EQ(0UL, ib_list_elements(list));

    populate_list(list, init, 5);

    node = ib_list_first(list);
    val = (int *)ib_list_node_data(node);
    ASSERT_EQ(0, *val);
    ib_list_node_remove(list, node);
    ASSERT_EQ(4UL, ib_list_elements(list));

    node = ib_list_first(list);
    val = (int *)ib_list_node_data(node);
    ASSERT_EQ(1, *val);
    ib_list_node_remove(list, node);
    ASSERT_EQ(3UL, ib_list_elements(list));

    node = ib_list_first(list);
    val = (int *)ib_list_node_data(node);
    ASSERT_EQ(2, *val);
    ib_list_node_remove(list, node);
    ASSERT_EQ(2UL, ib_list_elements(list));

    node = ib_list_first(list);
    val = (int *)ib_list_node_data(node);
    ASSERT_EQ(3, *val);
    ib_list_node_remove(list, node);
    ASSERT_EQ(1UL, ib_list_elements(list));

    node = ib_list_first(list);
    val = (int *)ib_list_node_data(node);
    ASSERT_EQ(4, *val);
    ib_list_node_remove(list, node);
    ASSERT_EQ(0UL, ib_list_elements(list));

}
/// @test Test util list library - IB_LIST_REMOVE()
TEST_F(TestIBUtilList, test_list_remove_tail)
{
    ib_list_t *list;
    ib_list_node_t *node;
    ib_status_t rc;
    int init[] = { 0, 1, 2, 3, 4 };
    int *val;

    rc = ib_list_create(&list, MM());
    ASSERT_EQ(IB_OK, rc);
    ASSERT_TRUE(list);
    ASSERT_EQ(0UL, ib_list_elements(list));

    populate_list(list, init, 5);

    node = ib_list_last(list);
    val = (int *)ib_list_node_data(node);
    ASSERT_EQ(4, *val);
    ib_list_node_remove(list, node);
    ASSERT_EQ(4UL, ib_list_elements(list));

    node = ib_list_last(list);
    val = (int *)ib_list_node_data(node);
    ASSERT_EQ(3, *val);
    ib_list_node_remove(list, node);
    ASSERT_EQ(3UL, ib_list_elements(list));

    node = ib_list_last(list);
    val = (int *)ib_list_node_data(node);
    ASSERT_EQ(2, *val);
    ib_list_node_remove(list, node);
    ASSERT_EQ(2UL, ib_list_elements(list));

    node = ib_list_last(list);
    val = (int *)ib_list_node_data(node);
    ASSERT_EQ(1, *val);
    ib_list_node_remove(list, node);
    ASSERT_EQ(1UL, ib_list_elements(list));

    node = ib_list_last(list);
    val = (int *)ib_list_node_data(node);
    ASSERT_EQ(0, *val);
    ib_list_node_remove(list, node);
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

    rc = ib_list_create(&list, MM());
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
    ib_status_t rc;
    int init[] = { 0, 1, 2, 3, 4 };

    rc = ib_list_create(&list, MM());
    ASSERT_EQ(IB_OK, rc);
    ASSERT_TRUE(list);
    ASSERT_EQ(0UL, ib_list_elements(list));

    populate_list(list, init, 5);
    check_list(list, init, 5);
}

/// @test Test util list library - ib_list_copy_nodes
TEST_F(TestIBUtilList, test_list_copy_nodes)
{
    ib_list_t *list1;
    ib_list_t *list2;
    ib_status_t rc;
    int init[] = { 0, 1, 2, 3, 4 };

    rc = ib_list_create(&list1, MM());
    ASSERT_EQ(IB_OK, rc);
    ASSERT_TRUE(list1 != NULL);
    ASSERT_EQ(0UL, ib_list_elements(list1));

    populate_list(list1, init, 5);
    check_list(list1, init, 5);

    rc = ib_list_create(&list2, MM());
    ASSERT_EQ(IB_OK, rc);
    ASSERT_TRUE(list2 != NULL);

    rc = ib_list_copy_nodes(list1, list2);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_TRUE(list2 != NULL);

    check_list(list2, init, 5);
}

/// @test Test util list library - ib_list_copy
TEST_F(TestIBUtilList, test_list_copy)
{
    ib_list_t *list1;
    ib_list_t *list2;
    ib_status_t rc;
    int init[] = { 0, 1, 2, 3, 4 };

    rc = ib_list_create(&list1, MM());
    ASSERT_EQ(IB_OK, rc);
    ASSERT_TRUE(list1 != NULL);
    ASSERT_EQ(0UL, ib_list_elements(list1));

    populate_list(list1, init, 5);
    check_list(list1, init, 5);

    rc = ib_list_copy(list1, MM(), &list2);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_TRUE(list2 != NULL);

    check_list(list2, init, 5);
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

    rc = ib_util_initialize();
    ASSERT_EQ(IB_OK, rc);

    rc = ib_list_create(&list, MM());
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

    rc = ib_list_create(&list, MM());
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

    rc = ib_list_create(&list, MM());
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

/// @test Test util list library - IB_LIST_REMOVE() from loop
TEST_F(TestIBUtilList, test_list_loop_remove)
{
    ib_list_t *list;
    ib_list_node_t *node;
    ib_list_node_t *node_next;
    ib_status_t rc;
    int init[] = { 0, 1, 2, 3, 4 };
    int *val;
    int i;

    rc = ib_list_create(&list, MM());
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
        ++i;
        ib_list_node_remove(list, node);
    }
    ASSERT_EQ(0UL, ib_list_elements(list));
}


/// @test Test util list library - IB_LIST_REMOVE() from loop reverse
TEST_F(TestIBUtilList, test_list_loop_reverse_remove)
{
    ib_list_t *list;
    ib_list_node_t *node;
    ib_list_node_t *node_next;
    ib_status_t rc;
    int init[] = { 0, 1, 2, 3, 4 };
    int *val;
    int i;

    rc = ib_list_create(&list, MM());
    ASSERT_EQ(IB_OK, rc);
    ASSERT_TRUE(list);
    ASSERT_EQ(0UL, ib_list_elements(list));

    for (i = 0; i < 5; i++) {
        rc = ib_list_push(list, &init[i]);
        ASSERT_EQ(IB_OK, rc);
    }
    ASSERT_EQ(5UL, ib_list_elements(list));

    i = 4;
    IB_LIST_LOOP_REVERSE_SAFE(list, node, node_next) {
        val = (int *)ib_list_node_data(node);
        ASSERT_EQ(init[i], *val);
        --i;
        ib_list_node_remove(list, node);
    }
    ASSERT_EQ(0UL, ib_list_elements(list));
}

TEST_F(TestIBUtilList, test_insert) {
    ib_list_t      *list;
    void           *p;

    int i = 1, j = 2, k = 3;

    ASSERT_EQ(IB_OK, ib_list_create(&list, MM()));

    ASSERT_EQ(IB_OK, ib_list_insert(list, &i, 0));
    ASSERT_EQ(IB_OK, ib_list_insert(list, &k, 1));
    ASSERT_EQ(IB_OK, ib_list_insert(list, &j, 1));

    ASSERT_EQ(3, ib_list_elements(list));

    ASSERT_EQ(IB_OK, ib_list_shift(list, &p));
    ASSERT_EQ(&i, p) << "i expected";

    ASSERT_EQ(IB_OK, ib_list_shift(list, &p));
    ASSERT_EQ(&j, p) << "j expected";

    ASSERT_EQ(IB_OK, ib_list_shift(list, &p));
    ASSERT_EQ(&k, p) << "k expected";

}