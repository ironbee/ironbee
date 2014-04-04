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
 * @brief Predicate --- Standard Math Tests
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 **/

#include <predicate/standard_math.hpp>

#include "standard_test.hpp"

using namespace IronBee::Predicate;
using namespace std;

class TestStandardMath :
    public StandardTest
{
protected:
    void SetUp()
    {
        Standard::load_math(factory());
    }
};

TEST_F(TestStandardMath, add)
{
    EXPECT_EQ("7", eval("(add 3 4)"));
    EXPECT_EQ("7.200000", eval("(add 3.2 4)"));
    EXPECT_EQ("7.200000", eval("(add 3 4.2)"));
    EXPECT_EQ("[7 8]", eval("(add 3 [4 5])"));
    EXPECT_EQ("x:7", eval("(add a:3 x:4)"));
    EXPECT_EQ("x:[a:7 b:8]", eval("(add z:3 x:[a:4 b:5])"));
    EXPECT_EQ("x:[a:7 b:8 'hello']", eval("(add z:3 x:[a:4 b:5 'hello'])"));
    EXPECT_EQ(":", eval("(add z:3 :)"));

    EXPECT_EQ("7", transform("(add 3 4)"));
    EXPECT_EQ("7.200000", transform("(add 3.2 4)"));
    EXPECT_EQ("7.200000", transform("(add 3 4.2)"));
    EXPECT_EQ("[7 8]", transform("(add 3 [4 5])"));
    EXPECT_EQ("x:7", transform("(add a:3 x:4)"));
    EXPECT_EQ("x:[a:7 b:8]", transform("(add z:3 x:[a:4 b:5])"));
    EXPECT_EQ("x:[a:7 b:8 'hello']", transform("(add z:3 x:[a:4 b:5 'hello'])"));
    EXPECT_EQ(":", transform("(add z:3 :)"));

    EXPECT_THROW(eval("(add 'a' 6)"), IronBee::einval);
    EXPECT_THROW(eval("(add)"), IronBee::einval);
    EXPECT_THROW(eval("(add 1)"), IronBee::einval);
    EXPECT_THROW(eval("(add 1 2 3)"), IronBee::einval);
}

TEST_F(TestStandardMath, mult)
{
    EXPECT_EQ("12", eval("(mult 3 4)"));
    EXPECT_EQ("12.800000", eval("(mult 3.2 4)"));
    EXPECT_EQ("12.600000", eval("(mult 3 4.2)"));
    EXPECT_EQ("[12 15]", eval("(mult 3 [4 5])"));
    EXPECT_EQ("x:12", eval("(mult a:3 x:4)"));
    EXPECT_EQ("x:[a:12 b:15]", eval("(mult z:3 x:[a:4 b:5])"));
    EXPECT_EQ("x:[a:12 b:15 'hello']", eval("(mult z:3 x:[a:4 b:5 'hello'])"));
    EXPECT_EQ(":", eval("(mult z:3 :)"));

    EXPECT_EQ("12", transform("(mult 3 4)"));
    EXPECT_EQ("12.800000", transform("(mult 3.2 4)"));
    EXPECT_EQ("12.600000", transform("(mult 3 4.2)"));
    EXPECT_EQ("[12 15]", transform("(mult 3 [4 5])"));
    EXPECT_EQ("x:12", transform("(mult a:3 x:4)"));
    EXPECT_EQ("x:[a:12 b:15]", transform("(mult z:3 x:[a:4 b:5])"));
    EXPECT_EQ("x:[a:12 b:15 'hello']", transform("(mult z:3 x:[a:4 b:5 'hello'])"));
    EXPECT_EQ(":", transform("(mult z:3 :)"));

    EXPECT_THROW(eval("(mult 'a' 6)"), IronBee::einval);
    EXPECT_THROW(eval("(mult)"), IronBee::einval);
    EXPECT_THROW(eval("(mult 1)"), IronBee::einval);
    EXPECT_THROW(eval("(mult 1 2 3)"), IronBee::einval);
}

TEST_F(TestStandardMath, neg)
{
    EXPECT_EQ("-2", eval("(neg 2)"));
    EXPECT_EQ("-2.000000", eval("(neg 2.0)"));
    EXPECT_EQ("[-2 -4]", eval("(neg [2 4])"));
    EXPECT_EQ("x:-2", eval("(neg x:2)"));
    EXPECT_EQ("x:[a:-2 b:-4]", eval("(neg x:[a:2 b:4])"));
    EXPECT_EQ(":", eval("(neg :)"));
    EXPECT_EQ("b:'a'", eval("(neg b:'a')"));

    EXPECT_EQ("-2", transform("(neg 2)"));
    EXPECT_EQ("-2.000000", transform("(neg 2.0)"));
    EXPECT_EQ("[-2 -4]", transform("(neg [2 4])"));
    EXPECT_EQ("x:-2", transform("(neg x:2)"));
    EXPECT_EQ("x:[a:-2 b:-4]", transform("(neg x:[a:2 b:4])"));
    EXPECT_EQ(":", transform("(neg :)"));
    EXPECT_EQ("b:'a'", transform("(neg b:'a')"));

    EXPECT_THROW(eval("(neg)"), IronBee::einval);
    EXPECT_THROW(eval("(neg 1 2)"), IronBee::einval);
}

TEST_F(TestStandardMath, recip)
{
    EXPECT_EQ("0.500000", eval("(recip 2)"));
    EXPECT_EQ("[0.500000 0.250000]", eval("(recip [2 4])"));
    EXPECT_EQ("x:0.500000", eval("(recip x:2)"));
    EXPECT_EQ("x:[a:0.500000 b:0.250000]", eval("(recip x:[a:2 b:4])"));
    EXPECT_EQ(":", eval("(recip :)"));
    EXPECT_EQ("b:'a'", eval("(recip b:'a')"));

    EXPECT_EQ("0.500000", transform("(recip 2)"));
    EXPECT_EQ("[0.500000 0.250000]", transform("(recip [2 4])"));
    EXPECT_EQ("x:0.500000", transform("(recip x:2)"));
    EXPECT_EQ("x:[a:0.500000 b:0.250000]", transform("(recip x:[a:2 b:4])"));
    EXPECT_EQ(":", transform("(recip :)"));
    EXPECT_EQ("b:'a'", transform("(recip b:'a')"));

    EXPECT_THROW(eval("(recip)"), IronBee::einval);
    EXPECT_THROW(eval("(recip 1 2)"), IronBee::einval);
}

TEST_F(TestStandardMath, max)
{
    EXPECT_EQ("4", eval("(max [1 2 3 4 'a' 'b'])"));
    EXPECT_EQ("a:4", eval("(max [1 2 3 a:4 b:4 'a' 'b'])"));
    EXPECT_EQ(":", eval("(max ['a' 'b'])"));

    EXPECT_EQ("4", transform("(max [1 2 3 4 'a' 'b'])"));
    EXPECT_EQ("a:4", transform("(max [1 2 3 a:4 b:4 'a' 'b'])"));
    EXPECT_EQ(":", transform("(max ['a' 'b'])"));

    EXPECT_THROW(eval("(max)"), IronBee::einval);
    EXPECT_THROW(eval("(max [1] [2])"), IronBee::einval);
    EXPECT_THROW(eval("(max 'a')"), IronBee::einval);
}

TEST_F(TestStandardMath, min)
{
    EXPECT_EQ("1", eval("(min [1 2 3 4 'a' 'b'])"));
    EXPECT_EQ("a:1", eval("(min [a:1 2 3 a:4 b:4 'a' 'b'])"));
    EXPECT_EQ(":", eval("(min ['a' 'b'])"));

    EXPECT_EQ("1", transform("(min [1 2 3 4 'a' 'b'])"));
    EXPECT_EQ("a:1", transform("(min [a:1 2 3 a:4 b:4 'a' 'b'])"));
    EXPECT_EQ(":", transform("(min ['a' 'b'])"));

    EXPECT_THROW(eval("(min)"), IronBee::einval);
    EXPECT_THROW(eval("(min [1] [2])"), IronBee::einval);
    EXPECT_THROW(eval("(min 'a')"), IronBee::einval);
}