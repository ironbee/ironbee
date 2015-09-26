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
 * @brief Predicate --- Standard IronBee Tests
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 **/

#include "standard_test.hpp"

#include <ironbee/predicate/standard_ironbee.hpp>

#include <ironbee/predicate/reporter.hpp>
#include <ironbee/predicate/standard_development.hpp>
#include <ironbee/predicate/standard_predicate.hpp>

#include <ironbee/rule_engine.h>
#include <ironbee/string.h>

using namespace IronBee::Predicate;
using namespace std;

class TestStandardIronBee :
    public StandardTest
{
protected:
    void SetUp()
    {
        Standard::load_ironbee(factory());
        Standard::load_predicate(factory());
        Standard::load_development(factory());
    }
};

TEST_F(TestStandardIronBee, var)
{
    const char *data = "test";
    ib_status_t rc;
    ib_var_source_t *source;
    IronBee::ScopedMemoryPoolLite smp;
    IronBee::MemoryManager mm = smp;

    m_transaction.ib()->rule_exec =
        mm.allocate<ib_rule_exec_t>();
    m_transaction.ib()->rule_exec->phase = IB_PHASE_REQUEST;

    rc = ib_var_source_acquire(
        &source,
        m_transaction.memory_manager().ib(),
        ib_engine_var_config_get(m_transaction.engine().ib()),
        IB_S2SL("TestStandard.Var")
    );
    ASSERT_EQ(IB_OK, rc);

    rc = ib_var_source_set(
        source,
        m_transaction.ib()->var_store,
        IronBee::Field::create_byte_string(
            m_transaction.memory_manager(),
            "", 0,
            IronBee::ByteString::create(
                m_transaction.memory_manager(),
                data, 4
            )
        ).ib()
    );
    EXPECT_EQ(IB_OK, rc);

    EXPECT_EQ("TestStandard.Var:'test'", eval("(var 'TestStandard.Var')"));
    EXPECT_EQ("TestStandard.Var:'test'", eval("(var 'TestStandard.Var' 'REQUEST_HEADER' 'REQUEST')"));
    EXPECT_THROW(eval("(var 'foo' 'bar' 'baz')"), IronBee::einval);
    EXPECT_THROW(eval("(var 'foo' 'REQUEST_HEADER')"), IronBee::einval);
    EXPECT_THROW(eval("(var 'foo' 'REQUEST_HEADER' 'REQUEST' 'baz')"), IronBee::einval);
}

TEST_F(TestStandardIronBee, Operator)
{
    EXPECT_EQ("['foo']", eval("(operator 'istreq' 'fOo' 'foo')"));
    EXPECT_EQ("[['foo']]", eval("(operator 'istreq' 'fOo' ['foo' 'bar'])"));
    EXPECT_EQ(":", eval("(operator 'istreq' 'fOo' 'bar')"));
    EXPECT_EQ("[]", eval("(operator 'istreq' 'fOo' ['bar' 'baz'])"));
    EXPECT_THROW(eval("(operator 'dne' 'a' 'b')"), IronBee::einval);
    EXPECT_THROW(eval("(operator)"), IronBee::einval);
    EXPECT_THROW(eval("(operator 'a')"), IronBee::einval);
    EXPECT_THROW(eval("(operator 'a' 'b')"), IronBee::einval);
    EXPECT_THROW(eval("(operator 'a' 'b' 'c' 'd')"), IronBee::einval);
    EXPECT_THROW(eval("(operator 'a' null 'c')"), IronBee::einval);
    EXPECT_THROW(eval("(operator null 'b' 'c')"), IronBee::einval);
}

TEST_F(TestStandardIronBee, FOperator)
{
    EXPECT_EQ("'foo'", eval("(foperator 'istreq' 'fOo' 'foo')"));
    EXPECT_EQ("['foo']", eval("(foperator 'istreq' 'fOo' ['foo' 'bar'])"));
}

TEST_F(TestStandardIronBee, transformation)
{
    EXPECT_EQ("'foo'", eval("(transformation 'lowercase' '' 'fOO')"));
    EXPECT_EQ("'foo'", transform("(transformation 'lowercase' '' 'fOO')"));
    EXPECT_THROW(eval("(transformation)"), IronBee::einval);
    EXPECT_THROW(eval("(transformation 'a')"), IronBee::einval);
    EXPECT_THROW(eval("(transformation 'a' 'b' 'c')"), IronBee::enoent);
    EXPECT_THROW(eval("(transformation null 'b')"), IronBee::einval);
}

TEST_F(TestStandardIronBee, phase)
{
    // Track old rule_exec so we can restore it.
    ib_rule_exec_t* old_rule_exec = m_transaction.ib()->rule_exec;
    // Test data rule exec.
    ib_rule_exec_t rule_exec;
    m_transaction.ib()->rule_exec = &rule_exec;

    rule_exec.phase = IB_PHASE_REQUEST_HEADER;
    EXPECT_EQ(":", eval("(isFinished (waitPhase 'response_header' 'foo'))"));
    rule_exec.phase = IB_PHASE_RESPONSE_HEADER;
    EXPECT_EQ("''", eval("(isFinished (waitPhase 'response_header' 'foo'))"));

    rule_exec.phase = IB_PHASE_REQUEST_HEADER;
    EXPECT_EQ("''", eval("(isFinished (finishPhase 'request_header' (sequence 0)))"));
    rule_exec.phase = IB_PHASE_RESPONSE_HEADER;
    EXPECT_EQ(":", eval("(isFinished (finishPhase 'request_header' (sequence 0)))"));

    EXPECT_THROW(eval("(waitPhase)"), IronBee::einval);
    EXPECT_THROW(eval("(waitPhase 'request_header')"), IronBee::einval);
    EXPECT_THROW(eval("(waitPhase 'request_header' 'b' 'c')"), IronBee::einval);
    EXPECT_THROW(eval("(waitPhase null 'b')"), IronBee::einval);
    EXPECT_THROW(eval("(waitPhase 'foo' 'b')"), IronBee::einval);

    EXPECT_THROW(eval("(finishPhase)"), IronBee::einval);
    EXPECT_THROW(eval("(finishPhase 'request_header')"), IronBee::einval);
    EXPECT_THROW(eval("(finishPhase 'request_header' 'b' 'c')"), IronBee::einval);
    EXPECT_THROW(eval("(finishPhase null 'b')"), IronBee::einval);
    EXPECT_THROW(eval("(finishPhase 'foo' 'b')"), IronBee::einval);

    m_transaction.ib()->rule_exec = old_rule_exec;
}

class TestAsk :
    public Call
{
public:
    static const string S_NAME;

    virtual const std::string& name() const
    {
        return S_NAME;
    }

    virtual void pre_eval(Environment environment, NodeReporter reporter)
    {
        m_value =
            Value(IronBee::Field::create_dynamic_list<IronBee::Field>(
                environment.engine().main_memory_mm(),
                "", 0,
                getter,
                boost::function<
                    void(
                        IronBee::Field,
                        const char*, size_t,
                        IronBee::ConstList<IronBee::Field>
                    )
                >()
            ));
    }

    virtual void eval_calculate(
        GraphEvalState& graph_eval_state,
        EvalContext     context
    ) const
    {
        NodeEvalState& nes = graph_eval_state.node_eval_state(this, context);
        nes.finish(m_value);
    }

private:
    static
    IronBee::ConstList<IronBee::Field> getter(
        IronBee::ConstField f,
        const char*         param,
        size_t              n
    )
    {
        using namespace IronBee;
        typedef List<Field> result_t;

        result_t result = result_t::create(f.memory_manager());
        result.push_back(
            Field::create_byte_string(
                f.memory_manager(),
                param, n,
                ByteString::create(
                    f.memory_manager(),
                    param, n
                )
            )
        );

        return result;
    }

    Value m_value;
};

const string TestAsk::S_NAME("test_ask");

TEST_F(TestStandardIronBee, Ask)
{
    factory().add<TestAsk>();

    /* Test dynamic behavior. */
    EXPECT_EQ("[foo:'foo']", eval("(ask 'foo' (test_ask))"));

    /* Test sublike behavior. */
    EXPECT_EQ("[a:1]", eval("(ask 'a' [a:1 b:2])"));
    EXPECT_EQ("[a:1 a:3]", eval("(ask 'a' [a:1 b:2 a:3])"));

    EXPECT_THROW(eval("(ask)"), IronBee::einval);
    EXPECT_THROW(eval("(ask 1 'b')"), IronBee::einval);
    EXPECT_THROW(eval("(ask 'a')"), IronBee::einval);
    EXPECT_THROW(eval("(ask 'a' 'b' 'c')"), IronBee::einval);
}
