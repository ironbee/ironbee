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
 * @brief IronBee++ Internals &mdash; Catch Tests
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 **/

#include <ironbeepp/internal/catch.hpp>
#include <ironbeepp/exception.hpp>

#include <ironbee/types.h>

#include "gtest/gtest.h"

#include <ironbee/debug.h>

struct other_boost_exception : public boost::exception, std::exception {};

namespace {

template <typename ExceptionType>
ib_status_t ibpp_test()
{
    try {
        throw ExceptionType();
    }
    catch (...) {
        return IronBee::Internal::convert_exception();
    }
}

template <typename ExceptionType>
ib_status_t ibpp_test_std()
{
    try {
        throw ExceptionType("");
    }
    catch (...) {
        return IronBee::Internal::convert_exception();
    }
}

}

TEST(TestCatch, ironbeepp_exception)
{
    using namespace IronBee;
    EXPECT_EQ(IB_DECLINED,  ibpp_test<declined>());
    EXPECT_EQ(IB_EUNKNOWN,  ibpp_test<eunknown>());
    EXPECT_EQ(IB_ENOTIMPL,  ibpp_test<enotimpl>());
    EXPECT_EQ(IB_EINCOMPAT, ibpp_test<eincompat>());
    EXPECT_EQ(IB_EALLOC,    ibpp_test<ealloc>());
    EXPECT_EQ(IB_EINVAL,    ibpp_test<einval>());
    EXPECT_EQ(IB_ENOENT,    ibpp_test<enoent>());
    EXPECT_EQ(IB_ETRUNC,    ibpp_test<etrunc>());
    EXPECT_EQ(IB_ETIMEDOUT, ibpp_test<etimedout>());
    EXPECT_EQ(IB_EAGAIN,    ibpp_test<eagain>());
    EXPECT_EQ(IB_EBADVAL,   ibpp_test<ebadval>());
    EXPECT_EQ(IB_EEXIST,    ibpp_test<eexist>());
    EXPECT_EQ(IB_EOTHER,    ibpp_test<eother>());
}

TEST(TestCatch, boost_exception)
{
    EXPECT_EQ(
        IB_EUNKNOWN,
        ibpp_test<other_boost_exception>()
    );
}

TEST(TestCatch, std_exception)
{
    using namespace std;
    EXPECT_EQ(
        IB_EUNKNOWN,
        ibpp_test_std<runtime_error>()
    );
    EXPECT_EQ(
        IB_EINVAL,
        ibpp_test_std<invalid_argument>()
    );
}

TEST(TestCatch, bad_alloc)
{
    using namespace std;
    EXPECT_EQ(
        IB_EALLOC,
        ibpp_test<std::bad_alloc>()
    );
}
