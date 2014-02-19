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
 * @brief IronBee - String related functions
 * @author Nick LeRoy <nleroy@qualys.com>
 */

#include "ironbee_config_auto.h"

#include <ironbee/mpool.h>
#include <ironbee/string.h>
#include <ironbee/types.h>

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

/**
 * Count the amount of whitespace in a string function
 *
 * @param[in] minlen Minimum length of a run of whitespace to count
 * @param[in] data String to analyze
 * @param[in] dlen Length of @a dlen
 * @param[out] count Number of whitespace characters of runs > whitespace
 * @param[out] other Number of whitespace characters that are not ' '.
 */
typedef void (* count_fn_t)(size_t minlen,
                            const uint8_t *data,
                            size_t dlen,
                            size_t *count,
                            size_t *other);

/**
 * In-place whitespace removal/compression function
 *
 * @param[in,out] buf Buffer to operate on
 * @param[in] dlen_in Input length
 * @param[out] dlen_out Length after whitespace removal
 * @param[out] result Result flags (@c IB_STRFLAG_xxx)
 *
 * @returns Status code.
 */
typedef ib_status_t (* inplace_fn_t)(uint8_t *buf,
                                     size_t dlen_in,
                                     size_t *dlen_out,
                                     ib_flags_t *result);
/**
 * Non-in-place whitespace removal/compression function
 *
 * @param[in] data_in Input buffer
 * @param[in] dlen_in Length of @a data_in
 * @param[out] data_out Output buffer
 * @param[in] dlen_out Length of @a data_out
 *
 * @returns Status code.
 */
typedef ib_status_t (* outplace_fn_t)(const uint8_t *data_in,
                                      size_t dlen_in,
                                      uint8_t *data_out,
                                      size_t dlen_out);

/**
 * Count the amount of whitespace in a string
 *
 * @param[in] force_other_zero If true, force @a other to zero
 * @param[in] minlen Minimum length of a run of whitespace to count
 * @param[in] data String to analyze
 * @param[in] dlen Length of @a dlen
 * @param[out] count Number of whitespace characters of runs > whitespace
 * @param[out] other Number of whitespace characters that are not ' '.
 */
static void ws_count(bool force_other_zero,
                     size_t minlen,
                     const uint8_t *data,
                     size_t dlen,
                     size_t *count,
                     size_t *other)
{
    const uint8_t *end;
    size_t icount = 0;     /* Internal count */
    size_t iother = 0;     /* Internal other */
    size_t runlen = 0;

    /* Loop through the whole string */
    end = data + dlen;
    while (data < end) {
        uint8_t c = *data;
        if (isspace(c) == 0) {
            runlen = 0;
        }
        else {
            ++runlen;
            if (runlen >= minlen) {
                ++icount;
            }
            if (c != ' ') {
                ++iother;
            }
        }
        ++data;
    }

    *count = icount;
    *other = (force_other_zero) ? 0 : iother;
    return;
}

/**
 * Count the amount of whitespace in a string for whitespace removal
 *
 * @param[in] minlen Minimum length of a run of whitespace to count
 * @param[in] data String to analyze
 * @param[in] dlen Length of @a dlen
 * @param[out] count Number of whitespace characters of runs > whitespace
 * @param[out] other Number of whitespace characters that are not ' '.
 */
static void ws_remove_count(size_t minlen,
                            const uint8_t *data,
                            size_t dlen,
                            size_t *count,
                            size_t *other)
{
    ws_count(true, minlen, data, dlen, count, other);
    return;
}

/**
 * In-place whitespace removal
 *
 * @param[in,out] buf Buffer to operate on
 * @param[in] dlen_in Input length
 * @param[out] dlen_out Length after whitespace removal
 * @param[out] result Result flags (@c IB_STRFLAG_xxx)
 *
 * @returns Status code.
 */
static ib_status_t ws_remove_inplace(uint8_t *buf,
                                     size_t dlen_in,
                                     size_t *dlen_out,
                                     ib_flags_t *result)
{
    const uint8_t *iend;
    const uint8_t *iptr;
    uint8_t *optr;

    assert(buf != NULL);
    assert(dlen_out != NULL);
    assert(result != NULL);

    *result = IB_STRFLAG_ALIAS;

    /* Special case zero length string */
    if (dlen_in == 0) {
        *dlen_out = 0;
        return IB_OK;
    }

    /* Loop through all of the input */
    optr = buf;
    iptr = buf;
    iend = buf + dlen_in;
    while (iptr < iend) {
        uint8_t c = *iptr;
        if (isspace(c) == 0) {
            *optr = c;
            ++optr;
        }
        ++iptr;
    }

    /* Store the output length & result */
    *dlen_out = (optr - buf);
    if (*dlen_out != dlen_in) {
        *result |= IB_STRFLAG_MODIFIED;
    }
    return IB_OK;
}

/**
 * Non-inline whitespace removal
 *
 * @param[in] data_in Input buffer
 * @param[in] dlen_in Length of @a data_in
 * @param[out] data_out Output buffer
 * @param[in] dlen_out Length of @a data_out
 *
 * @returns Status code.
 */
static ib_status_t ws_remove(const uint8_t *data_in,
                             size_t dlen_in,
                             uint8_t *data_out,
                             size_t dlen_out)
{
    const uint8_t *iend;
    const uint8_t *oend;
    uint8_t *optr;

    assert(data_in != NULL);
    assert(data_out != NULL);
    assert(dlen_out <= dlen_in);

    /* Special case zero length string */
    if (dlen_in == 0) {
        return IB_OK;
    }

    /* Loop through all of the input */
    optr = data_out;
    oend = data_out + dlen_out;
    iend = data_in + dlen_in;
    while (data_in < iend) {
        uint8_t c = *data_in;
        if (isspace(c) == 0) {
            assert (optr <= oend);
            *optr = c;
            ++optr;
        }
        ++data_in;
    }

    return IB_OK;
}

/**
 * Count the amount of whitespace in a string for whitespace compression
 *
 * @param[in] minlen Minimum length of a run of whitespace to count
 * @param[in] data String to analyze
 * @param[in] dlen Length of @a dlen
 * @param[out] count Number of whitespace characters of runs > whitespace
 * @param[out] other Number of whitespace characters that are not ' '.
 */
static void ws_compress_count(size_t minlen,
                              const uint8_t *data,
                              size_t dlen,
                              size_t *count,
                              size_t *other)
{
    ws_count(false, minlen, data, dlen, count, other);
    return;
}

/**
 * Inline whitespace compression
 *
 * @param[in,out] buf Buffer to operate on
 * @param[in] dlen_in Input length
 * @param[out] dlen_out Length after whitespace compression
 * @param[out] result Result flags (@c IB_STRFLAG_xxx)
 *
 * @returns Status code.
 */
static ib_status_t ws_compress_inplace(uint8_t *buf,
                                       size_t dlen_in,
                                       size_t *dlen_out,
                                       ib_flags_t *result)
{
    const uint8_t *iend;
    const uint8_t *iptr;
    uint8_t *optr;
    bool in_wspc = false;
    bool modified = false;

    assert(buf != NULL);
    assert(dlen_out != NULL);
    assert(result != NULL);

    *result = IB_STRFLAG_ALIAS;

    /* Special case zero length string */
    if (dlen_in == 0) {
        *dlen_out = 0;
        return IB_OK;
    }

    /* Loop through all of the input */
    optr = buf;
    iptr = buf;
    iend = buf + dlen_in;
    while (iptr < iend) {
        uint8_t c = *iptr;
        if (isspace(c) == 0) {
            *optr = c;
            ++optr;
            in_wspc = false;
        }
        else if (in_wspc) {
            modified = true;
        }
        else if (! in_wspc) {
            *optr = ' ';
            ++optr;
            in_wspc = true;
            if (c != ' ') {
                modified = true;
            }
        }
        ++iptr;
    }

    /* Store the output length & result */
    *dlen_out = (optr - buf);
    if (modified) {
        *result |= IB_STRFLAG_MODIFIED;
    }
    return IB_OK;
}

/**
 * Non-inline whitespace compression
 *
 * @param[in] data_in Input buffer
 * @param[in] dlen_in Length of @a data_in
 * @param[out] data_out Output buffer
 * @param[in] dlen_out Length of @a data_out
 *
 * @returns Status code.
 */
static ib_status_t ws_compress(const uint8_t *data_in,
                               size_t dlen_in,
                               uint8_t *data_out,
                               size_t dlen_out)
{
    const uint8_t *iend;
    const uint8_t *oend;
    uint8_t *optr;
    bool in_wspc = false;

    assert(data_in != NULL);
    assert(data_out != NULL);
    assert(dlen_out <= dlen_in);

    /* Special case zero length string */
    if (dlen_in == 0) {
        return IB_OK;
    }

    /* Loop through all of the input */
    optr = data_out;
    oend = data_out + dlen_out;
    iend = data_in + dlen_in;
    while (data_in < iend) {
        uint8_t c = *data_in;
        if (isspace(c) == 0) {
            assert (optr < oend);
            *optr = c;
            ++optr;
            in_wspc = false;
        }
        else if (! in_wspc) {
            assert (optr < oend);
            *optr = ' ';
            ++optr;
            in_wspc = true;
        }
        ++data_in;
    }

    return IB_OK;
}

/**
 * Perform whitespace removal / compression
 *
 * @param[in] op String trim operation
 * @param[in] mp Memory pool
 * @param[in] minlen Minimum length of a run of whitespace to count
 * @param[in] nul Add NUL byte
 * @param[in] fn_count Function to count whitespace
 * @param[in] fn_inplace In-place whitespace removal/compression function
 * @param[in] fn_outplace Non-in-place whitespace removal/compression function
 * @param[in] data_in Pointer to input data
 * @param[in] dlen_in Length of @a data_in
 * @param[out] data_out Pointer to output data
 * @param[in] dlen_out Length of @a data_out
 * @param[out] result Flags detailing the result (@c IB_STRFLAG_xx)
 *
 * @result Status code
 */
static ib_status_t ws_op(ib_strop_t op,
                         ib_mm_t mm,
                         size_t minlen,
                         bool nul,
                         count_fn_t fn_count,
                         inplace_fn_t fn_inplace,
                         outplace_fn_t fn_outplace,
                         uint8_t *data_in,
                         size_t dlen_in,
                         uint8_t **data_out,
                         size_t *dlen_out,
                         ib_flags_t *result)
{
    size_t count;
    size_t other;
    size_t olen;
    ib_status_t rc = IB_OK;

    assert(minlen > 0);
    assert(fn_inplace != NULL);
    assert(fn_outplace != NULL);
    assert(data_in != NULL);
    assert(data_out != NULL);
    assert(dlen_out != NULL);
    assert(result != NULL);

    switch(op) {
    case IB_STROP_INPLACE:
        *data_out = data_in;
        rc = fn_inplace(data_in, dlen_in, dlen_out, result);
        break;

    case IB_STROP_COPY:
        fn_count(minlen, data_in, dlen_in, &count, &other);
        olen = dlen_in - count;
        *data_out = ib_mm_alloc(mm, olen + (nul ? 1 : 0));
        if (*data_out == NULL) {
            return IB_EALLOC;
        }
        if ( (count == 0) && (other == 0) ) {
            *result = (IB_STRFLAG_NEWBUF);
            memcpy(*data_out, data_in, dlen_in);
        }
        else {
            *result = (IB_STRFLAG_NEWBUF | IB_STRFLAG_MODIFIED);
            rc = fn_outplace(data_in, dlen_in, *data_out, olen);
        }
        *dlen_out = olen;
        break;

    case IB_STROP_COW:
        fn_count(minlen, data_in, dlen_in, &count, &other);
        if ( (count == 0) && (other == 0) ) {
            *data_out = data_in;
            *dlen_out = dlen_in;
            *result = (IB_STRFLAG_ALIAS);
        }
        else {
            olen = dlen_in - count;
            *data_out = ib_mm_alloc(mm, olen + (nul ? 1 : 0));
            if (*data_out == NULL) {
                return IB_EALLOC;
            }
            *result = (IB_STRFLAG_MODIFIED | IB_STRFLAG_NEWBUF);
            rc = fn_outplace(data_in, dlen_in, *data_out, olen);
            *dlen_out = olen;
        }
        break;

    default:
        return IB_EINVAL;
    }

    if (nul) {
        *(*data_out + (*dlen_out)) = '\0';
    }
    return rc;
}

/* Delete all whitespace from a string (extended version) */
ib_status_t ib_str_wspc_remove_ex(ib_strop_t op,
                                  ib_mm_t mm,
                                  uint8_t *data_in,
                                  size_t dlen_in,
                                  uint8_t **data_out,
                                  size_t *dlen_out,
                                  ib_flags_t *result)
{
    ib_status_t rc = IB_OK;

    assert(data_in != NULL);
    assert(data_out != NULL);
    assert(dlen_out != NULL);
    assert(result != NULL);

    rc = ws_op(op, mm,
               1, false,
               ws_remove_count, ws_remove_inplace, ws_remove,
               data_in, dlen_in,
               data_out, dlen_out, result);

    return rc;
}

/* Delete all whitespace from a string (NUL terminated string version) */
ib_status_t ib_str_wspc_remove(ib_strop_t op,
                               ib_mm_t mm,
                               char *data_in,
                               char **data_out,
                               ib_flags_t *result)
{
    size_t len;
    ib_status_t rc;

    assert(data_in != NULL);
    assert(data_out != NULL);
    assert(result != NULL);

    rc = ws_op(op, mm,
               1, true,
               ws_remove_count, ws_remove_inplace, ws_remove,
               (uint8_t *)data_in, strlen(data_in),
               (uint8_t **)data_out, &len, result);

    return rc;
}

/* Compress whitespace in a string (extended version) */
ib_status_t ib_str_wspc_compress_ex(ib_strop_t op,
                                    ib_mm_t mm,
                                    uint8_t *data_in,
                                    size_t dlen_in,
                                    uint8_t **data_out,
                                    size_t *dlen_out,
                                    ib_flags_t *result)
{
    ib_status_t rc = IB_OK;

    assert(data_in != NULL);
    assert(data_out != NULL);
    assert(dlen_out != NULL);
    assert(result != NULL);

    rc = ws_op(op, mm,
               2, false,
               ws_compress_count, ws_compress_inplace, ws_compress,
               data_in, dlen_in,
               data_out, dlen_out, result);

    return rc;
}

/* Compress whitespace in a string (NUL terminated string version) */
ib_status_t ib_str_wspc_compress(ib_strop_t op,
                                 ib_mm_t mm,
                                 char *data_in,
                                 char **data_out,
                                 ib_flags_t *result)
{
    size_t len;
    ib_status_t rc;

    assert(data_in != NULL);
    assert(data_out != NULL);
    assert(result != NULL);

    rc = ws_op(op, mm,
               2, true,
               ws_compress_count, ws_compress_inplace, ws_compress,
               (uint8_t *)data_in, strlen(data_in),
               (uint8_t **)data_out, &len, result);

    return rc;
}
