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
/// @brief IronBee --- String Escape Util Test Functions
///
/// @author Nick LeRoy <nleroy@qualys.com>
//////////////////////////////////////////////////////////////////////////////

#include "ironbee_config_auto.h"

#include <ironbee/types.h>
#include <ironbee/string.h>
#include <ironbee/util.h>
#include <ironbee/mpool.h>
#include <ironbee/list.h>
#include <ironbee/escape.h>

#include "ibtest_textbuf.hh"
#include "ibtest_strbase.hh"
#include "simple_fixture.hh"

#include "gtest/gtest.h"
#include "gtest/gtest-spi.h"

#include <stdexcept>

const size_t BufSize = 512;
const size_t CallBufSize = BufSize + 32;

class TestEscapeJSON : public TestSimpleStringManipulation
{
public:
    TestEscapeJSON() : m_quote(false) { };

    void SetQuote(bool quote) { this->m_quote = quote; };

    const char *TestName(ib_strop_t op, test_type_t tt)
    {
        return TestNameImpl("escape_json", op, tt);
    }

    ib_status_t ExecCopyEx(const uint8_t *data_in,
                           size_t dlen_in,
                           uint8_t **data_out,
                           size_t &dlen_out,
                           ib_flags_t &result)
    {
        return ib_string_escape_json_ex(m_mpool,
                                        data_in, dlen_in,
                                        false, m_quote,
                                        (char **)data_out, &dlen_out,
                                        &result);
    }

    ib_status_t ExecCopyExToNul(const uint8_t *data_in,
                                size_t dlen_in,
                                char **data_out,
                                ib_flags_t &result)
    {
        size_t dlen_out;
        return ib_string_escape_json_ex(m_mpool,
                                        data_in, dlen_in,
                                        true, m_quote,
                                        data_out, &dlen_out,
                                        &result);
    }

    ib_status_t ExecCopyNul(const char *data_in,
                            char **data_out,
                            ib_flags_t &result)
    {
        return ib_string_escape_json(m_mpool,
                                     data_in,
                                     m_quote,
                                     data_out,
                                     &result);
    }

    ib_status_t ExecNulToNulBuf(const char *data_in,
                                char *data_out,
                                size_t dsize_out,
                                size_t &dlen_out,
                                ib_flags_t &result)
    {
        return ib_string_escape_json_buf(data_in, m_quote,
                                         data_out, dsize_out, &dlen_out,
                                         &result);
    }

    ib_status_t ExecExToNulBuf(const uint8_t *data_in,
                               size_t dlen_in,
                               char *data_out,
                               size_t dsize_out,
                               size_t &dlen_out,
                               ib_flags_t &result)
    {
        return ib_string_escape_json_buf_ex(data_in, dlen_in,
                                            true, m_quote,
                                            data_out, dsize_out, &dlen_out,
                                            &result);
    }
protected:
    bool m_quote;
};

TEST_F(TestEscapeJSON, Basic)
{
    {
        SCOPED_TRACE("Empty");
        RunTest("", "");
    }
    {
        SCOPED_TRACE("Basic #1");
        RunTest("TestCase", "TestCase");
    }
    {
        SCOPED_TRACE("Basic #2");
        RunTest("Test+Case", "Test+Case");
    }
}

TEST_F(TestEscapeJSON, Simple)
{
    {
        SCOPED_TRACE("Simple #1");
        RunTest("/", "\\/");
    }
    {
        SCOPED_TRACE("Simple #2");
        RunTest("\"", "\\\"");
    }
    {
        SCOPED_TRACE("Simple #3");
        RunTest("'", "'");
    }
    {
        SCOPED_TRACE("Simple #4");
        RunTest("\"", "\\\"");
    }
    {
        SCOPED_TRACE("Simple #5");
        RunTest("\\", "\\\\");
    }
    {
        SCOPED_TRACE("Simple #6");
        RunTest("\b", "\\b");
    }
    {
        SCOPED_TRACE("Simple #7");
        RunTest("\f", "\\f");
    }
    {
        SCOPED_TRACE("Simple #8");
        RunTest("\n", "\\n");
    }
    {
        SCOPED_TRACE("Simple #9");
        RunTest("\r", "\\r");
    }
    {
        SCOPED_TRACE("Simple #10");
        RunTest("\t", "\\t");
    }
    {
        SCOPED_TRACE("Simple #11");
        const uint8_t in[]  = "\0";
        const char    out[] = "\\u0000";
        RunTest(in, sizeof(in)-1, out);
    }
}

TEST_F(TestEscapeJSON, Quoted)
{
    SetQuote(true);
    {
        SCOPED_TRACE("Simple #1");
        RunTest("/", "\"\\/\"");
    }
    {
        SCOPED_TRACE("Simple #2");
        RunTest("\"", "\"\\\"\"");
    }
    {
        SCOPED_TRACE("Simple #3");
        RunTest("'", "\"'\"");
    }
    {
        SCOPED_TRACE("Simple #4");
        RunTest("\"", "\"\\\"\"");
    }
    {
        SCOPED_TRACE("Simple #5");
        RunTest("\\", "\"\\\\\"");
    }
    {
        SCOPED_TRACE("Simple #6");
        RunTest("\b", "\"\\b\"");
    }
    {
        SCOPED_TRACE("Simple #7");
        RunTest("\f", "\"\\f\"");
    }
    {
        SCOPED_TRACE("Simple #8");
        RunTest("\n", "\"\\n\"");
    }
    {
        SCOPED_TRACE("Simple #9");
        RunTest("\r", "\"\\r\"");
    }
    {
        SCOPED_TRACE("Simple #10");
        RunTest("\t", "\"\\t\"");
    }
    {
        SCOPED_TRACE("Simple #11");
        const uint8_t in[]  = "\0";
        const char    out[] = "\"\\u0000\"";
        RunTest(in, sizeof(in)-1, out);
    }
}

TEST_F(TestEscapeJSON, NonPrint)
{
    {
        SCOPED_TRACE("NonPrint #1");
        const uint8_t in[]  = "Test""\x001""Case";
        const char    out[] = "Test\\u0001Case";
        RunTest(in, sizeof(in)-1, out);
    }
    {
        SCOPED_TRACE("NonPrint #2");
        RunTest("x" "\x07f"   "\x080"   "\x0ff"   "y",
                "x" "\\u007f" "\\u0080" "\\u00ff" "y");
    }
    {
        int c;
        for (c = 1;  c <= 0xff;  ++c) {
            char inbuf[16];
            char outbuf[16];
            char testname[32];

            // Skip characters that are escaped specially
            if (isprint(c) || isspace(c) || (c == 0x08) ) {
                continue;
            }

            snprintf(testname, sizeof(testname), "NonPrint #3-%02x", c);
            SCOPED_TRACE(testname);

            strcpy(inbuf, "|x|");
            inbuf[1] = c;
            snprintf(outbuf, sizeof(outbuf), "|\\u%04x|", c);
            RunTest(inbuf, outbuf);
        }
    }
}

TEST_F(TestEscapeJSON, Complex)
{
    {
        SCOPED_TRACE("Complex #1");
        const uint8_t in[]  = "Test\0Case";
        const char    out[] = "Test\\u0000Case";
        RunTest(in, sizeof(in)-1, out);
    }
    {
        SCOPED_TRACE("Complex #2");
        RunTest("x\ty", "x\\ty");
    }
    {
        SCOPED_TRACE("Complex #3");
        RunTest("x\t\ty", "x\\t\\ty");
    }
    {
        SCOPED_TRACE("Complex #4");
        const uint8_t in[]  = "x\t\tfoo\0y";
        const char    out[] = "x\\t\\tfoo\\u0000y";
        RunTest(in, sizeof(in)-1, out);
    }
    {
        SCOPED_TRACE("Complex #5");
        RunTest("x\n\ry", "x\\n\\ry");
    }
}

TEST_F(TestEscapeJSON, FixedBuffer)
{
    {
        SCOPED_TRACE("FixedBuffer #1");
        RunTestBuf("x", "x", 1);
    }
    {
        SCOPED_TRACE("FixedBuffer #2");
        RunTestBuf("x", "x", 2);
    }
    {
        SCOPED_TRACE("FixedBuffer #3");
        RunTestBuf("xx", "xx", 2);
    }
    {
        SCOPED_TRACE("FixedBuffer #4");
        RunTestBuf("xx", "xx", 3);
    }
    {
        SCOPED_TRACE("FixedBuffer #5");
        RunTestBuf("/", "\\/", 1);
    }
    {
        SCOPED_TRACE("FixedBuffer #6");
        RunTestBuf("/", "\\/", 2);
    }
    {
        SCOPED_TRACE("FixedBuffer #7");
        RunTestBuf("/", "\\/", 3);
    }
    {
        SCOPED_TRACE("FixedBuffer #8");
        RunTestBuf("\"", "\\\"", 1);
    }
    {
        SCOPED_TRACE("FixedBuffer #9");
        RunTestBuf("\"", "\\\"", 2);
    }
}

class TestEscapeStrListJSON : public SimpleFixture
{
public:

    void RunTest(size_t bufsize,
                 ib_status_t expected_rc,
                 ib_flags_t expected_result,
                 const char *expected,
                 bool quote,
                 const char *join,
                 size_t num, ...)
    {
        va_list va;
        const char *s;
        ib_status_t rc;
        ib_list_t *slist;
        size_t n;

        rc = ib_list_create(&slist, m_pool);
        if (rc != IB_OK) {
            throw std::runtime_error("Error creating string list");
        }

        va_start(va, num);
        for (n = 0;  n < num;  ++n) {
            s = va_arg(va, char *);
            rc = ib_list_push(slist, (void *)s);
            if (rc != IB_OK) {
                throw std::runtime_error("Error creating string list");
            }
        }
        va_end(va);

        RunTest(slist, quote, join, bufsize,
                expected_rc, expected_result, expected);

    }

    void RunTest(const ib_list_t *slist,
                 bool quote,
                 const char *join,
                 size_t bufsize,
                 ib_status_t expected_rc,
                 ib_flags_t expected_result,
                 const char *expected)
    {
        char buf[bufsize];
        size_t len;
        ib_flags_t result;
        ib_status_t rc;

        rc = ib_strlist_escape_json_buf(slist, quote, join, buf, bufsize,
                                        &len, &result);
        ASSERT_EQ(expected_rc, rc);
        if (rc != IB_OK) {
            return;
        }
        ASSERT_EQ(expected_result, result);
        ASSERT_STREQ(expected, buf);
    }
};

TEST_F(TestEscapeStrListJSON, simple)
{
    {
        SCOPED_TRACE("NULL list");
        RunTest(NULL, false, "", 16, IB_OK, IB_STRFLAG_NONE, "");
    }
    {
        SCOPED_TRACE("Empty list");
        RunTest(16, IB_OK, IB_STRFLAG_NONE, "", false, "", 0);
    }
    {
        SCOPED_TRACE("List #1");
        RunTest(16, IB_OK, IB_STRFLAG_NONE, "x", false, "", 1, "x");
    }
    {
        SCOPED_TRACE("List #2");
        RunTest(16, IB_OK, IB_STRFLAG_NONE, "x", false, ",", 1, "x");
    }
    {
        SCOPED_TRACE("List #3");
        RunTest(16, IB_OK, IB_STRFLAG_NONE, "xy", false, "", 2, "x", "y");
    }
    {
        SCOPED_TRACE("List #4");
        RunTest(16, IB_OK, IB_STRFLAG_NONE, "x,y", false, ",", 2, "x", "y");
    }
    {
        SCOPED_TRACE("List #5");
        RunTest(16, IB_OK, IB_STRFLAG_NONE, "x, y", false, ", ", 2, "x", "y");
    }
    {
        SCOPED_TRACE("List #6");
        RunTest(16, IB_ETRUNC, IB_STRFLAG_MODIFIED,
                "aaaa,bbbb,cccc,dddd", false, ",",
                4, "aaaa", "bbbb", "cccc", "dddd");
    }
    {
        SCOPED_TRACE("List #7");
        RunTest(32, IB_OK, IB_STRFLAG_NONE,
                "aaaa,bbbb,cccc,dddd", false, ",",
                4, "aaaa", "bbbb", "cccc", "dddd");
    }
}

TEST_F(TestEscapeStrListJSON, quoted)
{
    {
        SCOPED_TRACE("NULL list");
        RunTest(NULL, true, "", 16, IB_OK, IB_STRFLAG_NONE, "");
    }
    {
        SCOPED_TRACE("Empty list");
        RunTest(16, IB_OK, IB_STRFLAG_NONE, "", true, "", 0);
    }
    {
        SCOPED_TRACE("List #1");
        RunTest(16, IB_OK, IB_STRFLAG_MODIFIED, "\"x\"", true, "", 1, "x");
    }
    {
        SCOPED_TRACE("List #2");
        RunTest(16, IB_OK, IB_STRFLAG_MODIFIED, "\"x\"", true, ",", 1, "x");
    }
    {
        SCOPED_TRACE("List #3");
        RunTest(16, IB_OK, IB_STRFLAG_MODIFIED, "\"x\"\"y\"", true, "",
                2, "x", "y");
    }
    {
        SCOPED_TRACE("List #4");
        RunTest(16, IB_OK, IB_STRFLAG_MODIFIED, "\"x\",\"y\"", true, ",",
                2, "x", "y");
    }
    {
        SCOPED_TRACE("List #5");
        RunTest(16, IB_OK, IB_STRFLAG_MODIFIED, "\"x\", \"y\"", true, ", ",
                2, "x", "y");
    }
    {
        SCOPED_TRACE("List #6");
        RunTest(16, IB_ETRUNC, IB_STRFLAG_MODIFIED,
                "\"aaaa\",\"bbbb\",\"cccc\",\"dddd\"", true, ",",
                4, "aaaa", "bbbb", "cccc", "dddd");
    }
    {
        SCOPED_TRACE("List #7");
        RunTest(32, IB_OK, IB_STRFLAG_MODIFIED,
                "\"aaaa\",\"bbbb\",\"cccc\",\"dddd\"", true, ",",
                4, "aaaa", "bbbb", "cccc", "dddd");
    }
}

TEST_F(TestEscapeStrListJSON, JSON)
{
    {
        SCOPED_TRACE("Simple #1");
        RunTest(16, IB_OK, IB_STRFLAG_MODIFIED, "a\\tb", false, "", 1, "a\tb");
    }
    {
        SCOPED_TRACE("Simple #2");
        RunTest(16, IB_OK, IB_STRFLAG_MODIFIED, "a\\tb,x\\ty",
                false, ",", 2, "a\tb", "x\ty");
    }
    {
        SCOPED_TRACE("Simple #3");
        RunTest(16, IB_ETRUNC, IB_STRFLAG_MODIFIED, "a\\tb, c\\nd, x\\ty",
                false, ", ", 3, "a\tb", "c\nd", "x\ty");
    }
    {
        SCOPED_TRACE("Simple #4");
        RunTest(32, IB_OK, IB_STRFLAG_MODIFIED, "a\\tb, c\\nd, x\\ty",
                false, ", ", 3, "a\tb", "c\nd", "x\ty");
    }
}
