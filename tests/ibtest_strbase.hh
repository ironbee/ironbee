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
/// @brief IronBee &mdash; String Util Test Functions
/// 
/// @author Nick LeRoy <nleroy@qualys.com>
//////////////////////////////////////////////////////////////////////////////
#ifndef __IBTEST_STRBASE_HH__
#define __IBTEST_STRBASE_HH__

#include "ironbee_config_auto.h"

#include <ironbee/types.h>
#include <ironbee/string.h>
#include <ironbee/util.h>
#include "ibtest_textbuf.hh"

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
    size_t LineNo(void) const { return m_lineno; };
    bool IsEnd(void) const { return m_end; };
    const TextBuf &InBuf(void) const { return m_inbuf; };

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

// Base class for string modification tests
class BaseStrModTest : public ::testing::Test
{
public:
    BaseStrModTest(size_t call_buf_size, size_t buf_size,
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
    ib_strop_t Op(void) const { return m_op; };
    const char *OpStr( void ) const { return OpStr(m_op); };
    const char *OpStr( ib_strop_t op ) const
    {
        switch (op) {
        case IB_STROP_INPLACE:
            return "INPLACE";
        case IB_STROP_COW:
            return "COW";
        case IB_STROP_COPY:
            return "COPY";
        }
        assert(0);
    }
    
    virtual const char *FnName(void) const = 0;
    ib_mpool_t *MemPool( void ) { return m_mpool; };

    const char *BoolStr(ib_bool_t v) const
    {
        return (v == IB_TRUE ? "IB_TRUE" : "IB_FALSE");
    }
    const char *BoolStr(bool v) const
    {
        return (v == true ? "true" : "false");
    }
    const char *TriStr(ib_tristate_t v) const
    {
        switch (v) {
        case IB_TRI_FALSE:
            return "TRI/FALSE";
        case IB_TRI_TRUE:
            return "TRI/TRUE";
        case IB_TRI_UNSET:
            return "TRI/UNSET";
        }
        assert(0);
    }

    virtual bool ExpectCowAlias(bool mod) const
    {
        return (mod == false) ? true : false;
    }

    ib_flags_t ExpectedResult(ib_strop_t op, ib_bool_t mod) const
    {
        return ExpectedResult(op, (mod == IB_TRUE) ? true : false);
    }
    ib_flags_t ExpectedResult(ib_strop_t op, bool mod) const
    {
        ib_flags_t result;

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
        }
        if (mod == true) {
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
        if (ib_flags_all(result, IB_STRFLAG_MODIFIED) == IB_TRUE) {
            if (n++ > 0) {
                s += ",";
            }
            s += "MODIFIED";
        }
        if (ib_flags_all(result, IB_STRFLAG_NEWBUF) == IB_TRUE) {
            if (n++ > 0) {
                s += ",";
            }
            s += "NEWBUF";
        }
        if (ib_flags_all(result, IB_STRFLAG_ALIAS) == IB_TRUE) {
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
        rc = m_strmod_fn(m_op, MemPool(), in, &out, &result);
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

        rc = m_strmod_ex_fn(m_op, MemPool(),
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
        ib_bool_t both = 
            ib_flags_all(result, IB_STRFLAG_NEWBUF|IB_STRFLAG_ALIAS);
        ASSERT_EQ(IB_FALSE, both)
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
