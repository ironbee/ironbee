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
/// @brief IronBee &mdash; Memory pool tests
///
/// @author Craig Forbes <cforbes@qualys.com>
//////////////////////////////////////////////////////////////////////////////

#include <iostream>

#include "ironbee_config_auto.h"
#include "gtest/gtest.h"

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdelete-non-virtual-dtor"
#endif
#include <boost/random.hpp>
#include <boost/thread.hpp>
#ifdef __clang__
#pragma clang diagnostic pop
#endif

#include <ironbee/mpool.h>

using namespace std;

extern "C" {
    
static size_t g_malloc_calls;
static size_t g_malloc_bytes;
static size_t g_free_calls;
static size_t g_free_bytes;

struct test_memory_t
{
    size_t size;
    char first_byte;
};

static
void *test_malloc(size_t size)
{
    ++g_malloc_calls;
    g_malloc_bytes += size;
    
    test_memory_t *mem = (test_memory_t *)malloc(size + sizeof(size_t));
    mem->size = size;
    
    return &(mem->first_byte);
}

static
void test_free(void *p)
{
    char *cp = (char *)p;
    test_memory_t *mem = (test_memory_t *)(cp - sizeof(size_t));
    
    ++g_free_calls;
    g_free_bytes += mem->size;
    
    free(mem);
}

static
void reset_test()
{
    g_malloc_calls = 0;
    g_malloc_bytes = 0;
    g_free_calls   = 0;
    g_free_bytes   = 0;
}

}

#define EXPECT_VALID(mp) \
    do { \
        char* av_message; \
        EXPECT_EQ(IB_OK, ib_mpool_validate(mp, &av_message)) << av_message; \
    } while (0)

TEST(TestMpool, Basic)
{
    ib_mpool_t* mp = NULL;
    ib_status_t rc = ib_mpool_create(&mp, NULL, NULL);
    
    ASSERT_EQ(IB_OK, rc);
    ASSERT_TRUE(mp);
    
    void* p = ib_mpool_alloc(mp, 100);
    
    EXPECT_TRUE(p);
    
    ib_mpool_destroy(mp);
}

TEST(TestMpool, CreateDestroy)
{
    reset_test();
    
    ib_mpool_t* mp = NULL;
    ib_status_t rc = 
        ib_mpool_create_ex(&mp, "create_destroy", NULL, 0, 
            &test_malloc, &test_free);
    EXPECT_VALID(mp);
    
    ASSERT_EQ(IB_OK, rc);
    ASSERT_TRUE(mp);
    EXPECT_LT(0U, g_malloc_calls);
    EXPECT_LT(0U, g_malloc_bytes);
    
    void* p = ib_mpool_alloc(mp, 100);
    EXPECT_VALID(mp);
    
    EXPECT_TRUE(p);
    
    ib_mpool_destroy(mp);

    ASSERT_EQ(g_malloc_calls, g_free_calls);
    ASSERT_EQ(g_malloc_bytes, g_free_bytes);
}

TEST(TestMpool, OneThousandAllocs)
{
    static const size_t c_max_size = 1048;
    static const size_t c_num_allocs = 1e3;
    
    boost::random::mt19937 rng;
    boost::random::uniform_int_distribution<size_t> g(1, c_max_size);
    
    reset_test();
    
    ib_mpool_t* mp = NULL;
    ib_status_t rc = 
        ib_mpool_create_ex(&mp, "create_destroy", NULL, 0, 
            &test_malloc, &test_free);
    EXPECT_VALID(mp);
    
    ASSERT_EQ(IB_OK, rc);
    ASSERT_TRUE(mp);

    for (size_t i = 0; i < c_num_allocs; ++i) {
        void* p = ib_mpool_alloc(mp, g(rng));
        EXPECT_TRUE(p);
        EXPECT_VALID(mp);
    }

    EXPECT_LT(0U, g_malloc_bytes);
    EXPECT_LT(0U, g_malloc_calls);
    EXPECT_EQ(0U, g_free_bytes);
    EXPECT_EQ(0U, g_free_calls);
        
    ib_mpool_destroy(mp);

    ASSERT_EQ(g_malloc_calls, g_free_calls);
    ASSERT_EQ(g_malloc_bytes, g_free_bytes);
}

TEST(TestMpool, Clear)
{
    reset_test();
    
    ib_mpool_t* mp = NULL;
    ib_status_t rc = 
        ib_mpool_create_ex(&mp, "create_destroy", NULL, 0, 
            &test_malloc, &test_free);
    EXPECT_VALID(mp);
    
    ASSERT_EQ(IB_OK, rc);
    ASSERT_TRUE(mp);
    EXPECT_LT(0U, g_malloc_calls);
    EXPECT_LT(0U, g_malloc_bytes);
    
    for (int i = 1; i <= 1000; ++i) {
        void* p = ib_mpool_alloc(mp, i);
        EXPECT_TRUE(p);
        EXPECT_VALID(mp);
    }

    EXPECT_EQ(500U*1001U, ib_mpool_inuse(mp));
    ib_mpool_clear(mp);
    EXPECT_EQ(0U, ib_mpool_inuse(mp));
    EXPECT_EQ(0U, g_free_calls);
    EXPECT_EQ(0U, g_free_bytes);
    
    ib_mpool_destroy(mp);

    ASSERT_EQ(g_malloc_calls, g_free_calls);
    ASSERT_EQ(g_malloc_bytes, g_free_bytes);  
}

namespace {

void test_mpool_helper(ib_mpool_t* parent, size_t remaining_depth)
{
    ib_status_t rc;
    ib_mpool_t* a = NULL;
    ib_mpool_t* b = NULL;
    void *p = NULL;
    string parent_name(ib_mpool_name(parent));
    
    rc = ib_mpool_create_ex(
        &a,
        (parent_name + ".a").c_str(),
        parent,
        0,
        &test_malloc, &test_free
    );
    ASSERT_EQ(IB_OK, rc);
    ASSERT_TRUE(a);
    
    p = NULL;
    p = ib_mpool_alloc(a, 100);
    EXPECT_TRUE(p);
    
    rc = ib_mpool_create_ex(
        &b,
        (parent_name + ".b").c_str(),
        parent,
        0,
        &test_malloc, &test_free
    );
    ASSERT_EQ(IB_OK, rc);
    ASSERT_TRUE(b);
    
    p = NULL;
    p = ib_mpool_alloc(b, 100);
    EXPECT_TRUE(p);
    
    if (remaining_depth > 0) {
        test_mpool_helper(a, remaining_depth - 1);
        test_mpool_helper(b, remaining_depth - 1);
    }
}

}

TEST(TestMpool, ChildrenDeep)
{
    ib_mpool_t* top = NULL;
    ib_status_t rc;
    
    reset_test();
    rc = ib_mpool_create_ex(
        &top, "children_deep", NULL, 0, 
        &test_malloc, &test_free
    );
    EXPECT_EQ(IB_OK, rc);
    EXPECT_VALID(top);

    test_mpool_helper(top, 5);
    
    EXPECT_VALID(top);
    
    ASSERT_LT(0U, g_malloc_calls);
    ASSERT_LT(0U, g_malloc_bytes);
    ASSERT_EQ(0U, g_free_calls);
    ASSERT_EQ(0U, g_free_bytes);
    
    ib_mpool_destroy(top);
    
    ASSERT_EQ(g_malloc_calls, g_free_calls);
    ASSERT_EQ(g_malloc_bytes, g_free_bytes);  
}

TEST(TestMpool, ChildrenWide)
{
    ib_mpool_t* top = NULL;
    ib_status_t rc;
    
    reset_test();
    rc = ib_mpool_create_ex(
        &top, "children", NULL, 0, 
        &test_malloc, &test_free
    );
    EXPECT_VALID(top);

    for (int i = 0; i < 1000; ++i) {
        ib_mpool_t* child;
        rc = ib_mpool_create_ex(
            &child, 
            ("children_wide." + boost::lexical_cast<string>(i)).c_str(),
            top,
            0,
            &test_malloc, &test_free
        );
        
        ASSERT_EQ(IB_OK, rc);
        ASSERT_TRUE(child);
    }
    
    EXPECT_VALID(top);
    
    ASSERT_LT(0U, g_malloc_calls);
    ASSERT_LT(0U, g_malloc_bytes);
    ASSERT_EQ(0U, g_free_calls);
    ASSERT_EQ(0U, g_free_bytes);
    
    ib_mpool_destroy(top);
    
    ASSERT_EQ(g_malloc_calls, g_free_calls);
    ASSERT_EQ(g_malloc_bytes, g_free_bytes);  
}

TEST(TestMpool, SetName) 
{
    ib_mpool_t* mp = NULL;
    ib_status_t rc = ib_mpool_create(&mp, NULL, NULL);
    
    ASSERT_EQ(IB_OK, rc);
    ASSERT_TRUE(mp);
    
    EXPECT_FALSE(ib_mpool_name(mp));

    static const char* new_name = "hello";
    
    EXPECT_EQ(IB_OK, ib_mpool_setname(mp, new_name));
    EXPECT_EQ("hello", string(ib_mpool_name(mp)));
    EXPECT_NE(new_name, ib_mpool_name(mp));

    static const char* new_new_name = "foobar";
    
    EXPECT_EQ(IB_OK, ib_mpool_setname(mp, new_new_name));
    EXPECT_EQ("foobar", string(ib_mpool_name(mp)));
    EXPECT_NE(new_new_name, ib_mpool_name(mp));

    ib_mpool_destroy(mp);    
}

TEST(TestMpool, StrangePagesize)
{
    for (int i = 0; i < 2048; ++i) {
        ib_mpool_t* mp;
        ib_status_t rc = ib_mpool_create_ex(&mp, NULL, NULL, i, NULL, NULL);
        
        EXPECT_VALID(mp);
        EXPECT_EQ(IB_OK, rc) << "Failed to create for size " << i;
        
        void* p = ib_mpool_alloc(mp, 100);
        EXPECT_VALID(mp);
        
        EXPECT_TRUE(p) << "Failed to allocate for size " << i;
        
        ib_mpool_destroy(mp);
    }
}

TEST(TestMpool, calloc)
{
    ib_mpool_t* mp = NULL;
    ib_status_t rc = ib_mpool_create(&mp, NULL, NULL);
    
    ASSERT_EQ(IB_OK, rc);
    ASSERT_TRUE(mp);
    
    int* p = reinterpret_cast<int *>(ib_mpool_calloc(mp, 100, sizeof(int)));
    
    EXPECT_TRUE(p);
    
    for (int i = 0; i < 100; ++i) {
        EXPECT_EQ(0, p[i]);
    }
    
    ib_mpool_destroy(mp);
}

TEST(TestMpool, strdup)
{
    static const char* s = "Hello World";
    ib_mpool_t* mp = NULL;
    ib_status_t rc = ib_mpool_create(&mp, NULL, NULL);
    
    ASSERT_EQ(IB_OK, rc);
    ASSERT_TRUE(mp);
    
    char* s2 = ib_mpool_strdup(mp, s);

    ASSERT_TRUE(s2);
    EXPECT_EQ(string(s), string(s2));
    EXPECT_NE(s, s2);
    
    ib_mpool_destroy(mp);    
}

TEST(TestMpool, memdup)
{
    static const int numbers[] = {1, 2, 3, 4};

    ib_mpool_t* mp = NULL;
    ib_status_t rc = ib_mpool_create(&mp, NULL, NULL);
    
    ASSERT_EQ(IB_OK, rc);
    ASSERT_TRUE(mp);
    
    int* numbers2 = reinterpret_cast<int *>(
        ib_mpool_memdup(mp, numbers, sizeof(numbers))
    );

    ASSERT_TRUE(numbers2);
    EXPECT_EQ(numbers2[0], numbers[0]);
    EXPECT_EQ(numbers2[1], numbers[1]);
    EXPECT_EQ(numbers2[2], numbers[2]);
    EXPECT_EQ(numbers2[3], numbers[3]);
    EXPECT_NE(numbers, numbers2);
    
    ib_mpool_destroy(mp);    
}

TEST(TestMpool, memdup_to_str)
{
    static const char* s = "Hello World";
    ib_mpool_t* mp = NULL;
    ib_status_t rc = ib_mpool_create(&mp, NULL, NULL);
    
    ASSERT_EQ(IB_OK, rc);
    ASSERT_TRUE(mp);
    
    char* s2 = ib_mpool_memdup_to_str(mp, s, 5);

    EXPECT_TRUE(s2);
    EXPECT_EQ(string("Hello"), string(s2));
    EXPECT_NE(s, s2);
    
    char* s3 = ib_mpool_memdup_to_str(mp, s, 0);
    EXPECT_TRUE(s3);
    EXPECT_EQ(string(""), s3);
    
    ib_mpool_destroy(mp);       
}

extern "C" {

static 
void test_cleanup(void* p)
{
    int* i = reinterpret_cast<int *>(p);
    *i = 0;
}

}

TEST(TestMpool, TestCleanupDestroy)
{
    reset_test();

    ib_mpool_t* mp = NULL;
    ib_status_t rc = 
        ib_mpool_create_ex(&mp, "cleanup_destroy", NULL, 0, 
            &test_malloc, &test_free);

    
    ASSERT_EQ(IB_OK, rc);
    ASSERT_TRUE(mp);
    
    int a = 1;
    int b = 1;
    int c = 1;
    int d = 1;
    
    rc = ib_mpool_cleanup_register(mp, test_cleanup, &a);
    EXPECT_EQ(IB_OK, rc);
    rc = ib_mpool_cleanup_register(mp, test_cleanup, &b);
    EXPECT_EQ(IB_OK, rc);
    rc = ib_mpool_cleanup_register(mp, test_cleanup, &c);
    EXPECT_EQ(IB_OK, rc);
    rc = ib_mpool_cleanup_register(mp, test_cleanup, &d);
    EXPECT_EQ(IB_OK, rc);
    
    EXPECT_VALID(mp);
    
    ib_mpool_destroy(mp);
    
    ASSERT_EQ(g_malloc_calls, g_free_calls);
    ASSERT_EQ(g_malloc_bytes, g_free_bytes);  
    
    EXPECT_EQ(0, a);
    EXPECT_EQ(0, b);
    EXPECT_EQ(0, c);
    EXPECT_EQ(0, d);
}

TEST(TestMpool, TestCleanupClear)
{
    reset_test();

    ib_mpool_t* mp = NULL;
    ib_status_t rc = 
        ib_mpool_create_ex(&mp, "cleanup_clear", NULL, 0, 
            &test_malloc, &test_free);

    
    ASSERT_EQ(IB_OK, rc);
    ASSERT_TRUE(mp);
    
    int a = 1;
    int b = 1;
    int c = 1;
    int d = 1;
    
    rc = ib_mpool_cleanup_register(mp, test_cleanup, &a);
    EXPECT_EQ(IB_OK, rc);
    rc = ib_mpool_cleanup_register(mp, test_cleanup, &b);
    EXPECT_EQ(IB_OK, rc);
    rc = ib_mpool_cleanup_register(mp, test_cleanup, &c);
    EXPECT_EQ(IB_OK, rc);
    rc = ib_mpool_cleanup_register(mp, test_cleanup, &d);
    EXPECT_EQ(IB_OK, rc);
    
    EXPECT_VALID(mp);
    
    ib_mpool_clear(mp);
    
    EXPECT_VALID(mp);

    EXPECT_EQ(0, a);
    EXPECT_EQ(0, b);
    EXPECT_EQ(0, c);
    EXPECT_EQ(0, d);

    ib_mpool_destroy(mp);
    
    ASSERT_EQ(g_malloc_calls, g_free_calls);
    ASSERT_EQ(g_malloc_bytes, g_free_bytes);  
}

TEST(TestMpool, TestEINVAL)
{
    ib_mpool_t* mp;

    ASSERT_EQ(IB_OK, ib_mpool_create(&mp, NULL, NULL));
    ASSERT_TRUE(mp);
    
    EXPECT_EQ(IB_EINVAL, ib_mpool_create(NULL, NULL, NULL));
    EXPECT_EQ(IB_EINVAL, ib_mpool_create_ex(NULL, NULL, NULL, 0, NULL, NULL));
    EXPECT_EQ(IB_EINVAL, ib_mpool_setname(NULL, NULL));
    EXPECT_EQ(IB_EINVAL, ib_mpool_cleanup_register(NULL, test_cleanup, NULL));
    EXPECT_EQ(IB_EINVAL, ib_mpool_cleanup_register(mp, NULL, NULL));
    
    char* message;
    
    EXPECT_EQ(IB_EINVAL, ib_mpool_validate(NULL, &message));
    EXPECT_EQ(IB_EINVAL, ib_mpool_validate(mp, NULL));
}

namespace {
    
void muck_with_parent(ib_mpool_t* parent)
{
    static const size_t num_mucks = 1e4;
    ib_mpool_t* mp;
    
    for (size_t i = 0; i < num_mucks; ++i) {
        ib_mpool_create(&mp, NULL, parent);
        ib_mpool_destroy(mp);
    }
}

}

TEST(TestMpool, Multithreading)
{
    static const size_t num_threads = 4;
    
    ib_mpool_t* mp = NULL;
    ib_status_t rc = ib_mpool_create(&mp, NULL, NULL);
    
    ASSERT_EQ(IB_OK, rc);
    ASSERT_TRUE(mp);
    
    boost::thread_group threads;
    for (size_t i = 0; i < num_threads; ++i) {
        threads.create_thread(boost::bind(muck_with_parent, mp));
    }
    
    threads.join_all();
    
    EXPECT_VALID(mp);
    
    ib_mpool_destroy(mp);
}

TEST(TestMpool, ZeroLength)
{
    ib_mpool_t* mp = NULL;
    ib_status_t rc = ib_mpool_create(&mp, NULL, NULL);
    
    ASSERT_EQ(IB_OK, rc);
    ASSERT_TRUE(mp);

    void *p = ib_mpool_alloc(mp, 0);
    EXPECT_TRUE(p); // Not dereferencable
    
    p = ib_mpool_calloc(mp, 1, 0);
    EXPECT_TRUE(p);

    p = ib_mpool_calloc(mp, 0, 1);
    EXPECT_TRUE(p);

    p = ib_mpool_calloc(mp, 0, 0);
    EXPECT_TRUE(p);

    p = ib_mpool_memdup(mp, "", 0);
    EXPECT_TRUE(p);
    
    ib_mpool_destroy(mp);
}

TEST(TestMpool, Path)
{
    ib_mpool_t* mp   = NULL;
    ib_mpool_t* mp_a = NULL;
    ib_mpool_t* mp_b = NULL;

    ib_status_t rc = ib_mpool_create(&mp, "foo", NULL);
    
    ASSERT_EQ(IB_OK, rc);
    ASSERT_TRUE(mp);
    
    char* path = ib_mpool_path(mp);
    
    ASSERT_TRUE(path);
    EXPECT_EQ(string("/foo"), path);
    
    free(path);
    
    rc = ib_mpool_create(&mp_a, "bar", mp);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_TRUE(mp_a);
    
    path = ib_mpool_path(mp_a);
    
    ASSERT_TRUE(path);
    EXPECT_EQ(string("/foo/bar"), path);
    
    free(path);

    rc = ib_mpool_create(&mp_b, "baz", mp_a);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_TRUE(mp_b);
    
    path = ib_mpool_path(mp_b);
    
    ASSERT_TRUE(path);
    EXPECT_EQ(string("/foo/bar/baz"), path);
    
    free(path);
    
    ib_mpool_destroy(mp);
}
