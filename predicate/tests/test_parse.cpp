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
 * @brief Predicate --- Parse Call Tests
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 **/

#include <predicate/parse.hpp>
#include <predicate/eval.hpp>

#include "gtest/gtest.h"

using namespace IronBee::Predicate;
using namespace std;

class CallBase : public Call
{
public:
    virtual size_t hash() const
    {
        return 1;
    }

protected:
    virtual void eval_calculate(
        GraphEvalState& graph_eval_state,
        EvalContext     context
    ) const
    {
        graph_eval_state[index()].finish_false(context);
    }
};

class CallA : public CallBase
{
public:
    virtual std::string name() const
    {
        return "CallA";
    }
};

class CallB : public CallBase
{
public:
    virtual std::string name() const
    {
        return "CallB";
    }
};

class Named : public CallBase
{
public:
    explicit Named(const std::string& name) :
        m_name(name)
    {
        // nop
    }

    virtual std::string name() const
    {
        return m_name;
    }

private:
    const std::string m_name;
};

call_p named(const std::string& name)
{
    return call_p(new Named(name));
}

TEST(TestParse, ValidLiteral)
{
    size_t i;
    string expr;
    node_p r;

    expr = "'foo'";
    i = 0;
    ASSERT_NO_THROW(r = parse_literal(expr, i));
    EXPECT_EQ(expr, r->to_s());
    EXPECT_EQ(expr.length() - 1, i);

    expr = "'foo\\'d'";
    i = 0;
    ASSERT_NO_THROW(r = parse_literal(expr, i));
    EXPECT_EQ(expr, r->to_s());
    EXPECT_EQ(expr.length() - 1, i);

    expr = "'foo\\\\bar'";
    i = 0;
    ASSERT_NO_THROW(r = parse_literal(expr, i));
    EXPECT_EQ(expr, r->to_s());
    EXPECT_EQ(expr.length() - 1, i);

    expr = "''";
    i = 0;
    ASSERT_NO_THROW(r = parse_literal(expr, i));
    EXPECT_EQ(expr, r->to_s());
    EXPECT_EQ(expr.length() - 1, i);

    expr = "'foobar'more";
    i = 0;
    ASSERT_NO_THROW(r = parse_literal(expr, i));
    EXPECT_EQ(expr.substr(0, i + 1), r->to_s());
    EXPECT_GT(expr.length() - 1, i);

    expr = "null";
    i = 0;
    ASSERT_NO_THROW(r = parse_literal(expr, i));
    EXPECT_EQ(expr, r->to_s());
    EXPECT_EQ(expr.length() - 1, i);

    expr = "nullextra";
    i = 0;
    ASSERT_NO_THROW(r = parse_literal(expr, i));
    EXPECT_EQ(expr.substr(0, i + 1), r->to_s());
    EXPECT_GT(expr.length() - 1, i);

    expr = "1234";
    i = 0;
    ASSERT_NO_THROW(r = parse_literal(expr, i));
    EXPECT_EQ(expr.substr(0, i + 1), r->to_s());
    EXPECT_EQ(expr.length() - 1, i);

    expr = "-1234";
    i = 0;
    ASSERT_NO_THROW(r = parse_literal(expr, i));
    EXPECT_EQ(expr.substr(0, i + 1), r->to_s());
    EXPECT_EQ(expr.length() - 1, i);

    expr = "1234.5678";
    i = 0;
    ASSERT_NO_THROW(r = parse_literal(expr, i));
    // Ignore last digit to avoid floating point rounding issues.
    EXPECT_EQ(expr.substr(0, i), r->to_s().substr(0, i));
    EXPECT_EQ(expr.length() - 1, i);

    expr = "-1234.5678";
    i = 0;
    ASSERT_NO_THROW(r = parse_literal(expr, i));
    EXPECT_EQ(expr.substr(0, i), r->to_s().substr(0, i));
    EXPECT_EQ(expr.length() - 1, i);

    expr = "-1234.5678foo";
    i = 0;
    ASSERT_NO_THROW(r = parse_literal(expr, i));
    EXPECT_EQ(expr.substr(0, i), r->to_s().substr(0, i));
    EXPECT_GT(expr.length() - 1, i);
}

TEST(TestParse, InvalidLiteral)
{
    size_t i;
    string expr;

    i = 0;
    EXPECT_THROW(parse_literal("", i), IronBee::einval);
    i = 0;
    EXPECT_THROW(parse_literal("'unfinished", i), IronBee::einval);
    i = 0;
    EXPECT_THROW(parse_literal("'unfinished\\'", i), IronBee::einval);
    i = 0;
    EXPECT_THROW(parse_literal("'unfinished\\", i), IronBee::einval);
    i = 0;
    EXPECT_THROW(parse_literal("garbage", i), IronBee::einval);
    i = 0;
    EXPECT_THROW(parse_literal("-", i), IronBee::einval);
    i = 0;
    EXPECT_THROW(parse_literal("1.2.3", i), IronBee::einval);
    i = 0;
    EXPECT_THROW(parse_literal("1.2.", i), IronBee::einval);
}

TEST(TestParse, ValidCall)
{
    size_t i;
    CallFactory f;
    f.add<CallA>();
    f.add<CallB>();

    string expr;
    node_p r;
    expr = "(CallA)";
    i = 0;
    ASSERT_NO_THROW(r = parse_call(expr, i, f));
    EXPECT_EQ(expr, r->to_s());
    EXPECT_EQ(expr.length() - 1, i);
    expr = "(CallA 'foo')";
    i = 0;
    ASSERT_NO_THROW(r = parse_call(expr, i, f));
    EXPECT_EQ(expr, r->to_s());
    EXPECT_EQ(expr.length() - 1, i);
    expr = "(CallA (CallB (CallA)))";
    i = 0;
    ASSERT_NO_THROW(r = parse_call(expr, i, f));
    EXPECT_EQ(expr, r->to_s());
    EXPECT_EQ(expr.length() - 1, i);
    expr = "(CallA 'foo' (CallB 'bar' (CallA 'baz')))";
    i = 0;
    ASSERT_NO_THROW(r = parse_call(expr, i, f));
    EXPECT_EQ(expr, r->to_s());
    EXPECT_EQ(expr.length() - 1, i);
    expr = "";
    i = 0;
    ASSERT_NO_THROW(r = parse_call(expr, i, f));
    EXPECT_FALSE(r);

    expr = "(CallA)extra";
    i = 0;
    EXPECT_NO_THROW(r = parse_call(expr, i, f));
    EXPECT_EQ(expr.substr(0, i + 1), r->to_s());
    EXPECT_GT(expr.length() - 1, i);

    expr = "(CallA))";
    i = 0;
    EXPECT_NO_THROW(r = parse_call(expr, i, f));
    EXPECT_EQ(expr.substr(0, i + 1), r->to_s());
    EXPECT_GT(expr.length() - 1, i);
}

TEST(TestParse, InvalidCall)
{
    size_t i;
    CallFactory f;
    f.add<CallA>();
    f.add<CallB>();

    i = 0;
    EXPECT_THROW(parse_call("(foo)", i, f), IronBee::enoent);
    i = 0;
    EXPECT_THROW(parse_call("(bad=function)", i, f), IronBee::enoent);
    i = 0;
    EXPECT_THROW(parse_call("'naked literal'", i, f), IronBee::einval);
    i = 0;
    EXPECT_THROW(parse_call("(CallA 'unfinished literal)", i, f), IronBee::einval);
    i = 0;
    EXPECT_THROW(parse_call("(CallA 'unfinished'", i, f), IronBee::einval);
    i = 0;
    EXPECT_THROW(parse_call("extra(CallA)", i, f), IronBee::einval);
    i = 0;
    EXPECT_THROW(parse_call("('no name')", i, f), IronBee::einval);
    i = 0;
    EXPECT_THROW(parse_call("()", i, f), IronBee::einval);
    i = 0;
    EXPECT_THROW(parse_call("(CallA @)", i, f), IronBee::einval);
}

TEST(TestParse, Names)
{
    static const char* names[] = {
        "foo-bar",
        "_foobar",
        "129839213",
        "fO0-_",
        NULL
    };

    size_t i;
    CallFactory f;

    for (const char** name = names; *name; ++name) {
        f.add(*name, named);

        string expr;
        node_p r;
        expr = string("(") + *name + ")";
        i = 0;
        ASSERT_NO_THROW(r = parse_call(expr, i, f)) << expr;
        EXPECT_EQ(expr, r->to_s()) << expr;
        EXPECT_EQ(expr.length() - 1, i) << expr;
    }
}
