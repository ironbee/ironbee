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

#include <predicate/reporter.hpp>

#include <ironbee/rule_engine.h>
#include <ironbee/string.h>

using namespace IronBee::Predicate;
using namespace std;

class TestStandardIronBee :
    public StandardTest
{
};

TEST_F(TestStandardIronBee, var)
{
    const char *data = "test";
    ib_status_t rc;
    ib_var_source_t *source;
    IronBee::ScopedMemoryPool smp;
    IronBee::MemoryPool mp = smp;

    m_transaction.ib()->rule_exec =
        mp.allocate<ib_rule_exec_t>();
    m_transaction.ib()->rule_exec->phase = IB_PHASE_REQUEST;

    rc = ib_var_source_acquire(
        &source,
        m_transaction.memory_pool().ib(),
        ib_engine_var_config_get(m_transaction.engine().ib()),
        IB_S2SL("TestStandard.Var")
    );
    ASSERT_EQ(IB_OK, rc);

    rc = ib_var_source_set(
        source,
        m_transaction.ib()->var_store,
        IronBee::Field::create_byte_string(
            m_transaction.memory_pool(),
            "", 0,
            IronBee::ByteString::create(
                m_transaction.memory_pool(),
                data, 4
            )
        ).ib()
    );
    EXPECT_EQ(IB_OK, rc);

    EXPECT_EQ("test", eval_s("(var 'TestStandard.Var')"));
    EXPECT_EQ("test", eval_s("(var 'TestStandard.Var' 'REQUEST_HEADER' 'REQUEST')"));
    EXPECT_THROW(eval_bool("(var 'foo' 'bar' 'baz')"), IronBee::einval);
    EXPECT_THROW(eval_bool("(var 'foo' 'REQUEST_HEADER')"), IronBee::einval);
    EXPECT_THROW(eval_bool("(var 'foo' 'REQUEST_HEADER' 'REQUEST' 'baz')"), IronBee::einval);
}

TEST_F(TestStandardIronBee, Field)
{
    EXPECT_EQ("(var 'foo')", transform("(field 'foo')"));
}

TEST_F(TestStandardIronBee, Operator)
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

TEST_F(TestStandardIronBee, FOperator)
{
    EXPECT_EQ("foo", eval_s("(foperator 'istreq' 'fOo' 'foo')"));
}

TEST_F(TestStandardIronBee, transformation)
{
    EXPECT_EQ("foo", eval_s("(transformation 'lowercase' 'fOO')"));
    EXPECT_THROW(eval_s("(transformation)"), IronBee::einval);
    EXPECT_THROW(eval_s("(transformation 'a')"), IronBee::einval);
    EXPECT_THROW(eval_s("(transformation 'a' 'b' 'c')"), IronBee::einval);
    EXPECT_THROW(eval_s("(transformation null 'b')"), IronBee::einval);
}

TEST_F(TestStandardIronBee, phase)
{
    // Track old rule_exec so we can restore it.
    ib_rule_exec_t* old_rule_exec = m_transaction.ib()->rule_exec;
    // Test data rule exec.
    ib_rule_exec_t rule_exec;
    m_transaction.ib()->rule_exec = &rule_exec;

    rule_exec.phase = IB_PHASE_REQUEST_HEADER;
    EXPECT_FALSE(eval_bool("(isFinished (waitPhase 'response_header' 'foo'))"));
    rule_exec.phase = IB_PHASE_RESPONSE_HEADER;
    EXPECT_TRUE(eval_bool("(isFinished (waitPhase 'response_header' 'foo'))"));

    rule_exec.phase = IB_PHASE_REQUEST_HEADER;
    EXPECT_TRUE(eval_bool("(isFinished (finishPhase 'request_header' (sequence 0)))"));
    rule_exec.phase = IB_PHASE_RESPONSE_HEADER;
    EXPECT_FALSE(eval_bool("(isFinished (finishPhase 'request_header' (sequence 0)))"));

    EXPECT_THROW(eval_s("(waitPhase)"), IronBee::einval);
    EXPECT_THROW(eval_s("(waitPhase 'request_header')"), IronBee::einval);
    EXPECT_THROW(eval_s("(waitPhase 'request_header' 'b' 'c')"), IronBee::einval);
    EXPECT_THROW(eval_s("(waitPhase null 'b')"), IronBee::einval);
    EXPECT_THROW(eval_s("(waitPhase 'foo' 'b')"), IronBee::einval);

    EXPECT_THROW(eval_s("(finishPhase)"), IronBee::einval);
    EXPECT_THROW(eval_s("(finishPhase 'request_header')"), IronBee::einval);
    EXPECT_THROW(eval_s("(finishPhase 'request_header' 'b' 'c')"), IronBee::einval);
    EXPECT_THROW(eval_s("(finishPhase null 'b')"), IronBee::einval);
    EXPECT_THROW(eval_s("(finishPhase 'foo' 'b')"), IronBee::einval);

    m_transaction.ib()->rule_exec = old_rule_exec;
}

class TestAsk :
    public Call
{
public:
    virtual std::string name() const
    {
        return "test_ask";
    }

    virtual void pre_eval(Environment environment, NodeReporter reporter)
    {
        m_value =
            IronBee::Field::create_dynamic_list<Value>(
                environment.main_memory_pool(),
                "", 0,
                getter,
                boost::function<
                    void(
                        IronBee::Field,
                        const char*, size_t,
                        IronBee::ConstList<Value>
                    )
                >()
            );
    }

protected:
    virtual void calculate(EvalContext context)
    {
        add_value(m_value);
        finish();
    }

private:
    static
    IronBee::ConstList<Value> getter(
        IronBee::ConstField f,
        const char*         param,
        size_t              n
    )
    {
        using namespace IronBee;
        typedef List<Value> result_t;

        result_t result = result_t::create(f.memory_pool());
        result.push_back(
            IronBee::Field::create_byte_string(
                f.memory_pool(),
                param, n,
                IronBee::ByteString::create(
                    f.memory_pool(),
                    param, n
                )
            )
        );

        return result;
    }

    Value m_value;
};

TEST_F(TestStandardIronBee, Ask)
{
    m_factory.add<TestAsk>();

    /* Test dynamic behavior. */
    EXPECT_EQ("foo", eval_s("(ask 'foo' (test_ask))"));

    /* Test sublike behavior. */
    EXPECT_EQ("1", eval_s("(ask 'a' (gather (cat (setName 'a' '1') (setName 'b' 2) (setName 'c' 3))))"));
    EXPECT_TRUE(eval_bool("(isLonger 1 (ask 'a' (gather (cat (setName 'a' '1') (setName 'b' 2) (setName 'a' 3)))))"));

    EXPECT_THROW(eval_bool("(ask)"), IronBee::einval);
    EXPECT_THROW(eval_bool("(ask 1 'b')"), IronBee::einval);
    EXPECT_THROW(eval_bool("(ask 'a')"), IronBee::einval);
    EXPECT_THROW(eval_bool("(ask 'a' 'b' 'c')"), IronBee::einval);
}
