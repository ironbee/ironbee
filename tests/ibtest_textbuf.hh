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
/// @brief IronBee - String Util Test Functions
/// 
/// @author Nick LeRoy <nleroy@qualys.com>
//////////////////////////////////////////////////////////////////////////////

#include "ironbee_config_auto.h"

#include <ironbee/types.h>
#include <ironbee/string.h>

#include <stdexcept>
#include <assert.h>
#include <stdio.h>

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
        //assert(other.IsByteStr() == IsByteStr());
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

    const char *BuildFmt(void) const {
        if (m_bytestr) {
            return BuildFmt(m_buf, m_len);
        }
        else {
            return BuildFmt(m_buf);
        }
    }

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

