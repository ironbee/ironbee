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
#include <ironbee/util.h>

#include "ibtest_textbuf.hpp"
#include "ibtest_strbase.hpp"

#include "gtest/gtest.h"

#include <stdexcept>

const size_t BufSize = 64;
const size_t CallBufSize = BufSize + 32;

// Expected output buffer
class ExpectedTextBuf : public TextBuf
{
public:
    ExpectedTextBuf(size_t lno, const char *s)
        : TextBuf(BufSize, s),
          m_lineno(lno), m_cutleft(0), m_cutright(0), m_choplen(0) {
    };

    ExpectedTextBuf(size_t lno, const char *s, size_t len)
        : TextBuf(BufSize, s, len),
          m_lineno(lno), m_cutleft(0), m_cutright(0), m_choplen(0) {
    };

    ExpectedTextBuf(size_t lno, const char *s, size_t skip, size_t cut)
        : TextBuf(BufSize, s),
          m_lineno(lno), m_cutleft(skip), m_cutright(cut), m_choplen(0) {
    };

    ExpectedTextBuf(size_t lno, const char *s,
                    size_t len, size_t skip, size_t cut)
        : TextBuf(BufSize, s, len),
          m_lineno(lno), m_cutleft(skip), m_cutright(cut), m_choplen(0) {
    };

    char *GetBuf() const { return m_chopbuf; };
    size_t GetLen() const { return m_choplen; };
    const char *GetFmt() const {
        if (! IsFmtValid()) {
            if (m_bytestr) {
                BuildFmt(m_chopbuf, m_choplen);
            }
            else {
                BuildFmt(m_chopbuf);
            }
        }
        return m_fmtbuf;
    };

    bool BuildChopBuf(bool skip, bool cut) const
    {
        m_choplen = m_len;
        const char *start = m_buf;
        if (skip) {
            assert(m_choplen >= m_cutleft);
            m_choplen -= m_cutleft;
            start += m_cutleft;
        }
        if (cut) {
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
class TestDatum : public BaseTestDatum
{
public:
    TestDatum()
        : BaseTestDatum(),
          m_exvalid(false),
          m_exconst(true),
          m_exout(0, "")
    { };

    TestDatum(size_t lno, const char *in, const char *exout)
        : BaseTestDatum(lno, BufSize, in),
          m_exvalid(false),
          m_exconst(true),
          m_exout(lno, exout),
          m_exmod((strcmp(in,exout) != 0))
    { };

    TestDatum(size_t lno, const char *in, size_t skip, size_t cut)
        : BaseTestDatum(lno, BufSize, in),
          m_exvalid(false),
          m_exconst(false),
          m_exout(lno, in, skip, cut)
    {
    };

    // Ex version
    TestDatum(size_t lno,
              const char *in, size_t inlen,
              size_t skip, size_t cut)
        : BaseTestDatum(lno, BufSize, in, inlen),
          m_exvalid(false),
          m_exconst(false),
          m_exout(lno, in, inlen, skip, cut)
    {
    };

    // Accessors
    void ClearExpected() const { m_exvalid = false; };
    void SetByteStr(bool is_bytestr) const {
        m_exout.SetByteStr(is_bytestr);
    };
    const ExpectedTextBuf &ExpectedOut() const {
        assert (m_exvalid);
        return m_exout;
    };
    bool ExpectedMod() const {
        assert (m_exvalid);
        return m_exmod;
    }
    bool BuildChopBuf(bool skip, bool cut) const {
        if (m_exconst) {
            m_exvalid = true;
        }
        if (! m_exvalid) {
            bool mod = m_exout.BuildChopBuf(skip, cut);
            m_exmod = mod;
            m_exvalid = true;
        }
        return m_exmod;
    };

private:
    mutable bool            m_exvalid;     // Expected output valid?
    mutable bool            m_exconst;     // Expected output constant?
    mutable ExpectedTextBuf m_exout;       // Expected output text
    mutable bool            m_exmod;       // Expected output modified?
};


class TestStringTrim : public TestStringModification
{
public:
    TestStringTrim(ib_strmod_fn_t fn, const char *fn_name,
                   ib_strmod_ex_fn_t ex_fn, const char *ex_fn_name)
        : TestStringModification(BufSize, CallBufSize, fn, fn_name,
                                 ex_fn, ex_fn_name)
    {
    }

    bool ExpectCowAliasLeft(bool mod) const
    {
        bool ls = LeftSpace();
        bool as = AllSpace();

        if ( (! mod) || (as) || (ls) ) {
            return true;
        }
        else {
            return false;
        }
    }

    bool ExpectCowAliasRight(bool mod) const
    {
        bool rs = RightSpace();
        bool as = AllSpace();

        if ( (! mod) || (as) ) {
            return true;
        }
        else if ( (IsByteStr()) && (rs) ) {
            return true;
        }
        else {
            return false;
        }
    }

    bool ExpectCowAliasLr(bool mod) const
    {
        bool ls = LeftSpace();
        bool rs = RightSpace();
        bool as = AllSpace();

        if ( (! mod) || (as) ) {
            return true;
        }
        else if ( (IsByteStr()) && (rs) ) {
            return true;
        }
        else if ( ls && (! rs) ) {
            return true;
        }
        else {
            return false;
        }
    }

    ib_flags_t BuildExpBuf(ib_strop_t op, const TestDatum &test) const
    {
        bool left = ChopLeft();
        bool right = ChopRight();
        bool mod = test.BuildChopBuf(left, right);
        return ExpectedResult(op, mod);
    }

    // Specify these for specific tests
    virtual bool ChopLeft() const = 0;
    virtual bool ChopRight() const = 0;

    // Specify these for str/ex versions
    virtual bool IsByteStr() const = 0;

    // Specify these for str/ex versions
    virtual void CheckResults(const TestDatum &test,
                              ib_status_t rc,
                              ib_flags_t result) = 0;

    void RunTests( ib_strop_t op, const TestDatum test_data[] )
    {
        const TestDatum *test;
        ib_status_t rc;

        SetOp( op );
        for (test = test_data; ! test->IsEnd();  ++test) {
            ib_flags_t result;
            test->ClearExpected();
            test->SetByteStr( IsByteStr() );
            rc = RunTest(*test, result);
            CheckResults(*test, rc, result);
        }
    }
};


// Base string version of trim tests
class TestIBUtilStrTrim : public TestStringTrim
{
public:
    TestIBUtilStrTrim(ib_strmod_fn_t fn, const char *name)
        : TestStringTrim(fn, name, NULL, NULL)
    {
    }

    virtual bool IsByteStr() const { return false; };

    virtual void CheckResults(const TestDatum &test,
                              ib_status_t rc,
                              ib_flags_t result)
    {
        size_t lno = test.LineNo();
        const char *out = m_outbuf.GetBuf();
        ib_flags_t exresult;

        exresult = BuildExpBuf(m_op, test);
        CheckResult(lno, test, rc, exresult, result);

        EXPECT_STRNE(NULL, out)
            << "Line " << lno << ": "
            << Stringize(test)
            << " Data out is NULL";

        if (out != NULL) {
            const char *exbuf = test.ExpectedOut().GetBuf();
            EXPECT_STREQ(exbuf, out)
                << "Line " << lno << ": " << Stringize(test)
                << " expected=\"" << test.ExpectedOut().GetFmt() << "\""
                << " actual=\""   << m_outbuf.GetFmt() << "\"";
        }
    }
};

// Left trim tests
class TestIBUtilStrTrimLeft : public TestIBUtilStrTrim
{
public:
    TestIBUtilStrTrimLeft( ) :
        TestIBUtilStrTrim( ib_strtrim_left, FnName() )
    { };

    bool ChopLeft() const { return true; };
    bool ChopRight() const { return false; };

    bool ExpectCowAlias(bool mod) const
    {
        return ExpectCowAliasLeft(mod);
    }
    const char *FnName() const { return "ib_strtrim_left"; };
};

// Right trim tests
class TestIBUtilStrTrimRight : public TestIBUtilStrTrim
{
public:
    TestIBUtilStrTrimRight( ) :
        TestIBUtilStrTrim( ib_strtrim_right, FnName() )
    { };

    bool ChopLeft() const { return false; };
    bool ChopRight() const { return true; };

    bool ExpectCowAlias(bool mod) const
    {
        return ExpectCowAliasRight(mod);
    }
    const char *FnName() const { return "ib_strtrim_right"; };
};

// Left/Right trim tests
class TestIBUtilStrTrimLr : public TestIBUtilStrTrim
{
public:
    TestIBUtilStrTrimLr( ) :
        TestIBUtilStrTrim( ib_strtrim_lr, FnName() )
    { };
    bool ChopLeft() const { return true; };
    bool ChopRight() const { return true; };

    bool ExpectCowAlias(bool mod) const
    {
        return ExpectCowAliasLr(mod);
    }
    const char *FnName() const { return "ib_strtrim_lr"; };
};

// Base "ex" version of trim tests
class TestIBUtilStrTrimEx : public TestStringTrim
{
public:
    TestIBUtilStrTrimEx(ib_strmod_ex_fn_t fn, const char *fn_name )
        : TestStringTrim(NULL, NULL, fn, fn_name)
    {
    }

    virtual bool IsByteStr() const { return true; };

    void CheckResults(const TestDatum &test,
                      ib_status_t rc,
                      ib_flags_t result)
    {
        size_t lno = test.LineNo();
        const char *out = m_outbuf.GetBuf();
        size_t outlen = m_outbuf.GetLen();
        ib_flags_t exresult = BuildExpBuf(m_op, test);

        CheckResult(lno, test, rc, exresult, result);

        EXPECT_STRNE(NULL, out)
            << "Line " << lno << ": "
            << Stringize(test)
            << " Data out is NULL";

        if (out != NULL) {
            const ExpectedTextBuf &expected = test.ExpectedOut();
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
};

// Left (_ex version) trim tests
class TestIBUtilStrTrimLeftEx : public TestIBUtilStrTrimEx
{
public:
    TestIBUtilStrTrimLeftEx( ) :
        TestIBUtilStrTrimEx( ib_strtrim_left_ex, FnName() )
    { };

    bool ChopLeft() const { return true; };
    bool ChopRight() const { return false; };
    bool ExpectCowAlias(bool mod) const
    {
        return ExpectCowAliasLeft(mod);
    }
    const char *FnName() const { return "ib_strtrim_left_ex"; };
};

// Right (_ex version) trim tests
class TestIBUtilStrTrimRightEx : public TestIBUtilStrTrimEx
{
public:
    TestIBUtilStrTrimRightEx( ) :
        TestIBUtilStrTrimEx( ib_strtrim_right_ex, FnName() )
    { };

    bool ChopLeft() const { return false; };
    bool ChopRight() const { return true; };
    bool ExpectCowAlias(bool mod) const
    {
        return ExpectCowAliasRight(mod);
    }
    const char *FnName() const { return "ib_strtrim_right_ex"; };
};

// Left/Right (_ex version) trim tests
class TestIBUtilStrTrimLrEx : public TestIBUtilStrTrimEx
{
public:
    TestIBUtilStrTrimLrEx( ) :
        TestIBUtilStrTrimEx( ib_strtrim_lr_ex, FnName() )
    { };

    bool ChopLeft() const { return true; };
    bool ChopRight() const { return true; };
    bool ExpectCowAlias(bool mod) const
    {
        return ExpectCowAliasLr(mod);
    }
    const char *FnName() const { return "ib_strtrim_lr_ex"; };
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
TEST_F(TestIBUtilStrTrimLeft, test_strtrim_left_inplace)
{
    RunTests(IB_STROP_INPLACE, str_test_data);
}
TEST_F(TestIBUtilStrTrimLeft, test_strtrim_left_copy)
{
    RunTests(IB_STROP_COPY, str_test_data);
}
TEST_F(TestIBUtilStrTrimLeft, test_strtrim_left_COW)
{
    RunTests(IB_STROP_COW, str_test_data);
}

TEST_F(TestIBUtilStrTrimRight, test_strtrim_right_inplace)
{
    RunTests(IB_STROP_INPLACE, str_test_data );
}
TEST_F(TestIBUtilStrTrimRight, test_strtrim_right_copy)
{
    RunTests(IB_STROP_COPY, str_test_data );
}
TEST_F(TestIBUtilStrTrimRight, test_strtrim_right_COW)
{
    RunTests(IB_STROP_COW, str_test_data );
}

TEST_F(TestIBUtilStrTrimLr, test_strtrim_lr_inplace)
{
    RunTests(IB_STROP_INPLACE, str_test_data );
}
TEST_F(TestIBUtilStrTrimLr, test_strtrim_lr_copy)
{
    RunTests(IB_STROP_COPY, str_test_data );
}
TEST_F(TestIBUtilStrTrimLr, test_strtrim_lr_COW)
{
    RunTests(IB_STROP_COW, str_test_data );
}

// Test the ex versions with normal strings
TEST_F(TestIBUtilStrTrimLeftEx, test_strtrim_left_inplace_ex)
{
    RunTests(IB_STROP_INPLACE, str_test_data );
}
TEST_F(TestIBUtilStrTrimLeftEx, test_strtrim_left_copy_ex)
{
    RunTests(IB_STROP_INPLACE, str_test_data );
}
TEST_F(TestIBUtilStrTrimLeftEx, test_strtrim_left_COW_ex)
{
    RunTests(IB_STROP_INPLACE, str_test_data );
}

TEST_F(TestIBUtilStrTrimRightEx, test_strtrim_right_inplace_ex)
{
    RunTests(IB_STROP_INPLACE, str_test_data );
}
TEST_F(TestIBUtilStrTrimRightEx, test_strtrim_right_copy_ex)
{
    RunTests(IB_STROP_INPLACE, str_test_data );
}
TEST_F(TestIBUtilStrTrimRightEx, test_strtrim_right_COW_ex)
{
    RunTests(IB_STROP_INPLACE, str_test_data );
}

TEST_F(TestIBUtilStrTrimLrEx, test_strtrim_lr_inplace_ex)
{
    RunTests(IB_STROP_INPLACE, str_test_data );
}
TEST_F(TestIBUtilStrTrimLrEx, test_strtrim_lr_copy_ex)
{
    RunTests(IB_STROP_INPLACE, str_test_data );
}
TEST_F(TestIBUtilStrTrimLrEx, test_strtrim_lr_COW_ex)
{
    RunTests(IB_STROP_INPLACE, str_test_data );
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
TEST_F(TestIBUtilStrTrimLeftEx, test_strtrim_left_inplace_bs_ex)
{
    RunTests(IB_STROP_INPLACE, ex_test_data );
}
TEST_F(TestIBUtilStrTrimLeftEx, test_strtrim_left_copy_bs_ex)
{
    RunTests(IB_STROP_INPLACE, ex_test_data );
}
TEST_F(TestIBUtilStrTrimLeftEx, test_strtrim_left_COW_bs_ex)
{
    RunTests(IB_STROP_INPLACE, ex_test_data );
}

TEST_F(TestIBUtilStrTrimRightEx, test_strtrim_right_inplace_bs_ex)
{
    RunTests(IB_STROP_INPLACE, ex_test_data );
}
TEST_F(TestIBUtilStrTrimRightEx, test_strtrim_right_copy_bs_ex)
{
    RunTests(IB_STROP_INPLACE, ex_test_data );
}
TEST_F(TestIBUtilStrTrimRightEx, test_strtrim_right_COW_bs_ex)
{
    RunTests(IB_STROP_INPLACE, ex_test_data );
}

TEST_F(TestIBUtilStrTrimLrEx, test_strtrim_lr_inplace_bs_ex)
{
    RunTests(IB_STROP_INPLACE, ex_test_data );
}
TEST_F(TestIBUtilStrTrimLrEx, test_strtrim_lr_copy_bs_ex)
{
    RunTests(IB_STROP_INPLACE, ex_test_data );
}
TEST_F(TestIBUtilStrTrimLrEx, test_strtrim_lr_COW_bs_ex)
{
    RunTests(IB_STROP_INPLACE, ex_test_data );
}
