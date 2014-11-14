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
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 * @author Nick LeRoy <nleroy@qualys.com>
 */

#include "ironbee_config_auto.h"

#include <ironbee/escape.h>
#include <ironbee/string.h>
#include <ironbee/type_convert.h>

#include <assert.h>
#include <ctype.h>
#include <stdio.h>

ib_status_t ib_string_escape_json_buf(
    const uint8_t *data_in,
    size_t dlen_in,
    char *data_out,
    size_t dsize_out,
    size_t *dlen_out
)
{
    assert(data_out != NULL);

    const uint8_t *iptr;
    const uint8_t *iend = data_in + dlen_in;
    char *optr;
    const char *oend;
    ib_status_t rc = IB_OK;

    if (data_in == NULL) {
        assert(dlen_in == 0);
        data_in = (const uint8_t *)"";
    }

    oend = data_out + dsize_out - 2;
    optr = data_out;
    *optr = '\"';
    ++optr;
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
            break;
        }
        if (size == 1) {
            *optr = (char)*iptr;
            ++optr;
        }
        else {
            memcpy(optr, ostr, size);
            optr += size;
        }
   }

    *optr = '\"';
    ++optr;
    *optr = '\0';
    ++optr;

    if (dlen_out != NULL) {
        *dlen_out = optr - data_out - 1;
    }
    return rc;
}

ib_status_t ib_util_unescape_string(
    char       *dst,
    size_t     *dst_len,
    const char *src,
    size_t      src_len
)
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
                dst[dst_i] = ib_type_htoa(src[src_i-1], src[src_i]);

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
                dst[dst_i++] = ib_type_htoa(src[src_i-1], src[src_i]);

                /* Convert the second byte. */
                src_i+=2;
                dst[dst_i] = ib_type_htoa(src[src_i-1], src[src_i]);

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

    *dst_len = dst_i;

    return IB_OK;
}

char *ib_util_hex_escape(
    ib_mm_t        mm,
    const uint8_t *src,
    size_t         src_len
)
{
    size_t buf_size = src_len * 4 + 1;
    char *buf;

    if (src == NULL) {
        src = (const uint8_t *)"";
    }

    buf = ib_mm_alloc(mm, buf_size);
    if (buf == NULL) {
        return NULL;
    }

    const uint8_t *src_ptr;
    const uint8_t *src_end = (uint8_t *)src + src_len;
    char          *dst_ptr;
    const char    *dst_end = buf + buf_size;

    for (src_ptr = src, dst_ptr = buf;
         (src_ptr < src_end) && (dst_ptr < (dst_end-3));
         ++src_ptr)
    {
        if (isprint(*src_ptr))
        {
            *dst_ptr = *src_ptr;
            ++dst_ptr;
        }
        else {
            size_t avail = dst_end - dst_ptr;
            int written;

            assert(avail >= 3);
            written = snprintf(dst_ptr, avail, "0x%x", *src_ptr);
            assert(written <= 4);
            if (written < 3) {
                break;
            }
            dst_ptr += written;
        }
    }
    *dst_ptr = '\0';

    return buf;
}
