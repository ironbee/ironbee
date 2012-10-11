/*****************************************************************************
 * Licensed to Qualys, Inc. (QUALYS) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * QUALYS licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 ****************************************************************************/

/**
 * @file
 * @brief IronBee --- String escape functions
 * @author Nick LeRoy <nleroy@qualys.com>
 */

#include "ironbee_config_auto.h"

#include <ironbee/escape.h>

#include <ironbee/debug.h>
#include <ironbee/mpool.h>
#include <ironbee/string.h>
#include <ironbee/types.h>
#include <ironbee/util.h>

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

/* Convert a c-string to a json string with escaping, external buf version */
ib_status_t ib_string_escape_json_buf_ex(
    const uint8_t *data_in,
    size_t dlen_in,
    bool add_nul,
    bool quote,
    char *data_out,
    size_t dsize_out,
    size_t *dlen_out,
    ib_flags_t *result
)
{
    IB_FTRACE_INIT();
    assert(data_in != NULL);
    assert(data_out != NULL);

    const uint8_t *iptr;
    const uint8_t *iend = data_in + dlen_in;
    char *optr;
    const char *oend;
    bool modified = false;
    ib_status_t rc = IB_OK;

    if (result != NULL) {
        *result = IB_STRFLAG_NONE;
    }

    oend = data_out + dsize_out;
    if (quote) {
        oend -= 1;
    }
    if (add_nul) {
        oend -= 1;
    }
    optr = data_out;
    if (quote) {
        *optr = '\"';
        ++optr;
    }
    for (iptr = data_in; iptr < iend; ++iptr) {
        size_t size = 1;
        const char *ostr = NULL;
        uint8_t c = *iptr;
        char tmp[16];

        switch (c) {

        case '\"':
            size = 2;
            ostr = "\\\"";
            break;

        case '\\':
            size = 2;
            ostr = "\\\\";
            break;

        case '/':
            size = 2;
            ostr = "\\/";
            break;

        case '\b':
            size = 2;
            ostr = "\\b";
            break;

        case '\f':
            size = 2;
            ostr = "\\f";
            break;

        case '\n':
            size = 2;
            ostr = "\\n";
            break;

        case '\r':
            size = 2;
            ostr = "\\r";
            break;

        case '\t':
            size = 2;
            ostr = "\\t";
            break;

        case '\0':
            size = 6;
            ostr = "\\u0000";
            break;

        default:
            if (isprint(c) == 0) {
                size = sprintf(tmp, "\\u%04x", c);
                ostr = tmp;
            }
            else {
                size = 1;
            }
            break;
        }

        if (optr + size > oend) {
            rc = IB_ETRUNC;
            modified = true;
            break;
        }
        if (size == 1) {
            *optr = (char)*iptr;
            ++optr;
        }
        else {
            memcpy(optr, ostr, size);
            optr += size;
            modified = true;
        }
   }

    /* Add on our quote and nul byte if required */
    if (quote) {
        *optr = '\"';
        modified = true;
        ++optr;
    }
    if (add_nul) {
        *optr = '\0';
        ++optr;
    }

    if ( (result != NULL) && modified) {
        *result = IB_STRFLAG_MODIFIED;
    }
    if (dlen_out != NULL) {
        *dlen_out = optr - data_out;
    }
    IB_FTRACE_RET_STATUS(rc);
}

/* Convert a c-string to a json string with escaping */
ib_status_t ib_string_escape_json_buf(const char *data_in,
                                      bool quote,
                                      char *data_out,
                                      size_t dsize_out,
                                      size_t *dlen_out,
                                      ib_flags_t *result)
{
    IB_FTRACE_INIT();
    assert(data_in != NULL);
    assert(data_out != NULL);

    ib_status_t rc;
    rc = ib_string_escape_json_buf_ex((const uint8_t *)data_in, strlen(data_in),
                                      true, quote,
                                      data_out, dsize_out, dlen_out,
                                      result);

    /* Subtract the nul byte from the length */
    if (dlen_out != NULL) {
        --(*dlen_out);
    }
    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_strlist_escape_json_buf(const ib_list_t *items,
                                       bool quote,
                                       const char *join,
                                       char *data_out,
                                       size_t dsize_out,
                                       size_t *dlen_out,
                                       ib_flags_t *result)
{
    IB_FTRACE_INIT();
    assert(data_out != NULL);
    assert(join != NULL);

    const ib_list_node_t *node;
    char *cur = data_out;
    size_t remain = dsize_out;
    bool first = true;
    size_t elements;
    size_t joinlen = strlen(join);

    if (dlen_out != NULL) {
        *dlen_out = 0;
    }
    if (result != NULL) {
        *result = IB_STRFLAG_NONE;
    }

    /* Handle NULL / empty list */
    if (items == NULL) {
        elements = 0;
    }
    else {
        elements = ib_list_elements(items);
    }
    if (elements == 0) {
        *data_out = '\0';
        IB_FTRACE_RET_STATUS(IB_OK);
    }

    IB_LIST_LOOP_CONST(items, node) {
        const char *str = (const char *)ib_list_node_data_const(node);
        ib_status_t rc;
        ib_flags_t rslt;
        size_t len;

        /* First one? */
        if (! first) {
            if (remain < (joinlen + 1) ) {
                IB_FTRACE_RET_STATUS(IB_ETRUNC);
            }
            strcpy(cur, join);
            cur += joinlen;
            remain -= joinlen;
        }
        else {
            first = false;
        }

        /* Escape directly into the current buffer */
        rc = ib_string_escape_json_buf(str, quote, cur, remain, &len, &rslt);

        /* Adjust pointer and length */
        if ( (rc == IB_OK) || (rc == IB_ETRUNC) ) {
            if (dlen_out != NULL) {
                *dlen_out += len;
            }
            cur += len;
            remain -= len;
        }

        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }

        if ( (result != NULL) && (ib_flags_all(rslt, IB_STRFLAG_MODIFIED)) ) {
            *result = IB_STRFLAG_MODIFIED;
        }

        /* Quit if we're out of space */
        if (remain == 0) {
            IB_FTRACE_RET_STATUS(IB_ETRUNC);
        }
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

/* Convert a bytestring to a json string with escaping, ex version */
ib_status_t ib_string_escape_json_ex(ib_mpool_t *mp,
                                     const uint8_t *data_in,
                                     size_t dlen_in,
                                     bool add_nul,
                                     bool quote,
                                     char **data_out,
                                     size_t *dlen_out,
                                     ib_flags_t *result)
{
    IB_FTRACE_INIT();
    assert(mp != NULL);
    assert(data_in != NULL);
    assert(data_out != NULL);
    assert(result != NULL);

    size_t mult = 2; /* Size multiplier */
    size_t buflen;   /* Length of data buf can hold */
    size_t bufsize;  /* Size of allocated buffer (may hold trailing nul) */
    char *buf;
    ib_status_t rc;

allocate:
    buflen = mult * dlen_in;
    bufsize = buflen + (add_nul ? 1 : 0) + (quote ? 2 : 0);
    buf = ib_mpool_alloc(mp, bufsize);
    if (buf == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    rc = ib_string_escape_json_buf_ex(data_in, dlen_in, add_nul, quote,
                                      buf, bufsize, dlen_out, result);
    if (rc == IB_ETRUNC) {
        assert (mult == 2);
        mult = 6;
        goto allocate;
    }
    *result |= IB_STRFLAG_NEWBUF;
    *data_out = buf;

    IB_FTRACE_RET_STATUS(rc);
}

/* Convert a c-string to a json string with escaping */
ib_status_t ib_string_escape_json(ib_mpool_t *mp,
                                  const char *data_in,
                                  bool quote,
                                  char **data_out,
                                  ib_flags_t *result)
{
    IB_FTRACE_INIT();
    assert(mp != NULL);
    assert(data_in != NULL);
    assert(data_out != NULL);
    assert(result != NULL);

    ib_status_t rc;
    rc = ib_string_escape_json_ex(mp,
                                  (const uint8_t *)data_in, strlen(data_in),
                                  true, quote,
                                  data_out, NULL,
                                  result);
    IB_FTRACE_RET_STATUS(rc);
}

/**
 * @brief Convert the input character to the byte value represented by
 *        it's hexadecimal value. The input of 'F' results in the
 *        character 15 being returned. If a character is not
 *        submitted that is hexadecimal, then -1 is returned.
 * @param[in] a the input character to be converted. A-F, a-f, or 0-9.
 * @returns The byte value of the passed in hexadecimal character or -1 on
 *          failure.
 */
static inline char hexchar_to_byte(char a)
{
    switch(a) {
    case '0':  return 0;
    case '1':  return 1;
    case '2':  return 2;
    case '3':  return 3;
    case '4':  return 4;
    case '5':  return 5;
    case '6':  return 6;
    case '7':  return 7;
    case '8':  return 8;
    case '9':  return 9;
    case 'a':
    case 'A':  return 10;
    case 'b':
    case 'B':  return 11;
    case 'c':
    case 'C':  return 12;
    case 'd':
    case 'D':  return 13;
    case 'e':
    case 'E':  return 14;
    case 'f':
    case 'F':  return 15;
    default:
        return -1;
    }
}

/**
 * @brief Take two hex characters and merge them into a single byte.
 * @param[in] high high order byte.
 * @param[in] low low order byte.
 * @returns high and low mixed into a single byte.
 */
static inline char hex_to_int(char high, char low)
{
    return hexchar_to_byte(high) << 4 | hexchar_to_byte(low);
}

ib_status_t DLL_PUBLIC ib_util_unescape_string(char* dst,
                                               size_t* dst_len,
                                               const char* src,
                                               size_t src_len,
                                               uint32_t flags)
{
    size_t dst_i = 0;
    size_t src_i = 0;

    /* For loop variables. */
    int i;

    for (src_i=0; src_i<src_len; ++src_i) {
        switch (src[src_i]) {
        case '\\':
            ++src_i;

            if (src_i>=src_len) {
                return IB_EINVAL;
            }

            switch (src[src_i]) {
            case 'b':
                dst[dst_i++] = '\b';
                break;
            case 'f':
                dst[dst_i++] = '\f';
                break;
            case 'n':
                dst[dst_i++] = '\n';
                break;
            case 'r':
                dst[dst_i++] = '\r';
                break;
            case 't':
                dst[dst_i++] = '\t';
                break;
            case 'v':
                dst[dst_i++] = '\v';
                break;
            case '\'':
            case '\"':
            case '\\':
                dst[dst_i++] = src[src_i];
                break;
            case 'x':
                /* Hex Hex decode */

                /* Protect against array out of bounds. */
                if (src_i+2>=src_len) {
                    return IB_EINVAL;
                }

                /* Ensure that the next 2 characters are hex digits.
                 */
                for (i=1; i <= 2; i++) {
                    if ( ! isxdigit(src[src_i + i]) ) {
                        return IB_EINVAL;
                    }
                }

                src_i+=2;
                dst[dst_i] = hex_to_int(src[src_i-1], src[src_i]);

                /* UNESCAPE_NONULL flags prohibits nulls appearing
                 * mid-string. */
                if (flags & IB_UTIL_UNESCAPE_NONULL &&
                    dst[dst_i] == 0)
                {
                    return IB_EBADVAL;
                }

                ++dst_i;
                break;
            case 'u':
                /* Hex Hex Hex Hex decode */

                /* Protect against array out of bounds. */
                if ( src_i+4>=src_len ) {
                    return IB_EINVAL;
                }

                /* Ensure that the next 4 characters are hex digits.
                 */
                for ( i=1; i <= 4; i++ ) {
                    if ( ! isxdigit(src[src_i + i]) ) {
                        return IB_EINVAL;
                    }
                }

                /* Convert the first byte. */
                src_i+=2;
                dst[dst_i++] = hex_to_int(src[src_i-1], src[src_i]);

                /* Convert the second byte. */
                src_i+=2;
                dst[dst_i] = hex_to_int(src[src_i-1], src[src_i]);

                /* UNESCAPE_NONULL flags prohibits nulls appearing
                 * mid-string. */
                if ( flags & IB_UTIL_UNESCAPE_NONULL &&
                     ( dst[dst_i-1] == 0 || dst[dst_i] == 0 ) )
                {
                    return IB_EBADVAL;
                }

                ++dst_i;
                break;
            default:
                dst[dst_i++] = src[src_i];
            }
            break;
        default:
            dst[dst_i++] = src[src_i];
        }
    }

    /* Terminate the string. */
    if ( flags & IB_UTIL_UNESCAPE_NULTERMINATE ) {
        dst[dst_i] = '\0';
    }

    *dst_len = dst_i;

    return IB_OK;
}

char *ib_util_hex_escape(const char *src, size_t src_len)
{
    size_t dst_i = 0;
    size_t src_i = 0;
    size_t dst_len = src_len * 4 + 1;
    char *dst = malloc(dst_len);

    if ( dst == NULL ) {
        return dst;
    }

    for ( src_i = 0; src_i < src_len; ++src_i )
    {
        if ( isprint(src[src_i]) )
        {
            dst[dst_i] = src[src_i];
            ++dst_i;
        }
        else {
            dst_i += sprintf(dst + dst_i, "0x%x", src[src_i]);
        }
    }

    dst[dst_i] = '\0';

    return dst;
}
