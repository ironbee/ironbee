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
 * @brief Predicate --- Standard Template Tests
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 **/

#include <predicate/standard_template.hpp>

#include <predicate/standard_list.hpp>

#include "standard_test.hpp"

using namespace IronBee::Predicate;
using namespace std;

class TestStandardTemplate :
    public StandardTest
{
protected:
    void SetUp()
    {
        Standard::load_template(factory());
        Standard::load_list(factory());
    }

    void define_template(
        const string&                        name,
        const Standard::template_arg_list_t& args,
        const node_cp&                       body
    )
    {
        factory().add(name, Standard::define_template(args, body));
    }
};

TEST_F(TestStandardTemplate, NoRef)
{
    define_template(
        "noref", Standard::template_arg_list_t(), parse("(cat 'foo')")
    );

    EXPECT_EQ("(cat 'foo')", transform("(noref)"));
    EXPECT_THROW(eval("(noref 'a')"), IronBee::einval);
}

TEST_F(TestStandardTemplate, Basic)
{
    Standard::template_arg_list_t args;
    args.push_back("a");
    args.push_back("b");
    args.push_back("c");
    define_template(
        "basic", args,
        parse("(cat (ref 'c') (ref 'b') (ref 'a'))")
    );

    EXPECT_EQ("(cat 'foo' 'bar' 'baz')", transform("(basic 'baz' 'bar' 'foo')"));
    EXPECT_THROW(eval("(basic)"), IronBee::einval);
    EXPECT_THROW(eval("(basic 'a')"), IronBee::einval);
    EXPECT_THROW(eval("(basic 'a' 'b')"), IronBee::einval);
    EXPECT_THROW(eval("(basic 'a' 'b' 'c' 'd')"), IronBee::einval);
}

TEST_F(TestStandardTemplate, Deep)
{
    Standard::template_arg_list_t args;
    args.push_back("a");
    args.push_back("b");
    args.push_back("c");
    define_template(
        "deep", args,
        parse("(cat (ref 'a') (list (cat (ref 'b') (list (ref 'c')))))")
    );

    EXPECT_EQ("(cat 'baz' (list (cat 'bar' (list 'foo'))))", transform("(deep 'baz' 'bar' 'foo')"));
}

TEST_F(TestStandardTemplate, SelfReference)
{
    Standard::template_arg_list_t args;
    args.push_back("a");
    define_template(
        "selfref", args,
        parse("(cat (ref 'a'))")
    );
    EXPECT_EQ("(cat (ref 'b'))", transform("(selfref (ref 'b'))"));
}

TEST_F(TestStandardTemplate, Top)
{
    Standard::template_arg_list_t args;
    args.push_back("a");
    define_template(
        "top", args,
        parse("(ref 'a')")
    );
    EXPECT_EQ("(cat 'foo')", transform("(top (cat 'foo'))"));
}

TEST_F(TestStandardTemplate, BadRef)
{
    Standard::template_arg_list_t args;
    args.push_back("a");
    define_template(
        "badref", args,
        parse("(ref 'b')")
    );
    EXPECT_THROW(transform("(badref 'foo')"), runtime_error);
}
