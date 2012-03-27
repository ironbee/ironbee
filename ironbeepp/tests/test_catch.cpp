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
 * @brief IronBee++ Internals -- Catch Tests
 * @internal
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 **/

#include <ironbeepp/internal/catch.hpp>

#include <ironbee/types.h>

#include "gtest/gtest.h"

#include <ironbee/debug.h>

struct other_boost_exception : public boost::exception, std::exception {};

TEST(TestCatch, ironbeepp_exception)
{
    using namespace IronBee;
    EXPECT_EQ(IB_DECLINED,  IBPP_TRY_CATCH(NULL, throw declined()));
    EXPECT_EQ(IB_EUNKNOWN,  IBPP_TRY_CATCH(NULL, throw eunknown()));
    EXPECT_EQ(IB_ENOTIMPL,  IBPP_TRY_CATCH(NULL, throw enotimpl()));
    EXPECT_EQ(IB_EINCOMPAT, IBPP_TRY_CATCH(NULL, throw eincompat()));
    EXPECT_EQ(IB_EALLOC,    IBPP_TRY_CATCH(NULL, throw ealloc()));
    EXPECT_EQ(IB_EINVAL,    IBPP_TRY_CATCH(NULL, throw einval()));
    EXPECT_EQ(IB_ENOENT,    IBPP_TRY_CATCH(NULL, throw enoent()));
    EXPECT_EQ(IB_ETRUNC,    IBPP_TRY_CATCH(NULL, throw etrunc()));
    EXPECT_EQ(IB_ETIMEDOUT, IBPP_TRY_CATCH(NULL, throw etimedout()));
    EXPECT_EQ(IB_EAGAIN,    IBPP_TRY_CATCH(NULL, throw eagain()));
    EXPECT_EQ(IB_EBADVAL,   IBPP_TRY_CATCH(NULL, throw ebadval()));
    EXPECT_EQ(IB_EOTHER,    IBPP_TRY_CATCH(NULL, throw eother()));
}

TEST(TestCatch, boost_exception)
{
    EXPECT_EQ(
        IB_EUNKNOWN,
        IBPP_TRY_CATCH(NULL, throw other_boost_exception())
    );
}

TEST(TestCatch, std_exception)
{
    using namespace std;
    EXPECT_EQ(
        IB_EUNKNOWN,
        IBPP_TRY_CATCH(NULL, throw runtime_error(""))
    );
    EXPECT_EQ(
        IB_EINVAL,
        IBPP_TRY_CATCH(NULL, throw invalid_argument(""))
    );
}

TEST(TestCatch, bad_alloc)
{
    using namespace std;
    EXPECT_EQ(
        IB_EALLOC,
        IBPP_TRY_CATCH(NULL, throw std::bad_alloc())
    );
}
