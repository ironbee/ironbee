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
    EXPECT_EQ("a", eval_s("(nth 1 'a')"));
    EXPECT_EQ("a", eval_s("(nth 1 (cat 'a' 'b' 'c'))"));
    EXPECT_EQ("b", eval_s("(nth 2 (cat 'a' 'b' 'c'))"));
    EXPECT_EQ("c", eval_s("(nth 3 (cat 'a' 'b' 'c'))"));
    EXPECT_FALSE(eval_bool("(nth 0 (cat 'a' 'b' 'c'))"));

    EXPECT_THROW(eval_bool("(nth -3 (cat 'a' 'b' 'c'))"), IronBee::einval);
    EXPECT_THROW(eval_bool("(nth)"), IronBee::einval);
    EXPECT_THROW(eval_bool("(nth 1)"), IronBee::einval);
    EXPECT_THROW(eval_bool("(nth 'a' 'b')"), IronBee::einval);
    EXPECT_THROW(eval_bool("(nth 1 'a' 'b')"), IronBee::einval);
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

TEST_F(TestStandardValueList, Sub)
{
    EXPECT_EQ("1", eval_s("(sub 'a' (gather (cat (setName 'a' '1') (setName 'b' 2) (setName 'c' 3))))"));
    EXPECT_TRUE(eval_bool("(isLonger 1 (sub 'a' (gather (cat (setName 'a' '1') (setName 'b' 2) (setName 'a' 3)))))"));

    EXPECT_THROW(eval_bool("(sub)"), IronBee::einval);
    EXPECT_THROW(eval_bool("(sub 1 'b')"), IronBee::einval);
    EXPECT_THROW(eval_bool("(sub 'a')"), IronBee::einval);
    EXPECT_THROW(eval_bool("(sub 'a' 'b' 'c')"), IronBee::einval);
}
