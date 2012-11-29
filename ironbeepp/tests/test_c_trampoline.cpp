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
 * @brief IronBee++ --- C Trampoline Tests
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 **/

#include <ironbeepp/c_trampoline.hpp>

#include "gtest/gtest.h"

using namespace std;
using namespace IronBee;

struct plus3
{
    int operator()(int x) const
    {
        return x + 3;
    }
};

template <typename R, typename A, typename B>
struct times
{
    R operator()(A a, B b) const
    {
        return a * b;
    }
};

TEST(TestCTrampoline, Unary)
{
    pair<int(*)(int, void*), void*> a = make_c_trampoline<int(int)>(plus3());

    int x = a.first(17, a.second);
    EXPECT_EQ(20, x);

    delete_c_trampoline(a.second);
}

TEST(TestCTrampoline, Binary)
{
    pair<int(*)(int, int, void*), void*> a = make_c_trampoline<int(int, int)>(times<int, int, int>());

    int x = a.first(17, 2, a.second);
    EXPECT_EQ(34, x);

    delete_c_trampoline(a.second);
}

TEST(TestCTrampoline, Mixed)
{
    pair<double(*)(double, int, void*), void*> a = make_c_trampoline<double(double, int)>(times<double, double, int>());

    double x = a.first(17.1, 2, a.second);
    EXPECT_EQ(17.1 * 2, x);

    delete_c_trampoline(a.second);
}
