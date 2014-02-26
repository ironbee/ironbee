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
 * @brief IronBee++ Internals --- Field Tests
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 **/

#include <ironbeepp/field.hpp>
#include <ironbeepp/memory_pool.hpp>
#include <ironbeepp/exception.hpp>

#include "gtest/gtest.h"

#include <ironbee/util.h>

#include <string>

using namespace std;
using IronBee::Field;
using IronBee::ConstField;
using IronBee::MemoryPool;
using IronBee::ByteString;
using IronBee::ConstByteString;
using IronBee::List;
using IronBee::ConstList;

class TestField : public ::testing::Test
{
public:
    TestField()
    {
        m_pool = MemoryPool::create();
    }

    ~TestField()
    {
        m_pool.destroy();
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
    EXPECT_FALSE(f.is_dynamic());

    f = Field::create_time(m_pool, "test", 4, 18);
    EXPECT_TRUE(f);
    EXPECT_EQ(Field::TIME, f.type());
    EXPECT_EQ(18UL, f.value_as_time());
    EXPECT_EQ("test", f.name_as_s());
    EXPECT_FALSE(f.is_dynamic());

    f = Field::create_float(m_pool, "test", 4, 17.2);
    EXPECT_TRUE(f);
    EXPECT_EQ(Field::FLOAT, f.type());
    EXPECT_EQ(17.2, f.value_as_float());
    EXPECT_EQ("test", f.name_as_s());
    EXPECT_FALSE(f.is_dynamic());

    f = Field::create_null_string(m_pool, "test", 4, "value");
    EXPECT_TRUE(f);
    EXPECT_EQ(Field::NULL_STRING, f.type());
    EXPECT_EQ("value", string(f.value_as_null_string()));
    EXPECT_EQ("test", f.name_as_s());
    EXPECT_FALSE(f.is_dynamic());

    ByteString bs = ByteString::create(m_pool, "value");
    f = Field::create_byte_string(m_pool, "test", 4, bs);
    EXPECT_TRUE(f);
    EXPECT_EQ(Field::BYTE_STRING, f.type());
    EXPECT_EQ(bs.to_s(), f.value_as_byte_string().to_s());
    EXPECT_EQ("test", f.name_as_s());
    EXPECT_FALSE(f.is_dynamic());

    // No Copy specific behavior tested in create no copy test below.
    List<int*> l = List<int*>::create(m_pool);
    f = Field::create_no_copy_list(m_pool, "test", 4, l);
    EXPECT_TRUE(f);
    EXPECT_EQ(Field::LIST, f.type());
    EXPECT_EQ("test", f.name_as_s());
    EXPECT_FALSE(f.is_dynamic());
}

TEST_F(TestField, SetAndGet)
{
    Field f;
    ByteString bs = ByteString::create(m_pool, "value");

    f = Field::create_number(m_pool, "test", 4, 17);
    EXPECT_THROW(f.set_time(1),                IronBee::einval);
    EXPECT_THROW(f.set_float(1.1),             IronBee::einval);
    EXPECT_THROW(f.set_null_string("hello"),   IronBee::einval);
    EXPECT_THROW(f.set_byte_string(bs),        IronBee::einval);
    EXPECT_THROW(f.value_as_null_string(),     IronBee::einval);
    EXPECT_THROW(f.value_as_byte_string(),     IronBee::einval);
    EXPECT_THROW(f.value_as_list<int*>(),      IronBee::einval);
    EXPECT_NO_THROW(f.set_number(-5));
    EXPECT_EQ(-5, f.value_as_number());

    f = Field::create_time(m_pool, "test", 4, 18);
    EXPECT_THROW(f.set_float(1.1),             IronBee::einval);
    EXPECT_THROW(f.set_null_string("hello"),   IronBee::einval);
    EXPECT_THROW(f.set_byte_string(bs),        IronBee::einval);
    EXPECT_THROW(f.value_as_null_string(),     IronBee::einval);
    EXPECT_THROW(f.value_as_byte_string(),     IronBee::einval);
    EXPECT_THROW(f.value_as_list<int*>(),      IronBee::einval);
    EXPECT_NO_THROW(f.set_time(6));
    EXPECT_EQ(6UL, f.value_as_time());

    f = Field::create_float(m_pool, "test", 4, 17.1);
    EXPECT_THROW(f.set_number(1),              IronBee::einval);
    EXPECT_THROW(f.set_time(1),                IronBee::einval);
    EXPECT_THROW(f.set_null_string("hello"),   IronBee::einval);
    EXPECT_THROW(f.set_byte_string(bs),        IronBee::einval);
    EXPECT_THROW(f.value_as_number(),          IronBee::einval);
    EXPECT_THROW(f.value_as_null_string(),     IronBee::einval);
    EXPECT_THROW(f.value_as_byte_string(),     IronBee::einval);
    EXPECT_THROW(f.value_as_list<int*>(),      IronBee::einval);
    EXPECT_NO_THROW(f.set_float(5.2));
    EXPECT_EQ(5.2, f.value_as_float());

    f = Field::create_null_string(m_pool, "test", 4, "value");
    EXPECT_THROW(f.set_number(1),              IronBee::einval);
    EXPECT_THROW(f.set_time(1),                IronBee::einval);
    EXPECT_THROW(f.set_float(1.1),             IronBee::einval);
    EXPECT_THROW(f.set_byte_string(bs),        IronBee::einval);
    EXPECT_THROW(f.value_as_number(),          IronBee::einval);
    EXPECT_THROW(f.value_as_byte_string(),     IronBee::einval);
    EXPECT_THROW(f.value_as_list<int*>(),      IronBee::einval);
    EXPECT_NO_THROW(f.set_null_string("value2"));
    EXPECT_EQ("value2", string(f.value_as_null_string()));

    ByteString bs2 = ByteString::create(m_pool, "value2");
    f = Field::create_byte_string(m_pool, "test", 4, bs);
    EXPECT_THROW(f.set_number(1),              IronBee::einval);
    EXPECT_THROW(f.set_time(1),                IronBee::einval);
    EXPECT_THROW(f.set_float(1.1),             IronBee::einval);
    EXPECT_THROW(f.set_null_string("foo"),     IronBee::einval);
    EXPECT_THROW(f.value_as_number(),          IronBee::einval);
    EXPECT_THROW(f.value_as_null_string(),     IronBee::einval);
    EXPECT_THROW(f.value_as_list<int*>(),      IronBee::einval);
    EXPECT_NO_THROW(f.set_byte_string(bs2));
    EXPECT_EQ("value2", f.value_as_byte_string().to_s());

    List<int*> l = List<int*>::create(m_pool);
    f = Field::create_no_copy_list(m_pool, "test", 4, l);
    EXPECT_THROW(f.set_number(1),              IronBee::einval);
    EXPECT_THROW(f.set_time(1),                IronBee::einval);
    EXPECT_THROW(f.set_float(1.1),             IronBee::einval);
    EXPECT_THROW(f.set_byte_string(bs),        IronBee::einval);
    EXPECT_THROW(f.set_null_string("foo"),     IronBee::einval);
    EXPECT_THROW(f.value_as_number(),          IronBee::einval);
    EXPECT_THROW(f.value_as_null_string(),     IronBee::einval);
    EXPECT_NO_THROW(f.set_no_copy_list<int*>(l));
    EXPECT_EQ(l.ib(), f.value_as_list<int*>().ib());
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
        f = Field::create_dynamic_number(
            m_pool, "test", 4,
            test_getter<int64_t>(v, args),
            test_setter<int64_t>(v, args)
        );
        v = 12;
        EXPECT_EQ(v, f.value_as_number());
        EXPECT_EQ(f, args.field);
        EXPECT_FALSE(args.arg);
        EXPECT_EQ(0UL, args.arg_length);
        v = 13;
        args.reset();
        EXPECT_EQ(v, f.value_as_number("Hello", 5));
        EXPECT_EQ(f, args.field);
        EXPECT_EQ("Hello", string(args.arg, args.arg_length));
        EXPECT_TRUE(f.is_dynamic());

        args.reset();
        v = 0;
        f.set_number(23);
        EXPECT_EQ(23, v);
        EXPECT_EQ(f, args.field);
        EXPECT_FALSE(args.arg);
        EXPECT_EQ(0UL, args.arg_length);
        args.reset();
        v = 0;
        f.set_number(24, "Hello", 5);
        EXPECT_EQ(24, v);
        EXPECT_EQ(f, args.field);
        EXPECT_EQ("Hello", string(args.arg, args.arg_length));

        f.make_static();
        f.set_number(123);
        EXPECT_FALSE(f.is_dynamic());
        EXPECT_EQ(123, f.value_as_number());
    }

    {
        uint64_t v;
        f = Field::create_dynamic_time(
            m_pool, "test", 4,
            test_getter<uint64_t>(v, args),
            test_setter<uint64_t>(v, args)
        );
        v = 12;
        EXPECT_EQ(v, f.value_as_time());
        EXPECT_EQ(f, args.field);
        EXPECT_FALSE(args.arg);
        EXPECT_EQ(0UL, args.arg_length);
        v = 13;
        args.reset();
        EXPECT_EQ(v, f.value_as_time("Hello", 5));
        EXPECT_EQ(f, args.field);
        EXPECT_EQ("Hello", string(args.arg, args.arg_length));
        EXPECT_TRUE(f.is_dynamic());

        args.reset();
        v = 0;
        f.set_time(23);
        EXPECT_EQ(23UL, v);
        EXPECT_EQ(f, args.field);
        EXPECT_FALSE(args.arg);
        EXPECT_EQ(0UL, args.arg_length);
        args.reset();
        v = 0;
        f.set_time(24, "Hello", 5);
        EXPECT_EQ(24UL, v);
        EXPECT_EQ(f, args.field);
        EXPECT_EQ("Hello", string(args.arg, args.arg_length));

        f.make_static();
        f.set_time(123);
        EXPECT_FALSE(f.is_dynamic());
        EXPECT_EQ(123UL, f.value_as_time());
    }

    {
        long double v;
        f = Field::create_dynamic_float(m_pool, "test", 4,
            test_getter<long double>(v, args),
            test_setter<long double>(v, args)
        );
        v = 12.2;
        EXPECT_EQ(v, f.value_as_float());
        EXPECT_EQ(f, args.field);
        EXPECT_FALSE(args.arg);
        EXPECT_EQ(0UL, args.arg_length);
        v = 13.2;
        args.reset();
        EXPECT_EQ(v, f.value_as_float("Hello", 5));
        EXPECT_EQ(f, args.field);
        EXPECT_EQ("Hello", string(args.arg, args.arg_length));
        EXPECT_TRUE(f.is_dynamic());

        args.reset();
        v = 0;
        f.set_float(23);
        EXPECT_EQ(23UL, v);
        EXPECT_EQ(f, args.field);
        EXPECT_FALSE(args.arg);
        EXPECT_EQ(0UL, args.arg_length);
        args.reset();
        v = 0;
        f.set_float(24.2, "Hello", 5);
        EXPECT_EQ(24.2, v);
        EXPECT_EQ(f, args.field);
        EXPECT_EQ("Hello", string(args.arg, args.arg_length));

        f.make_static();
        f.set_float(123.2);
        EXPECT_FALSE(f.is_dynamic());
        EXPECT_EQ(123.2, f.value_as_float());
    }

    {
        const char* v;
        f = Field::create_dynamic_null_string(
            m_pool, "test", 4,
            test_getter<const char*>(v, args),
            test_setter<const char*>(v, args)
        );
        v = "foo";
        EXPECT_EQ(string(v), f.value_as_null_string());
        EXPECT_EQ(f, args.field);
        EXPECT_FALSE(args.arg);
        EXPECT_EQ(0UL, args.arg_length);
        v = "bar";
        args.reset();
        EXPECT_EQ(string(v), f.value_as_null_string("Hello", 5));
        EXPECT_EQ(f, args.field);
        EXPECT_EQ("Hello", string(args.arg, args.arg_length));
        EXPECT_TRUE(f.is_dynamic());

        args.reset();
        v = NULL;
        f.set_null_string("abc");
        EXPECT_EQ(string("abc"), v);
        EXPECT_EQ(f, args.field);
        EXPECT_FALSE(args.arg);
        EXPECT_EQ(0UL, args.arg_length);
        args.reset();
        v = NULL;
        f.set_null_string("def", "Hello", 5);
        EXPECT_EQ(string("def"), v);
        EXPECT_EQ(f, args.field);
        EXPECT_EQ("Hello", string(args.arg, args.arg_length));

        f.make_static();
        f.set_null_string("123");
        EXPECT_FALSE(f.is_dynamic());
        EXPECT_EQ(string("123"), f.value_as_null_string());
    }

    {
        ByteString v;

        f = Field::create_dynamic_byte_string(
            m_pool,
            "test", 4,
            test_getter<ConstByteString>(v, args),
            test_setter<ConstByteString>(v, args)
        );
        v = ByteString::create(m_pool, "foo");
        EXPECT_EQ(v.to_s(), f.value_as_byte_string().to_s());
        EXPECT_EQ(f, args.field);
        EXPECT_FALSE(args.arg);
        EXPECT_EQ(0UL, args.arg_length);
        v = ByteString::create(m_pool, "bar");
        args.reset();
        EXPECT_EQ(v.to_s(), f.value_as_byte_string("Hello", 5).to_s());
        EXPECT_EQ(f, args.field);
        EXPECT_EQ("Hello", string(args.arg, args.arg_length));
        EXPECT_TRUE(f.is_dynamic());

        args.reset();
        v = ByteString();
        f.set_byte_string(ByteString::create(m_pool, "abc"));
        EXPECT_EQ("abc", v.to_s());
        EXPECT_EQ(f, args.field);
        EXPECT_FALSE(args.arg);
        EXPECT_EQ(0UL, args.arg_length);
        args.reset();
        v = ByteString();
        f.set_byte_string(ByteString::create(m_pool, "def"), "Hello", 5);
        EXPECT_EQ("def", v.to_s());
        EXPECT_EQ(f, args.field);
        EXPECT_EQ("Hello", string(args.arg, args.arg_length));

        f.make_static();
        f.set_byte_string(ByteString::create(m_pool, "123"));
        EXPECT_FALSE(f.is_dynamic());
        EXPECT_EQ("123", f.value_as_byte_string().to_s());
    }

    {
        ConstList<int*> v;
        List<int*> v2;

        f = Field::create_dynamic_list<int*>(
            m_pool,
            "test", 4,
            test_getter<ConstList<int*> >(v, args),
            test_setter<ConstList<int*> >(v, args)
        );
        v = List<int*>::create(m_pool);
        EXPECT_EQ(v.ib(), f.value_as_list<int*>().ib());
        EXPECT_EQ(f, args.field);
        EXPECT_FALSE(args.arg);
        EXPECT_EQ(0UL, args.arg_length);
        v2 = List<int*>::create(m_pool);
        args.reset();
        EXPECT_EQ(v.ib(), f.value_as_list<int*>("Hello", 5).ib());
        EXPECT_EQ(f, args.field);
        EXPECT_EQ("Hello", string(args.arg, args.arg_length));
        EXPECT_TRUE(f.is_dynamic());

        args.reset();
        v = List<int*>();
        v2 = List<int*>::create(m_pool);
        f.set_no_copy_list(v2);
        EXPECT_EQ(v2.ib(), v.ib());
        EXPECT_EQ(f, args.field);
        EXPECT_FALSE(args.arg);
        EXPECT_EQ(0UL, args.arg_length);
        args.reset();
        v = List<int*>();
        v2 = List<int*>::create(m_pool);
        f.set_no_copy_list(v2, "Hello", 5);
        EXPECT_EQ(v2.ib(), v.ib());
        EXPECT_EQ(f, args.field);
        EXPECT_EQ("Hello", string(args.arg, args.arg_length));

        f.make_static();
        v2 = List<int*>::create(m_pool);
        f.set_no_copy_list(v2);
        EXPECT_FALSE(f.is_dynamic());
        EXPECT_EQ(v2.ib(), f.value_as_list<int*>().ib());
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

TEST_F(TestField, TypeForType)
{
    EXPECT_EQ(Field::NUMBER, Field::field_type_for_type<int>());
    EXPECT_EQ(Field::NUMBER, Field::field_type_for_type<int64_t>());
    EXPECT_EQ(Field::TIME, Field::field_type_for_type<uint64_t>());
    EXPECT_EQ(
        Field::FLOAT,
        Field::field_type_for_type<long double>()
    );
    EXPECT_EQ(Field::NULL_STRING, Field::field_type_for_type<char*>());
    EXPECT_EQ(Field::NULL_STRING, Field::field_type_for_type<const char*>());
    EXPECT_EQ(Field::BYTE_STRING, Field::field_type_for_type<ByteString>());
    EXPECT_EQ(
        Field::BYTE_STRING,
        Field::field_type_for_type<ConstByteString>()
    );
    EXPECT_EQ(
        Field::LIST,
        Field::field_type_for_type<List<int*> >()
    );
    EXPECT_EQ(
        Field::LIST,
        Field::field_type_for_type<ConstList<int*> >()
    );
}

TEST_F(TestField, CreateNoCopy)
{
    char s[100];
    Field f = Field::create_no_copy_null_string(m_pool, "foo", 3, s);
    std::string v("Hello World");
    std::copy(v.begin(), v.end(), s);
    EXPECT_EQ(std::string(s), f.value_as_null_string());

    ByteString b = ByteString::create(m_pool, "Test2");
    Field f2 = Field::create_no_copy_byte_string(m_pool, "foo", 3, b);
    b.set("Test4");
    EXPECT_EQ(b.to_s(), f2.value_as_byte_string().to_s());

    List<int*> l = List<int*>::create(m_pool);
    Field f3 = Field::create_no_copy_list(m_pool, "foo", 3, l);
    EXPECT_EQ(l.ib(), f3.value_as_list<int*>().ib());
}

TEST_F(TestField, CreateAlias)
{
    {
        int64_t n = 0;
        Field f = Field::create_alias_number(m_pool, "foo", 3, n);

        f.set_number(8);

        EXPECT_EQ(n, 8);
    }
    {
        uint64_t n = 0;
        Field f = Field::create_alias_time(m_pool, "foo", 3, n);

        f.set_time(8);

        EXPECT_EQ(8UL, n);
    }
    {
        long double n = 0;
        Field f = Field::create_alias_float(m_pool, "foo", 3, n);

        f.set_float(8.1);

        EXPECT_EQ(n, 8.1);
    }
    {
        char *s = NULL;
        Field f = Field::create_alias_null_string(m_pool, "foo", 3, s);

        f.set_null_string("Hello");

        EXPECT_EQ(string("Hello"), s);
    }
    {
        ib_bytestr_t* b = NULL;
        Field f = Field::create_alias_byte_string(m_pool, "foo", 3, b);

        f.set_byte_string(ByteString::create(m_pool, "Hello"));

        EXPECT_EQ("Hello", ByteString(b).to_s());
    }
    {
        ib_list_t* l = NULL;
        Field f = Field::create_alias_list(m_pool, "foo", 3, l);

        List<int*> l2 = List<int*>::create(m_pool);
        f.set_no_copy_list(l2);

        EXPECT_EQ(l2.ib(), l);
    }
}

TEST_F(TestField, Mutable)
{
    {
        Field f = Field::create_number(m_pool, "foo", 3, 7);
        f.mutable_value_as_number() = 9;
        EXPECT_EQ(9, f.value_as_number());
    }
    {
        Field f = Field::create_time(m_pool, "foo", 3, 7);
        f.mutable_value_as_time() = 9;
        EXPECT_EQ(9UL, f.value_as_time());
    }
    {
        Field f = Field::create_float(m_pool, "foo", 3, 7.1);
        f.mutable_value_as_float() = 9.1;
        EXPECT_EQ(9.1, f.value_as_float());
    }
    {
        Field f = Field::create_null_string(m_pool, "foo", 3, "Hello");
        f.mutable_value_as_null_string()[0] = 'g';
        EXPECT_EQ(string("gello"), f.value_as_null_string());
    }
    {
        Field f = Field::create_byte_string(m_pool, "foo", 3,
            ByteString::create(m_pool, "Hello"));
        f.mutable_value_as_byte_string().set("ABC");
        EXPECT_EQ("ABC", f.value_as_byte_string().to_s());
    }
    {
        List<int*> l = List<int*>::create(m_pool);
        Field f = Field::create_no_copy_list(m_pool, "foo", 3, l);
        List<int*> l2 = f.mutable_value_as_list<int*>();
        EXPECT_EQ(l.ib(), l2.ib());
    }
}

TEST_F(TestField, set_no_copy)
{
    {
        Field f = Field::create_null_string(m_pool, "foo", 3, "ABC");
        char s[] = "Hello";
        f.set_no_copy_null_string(s);
        s[0] = 'g';
        EXPECT_EQ(string("gello"), f.value_as_null_string());
    }
    {
        Field f = Field::create_byte_string(m_pool, "foo", 3,
            ByteString::create(m_pool, "Hello"));
        ByteString b = ByteString::create(m_pool, "Foo");
        f.set_no_copy_byte_string(b);
        b.set("ABC");
        EXPECT_EQ("ABC", f.value_as_byte_string().to_s());
    }
}
