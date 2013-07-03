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
 * @brief Predicate --- Standard ValueList Tests
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 **/

#include <predicate/standard_valuelist.hpp>
#include "standard_test.hpp"

using namespace IronBee::Predicate;
using namespace std;

class TestStandardValueList :
    public StandardTest
{
};

TEST_F(TestStandardValueList, Name)
{
    EXPECT_TRUE(eval_bool("(setName 'a' 'b')"));
    EXPECT_EQ("b", eval_s("(setName 'a' 'b')"));
    EXPECT_THROW(eval_bool("(setName)"), IronBee::einval);
    EXPECT_THROW(eval_bool("(setName null 'a')"), IronBee::einval);
    EXPECT_THROW(eval_bool("(setName 'a')"), IronBee::einval);
    EXPECT_THROW(eval_bool("(setName 'a' 'b' 'c')"), IronBee::einval);
}

TEST_F(TestStandardValueList, CatFirstRest)
{
    EXPECT_EQ("a", eval_s("(first 'a')"));
    EXPECT_EQ("a", eval_s("(first (cat 'a'))"));
    EXPECT_EQ("a", eval_s("(first (cat 'a' 'b'))"));
    EXPECT_EQ("b", eval_s("(first (rest (cat 'a' 'b')))"));
    EXPECT_EQ("b", eval_s("(first (rest (cat 'a' 'b' 'c')))"));
    EXPECT_EQ("b", eval_s("(first (rest (cat 'a' (cat 'b' 'c'))))"));

    EXPECT_THROW(eval_s("(first 'a' 'b')"), IronBee::einval);
    EXPECT_THROW(eval_s("(first)"), IronBee::einval);
    EXPECT_THROW(eval_s("(rest 'a' 'b')"), IronBee::einval);
    EXPECT_THROW(eval_s("(rest)"), IronBee::einval);

    EXPECT_FALSE(eval_bool("(cat)"));
    EXPECT_FALSE(eval_bool("(first (cat))"));
}

TEST_F(TestStandardValueList, Nth)
{
    EXPECT_EQ("a", eval_s("(Nth 1 'a')"));
    EXPECT_EQ("a", eval_s("(Nth 1 (cat 'a' 'b' 'c'))"));
    EXPECT_EQ("b", eval_s("(Nth 2 (cat 'a' 'b' 'c'))"));
    EXPECT_EQ("c", eval_s("(Nth 3 (cat 'a' 'b' 'c'))"));
    EXPECT_FALSE(eval_bool("(Nth 0 (cat 'a' 'b' 'c'))"));

    EXPECT_THROW(eval_bool("(Nth -3 (cat 'a' 'b' 'c'))"), IronBee::einval);
    EXPECT_THROW(eval_bool("(Nth)"), IronBee::einval);
    EXPECT_THROW(eval_bool("(Nth 1)"), IronBee::einval);
    EXPECT_THROW(eval_bool("(Nth 'a' 'b')"), IronBee::einval);
    EXPECT_THROW(eval_bool("(Nth 1 'a' 'b')"), IronBee::einval);
}

TEST_F(TestStandardValueList, ScatterGather)
{
    EXPECT_EQ("a", eval_s("(first (scatter (gather (cat 'a' 'b'))))"));
    EXPECT_EQ("b", eval_s("(rest (scatter (gather (cat 'a' 'b'))))"));

    EXPECT_THROW(eval_bool("(scatter)"), IronBee::einval);
    EXPECT_THROW(eval_bool("(scatter 'a' 'b')"), IronBee::einval);
    EXPECT_THROW(eval_bool("(gather)"), IronBee::einval);
    EXPECT_THROW(eval_bool("(gather 'a' 'b')"), IronBee::einval);
}
