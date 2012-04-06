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

#include "ibtest_textbuf.hh"

#include "gtest/gtest.h"
#include "gtest/gtest-spi.h"

#include <stdexcept>

const size_t BufSize = 64;
const size_t CallBufSize = BufSize + 32;


// Single test data point
class TestDatum
{
public:
    TestDatum(void)
        : m_end(true),
          m_lineno(0),
          m_inbuf(BufSize, ""),
          m_exbuf(BufSize, "")
    { };

    // Constructor with in and out strings
    TestDatum(size_t lno, const char *in, const char *ex)
        : m_end(false),
          m_lineno(lno),
          m_inbuf(BufSize, in),
          m_exbuf(BufSize, ex)
    { };

    // Constructor with only input string
    TestDatum(size_t lno, const char *in)
        : m_end(false),
          m_lineno(lno),
          m_inbuf(BufSize, in),
          m_exbuf(BufSize, in)
    { };

    // Ex version
    TestDatum(size_t lno,
              const char *in, size_t inlen,
              const char *ex, size_t exlen)
        : m_end(false),
          m_lineno(lno),
          m_inbuf(BufSize, in, inlen),
          m_exbuf(BufSize, ex, exlen)
    {
    };

    // Ex version with only input
    TestDatum(size_t lno,
              const char *in, size_t inlen)
        : m_end(false),
          m_lineno(lno),
          m_inbuf(BufSize, in, inlen),
          m_exbuf(BufSize, in, inlen)
    {
    };

    // Accessors
    size_t LineNo(void) const { return m_lineno; };
    bool IsEnd(void) const { return m_end; };
    const TextBuf &InBuf(void) const { return m_inbuf; };
    const TextBuf &ExpectedBuf() const { return m_exbuf_remove; };

private:
    bool        m_end;            // Special case: End marker.
    size_t      m_lineno;         // Test's line number
    TextBuf     m_inbuf;          // Input text buffer
    TextBuf     m_exbuf;          // Expected output text
};


// Formatted call text buffer
class CallTextBuf : public TextBuf
{
public:
    CallTextBuf(const char *fn) : TextBuf(CallBufSize, fn) { };
    
    const char *Stringize(const TestDatum &datum)
    {
        snprintf(m_fmtbuf, m_size,
                 "%s(\"%s\", ...)", m_buf, datum.InBuf().GetFmt());
        return m_fmtbuf;
    };
};

class TestIBUtilStrLowerBase : public ::testing::Test
{
public:
    TestIBUtilStrLowerBase(const char *fn) : m_callbuf(fn) {
        ib_status_t rc = ib_mpool_create(&m_mpool, "Test", NULL);
        if (rc != IB_OK) {
            throw std::runtime_error("Could not create mpool.");
        }
    }

    const char *BoolStr(ib_bool_t v) {
        return (v == IB_TRUE ? "IB_TRUE" : "IB_FALSE");
    }
    const char *Stringize(const TestDatum &test) {
        return m_callbuf.Stringize(test);
    }

    virtual const TextBuf &ExpectedOut(const TestDatum &test) const = 0;
    virtual const char *FnName(void) const = 0;

    void RunTests(const TestDatum *test_data) {
        const TestDatum *test;
        ib_status_t rc;
        for (test = test_data;  test->IsEnd() == false;  ++test) {
            ib_bool_t modified;
            rc = RunTest(*test, modified);
            CheckResults(*test, rc, modified);
        }
    }

    // Specify these for str/ex versions
    virtual ib_status_t RunTest(const TestDatum &test,
                                ib_bool_t &modified) = 0;
    virtual void CheckResults(const TestDatum &test,
                              ib_status_t rc,
                              ib_bool_t modified) = 0;

protected:
    CallTextBuf    m_callbuf;       // Call string buffer
    ib_mpool_t    *m_mpool;         // Memory pool
};

// Base string version of trim tests
class TestIBUtilStrLowerStr : public TestIBUtilStrLowerBase
{
public:
    TestIBUtilStrLowerStr(const char *fn)
        : TestIBUtilStrLowerBase(fn),
          m_inbuf(new char[BufSize+1]),
          m_outbuf(BufSize)
    {
    }

    ~TestIBUtilStrLowerStr()
    {
        delete [] m_inbuf;
    }

    // This is test specific
    virtual ib_status_t RunTestFn(char *in,
                                  char **out,
                                  ib_flags_t *result) = 0;

    virtual ib_status_t RunTest(const TestDatum &test, ib_flags_t &result)
    {
        char *out = NULL;
        ib_status_t rc;
        
        strncpy(m_inbuf, test.InBuf().GetBuf(), BufSize);
        rc = RunTestFn(m_inbuf, &out, &result);
        if (rc == IB_OK) {
            m_outbuf.SetStr(out);
        }
        return rc;
    }

    virtual void CheckResults(const TestDatum &test,
                              ib_status_t rc,
                              ib_bool_t modified)
    {
        size_t lno = test.LineNo();
        const char *out = m_outbuf.GetBuf();
        const TextBuf &exout = ExpectedOut(test);
        ib_bool_t exmod;

        EXPECT_EQ(IB_OK, rc)
            << "Line " << lno << ": " << Stringize(test) << " returned " << rc;
        if (rc != IB_OK) {
            return;
        }

        // Expect change?
        exmod = (test.InBuf() == exout) ? IB_FALSE : IB_TRUE;
        EXPECT_EQ(exmod, modified)
            << "Line " << lno << ": " << Stringize(test)
            << " expected modified=" << BoolStr(exmod)
            << " actual=" << BoolStr(modified);

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

protected:
    char      *m_inbuf;         // Copy of input
    TextBuf    m_outbuf;        // Output buffer
};

// Base "ex" version of trim tests
class TestIBUtilStrLowerEx : public TestIBUtilStrLowerBase
{
public:
    TestIBUtilStrLowerEx(const char *fn)
        : TestIBUtilStrLowerBase(fn),
          m_inbuf(BufSize),
          m_outbuf(BufSize)
    {
    }

    // This is test specific
    virtual ib_status_t RunTestFn(uint8_t *in,
                                  size_t inlen,
                                  uint8_t **out,
                                  size_t *outlen,
                                  ib_bool_t *modified) = 0;

    ib_status_t RunTest(const TestDatum &test, ib_bool_t &modified)
    {
        uint8_t *out;
        size_t outlen;
        ib_status_t rc;

        m_inbuf.SetText(test.InBuf().GetBuf(), test.InBuf().GetLen());
        rc = RunTestFn((uint8_t *)m_inbuf.GetBuf(), m_inbuf.GetLen(),
                       &out, &outlen,
                       &modified);
        if (rc == IB_OK) {
            m_outbuf.SetText((char *)out, outlen);
        }
        return rc;
    }

    void CheckResults(const TestDatum &test,
                      ib_status_t rc,
                      ib_bool_t modified)
    {
        size_t lno = test.LineNo();
        const TextBuf &exout = ExpectedOut(test);
        const char *out = m_outbuf.GetBuf();
        size_t outlen = m_outbuf.GetLen();
        ib_bool_t exmod = (exout == test.InBuf()) ? IB_FALSE : IB_TRUE;

        EXPECT_EQ(IB_OK, rc)
            << "Line " << lno << ": " << Stringize(test) << " returned " << rc;
        if (rc != IB_OK) {
            return;
        }

        // Build the expected output
        EXPECT_EQ(exmod, modified)
            << "Line " << lno << ": " << Stringize(test)
            << " expected modified=" << BoolStr(exmod)
            << " actual=" << BoolStr(modified);

        EXPECT_STRNE(NULL, out)
            << "Line " << lno << ": "
            << Stringize(test)
            << " Data out is NULL";

        if (out != NULL) {
            size_t exlen = exout.GetLen();
            EXPECT_EQ(exlen, outlen)
                << "Line " << lno << ": " << Stringize(test)
                << " expected len=" << exlen
                << ", actual len=" << outlen;
            EXPECT_TRUE(exout == m_outbuf)
                << "Line " << lno << ": " << Stringize(test)
                << " expected=\"" << exout.GetFmt() << "\""
                << " actual=\""   << m_outbuf.GetFmt() << "\"";
        }
    }

protected:
    TextBuf  m_inbuf;         // Copy of input
    TextBuf  m_outbuf;        // Output buffer
};

// Remove (_ex version) whitespace tests
class TestIBUtilStrLowerRemoveEx : public TestIBUtilStrLowerEx
{
public:
    TestIBUtilStrLowerRemoveEx( ) : TestIBUtilStrLowerEx( FnName() )
    { };

    const char *FnName(void) const { return "ib_str_wspc_remove_ex"; };
    const TextBuf &ExpectedOut(const TestDatum &test) const {
        return test.ExpectedOutRemove();
    };
    ib_status_t RunTestFn(uint8_t *in, size_t inlen,
                          uint8_t **out, size_t *outlen,
                          ib_bool_t *modified) {
        return ::ib_str_wspc_remove_ex(m_mpool,
                                       in, inlen,
                                       out, outlen,
                                       modified);
    }

};

// Compress (_ex version) whitespace tests
class TestIBUtilStrLowerCompressEx : public TestIBUtilStrLowerEx
{
public:
    TestIBUtilStrLowerCompressEx( ) : TestIBUtilStrLowerEx( FnName() )
    { };

    const char *FnName(void) const { return "ib_str_wspc_compress_ex"; };
    const TextBuf &ExpectedOut(const TestDatum &test) const {
        return test.ExpectedOutCompress();
    };
    ib_status_t RunTestFn(uint8_t *in, size_t inlen,
                          uint8_t **out, size_t *outlen,
                          ib_bool_t *modified) {
        return ::ib_str_wspc_compress_ex(m_mpool,
                                         in, inlen,
                                         out, outlen,
                                         modified);
    }

};

static TestDatum str_test_data [ ] =
{
    TestDatum(__LINE__, "",            "")
    TestDatum(__LINE__, "a",           "a"),
    TestDatum(__LINE__, "ab",          "ab"),
    TestDatum(__LINE__, "ab:",         "ab:"),
    TestDatum(__LINE__, ":ab:",        ":ab:"),

    TestDatum(__LINE__, "a",           "a"),
    TestDatum(__LINE__, "ab",          "ab"),
    TestDatum(__LINE__, "ab:",         "ab:"),
    TestDatum(__LINE__, "ab:",         ":ab:"),

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
TEST_F(TestIBUtilStrLowerRemove, test_str_wspc_remove)
{
    RunTests( str_test_data );
}

TEST_F(TestIBUtilStrLowerCompress, test_str_wspc_compress)
{
    RunTests( str_test_data );
}

// Test the ex versions with normal strings
TEST_F(TestIBUtilStrLowerRemoveEx, test_str_wspc_remove_strex)
{
}

TEST_F(TestIBUtilStrLowerCompressEx, test_str_wspc_compress_strex)
{
    RunTests( str_test_data );
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
TEST_F(TestIBUtilStrLowerRemoveEx, test_str_wspc_remove_ex)
{
    RunTests( ex_test_data );
}

TEST_F(TestIBUtilStrLowerCompressEx, test_str_wspc_compress_ex)
{
    RunTests( ex_test_data );
}
