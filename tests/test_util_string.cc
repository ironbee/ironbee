//////////////////////////////////////////////////////////////////////////////
// Licensed to Qualys, Inc. (QUALYS) under one or more
// contributor license agreements.  See the NOTICE file distributed with
// this work for additional information regarding copyright ownership.
// QUALYS licenses this file to You under the Apache License, Version 2.0
// (the "License"); you may not use this file except in compliance with
// the License.  You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
/// @file
/// @brief IronBee - String Util Test Functions
/// 
/// @author Nick LeRoy <nleroy@qualys.com>
//////////////////////////////////////////////////////////////////////////////

#include "ironbee_config_auto.h"

#include <ironbee/types.h>
#include <ironbee/string.h>

#include "ironbee_util_private.h"

#include "gtest/gtest.h"
#include "gtest/gtest-spi.h"

#include <stdexcept>

class TestIBUtilStringToNum : public ::testing::Test
{
public:

    const char *BoolStr(ib_bool_t b)
    {
        return (b == IB_TRUE) ? "True" : "False";
    }

    void RunTest(const char *s,
                 ib_bool_t allow_hex,
                 ib_num_t expected)
    {
        RunTest(s, allow_hex, IB_OK, expected);
    }

    void RunTest(const char *s,
                 ib_bool_t allow_hex,
                 ib_status_t estatus)
    {
        ib_status_t rc;
        ib_num_t result;
        rc = ::string_to_num(s, allow_hex, &result);
        ASSERT_EQ(rc, estatus)
            << "Conversion of '" << s << "' hex=" << BoolStr(allow_hex)
            << "failed; rc=" << rc;
    }

    void RunTest(const char *s,
                 ib_bool_t allow_hex,
                 ib_status_t estatus,
                 ib_num_t expected)
    {
        ib_status_t rc;
        ib_num_t result;

        rc = ::string_to_num(s, allow_hex, &result);
        if (estatus == IB_OK) {
            EXPECT_EQ(rc, IB_OK)
                << "Conversion of '" << s << "' hex=" << BoolStr(allow_hex)
                << "failed; rc=" << rc;
        }
        else {
            EXPECT_EQ(rc, estatus)
                << "Conversion of '" << s << "' hex=" << BoolStr(allow_hex)
                << " expected status " << estatus << " returned "<< rc;
        }

        if (rc == IB_OK) {
            EXPECT_EQ(expected, result)
                << "Conversion of '" << s << "' hex=" << BoolStr(allow_hex)
                << " expected value=" << expected << " result="<<result;
        }
    }
};

/* -- Tests -- */

/// @test Test util string library - convert string to number : error detection
TEST_F(TestIBUtilStringToNum, test_string_to_num_errors)
{
    RunTest(" ",     IB_TRUE,  IB_EINVAL);
    RunTest(" ",     IB_FALSE, IB_EINVAL);
    RunTest("",      IB_TRUE,  IB_EINVAL);
    RunTest("",      IB_FALSE, IB_EINVAL);
    RunTest(":",     IB_TRUE,  IB_EINVAL);
    RunTest(":",     IB_FALSE, IB_EINVAL);
    RunTest("x",     IB_TRUE,  IB_EINVAL);
    RunTest("x",     IB_FALSE, IB_EINVAL);
    RunTest("0x",    IB_TRUE,  IB_EINVAL);
    RunTest("0x",    IB_FALSE, IB_EINVAL);
    RunTest("-" ,    IB_TRUE,  IB_EINVAL);
    RunTest("-",     IB_FALSE, IB_EINVAL);
    RunTest("+" ,    IB_TRUE,  IB_EINVAL);
    RunTest("+",     IB_FALSE, IB_EINVAL);
    RunTest("0" ,    IB_TRUE,  IB_OK);
    RunTest("0",     IB_FALSE, IB_OK);
    RunTest("0x0",   IB_TRUE,  IB_OK);
    RunTest("0x0",   IB_FALSE, IB_EINVAL);
    RunTest("-1",    IB_TRUE,  IB_OK);
    RunTest("-1",    IB_FALSE, IB_OK);
    RunTest("+1",    IB_TRUE,  IB_OK);
    RunTest("+1",    IB_FALSE, IB_OK);
    RunTest("01",    IB_TRUE,  IB_OK);
    RunTest("01",    IB_FALSE, IB_OK);
    RunTest("0x100", IB_FALSE, IB_EINVAL);
    RunTest("-0x1",  IB_TRUE,  IB_EINVAL);
    RunTest("-0x1",  IB_FALSE, IB_EINVAL);
    RunTest("+0x1",  IB_TRUE,  IB_EINVAL);
    RunTest("+0x1",  IB_FALSE, IB_EINVAL);
}

/// @test Test util string library - convert string to number : error detection
TEST_F(TestIBUtilStringToNum, test_string_to_num)
{
    RunTest("0",      IB_TRUE,  0);
    RunTest("0",      IB_FALSE, 0);
    RunTest("1",      IB_TRUE,  1);
    RunTest("1",      IB_FALSE, 1);
    RunTest("10",     IB_TRUE,  10);
    RunTest("10",     IB_FALSE, 10);
    RunTest("100",    IB_TRUE,  100);
    RunTest("100",    IB_FALSE, 100);
    RunTest("0x100",  IB_TRUE,  0x100);
    RunTest("0xf",    IB_TRUE,  0xf);
    RunTest("0xff",   IB_TRUE,  0xff);
    RunTest("0xffff", IB_TRUE,  0xffff);
    RunTest("+1",     IB_TRUE,  1);
    RunTest("+1",     IB_FALSE, 1);
    RunTest("-1",     IB_TRUE,  -1);
    RunTest("-1",     IB_FALSE, -1);
    RunTest("+0",     IB_TRUE,  0);
    RunTest("+0",     IB_FALSE, 0);
    RunTest("-0",     IB_TRUE,  0);
    RunTest("-0",     IB_FALSE, 0);
    RunTest("9",      IB_TRUE,  9);
    RunTest("9",      IB_FALSE, 9);
    RunTest("99999",  IB_TRUE,  99999);
    RunTest("99999",  IB_FALSE, 99999);
    RunTest("-99999", IB_TRUE,  -99999);
    RunTest("-99999", IB_FALSE, -99999);
    RunTest("+99999", IB_TRUE,  99999);
    RunTest("+99999", IB_FALSE, 99999);
}
