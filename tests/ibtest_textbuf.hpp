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
#ifndef __IBTEST_TEXTBUF_HH__
#define __IBTEST_TEXTBUF_HH__

#include "ironbee_config_auto.h"

#include <ironbee/types.h>
#include <ironbee/string.h>

#include <stdexcept>
#include <assert.h>
#include <stdio.h>
#include <ctype.h>

/**
 * Utility class for handing blocks of text and formatting them in an escaped, printable, fashion.
 */
class TextBuf
{
public:
    TextBuf(size_t bufsize)
        : m_size(bufsize ? bufsize : 1),
          m_buf(new char [bufsize+1]),
          m_fmtsize(4*bufsize ? 4*bufsize : 1),
          m_fmtbuf(new char [m_fmtsize+1])
    {
        SetStr("");
    }

    TextBuf(const char *s)
        : m_size(strlen(s)+1),
          m_buf(new char [m_size]),
          m_fmtsize(4*m_size),
          m_fmtbuf(new char [m_fmtsize+1])
    {
        SetStr(s);
    }

    TextBuf(size_t bufsize, const char *s)
        : m_size(bufsize),
          m_buf(new char [bufsize+1]),
          m_fmtsize(4*bufsize ? 4*bufsize : 1),
          m_fmtbuf(new char [m_fmtsize+1])
    {
        SetStr(s);
    }

    TextBuf(const uint8_t *text, size_t len)
        : m_size(len+1),
          m_buf(new char [len+1]),
          m_fmtsize(4*len),
          m_fmtbuf(new char [m_fmtsize+1])
    {
        SetText((uint8_t *)text, len);
    }

    TextBuf(size_t bufsize, const char *text, size_t len)
        : m_size(bufsize),
          m_buf(new char [bufsize ? bufsize : 1]),
          m_fmtsize(4*bufsize ? 4*bufsize : 1),
          m_fmtbuf(new char [bufsize*2])
    {
        SetText(text, len);
    }

    virtual ~TextBuf()
    {
        delete[] m_buf;
        delete[] m_fmtbuf;
    }

    size_t SetNull( bool is_bytestr )
    {
        m_buf[0] = '\0';
        m_len = 0;
        m_null = true;
        m_bytestr = is_bytestr;
        InvalidateFmt( );
        return m_len;
    }

    size_t Set(const TextBuf &other)
    {
        if (other.IsByteStr() ) {
            return SetText(other.GetText(), other.GetLen());
        }
        else {
            return SetStr(other.GetStr());
        }
    }

    size_t SetStr(const char *s, bool is_bytestr=false)
    {
        if (s == NULL) {
            return SetNull( false );
        }
        m_len = strlen(s);
        if (m_len == 0) {
            m_buf[0] = '\0';
        }
        else {
            strncpy(m_buf, s, m_size);
        }
        m_null = false;
        m_bytestr = is_bytestr;
        InvalidateFmt( );
        return m_len;
    };

    size_t SetText(const uint8_t *text, size_t len)
    {
        assert (len <= m_size);
        if (text == NULL) {
            return SetNull( true );
        }
        memcpy(m_buf, text, len);
        m_null = false;
        m_len = len;
        m_buf[len] = '\0';
        m_bytestr = true;
        InvalidateFmt( );
        return m_len;
    };

    size_t SetText(const char *text, size_t len)
    {
        return SetText( (const uint8_t *)text, len);
    }

    void InvalidateFmt(void) const { m_fmtvalid = false; };
    bool IsFmtValid(void) const { return m_fmtvalid; };

    void SetByteStr(bool is_bytestr) { m_bytestr = is_bytestr; };
    bool IsByteStr(void) const { return m_bytestr; };
    bool IsNull(void) const { return m_null; };

    virtual char *GetBuf(void) const
    {
        return m_null ? NULL : m_buf;
    };
    virtual uint8_t *GetUBuf(void) const
    {
        return m_null ? NULL : (uint8_t *)m_buf;
    }
    virtual const char *GetStr(void) const
    {
        return GetBuf();
    };
    virtual const uint8_t *GetText(void) const
    {
        return m_null ? NULL : (uint8_t *)m_buf;
    };
    virtual size_t GetLen(void) const { return m_len; };
    virtual const char *GetFmt(void) const
    {
        if (! IsFmtValid()) {
            if (m_bytestr) {
                return BuildFmt(m_buf, m_len);
            }
            else {
                return BuildFmt(m_buf);
            }
        }
        return m_fmtbuf;
    };

    bool operator == (const TextBuf &other) const
    {
        if (IsNull() || other.IsNull() ) {
            return false;
        }
        if (IsByteStr() || other.IsByteStr()) {
            if (GetLen() != other.GetLen()) {
                return false;
            }
            else if (GetLen() == 0) {
                return true;
            }
            else {
                return (memcmp(GetBuf(), other.GetBuf(), GetLen()) == 0);
            }
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
            else if (*str == '"') {
                *buf++ = '\\';
                *buf++ = '"';
            }
            else if (isprint(*str) ) {
                *buf++ = *str;
            }
            else {
                uint8_t *p = (uint8_t *)str;
                char tmp[8];
                snprintf(tmp, sizeof(tmp), "_\\x%02x_", *p);
                strcpy(buf, tmp);
                buf += strlen(tmp);
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

#endif
