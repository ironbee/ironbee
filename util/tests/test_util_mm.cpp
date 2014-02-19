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

/*****************************************************************************
 * @file
 * @brief IronBee --- Memory Manager tests
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
****************************************************************************/

#include "ironbee_config_auto.h"
#include "gtest/gtest.h"

#include <ironbee/mm_mpool.h>

using namespace std;

extern "C" {

static void cleanup(void* cbdata)
{
    *reinterpret_cast<bool*>(cbdata) = true;
}

static void* test_alloc(size_t size, void* cbdata)
{
    *reinterpret_cast<size_t*>(cbdata) = size;
    return cbdata;
}

static
void test_register_cleanup(
    ib_mm_cleanup_fn_t fn,
    void*              fndata,
    void*              cbdata
)
{
    *reinterpret_cast<void**>(cbdata) = fndata;
}

}

TEST(TestMM, Basic)
{
    size_t alloc_data;
    void* register_cleanup_data;

    ib_mm_t mm = {
        test_alloc, &alloc_data,
        test_register_cleanup, &register_cleanup_data
    };

    EXPECT_EQ(&alloc_data, ib_mm_alloc(mm, 100));
    EXPECT_EQ(100UL, alloc_data);
    ib_mm_register_cleanup(mm, cleanup, &alloc_data);
    EXPECT_EQ(&alloc_data, register_cleanup_data);
}

TEST(TestMM, MMMpool)
{
    ib_mpool_t* mp;
    ib_status_t rc;
    ib_mm_t mm;

    rc = ib_mpool_create(&mp, "", NULL);
    ASSERT_EQ(IB_OK, rc);
    mm = ib_mm_mpool(mp);

    void* p = ib_mm_alloc(mm, 100);
    EXPECT_TRUE(p);

    bool cleanup_data = false;
    ib_mm_register_cleanup(mm, cleanup, &cleanup_data);

    ib_mpool_destroy(mp);
    EXPECT_TRUE(cleanup_data);
}

TEST(TestMM, Helpers)
{
    ib_mpool_t* mp;
    ib_status_t rc;
    ib_mm_t mm;

    rc = ib_mpool_create(&mp, "", NULL);
    ASSERT_EQ(IB_OK, rc);
    mm = ib_mm_mpool(mp);

    {
        const size_t n = 5;
        const size_t s = 10;

        void* a = ib_mm_calloc(mm, n, s);
        void* b = calloc(n, s);

        ASSERT_TRUE(a);
        ASSERT_TRUE(b);
        EXPECT_EQ(0, memcmp(a, b, n * s));

        free(b);
    }

    {
        const char* s = "Hello World";

        void* a = ib_mm_strdup(mm, s);

        ASSERT_TRUE(a);
        EXPECT_EQ(0, memcmp(s, a, strlen(s)+1));
    }

    {
        const char* s = "Hello World";

        void* a = ib_mm_memdup(mm, const_cast<char*>(s), strlen(s));

        ASSERT_TRUE(a);
        EXPECT_EQ(0, memcmp(s, a, strlen(s)));
    }

    {
        const char* s = "Hello World";

        void* a = ib_mm_memdup_to_str(
            mm,
            const_cast<char*>(s), strlen(s)
        );

        ASSERT_TRUE(a);
        EXPECT_EQ(0, memcmp(s, a, strlen(s)+1));
    }
}
