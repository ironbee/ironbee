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
 * @brief IronBee++ Internals &mdash; Field Tests
 * @internal
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 **/

#include <ironbeepp/field.hpp>
#include <ironbeepp/memory_pool.hpp>
#include <ironbeepp/exception.hpp>

#include "gtest/gtest.h"

#include <ironbee/debug.h>

#include <string>

using namespace std;
using IronBee::Field;
using IronBee::ConstField;
using IronBee::MemoryPool;
using IronBee::ByteString;
using IronBee::ConstByteString;

class TestField : public ::testing::Test
{
public:
    TestField()
    {
        m_pool = MemoryPool::create();
    }

protected:
    MemoryPool m_pool;
};

TEST_F(TestField, Construction)
{
    Field f;

    f = Field::create_number(m_pool, "test", 4, 17);
    EXPECT_TRUE(f);
    EXPECT_EQ(Field::NUMBER, f.type());
    EXPECT_EQ(17, f.value_as_number());
    EXPECT_EQ("test", f.name_as_s());
    EXPECT_EQ(m_pool, f.memory_pool());
    EXPECT_FALSE(f.is_dynamic());

    f = Field::create_unsigned_number(m_pool, "test", 4, 17);
    EXPECT_TRUE(f);
    EXPECT_EQ(Field::UNSIGNED_NUMBER, f.type());
    EXPECT_EQ(17UL, f.value_as_unsigned_number());
    EXPECT_EQ("test", f.name_as_s());
    EXPECT_EQ(m_pool, f.memory_pool());
    EXPECT_FALSE(f.is_dynamic());

    f = Field::create_null_string(m_pool, "test", 4, "value");
    EXPECT_TRUE(f);
    EXPECT_EQ(Field::NULL_STRING, f.type());
    EXPECT_EQ("value", string(f.value_as_null_string()));
    EXPECT_EQ("test", f.name_as_s());
    EXPECT_EQ(m_pool, f.memory_pool());
    EXPECT_FALSE(f.is_dynamic());

    ByteString bs = ByteString::create(m_pool, "value");
    f = Field::create_byte_string(m_pool, "test", 4, bs);
    EXPECT_TRUE(f);
    EXPECT_EQ(Field::BYTE_STRING, f.type());
    EXPECT_EQ(bs.to_s(), f.value_as_byte_string().to_s());
    EXPECT_EQ("test", f.name_as_s());
    EXPECT_EQ(m_pool, f.memory_pool());
    EXPECT_FALSE(f.is_dynamic());
}

TEST_F(TestField, SetAndGet)
{
    Field f;
    ByteString bs = ByteString::create(m_pool, "value");

    f = Field::create_number(m_pool, "test", 4, 17);
    EXPECT_THROW(f.set_unsigned_number(1),     IronBee::einval);
    EXPECT_THROW(f.set_null_string("hello"),   IronBee::einval);
    EXPECT_THROW(f.set_byte_string(bs),        IronBee::einval);
    EXPECT_THROW(f.value_as_unsigned_number(), IronBee::einval);
    EXPECT_THROW(f.value_as_null_string(),     IronBee::einval);
    EXPECT_THROW(f.value_as_byte_string(),     IronBee::einval);
    EXPECT_NO_THROW(f.set_number(-5));
    EXPECT_EQ(-5, f.value_as_number());

    f = Field::create_unsigned_number(m_pool, "test", 4, 17);
    EXPECT_THROW(f.set_number(1),              IronBee::einval);
    EXPECT_THROW(f.set_null_string("hello"),   IronBee::einval);
    EXPECT_THROW(f.set_byte_string(bs),        IronBee::einval);
    EXPECT_THROW(f.value_as_number(),          IronBee::einval);
    EXPECT_THROW(f.value_as_null_string(),     IronBee::einval);
    EXPECT_THROW(f.value_as_byte_string(),     IronBee::einval);
    EXPECT_NO_THROW(f.set_unsigned_number(5));
    EXPECT_EQ(5, f.value_as_unsigned_number());

    f = Field::create_null_string(m_pool, "test", 4, "value");
    EXPECT_THROW(f.set_number(1),              IronBee::einval);
    EXPECT_THROW(f.set_unsigned_number(7),     IronBee::einval);
    EXPECT_THROW(f.set_byte_string(bs),        IronBee::einval);
    EXPECT_THROW(f.value_as_number(),          IronBee::einval);
    EXPECT_THROW(f.value_as_unsigned_number(), IronBee::einval);
    EXPECT_THROW(f.value_as_byte_string(),     IronBee::einval);
    EXPECT_NO_THROW(f.set_null_string("value2"));
    EXPECT_EQ("value2", string(f.value_as_null_string()));

    ByteString bs2 = ByteString::create(m_pool, "value2");
    f = Field::create_byte_string(m_pool, "test", 4, bs);
    EXPECT_THROW(f.set_number(1),              IronBee::einval);
    EXPECT_THROW(f.set_unsigned_number(7),     IronBee::einval);
    EXPECT_THROW(f.set_null_string("foo"),     IronBee::einval);
    EXPECT_THROW(f.value_as_number(),          IronBee::einval);
    EXPECT_THROW(f.value_as_unsigned_number(), IronBee::einval);
    EXPECT_THROW(f.value_as_null_string(),     IronBee::einval);
    EXPECT_NO_THROW(f.set_byte_string(bs2));
    EXPECT_EQ("value2", f.value_as_byte_string().to_s());
}

struct test_args
{
    ConstField field;
    const char* arg;
    size_t arg_length;

    test_args()
    {
        reset();
    }

    void reset()
    {
        field = ConstField();
        arg = NULL;
        arg_length = -1;
    }
};

template <typename T>
class test_getter
{
public:
    test_getter(
        const T& value,
        test_args& args
    ) :
        m_value(value),
        m_args(args)
    {
        // nop
    }

    T operator()(ConstField field, const char* arg, size_t arg_length)
    {
        m_args.field = field;
        m_args.arg = arg;
        m_args.arg_length = arg_length;

        return m_value;
    }

private:
    const T&   m_value;
    test_args& m_args;
};

template <typename T>
class test_setter
{
public:
    test_setter(
        T&         value,
        test_args& args
    ) :
        m_value(value),
        m_args(args)
    {
        // nop
    }

    void operator()(
        ConstField field,
        const char* arg,
        size_t arg_length,
        T value
    )
    {
        m_args.field = field;
        m_args.arg = arg;
        m_args.arg_length = arg_length;

        m_value = value;
    }

private:
    T&         m_value;
    test_args& m_args;
};

TEST_F(TestField, Dynamic)
{
    test_args args;
    Field f;

    {
        int64_t v;
        f = Field::create_number(m_pool, "test", 4, 7);
        f.register_dynamic_get_number(test_getter<int64_t>(v, args));
        v = 12;
        EXPECT_EQ(v, f.value_as_number());
        EXPECT_EQ(f, args.field);
        EXPECT_EQ(NULL, args.arg);
        EXPECT_EQ(0, args.arg_length);
        v = 13;
        args.reset();
        EXPECT_EQ(v, f.value_as_number("Hello", 5));
        EXPECT_EQ(f, args.field);
        EXPECT_EQ("Hello", string(args.arg, args.arg_length));
        EXPECT_TRUE(f.is_dynamic());

        f.register_dynamic_set_number(test_setter<int64_t>(v,args));
        args.reset();
        v = 0;
        f.set_number(23);
        EXPECT_EQ(23, v);
        EXPECT_EQ(f, args.field);
        EXPECT_EQ(NULL, args.arg);
        EXPECT_EQ(0, args.arg_length);
        args.reset();
        v = 0;
        f.set_number(24, "Hello", 5);
        EXPECT_EQ(24, v);
        EXPECT_EQ(f, args.field);
        EXPECT_EQ("Hello", string(args.arg, args.arg_length));

        f.set_static_number(123);
        EXPECT_FALSE(f.is_dynamic());
        EXPECT_EQ(123, f.value_as_number());
    }

    {
        uint64_t v;
        f = Field::create_unsigned_number(m_pool, "test", 4, 7);
        f.register_dynamic_get_unsigned_number(test_getter<uint64_t>(v, args));
        v = 12;
        EXPECT_EQ(v, f.value_as_unsigned_number());
        EXPECT_EQ(f, args.field);
        EXPECT_EQ(NULL, args.arg);
        EXPECT_EQ(0, args.arg_length);
        v = 13;
        args.reset();
        EXPECT_EQ(v, f.value_as_unsigned_number("Hello", 5));
        EXPECT_EQ(f, args.field);
        EXPECT_EQ("Hello", string(args.arg, args.arg_length));
        EXPECT_TRUE(f.is_dynamic());

        f.register_dynamic_set_unsigned_number(test_setter<uint64_t>(v,args));
        args.reset();
        v = 0;
        f.set_unsigned_number(23);
        EXPECT_EQ(23, v);
        EXPECT_EQ(f, args.field);
        EXPECT_EQ(NULL, args.arg);
        EXPECT_EQ(0, args.arg_length);
        args.reset();
        v = 0;
        f.set_unsigned_number(24, "Hello", 5);
        EXPECT_EQ(24, v);
        EXPECT_EQ(f, args.field);
        EXPECT_EQ("Hello", string(args.arg, args.arg_length));

        f.set_static_unsigned_number(123);
        EXPECT_FALSE(f.is_dynamic());
        EXPECT_EQ(123, f.value_as_unsigned_number());
    }

    {
        const char* v;
        f = Field::create_null_string(m_pool, "test", 4, "hello");
        f.register_dynamic_get_null_string(test_getter<const char *>(v, args));
        v = "foo";
        EXPECT_EQ(string(v), f.value_as_null_string());
        EXPECT_EQ(f, args.field);
        EXPECT_EQ(NULL, args.arg);
        EXPECT_EQ(0, args.arg_length);
        v = "bar";
        args.reset();
        EXPECT_EQ(string(v), f.value_as_null_string("Hello", 5));
        EXPECT_EQ(f, args.field);
        EXPECT_EQ("Hello", string(args.arg, args.arg_length));
        EXPECT_TRUE(f.is_dynamic());

        f.register_dynamic_set_null_string(test_setter<const char *>(v,args));
        args.reset();
        v = NULL;
        f.set_null_string("abc");
        EXPECT_EQ(string("abc"), v);
        EXPECT_EQ(f, args.field);
        EXPECT_EQ(NULL, args.arg);
        EXPECT_EQ(0, args.arg_length);
        args.reset();
        v = NULL;
        f.set_null_string("def", "Hello", 5);
        EXPECT_EQ(string("def"), v);
        EXPECT_EQ(f, args.field);
        EXPECT_EQ("Hello", string(args.arg, args.arg_length));

        f.set_static_null_string("123");
        EXPECT_FALSE(f.is_dynamic());
        EXPECT_EQ(string("123"), f.value_as_null_string());
    }

    {
        ByteString v;

        f = Field::create_byte_string(
            m_pool,
            "test", 4,
            ByteString::create(m_pool, "hello")
        );
        f.register_dynamic_get_byte_string(
            test_getter<ConstByteString>(v, args)
        );
        v = ByteString::create(m_pool, "foo");
        EXPECT_EQ(v.to_s(), f.value_as_byte_string().to_s());
        EXPECT_EQ(f, args.field);
        EXPECT_EQ(NULL, args.arg);
        EXPECT_EQ(0, args.arg_length);
        v = ByteString::create(m_pool, "bar");
        args.reset();
        EXPECT_EQ(v.to_s(), f.value_as_byte_string("Hello", 5).to_s());
        EXPECT_EQ(f, args.field);
        EXPECT_EQ("Hello", string(args.arg, args.arg_length));
        EXPECT_TRUE(f.is_dynamic());

        f.register_dynamic_set_byte_string(
            test_setter<ConstByteString>(v,args)
        );
        args.reset();
        v = ByteString();
        f.set_byte_string(ByteString::create(m_pool, "abc"));
        EXPECT_EQ("abc", v.to_s());
        EXPECT_EQ(f, args.field);
        EXPECT_EQ(NULL, args.arg);
        EXPECT_EQ(0, args.arg_length);
        args.reset();
        v = ByteString();
        f.set_byte_string(ByteString::create(m_pool, "def"), "Hello", 5);
        EXPECT_EQ("def", v.to_s());
        EXPECT_EQ(f, args.field);
        EXPECT_EQ("Hello", string(args.arg, args.arg_length));

        f.set_static_byte_string(ByteString::create(m_pool, "123"));
        EXPECT_FALSE(f.is_dynamic());
        EXPECT_EQ("123", f.value_as_byte_string().to_s());
    }
}

TEST_F(TestField, ExposeC)
{
    ib_field_t ib_f;

    Field f(&ib_f);
    EXPECT_TRUE(f);
    EXPECT_EQ(&ib_f, f.ib());

    const Field& cf = f;
    EXPECT_EQ(&ib_f, cf.ib());
}

TEST_F(TestField, Const)
{
    Field f = Field::create_number(m_pool, "data", 4, 17);
    ConstField cf = f;
    EXPECT_EQ(cf, f);

    Field f2 = Field::remove_const(cf);

    EXPECT_EQ(cf, f2);
    EXPECT_EQ(f, f2);
}
