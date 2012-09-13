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
#include <ironbee/escape.h>

#include "ibtest_textbuf.hh"
#include "ibtest_strbase.hh"

#include "gtest/gtest.h"
#include "gtest/gtest-spi.h"

#include <stdexcept>

const size_t BufSize = 512;
const size_t CallBufSize = BufSize + 32;

class TestEscapeJSON : public TestSimpleStringManipulation
{
public:
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
                                        false,
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
                                        true,
                                        data_out, &dlen_out,
                                        &result);
    }
    ib_status_t ExecCopyNul(const char *data_in,
                            char **data_out,
                            ib_flags_t &result)
    {
        return ib_string_escape_json(m_mpool,
                                     data_in,
                                     data_out,
                                     &result);
    }
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
