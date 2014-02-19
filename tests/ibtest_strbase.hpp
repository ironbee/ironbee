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
/// IronBee - Unit testing utility functions implementation.
/// @brief IronBee --- String Util Test Functions
///
/// @author Nick LeRoy <nleroy@qualys.com>
//////////////////////////////////////////////////////////////////////////////
#ifndef __IBTEST_STRBASE_HH__
#define __IBTEST_STRBASE_HH__

#include "ironbee_config_auto.h"

#include <ironbee/flags.h>
#include <ironbee/mm_mpool.h>
#include <ironbee/types.h>
#include <ironbee/string.h>
#include <ironbee/util.h>
#include "ibtest_textbuf.hpp"

#include "gtest/gtest.h"

#include <stdexcept>
#include <assert.h>
#include <stdio.h>

// Single test data point
class BaseTestDatum
{
public:
    BaseTestDatum()
        : m_end(true),
          m_lineno(0),
          m_inbuf(1, "")
    { };

    BaseTestDatum(size_t lno, size_t bufsize, const char *in)
        : m_end(false),
          m_lineno(lno),
          m_inbuf(bufsize, in)
    { };

    BaseTestDatum(size_t lno, size_t bufsize, const char *in, size_t len)
        : m_end(false),
          m_lineno(lno),
          m_inbuf(bufsize, in, len)
    {
    };

    // Accessors
    size_t LineNo() const { return m_lineno; };
    bool IsEnd() const { return m_end; };
    const TextBuf &InBuf() const { return m_inbuf; };

private:
    bool               m_end;         // Special case: End marker.
    size_t             m_lineno;      // Test's line number
    TextBuf            m_inbuf;       // Input text buffer
};

// Formatted call text buffer
class CallTextBuf : public TextBuf
{
public:
    CallTextBuf(size_t bufsize, const char *fn) :
        TextBuf(bufsize, fn)
    {
    };

    const char *Stringize(const char *op, const BaseTestDatum &datum)
    {
        snprintf(m_fmtbuf, m_size,
                 "%s(%s, \"%s\", ...)", m_buf, op, datum.InBuf().GetFmt());
        return m_fmtbuf;
    };
};

// Class to test simple string manipulations.  This class is designed to work
// with string manipulation function families that have the following
// implementation functions:
// - in-place manipulation of NUL-terminated strings
// - in-place manipulation of byte strings (_ex version)
// - copy-on-write manipulation of NUL-terminated strings
// - copy-on-write manipulation of byte strings (_ex version)
class TestSimpleStringManipulation : public ::testing::Test
{
public:
    TestSimpleStringManipulation()
        : m_mpool(NULL)
    {
    };

    virtual void SetUp()
    {
        ib_status_t rc = ib_mpool_create(&m_mpool, NULL, NULL);
        if (rc != IB_OK) {
            throw std::runtime_error("Could not create memory pool");
        }
    }

    virtual void TearDown()
    {
        if (m_mpool != NULL) {
            ib_mpool_destroy(m_mpool);
        }
        m_mpool = NULL;
    }

    typedef enum { TYPE_NUL, TYPE_EX, TYPE_EX_TO_STR } test_type_t;

    virtual const char *TestName(
        ib_strop_t strop,
        test_type_t ex) = 0;

    virtual ib_status_t ExecInplaceNul(
        char *buf,
        ib_flags_t &result)
    {
        return IB_ENOTIMPL;
    }

    virtual ib_status_t ExecInplaceEx(
        uint8_t *data_in,
        size_t dlen_in,
        size_t &dlen_out,
        ib_flags_t &result)
    {
        return IB_ENOTIMPL;
    }

    virtual ib_status_t ExecCowNul(
        const char *data_in,
        char **data_out,
        ib_flags_t &result)
    {
        return IB_ENOTIMPL;
    }

    virtual ib_status_t ExecCowEx(
        const uint8_t *data_in,
        size_t dlen_in,
        uint8_t **data_out,
        size_t &dlen_out,
        ib_flags_t &result)
    {
        return IB_ENOTIMPL;
    }

    virtual ib_status_t ExecCopyNul(
        const char *data_in,
        char **data_out,
        ib_flags_t &result)
    {
        return IB_ENOTIMPL;
    }

    virtual ib_status_t ExecCopyEx(
        const uint8_t *data_in,
        size_t dlen_in,
        uint8_t **data_out,
        size_t &dlen_out,
        ib_flags_t &result)
    {
        return IB_ENOTIMPL;
    }

    virtual ib_status_t ExecCopyExToNul(
        const uint8_t *data_in,
        size_t dlen_in,
        char **data_out,
        ib_flags_t &result)
    {
        return IB_ENOTIMPL;
    }

    virtual ib_status_t ExecNulToNulBuf(
        const char *data_in,
        char *data_out,
        size_t dsize_out,
        size_t &dlen_out,
        ib_flags_t &result)
    {
        return IB_ENOTIMPL;
    }

    virtual ib_status_t ExecExToNulBuf(
        const uint8_t *data_in,
        size_t dlen_in,
        char *data_out,
        size_t dsize_out,
        size_t &dlen_out,
        ib_flags_t &result)
    {
        return IB_ENOTIMPL;
    }


    void RunTest(const uint8_t *in, size_t inlen,
                 const uint8_t *out = NULL, size_t outlen = 0)
    {
        TextBuf input(in, inlen);
        if (out == NULL) {
            out = in;
            outlen = inlen;
        }
        TextBuf expected(out, outlen);
        RunTestInplaceEx(input, expected);
        RunTestCowEx(input, expected);
    }

    void RunTest(const uint8_t *in, size_t inlen, const char *out)
    {
        TextBuf input(in, inlen);
        TextBuf expected(out);
        RunTestCopyEx(input, expected);
        RunTestCopyExToNul(input, expected);
        RunTestBuf(in, inlen, out, strlen(out)+1, IB_OK);
    }

    void RunTestBuf(const char *in,
                    const char *out,
                    size_t bufsize,
                    ib_status_t rc = IB_OK)
    {
        TextBuf input(in);
        if (out == NULL) {
            out = in;
        }
        TextBuf expected(out);
        if ( (rc == IB_OK) && (bufsize <= expected.GetLen()) ) {
            rc = IB_ETRUNC;
        }
        RunTestNulToNulBuf(input, expected, bufsize, rc);
        RunTestExToNulBuf(input, expected, bufsize, rc);
    }

    void RunTestBuf(const uint8_t *in,
                    size_t inlen,
                    const char *out,
                    size_t bufsize,
                    ib_status_t rc = IB_OK)
    {
        TextBuf input(in, inlen);
        if (out == NULL) {
            out = (const char *)in;
        }
        TextBuf expected(out);
        if ( (rc == IB_OK) && (bufsize <= expected.GetLen()) ) {
            rc = IB_ETRUNC;
        }
        RunTestExToNulBuf(input, expected, bufsize, rc);
    }

protected:
    const char *TestOpName(ib_strop_t op)
    {
        static const char *ops[] = { "", "_copy", "_cow", "_buf" };
        return ops[op];
    }
    const char *TestTypeName(test_type_t tt)
    {
        static const char *types[] = { "nul", "ex", "ex_to_str" };
        return types[tt];
    }
    const char *TestNameImpl(const char *test, ib_strop_t op, test_type_t tt)
    {
        static char buf[128];
        snprintf(buf, sizeof(buf),
                 "%s%s_%s()", test, TestOpName(op), TestTypeName(tt) );
        return buf;
    }

    void CheckResult(const char *name,
                     const TextBuf &input,
                     const TextBuf &expected,
                     ib_flags_t expected_unmodified_result,
                     ib_flags_t expected_modified_result,
                     ib_status_t rc,
                     ib_flags_t result,
                     const TextBuf &output)
    {
        const char *modstr;
        ib_flags_t eresult;

        if ( (rc == IB_ETRUNC) || (input != expected) ) {
            eresult = expected_modified_result;
            modstr = "should be modified";
        }
        else {
            eresult = expected_unmodified_result;
            modstr = "should not be modified";
        }
        ASSERT_EQ(eresult, result)
            << name << " " << modstr << " " << std::endl
            << " Expected: "
            << " [" << expected.GetLen() << "]"
            << " \"" << expected.GetFmt() << "\"" << std::endl
            << " Actual:   "
            << " [" << output.GetLen() << "]"
            << " \"" << output.GetFmt() << "\"";

        if (rc != IB_ETRUNC) {
            ASSERT_TRUE(expected == output)
                << name << std::endl
                << " Expected: "
                << " [" << expected.GetLen() << "]"
                << " \"" << expected.GetFmt() << "\"" << std::endl
                << " Actual:   "
                << " [" << output.GetLen() << "]"
                << " \"" << output.GetFmt() << "\"";
        }
    }

    void RunTestInplaceNul(const TextBuf &input, const TextBuf &expected)
    {
        size_t len = input.GetLen();
        char buf[len ? len : 1];
        ib_status_t rc;
        ib_flags_t result;

        strcpy(buf, input.GetStr());

        rc = ExecInplaceNul(buf, result);
        if (rc == IB_ENOTIMPL) {
            return;
        }
        const char *name = TestName(IB_STROP_INPLACE, TYPE_NUL);
        ASSERT_EQ(IB_OK, rc) << name;

        TextBuf output(buf);
        CheckResult(name, input,
                    expected,
                    IB_STRFLAG_ALIAS,
                    (IB_STRFLAG_ALIAS | IB_STRFLAG_MODIFIED),
                    rc, result, output);
    }

    void RunTestInplaceEx(const TextBuf &input, const TextBuf &expected)
    {
        size_t len = input.GetLen();
        uint8_t buf[len ? len : 1];
        ib_status_t rc;
        size_t outlen;
        ib_flags_t result;

        memcpy(buf, input.GetBuf(), len);

        rc = ExecInplaceEx(buf, len, outlen, result);
        if (rc == IB_ENOTIMPL) {
            return;
        }
        const char *name = TestName(IB_STROP_INPLACE, TYPE_EX);
        ASSERT_EQ(IB_OK, rc) << name;

        TextBuf output(buf, outlen);
        CheckResult(name, input,
                    expected,
                    IB_STRFLAG_ALIAS,
                    (IB_STRFLAG_ALIAS | IB_STRFLAG_MODIFIED),
                    rc, result, output);
    }

    void RunTestCowNul(const TextBuf &input, const TextBuf &expected)
    {
        char *out;
        ib_status_t rc;
        ib_flags_t result;

        rc = ExecCowNul(input.GetStr(), &out, result);
        if (rc == IB_ENOTIMPL) {
            return;
        }
        const char *name = TestName(IB_STROP_COW, TYPE_NUL);
        ASSERT_EQ(IB_OK, rc) << name;

        TextBuf output(out);
        CheckResult(name, input,
                    expected,
                    IB_STRFLAG_ALIAS,
                    (IB_STRFLAG_NEWBUF | IB_STRFLAG_MODIFIED),
                    rc, result, output);
    }

    void RunTestCowEx(const TextBuf &input, const TextBuf &expected)
    {
        size_t len = input.GetLen();
        uint8_t *out;
        ib_status_t rc;
        size_t outlen;
        ib_flags_t result;

        rc = ExecCowEx(input.GetUBuf(), len, &out, outlen, result);
        if (rc == IB_ENOTIMPL) {
            return;
        }
        const char *name = TestName(IB_STROP_COW, TYPE_EX);
        ASSERT_EQ(IB_OK, rc) << name;

        TextBuf output(out, outlen);
        CheckResult(name, input,
                    expected,
                    IB_STRFLAG_ALIAS,
                    (IB_STRFLAG_NEWBUF | IB_STRFLAG_MODIFIED),
                    rc, result, output);
    }

    void RunTestCopyNul(const TextBuf &input, const TextBuf &expected)
    {
        char *out;
        ib_status_t rc;
        ib_flags_t result;

        rc = ExecCopyNul(input.GetStr(), &out, result);
        if (rc == IB_ENOTIMPL) {
            return;
        }
        const char *name = TestName(IB_STROP_COPY, TYPE_NUL);
        ASSERT_EQ(IB_OK, rc) << name;

        TextBuf output(out);
        CheckResult(name, input,
                    expected,
                    IB_STRFLAG_NEWBUF,
                    (IB_STRFLAG_NEWBUF | IB_STRFLAG_MODIFIED),
                    rc, result, output);
    }

    void RunTestCopyEx(const TextBuf &input, const TextBuf &expected)
    {
        size_t len = input.GetLen();
        uint8_t *out;
        ib_status_t rc;
        size_t outlen;
        ib_flags_t result;

        rc = ExecCopyEx(input.GetUBuf(), len, &out, outlen, result);
        if (rc == IB_ENOTIMPL) {
            return;
        }
        const char *name = TestName(IB_STROP_COPY, TYPE_EX);
        ASSERT_EQ(IB_OK, rc) << name;

        TextBuf output(out, outlen);
        CheckResult(name, input,
                    expected,
                    IB_STRFLAG_NEWBUF,
                    (IB_STRFLAG_NEWBUF | IB_STRFLAG_MODIFIED),
                    rc, result, output);
    }

    void RunTestCopyExToNul(const TextBuf &input, const TextBuf &expected)
    {
        ib_status_t rc;
        size_t len = input.GetLen();
        char *out;
        ib_flags_t result;

        rc = ExecCopyExToNul(input.GetUBuf(), len, &out, result);
        if (rc == IB_ENOTIMPL) {
            return;
        }
        const char *name = TestName(IB_STROP_COPY, TYPE_NUL);
        ASSERT_EQ(IB_OK, rc) << name;

        TextBuf output(out);
        CheckResult(name, input,
                    expected,
                    IB_STRFLAG_NEWBUF,
                    (IB_STRFLAG_NEWBUF | IB_STRFLAG_MODIFIED),
                    rc, result, output);
    }

    void RunTestNulToNulBuf(const TextBuf &input,
                            const TextBuf &expected,
                            size_t bufsize,
                            ib_status_t expected_rc)
    {
        char buf[bufsize];
        size_t len;
        ib_status_t rc;
        ib_flags_t result;

        rc = ExecNulToNulBuf(input.GetBuf(),
                             buf, bufsize, len,
                             result);
        if (rc == IB_ENOTIMPL) {
            return;
        }
        const char *name = TestName(IB_STROP_BUF, TYPE_NUL);
        ASSERT_EQ(expected_rc, rc) << name;

        TextBuf output(buf);
        CheckResult(name, input,
                    expected,
                    IB_STRFLAG_NONE,
                    IB_STRFLAG_MODIFIED,
                    rc, result, output);
    }

    void RunTestExToNulBuf(const TextBuf &input,
                           const TextBuf &expected,
                           size_t bufsize,
                           ib_status_t expected_rc)
    {
        char buf[bufsize];
        size_t len;
        ib_status_t rc;
        ib_flags_t result;

        rc = ExecExToNulBuf(input.GetUBuf(), input.GetLen(),
                            buf, bufsize, len,
                            result);
        if (rc == IB_ENOTIMPL) {
            return;
        }
        const char *name = TestName(IB_STROP_BUF, TYPE_EX_TO_STR);
        ASSERT_EQ(expected_rc, rc) << name;

        TextBuf output(buf);
        CheckResult(name, input,
                    expected,
                    IB_STRFLAG_NONE,
                    IB_STRFLAG_MODIFIED,
                    rc, result, output);
    }

public:
    ib_mpool_t *m_mpool;
};

// Base class for string modification tests.  This class is specifically for
// testing string modification tests that use the ib_strmod_fn_t and
// ib_strmod_ex_fn_t interfaces.
class TestStringModification : public ::testing::Test
{
public:
    TestStringModification(size_t call_buf_size, size_t buf_size,
                           ib_strmod_fn_t fn, const char *fn_name,
                           ib_strmod_ex_fn_t ex_fn, const char *ex_fn_name)
        : m_strmod_fn(fn),
          m_callbuf(call_buf_size, fn_name),
          m_strmod_ex_fn(ex_fn),
          m_ex_callbuf(call_buf_size, ex_fn_name),
          m_op(IB_STROP_INPLACE),
          m_inbuf(buf_size),
          m_outbuf(buf_size)
    {
        ib_status_t rc = ib_mpool_create(&m_mpool, NULL, NULL);
        if (rc != IB_OK) {
            throw std::runtime_error("Could not create mpool.");
        }
    }

    void SetOp(ib_strop_t op) { m_op = op; };
    ib_strop_t Op() const { return m_op; };
    const char *OpStr() const { return OpStr(m_op); };
    const char *OpStr( ib_strop_t op ) const
    {
        switch (op) {
        case IB_STROP_INPLACE:
            return "INPLACE";
        case IB_STROP_COW:
            return "COW";
        case IB_STROP_COPY:
            return "COPY";
        case IB_STROP_BUF:
            return "BUF";
        }
        assert(0);
    }

    virtual const char *FnName() const = 0;
    ib_mpool_t *MemPool() { return m_mpool; };
    ib_mm_t MM() { return ib_mm_mpool(MemPool()); };

    const char *BoolStr(bool v) const
    {
        return (v ? "true" : "false");
    }

    virtual bool ExpectCowAlias(bool mod) const
    {
        return (! mod);
    }

    ib_flags_t ExpectedResult(ib_strop_t op, bool mod) const
    {
        ib_flags_t result = 0;

        switch (op) {
        case IB_STROP_INPLACE:
            result = IB_STRFLAG_ALIAS;
            break;

        case IB_STROP_COPY:
            result = IB_STRFLAG_NEWBUF;
            break;

        case IB_STROP_COW:
            result = ExpectCowAlias(mod) ? IB_STRFLAG_ALIAS : IB_STRFLAG_NEWBUF;
            break;

        case IB_STROP_BUF:
            result = IB_STRFLAG_NONE;
            break;
        }
        if (mod) {
            result |= IB_STRFLAG_MODIFIED;
        }
        return result;
    }

    bool AllSpace( const TextBuf *buf=NULL ) const
    {
        if (buf == NULL) {
            buf = &m_inbuf;
        }
        const char *p = buf->GetBuf();
        const char *end = p + buf->GetLen();
        while(p < end) {
            if (isspace(*p) == 0) {
                return false;
            }
            ++p;
        }
        return true;
    }

    bool LeftSpace( const TextBuf *buf=NULL ) const
    {
        if (buf == NULL) {
            buf = &m_inbuf;
        }
        const char *p = buf->GetBuf();
        if ( (buf->GetLen() > 0) && (isspace(*p) != 0) ) {
            return true;
        }
        else {
            return false;
        }
    }

    bool RightSpace( const TextBuf *buf=NULL ) const
    {
        if (buf == NULL) {
            buf = &m_inbuf;
        }
        const char *p = buf->GetBuf() + buf->GetLen() - 1;
        if ( (buf->GetLen() > 0) && (isspace(*p) != 0) ) {
            return true;
        }
        else {
            return false;
        }
    }

    const std::string &ResultStr(ib_flags_t result) const
    {
        std::string s;
        return ResultStr(result, s);
    }
    const std::string &ResultStr(ib_flags_t result, std::string &s) const
    {
        int n = 0;
        if (result == IB_STRFLAG_NONE) {
            s = "<None>";
            return s;
        }
        s = "<";
        if (ib_flags_all(result, IB_STRFLAG_MODIFIED)) {
            if (n++ > 0) {
                s += ",";
            }
            s += "MODIFIED";
        }
        if (ib_flags_all(result, IB_STRFLAG_NEWBUF)) {
            if (n++ > 0) {
                s += ",";
            }
            s += "NEWBUF";
        }
        if (ib_flags_all(result, IB_STRFLAG_ALIAS)) {
            if (n++ > 0) {
                s += ",";
            }
            s += "ALIAS";
        }
        s += ">";
        return s;
    }

    ib_status_t RunTest(const BaseTestDatum &test, ib_flags_t &result)
    {
        if (m_strmod_fn != NULL) {
            return RunTestStr(test, result);
        }
        else if (m_strmod_ex_fn != NULL) {
            return RunTestEx(test, result);
        }
        else {
            assert (0);
        }
    }

    ib_status_t RunTestStr(const BaseTestDatum &test, ib_flags_t &result)
    {
        char *out = NULL;
        char *in;
        ib_status_t rc;

        m_inbuf.Set(test.InBuf());
        in = const_cast<char *>(m_inbuf.GetBuf());
        assert(in != NULL);
        assert(m_strmod_fn != NULL);
        rc = m_strmod_fn(m_op, MM(), in, &out, &result);
        if (rc == IB_OK) {
            m_outbuf.SetStr(out);
        }
        return rc;
    }

    virtual ib_status_t RunTestEx(const BaseTestDatum &test, ib_flags_t &result)
    {
        uint8_t *out;
        size_t outlen;
        ib_status_t rc;

        m_inbuf.Set(test.InBuf());

        rc = m_strmod_ex_fn(m_op, MM(),
                            const_cast<uint8_t *>(m_inbuf.GetText()),
                            m_inbuf.GetLen(),
                            &out, &outlen,
                            &result);
        if (rc == IB_OK) {
            m_outbuf.SetText((char *)out, outlen);
        }
        return rc;
    }

    void CheckResult(int lineno,
                     const BaseTestDatum &test,
                     ib_status_t rc,
                     ib_flags_t exresult,
                     ib_flags_t result)
    {
        std::string s1, s2;
        EXPECT_EQ(IB_OK, rc)
            << "Line " << lineno << ": "
            << Stringize(test) << " returned " << rc;
        if (rc != IB_OK) {
            return;
        }

        // NEWBUF and ALIAS result flags should never both be set
        bool both =
            ib_flags_all(result, IB_STRFLAG_NEWBUF|IB_STRFLAG_ALIAS);
        ASSERT_FALSE(both)
            << "Line " << lineno << ": " << Stringize(test)
            << " both NEWBUF and ALIAS result flags are set!"
            << ResultStr(result, s1);

        // Build the expected output
        EXPECT_EQ(exresult, result)
            << "Line " << lineno << ": " << Stringize(test)
            << " expected result=" << ResultStr(exresult, s1) << exresult
            << " actual=" << ResultStr(result, s2) << result;
    }

    const char *Stringize(const BaseTestDatum &test)
    {
        return m_callbuf.Stringize(OpStr(m_op), test);
    }

protected:
    ib_strmod_fn_t    m_strmod_fn;     // String mod function
    CallTextBuf       m_callbuf;       // Call string buffer
    ib_strmod_ex_fn_t m_strmod_ex_fn;  // ex version of string mod function
    CallTextBuf       m_ex_callbuf;       // Call string buffer
    ib_strop_t        m_op;            // String operation
    ib_mpool_t       *m_mpool;         // Memory pool to use
    TextBuf           m_inbuf;         // Copy of input
    TextBuf           m_outbuf;        // Output buffer
};

#endif
