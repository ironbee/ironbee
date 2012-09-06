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
/// @brief IronBee &clock; Debug utility tests
///
/// @author Nick LeRoy <nleroy@qualys.com>
//////////////////////////////////////////////////////////////////////////////

#include <ironbee/debug.h>

#include "ironbee_config_auto.h"

#include "gtest/gtest.h"
#include "gtest/gtest-spi.h"
#include "ibtest_log_fixture.hh"

#ifdef IB_DEBUG

#include <ironbee/types.h>
#include <ironbee/util.h>

#include <stdio.h>

#include <string>
#include <stdexcept>

class TestIBUtilDebug : public IBLogFixture
{
public:
    TestIBUtilDebug() : IBLogFixture()
    {
    }

    ~TestIBUtilDebug()
    {
    }

    void SetUp()
    {
        IBLogFixture::SetUp();
        ib_trace_init_fp(log_fp);
    }

    void TearDown()
    {
        IBLogFixture::TearDown();
    }

    void ClosedFp( )
    {
        ib_trace_init_fp(stderr);
    }

    bool GrepCurFn(void)
    {
        return Grep( FunctionName() );
    }

    bool GrepCurFn(const char *pat)
    {
        return Grep(FunctionName(), std::string(pat));
    }

    void SetFunctionName(const char *fn = NULL)
    {
        if (fn == NULL) {
            fn = IB_CURRENT_FUNCTION;
        }
        cur_fn = fn;
    };
    const std::string &FunctionName( void ) const { return cur_fn; };
    const char *FunctionNameC( void ) const { return cur_fn.c_str(); };

protected:
    std::string          cur_fn;
};

TEST_F(TestIBUtilDebug, trace_msg)
{
    ib_trace_msg(__FILE__, __LINE__, "test_msg()", "Test 1");
    ASSERT_TRUE ( Grep("test_msg\\(\\)") );
    ASSERT_TRUE ( Grep("Test 1") );
    ASSERT_TRUE ( Grep("test_msg\\(\\)", "Test 1") );
    ASSERT_FALSE( Grep("Test 2") );
}

TEST_F(TestIBUtilDebug, trace_function)
{
    SetFunctionName("trace_fuction");
    ib_trace_msg(__FILE__, __LINE__, FunctionNameC(), "here");
    ASSERT_TRUE ( GrepCurFn() ) << Cat() << std::endl;
    ASSERT_TRUE ( Grep("here") ) << Cat() << std::endl;
    ASSERT_FALSE( GrepCurFn("Num 666") ) << Cat() << std::endl;
}

TEST_F(TestIBUtilDebug, trace_ptr)
{
    const size_t bufsize = 64;
    char buf[bufsize + 1];

    SetFunctionName("trace_ptr");
    ib_trace_ptr(__FILE__, __LINE__, FunctionNameC(), "Ptr:", &buf[0]);
    ASSERT_TRUE( GrepCurFn() ) << Cat() << std::endl;
    snprintf(buf, bufsize, "Ptr: %p", &buf[0]);
    ASSERT_TRUE( Grep(buf) ) << Cat() << std::endl;
    ASSERT_TRUE( GrepCurFn(buf) ) << Cat() << std::endl;
}

TEST_F(TestIBUtilDebug, trace_status_ok)
{
    const char *str;

    SetFunctionName("trace_status");

    ib_trace_status(__FILE__, __LINE__, FunctionNameC(), "Status:", IB_OK);
    str = "Status: OK";
    ASSERT_TRUE( GrepCurFn() ) << Cat() << std::endl;
    ASSERT_TRUE( Grep(str) ) << Cat() << std::endl;
    ASSERT_TRUE( GrepCurFn(str) ) << Cat() << std::endl;

    ib_trace_status(__FILE__, __LINE__, FunctionNameC(), "Status:", IB_ENOENT);
    str = "Status: ENOENT";
    ASSERT_TRUE( GrepCurFn() ) << Cat() << std::endl;
    ASSERT_TRUE( Grep(str) ) << Cat() << std::endl;
    ASSERT_TRUE( GrepCurFn(str) ) << Cat() << std::endl;
}

#else

TEST(TestDebug, test_not_supported)
{
    printf("Test not supported (IB_DEBUG disabled)\n");
}

#endif
