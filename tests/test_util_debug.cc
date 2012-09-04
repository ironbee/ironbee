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

#ifdef IB_DEBUG

#include <ironbee/types.h>
#include <ironbee/util.h>

#include <stdio.h>

#include <string>
#include <map>
#include <stdexcept>
#include <iostream>
#include <fstream>

#include <boost/regex.hpp>


const size_t linebuf_size = 1024;

class TestIBUtilDebug : public testing::Test
{
public:
    TestIBUtilDebug() : log_fp(NULL)
    {
    }

    ~TestIBUtilDebug()
    {
        TearDown( );
    }

    virtual void SetUp()
    {
        Close( );
        log_fp = tmpfile();
        if (log_fp == NULL) {
            throw std::runtime_error("Failed to open tempory file.");
        }
        ib_trace_init_fp(log_fp);
    }

    virtual void TearDown()
    {
        Close( );
    }

    void Close( )
    {
        if (log_fp != NULL) {
            fclose(log_fp);
            log_fp = NULL;
            ib_trace_init_fp(stderr);
        }
    }

    const std::string &Cat( void )
    {
        fpos_t pos;

        if (fgetpos(log_fp, &pos) != 0) {
            throw std::runtime_error("Failed to get file position.");
        }
        rewind(log_fp);

        catbuf = "";
        while(fgets(linebuf, linebuf_size, log_fp) != NULL) {
            catbuf += linebuf;
        }
        if (fsetpos(log_fp, &pos) != 0) {
            throw std::runtime_error("Failed to set file position.");
        }
        return catbuf;
    }

    bool Grep(const std::string &pat)
    {
        boost::regex re = boost::regex(pat);
        bool match_found = false;
        fpos_t pos;

        if (fgetpos(log_fp, &pos) != 0) {
            throw std::runtime_error("Failed to get file position.");
        }
        rewind(log_fp);

        while(fgets(linebuf, linebuf_size, log_fp) != NULL) {
            bool result = boost::regex_search(linebuf, re);
            if(result) {
                match_found = true;
                break;
            }
        }
        if (fsetpos(log_fp, &pos) != 0) {
            throw std::runtime_error("Failed to set file position.");
        }

        return match_found;
    }

    bool Grep(const char *pat)
    {
        return Grep( std::string(pat) );
    }

    bool Grep(const std::string &p1, const std::string &p2)
    {
        std::string pat;
        pat = p1;
        pat += ".*";
        pat += p2;
        return Grep(pat);
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
    mutable std::string  catbuf;
    FILE                *log_fp;
    char                 linebuf[linebuf_size+1];
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
