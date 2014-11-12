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
 * @brief IronBee --- Type conversion functions.
 * @author Sam Baskinger <sbaskinger@qualys.com>
 */

#include <ironbee/type_convert.h>

#include <assert.h>
#include <limits.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <inttypes.h>


/* Helper Functions. */

/**
 * Get the number of digits in a number
 *
 * @param[in] num The number to operate on
 *
 * @returns Number of digits (including '-')
 */
static size_t ib_num_digits(int64_t num);

/**
 * Get the size of a string buffer required to store a number
 *
 * @param[in] num The number to operate on
 *
 * @returns Required string length
 */
static size_t ib_num_buf_size(int64_t num);

static const int64_t  P10_INT64_LIMIT  = (INT64_MAX  / 10);

static size_t ib_num_digits(int64_t num)
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

static size_t ib_num_buf_size(int64_t num)
{
    size_t digits = ib_num_digits(num);
    return digits + 1;
}


ib_status_t ib_type_atoi_ex(
    const char *s,
    size_t      slen,
    int         base,
    ib_num_t   *result
)
{
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
    rc = ib_type_atoi(buf, base, result);
    free(buf);
    return rc;
}

ib_status_t ib_type_atoi(
    const char *s,
    int         base,
    ib_num_t   *result
)
{
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

ib_status_t ib_type_atot_ex(
    const char *s,
    size_t      slen,
    ib_time_t  *result
)
{
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
    rc = ib_type_atot(buf, result);

    free(buf);
    return rc;
}

ib_status_t ib_type_atot(
    const char *s,
    ib_time_t  *result
)
{
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

ib_status_t ib_type_atof_ex(
    const char *s,
    size_t      slen,
    ib_float_t *result
)
{
    assert(result != NULL);

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
    rc = ib_type_atof(sdup, result);

    free(sdup);

    return rc;
}

ib_status_t ib_type_atof(const char *s, ib_float_t *result)
{
    assert(result != NULL);

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

const char *ib_type_itoa(
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

const char *ib_type_ttoa(ib_mm_t mm, ib_time_t value)
{
    size_t size = ib_num_buf_size(value);
    char *buf = ib_mm_alloc(mm, size);
    if (buf != NULL) {
        snprintf(buf, size, "%"PRIu64, value);
    }
    return buf;
}

const char *ib_type_ftoa(
    ib_mm_t mm,
    long double value
) {
    char *buf = ib_mm_alloc(mm, 10);
    if (buf != NULL) {
        snprintf(buf, 10, "%Lf", value);
    }
    return buf;
}
