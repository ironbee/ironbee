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

#include "standard_test.hpp"

using namespace IronBee::Predicate;
using namespace std;

class TestStandardFilter :
    public StandardTest
{
};

TEST_F(TestStandardFilter, eq)
{
    EXPECT_EQ("b", eval_s(parse("(nth 2 (eq (identity 'b') (cat 'a' 'b' 'b')))")));
    EXPECT_FALSE(eval_bool(parse("(isLonger 2 (eq (identity 'b') (cat 'a' 'b' 'b')))")));

    EXPECT_THROW(eval_bool(parse("(eq)")), IronBee::einval);
    EXPECT_THROW(eval_bool(parse("(eq 'a' 'b' 'c')")), IronBee::einval);
}

TEST_F(TestStandardFilter, ne)
{
    EXPECT_EQ("b", eval_s(parse("(nth 2 (ne (identity 'a') (cat 'a' 'b' 'b')))")));
    EXPECT_FALSE(eval_bool(parse("(isLonger 2 (ne (identity 'b') (cat 'a' 'b' 'b')))")));

    EXPECT_THROW(eval_bool(parse("(ne)")), IronBee::einval);
    EXPECT_THROW(eval_bool(parse("(ne 'a' 'b' 'c')")), IronBee::einval);
}

TEST_F(TestStandardFilter, gt)
{
    EXPECT_EQ(2, eval_n(parse("(nth 2 (gt (identity 1) (cat 0 2 2)))")));
    EXPECT_FALSE(eval_bool(parse("(isLonger 2 (gt (identity 1) (cat 0 2 2)))")));

    EXPECT_THROW(eval_bool(parse("(gt)")), IronBee::einval);
    EXPECT_THROW(eval_bool(parse("(gt 1 2 3)")), IronBee::einval);
    EXPECT_THROW(eval_bool(parse("(gt 'a' 'b')")), IronBee::einval);
}

TEST_F(TestStandardFilter, ge)
{
    EXPECT_EQ(2, eval_n(parse("(nth 2 (ge (identity 2) (cat 1 2 2)))")));
    EXPECT_FALSE(eval_bool(parse("(isLonger 2 (ge (identity 2) (cat 1 2 2)))")));

    EXPECT_THROW(eval_bool(parse("(ge)")), IronBee::einval);
    EXPECT_THROW(eval_bool(parse("(ge 1 2 3)")), IronBee::einval);
    EXPECT_THROW(eval_bool(parse("(ge 'a' 'b')")), IronBee::einval);
}

TEST_F(TestStandardFilter, lt)
{
    EXPECT_EQ(2, eval_n(parse("(nth 2 (lt (identity 3) (cat 5 2 2)))")));
    EXPECT_FALSE(eval_bool(parse("(isLonger 2 (lt (identity 3) (cat 5 2 2)))")));

    EXPECT_THROW(eval_bool(parse("(lt)")), IronBee::einval);
    EXPECT_THROW(eval_bool(parse("(lt 1 2 3)")), IronBee::einval);
    EXPECT_THROW(eval_bool(parse("(lt 'a' 'b')")), IronBee::einval);
}

TEST_F(TestStandardFilter, le)
{
    EXPECT_EQ(2, eval_n(parse("(nth 2 (le (identity 2) (cat 5 2 2)))")));
    EXPECT_FALSE(eval_bool(parse("(isLonger 2 (le (identity 2) (cat 5 2 2)))")));

    EXPECT_THROW(eval_bool(parse("(le)")), IronBee::einval);
    EXPECT_THROW(eval_bool(parse("(le 1 2 3)")), IronBee::einval);
    EXPECT_THROW(eval_bool(parse("(le 'a' 'b')")), IronBee::einval);
}

TEST_F(TestStandardFilter, typed)
{
    EXPECT_EQ(2, eval_n(parse("(nth 2 (typed 'number' (cat 'foo' 2 2 (gather (cat 1 2)))))")));
    EXPECT_EQ("a", eval_s(parse("(nth 2 (typed 'string' (cat 2 'a' (gather (cat 1 2)) 'a')))")));
    EXPECT_FALSE(eval_bool(parse("(typed 'list' (cat 1 2 3))")));
    EXPECT_TRUE(eval_bool(parse("(typed 'list' (cat 1 2 3 (gather (cat 1 2))))")));
    EXPECT_EQ("b", eval_s(parse("(nth 2 (scatter (nth 2 (typed 'list' (cat (gather (cat 1 2 )) (gather (cat 'a' 'b')) 2 3 'foo')))))")));

    EXPECT_THROW(eval_bool(parse("(typed)")), IronBee::einval);
    EXPECT_THROW(eval_bool(parse("(typed 'string' 2 3)")), IronBee::einval);
    EXPECT_THROW(eval_bool(parse("(typed 'not-valid' 'b')")), IronBee::einval);
}

TEST_F(TestStandardFilter, named)
{
    EXPECT_EQ(2, eval_n(parse("(named (identity 'foo') (cat (setName 'bar' 1) (setName 'foo' 2)))")));

    EXPECT_THROW(eval_bool(parse("(named)")), IronBee::einval);
    EXPECT_THROW(eval_bool(parse("(named 'a' 2 3)")), IronBee::einval);
    EXPECT_THROW(eval_bool(parse("(named 5 'b')")), IronBee::einval);
}

TEST_F(TestStandardFilter, namedi)
{
    EXPECT_EQ(2, eval_n(parse("(namedi (identity 'fOo') (cat (setName 'bar' 1) (setName 'foo' 2)))")));

    EXPECT_THROW(eval_bool(parse("(namedi)")), IronBee::einval);
    EXPECT_THROW(eval_bool(parse("(namedi 'a' 2 3)")), IronBee::einval);
    EXPECT_THROW(eval_bool(parse("(namedi 5 'b')")), IronBee::einval);
}

TEST_F(TestStandardFilter, sub)
{
    EXPECT_EQ("(namedi 'foo' 'bar')", transform("(sub 'foo' 'bar')"));
}

TEST_F(TestStandardFilter, namedRx)
{
    EXPECT_EQ(2, eval_n(parse("(namedRx '^.foo' (cat (setName 'nopey' 1) (setName 'afoobaz' 2) (setName 'nope' 3)))")));

    EXPECT_THROW(eval_bool(parse("(namedRx)")), IronBee::einval);
    EXPECT_THROW(eval_bool(parse("(namedRx 'a' 2 3)")), IronBee::einval);
    EXPECT_THROW(eval_bool(parse("(namedRx 5 'b')")), IronBee::einval);
    EXPECT_THROW(eval_bool(parse("(namedRx '(' 'b')")), IronBee::einval);
}
