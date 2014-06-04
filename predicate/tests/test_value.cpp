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
 * @brief Predicate --- Value Tests
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 **/

#include <ironbee/predicate/value.hpp>

#include <ironbeepp/memory_pool_lite.hpp>

#include "gtest/gtest.h"

#include <boost/lexical_cast.hpp>

using namespace IronBee::Predicate;
using namespace std;

TEST(TestValue, Singular)
{
    Value v;

    EXPECT_FALSE(v);
    EXPECT_FALSE(v.to_field());
    EXPECT_FALSE(v.ib());
    EXPECT_EQ(":", v.to_s());
}

TEST(TestValue, Number)
{
    IronBee::ScopedMemoryPoolLite mpl;

    Value v = Value::create_number(mpl, 6);

    ASSERT_TRUE(v);
    EXPECT_EQ(Value::NUMBER, v.type());
    EXPECT_EQ("", string(v.name(), v.name_length()));
    EXPECT_EQ("6", v.to_s());
    EXPECT_EQ(6, v.as_number());
    EXPECT_THROW(v.as_float(), IronBee::einval);
    EXPECT_THROW(v.as_string(), IronBee::einval);
    EXPECT_THROW(v.as_list(), IronBee::einval);

    v = Value::create_number(mpl, "hello", 5, 6);

    ASSERT_TRUE(v);
    EXPECT_EQ(Value::NUMBER, v.type());
    EXPECT_EQ("hello", string(v.name(), v.name_length()));
    EXPECT_EQ("hello:6", v.to_s());
    EXPECT_EQ(6, v.as_number());
    EXPECT_THROW(v.as_float(), IronBee::einval);
    EXPECT_THROW(v.as_string(), IronBee::einval);
    EXPECT_THROW(v.as_list(), IronBee::einval);

    v = v.dup(mpl, "goodbye", 7);

    ASSERT_TRUE(v);
    EXPECT_EQ(Value::NUMBER, v.type());
    EXPECT_EQ("goodbye", string(v.name(), v.name_length()));
    EXPECT_EQ("goodbye:6", v.to_s());
    EXPECT_EQ(6, v.as_number());
    EXPECT_THROW(v.as_float(), IronBee::einval);
    EXPECT_THROW(v.as_string(), IronBee::einval);
    EXPECT_THROW(v.as_list(), IronBee::einval);
}

TEST(TestValue, Float)
{
    IronBee::ScopedMemoryPoolLite mpl;

    Value v = Value::create_float(mpl, 6.0);

    ASSERT_TRUE(v);
    EXPECT_EQ(Value::FLOAT, v.type());
    EXPECT_EQ("", string(v.name(), v.name_length()));
    EXPECT_EQ("6.0", v.to_s().substr(0, 3));
    EXPECT_FLOAT_EQ(6.0, v.as_float());
    EXPECT_THROW(v.as_number(), IronBee::einval);
    EXPECT_THROW(v.as_string(), IronBee::einval);
    EXPECT_THROW(v.as_list(), IronBee::einval);

    v = Value::create_float(mpl, "hello", 5, 6.0);

    ASSERT_TRUE(v);
    EXPECT_EQ(Value::FLOAT, v.type());
    EXPECT_EQ("hello", string(v.name(), v.name_length()));
    EXPECT_EQ("hello:6.0", v.to_s().substr(0, 9));
    EXPECT_FLOAT_EQ(6.0, v.as_float());
    EXPECT_THROW(v.as_number(), IronBee::einval);
    EXPECT_THROW(v.as_string(), IronBee::einval);
    EXPECT_THROW(v.as_list(), IronBee::einval);

    v = v.dup(mpl, "goodbye", 7);

    ASSERT_TRUE(v);
    EXPECT_EQ(Value::FLOAT, v.type());
    EXPECT_EQ("goodbye", string(v.name(), v.name_length()));
    EXPECT_EQ("goodbye:6.0", v.to_s().substr(0, 11));
    EXPECT_FLOAT_EQ(6.0, v.as_float());
    EXPECT_THROW(v.as_number(), IronBee::einval);
    EXPECT_THROW(v.as_string(), IronBee::einval);
    EXPECT_THROW(v.as_list(), IronBee::einval);
}

TEST(TestValue, String)
{
    IronBee::ScopedMemoryPoolLite mpl;

    IronBee::ConstByteString bs = IronBee::ByteString::create(mpl, "foo");
    Value v = Value::create_string(mpl, bs);

    ASSERT_TRUE(v);
    EXPECT_EQ(Value::STRING, v.type());
    EXPECT_EQ("", string(v.name(), v.name_length()));
    EXPECT_EQ("'foo'", v.to_s());
    EXPECT_EQ("foo", v.as_string().to_s());
    EXPECT_THROW(v.as_float(), IronBee::einval);
    EXPECT_THROW(v.as_number(), IronBee::einval);
    EXPECT_THROW(v.as_list(), IronBee::einval);

    v = Value::create_string(mpl, "hello", 5, bs);

    ASSERT_TRUE(v);
    EXPECT_EQ(Value::STRING, v.type());
    EXPECT_EQ("hello", string(v.name(), v.name_length()));
    EXPECT_EQ("hello:'foo'", v.to_s());
    EXPECT_EQ("foo", v.as_string().to_s());
    EXPECT_THROW(v.as_float(), IronBee::einval);
    EXPECT_THROW(v.as_number(), IronBee::einval);
    EXPECT_THROW(v.as_list(), IronBee::einval);

    v = v.dup(mpl, "goodbye", 7);

    ASSERT_TRUE(v);
    EXPECT_EQ(Value::STRING, v.type());
    EXPECT_EQ("goodbye", string(v.name(), v.name_length()));
    EXPECT_EQ("goodbye:'foo'", v.to_s());
    EXPECT_EQ("foo", v.as_string().to_s());
    EXPECT_THROW(v.as_float(), IronBee::einval);
    EXPECT_THROW(v.as_number(), IronBee::einval);
    EXPECT_THROW(v.as_list(), IronBee::einval);
}

TEST(TestValue, List)
{
    IronBee::ScopedMemoryPoolLite mpl;

    IronBee::List<Value> l = IronBee::List<Value>::create(mpl);
    l.push_back(Value::create_number(mpl, 5));
    l.push_back(Value::create_number(mpl, 10));

    Value v = Value::alias_list(mpl, l);

    ASSERT_TRUE(v);
    EXPECT_EQ(Value::LIST, v.type());
    EXPECT_EQ("", string(v.name(), v.name_length()));
    EXPECT_EQ("[5 10]", v.to_s());
    EXPECT_EQ(2UL, v.as_list().size());
    EXPECT_THROW(v.as_float(), IronBee::einval);
    EXPECT_THROW(v.as_number(), IronBee::einval);
    EXPECT_THROW(v.as_string(), IronBee::einval);

    v = Value::alias_list(mpl, "hello", 5, l);

    ASSERT_TRUE(v);
    EXPECT_EQ(Value::LIST, v.type());
    EXPECT_EQ("hello", string(v.name(), v.name_length()));
    EXPECT_EQ("hello:[5 10]", v.to_s());
    EXPECT_EQ(2UL, v.as_list().size());
    EXPECT_THROW(v.as_float(), IronBee::einval);
    EXPECT_THROW(v.as_number(), IronBee::einval);
    EXPECT_THROW(v.as_string(), IronBee::einval);

    v = v.dup(mpl, "goodbye", 7);

    ASSERT_TRUE(v);
    EXPECT_EQ(Value::LIST, v.type());
    EXPECT_EQ("goodbye", string(v.name(), v.name_length()));
    EXPECT_EQ("goodbye:[5 10]", v.to_s());
    EXPECT_EQ(2UL, v.as_list().size());
    EXPECT_THROW(v.as_float(), IronBee::einval);
    EXPECT_THROW(v.as_number(), IronBee::einval);
    EXPECT_THROW(v.as_string(), IronBee::einval);
}

TEST(TestValue, DeepDup)
{
    IronBee::ScopedMemoryPoolLite mpl;

    IronBee::List<Value> l2 = IronBee::List<Value>::create(mpl);
    l2.push_back(Value::create_number(mpl, 5));
    l2.push_back(Value::create_number(mpl, 10));
    IronBee::List<Value> l = IronBee::List<Value>::create(mpl);
    l.push_back(Value::alias_list(mpl, "a", 1, l2));
    l.push_back(Value::alias_list(mpl, "b", 1, l2));

    Value v = Value::alias_list(mpl, l);
    v = v.dup(mpl);

    ASSERT_EQ("[a:[5 10] b:[5 10]]", v.to_s());

    IronBee::ConstList<Value>::iterator i = v.as_list().begin();
    EXPECT_EQ("a:[5 10]", i->to_s());
    IronBee::ConstList<Value> m1 = i->as_list();
    ++i;
    EXPECT_EQ("b:[5 10]", i->to_s());
    IronBee::ConstList<Value> m2 = i->as_list();

    EXPECT_NE(l2, m1);
    EXPECT_NE(l2, m2);
    EXPECT_NE(m1, m2);
}
