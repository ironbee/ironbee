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

#include <ironbee/rule_engine.h>

using namespace IronBee::Predicate;
using namespace std;

class TestStandardIronBee :
    public StandardTest
{
};

TEST_F(TestStandardIronBee, field)
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
