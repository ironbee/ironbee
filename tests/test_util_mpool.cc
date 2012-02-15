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
/// @brief IronBee - Memory pool tests
///
/// @author Craig Forbes <cforbes@qualys.com>
//////////////////////////////////////////////////////////////////////////////

#include <iostream>

#include "ironbee_config_auto.h"
#include "gtest/gtest.h"
#include "base_fixture.h"

#include <ironbee/mpool.h>
#include <ironbee/debug.h>

#if defined(HAVE_VALGRIND)
#include <valgrind/memcheck.h>
#endif

// used for accessing the mpool internals
#include <util/ironbee_util_private.h>


#if defined(HAVE_VALGRIND)
static unsigned int last_leaked = 0;
static unsigned int last_reachable = 0;
static unsigned int last_suppressed = 0;
static unsigned int last_dubious = 0;
#endif

int buffer_list_size(ib_mpool_buffer_t *buf)
{
    int size =0;
    ib_mpool_buffer_t *iter;
    for (iter = buf; iter != NULL; iter = iter->next) {
        ++size;
    }
    return size;
}

void check_for_leaks()
{
#if defined(HAVE_VALGRIND)
    unsigned int leaked = 0;
    unsigned int reachable = 0;
    unsigned int suppressed = 0;
    unsigned int dubious = 0;

    VALGRIND_DO_QUICK_LEAK_CHECK;
    VALGRIND_COUNT_LEAKS(leaked, dubious, reachable, suppressed);

    EXPECT_EQ(0UL, (leaked - last_leaked)) << "Memory was leaked";
#endif
}

class MpoolTest : public ::testing::Test {
    virtual void SetUp() {
#if defined(HAVE_VALGRIND)
        VALGRIND_DO_QUICK_LEAK_CHECK;
        VALGRIND_COUNT_LEAKS(last_leaked, last_dubious, last_reachable, last_suppressed);
#endif
    }

    virtual void TearDown() {
    }
};



TEST_F(MpoolTest, CreateDestroy) {
    ib_mpool_t *pool;
    ib_status_t rc;

    rc = ib_mpool_create(&pool, "base", NULL);
    ASSERT_EQ(IB_OK, rc);

    EXPECT_EQ(IB_MPOOL_DEFAULT_PAGE_SIZE, pool->size);
    EXPECT_EQ(1UL, pool->buffer_cnt);
    EXPECT_EQ(0UL, pool->inuse);
    EXPECT_EQ(IB_MPOOL_DEFAULT_PAGE_SIZE, pool->page_size);

    ib_mpool_destroy(pool);

    check_for_leaks();
}

TEST_F(MpoolTest, SingleAlloc) {
    ib_mpool_t *pool;
    ib_status_t rc;

    rc = ib_mpool_create(&pool, "base", NULL);
    ASSERT_EQ(IB_OK, rc);

    ib_mpool_alloc(pool, 32);
    EXPECT_EQ(32UL, pool->inuse);

    ib_mpool_destroy(pool);

    check_for_leaks();
}

TEST_F(MpoolTest, TwoAllocs) {
    ib_mpool_t *pool;
    ib_status_t rc;

    rc = ib_mpool_create(&pool, "base", NULL);
    ASSERT_EQ(IB_OK, rc);

    ib_mpool_alloc(pool, 32);
    ib_mpool_alloc(pool, 32);

    EXPECT_EQ(64UL, pool->inuse);

    ib_mpool_destroy(pool);

    check_for_leaks();
}

TEST_F(MpoolTest, EngineTest) {
    ib_engine_t *ib_engine;
    ib_plugin_t ibt_ibplugin;
    ibt_ibplugin.vernum = IB_VERNUM;
    ibt_ibplugin.abinum = IB_ABINUM;
    ibt_ibplugin.version = IB_VERSION;
    ibt_ibplugin.filename = __FILE__;
    ibt_ibplugin.name = "unit_tests";

    ib_engine_create(&ib_engine, &ibt_ibplugin);
    check_for_leaks();
    ib_engine_destroy(ib_engine);
    check_for_leaks();
}

// Common main() to reset tracing and execute all tests
int main(int argc, char **argv)
{
    std::cout << "\n" << argv[0] << ":\n";
    ::testing::InitGoogleTest(&argc, argv);
    ib_trace_init(NULL);
    ib_initialize();

    return RUN_ALL_TESTS();
}
