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
 * @brief Predicate --- Less Tests
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 **/

#include <ironbee/predicate/less.hpp>

#include "gtest/gtest.h"

using namespace IronBee::Predicate;
using namespace std;

TEST(TestLess, Equal)
{
    string a("abc");

    EXPECT_FALSE(less_sexpr()(a, a));
}

TEST(TestLess, Prefix)
{
    string a("abc");
    string b("ab");

    EXPECT_FALSE( less_sexpr()(a, b) );
    EXPECT_TRUE(  less_sexpr()(b, a) );
}

TEST(TestLess, CommonPrefix)
{
    string a("abcb");
    string b("abca");

    EXPECT_FALSE( less_sexpr()(a, b) );
    EXPECT_TRUE(  less_sexpr()(b, a) );
}

TEST(TestLess, CommonSuffix)
{
    string a("babc");
    string b("aabc");

    EXPECT_FALSE( less_sexpr()(a, b) );
    EXPECT_TRUE(  less_sexpr()(b, a) );
}

TEST(TestLess, InteriorEven)
{
    string a("abba");
    string b("aaaa");

    EXPECT_FALSE( less_sexpr()(a, b) );
    EXPECT_TRUE(  less_sexpr()(b, a) );
}

TEST(TestLess, InteriorOdd)
{
    string a("abbba");
    string b("ababa");

    EXPECT_FALSE( less_sexpr()(a, b) );
    EXPECT_TRUE(  less_sexpr()(b, a) );
}

TEST(TestLess, RegressionOffByOne)
{
    string a("(A 'B')");
    string b("(A 'C')");
    EXPECT_TRUE(  less_sexpr()(a, b) );
    EXPECT_FALSE( less_sexpr()(b, a) );
}
