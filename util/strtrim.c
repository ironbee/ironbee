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
 * Special case return value for search_{left,right}
 */
#define ALL_WHITESPACE ((size_t) -1) /**< String is entirely whitespace */

/**
 * Search start from the left for the first non-whitespace.
 *
 * @param[in] str String to search
 * @param[in] len Length of string
 *
 * @returns Offset of first non-whitespace, -1 if none found
 */
static size_t find_nonws_left(const uint8_t *str,
                              size_t len)
{
    assert (str != NULL);
    const uint8_t *cur;
    const uint8_t *end = (str + len);

    /* Special case: length of zero */
    if (len == 0) {
        return 0;
    }

    /* Loop through all of the input until we find the first non-space */
    for (cur = str;  cur < end;  ++cur) {
        if (isspace(*cur) == 0) {
            return cur - str;
        }
    }

    /* No non-whitespace found */
    return ALL_WHITESPACE;
}

/**
 * Return a zero-length string that may be an alias into the original
 * or a new allocation.
 *
 * @param[in] op String modification operation
 * @param[in] mm Memory manager
 * @param[in] copy_on_cow Use copy-on-write semantics?
 * @param[in] flags string-op flags
 * @param[in] data_in Input data
 * @param[out] data_out Output data
 * @param[out] dlen_out Length of @a data_out
 * @param[out] result Flags detailing the result (@c IB_STRFLAG_xx)
 *
 * @result Status code
 */
static ib_status_t zero_len_ex(ib_strop_t op,
                               ib_mm_t mm,
                               bool copy_on_cow,
                               ib_flags_t flags,
                               uint8_t *data_in,
                               uint8_t **data_out,
                               size_t *dlen_out,
                               ib_flags_t *result)
{
    if ( copy_on_cow && (op == IB_STROP_COW) ) {
        op = IB_STROP_COPY;
    }

    switch (op) {
    case IB_STROP_INPLACE:
    case IB_STROP_COW:
        *data_out = data_in;
        flags |= IB_STRFLAG_ALIAS;
        break;

    case IB_STROP_COPY:
        *data_out = ib_mm_alloc(mm, 0);
        if (*data_out == NULL) {
            return IB_EALLOC;
        }
        flags |= IB_STRFLAG_NEWBUF;
        break;

    default:
        return IB_EINVAL;
    }
    *dlen_out = 0;
    *result = flags;
    return IB_OK;
}

/**
 * Return a zero-length string that may be an alias into the original
 * or a new allocation.
 *
 * @param[in] op String modification operation
 * @param[in] mm Memory manager
 * @param[in] copy_on_cow Always treat COW as copy
 * @param[in] flags String-op flags
 * @param[in] str_in Input data
 * @param[in] offset Offset into @a str_in
 * @param[out] str_out Output data
 * @param[out] result Flags detailing the result (@c IB_STRFLAG_xx)
 *
 * @result Status code
 */
static ib_status_t zero_len(ib_strop_t op,
                            ib_mm_t mm,
                            bool copy_on_cow,
                            ib_flags_t flags,
                            char *str_in,
                            size_t offset,
                            char **str_out,
                            ib_flags_t *result)
{
    if ( copy_on_cow && (op == IB_STROP_COW) ) {
        op = IB_STROP_COPY;
    }

    switch (op) {
    case IB_STROP_INPLACE:
    case IB_STROP_COW:
        *str_out = str_in+offset;
        flags |= IB_STRFLAG_ALIAS;
        break;

    case IB_STROP_COPY:
        *str_out = ib_mm_strdup(mm, "");
        if (*str_out == NULL) {
            return IB_EALLOC;
        }
        flags |= IB_STRFLAG_NEWBUF;
        break;

    default:
        return IB_EINVAL;
    }
    *result = flags;
    return IB_OK;
}

/**
 * Left trim the input string
 *
 * @param[in] op String modification operation
 * @param[in] mm Memory manager
 * @param[in] str_in Input data
 * @param[in] offset Offset into @a str_in
 * @param[out] str_out Output data
 * @param[out] result Flags detailing the result (@c IB_STRFLAG_xx)
 *
 * @result Status code
 */
static ib_status_t trim_left(ib_strop_t op,
                             ib_mm_t mm,
                             char *str_in,
                             size_t offset,
                             char **str_out,
                             ib_flags_t *result)
{
    ib_flags_t flags;

    /* Set the base result flags */
    if (offset == 0) {
        flags = IB_STRFLAG_NONE;
    }
    else {
        flags = IB_STRFLAG_MODIFIED;
    }

    /* Handle normal case */
    switch (op) {
    case IB_STROP_INPLACE:
    case IB_STROP_COW:
        *str_out = str_in + offset;
        flags |= IB_STRFLAG_ALIAS;
        break;

    case IB_STROP_COPY:
        *str_out = ib_mm_strdup(mm, str_in + offset);
        if (*str_out == NULL) {
            return IB_EALLOC;
        }
        flags |= IB_STRFLAG_NEWBUF;
        break;

    default:
        return IB_EINVAL;
    }

    /* Done */
    *result = flags;
    return IB_OK;
}

/**
 * Search start from the right for the first non-whitespace.
 *
 * @param[in] str String to search
 * @param[in] len Length of string
 *
 * @returns Offset of first non-whitespace, -1 if none found
 */
static size_t find_nonws_right(const uint8_t *str,
                               size_t len)
{
    assert (str != NULL);
    const uint8_t *cur;

    /* Special case: length of zero */
    if (len == 0) {
        return 0;
    }

    /* Loop through all of the input until we find the first non-space */
    for (cur = str + len - 1;  cur >= str;  --cur) {
        if (isspace(*cur) == 0) {
            return cur - str;
        }
    }

    /* No non-whitespace found */
    return ALL_WHITESPACE;
}

/**
 * Return a zero-length string that may be an alias into the original
 * or a new allocation.
 *
 * @param[in] op String modification operation
 * @param[in] mm Memory manager
 * @param[in] flags Incoming flags
 * @param[in] data_in Input data
 * @param[in] dlen_in Length of @a data_in
 * @param[in] offset Offset of last non-whitespace in @a data_in
 * @param[out] data_out Output data
 * @param[out] dlen_out Length of @a data_out to use
 * @param[out] result Flags detailing the result (@c IB_STRFLAG_xx)
 *
 * @result Status code
 */
static ib_status_t trim_right_ex(ib_strop_t op,
                                 ib_mm_t mm,
                                 ib_flags_t flags,
                                 uint8_t *data_in,
                                 size_t dlen_in,
                                 size_t offset,
                                 uint8_t **data_out,
                                 size_t *dlen_out,
                                 ib_flags_t *result)
{
    /* Set the base result flags */
    *dlen_out = offset + 1;
    if (dlen_in != offset +1) {
        flags |= IB_STRFLAG_MODIFIED;
    }

    /* Handle normal case */
    switch (op) {
    case IB_STROP_INPLACE:
    case IB_STROP_COW:
        *data_out = data_in;
        flags |= IB_STRFLAG_ALIAS;
        break;

    case IB_STROP_COPY:
        *data_out = ib_mm_alloc(mm, *dlen_out);
        if (*data_out == NULL) {
            return IB_EALLOC;
        }
        memcpy(data_out, data_in, *dlen_out);
        flags |= IB_STRFLAG_NEWBUF;
        break;

    default:
        return IB_EINVAL;
    }

    /* Done */
    *result = flags;
    return IB_OK;
}

/**
 * Trim whitespace off the right (end) of a string
 *
 * @param[in] op String modification operation
 * @param[in] mm Memory manager
 * @param[in] flags Incoming flags
 * @param[in] str_in Input string
 * @param[in] len Length of @a str_in to use
 * @param[in] offset Offset into @a str_in
 * @param[out] str_out Output data
 * @param[out] result Flags detailing the result (@c IB_STRFLAG_xx)
 *
 * @result Status code
 */
static ib_status_t trim_right(ib_strop_t op,
                              ib_mm_t mm,
                              ib_flags_t flags,
                              char *str_in,
                              size_t len,
                              size_t offset,
                              char **str_out,
                              ib_flags_t *result)
{
    char *out = NULL;

    /* Set the base result flags */
    if (len != offset + 1) {
        flags = IB_STRFLAG_MODIFIED;
    }
    else if (op == IB_STROP_COW) {
        op = IB_STROP_INPLACE;
    }

    /* */
    switch (op) {
    case IB_STROP_INPLACE:
        out = str_in;
        flags |= IB_STRFLAG_ALIAS;
        break;

    case IB_STROP_COW:
    case IB_STROP_COPY:
        out = ib_mm_alloc(mm, len + 1);
        if (out == NULL) {
            return IB_EALLOC;
        }
        memcpy(out, str_in, len);
        flags |= IB_STRFLAG_NEWBUF;
        break;

    default:
        return IB_EINVAL;
    }

    *(out + offset + 1) = '\0';
    *str_out = out;
    *result = flags;
    return IB_OK;
}

/* Simple ASCII trimLeft function (see string.h). */
ib_status_t ib_strtrim_left_ex(ib_strop_t op,
                               ib_mm_t mm,
                               uint8_t *data_in,
                               size_t dlen_in,
                               uint8_t **data_out,
                               size_t *dlen_out,
                               ib_flags_t *result)
{
    size_t offset;
    ib_flags_t flags;

    assert(data_in != NULL);
    assert(data_out != NULL);
    assert(dlen_out != NULL);
    assert(result != NULL);

    if (dlen_in == 0) {
        ib_status_t rc =
            zero_len_ex(op, mm,
                        false, IB_STRFLAG_NONE,
                        data_in, data_out, dlen_out, result);
        return rc;
    }

    /* Find the first non-space */
    offset = find_nonws_left(data_in, dlen_in);

    /* Handle all whitespace separately */
    if (offset == ALL_WHITESPACE) {
        ib_status_t rc =
            zero_len_ex(op, mm,
                        false, IB_STRFLAG_MODIFIED,
                        data_in, data_out, dlen_out, result);
        return rc;
    }

    /* Set the base result flags */
    if (offset == 0) {
        flags = IB_STRFLAG_NONE;
        *dlen_out = dlen_in;
    }
    else {
        flags = IB_STRFLAG_MODIFIED;
        *dlen_out = dlen_in - offset;
    }

    /* Handle normal case */
    switch (op) {
    case IB_STROP_INPLACE:
    case IB_STROP_COW:
        *data_out = data_in + offset;
        flags |= IB_STRFLAG_ALIAS;
        break;

    case IB_STROP_COPY:
        *data_out = ib_mm_alloc(mm, *dlen_out);
        if (*data_out == NULL) {
            return IB_EALLOC;
        }
        memcpy(data_out, data_in + offset, *dlen_out);
        flags |= IB_STRFLAG_NEWBUF;
        break;

    default:
        return IB_EINVAL;
    }

    /* Done */
    *result = flags;
    return IB_OK;
}

/* Simple ASCII trimLeft function (see string.h). */
ib_status_t ib_strtrim_left(ib_strop_t op,
                            ib_mm_t mm,
                            char *str_in,
                            char **str_out,
                            ib_flags_t *result)
{
    size_t len;
    size_t offset;
    ib_status_t rc;

    assert(str_in != NULL);
    assert(str_out != NULL);
    assert(result != NULL);

    len = strlen(str_in);
    if (len == 0) {
        rc = zero_len(op, mm,
                      false, IB_STRFLAG_NONE,
                      str_in, len,
                      str_out, result);
        return rc;
    }

    /* Find the first non-space */
    offset = find_nonws_left((uint8_t *)str_in, len);

    /* Handle no match separately */
    if (offset == ALL_WHITESPACE) {
        rc = zero_len(op, mm,
                      false, IB_STRFLAG_MODIFIED,
                      str_in, len,
                      str_out, result);
        return rc;
    }

    /* Perform the actual trim */
    rc = trim_left(op, mm, str_in, offset, str_out, result);
    return rc;
}

/* Simple ASCII trimRight function (see string.h). */
ib_status_t ib_strtrim_right_ex(ib_strop_t op,
                                ib_mm_t mm,
                                uint8_t *data_in,
                                size_t dlen_in,
                                uint8_t **data_out,
                                size_t *dlen_out,
                                ib_flags_t *result)
{
    size_t offset;
    ib_status_t rc;

    assert(data_in != NULL);
    assert(data_out != NULL);
    assert(dlen_out != NULL);
    assert(result != NULL);

    if (dlen_in == 0) {
        rc = zero_len_ex(op, mm, false, IB_STRFLAG_NONE,
                         data_in, data_out, dlen_out, result);
        return rc;
    }

    /* Find the right-most non-space */
    offset = find_nonws_right(data_in, dlen_in);

    /* Handle all whitespace */
    if (offset == ALL_WHITESPACE) {
        rc = zero_len_ex(op, mm, false, IB_STRFLAG_MODIFIED,
                         data_in, data_out, dlen_out, result);
        return rc;
    }

    /* Handle the normal case */
    rc = trim_right_ex(op, mm,
                       IB_STRFLAG_NONE,
                       data_in, dlen_in, offset,
                       data_out, dlen_out, result);
    return rc;
}

/* Simple ASCII trimRight function (see string.h). */
ib_status_t ib_strtrim_right(ib_strop_t op,
                             ib_mm_t mm,
                             char *str_in,
                             char **str_out,
                             ib_flags_t *result)
{
    size_t offset;
    size_t len;
    ib_status_t rc;

    assert(str_in != NULL);
    assert(str_out != NULL);
    assert(result != NULL);

    /* Find the right-most non-space */
    len = strlen(str_in);
    if (len == 0) {
        rc = zero_len(op, mm,
                      false, IB_STRFLAG_NONE,
                      str_in, len,
                      str_out, result);
        return rc;
    }
    offset = find_nonws_right((uint8_t *)str_in, len);

    /* Handle all whitespace */
    if (offset == ALL_WHITESPACE) {
        rc = zero_len(op, mm,
                      false, IB_STRFLAG_MODIFIED,
                      str_in, len,
                      str_out, result);
        return rc;
    }

    /* Handle normal case */
    rc = trim_right(op, mm,
                    IB_STRFLAG_NONE,
                    str_in, len, offset,
                    str_out, result);
    return rc;
}

/* Simple ASCII trim function (see string.h). */
ib_status_t ib_strtrim_lr_ex(ib_strop_t op,
                             ib_mm_t mm,
                             uint8_t *data_in,
                             size_t dlen_in,
                             uint8_t **data_out,
                             size_t *dlen_out,
                             ib_flags_t *result)
{
    size_t loffset;
    size_t roffset;
    ib_flags_t flags = IB_STRFLAG_NONE;
    ib_status_t rc;

    assert(data_in != NULL);
    assert(data_out != NULL);
    assert(dlen_out != NULL);
    assert(result != NULL);

    if (dlen_in == 0) {
        rc = zero_len_ex(op, mm,
                         false, IB_STRFLAG_NONE,
                         data_in, data_out, dlen_out, result);
        return rc;
    }

    /* Find the first non-space */
    loffset = find_nonws_left(data_in, dlen_in);

    /* Handle all whitespace separately */
    if (loffset == ALL_WHITESPACE) {
        rc = zero_len_ex(op, mm,
                         false, IB_STRFLAG_MODIFIED,
                         data_in, data_out, dlen_out, result);
        return rc;
    }

    /* Adjust inputs to account for the skipped whitespace */
    data_in += loffset;
    dlen_in -= loffset;
    if (loffset != 0) {
        flags |= IB_STRFLAG_MODIFIED;
    }

    /* Find the right-most non-space */
    roffset = find_nonws_right(data_in, dlen_in);

    /* Handle the normal case */
    rc = trim_right_ex(op, mm,
                       flags,
                       data_in, dlen_in, roffset,
                       data_out, dlen_out, result);
    return rc;
}

/* Simple ASCII trim function (see string.h) */
ib_status_t ib_strtrim_lr(ib_strop_t op,
                          ib_mm_t mm,
                          char *str_in,
                          char **str_out,
                          ib_flags_t *result)
{
    size_t len;
    size_t loffset;
    size_t roffset;
    ib_flags_t flags;
    ib_status_t rc;

    assert(str_in != NULL);
    assert(str_out != NULL);
    assert(result != NULL);

    len = strlen(str_in);
    if (len == 0) {
        rc = zero_len(op, mm,
                      false, IB_STRFLAG_NONE,
                      str_in, len,
                      str_out, result);
        return rc;
    }

    /* Find the first non-space */
    loffset = find_nonws_left((uint8_t *)str_in, len);

    /* Handle no match separately */
    if (loffset == ALL_WHITESPACE) {
        rc = zero_len(op, mm,
                      false, IB_STRFLAG_MODIFIED,
                      str_in, len,
                      str_out, result);
        return rc;
    }
    else if (loffset == 0) {
        flags = IB_STRFLAG_NONE;
    }
    else {
        flags = IB_STRFLAG_MODIFIED;
        str_in += loffset;
        len -= loffset;
    }

    /* Handle normal case */
    roffset = find_nonws_right((uint8_t *)str_in, len);
    rc = trim_right(op, mm,
                    flags,
                    str_in, len, roffset,
                    str_out, result);
    return rc;
}
