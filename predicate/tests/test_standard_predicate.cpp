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

#include <predicate/standard_predicate.hpp>
#include "standard_test.hpp"

using namespace IronBee::Predicate;
using namespace std;

class TestStandardPredicate :
    public StandardTest
{
};

TEST_F(TestStandardPredicate, IsLonger)
{
    EXPECT_TRUE(eval_bool(parse("(isLonger 2 (cat 'a' 'b' 'c'))")));
    EXPECT_FALSE(eval_bool(parse("(isLonger 3 (cat 'a' 'b' 'c'))")));
    EXPECT_EQ("[]", transform("(isLonger 1 'a')"));

    EXPECT_THROW(eval_bool(parse("(isLonger))")), IronBee::einval);
    EXPECT_THROW(eval_bool(parse("(isLonger 'a' 'b'))")), IronBee::einval);
    EXPECT_THROW(eval_bool(parse("(isLonger 2 'b' 'c'))")), IronBee::einval);
}

TEST_F(TestStandardPredicate, IsLiteral)
{
    EXPECT_EQ("''", transform("(isLiteral 'a')"));
    EXPECT_EQ("''", transform("(isLiteral [])"));
    EXPECT_EQ("''", transform("(isLiteral 5)"));
    EXPECT_EQ("''", transform("(isLiteral 5.2)"));
    EXPECT_EQ("[]", transform("(isLiteral (A))"));

    EXPECT_THROW(eval_bool(parse("(isLiteral))")), IronBee::einval);
    EXPECT_THROW(eval_bool(parse("(isLiteral 'a' 'b'))")), IronBee::einval);
}

TEST_F(TestStandardPredicate, IsSimple)
{
    EXPECT_TRUE(eval_bool(parse("(isSimple (cat 'a'))")));
    EXPECT_FALSE(eval_bool(parse("(isSimple (cat 'a' 'b' 'c'))")));
    EXPECT_EQ("''", transform("(isSimple 'a')"));

    EXPECT_THROW(eval_bool(parse("(isSimple))")), IronBee::einval);
    EXPECT_THROW(eval_bool(parse("(isSimple 'a' 'b'))")), IronBee::einval);
}

TEST_F(TestStandardPredicate, IsFinished)
{
    EXPECT_TRUE(eval_bool(parse("(isFinished (cat 'a'))")));
    // @todo Uncomment once we have sequence.
    //EXPECT_FALSE(eval_bool(parse("(isFinished (sequence 0))")));
    EXPECT_EQ("''", transform("(isFinished 'a')"));

    EXPECT_THROW(eval_bool(parse("(isFinished))")), IronBee::einval);
    EXPECT_THROW(eval_bool(parse("(isFinished 'a' 'b'))")), IronBee::einval);
}

TEST_F(TestStandardPredicate, IsHomogeneous)
{
    EXPECT_TRUE(eval_bool(parse("(isHomogeneous (cat 'a' 'b'))")));
    EXPECT_FALSE(eval_bool(parse("(isHomogeneous (cat 'a' 1))")));

    EXPECT_EQ("''", transform("(isHomogeneous 'a')"));

    EXPECT_THROW(eval_bool(parse("(isHomogeneous))")), IronBee::einval);
    EXPECT_THROW(eval_bool(parse("(isHomogeneous 'a' 'b'))")), IronBee::einval);
}
