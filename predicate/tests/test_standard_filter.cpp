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
    EXPECT_EQ("b", eval_s("(nth 2 (eq 'b' (cat 'a' 'b' 'b')))"));
    EXPECT_FALSE(eval_bool("(isLonger 2 (eq 'b' (cat 'a' 'b' 'b')))"));

    EXPECT_THROW(eval_bool("(eq)"), IronBee::einval);
    EXPECT_THROW(eval_bool("(eq 'a' 'b' 'c')"), IronBee::einval);
}

TEST_F(TestStandardFilter, ne)
{
    EXPECT_EQ("b", eval_s("(nth 2 (ne 'a' (cat 'a' 'b' 'b')))"));
    EXPECT_FALSE(eval_bool("(isLonger 2 (ne 'b' (cat 'a' 'b' 'b')))"));

    EXPECT_THROW(eval_bool("(ne)"), IronBee::einval);
    EXPECT_THROW(eval_bool("(ne 'a' 'b' 'c')"), IronBee::einval);
}

TEST_F(TestStandardFilter, lt)
{
    EXPECT_EQ(2, eval_n("(nth 2 (lt 1 (cat 0 2 2)))"));
    EXPECT_FALSE(eval_bool("(isLonger 2 (lt 1 (cat 0 2 2)))"));

    EXPECT_THROW(eval_bool("(lt)"), IronBee::einval);
    EXPECT_THROW(eval_bool("(lt 1 2 3)"), IronBee::einval);
    EXPECT_THROW(eval_bool("(lt 'a' 'b')"), IronBee::einval);
}

TEST_F(TestStandardFilter, le)
{
    EXPECT_EQ(2, eval_n("(nth 2 (le 2 (cat 1 2 2)))"));
    EXPECT_FALSE(eval_bool("(isLonger 2 (le 2 (cat 1 2 2)))"));

    EXPECT_THROW(eval_bool("(le)"), IronBee::einval);
    EXPECT_THROW(eval_bool("(le 1 2 3)"), IronBee::einval);
    EXPECT_THROW(eval_bool("(le 'a' 'b')"), IronBee::einval);
}

TEST_F(TestStandardFilter, gt)
{
    EXPECT_EQ(2, eval_n("(nth 2 (gt 3 (cat 5 2 2)))"));
    EXPECT_FALSE(eval_bool("(isLonger 2 (gt 3 (cat 5 2 2)))"));

    EXPECT_THROW(eval_bool("(gt)"), IronBee::einval);
    EXPECT_THROW(eval_bool("(gt 1 2 3)"), IronBee::einval);
    EXPECT_THROW(eval_bool("(gt 'a' 'b')"), IronBee::einval);
}

TEST_F(TestStandardFilter, ge)
{
    EXPECT_EQ(2, eval_n("(nth 2 (ge 2 (cat 5 2 2)))"));
    EXPECT_FALSE(eval_bool("(isLonger 2 (ge 2 (cat 5 2 2)))"));

    EXPECT_THROW(eval_bool("(ge)"), IronBee::einval);
    EXPECT_THROW(eval_bool("(ge 1 2 3)"), IronBee::einval);
    EXPECT_THROW(eval_bool("(ge 'a' 'b')"), IronBee::einval);
}

TEST_F(TestStandardFilter, typed)
{
    EXPECT_EQ(2, eval_n("(nth 2 (typed 'number' (cat 'foo' 2 2 (gather (cat 1 2)))))"));
    EXPECT_EQ("a", eval_s("(nth 2 (typed 'string' (cat 2 'a' (gather (cat 1 2)) 'a')))"));
    EXPECT_FALSE(eval_bool("(typed 'list' (cat 1 2 3))"));
    EXPECT_TRUE(eval_bool("(typed 'list' (cat 1 2 3 (gather (cat 1 2))))"));
    EXPECT_EQ("b", eval_s("(nth 2 (scatter (nth 2 (typed 'list' (cat (gather (cat 1 2 )) (gather (cat 'a' 'b')) 2 3 'foo')))))"));

    EXPECT_THROW(eval_bool("(typed)"), IronBee::einval);
    EXPECT_THROW(eval_bool("(typed 'string' 2 3)"), IronBee::einval);
    EXPECT_THROW(eval_bool("(typed 'not-valid' 'b')"), IronBee::einval);
}

TEST_F(TestStandardFilter, named)
{
    EXPECT_EQ(2, eval_n("(named 'foo' (cat (setName 'bar' 1) (setName 'foo' 2)))"));

    EXPECT_THROW(eval_bool("(named)"), IronBee::einval);
    EXPECT_THROW(eval_bool("(named 'a' 2 3)"), IronBee::einval);
    EXPECT_THROW(eval_bool("(named 5 'b')"), IronBee::einval);
}

TEST_F(TestStandardFilter, namedRx)
{
    EXPECT_EQ(2, eval_n("(namedRx '^.foo' (cat (setName 'nopey' 1) (setName 'afoobaz' 2) (setName 'nope' 3)))"));

    EXPECT_THROW(eval_bool("(namedRx)"), IronBee::einval);
    EXPECT_THROW(eval_bool("(namedRx 'a' 2 3)"), IronBee::einval);
    EXPECT_THROW(eval_bool("(namedRx 5 'b')"), IronBee::einval);
    EXPECT_THROW(eval_bool("(namedRx '(' 'b')"), IronBee::einval);
}
