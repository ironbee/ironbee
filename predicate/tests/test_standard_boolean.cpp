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

#include <predicate/standard_boolean.hpp>
#include "standard_test.hpp"

using namespace IronBee::Predicate;
using namespace std;

class TestStandardBoolean :
    public StandardTest
{
};

TEST_F(TestStandardBoolean, True)
{
    EXPECT_THROW(eval_bool(parse("(true)")), IronBee::einval);
    EXPECT_EQ("''", transform("(true)"));
}

TEST_F(TestStandardBoolean, False)
{
    EXPECT_THROW(eval_bool(parse("(false)")), IronBee::einval);
    EXPECT_EQ("null", transform("(false)"));
}

TEST_F(TestStandardBoolean, Not)
{
    EXPECT_FALSE(eval_bool(parse("(not '')")));
    EXPECT_FALSE(eval_bool(parse("(not 'foo')")));
    EXPECT_THROW(eval_bool(parse("(not)")), IronBee::einval);
    EXPECT_THROW(eval_bool(parse("(not 'a' 'b')")), IronBee::einval);
    EXPECT_EQ("null", transform("(not '')"));
    EXPECT_EQ("''", transform("(not null)"));
    EXPECT_EQ("(not (A))", transform("(not (A))"));
}

TEST_F(TestStandardBoolean, Or)
{
    EXPECT_TRUE(eval_bool(parse("(or '' null)")));
    EXPECT_TRUE(eval_bool(parse("(or '' null null)")));
    EXPECT_FALSE(eval_bool(parse("(or null null)")));
    EXPECT_THROW(eval_bool(parse("(or)")), IronBee::einval);
    EXPECT_THROW(eval_bool(parse("(or '')")), IronBee::einval);
    EXPECT_EQ("(or (A) (B))", transform("(or (A) (B))"));
    EXPECT_EQ("(or (A) (B))", transform("(or (B) (A))"));
    EXPECT_EQ("''", transform("(or (A) 'a')"));
    EXPECT_EQ("(or (A) (B))", transform("(or (A) (B) null)"));
    EXPECT_EQ("(A)", transform("(or (A) null)"));
    EXPECT_EQ("null", transform("(or null null)"));
}

TEST_F(TestStandardBoolean, And)
{
    EXPECT_FALSE(eval_bool(parse("(and '' null)")));
    EXPECT_FALSE(eval_bool(parse("(and '' null '')")));
    EXPECT_TRUE(eval_bool(parse("(and '' '')")));
    EXPECT_TRUE(eval_bool(parse("(and '' '' '')")));
    EXPECT_THROW(eval_bool(parse("(and)")), IronBee::einval);
    EXPECT_THROW(eval_bool(parse("(and '')")), IronBee::einval);
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
        eval_bool(parse("(and '' '')")),
        eval_bool(parse("(not (or (not '') (not '')))"))
    );
}

TEST_F(TestStandardBoolean, If)
{
    EXPECT_THROW(eval_bool(parse("(if '' 'foo')")), IronBee::einval);
    EXPECT_THROW(eval_bool(parse("(if '')")), IronBee::einval);
    EXPECT_THROW(eval_bool(parse("(if)")), IronBee::einval);
    EXPECT_THROW(eval_bool(parse("(if 'a' 'b' 'c' 'd')")), IronBee::einval);
    EXPECT_EQ("'foo'", transform("(if '' 'foo' 'bar')"));
    EXPECT_EQ("'bar'", transform("(if null 'foo' 'bar')"));
}

TEST_F(TestStandardBoolean, OrSC)
{
    EXPECT_TRUE(eval_bool(parse("(orSC '' null)")));
    EXPECT_TRUE(eval_bool(parse("(orSC '' null null)")));
    EXPECT_FALSE(eval_bool(parse("(orSC null null)")));
    EXPECT_THROW(eval_bool(parse("(orSC)")), IronBee::einval);
    EXPECT_THROW(eval_bool(parse("(orSC '')")), IronBee::einval);
    EXPECT_EQ("(orSC (A) (B))", transform("(orSC (A) (B))"));
    EXPECT_NE("(orSC (A) (B))", transform("(orSC (B) (A))"));
    EXPECT_EQ("''", transform("(orSC (A) 'a')"));
    EXPECT_EQ("(orSC (A) (B))", transform("(orSC (A) (B) null)"));
    EXPECT_EQ("(A)", transform("(orSC (A) null)"));
    EXPECT_EQ("null", transform("(orSC null null)"));
}

TEST_F(TestStandardBoolean, AndSC)
{
    EXPECT_FALSE(eval_bool(parse("(andSC '' null)")));
    EXPECT_FALSE(eval_bool(parse("(andSC '' null '')")));
    EXPECT_TRUE(eval_bool(parse("(andSC '' '')")));
    EXPECT_TRUE(eval_bool(parse("(andSC '' '' '')")));
    EXPECT_THROW(eval_bool(parse("(andSC)")), IronBee::einval);
    EXPECT_THROW(eval_bool(parse("(andSC '')")), IronBee::einval);
    EXPECT_EQ("(andSC (A) (B))", transform("(andSC (A) (B))"));
    EXPECT_NE("(andSC (A) (B))", transform("(andSC (B) (A))"));
    EXPECT_EQ("null", transform("(andSC (B) null)"));
    EXPECT_EQ("(andSC (A) (B))", transform("(andSC (A) (B) 'foo')"));
    EXPECT_EQ("(A)", transform("(andSC (A) 'foo')"));
    EXPECT_EQ("''", transform("(andSC 'foo' 'bar')"));
}
