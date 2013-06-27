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
 * @brief Predicate --- Standard Boolean Tests
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 **/

#include <predicate/standard.hpp>
#include "standard_test.hpp"

using namespace IronBee::Predicate;
using namespace std;

class TestStandardBoolean :
    public StandardTest
{
};

TEST_F(TestStandardBoolean, True)
{
    EXPECT_THROW(eval_bool("(true)"), IronBee::einval);
    EXPECT_EQ("''", transform("(true)"));
}

TEST_F(TestStandardBoolean, False)
{
    EXPECT_THROW(eval_bool("(false)"), IronBee::einval);
    EXPECT_EQ("null", transform("(false)"));
}

TEST_F(TestStandardBoolean, Not)
{
    EXPECT_FALSE(eval_bool("(not '')"));
    EXPECT_FALSE(eval_bool("(not 'foo')"));
    EXPECT_THROW(eval_bool("(not)"), IronBee::einval);
    EXPECT_THROW(eval_bool("(not 'a' 'b')"), IronBee::einval);
    EXPECT_EQ("null", transform("(not '')"));
    EXPECT_EQ("''", transform("(not null)"));
    EXPECT_EQ("(not (A))", transform("(not (A))"));
}

TEST_F(TestStandardBoolean, Or)
{
    EXPECT_TRUE(eval_bool("(or '' null)"));
    EXPECT_TRUE(eval_bool("(or '' null null)"));
    EXPECT_FALSE(eval_bool("(or null null)"));
    EXPECT_THROW(eval_bool("(or)"), IronBee::einval);
    EXPECT_THROW(eval_bool("(or '')"), IronBee::einval);
    EXPECT_EQ("(or (A) (B))", transform("(or (A) (B))"));
    EXPECT_EQ("(or (A) (B))", transform("(or (B) (A))"));
    EXPECT_EQ("''", transform("(or (A) 'a')"));
    EXPECT_EQ("(or (A) (B))", transform("(or (A) (B) null)"));
    EXPECT_EQ("(A)", transform("(or (A) null)"));
    EXPECT_EQ("null", transform("(or null null)"));
}

TEST_F(TestStandardBoolean, And)
{
    EXPECT_FALSE(eval_bool("(and '' null)"));
    EXPECT_FALSE(eval_bool("(and '' null '')"));
    EXPECT_TRUE(eval_bool("(and '' '')"));
    EXPECT_TRUE(eval_bool("(and '' '' '')"));
    EXPECT_THROW(eval_bool("(and)"), IronBee::einval);
    EXPECT_THROW(eval_bool("(and '')"), IronBee::einval);
    EXPECT_EQ("(and (A) (B))", transform("(and (A) (B))"));
    EXPECT_EQ("(and (A) (B))", transform("(and (B) (A))"));
    EXPECT_EQ("null", transform("(and (B) null)"));
    EXPECT_EQ("(and (A) (B))", transform("(and (A) (B) 'foo')"));
    EXPECT_EQ("(A)", transform("(and (A) 'foo')"));
    EXPECT_EQ("''", transform("(and 'foo' 'bar')"));
}

TEST_F(TestStandardBoolean, DeMorgan)
{
    EXPECT_EQ(
        eval_bool("(and '' '')"),
        eval_bool("(not (or (not '') (not '')))")
    );
}

TEST_F(TestStandardBoolean, If)
{
    EXPECT_EQ("foo", eval_s("(if '' 'foo' 'bar')"));
    EXPECT_EQ("bar", eval_s("(if null 'foo' 'bar')"));
    EXPECT_THROW(eval_bool("(if '' 'foo')"), IronBee::einval);
    EXPECT_THROW(eval_bool("(if '')"), IronBee::einval);
    EXPECT_THROW(eval_bool("(if)"), IronBee::einval);
    EXPECT_THROW(eval_bool("(if 'a' 'b' 'c' 'd')"), IronBee::einval);
    EXPECT_EQ("'foo'", transform("(if '' 'foo' 'bar')"));
    EXPECT_EQ("'bar'", transform("(if null 'foo' 'bar')"));
}
