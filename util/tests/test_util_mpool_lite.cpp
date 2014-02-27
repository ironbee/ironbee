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
 * @brief IronBee --- Memory Pool Lite Tests
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include "ironbee_config_auto.h"
#include "gtest/gtest.h"

#include <ironbee/mpool_lite.h>

#include <list>

using namespace std;

namespace {

ib_mpool_lite_t* make_mpl()
{
    ib_mpool_lite_t* mpl = NULL;
    ib_status_t rc = ib_mpool_lite_create(&mpl);
    assert(rc == IB_OK);
    assert(mpl);
    return mpl;
}

}

TEST(MpoolLiteTest, alloc)
{
    ib_mpool_lite_t* mpl = make_mpl();
    char* p = reinterpret_cast<char *>(ib_mpool_lite_alloc(mpl, 10));
    ASSERT_TRUE(p);
    /* The following is primarily for valgrind. */
    p[5] = 'a';

    /* Make a few more allocations.  Test failure would primarily show up
     * as a crash or valgrind error in teardown. */
    ASSERT_TRUE(ib_mpool_lite_alloc(mpl, 5));
    ASSERT_TRUE(ib_mpool_lite_alloc(mpl, 5));
    ASSERT_TRUE(ib_mpool_lite_alloc(mpl, 5));
    ASSERT_TRUE(ib_mpool_lite_alloc(mpl, 5));

    ib_mpool_lite_destroy(mpl);
}

TEST(MpoolLiteTest, ZeroAlloc)
{
    ib_mpool_lite_t* mpl = make_mpl();
    void* p = ib_mpool_lite_alloc(mpl, 0);
    ASSERT_TRUE(p);
    ib_mpool_lite_destroy(mpl);
}

typedef pair<list<int>*, int> cleanup_data_t;

extern "C" {

void test_cleanup(void* cbdata)
{
    cleanup_data_t* cleanup_data = reinterpret_cast<cleanup_data_t*>(cbdata);
    cleanup_data->first->push_back(cleanup_data->second);
}

}

TEST(MpoolLiteTest, cleanup)
{
    ib_mpool_lite_t* mpl = make_mpl();
    ib_status_t rc;

    list<int> cleanup_list;
    cleanup_data_t a(&cleanup_list, 1);
    cleanup_data_t b(&cleanup_list, 2);


    rc = ib_mpool_lite_register_cleanup(mpl, test_cleanup, &a);
    ASSERT_EQ(IB_OK, rc);
    rc = ib_mpool_lite_register_cleanup(mpl, test_cleanup, &b);
    ASSERT_EQ(IB_OK, rc);

    ib_mpool_lite_destroy(mpl);

    ASSERT_EQ(2UL, cleanup_list.size());
    list<int>::iterator i = cleanup_list.begin();
    EXPECT_EQ(2, *i);
    ++i;
    EXPECT_EQ(1, *i);
}
