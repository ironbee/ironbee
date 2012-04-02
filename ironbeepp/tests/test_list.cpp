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
 * @brief IronBee++ Internals &mdash; List Tests
 * @internal
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 **/

#include <ironbeepp/list.hpp>
#include <ironbeepp/field.hpp>

#include "gtest/gtest.h"

#include <string>

using namespace std;
using namespace IronBee;

class TestList : public ::testing::Test
{
public:
    TestList()
    {
        m_pool = MemoryPool::create();
    }

protected:
    MemoryPool m_pool;
};

TEST_F(TestList, pointer_list_const_iterator)
{
    static const char* a = "a";
    static const char* b = "b";
    static const char* c = "c";

    namespace I = Internal;
    ib_list_t* l;

    ASSERT_EQ(IB_OK, ib_list_create(&l, m_pool.ib()));

    ib_list_push(l, (void*)a);
    ib_list_push(l, (void*)b);
    ib_list_push(l, (void*)c);

    I::pointer_list_const_iterator<const char*> b_i(IB_LIST_FIRST(l));
    I::pointer_list_const_iterator<const char*> t_i(IB_LIST_LAST(l));

    EXPECT_TRUE(b_i != t_i);
    EXPECT_EQ(a, *b_i);

    I::pointer_list_const_iterator<const char*> n_i = b_i;
    EXPECT_TRUE(b_i == n_i);
    ++n_i;
    EXPECT_TRUE(b_i != n_i);
    EXPECT_TRUE(t_i != n_i);
    EXPECT_EQ(b, *n_i);
    ++n_i;
    EXPECT_TRUE(b_i != n_i);
    EXPECT_TRUE(t_i == n_i);
    EXPECT_EQ(c, *n_i);
    ++n_i;
    I::pointer_list_const_iterator<const char*> e_i = t_i;
    ++e_i;
    EXPECT_TRUE(n_i == e_i);

    --n_i;
    EXPECT_TRUE(t_i == t_i);
    --n_i;
    --n_i;
    EXPECT_TRUE(b_i == n_i);
}

TEST_F(TestList, list_const_iterator)
{
    static const ConstByteString a = ByteString::create(m_pool, "a");
    static const ConstByteString b = ByteString::create(m_pool, "b");
    static const ConstByteString c = ByteString::create(m_pool, "c");

    namespace I = Internal;
    ib_list_t* l;

    ASSERT_EQ(IB_OK, ib_list_create(&l, m_pool.ib()));

    ib_list_push(l, (void*)a.ib());
    ib_list_push(l, (void*)b.ib());
    ib_list_push(l, (void*)c.ib());

    I::list_const_iterator<ConstByteString> b_i(IB_LIST_FIRST(l));
    I::list_const_iterator<ConstByteString> t_i(IB_LIST_LAST(l));

    EXPECT_TRUE(b_i != t_i);
    EXPECT_EQ(a, *b_i);

    I::list_const_iterator<ConstByteString> n_i = b_i;
    EXPECT_TRUE(b_i == n_i);
    ++n_i;
    EXPECT_TRUE(b_i != n_i);
    EXPECT_TRUE(t_i != n_i);
    EXPECT_EQ(b, *n_i);
    ++n_i;
    EXPECT_TRUE(b_i != n_i);
    EXPECT_TRUE(t_i == n_i);
    EXPECT_EQ(c, *n_i);
    ++n_i;
    I::list_const_iterator<ConstByteString> e_i = t_i;
    ++e_i;
    EXPECT_TRUE(n_i == e_i);

    --n_i;
    EXPECT_TRUE(t_i == t_i);
    --n_i;
    --n_i;
    EXPECT_TRUE(b_i == n_i);
}

TEST_F(TestList, ConstList)
{
    static const char* a = "a";
    static const char* b = "b";
    static const char* c = "c";

    namespace I = Internal;
    ib_list_t* l;

    ASSERT_EQ(IB_OK, ib_list_create(&l, m_pool.ib()));

    ib_list_push(l, (void*)a);
    ib_list_push(l, (void*)b);
    ib_list_push(l, (void*)c);

    ConstList<const char*> L(l);

    EXPECT_TRUE(L);
    EXPECT_EQ(l, L.ib());
    EXPECT_EQ(3UL, L.size());
    EXPECT_NE(ConstList<const char*>(), L);
    EXPECT_EQ(a, L.front());
    EXPECT_EQ(c, L.back());

    std::vector<const char*> v;
    std::copy(L.begin(), L.end(), std::back_inserter(v));

    ASSERT_EQ(3UL, v.size());
    EXPECT_EQ(a, v[0]);
    EXPECT_EQ(b, v[1]);
    EXPECT_EQ(c, v[2]);

    v.clear();
    std::copy(L.rbegin(), L.rend(), std::back_inserter(v));

    ASSERT_EQ(3UL, v.size());
    EXPECT_EQ(c, v[0]);
    EXPECT_EQ(b, v[1]);
    EXPECT_EQ(a, v[2]);
}

TEST_F(TestList, ConstListIBIteration)
{
    static const ConstByteString a = ByteString::create(m_pool, "a");
    static const ConstByteString b = ByteString::create(m_pool, "b");
    static const ConstByteString c = ByteString::create(m_pool, "c");

    namespace I = Internal;
    ib_list_t* l;

    ASSERT_EQ(IB_OK, ib_list_create(&l, m_pool.ib()));

    ib_list_push(l, (void*)a.ib());
    ib_list_push(l, (void*)b.ib());
    ib_list_push(l, (void*)c.ib());

    ConstList<ConstByteString> L(l);

    std::vector<ConstByteString> v;

    std::copy(L.begin(), L.end(), std::back_inserter(v));

    ASSERT_EQ(3UL, v.size());
    EXPECT_EQ(a, v[0]);
    EXPECT_EQ(b, v[1]);
    EXPECT_EQ(c, v[2]);
}
