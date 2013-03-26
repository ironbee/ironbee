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

#include "../standard.hpp"
#include "../parse.hpp"
#include "../merge_graph.hpp"
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

    Value eval(const std::string& text)
    {
        size_t i = 0;
        return parse_call(text, i, m_factory)->eval(m_transaction);
    }

    node_cp transform(node_p n) const
    {
        MergeGraph G;
        Reporter r;
        size_t i = G.add_root(n);
        n->transform(NodeReporter(r, n), G, m_factory);
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

TEST_F(TestStandard, True)
{
    EXPECT_TRUE(eval("(true)"));
    EXPECT_EQ("''", transform("(true)"));
}

TEST_F(TestStandard, False)
{
    EXPECT_FALSE(eval("(false)"));
    EXPECT_EQ("null", transform("(false)"));
}

TEST_F(TestStandard, Not)
{
    EXPECT_TRUE(eval("(not (false))"));
    EXPECT_FALSE(eval("(not (true))"));
    EXPECT_FALSE(eval("(not '')"));
    EXPECT_FALSE(eval("(not 'foo')"));
    EXPECT_THROW(eval("(not)"), IronBee::einval);
    EXPECT_THROW(eval("(not 'a' 'b')"), IronBee::einval);
    EXPECT_EQ("null", transform("(not '')"));
    EXPECT_EQ("''", transform("(not null)"));
    EXPECT_EQ("(not (A))", transform("(not (A))"));
}

TEST_F(TestStandard, Or)
{
    EXPECT_TRUE(eval("(or (true) (false))"));
    EXPECT_TRUE(eval("(or (true) (false) (false))"));
    EXPECT_FALSE(eval("(or (false) (false))"));
    EXPECT_THROW(eval("(or)"), IronBee::einval);
    EXPECT_THROW(eval("(or (true))"), IronBee::einval);
    EXPECT_EQ("(or (A) (B))", transform("(or (A) (B))"));
    EXPECT_EQ("(or (A) (B))", transform("(or (B) (A))"));
    EXPECT_EQ("''", transform("(or (A) 'a')"));
    EXPECT_EQ("(or (A) (B))", transform("(or (A) (B) null)"));
    EXPECT_EQ("(A)", transform("(or (A) null)"));
    EXPECT_EQ("null", transform("(or null null)"));
}

TEST_F(TestStandard, And)
{
    EXPECT_FALSE(eval("(and (true) (false))"));
    EXPECT_FALSE(eval("(and (true) (false) (true))"));
    EXPECT_TRUE(eval("(and (true) (true))"));
    EXPECT_TRUE(eval("(and (true) (true) (true))"));
    EXPECT_THROW(eval("(and)"), IronBee::einval);
    EXPECT_THROW(eval("(and (true))"), IronBee::einval);
    EXPECT_EQ("(and (A) (B))", transform("(and (A) (B))"));
    EXPECT_EQ("(and (A) (B))", transform("(and (B) (A))"));
    EXPECT_EQ("null", transform("(and (B) null)"));
    EXPECT_EQ("(and (A) (B))", transform("(and (A) (B) 'foo')"));
    EXPECT_EQ("(A)", transform("(and (A) 'foo')"));
    EXPECT_EQ("''", transform("(and 'foo' 'bar')"));
}

TEST_F(TestStandard, DeMorgan)
{
    EXPECT_EQ(
        eval("(and (true) (true))"),
        eval("(not (or (not (true)) (not (true))))")
    );
}
