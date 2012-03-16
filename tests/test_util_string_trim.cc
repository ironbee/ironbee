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

const size_t BufSize = 64;
const size_t CallBufSize = BufSize + 32;

// Basic text buffer
class TextBuf
{
public:
    TextBuf(size_t bufsize)
        : m_size(bufsize), m_buf(new char [bufsize+1]),
          m_fmtsize(2*bufsize), m_fmtbuf(new char [m_fmtsize+1])
    {
        SetStr("");
    };

    TextBuf(size_t bufsize, const char *s)
        : m_size(bufsize), m_buf(new char [bufsize+1]),
          m_fmtsize(2*bufsize), m_fmtbuf(new char [m_fmtsize+1])
    {
        SetStr(s);
    };

    TextBuf(size_t bufsize, const char *text, size_t len)
        : m_size(bufsize), m_buf(new char [bufsize]),
          m_fmtsize(2*bufsize), m_fmtbuf(new char [bufsize*2])
    {
        SetText(text, len);
    };

    size_t SetNull( bool is_bytestr )
    {
        m_buf[0] = '\0';
        m_len = 0;
        m_null = true;
        m_bytestr = is_bytestr;
        InvalidateFmt( );
        return m_len;
    }

    size_t SetStr(const char *s, bool is_bytestr=false)
    {
        if (s == NULL) {
            return SetNull( false );
        }
        strncpy(m_buf, s, m_size);
        m_len = strlen(m_buf);
        m_null = false;
        m_bytestr = is_bytestr;
        InvalidateFmt( );
        return m_len;
    };

    size_t SetText(const char *text, size_t len)
    {
        assert (len < m_size);
        if (text == NULL) {
            return SetNull( true );
        }
        memcpy(m_buf, text, len);
        m_len = len;
        m_buf[len] = '\0';
        m_bytestr = true;
        InvalidateFmt( );
        return m_len;
    };

    void InvalidateFmt(void) const { m_fmtvalid = false; };
    bool IsFmtValid(void) const { return m_fmtvalid; };

    void SetByteStr(bool is_bytestr) { m_bytestr = is_bytestr; };
    bool IsByteStr(void) const { return m_bytestr; };
    bool IsNull(void) const { return m_null; };

    virtual const char *GetBuf(void) const {
        return m_null == true ? NULL : m_buf;
    };
    virtual size_t GetLen(void) const { return m_len; };
    virtual const char *GetFmt(void) const {
        if (IsFmtValid() == false) {
            if (m_bytestr) {
                return BuildFmt(m_buf, m_len);
            }
            else {
                return BuildFmt(m_buf);
            }
        }
        return m_fmtbuf;
    };

    bool operator == (const TextBuf &other) const {
        assert(other.IsByteStr() == IsByteStr());
        if (IsNull() || other.IsNull() ) {
            return false;
        }
        if (IsByteStr()) {
            if (GetLen() != other.GetLen()) {
                return false;
            }
            return (memcmp(GetBuf(), other.GetBuf(), GetLen()) == 0);
        }
        else {
            return (strcmp(GetBuf(), other.GetBuf()) == 0);
        }
    };


    bool operator != (const TextBuf &other) const {
        return !(*this == other);
    };

    const char *BuildFmt(const char *str) const {
        return BuildFmt(str, strlen(str));
    };

    const char *BuildFmt(const char *str, size_t len) const {
        char *buf = m_fmtbuf;
        const char *end = m_fmtbuf + m_fmtsize;
        size_t n = 0;
        while ( (n < len) && (buf < end) ) {
            if (*str == '\n') {
                *buf++ = '\\';
                *buf++ = 'n';
            }
            else if (*str == '\t') {
                *buf++ = '\\';
                *buf++ = 't';
            }
            else if (*str == '\0') {
                *buf++ = '\\';
                *buf++ = '0';
            }
            else {
                *buf++ = *str;
            }
            ++str;
            ++n;
        }
        *buf = '\0';
        m_fmtvalid = true;
        return m_fmtbuf;
    };

protected:
    size_t        m_size;
    char         *m_buf;
    bool          m_bytestr;
    bool          m_null;
    size_t        m_len;
    size_t        m_fmtsize;
    // The formatted buffer is mutable
    mutable char *m_fmtbuf;
    mutable bool  m_fmtvalid;

};


// Expected output buffer
class ExTextBuf : public TextBuf
{
public:
    ExTextBuf(size_t lno, const char *s)
        : TextBuf(BufSize, s),
          m_lineno(lno), m_cutleft(0), m_cutright(0), m_choplen(0) {
    };

    ExTextBuf(size_t lno, const char *s, size_t len)
        : TextBuf(BufSize, s, len),
          m_lineno(lno), m_cutleft(0), m_cutright(0), m_choplen(0) {
    };

    ExTextBuf(size_t lno, const char *s, size_t skip, size_t cut)
        : TextBuf(BufSize, s),
          m_lineno(lno), m_cutleft(skip), m_cutright(cut), m_choplen(0) {
    };

    ExTextBuf(size_t lno, const char *s, size_t len, size_t skip, size_t cut)
        : TextBuf(BufSize, s, len),
          m_lineno(lno), m_cutleft(skip), m_cutright(cut), m_choplen(0) {
    };

    const char *GetBuf(void) const { return m_chopbuf; };
    size_t GetLen(void) const { return m_choplen; };
    const char *GetFmt(void) const {
        if (IsFmtValid() == false) {
            if (m_bytestr) {
                BuildFmt(m_chopbuf, m_choplen);
            }
            else {
                BuildFmt(m_chopbuf);
            }
        }
        return m_fmtbuf;
    };

    bool BuildChopBuf(bool skip, bool cut) const {
        m_choplen = m_len;
        const char *start = m_buf;
        if (skip == true) {
            assert(m_choplen >= m_cutleft);
            m_choplen -= m_cutleft;
            start += m_cutleft;
        }
        if (cut == true) {
            assert(m_choplen >= m_cutright);
            m_choplen -= m_cutright;
        }
        memcpy(m_chopbuf, start, m_choplen);
        m_chopbuf[m_choplen] = '\0';
        InvalidateFmt( );
        return (m_choplen != m_len);
    };

protected:
    size_t          m_lineno;      // Test's line number
    size_t          m_cutleft;     // Bytes to cut off the left
    size_t          m_cutright;    // Bytes to cut off the right
    mutable char    m_chopbuf[BufSize+1];
    mutable size_t  m_choplen;
};


// Single test data point
class TestDatum
{
public:
    TestDatum(void)
        : m_end(true),
          m_lineno(0),
          m_inbuf(BufSize, ""),
          m_exvalid(false),
          m_exconst(true),
          m_exout(0, "")
    { };

    TestDatum(size_t lno, const char *in, const char *exout)
        : m_end(false),
          m_lineno(lno),
          m_inbuf(BufSize, in),
          m_exvalid(false),
          m_exconst(true),
          m_exout(lno, exout),
          m_exmod(strcmp(in,exout) == 0 ? IB_FALSE : IB_TRUE)
    { };

    TestDatum(size_t lno, const char *in, size_t skip, size_t cut)
        : m_end(false),
          m_lineno(lno),
          m_inbuf(BufSize, in),
          m_exvalid(false),
          m_exconst(false),
          m_exout(lno, in, skip, cut)
    {
    };

    // Ex version
    TestDatum(size_t lno,
              const char *in, size_t inlen,
              size_t skip, size_t cut)
        : m_end(false),
          m_lineno(lno),
          m_inbuf(BufSize, in, inlen),
          m_exvalid(false),
          m_exconst(false),
          m_exout(lno, in, inlen, skip, cut)
    {
    };

    // Accessors
    size_t LineNo(void) const { return m_lineno; };
    bool IsEnd(void) const { return m_end; };
    void ClearExpected(void) const { m_exvalid = false; };
    void SetByteStr(bool is_bytestr) const {
        m_exout.SetByteStr(is_bytestr);
    };
    const TextBuf &Inbuf(void) const { return m_inbuf; };
    const ExTextBuf &ExpectedOut(void) const {
        assert (m_exvalid == true);
        return m_exout;
    };
    ib_bool_t ExpectedMod(void) const {
        assert (m_exvalid == true);
        return m_exmod;
    }
    ib_bool_t BuildChopBuf(bool skip, bool cut) const {
        if (m_exconst) {
            m_exvalid = true;
        }
        if (m_exvalid != true) {
            bool mod = m_exout.BuildChopBuf(skip, cut);
            m_exmod = (mod == true) ? IB_TRUE : IB_FALSE;
            m_exvalid = true;
        }
        return m_exmod;
    };

private:
    bool               m_end;         // Special case: End marker.
    size_t             m_lineno;      // Test's line number
    TextBuf            m_inbuf;       // Input text buffer
    mutable bool       m_exvalid;     // Expected output valid?
    mutable bool       m_exconst;     // Expected output constant?
    mutable ExTextBuf  m_exout;       // Expected output text
    mutable ib_bool_t  m_exmod;       // Expected output modified?
};


// Formatted call text buffer
class CallTextBuf : public TextBuf
{
public:
    CallTextBuf(const char *fn) : TextBuf(CallBufSize, fn) { };
    
    const char *Stringize(const TestDatum &datum)
    {
        snprintf(m_fmtbuf, m_size,
                 "%s(\"%s\", ...)", m_buf, datum.Inbuf().GetFmt() );
        return m_fmtbuf;
    };
};

class TestIBUtilStrTrimBase : public ::testing::Test
{
public:
    TestIBUtilStrTrimBase(const char *fn)
        : m_callbuf(fn)
    { }

    ib_bool_t BuildExpBuf(const TestDatum &test) const {
        return test.BuildChopBuf(ChopLeft(), ChopRight());
    }

    // Specify these for specific tests
    virtual bool ChopLeft(void) const = 0;
    virtual bool ChopRight(void) const = 0;
    virtual const char *FnName(void) const = 0;

    const char *BoolStr(ib_bool_t v) {
        return (v == IB_TRUE ? "IB_TRUE" : "IB_FALSE");
    }

    const char *Stringize(const TestDatum &test) {
        return m_callbuf.Stringize(test);
    }

    void RunTests( const TestDatum test_data[] ) {
        const TestDatum *test;
        ib_status_t rc;
        for (test = test_data;  test->IsEnd() == false;  ++test) {
            ib_bool_t modified;
            test->ClearExpected();
            test->SetByteStr( IsByteStr() );
            rc = RunTest(*test, modified);
            CheckResults(*test, rc, modified);
        }
    }

    // Specify these for str/ex versions
    virtual bool IsByteStr(void) const = 0;
    virtual ib_status_t RunTest(const TestDatum &test,
                                ib_bool_t &modified) = 0;
    virtual void CheckResults(const TestDatum &test,
                              ib_status_t rc,
                              ib_bool_t modified) = 0;

protected:
    CallTextBuf       m_callbuf;       // Call string buffer
};


// Base string version of trim tests
class TestIBUtilStrTrimStr : public TestIBUtilStrTrimBase
{
public:
    TestIBUtilStrTrimStr(const char *fn)
        : TestIBUtilStrTrimBase(fn),
          m_inbuf(new char[BufSize+1]),
          m_outbuf(BufSize)
    {
    }

    ~TestIBUtilStrTrimStr()
    {
        delete [] m_inbuf;
    }

    // This is test specific
    virtual ib_status_t RunTestFn(char *in,
                                  char **out,
                                  ib_bool_t *modified) = 0;


    virtual bool IsByteStr(void) const { return false; };
    virtual ib_status_t RunTest(const TestDatum &test, ib_bool_t &modified)
    {
        char *out = NULL;
        ib_status_t rc;
        
        strncpy(m_inbuf, test.Inbuf().GetBuf(), BufSize);
        rc = RunTestFn(m_inbuf, &out, &modified);
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
        ib_bool_t exmod;

        EXPECT_EQ(IB_OK, rc)
            << "Line " << lno << ": " << Stringize(test) << " returned " << rc;
        if (rc != IB_OK) {
            return;
        }

        // Build the expected output
        exmod = BuildExpBuf(test);
        EXPECT_EQ(exmod, modified)
            << "Line " << lno << ": " << Stringize(test)
            << " expected modified=" << BoolStr(exmod)
            << " actual=" << BoolStr(modified);

        EXPECT_STRNE(NULL, out)
            << "Line " << lno << ": "
            << Stringize(test)
            << " Data out is NULL";

        if (out != NULL) {
            const char *exbuf = test.ExpectedOut().GetBuf();
            EXPECT_STREQ(exbuf, out)
                << "Line " << lno << ": " << Stringize(test)
                << " expected=\"" << test.ExpectedOut().GetFmt()
                << " actual=\""   << m_outbuf.GetFmt() << "\"";
        }
    }

protected:
    char      *m_inbuf;         // Copy of input
    TextBuf    m_outbuf;        // Output buffer
};

// Left trim tests
class TestIBUtilStrTrimLeft : public TestIBUtilStrTrimStr
{
public:
    TestIBUtilStrTrimLeft( ) : TestIBUtilStrTrimStr( FnName() )
    { };

    ib_status_t RunTestFn(char *in, char **out, ib_bool_t *modified) {
        return ::ib_strtrim_left(in, out, modified);
    }
    bool ChopLeft(void) const { return true; };
    bool ChopRight(void) const { return false; };
    const char *FnName(void) const { return "ib_strtrim_left"; };
};

// Right trim tests
class TestIBUtilStrTrimRight : public TestIBUtilStrTrimStr
{
public:
    TestIBUtilStrTrimRight( ) : TestIBUtilStrTrimStr( FnName() )
    { };

    ib_status_t RunTestFn(char *in, char **out, ib_bool_t *modified) {
        return ::ib_strtrim_right(in, out, modified);
    }
    bool ChopLeft(void) const { return false; };
    bool ChopRight(void) const { return true; };
    const char *FnName(void) const { return "ib_strtrim_right"; };
};

// Left/Right trim tests
class TestIBUtilStrTrimLr : public TestIBUtilStrTrimStr
{
public:
    TestIBUtilStrTrimLr( ) : TestIBUtilStrTrimStr( FnName() )
    { };

    ib_status_t RunTestFn(char *in, char **out, ib_bool_t *modified) {
        return ::ib_strtrim_lr(in, out, modified);
    }
    bool ChopLeft(void) const { return true; };
    bool ChopRight(void) const { return true; };
    const char *FnName(void) const { return "ib_strtrim_lr"; };
};

// Base "ex" version of trim tests
class TestIBUtilStrTrimEx : public TestIBUtilStrTrimBase
{
public:
    TestIBUtilStrTrimEx(const char *fn)
        : TestIBUtilStrTrimBase(fn),
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

    virtual bool IsByteStr(void) const { return true; };
    ib_status_t RunTest(const TestDatum &test, ib_bool_t &modified)
    {
        uint8_t *out;
        size_t outlen;
        ib_status_t rc;

        m_inbuf.SetText(test.Inbuf().GetBuf(), test.Inbuf().GetLen());
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
        const char *out = m_outbuf.GetBuf();
        size_t outlen = m_outbuf.GetLen();
        ib_bool_t exmod;

        EXPECT_EQ(IB_OK, rc)
            << "Line " << lno << ": " << Stringize(test) << " returned " << rc;
        if (rc != IB_OK) {
            return;
        }

        // Build the expected output
        exmod = BuildExpBuf(test);
        EXPECT_EQ(exmod, modified)
            << "Line " << lno << ": " << Stringize(test)
            << " expected modified=" << BoolStr(exmod)
            << " actual=" << BoolStr(modified);

        EXPECT_STRNE(NULL, out)
            << "Line " << lno << ": "
            << Stringize(test)
            << " Data out is NULL";

        if (out != NULL) {
            const ExTextBuf &expected = test.ExpectedOut();
            size_t exlen = expected.GetLen();
            EXPECT_EQ(exlen, outlen)
                << "Line " << lno << ": " << Stringize(test)
                << " expected len=" << exlen
                << ", actual len=" << outlen;
            EXPECT_TRUE(expected == m_outbuf)
                << "Line " << lno << ": " << Stringize(test)
                << " expected=\"" << expected.GetFmt() << "\""
                << " actual=\""   << m_outbuf.GetFmt() << "\"";
        }
    }

protected:
    TextBuf  m_inbuf;         // Copy of input
    TextBuf  m_outbuf;        // Output buffer
};

// Left (_ex version) trim tests
class TestIBUtilStrTrimLeftEx : public TestIBUtilStrTrimEx
{
public:
    TestIBUtilStrTrimLeftEx( ) : TestIBUtilStrTrimEx( FnName() )
    { };

    bool ChopLeft(void) const { return true; };
    bool ChopRight(void) const { return false; };
    const char *FnName(void) const { return "ib_strtrim_left_ex"; };

    ib_status_t RunTestFn(uint8_t *in, size_t inlen,
                          uint8_t **out, size_t *outlen,
                          ib_bool_t *modified) {
        return ::ib_strtrim_left_ex(in, inlen, out, outlen, modified);
    }

};

// Right (_ex version) trim tests
class TestIBUtilStrTrimRightEx : public TestIBUtilStrTrimEx
{
public:
    TestIBUtilStrTrimRightEx( ) : TestIBUtilStrTrimEx( FnName() )
    { };

    bool ChopLeft(void) const { return false; };
    bool ChopRight(void) const { return true; };
    const char *FnName(void) const { return "ib_strtrim_right_ex"; };

    ib_status_t RunTestFn(uint8_t *in, size_t inlen,
                          uint8_t **out, size_t *outlen,
                          ib_bool_t *modified) {
        return ::ib_strtrim_right_ex(in, inlen, out, outlen, modified);
    }

};

// Left/Right (_ex version) trim tests
class TestIBUtilStrTrimLrEx : public TestIBUtilStrTrimEx
{
public:
    TestIBUtilStrTrimLrEx( ) : TestIBUtilStrTrimEx( FnName() )
    { };

    bool ChopLeft(void) const { return true; };
    bool ChopRight(void) const { return true; };
    const char *FnName(void) const { return "ib_strtrim_lr_ex"; };

    ib_status_t RunTestFn(uint8_t *in, size_t inlen,
                          uint8_t **out, size_t *outlen,
                          ib_bool_t *modified) {
        return ::ib_strtrim_lr_ex(in, inlen, out, outlen, modified);
    }

};

static TestDatum str_test_data [ ] =
{
    TestDatum(__LINE__, "",            ""),
    TestDatum(__LINE__, " ",           ""),
    TestDatum(__LINE__, "  ",          ""),
    TestDatum(__LINE__, "  \n",        ""),
    TestDatum(__LINE__, "\t  \n",      ""),

    TestDatum(__LINE__, "a",           0, 0),
    TestDatum(__LINE__, "ab",          0, 0),
    TestDatum(__LINE__, "ab:",         0, 0),

    TestDatum(__LINE__, "a ",          0, 1),
    TestDatum(__LINE__, "a   ",        0, 3),
    TestDatum(__LINE__, "ab   ",       0, 3),
    TestDatum(__LINE__, "ab  \n",      0, 3),

    TestDatum(__LINE__, "a",           0, 0),
    TestDatum(__LINE__, " a",          1, 0),
    TestDatum(__LINE__, "  a",         2, 0),
    TestDatum(__LINE__, "   ab",       3, 0),
    TestDatum(__LINE__, "  \nab",      3, 0),
                                       
    TestDatum(__LINE__, " a ",         1, 1),
    TestDatum(__LINE__, " a   ",       1, 3),
    TestDatum(__LINE__, " ab   ",      1, 3),
    TestDatum(__LINE__, " ab  \n",     1, 3),

    TestDatum(__LINE__, " a",          1, 0),
    TestDatum(__LINE__, "  a",         2, 0),
    TestDatum(__LINE__, " ab",         1, 0),
    TestDatum(__LINE__, " a b",        1, 0),
    TestDatum(__LINE__, " a b ",       1, 1),
    TestDatum(__LINE__, " a b c",      1, 0),
    TestDatum(__LINE__, "\ta b c",     1, 0),
    TestDatum(__LINE__, "\na b c",     1, 0),
    TestDatum(__LINE__, " \tabc",      2, 0),
    TestDatum(__LINE__, " \nabc",      2, 0),
    TestDatum(__LINE__, " \t abc",     3, 0),
    TestDatum(__LINE__, " \n abc",     3, 0),

    TestDatum(__LINE__, "a ",          0, 1),
    TestDatum(__LINE__, "a  ",         0, 2),
    TestDatum(__LINE__, "ab ",         0, 1),
    TestDatum(__LINE__, "a b ",        0, 1),
    TestDatum(__LINE__, " a b ",       1, 1),
    TestDatum(__LINE__, "a b c ",      0, 1),
    TestDatum(__LINE__, "a b c\t",     0, 1),
    TestDatum(__LINE__, "a b c\n",     0, 1),
    TestDatum(__LINE__, "abc \t",      0, 2),
    TestDatum(__LINE__, "abc \n",      0, 2),
    TestDatum(__LINE__, "abc \t ",     0, 3),
    TestDatum(__LINE__, "abc \n ",     0, 3),

    TestDatum(__LINE__, " a ",         1, 1),
    TestDatum(__LINE__, "  a  ",       2, 2),
    TestDatum(__LINE__, " ab ",        1, 1),
    TestDatum(__LINE__, " a b ",       1, 1),
    TestDatum(__LINE__, " a b c ",     1, 1),
    TestDatum(__LINE__, "\ta b c\t",   1, 1),
    TestDatum(__LINE__, "\na b c\n",   1, 1),
    TestDatum(__LINE__, "\t abc \t",   2, 2),
    TestDatum(__LINE__, "\n abc \n",   2, 2),
    TestDatum(__LINE__, " \t abc \t ", 3, 3),
    TestDatum(__LINE__, " \n abc \n ", 3, 3),

     // Terminator
    TestDatum(),
};

// The actual str tests
TEST_F(TestIBUtilStrTrimLeft, test_strtrim_left)
{
    RunTests( str_test_data );
}

TEST_F(TestIBUtilStrTrimRight, test_strtrim_right)
{
    RunTests( str_test_data );
}

TEST_F(TestIBUtilStrTrimLr, test_strtrim_lr)
{
    RunTests( str_test_data );
}

// Test the ex versions with normal strings
TEST_F(TestIBUtilStrTrimLeftEx, test_strtrim_left_strex)
{
}

TEST_F(TestIBUtilStrTrimRightEx, test_strtrim_right_strex)
{
    RunTests( str_test_data );
}

TEST_F(TestIBUtilStrTrimLrEx, test_strtrim_lr_strex)
{
    RunTests( str_test_data );
}

static TestDatum ex_test_data [ ] =
{
    TestDatum(__LINE__, "\0", 1,        0, 0),
    TestDatum(__LINE__, "\0 ", 2,       0, 1),
    TestDatum(__LINE__, " \0 ", 3,      1, 1),
    TestDatum(__LINE__, "  \0\n", 4,    2, 1),
    TestDatum(__LINE__, "\t \0 \n", 5,  2, 2),
                                        
    TestDatum(__LINE__, "a\0", 2,       0, 0),
    TestDatum(__LINE__, "ab\0", 3,      0, 0),
    TestDatum(__LINE__, "ab\0:", 4,     0, 0),
                                        
    TestDatum(__LINE__, "\0a ", 3,      0, 1),
    TestDatum(__LINE__, "a\0   ", 5,    0, 3),
    TestDatum(__LINE__, "a\0b   ", 6,   0, 3),
    TestDatum(__LINE__, "ab\0  \n", 6,  0, 3),
                                        
    TestDatum(__LINE__, "a\0", 2,       0, 0),
    TestDatum(__LINE__, " \0a", 3,      1, 0),
    TestDatum(__LINE__, "  a\0", 4,     2, 0),
    TestDatum(__LINE__, "   a\0b", 6,   3, 0),
    TestDatum(__LINE__, "  \nab\0", 6,  3, 0),
                                        
    TestDatum(__LINE__, " a\0 ", 4,     1, 1),
    TestDatum(__LINE__, " \0a ", 4,     1, 1),
    TestDatum(__LINE__, " a\0   ", 6,   1, 3),
    TestDatum(__LINE__, " a\0b   ", 7,  1, 3),
    TestDatum(__LINE__, " ab\0  \n", 7, 1, 3),
                                        
    TestDatum(__LINE__, " a \0", 4,     1, 0),
    TestDatum(__LINE__, "\0 a \0", 5,   0, 0),
    TestDatum(__LINE__, "\0 ab\0", 5,   0, 0),
    TestDatum(__LINE__, " \0a b\0", 6,  1, 0),
    TestDatum(__LINE__, " \0a b\0 ", 7, 1, 1),

     // Terminator
    TestDatum(),
};

// The actual bytestr tests
TEST_F(TestIBUtilStrTrimLeftEx, test_strtrim_left_ex)
{
    RunTests( ex_test_data );
}

TEST_F(TestIBUtilStrTrimRightEx, test_strtrim_right_ex)
{
    RunTests( ex_test_data );
}

TEST_F(TestIBUtilStrTrimLrEx, test_strtrim_lr_ex)
{
    RunTests( ex_test_data );
}
