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
 * @brief Predicate --- Standard Tests
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 **/

#include <predicate/standard.hpp>
#include <predicate/parse.hpp>
#include <predicate/merge_graph.hpp>
#include "../../ironbeepp/tests/fixture.hpp"
#include "parse_fixture.hpp"

#include "gtest/gtest.h"

using namespace IronBee::Predicate;
using namespace std;

class TestStandard :
    public ::testing::Test,
    public IBPPTestFixture,
    public ParseFixture
{
protected:
    void SetUp()
    {
        Standard::load(m_factory);
        m_factory.add("A", &create);
        m_factory.add("B", &create);
    }

    node_p parse(const std::string& text) const
    {
        size_t i = 0;
        return parse_call(text, i, m_factory);
    }

    Value eval(const node_p& n)
    {
        Reporter r;
        NodeReporter nr(r, n);
        n->pre_transform(nr);
        if (r.num_errors() > 0 || r.num_warnings() > 0) {
            r.write_report(cout);
            BOOST_THROW_EXCEPTION(
                IronBee::einval() << IronBee::errinfo_what(
                    "pre_transform() failed."
                )
            );
        }
        n->pre_eval(m_engine, nr);
        if (r.num_errors() > 0 || r.num_warnings() > 0) {
            r.write_report(cout);
            BOOST_THROW_EXCEPTION(
                IronBee::einval() << IronBee::errinfo_what(
                    "pre_eval() failed."
                )
            );
        }
        return n->eval(m_transaction);
    }

    // The following copy the value out and thus are safe to use text
    // as there is no need to keep the expression tree around.
    bool eval_bool(const string& text)
    {
        node_p n = parse(text);
        return !! eval(n);
    }

    string eval_s(const string& text)
    {
        node_p n = parse(text);
        IronBee::ConstField v(eval(n));
        if (! v) {
            throw runtime_error("null is not a string.");
        }
        IronBee::ConstByteString bs = v.value_as_byte_string();
        return bs.to_s();
    }

    int64_t eval_n(const string& text)
    {
        node_p n = parse(text);
        IronBee::ConstField v(eval(n));
        if (! v) {
            throw runtime_error("null is not a string.");
        }
        return v.value_as_number();
    }

    node_cp transform(node_p n) const
    {
        MergeGraph G;
        Reporter r;
        size_t i = G.add_root(n);
        n->transform(G, m_factory, NodeReporter(r, n));
        if (r.num_warnings() || r.num_errors()) {
            throw runtime_error("Warnings/Errors during transform.");
        }
        return G.root(i);
    }

    string transform(const string& s) const
    {
        return transform(parse(s))->to_s();
    }
};

TEST_F(TestStandard, Field)
{
    uint8_t data[4] = {'t', 'e', 's', 't'};
    ib_status_t rc = ib_data_add_bytestr(
        m_transaction.ib()->data,
        "TestStandard.Field",
        data, 4,
        NULL
    );
    EXPECT_EQ(IB_OK, rc);

    EXPECT_EQ("test", eval_s("(field 'TestStandard.Field')"));
}

TEST_F(TestStandard, Operator)
{
    EXPECT_TRUE(eval_bool("(operator 'istreq' 'fOo' 'foo')"));
    EXPECT_FALSE(eval_bool("(operator 'istreq' 'fOo' 'bar')"));
    EXPECT_THROW(eval_bool("(operator 'dne' 'a' 'b')"), IronBee::einval);
    EXPECT_THROW(eval_bool("(operator)"), IronBee::einval);
    EXPECT_THROW(eval_bool("(operator 'a')"), IronBee::einval);
    EXPECT_THROW(eval_bool("(operator 'a' 'b')"), IronBee::einval);
    EXPECT_THROW(eval_bool("(operator 'a' 'b' 'c' 'd')"), IronBee::einval);
    EXPECT_THROW(eval_bool("(operator 'a' null 'c')"), IronBee::einval);
    EXPECT_THROW(eval_bool("(operator null 'b' 'c')"), IronBee::einval);
}

TEST_F(TestStandard, SpecificOperator)
{
    EXPECT_EQ("(operator 'istreq' 'a' 'b')", transform("(istreq 'a' 'b')"));
    EXPECT_THROW(eval_bool("(istreq)"), IronBee::einval);
    EXPECT_THROW(eval_bool("(istreq 'a')"), IronBee::einval);
    EXPECT_THROW(eval_bool("(istreq 'a' 'b' 'c')"), IronBee::einval);
    EXPECT_THROW(eval_bool("(istreq null 'c')"), IronBee::einval);
}

TEST_F(TestStandard, Transformation)
{
    EXPECT_EQ("foo", eval_s("(transformation 'lowercase' 'fOO')"));
    EXPECT_THROW(eval_s("(transformation)"), IronBee::einval);
    EXPECT_THROW(eval_s("(transformation 'a')"), IronBee::einval);
    EXPECT_THROW(eval_s("(transformation 'a' 'b' 'c')"), IronBee::einval);
    EXPECT_THROW(eval_s("(transformation null 'b')"), IronBee::einval);
}

TEST_F(TestStandard, SpecificTransformation)
{
    EXPECT_EQ("(transformation 'lowercase' 'foo')", transform("(lowercase 'foo')"));
    EXPECT_THROW(eval_bool("(lowercase)"), IronBee::einval);
    EXPECT_THROW(eval_bool("(lowercase 'a' 'b')"), IronBee::einval);
}

TEST_F(TestStandard, Name)
{
    EXPECT_TRUE(eval_bool("(set_name 'a' 'b')"));
    EXPECT_EQ("b", eval_s("(set_name 'a' 'b')"));
    EXPECT_THROW(eval_bool("(set_name)"), IronBee::einval);
    EXPECT_THROW(eval_bool("(set_name null 'a')"), IronBee::einval);
    EXPECT_THROW(eval_bool("(set_name 'a')"), IronBee::einval);
    EXPECT_THROW(eval_bool("(set_name 'a' 'b' 'c')"), IronBee::einval);
}

TEST_F(TestStandard, Sub)
{
    EXPECT_EQ("foo", eval_s("(sub 'a' (list (set_name 'a' 'foo') (set_name 'b' 'bar')))"));
    EXPECT_EQ("foo", eval_s("(sub 'A' (list (set_name 'a' 'foo') (set_name 'b' 'bar')))"));
    EXPECT_EQ("bar", eval_s("(sub 'b' (list (set_name 'a' 'foo') (set_name 'b' 'bar')))"));
    EXPECT_FALSE(eval_bool("(sub 'c' (list (set_name 'a' 'foo') (set_name 'b' 'bar')))"));

    EXPECT_THROW(eval_bool("(sub)"), IronBee::einval);
    EXPECT_THROW(eval_bool("(sub null (list))"), IronBee::einval);
    EXPECT_THROW(eval_bool("(sub 'a')"), IronBee::einval);
    EXPECT_THROW(eval_bool("(sub 'a' (list) 'b')"), IronBee::einval);
}

TEST_F(TestStandard, SubAll)
{
    EXPECT_EQ(2, eval_n("(transformation 'count' (suball 'a' (list (set_name 'a' 'foo') (set_name 'a' 'bar') (set_name 'b' 'baz'))))"));
    EXPECT_THROW(eval_bool("(suball)"), IronBee::einval);
    EXPECT_THROW(eval_bool("(suball null (list))"), IronBee::einval);
    EXPECT_THROW(eval_bool("(suball 'a')"), IronBee::einval);
    EXPECT_THROW(eval_bool("(suball 'a' (list) 'b')"), IronBee::einval);
}
