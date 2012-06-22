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
 * @brief IronBee &mdash; String related functions
 * @author Nick LeRoy <nleroy@qualys.com>
 */

#include "ironbee_config_auto.h"

#include <assert.h>
#include <ctype.h>
#include <stdbool.h>

#include <ironbee/types.h>
#include <ironbee/debug.h>
#include <ironbee/util.h>
#include <ironbee/string.h>
#include <ironbee/mpool.h>

/**
 * NOTE: Be careful as these can ONLY be used on static values for X.
 * (i.e. VALID_HEX(c++) will NOT work)
 */
#define IS_HEX_CHAR(X) \
    ( ((X >= '0') && (X <= '9')) || \
      ((X >= 'a') && (X <= 'f')) || \
      ((X >= 'A') && (X <= 'F')) )
#define ISODIGIT(X) \
    ((X >= '0') && (X <= '7'))

#define NBSP 160

/**
 * Convert a byte from hex to a digit.
 *
 * Converts a byte given as its hexadecimal representation
 * into a proper byte. Handles uppercase and lowercase letters
 * but does not check for overflows.
 *
 * @param[in] ptr Pointer to input
 *
 * @returns The digit
 */
static uint8_t x2c(const uint8_t *ptr)
{
    IB_FTRACE_INIT();
    register uint8_t digit;
    register uint8_t c;

    c = *(ptr + 0);
    digit = ( (c >= 'A') ? ((c & 0xdf) - 'A') + 10 : (c - '0') );

    digit *= 16;

    c = *(ptr + 1);
    digit += ( (c >= 'A') ? ((c & 0xdf) - 'A') + 10 : (c - '0') );

    IB_FTRACE_RET_UINT(digit);
}

ib_status_t ib_util_decode_url(char *data,
                               ib_flags_t *result)
{
    IB_FTRACE_INIT();
    ib_status_t rc;
    size_t len;
    rc = ib_util_decode_url_ex((uint8_t *)data, strlen(data), &len, result);
    if (rc == IB_OK) {
        *(data+len) = '\0';
    }
    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_util_decode_url_ex(uint8_t *data_in,
                                  size_t dlen_in,
                                  size_t *dlen_out,
                                  ib_flags_t *result)
{
    IB_FTRACE_INIT();

    assert(data_in != NULL);
    assert(dlen_out != NULL);
    assert(result != NULL);

    uint8_t *out = data_in;
    uint8_t *in  = data_in;
    uint8_t *end = data_in + dlen_in;
    bool modified = false;

    while (in < end) {
        if (*in == '%') {
            /* Character is a percent sign. */

            /* Are there enough bytes available? */
            if (in + 2 < end) {
                char c1 = *(in + 1);
                char c2 = *(in + 2);

                if (IS_HEX_CHAR(c1) && IS_HEX_CHAR(c2)) {
                    /* Valid encoding - decode it. */
                    *out++ = x2c(in + 1);
                    in += 3;
                    modified = true;
                } else {
                    /* Not a valid encoding, skip this % */
                    if (in == out) {
                        ++out;
                        ++in;
                    }
                    else {
                        *out++ = *in++;
                        modified = true;
                    }
                }
            } else {
                /* Not enough bytes available, copy the raw bytes. */
                if (in == out) {
                    ++out;
                    ++in;
                }
                else {
                    *out++ = *in++;
                    modified = true;
                }
            }
        } else {
            /* Character is not a percent sign. */
            if (*in == '+') {
                *out++ = ' ';
                modified = true;
            } else if (out != in) {
                *out++ = *in;
                modified = true;
            }
            else {
                ++out;
            }
            ++in;
        }
    }
    *dlen_out = (out - data_in);
    *result = ( (modified == true) ?
                (IB_STRFLAG_ALIAS | IB_STRFLAG_MODIFIED) : IB_STRFLAG_ALIAS );

    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t ib_util_decode_url_cow(ib_mpool_t *mp,
                                   const char *data_in,
                                   char **data_out,
                                   ib_flags_t *result)
{
    IB_FTRACE_INIT();
    ib_status_t rc;
    size_t len;
    uint8_t *out;
    rc = ib_util_decode_url_cow_ex(mp, (uint8_t *)data_in, strlen(data_in),
                                   &out, &len, result);
    if (rc == IB_OK) {
        *(out+len) = '\0';
        *data_out = (char *)out;
    }
    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_util_decode_url_cow_ex(ib_mpool_t *mp,
                                      const uint8_t *data_in,
                                      size_t dlen_in,
                                      uint8_t **data_out,
                                      size_t *dlen_out,
                                      ib_flags_t *result)
{
    IB_FTRACE_INIT();

    assert(mp != NULL);
    assert(data_in != NULL);
    assert(data_out != NULL);
    assert(dlen_out != NULL);
    assert(result != NULL);

    uint8_t *out = NULL;
    const uint8_t *in  = data_in;
    const uint8_t *end = data_in + dlen_in;
    *data_out = NULL;

    while (in < end) {
        if (*in == '%') {
            /* Character is a percent sign. */

            /* Are there enough bytes available? */
            if (in + 2 < end) {
                char c1 = *(in + 1);
                char c2 = *(in + 2);

                if (IS_HEX_CHAR(c1) && IS_HEX_CHAR(c2)) {
                    /* Valid encoding - decode it. */
                    out = ib_util_copy_on_write(mp, data_in, in, dlen_in,
                                                out, data_out, NULL);
                    if (out == NULL) {
                        IB_FTRACE_RET_STATUS(IB_EALLOC);
                    }
                    *out++ = x2c(in + 1);
                    in += 3;
                } else {
                    /* Not a valid encoding, skip this % */
                    if (out == NULL) {
                        ++in;
                    }
                    else {
                        *out++ = *in++;
                    }
                }
            } else {
                /* Not enough bytes available, copy the raw bytes. */
                if (out == NULL) {
                    ++in;
                }
                else {
                    *out++ = *in++;
                }
            }
        } else {
            /* Character is not a percent sign. */
            if (*in == '+') {
                out = ib_util_copy_on_write(mp, data_in, in, dlen_in,
                                            out, data_out, NULL);
                if (out == NULL) {
                    IB_FTRACE_RET_STATUS(IB_EALLOC);
                }
                *out++ = ' ';
            } else if (out != NULL) {
                *out++ = *in;
            }
            ++in;
        }
    }

    if (out == NULL) {
        *result = IB_STRFLAG_ALIAS;
        *data_out = (uint8_t *)data_in;
        *dlen_out = dlen_in;
    }
    else {
        *result = IB_STRFLAG_NEWBUF | IB_STRFLAG_MODIFIED;
        *dlen_out = out - *data_out;
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

static void *memdup(const void *in, size_t len)
{
    IB_FTRACE_INIT();
    assert(in != NULL);
    void *p;

    if (len <= 0) {
        IB_FTRACE_RET_PTR(void, NULL);
    }
    p = malloc(len);
    if (p == NULL) {
        IB_FTRACE_RET_PTR(void, NULL);
    }
    memcpy(p, in, len);

    IB_FTRACE_RET_PTR(void, p);
}

ib_status_t ib_util_decode_html_entity(char *data,
                                       ib_flags_t *result)
{
    IB_FTRACE_INIT();
    ib_status_t rc;
    size_t len;
    rc = ib_util_decode_html_entity_ex((uint8_t *)data,
                                       strlen(data),
                                       &len,
                                       result);
    if (rc == IB_OK) {
        *(data+len) = '\0';
    }
    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_util_decode_html_entity_ex(uint8_t *data,
                                          size_t dlen_in,
                                          size_t *dlen_out,
                                          ib_flags_t *result)
{
    IB_FTRACE_INIT();
    assert(data != NULL);
    assert(dlen_out != NULL);
    assert(result != NULL);

    uint8_t *out = data;
    const uint8_t *in = data;
    const uint8_t *end;
    bool modified = false;

    end = data + dlen_in;
    while( (in < end) && (out < end) ) {
        size_t copy = 1;

        /* Require an ampersand and at least one character to
         * start looking into the entity.
         */
        if ( (*in == '&') && (in + 1 < end) ) {
            const uint8_t *t1 = in + 1;

            if (*t1 == '#') {
                /* Numerical entity. */
                ++copy;

                if (t1 + 1 >= end) {
                    goto HTML_ENT_OUT; /* Not enough bytes. */
                }
                ++t1;

                if ( (*t1 == 'x') || (*t1 == 'X') ) {
                    const uint8_t *t2;

                    /* Hexadecimal entity. */
                    ++copy;

                    if (t1 + 1 >= end) {
                        goto HTML_ENT_OUT; /* Not enough bytes. */
                    }
                    ++t1; /* j is the position of the first digit now. */

                    t2 = t1;
                    while( (t1 < end) && (isxdigit(*t1)) ) {
                        ++t1;
                    }
                    if (t1 > t2) { /* Do we have at least one digit? */
                        /* Decode the entity. */
                        char *tmp = memdup(t2, t1 - t2);
                        if (tmp == NULL) {
                            IB_FTRACE_RET_STATUS(IB_EALLOC);
                        }
                        *out++ = (uint8_t)strtol(tmp, NULL, 16);
                        modified = true;
                        free(tmp);

                        /* Skip over the semicolon if it's there. */
                        if ( (t1 < end) && (*t1 == ';') ) {
                             in = t1 + 1;
                        }
                        else {
                            in = t1;
                        }

                        continue;
                    } else {
                        goto HTML_ENT_OUT;
                    }
                } else {
                    /* Decimal entity. */
                    const uint8_t *t2 = t1;

                    while( (t1 < end) && (isdigit(*t1)) ) {
                        ++t1;
                    }
                    if (t1 > t2) { /* Do we have at least one digit? */
                        /* Decode the entity. */
                        char *tmp = memdup(t2, t1 - t2);
                        if (tmp == NULL) {
                            IB_FTRACE_RET_STATUS(IB_EALLOC);
                        }
                        *out++ = (uint8_t)strtol(tmp, NULL, 10);
                        modified = true;
                        free(tmp);

                        /* Skip over the semicolon if it's there. */
                        if ( (t1 < end) && (*t1 == ';') ) {
                             in = t1 + 1;
                        }
                        else {
                            in = t1;
                        }

                        continue;
                    } else {
                        goto HTML_ENT_OUT;
                    }
                }
            } else {
                /* Text entity. */
                const uint8_t *t2 = t1;

                while( (t1 < end) && (isalnum(*t1)) ) {
                    ++t1;
                }
                if (t1 > t2) { /* Do we have at least one digit? */
                    char *tmp = memdup(t2, t1 - t2);
                    if (tmp == NULL) {
                        IB_FTRACE_RET_STATUS(IB_EALLOC);
                    }

                    /* Decode the entity. */
                    /* ENH What about others? */
                    if (strcasecmp(tmp, "quot") == 0) {
                        modified = true;
                        *out++ = '"';
                    }
                    else if (strcasecmp(tmp, "amp") == 0) {
                        modified = true;
                        *out++ = '&';
                    }
                    else if (strcasecmp(tmp, "lt") == 0) {
                        modified = true;
                        *out++ = '<';
                    }
                    else if (strcasecmp(tmp, "gt") == 0) {
                        modified = true;
                        *out++ = '>';
                    }
                    else if (strcasecmp(tmp, "nbsp") == 0) {
                        modified = true;
                        *out++ = NBSP;
                    }
                    else {
                        /* We do no want to convert this entity,
                         * copy the raw data over. */
                        copy = t1 - t2 + 1;
                        free(tmp);
                        goto HTML_ENT_OUT;
                    }
                    free(tmp);

                    /* Skip over the semicolon if it's there. */
                    if ( (t1 < end) && (*t1 == ';') ) {
                        in = t1 + 1;
                    }
                    else {
                         in = t1;
                    }
                    continue;
                }
            }
        }

  HTML_ENT_OUT:
        {
            size_t z;
            for(z = 0; ( (z < copy) && (out < end) ); ++z) {
                *out++ = *in++;
            }
        }
    }

    *dlen_out = (out - data);
    *result = ( (modified == true) ?
                (IB_STRFLAG_ALIAS | IB_STRFLAG_MODIFIED) : IB_STRFLAG_ALIAS );
    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t ib_util_decode_html_entity_cow(ib_mpool_t *mp,
                                           const char *data_in,
                                           char **data_out,
                                           ib_flags_t *result)
{
    IB_FTRACE_INIT();
    ib_status_t rc;
    size_t len;
    uint8_t *out;
    rc = ib_util_decode_html_entity_cow_ex(mp,
                                           (uint8_t *)data_in,
                                           strlen(data_in),
                                           &out, &len,
                                           result);
    if (rc == IB_OK) {
        *(out+len) = '\0';
        *data_out = (char *)out;
    }
    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_util_decode_html_entity_cow_ex(ib_mpool_t *mp,
                                              const uint8_t *data_in,
                                              size_t dlen_in,
                                              uint8_t **data_out,
                                              size_t *dlen_out,
                                              ib_flags_t *result)
{
    IB_FTRACE_INIT();
    assert(mp != NULL);
    assert(data_in != NULL);
    assert(data_out != NULL);
    assert(dlen_out != NULL);
    assert(result != NULL);

    uint8_t *out = NULL;
    const uint8_t *in = data_in;
    const uint8_t *end_in = data_in + dlen_in;
    const uint8_t *end_out = NULL;
    *data_out = NULL;

    while( (in < end_in) && ((out == NULL) || (out < end_out)) ) {
        size_t copy = 1;

        /* Require an ampersand and at least one character to
         * start looking into the entity.
         */
        if ( (*in == '&') && (in + 1 < end_in) ) {
            const uint8_t *t1 = in + 1;

            if (*t1 == '#') {
                /* Numerical entity. */
                ++copy;

                if (t1 + 1 >= end_in) {
                    goto HTML_ENT_OUT; /* Not enough bytes. */
                }
                ++t1;

                if ( (*t1 == 'x') || (*t1 == 'X') ) {
                    const uint8_t *t2;

                    /* Hexadecimal entity. */
                    ++copy;

                    if (t1 + 1 >= end_in) {
                        goto HTML_ENT_OUT; /* Not enough bytes. */
                    }
                    ++t1; /* j is the position of the first digit now. */

                    t2 = t1;
                    while( (t1 < end_in) && (isxdigit(*t1)) ) {
                        ++t1;
                    }
                    if (t1 > t2) { /* Do we have at least one digit? */
                        /* Decode the entity. */
                        char *tmp = memdup(t2, t1 - t2);
                        if (tmp == NULL) {
                            IB_FTRACE_RET_STATUS(IB_EALLOC);
                        }
                        out = ib_util_copy_on_write(mp, data_in, in, dlen_in,
                                                    out, data_out, &end_out);
                        if (out == NULL) {
                            IB_FTRACE_RET_STATUS(IB_EALLOC);
                        }
                        *out++ = (uint8_t)strtol(tmp, NULL, 16);
                        free(tmp);

                        /* Skip over the semicolon if it's there. */
                        if ( (t1 < end_in) && (*t1 == ';') ) {
                             in = t1 + 1;
                        }
                        else {
                            in = t1;
                        }

                        continue;
                    } else {
                        goto HTML_ENT_OUT;
                    }
                } else {
                    /* Decimal entity. */
                    const uint8_t *t2 = t1;

                    while( (t1 < end_in) && (isdigit(*t1)) ) {
                        ++t1;
                    }
                    if (t1 > t2) { /* Do we have at least one digit? */
                        /* Decode the entity. */
                        char *tmp = memdup(t2, t1 - t2);
                        if (tmp == NULL) {
                            IB_FTRACE_RET_STATUS(IB_EALLOC);
                        }
                        out = ib_util_copy_on_write(mp, data_in, in, dlen_in,
                                                    out, data_out, &end_out);
                        if (out == NULL) {
                            IB_FTRACE_RET_STATUS(IB_EALLOC);
                        }
                        *out++ = (uint8_t)strtol(tmp, NULL, 10);
                        free(tmp);

                        /* Skip over the semicolon if it's there. */
                        if ( (t1 < end_in) && (*t1 == ';') ) {
                             in = t1 + 1;
                        }
                        else {
                            in = t1;
                        }

                        continue;
                    } else {
                        goto HTML_ENT_OUT;
                    }
                }
            } else {
                /* Text entity. */
                const uint8_t *t2 = t1;

                while( (t1 < end_in) && (isalnum(*t1)) ) {
                    ++t1;
                }
                if (t1 > t2) { /* Do we have at least one digit? */
                    uint8_t c;
                    char *tmp = memdup(t2, t1 - t2);
                    if (tmp == NULL) {
                        IB_FTRACE_RET_STATUS(IB_EALLOC);
                    }

                    /* Decode the entity. */
                    /* ENH What about others? */
                    if (strcasecmp(tmp, "quot") == 0) {
                        c = '"';
                    }
                    else if (strcasecmp(tmp, "amp") == 0) {
                        c = '&';
                    }
                    else if (strcasecmp(tmp, "lt") == 0) {
                        c = '<';
                    }
                    else if (strcasecmp(tmp, "gt") == 0) {
                        c = '>';
                    }
                    else if (strcasecmp(tmp, "nbsp") == 0) {
                        c = NBSP;
                    }
                    else {
                        /* We do no want to convert this entity,
                         * copy the raw data over. */
                        copy = t1 - t2 + 1;
                        free(tmp);
                        goto HTML_ENT_OUT;
                    }
                    free(tmp);

                    out = ib_util_copy_on_write(mp, data_in, in, dlen_in,
                                                out, data_out, &end_out);
                    if (out == NULL) {
                        IB_FTRACE_RET_STATUS(IB_EALLOC);
                    }
                    *out++ = c;

                    /* Skip over the semicolon if it's there. */
                    if ( (t1 < end_in) && (*t1 == ';') ) {
                        in = t1 + 1;
                    }
                    else {
                         in = t1;
                    }
                    continue;
                }
            }
        }

  HTML_ENT_OUT:
        if (out != NULL) {
            size_t z;
            for(z = 0; ( (z < copy) && (out < end_out) ); ++z) {
                *out++ = *in++;
            }
        }
        else {
            in += copy;
        }
    }

    if (out == NULL) {
        *result = IB_STRFLAG_ALIAS;
        *data_out = (uint8_t *)data_in;
        *dlen_out = dlen_in;
    }
    else {
        *result = IB_STRFLAG_NEWBUF | IB_STRFLAG_MODIFIED;
        *dlen_out = out - *data_out;
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}
