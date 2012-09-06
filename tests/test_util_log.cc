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

#include <ironbee/types.h>
#include <ironbee/util.h>

#include <stdio.h>

#include <string>
#include <stdexcept>

class TestIBUtilLog : public IBLogFixture
{
public:
    TestIBUtilLog() : IBLogFixture(), m_lines(0)
    {
    }

    ~TestIBUtilLog()
    {
    }

    void SetUp()
    {
        IBLogFixture::SetUp();
        SetLogger();
    }

    void TearDown()
    {
        IBLogFixture::TearDown();
    }

    void ClosedFp()
    {
        UnsetLogger();
    }

    void SetLogger()
    {
        ib_status_t rc = ib_util_log_logger(TestIBUtilLog::LoggerFn, this);
        if (rc != IB_OK) {
            throw std::runtime_error("Failed to set logger.");
        }
    }

    void UnsetLogger()
    {
        ib_status_t rc = ib_util_log_logger(NULL, NULL);
        if (rc != IB_OK) {
            throw std::runtime_error("Failed to clear logger.");
        }
    }

    int lines() const { return m_lines; };
    void ClearLines() { m_lines = 0; }

    bool GrepF(const char *pat)
    {
        return Grep( std::string(__FILE__), std::string(pat));
    }

private:
    static void LoggerFn(void *cbdata,
                         int level,
                         const char *file,
                         int line,
                         const char *fmt,
                         va_list ap)
    {
        TestIBUtilLog *self = (TestIBUtilLog *)cbdata;
        self->LoggerFn(level, file, line, fmt, ap);
    }

    void LoggerFn(int level,
                  const char *file,
                  int line,
                  const char *fmt,
                  va_list ap)
    {
        ++m_lines;
        if ( (file != NULL) && (line > 0) ) {
            fprintf(log_fp, "[%d] (%s:%d) ", level, file, line);
        }
        else {
            fprintf(log_fp, "[%d] ", level);
        }
        vfprintf(log_fp, fmt, ap);
        fputs("\n", log_fp);
    }
    
protected:
    int       m_lines;
};

static int log_lines = 0;
static void LoggerFn(void *cbdata,
                     int level,
                     const char *file,
                     int line,
                     const char *fmt,
                     va_list ap)
{
    ++log_lines;
}

TEST(TestIBUtilLogSet, set_logger)
{
    ib_status_t rc;

    ASSERT_EQ((ib_util_fn_logger_t)NULL, ib_util_get_log_logger());

    rc = ib_util_log_logger(LoggerFn, NULL);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_EQ((ib_util_fn_logger_t)LoggerFn, ib_util_get_log_logger());

    ib_util_log_ex(0, __FILE__, __LINE__, "Message %d", 1);
    ASSERT_EQ(1, log_lines);

    rc = ib_util_log_logger(NULL, NULL);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_EQ((ib_util_fn_logger_t)NULL, ib_util_get_log_logger());

    ib_util_log_ex(0, __FILE__, __LINE__, "Message %d", 1);
    ASSERT_EQ(1, log_lines);
}

TEST_F(TestIBUtilLog, basic)
{
    ASSERT_EQ(IB_OK, ib_util_log_level(1));

    ib_util_log_ex(1, __FILE__, __LINE__, "Message %d", 1);
    ASSERT_TRUE ( Grep ("Message 1") ) << Cat() << std::endl;
    ASSERT_FALSE( GrepF("Message 1") ) << Cat() << std::endl;
}

TEST_F(TestIBUtilLog, levels)
{
    ASSERT_EQ(IB_OK, ib_util_log_level(1));

    ib_util_log_ex(1, __FILE__, __LINE__, "Message %d", 1);
    ASSERT_TRUE ( Grep ("Message 1") ) << Cat() << std::endl;
    ASSERT_FALSE( GrepF("Message 1") ) << Cat() << std::endl;

    ib_util_log_ex(2, __FILE__, __LINE__, "Message %d", 2);
    ASSERT_FALSE( Grep ("Message 2") ) << Cat() << std::endl;
    ASSERT_FALSE( GrepF("Message 2") ) << Cat() << std::endl;

    ASSERT_EQ(IB_OK, ib_util_log_level(7));
    ib_util_log_ex(1, __FILE__, __LINE__, "Message %d", 3);
    ASSERT_TRUE ( Grep ("Message 3") ) << Cat() << std::endl;
    ASSERT_TRUE ( GrepF("Message 3") ) << Cat() << std::endl;

    ib_util_log_ex(2, __FILE__, __LINE__, "Message %d", 4);
    ASSERT_TRUE ( Grep ("Message 4") ) << Cat() << std::endl;
    ASSERT_TRUE ( GrepF("Message 4") ) << Cat() << std::endl;

    ib_util_log_ex(7, __FILE__, __LINE__, "Message %d", 5);
    ASSERT_TRUE ( Grep ("Message 5") ) << Cat() << std::endl;
    ASSERT_TRUE ( GrepF("Message 5") ) << Cat() << std::endl;

    ib_util_log_ex(9, __FILE__, __LINE__, "Message %d", 5);
    ASSERT_FALSE( Grep ("Message 6") ) << Cat() << std::endl;
    ASSERT_FALSE( GrepF("Message 6") ) << Cat() << std::endl;
}

TEST_F(TestIBUtilLog, log_error)
{
    ASSERT_EQ(IB_OK, ib_util_log_level(1));
    ib_util_log_error("Message %d", 1);
    ASSERT_FALSE( Grep("Message 1") ) << Cat() << std::endl;

    ASSERT_EQ(IB_OK, ib_util_log_level(3));
    ib_util_log_error("Message %d", 2);
    ASSERT_TRUE ( Grep("Message 2") ) << Cat() << std::endl;
}

TEST_F(TestIBUtilLog, log_debug)
{
    ASSERT_EQ(IB_OK, ib_util_log_level(1));
    ib_util_log_debug("Message %d", 1);
    ASSERT_FALSE( Grep("Message 1") ) << Cat() << std::endl;

    ASSERT_EQ(IB_OK, ib_util_log_level(9));
    ib_util_log_debug("Message %d", 2);
    ASSERT_TRUE ( GrepF("Message 2") ) << Cat() << std::endl;
}
