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
 * @brief Predicate --- Standar Tests
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 **/

#include "../standard.hpp"
#include "../parse.hpp"
#include "../../ironbeepp/tests/fixture.hpp"

#include "gtest/gtest.h"

using namespace IronBee::Predicate;
using namespace std;

class TestStandard : public ::testing::Test, public IBPPTestFixture
{
protected:
    void SetUp()
    {
        Standard::load(m_factory);
    }

    DAG::Value eval(const std::string& text)
    {
        size_t i = 0;
        return parse_call(text, i, m_factory)->eval(m_transaction);
    }

    CallFactory m_factory;
};

TEST_F(TestStandard, true)
{
    EXPECT_TRUE(eval("(true)"));
}

TEST_F(TestStandard, false)
{
    EXPECT_FALSE(eval("(false)"));
}

TEST_F(TestStandard, not)
{
    EXPECT_TRUE(eval("(not (false))"));
    EXPECT_FALSE(eval("(not (true))"));
    EXPECT_FALSE(eval("(not '')"));
    EXPECT_FALSE(eval("(not 'foo')"));
    EXPECT_THROW(eval("(not)"), IronBee::einval);
    EXPECT_THROW(eval("(not 'a' 'b')"), IronBee::einval);
}

TEST_F(TestStandard, Or)
{
    EXPECT_TRUE(eval("(or (true) (false))"));
    EXPECT_TRUE(eval("(or (true) (false) (false))"));
    EXPECT_FALSE(eval("(or (false) (false))"));
    EXPECT_THROW(eval("(or)"), IronBee::einval);
    EXPECT_THROW(eval("(or (true))"), IronBee::einval);
}

TEST_F(TestStandard, And)
{
    EXPECT_FALSE(eval("(and (true) (false))"));
    EXPECT_FALSE(eval("(and (true) (false) (true))"));
    EXPECT_TRUE(eval("(and (true) (true))"));
    EXPECT_TRUE(eval("(and (true) (true) (true))"));
    EXPECT_THROW(eval("(and)"), IronBee::einval);
    EXPECT_THROW(eval("(and (true))"), IronBee::einval);
}

TEST_F(TestStandard, DeMorgan)
{
    EXPECT_EQ(
        eval("(and (true) (true))"),
        eval("(not (or (not (true)) (not (true))))")
    );
}
