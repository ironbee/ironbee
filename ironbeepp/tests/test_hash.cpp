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
 * @brief IronBee++ Internals --- Hash Tests
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 **/

#include <ironbeepp/hash.hpp>

#include <ironbeepp/memory_pool_lite.hpp>

#include "gtest/gtest.h"

#include <string>

#include <boost/foreach.hpp>

using namespace std;
using namespace IronBee;

class TestHash : public ::testing::Test
{
public:
    TestHash() :
        m_mm(MemoryPoolLite(m_scoped_pool))
    {
        // nop
    }

protected:
    ScopedMemoryPoolLite m_scoped_pool;
    MemoryManager m_mm;
};

TEST_F(TestHash, pointer_hash_const_iterator)
{
    static const char* a = "a";
    static const char* b = "b";
    static const char* c = "c";

    namespace I = Internal;
    ib_hash_t* h;

    ASSERT_EQ(IB_OK, ib_hash_create(&h, m_mm.ib()));

    ib_hash_set(h, "key_a", (void *)a);
    ib_hash_set(h, "key_b", (void *)b);
    ib_hash_set(h, "key_c", (void *)c);

    I::pointer_hash_const_iterator<const char*> b_i(h);
    I::pointer_hash_const_iterator<const char*> e_i;

    EXPECT_TRUE(b_i != e_i);

    multiset<string> found_keys;
    multiset<const char*> found_values;

    found_keys.insert(string(b_i->first.first, b_i->first.second));
    found_values.insert(b_i->second);

    I::pointer_hash_const_iterator<const char*> n_i = b_i;
    EXPECT_TRUE(b_i == n_i);
    ++n_i;
    EXPECT_TRUE(b_i != n_i);
    EXPECT_TRUE(e_i != n_i);

    found_keys.insert(string(n_i->first.first, n_i->first.second));
    found_values.insert(n_i->second);

    ++n_i;
    EXPECT_TRUE(b_i != n_i);
    EXPECT_TRUE(e_i != n_i);

    found_keys.insert(string(n_i->first.first, n_i->first.second));
    found_values.insert(n_i->second);

    ++n_i;
    EXPECT_TRUE(e_i == n_i);

    EXPECT_EQ(3UL, found_keys.size());
    EXPECT_EQ(3UL, found_values.size());
    EXPECT_EQ(1UL, found_keys.count("key_a"));
    EXPECT_EQ(1UL, found_keys.count("key_b"));
    EXPECT_EQ(1UL, found_keys.count("key_c"));
    EXPECT_EQ(1UL, found_values.count(a));
    EXPECT_EQ(1UL, found_values.count(b));
    EXPECT_EQ(1UL, found_values.count(c));
}

TEST_F(TestHash, hash_const_iterator)
{
    static const ConstByteString a = ByteString::create(m_mm, "a");
    static const ConstByteString b = ByteString::create(m_mm, "b");
    static const ConstByteString c = ByteString::create(m_mm, "c");

    namespace I = Internal;
    ib_hash_t* h;

    ASSERT_EQ(IB_OK, ib_hash_create(&h, m_mm.ib()));

    ib_hash_set(h, "key_a", (void *)a.ib());
    ib_hash_set(h, "key_b", (void *)b.ib());
    ib_hash_set(h, "key_c", (void *)c.ib());

    I::hash_const_iterator<ConstByteString> b_i(h);
    I::hash_const_iterator<ConstByteString> e_i;

    EXPECT_TRUE(b_i != e_i);

    multiset<string> found_keys;
    multiset<ConstByteString> found_values;

    found_keys.insert(string(b_i->first.first, b_i->first.second));
    found_values.insert(b_i->second);

    I::hash_const_iterator<ConstByteString> n_i = b_i;
    EXPECT_TRUE(b_i == n_i);
    ++n_i;
    EXPECT_TRUE(b_i != n_i);
    EXPECT_TRUE(e_i != n_i);

    found_keys.insert(string(n_i->first.first, n_i->first.second));
    found_values.insert(n_i->second);

    ++n_i;
    EXPECT_TRUE(b_i != n_i);
    EXPECT_TRUE(e_i != n_i);

    found_keys.insert(string(n_i->first.first, n_i->first.second));
    found_values.insert(n_i->second);

    ++n_i;
    EXPECT_TRUE(e_i == n_i);

    EXPECT_EQ(3UL, found_keys.size());
    EXPECT_EQ(3UL, found_values.size());
    EXPECT_EQ(1UL, found_keys.count("key_a"));
    EXPECT_EQ(1UL, found_keys.count("key_b"));
    EXPECT_EQ(1UL, found_keys.count("key_c"));
    EXPECT_EQ(1UL, found_values.count(a));
    EXPECT_EQ(1UL, found_values.count(b));
    EXPECT_EQ(1UL, found_values.count(c));
}

TEST_F(TestHash, ConstHash)
{
    static const char* a = "a";
    static const char* b = "b";
    static const char* c = "c";

    ib_hash_t* h;

    ASSERT_EQ(IB_OK, ib_hash_create(&h, m_mm.ib()));

    ib_hash_set(h, "key_a", (void *)a);
    ib_hash_set(h, "key_b", (void *)b);
    ib_hash_set(h, "key_c", (void *)c);

    ConstHash<const char*> hash(h);

    EXPECT_EQ(h, hash.ib());

    multiset<string> found_keys;
    multiset<const char*> found_values;
    BOOST_FOREACH(ConstHash<const char*>::const_reference v, hash) {
        found_keys.insert(string(v.first.first, v.first.second));
        found_values.insert(v.second);
    }
    EXPECT_EQ(3UL, found_keys.size());
    EXPECT_EQ(3UL, found_values.size());
    EXPECT_EQ(1UL, found_keys.count("key_a"));
    EXPECT_EQ(1UL, found_keys.count("key_b"));
    EXPECT_EQ(1UL, found_keys.count("key_c"));
    EXPECT_EQ(1UL, found_values.count(a));
    EXPECT_EQ(1UL, found_values.count(b));
    EXPECT_EQ(1UL, found_values.count(c));

    EXPECT_FALSE(hash.empty());
    EXPECT_EQ(3UL, hash.size());

    EXPECT_EQ(a, hash.get("key_a", 5));
    EXPECT_EQ(a, hash.get(string("key_a")));
    EXPECT_EQ(a, hash.get(ByteString::create(m_mm, "key_a")));
    EXPECT_EQ(a, hash[string("key_a")]);
    EXPECT_EQ(a, hash[ByteString::create(m_mm, "key_a")]);

    List<const char*> l = List<const char*>::create(m_mm);
    hash.get_all(l);

    EXPECT_EQ(3UL, l.size());
}

TEST_F(TestHash, PointerHash)
{
    typedef Hash<const char*> hash_t;
    hash_t h = hash_t::create(m_mm);

    ASSERT_TRUE(h);
    EXPECT_TRUE(h.empty());
    EXPECT_EQ(0UL, h.size());

    static const char* a = "a";
    static const char* b = "b";
    static const char* c = "c";

    h.set("key_a", 5, a);
    h.set("key_b", 5, b);
    h.set("key_c", 5, c);

    EXPECT_EQ(3UL, h.size());

    EXPECT_EQ(a, h.get("key_a"));
    EXPECT_EQ(b, h.remove("key_b"));
    EXPECT_EQ(2UL, h.size());
    EXPECT_THROW(h.get("key_b"), enoent);
    h.clear();
    EXPECT_TRUE(h.empty());
    EXPECT_THROW(h.get("key_a"), enoent);
}

TEST_F(TestHash, IBHash)
{
    typedef Hash<ConstByteString> hash_t;
    hash_t h = hash_t::create(m_mm);

    ASSERT_TRUE(h);
    EXPECT_TRUE(h.empty());
    EXPECT_EQ(0UL, h.size());

    static const ConstByteString a = ByteString::create(m_mm, "a");
    static const ConstByteString b = ByteString::create(m_mm, "b");
    static const ConstByteString c = ByteString::create(m_mm, "c");

    h.set("key_a", 5, a);
    h.set("key_b", 5, b);
    h.set("key_c", 5, c);

    EXPECT_EQ(3UL, h.size());

    EXPECT_EQ(a, h.get("key_a"));
    EXPECT_EQ(b, h.remove("key_b"));
    EXPECT_EQ(2UL, h.size());
    EXPECT_THROW(h.get("key_b"), enoent);
    h.clear();
    EXPECT_TRUE(h.empty());
    EXPECT_THROW(h.get("key_a"), enoent);
}
