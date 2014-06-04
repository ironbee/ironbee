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
 * @brief Predicate --- Standard Predicate Tests
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 **/

#include <ironbee/predicate/standard_predicate.hpp>
#include <ironbee/predicate/standard_development.hpp>

#include "standard_test.hpp"

using namespace IronBee::Predicate;
using namespace std;

class TestStandardPredicate :
    public StandardTest
{
protected:
    void SetUp()
    {
        Standard::load_predicate(factory());
        Standard::load_development(factory());
        factory().add("A", &create);
    }
};

TEST_F(TestStandardPredicate, IsLiteral)
{
    // Cannot evaluate IsLiteral as it always transforms.

    EXPECT_EQ("''", transform("(isLiteral 'a')"));
    EXPECT_EQ("''", transform("(isLiteral 1)"));
    EXPECT_EQ("''", transform("(isLiteral :)"));
    EXPECT_EQ("''", transform("(isLiteral [1 2 3])"));
    EXPECT_EQ(":", transform("(isLiteral (A))"));

    EXPECT_THROW(eval("(isLiteral 'a')"), IronBee::einval);
    EXPECT_THROW(transform("(isLiteral)"), IronBee::einval);
    EXPECT_THROW(transform("(isLiteral 1 2)"), IronBee::einval);
}

TEST_F(TestStandardPredicate, IsFinished)
{
    EXPECT_EQ("''", eval("(isFinished (identity 'a'))"));
    EXPECT_EQ(":", eval("(isFinished (sequence 0))"));
    EXPECT_EQ("''", transform("(isFinished 'a')"));

    EXPECT_THROW(eval("(isFinished))"), IronBee::einval);
    EXPECT_THROW(eval("(isFinished 'a' 'b'))"), IronBee::einval);
}

TEST_F(TestStandardPredicate, IsLonger)
{
    EXPECT_EQ("''", eval("(isLonger 2 [1 2 3])"));
    EXPECT_EQ(":", eval("(isLonger 5 [1 2 3])"));
    EXPECT_EQ(":", eval("(isLonger 0 3)"));

    EXPECT_EQ("''", transform("(isLonger 2 [1 2 3])"));
    EXPECT_EQ(":", transform("(isLonger 5 [1 2 3])"));
    EXPECT_EQ(":", transform("(isLonger 0 3)"));

    EXPECT_THROW(eval("(isLonger)"), IronBee::einval);
    EXPECT_THROW(eval("(isLonger 1)"), IronBee::einval);
    EXPECT_THROW(eval("(isLonger 1 [1 2 3] 3)"), IronBee::einval);
    EXPECT_THROW(eval("(isLonger 'a' [1 2 3])"), IronBee::einval);
}

TEST_F(TestStandardPredicate, IsList)
{
    EXPECT_EQ("''", eval("(isList [1 2 3])"));
    EXPECT_EQ("''", eval("(isList [])"));
    EXPECT_EQ(":", eval("(isList 5)"));

    EXPECT_EQ("''", transform("(isList [1 2 3])"));
    EXPECT_EQ("''", transform("(isList [])"));
    EXPECT_EQ(":", transform("(isList 5)"));

    EXPECT_THROW(eval("(isList)"), IronBee::einval);
    EXPECT_THROW(eval("(isList 1 2)"), IronBee::einval);
}
