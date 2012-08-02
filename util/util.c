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
 *****************************************************************************/

/**
 * @file
 * @brief IronBee &mdash; Utility Functions
 * @author Brian Rectanus <brectanus@qualys.com>
 */

#include "ironbee_config_auto.h"

#include <ironbee/util.h>

#include <ironbee/debug.h>
#include <ironbee/uuid.h>

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <libgen.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

/* -- Logging -- */

static struct _ibutil_logger_t {
    ib_util_fn_logger_t   callback;
    int                   level;
    void                 *cbdata;
} _ibutil_logger;

/**
 * Builtin Logger.
 */
static void _builtin_logger(void *cbdata, int level,
                            const char *file, int line,
                            const char *fmt, va_list ap)
{
    FILE *fp = (FILE *)cbdata;
    char fmt2[1024 + 1];

    assert(fp != NULL);
    assert(fmt != NULL);

    if ((file != NULL) && (line > 0)) {
        int ec = snprintf(fmt2, 1024,
                          "[%d] (%s:%d) %s\n",
                          level, file, line, fmt);
        /* It is a coding error if this format string is too big. */
        assert(ec <= 1024);
    }
    else {
        int ec = snprintf(fmt2, 1024,
                          "[%d] %s\n",
                          level, fmt);
        /* It is a coding error if this format string is too big. */
        assert(ec <= 1024);
    }

    vfprintf(fp, fmt2, ap);
    fflush(fp);
}

ib_status_t ib_util_log_level(int level)
{
    _ibutil_logger.level = level;

    return IB_OK;
}

int ib_util_get_log_level(void)
{
    return _ibutil_logger.level;
}

ib_status_t ib_util_log_logger(ib_util_fn_logger_t callback,
                               void *cbdata)
{
    _ibutil_logger.callback = callback;
    _ibutil_logger.cbdata = cbdata;

    return IB_OK;
}

void ib_util_log_ex(int level,
                    const char *file, int line,
                    const char *fmt, ...)
{
    va_list ap;

    if ((_ibutil_logger.callback == NULL) || (level > _ibutil_logger.level)) {
        return;
    }

    va_start(ap, fmt);
    /* Only pass on the file/line data if we are in DEBUG level or higher. */
    if (_ibutil_logger.level >= 7) {
        _ibutil_logger.callback(_ibutil_logger.cbdata,
                                level, file, line, fmt, ap);
    }
    else {
        _ibutil_logger.callback(_ibutil_logger.cbdata,
                                level, NULL, 0, fmt, ap);
    }

    va_end(ap);
}


/* -- Misc -- */

/**
 * @brief Convert the input character to the byte value represented by
 *        it's hexadecimal value. The input of 'F' results in the
 *        character 15 being returned. If a character is not
 *        submitted that is hexadecimal, then -1 is returned.
 * @param[in] a the input character to be converted. A-F, a-f, or 0-9.
 * @returns The byte value of the passed in hexadecimal character or -1 on
 *          failure.
 */
static inline char hexchar_to_byte(char a) {
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
static inline char hex_to_int(char high, char low) {
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

uint8_t *ib_util_copy_on_write(ib_mpool_t *mp,
                               const uint8_t *data_in,
                               const uint8_t *end_in,
                               size_t size,
                               uint8_t *cur_out,
                               uint8_t **data_out,
                               const uint8_t **end_out)
{
    IB_FTRACE_INIT();
    assert(mp != NULL);
    assert(data_in != NULL);
    assert(data_out != NULL);
    assert(end_in != NULL);
    assert(end_in >= data_in);

    if (*data_out == NULL) {
        *data_out = ib_mpool_alloc(mp, size);
        if (*data_out != NULL) {
            if (end_out != NULL) {
                *end_out = *data_out + size;
            }
            size_t off = end_in - data_in;
            if (off == 0) {
                /* Do nothing */
            }
            else if (off == 1) {
                **data_out = *data_in;
            }
            else {
                memcpy(*data_out, data_in, off);
            }
            cur_out = *data_out + off;
        }
    }

    IB_FTRACE_RET_PTR(uint8_t, cur_out);
}

void *ib_util_memdup(ib_mpool_t *mp,
                     const void *in,
                     size_t len,
                     bool nul)
{
    IB_FTRACE_INIT();
    assert(in != NULL);

    void *p;
    size_t size = len;
    if (nul) {
        ++size;
    }

    if (len <= 0) {
        IB_FTRACE_RET_PTR(void, NULL);
    }
    if (mp != NULL) {
        p = ib_mpool_alloc(mp, size);
    }
    else {
        p = malloc(size);
    }
    if (p == NULL) {
        IB_FTRACE_RET_PTR(void, NULL);
    }
    memcpy(p, in, len);
    if (nul) {
        *((char *)p + len) = '\0';
    }

    IB_FTRACE_RET_PTR(void, p);
}

/* -- Library Setup -- */

ib_status_t ib_initialize(void)
{
    ib_status_t rc;

    rc = ib_util_log_logger(_builtin_logger, stderr);
    if (rc != IB_OK) {
        rc = ib_util_log_logger(NULL, NULL);
        return rc;
    }

    ib_util_log_level(3);

    rc = ib_uuid_initialize();
    if (rc != IB_OK) {
        return rc;
    }

    return IB_OK;
}

void ib_shutdown(void)
{
    ib_uuid_shutdown();
}

