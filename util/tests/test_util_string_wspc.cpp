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
/// @brief IronBee --- String Util Test Functions
///
/// @author Nick LeRoy <nleroy@qualys.com>
//////////////////////////////////////////////////////////////////////////////

#include "ironbee_config_auto.h"

#include <ironbee/types.h>
#include <ironbee/string.h>

#include "ibtest_textbuf.hpp"
#include "ibtest_strbase.hpp"

#include "gtest/gtest.h"

#include <stdexcept>

const size_t BufSize = 64;
const size_t CallBufSize = BufSize + 32;

// Single test data point
class TestDatum : public BaseTestDatum
{
public:
    TestDatum()
        : BaseTestDatum(),
          m_exbuf_remove(BufSize, ""),
          m_exbuf_compress(BufSize, "")
    { };

    TestDatum(size_t lno,
              const char *in,
              const char *exstr_remove,
              const char *exstr_compress)
        : BaseTestDatum(lno, BufSize, in),
          m_exbuf_remove(BufSize, exstr_remove),
          m_exbuf_compress(BufSize, exstr_compress)
    { };

    // Ex version
    TestDatum(size_t lno,
              const char *in, size_t inlen,
              const char *exstr_remove, size_t exlen_remove,
              const char *exstr_compress, size_t exlen_compress)
        : BaseTestDatum(lno, BufSize, in, inlen),
          m_exbuf_remove(BufSize, exstr_remove, exlen_remove),
          m_exbuf_compress(BufSize, exstr_compress, exlen_compress)
    {
    };

    // Accessors
    const TextBuf &ExpectedOutRemove() const { return m_exbuf_remove; };
    const TextBuf &ExpectedOutCompress() const { return m_exbuf_compress; };

private:
    TextBuf     m_exbuf_remove;   // Expected output text
    TextBuf     m_exbuf_compress; // Expected output text
};

class TestIBUtilStrWspcBase : public TestStringModification
{
public:
    TestIBUtilStrWspcBase(ib_strmod_fn_t fn, const char *fn_name,
                          ib_strmod_ex_fn_t ex_fn, const char *ex_fn_name)
        : TestStringModification(BufSize, CallBufSize, fn, fn_name,
                                 ex_fn, ex_fn_name)
    {
    }

    virtual const TextBuf &ExpectedOut(const TestDatum &test) const = 0;

    bool ExpectCowAliasRemove(bool mod) const
    {
        return false;
    }

    bool ExpectCowAliasCompress(bool mod) const
    {
        return true;
    }

    // Specify these for str/ex versions
    virtual void CheckResults(const TestDatum &test,
                              ib_status_t rc,
                              ib_flags_t result) = 0;

    void RunTests(ib_strop_t op, const TestDatum *test_data)
    {
        const TestDatum *test;
        ib_status_t rc;

        SetOp(op);
        for (test = test_data; ! test->IsEnd();  ++test) {
            ib_flags_t result;
            rc = RunTest(*test, result);
            CheckResults(*test, rc, result);
        }
    }
};

// Base string version of trim tests
class TestIBUtilStrWspcStr : public TestIBUtilStrWspcBase
{
public:
    TestIBUtilStrWspcStr(ib_strmod_fn_t fn, const char *fn_name)
        : TestIBUtilStrWspcBase(fn, fn_name, NULL, NULL)
    {
    }

    virtual void CheckResults(const TestDatum &test,
                              ib_status_t rc,
                              ib_flags_t result)
    {
        size_t lno = test.LineNo();
        const char *out = m_outbuf.GetBuf();
        const TextBuf &exout = ExpectedOut(test);
        bool exmod = (test.InBuf() != exout);
        ib_flags_t exresult = ExpectedResult(Op(), exmod);

        CheckResult(lno, test, rc, exresult, result);

        EXPECT_STRNE(NULL, out)
            << "Line " << lno << ": "
            << Stringize(test)
            << " Data out is NULL";

        if (out != NULL) {
            const char *exbuf = exout.GetBuf();
            EXPECT_STREQ(exbuf, out)
                << "Line " << lno << ": " << Stringize(test)
                << " expected=\"" << exout.GetFmt()
                << " actual=\""   << m_outbuf.GetFmt() << "\"";
        }
    }
};

// Remove whitespace tests
class TestIBUtilStrWspcRemove : public TestIBUtilStrWspcStr
{
public:
    TestIBUtilStrWspcRemove( ) :
        TestIBUtilStrWspcStr( ib_str_wspc_remove, FnName() )
    { };

    const char *FnName() const { return "ib_str_wspc_remove"; };
    const TextBuf &ExpectedOut(const TestDatum &test) const {
        return test.ExpectedOutRemove();
    };
};

// Right trim tests
class TestIBUtilStrWspcCompress : public TestIBUtilStrWspcStr
{
public:
    TestIBUtilStrWspcCompress( ) :
        TestIBUtilStrWspcStr( ib_str_wspc_compress, FnName() )
    { };

    const char *FnName() const { return "ib_str_wspc_compress"; };
    const TextBuf &ExpectedOut(const TestDatum &test) const {
        return test.ExpectedOutCompress();
    };
};

// Base "ex" version of trim tests
class TestIBUtilStrWspcEx : public TestIBUtilStrWspcBase
{
public:
    TestIBUtilStrWspcEx(ib_strmod_ex_fn_t fn, const char *fn_name) :
        TestIBUtilStrWspcBase(NULL, NULL, fn, fn_name)
    {
    }

    void CheckResults(const TestDatum &test,
                      ib_status_t rc,
                      ib_flags_t result)
    {
        const TextBuf &exout = ExpectedOut(test);
        bool exmod = (exout != test.InBuf());

        ib_flags_t exresult = ExpectedResult( Op(), exmod );

        CheckResult(test.LineNo(), test, rc, exresult, result);

        const char *out = m_outbuf.GetBuf();
        if (out != NULL) {
            size_t outlen = m_outbuf.GetLen();
            size_t exlen = exout.GetLen();
            EXPECT_EQ(exlen, outlen)
                << "Line " << test.LineNo() << ": " << Stringize(test)
                << " expected len=" << exlen
                << ", actual len=" << outlen;
            EXPECT_TRUE(exout == m_outbuf)
                << "Line " << test.LineNo() << ": " << Stringize(test)
                << " expected=\"" << exout.GetFmt() << "\""
                << " actual=\""   << m_outbuf.GetFmt() << "\"";
        }
    }
};

// Remove (_ex version) whitespace tests
class TestIBUtilStrWspcRemoveEx : public TestIBUtilStrWspcEx
{
public:
    TestIBUtilStrWspcRemoveEx( ) :
        TestIBUtilStrWspcEx( ib_str_wspc_remove_ex, FnName() )
    { };

    const char *FnName() const { return "ib_str_wspc_remove_ex"; };
    const TextBuf &ExpectedOut(const TestDatum &test) const {
        return test.ExpectedOutRemove();
    };
};

// Compress (_ex version) whitespace tests
class TestIBUtilStrWspcCompressEx : public TestIBUtilStrWspcEx
{
public:
    TestIBUtilStrWspcCompressEx( ) :
        TestIBUtilStrWspcEx( ib_str_wspc_compress_ex, FnName() )
    { };

    const char *FnName() const { return "ib_str_wspc_compress_ex"; };
    const TextBuf &ExpectedOut(const TestDatum &test) const {
        return test.ExpectedOutCompress();
    };
};

static TestDatum str_test_data [ ] =
{
    TestDatum(__LINE__, "",            "",        ""),
    TestDatum(__LINE__, " ",           "",        " "),
    TestDatum(__LINE__, "\n",          "",        " "),
    TestDatum(__LINE__, "\t",          "",        " "),
    TestDatum(__LINE__, "  ",          "",        " "),
    TestDatum(__LINE__, "  \n",        "",        " "),
    TestDatum(__LINE__, "\t  \n",      "",        " "),

    TestDatum(__LINE__, "a",           "a",       "a"),
    TestDatum(__LINE__, "ab",          "ab",      "ab"),
    TestDatum(__LINE__, "ab:",         "ab:",     "ab:"),

    TestDatum(__LINE__, "a ",          "a",       "a "),
    TestDatum(__LINE__, "a   ",        "a",       "a "),
    TestDatum(__LINE__, "ab   ",       "ab",      "ab "),
    TestDatum(__LINE__, "ab  \n",      "ab",      "ab "),

    TestDatum(__LINE__, "a",           "a",       "a"),
    TestDatum(__LINE__, " a",          "a",       " a"),
    TestDatum(__LINE__, "  a",         "a",       " a"),
    TestDatum(__LINE__, "   ab",       "ab",      " ab"),
    TestDatum(__LINE__, "  \nab",      "ab",      " ab"),

    TestDatum(__LINE__, " a ",         "a",       " a "),
    TestDatum(__LINE__, " a   ",       "a",       " a "),
    TestDatum(__LINE__, " ab   ",      "ab",      " ab "),
    TestDatum(__LINE__, " ab  \n",     "ab",      " ab "),

    TestDatum(__LINE__, " a",          "a",       " a"),
    TestDatum(__LINE__, "  a",         "a",       " a"),
    TestDatum(__LINE__, " ab",         "ab",      " ab"),
    TestDatum(__LINE__, " a b",        "ab",      " a b"),
    TestDatum(__LINE__, " a b ",       "ab",      " a b "),
    TestDatum(__LINE__, " a b c",      "abc",     " a b c"),
    TestDatum(__LINE__, "\ta b c",     "abc",     " a b c"),
    TestDatum(__LINE__, "\na b c",     "abc",     " a b c"),
    TestDatum(__LINE__, " \tabc",      "abc",     " abc"),
    TestDatum(__LINE__, " \nabc",      "abc",     " abc"),
    TestDatum(__LINE__, " \t abc",     "abc",     " abc"),
    TestDatum(__LINE__, " \n abc",     "abc",     " abc"),

    TestDatum(__LINE__, "a ",          "a",       "a "),
    TestDatum(__LINE__, "a  ",         "a",       "a "),
    TestDatum(__LINE__, "ab ",         "ab",      "ab "),
    TestDatum(__LINE__, "a b ",        "ab",      "a b "),
    TestDatum(__LINE__, " a b ",       "ab",      " a b "),
    TestDatum(__LINE__, "a b c ",      "abc",     "a b c "),
    TestDatum(__LINE__, "a b    c ",   "abc",     "a b c "),
    TestDatum(__LINE__, "a b c\t",     "abc",     "a b c "),
    TestDatum(__LINE__, "a b c\n",     "abc",     "a b c "),
    TestDatum(__LINE__, "abc \t",      "abc",     "abc "),
    TestDatum(__LINE__, "abc \n",      "abc",     "abc "),
    TestDatum(__LINE__, "abc \t ",     "abc",     "abc "),
    TestDatum(__LINE__, "abc \n ",     "abc",     "abc "),

    TestDatum(__LINE__, " a ",         "a",       " a "),
    TestDatum(__LINE__, "  a  ",       "a",       " a "),
    TestDatum(__LINE__, " ab ",        "ab",      " ab "),
    TestDatum(__LINE__, " a b ",       "ab",      " a b "),
    TestDatum(__LINE__, " a b c ",     "abc",     " a b c "),
    TestDatum(__LINE__, " a\nb c ",    "abc",     " a b c "),
    TestDatum(__LINE__, " a\tb c ",    "abc",     " a b c "),
    TestDatum(__LINE__, " a b\tc ",    "abc",     " a b c "),
    TestDatum(__LINE__, " a b\nc ",    "abc",     " a b c "),
    TestDatum(__LINE__, " a\tb\tc ",   "abc",     " a b c "),
    TestDatum(__LINE__, " a\nb\nc ",   "abc",     " a b c "),
    TestDatum(__LINE__, "\ta b c\t",   "abc",     " a b c "),
    TestDatum(__LINE__, "\na b c\n",   "abc",     " a b c "),
    TestDatum(__LINE__, "\t abc \t",   "abc",     " abc "),
    TestDatum(__LINE__, "\n abc \n",   "abc",     " abc "),
    TestDatum(__LINE__, " \t abc \t ", "abc",     " abc "),
    TestDatum(__LINE__, " \n abc \n ", "abc",     " abc "),

     // Terminator
    TestDatum(),
};

// The actual str tests
TEST_F(TestIBUtilStrWspcRemove, test_str_wspc_remove_inplace)
{
    RunTests(IB_STROP_INPLACE, str_test_data);
}
TEST_F(TestIBUtilStrWspcRemove, test_str_wspc_remove_copy)
{
    RunTests(IB_STROP_COPY, str_test_data);
}
TEST_F(TestIBUtilStrWspcRemove, test_str_wspc_remove_cow)
{
    RunTests(IB_STROP_COW, str_test_data);
}

TEST_F(TestIBUtilStrWspcCompress, test_str_wspc_compress_inplace)
{
    RunTests(IB_STROP_INPLACE, str_test_data);
}
TEST_F(TestIBUtilStrWspcCompress, test_str_wspc_compress_copy)
{
    RunTests(IB_STROP_COPY, str_test_data);
}
TEST_F(TestIBUtilStrWspcCompress, test_str_wspc_compress_cow)
{
    RunTests(IB_STROP_COW, str_test_data);
}

// Test the ex versions with normal strings
TEST_F(TestIBUtilStrWspcRemoveEx, test_str_wspc_remove_strex_inplace)
{
    RunTests(IB_STROP_INPLACE, str_test_data);
}
TEST_F(TestIBUtilStrWspcRemoveEx, test_str_wspc_remove_strex_copy)
{
    RunTests(IB_STROP_COPY, str_test_data);
}
TEST_F(TestIBUtilStrWspcRemoveEx, test_str_wspc_remove_strex_cow)
{
    RunTests(IB_STROP_COW, str_test_data);
}

TEST_F(TestIBUtilStrWspcCompressEx, test_str_wspc_compress_strex_inplace)
{
    RunTests(IB_STROP_INPLACE, str_test_data);
}
TEST_F(TestIBUtilStrWspcCompressEx, test_str_wspc_compress_strex_copy)
{
    RunTests(IB_STROP_COPY, str_test_data);
}
TEST_F(TestIBUtilStrWspcCompressEx, test_str_wspc_compress_strex_cow)
{
    RunTests(IB_STROP_COW, str_test_data);
}

static TestDatum ex_test_data [ ] =
{
    TestDatum(__LINE__, "\0", 1,        "\0", 1, "\0", 1),
    TestDatum(__LINE__, "\0 ", 2,       "\0", 1, "\0 ", 2),
    TestDatum(__LINE__, " \0 ", 3,      "\0", 1, " \0 ", 3),
    TestDatum(__LINE__, "  \0\n", 4,    "\0", 1, " \0 ", 3),
    TestDatum(__LINE__, "\t \0 \n", 5,  "\0", 1, " \0 ", 3),

    TestDatum(__LINE__, "a\0", 2,       "a\0", 2, "a\0", 2),
    TestDatum(__LINE__, "ab\0", 3,      "ab\0", 3, "ab\0", 3),
    TestDatum(__LINE__, "ab\0:", 4,     "ab\0:", 4, "ab\0:", 4),

    TestDatum(__LINE__, "\0a ", 3,      "\0a", 2, "\0a ", 3),
    TestDatum(__LINE__, "a\0   ", 5,    "a\0", 2, "a\0 ", 3),
    TestDatum(__LINE__, "a\0b   ", 6,   "a\0b", 3, "a\0b ", 4),
    TestDatum(__LINE__, "ab\0  \n", 6,  "ab\0", 3, "ab\0 ", 4),

    TestDatum(__LINE__, "a\0", 2,       "a\0", 2, "a\0", 2),
    TestDatum(__LINE__, " \0a", 3,      "\0a", 2, " \0a", 3),
    TestDatum(__LINE__, "  a\0", 4,     "a\0", 2, " a\0", 3),
    TestDatum(__LINE__, "   a\0b", 6,   "a\0b", 3, " a\0b", 4),
    TestDatum(__LINE__, "  \nab\0", 6,  "ab\0", 3, " ab\0", 4),

    TestDatum(__LINE__, " a\0 ", 4,     "a\0", 2, " a\0 ", 4),
    TestDatum(__LINE__, " \0a ", 4,     "\0a", 2, " \0a ", 4),
    TestDatum(__LINE__, " a\0   ", 6,   "a\0", 2, " a\0 ", 4),
    TestDatum(__LINE__, " a\0b   ", 7,  "a\0b", 3, " a\0b ", 5),
    TestDatum(__LINE__, " ab\0  \n", 7, "ab\0", 3, " ab\0 ", 5),

    TestDatum(__LINE__, " a \0", 4,     "a\0", 2, " a \0", 4),
    TestDatum(__LINE__, "\0 a \0", 5,   "\0a\0", 3, "\0 a \0", 5),
    TestDatum(__LINE__, "\0 ab\0", 5,   "\0ab\0", 4, "\0 ab\0", 5),
    TestDatum(__LINE__, " \0a b\0", 6,  "\0ab\0", 4, " \0a b\0", 6),
    TestDatum(__LINE__, " \0a b\0 ", 7, "\0ab\0", 4, " \0a b\0 ", 7),

     // Terminator
    TestDatum(),
};

// The actual bytestr tests
TEST_F(TestIBUtilStrWspcRemoveEx, test_str_wspc_remove_ex_inplace)
{
    RunTests(IB_STROP_INPLACE, ex_test_data);
}
TEST_F(TestIBUtilStrWspcRemoveEx, test_str_wspc_remove_ex_copy)
{
    RunTests(IB_STROP_COPY, ex_test_data);
}
TEST_F(TestIBUtilStrWspcRemoveEx, test_str_wspc_remove_ex_cow)
{
    RunTests(IB_STROP_COW, ex_test_data);
}

TEST_F(TestIBUtilStrWspcCompressEx, test_str_wspc_compress_ex_inplace)
{
    RunTests(IB_STROP_INPLACE, ex_test_data);
}
TEST_F(TestIBUtilStrWspcCompressEx, test_str_wspc_compress_ex_copy)
{
    RunTests(IB_STROP_COPY, ex_test_data);
}
TEST_F(TestIBUtilStrWspcCompressEx, test_str_wspc_compress_ex_cow)
{
    RunTests(IB_STROP_COW, ex_test_data);
}
