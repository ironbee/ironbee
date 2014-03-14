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
 * @brief Predicate --- CallFactory Tests
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 **/

#include <predicate/call_factory.hpp>
#include <predicate/eval.hpp>

#include "gtest/gtest.h"

using namespace IronBee::Predicate;

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

class CallDynamic : public CallBase
{
public:
    explicit CallDynamic(const std::string& name) :
        m_name(name)
    {
        // nop
    }

    virtual std::string name() const
    {
        return m_name;
    }

private:
    const std::string& m_name; // Actual string held in Generator.
};

class CallDynamicGenerator
{
public:
    CallDynamicGenerator(const std::string& name) :
        m_name(name)
    {
        // nop
    }

    call_p operator()(const std::string& name) const
    {
        return call_p(new CallDynamic(m_name));
    }

private:
    std::string m_name;
};

TEST(TestCallFactory, Basic)
{
    CallFactory f;
    f.add<CallA>();
    f.add<CallB>();

    node_p a = f("CallA");
    EXPECT_TRUE(bool(a));
    EXPECT_EQ("(CallA)", a->to_s());
    node_p b = f("CallB");
    EXPECT_TRUE(bool(b));
    EXPECT_EQ("(CallB)", b->to_s());

    EXPECT_THROW(f("foo"), IronBee::enoent);
}

TEST(TestCallFactory, Generator)
{
    CallFactory f;
    f.add("foo", CallDynamicGenerator("foo"));
    f.add("bar", CallDynamicGenerator("bar"));

    node_p foo = f("foo");
    EXPECT_EQ("(foo)", foo->to_s());
    node_p bar = f("bar");
    EXPECT_EQ("(bar)", bar->to_s());

    EXPECT_THROW(f("a"), IronBee::enoent);

    f.add("invalid", CallDynamicGenerator("something else"));
    EXPECT_THROW(f("invalid"), IronBee::einval);
}
