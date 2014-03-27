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
 * @brief IronBee --- String related functions
 * @author Nick LeRoy <nleroy@qualys.com>
 */

#include "ironbee_config_auto.h"

#include <ironbee/string.h>

#include <ironbee/types.h>

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

/**
 * Length of the string buffer for converting strings.
 */
/**
 * Convert a string (with length) to a number.
 */
ib_status_t ib_string_to_num_ex(
    const char *s,
    size_t slen,
    int base,
    ib_num_t *result
) {
    assert(result != NULL);

    char *buf;
    ib_status_t rc;

    /* Check for zero length string */
    if ( (s == NULL) || (slen == 0) ) {
        return IB_EINVAL;
    }

    buf = malloc(slen+1);
    if (buf == NULL) {
        return IB_EALLOC;
    }
    memcpy(buf, s, slen);
    buf[slen] = '\0';
    rc = ib_string_to_num(buf, base, result);
    free(buf);
    return rc;
}

/**
 * Convert a string (with length) to a number.
 */
ib_status_t ib_string_to_num(
    const char *s,
    int base,
    ib_num_t *result
) {
    assert(result != NULL);

    size_t slen;
    char *end;
    long int value;
    size_t vlen;

    /* Check for zero length string */
    if ( (s == NULL) || (*s == '\0') ) {
        return IB_EINVAL;
    }

    slen = strlen(s);

    /* Do the conversion, check for errors */
    value = strtol(s, &end, base);
    vlen = (end - s);
    if (vlen != slen) {
        return IB_EINVAL;
    }
    else if ( ((value == LONG_MIN) || (value == LONG_MAX)) &&
              (errno == ERANGE) )
    {
        return IB_EINVAL;
    }
    else {
        *result = value;
        return IB_OK;
    }
}

ib_status_t ib_string_to_time_ex(
    const char *s,
    size_t slen,
    ib_time_t *result
) {
    assert(result != NULL);

    ib_status_t rc;
    char *buf;

    /* Check for zero length string */
    if ( (s == NULL) || (slen == 0) ) {
        return IB_EINVAL;
    }

    buf = malloc(slen+1);
    if (buf == NULL) {
        return IB_EALLOC;
    }

    memcpy(buf, s, slen);
    buf[slen] = '\0';
    rc = ib_string_to_time(buf, result);

    free(buf);
    return rc;
}

ib_status_t ib_string_to_time(
    const char *s,
    ib_time_t *result
) {
    assert(result != NULL);

    size_t slen;
    char *end;
    unsigned long int value;
    size_t vlen;

    /* Check for zero length string */
    if ( (s == NULL) || (*s == '\0') ) {
        return IB_EINVAL;
    }

    slen = strlen(s);

    /* Do the conversion, check for errors */
    value = strtoul(s, &end, 0);
    vlen = (end - s);
    if (vlen != slen) {
        return IB_EINVAL;
    }
    else if ( (value == ULONG_MAX) && (errno == ERANGE) )
    {
        return IB_EINVAL;
    }
    else {
        *result = value;
        return IB_OK;
    }
}


/**
 * Convert a string to a ib_float_t.
 */
ib_status_t ib_string_to_float_ex(
    const char *s,
    size_t slen,
    ib_float_t *result
) {
    ib_status_t rc;

    /* Check for zero length string */
    if ( (s == NULL) || (*s == '\0') ) {
        return IB_EINVAL;
    }

    char *sdup = strndup(s, slen);

    if ( ! sdup ) {
        return IB_EALLOC;
    }

    /* In this case the _ex function calls out to the no-length function. */
    rc = ib_string_to_float(sdup, result);

    free(sdup);

    return rc;
}

ib_status_t ib_string_to_float(const char *s, ib_float_t *result)
{
    char *endptr;
    const char *send;
    ib_float_t val;
    size_t len;

    *result = 0.0;

    /* Check for zero length string */
    if ( (s == NULL) || (*s == '\0') ) {
        return IB_EINVAL;
    }

    /* Get the length */
    len = strlen(s);
    send = s + len;

    errno = 0;
    val = strtold(s, &endptr);

    /* Conversion failed */
    if (endptr != send) {
        return IB_EINVAL;
    }

    /* Check for Underflow would occur. */
    if ( (val == 0.0) && (errno == ERANGE) ) {
        return IB_EINVAL;
    }

    /* Overflow would occur. */
    if ( ((val == HUGE_VALL) || (val == -HUGE_VALL)) && (errno == ERANGE)) {
        return IB_EINVAL;
    }

    *result = val;
    return IB_OK;
}


/**
 * strstr() clone that works with non-NUL terminated strings
 */
const char *ib_strstr_ex(
    const char *haystack,
    size_t      haystack_len,
    const char *needle,
    size_t      needle_len
) {
    size_t i = 0;
    size_t imax;

    /* If either pointer is NULL or either length is zero, done */
    if ( (haystack == NULL) || (haystack_len == 0) ||
         (needle == NULL) || (needle_len == 0) )
    {
        return NULL;
    }

    /* Search for the needle */
    imax = haystack_len - (needle_len-1);
    for (i = 0; i < imax; ++i) {
        const char *hp = haystack + i;
        bool found = true;
        size_t j = 0;

        for (j = 0; j < needle_len; ++j) {
            if ( *(hp + j) != *(needle + j) ) {
                found = false;
                break;
            }
        }
        if (found) {
            return hp;
        }
    }

    return NULL;
}

/**
 * Reverse strstr() clone that works with non-NUL terminated strings
 */
const char *ib_strrstr_ex(
    const char *haystack,
    size_t      haystack_len,
    const char *needle,
    size_t      needle_len
) {
    size_t imax;
    const char *hp;

    /* If either pointer is NULL or either length is zero, done */
    if ( (haystack == NULL) || (haystack_len == 0) ||
         (needle == NULL) || (needle_len == 0) )
    {
        return NULL;
    }

    /* Search for the needle */
    imax = haystack_len - needle_len;
    for (hp = haystack + imax; hp >= haystack; --hp) {
        bool found = true;
        size_t j = 0;

        for (j = 0; j < needle_len; ++j) {
            if ( *(hp + j) != *(needle + j) ) {
                found = false;
                break;
            }
        }
        if (found) {
            return hp;
        }
    }

    return NULL;
}

static const int64_t  P10_INT64_LIMIT  = (INT64_MAX  / 10);
static const uint64_t P10_UINT64_LIMIT = (UINT64_MAX / 10);

size_t ib_num_digits(int64_t num)
{
    size_t n = 1;
    int64_t po10;

    if (num < 0) {
        num = -num;
        ++n;
    }

    po10 = 10;
    while (num >= po10) {
        ++n;
        if (po10 > P10_INT64_LIMIT)
            break;
        po10 *= 10;
    }
    return n;
}

size_t ib_unum_digits(uint64_t num)
{
    size_t n = 1;
    uint64_t po10;

    po10 = 10;
    while (num >= po10) {
        ++n;
        if (po10 > P10_UINT64_LIMIT)
            break;
        po10 *= 10;
    }
    return n;
}

size_t ib_num_buf_size(int64_t num)
{
    size_t digits = ib_num_digits(num);
    return digits + 1;
}

size_t ib_unum_buf_size(uint64_t unum)
{
    size_t digits = ib_unum_digits(unum);
    return digits + 1;
}

const char *ib_num_to_string(
    ib_mm_t mm,
    int64_t value
) {
    size_t size = ib_num_buf_size(value);
    char *buf = ib_mm_alloc(mm, size);
    if (buf != NULL) {
        snprintf(buf, size, "%"PRId64, value);
    }
    return buf;
}

const char *ib_time_to_string(ib_mm_t mm, ib_time_t value)
{
    size_t size = ib_num_buf_size(value);
    char *buf = ib_mm_alloc(mm, size);
    if (buf != NULL) {
        snprintf(buf, size, "%"PRIu64, value);
    }
    return buf;
}

const char *ib_unum_to_string(
    ib_mm_t mm,
    uint64_t value
) {
    size_t size = ib_unum_buf_size(value);
    char *buf = ib_mm_alloc(mm, size);
    if (buf != NULL) {
        snprintf(buf, size, "%"PRIu64, value);
    }
    return buf;
}

const char *ib_float_to_string(
    ib_mm_t mm,
    long double value
) {
    char *buf = ib_mm_alloc(mm, 10);
    if (buf != NULL) {
        snprintf(buf, 10, "%Lf", value);
    }
    return buf;
}

/**
 * Look for a character in a string that can have embedded NUL characters
 * in it.  This version will ignore NUL characters.
 */
ib_status_t ib_strchr_nul_ignore(
    const char *str,
    size_t len,
    int c,
    ssize_t *offset
) {
    const char *p;

    for ( p=str;  len > 0;  ++p, --len) {
        if (*p == c) {
            *offset = (p - str);
            return IB_OK;
        }
    }
    *offset = -1;
    return IB_OK;
}

ib_status_t ib_strchr_nul_error(
    const char *str,
    size_t len,
    int c,
    ssize_t *offset
) {
    const char *p;

    for ( p=str;  len > 0;  ++p, --len) {
        if (*p == c) {
            *offset = (p - str);
            return IB_OK;
        }
        else if (*p == '\0') {
            *offset = -1;
            return IB_EINVAL;
        }
    }
    *offset = -1;
    return IB_OK;
}
