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
 * @brief IronBee++ Internals --- List Tests
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 **/

#include <ironbeepp/list.hpp>
#include <ironbeepp/field.hpp>
#include <ironbeepp/memory_manager.hpp>
#include <ironbeepp/memory_pool_lite.hpp>

#include "gtest/gtest.h"

#include <string>

using namespace std;
using namespace IronBee;

class TestList : public ::testing::Test
{
public:
    TestList() :
        m_mm(MemoryPoolLite(m_pool))
    {
        // nop
    }

protected:
    ScopedMemoryPoolLite m_pool;
    MemoryManager m_mm;
};

TEST_F(TestList, pointer_list_const_iterator)
{
    static const char* a = "a";
    static const char* b = "b";
    static const char* c = "c";

    namespace I = Internal;
    ib_list_t* l;

    ASSERT_EQ(IB_OK, ib_list_create(&l, m_mm.ib()));

    ib_list_push(l, (void*)a);
    ib_list_push(l, (void*)b);
    ib_list_push(l, (void*)c);

    I::pointer_list_const_iterator<const char*> b_i(l, IB_LIST_FIRST(l));
    I::pointer_list_const_iterator<const char*> t_i(l, IB_LIST_LAST(l));

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
    static const ConstByteString a = ByteString::create(m_mm, "a");
    static const ConstByteString b = ByteString::create(m_mm, "b");
    static const ConstByteString c = ByteString::create(m_mm, "c");

    namespace I = Internal;
    ib_list_t* l;

    ASSERT_EQ(IB_OK, ib_list_create(&l, m_mm.ib()));

    ib_list_push(l, (void*)a.ib());
    ib_list_push(l, (void*)b.ib());
    ib_list_push(l, (void*)c.ib());

    I::list_const_iterator<ConstByteString> b_i(l, IB_LIST_FIRST(l));
    I::list_const_iterator<ConstByteString> t_i(l, IB_LIST_LAST(l));

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

    ASSERT_EQ(IB_OK, ib_list_create(&l, m_mm.ib()));

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

    vector<const char*> v;
    copy(L.begin(), L.end(), back_inserter(v));

    ASSERT_EQ(3UL, v.size());
    EXPECT_EQ(a, v[0]);
    EXPECT_EQ(b, v[1]);
    EXPECT_EQ(c, v[2]);

    v.clear();
    copy(L.rbegin(), L.rend(), back_inserter(v));

    ASSERT_EQ(3UL, v.size());
    EXPECT_EQ(c, v[0]);
    EXPECT_EQ(b, v[1]);
    EXPECT_EQ(a, v[2]);
}

TEST_F(TestList, ConstListIBIteration)
{
    static const ConstByteString a = ByteString::create(m_mm, "a");
    static const ConstByteString b = ByteString::create(m_mm, "b");
    static const ConstByteString c = ByteString::create(m_mm, "c");

    namespace I = Internal;
    ib_list_t* l;

    ASSERT_EQ(IB_OK, ib_list_create(&l, m_mm.ib()));

    ib_list_push(l, (void*)a.ib());
    ib_list_push(l, (void*)b.ib());
    ib_list_push(l, (void*)c.ib());

    ConstList<ConstByteString> L(l);

    vector<ConstByteString> v;

    copy(L.begin(), L.end(), back_inserter(v));

    ASSERT_EQ(3UL, v.size());
    EXPECT_EQ(a, v[0]);
    EXPECT_EQ(b, v[1]);
    EXPECT_EQ(c, v[2]);
}

TEST_F(TestList, EmptyList)
{
    ib_list_t* l;

    ASSERT_EQ(IB_OK, ib_list_create(&l, m_mm.ib()));

    ConstList<int*> L(l);

    ASSERT_TRUE(L.begin() == L.end());
}

TEST_F(TestList, List)
{
    static const char* a = "a";
    static const char* b = "b";
    static const char* c = "c";

    typedef List<const char*> list_t;
    list_t L = list_t::create(m_mm);

    ASSERT_TRUE(L);
    EXPECT_NE(list_t(), L);
    ASSERT_TRUE(L.empty());

    L.push_back(a); // a
    EXPECT_EQ(a, L.back());
    EXPECT_EQ(a, L.front());
    EXPECT_EQ(1UL, L.size());
    EXPECT_FALSE(L.empty());
    L.push_back(b); // a b
    EXPECT_EQ(a, L.front());
    EXPECT_EQ(b, L.back());
    EXPECT_EQ(2UL, L.size());
    L.push_front(c); // c a b
    EXPECT_EQ(c, L.front());
    EXPECT_EQ(b, L.back());

    L.pop_back(); // c a
    EXPECT_EQ(c, L.front());
    EXPECT_EQ(a, L.back());
    L.pop_front(); // a
    EXPECT_EQ(a, L.front());
    EXPECT_EQ(a, L.back());

    L.clear();
    EXPECT_TRUE(L.empty());
}

TEST_F(TestList, is_list)
{
    EXPECT_TRUE(is_list<List<int> >::value);
    EXPECT_FALSE(is_list<int>::value);
    EXPECT_TRUE(is_list<ConstList<int> >::value);
}

TEST_F(TestList, PushToListOfConst)
{
    List<ConstField> L = List<ConstField>::create(m_mm);
    Field f = Field::create_number(m_mm, "foo", 3, 5);
    L.push_back(f);
}
