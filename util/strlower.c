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

#include <ironbee/flags.h>
#include <ironbee/string.h>
#include <ironbee/types.h>
#include <ironbee/util.h>

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

/**
 * Simple in-place ASCII lowercase function.
 *
 * @param[in] inflags Incoming flags
 * @param[in,out] data Data to translate to lowercase
 * @param[in] dlen Length of @a data
 * @param[out] result Result flags (@c IB_STRFLAG_xxx)
 *
 * @returns Status code.
 */
static ib_status_t inplace(ib_flags_t inflags,
                           uint8_t *data,
                           size_t dlen,
                           ib_flags_t *result)
{
    size_t i = 0;
    int modcount = 0;

    assert(data != NULL);
    assert(result != NULL);

    while(i < dlen) {
        int c = *(data+i);
        *(data+i) = tolower(c);
        if (c != *(data+i)) {
            ++modcount;
        }
        ++i;
    }

    /* Note if any modifications were made. */
    if (modcount != 0) {
        *result = (inflags | IB_STRFLAG_MODIFIED);
    }
    else {
        *result = inflags;
    }

    return IB_OK;
}

/**
 * ASCII lowercase function with copy-on-write semantics.
 *
 * @param[in] mm Memory manager for allocations
 * @param[in] data_in Data to convert to lower case
 * @param[in] dlen_in Length of @a data_in
 * @param[out] data_out Output data
 * @param[out] dlen_out Length of @a data_out (or NULL)
 * @param[out] result Output flags (@c IB_STRFLAG_xxx)
 *
 * @returns Status code.
 */
static ib_status_t copy_on_write(ib_mm_t mm,
                                 const uint8_t *data_in,
                                 size_t dlen_in,
                                 uint8_t **data_out,
                                 size_t *dlen_out,
                                 ib_flags_t *result)
{
    const uint8_t *iptr;
    const uint8_t *iend;
    uint8_t *optr;
    uint8_t *obuf;

    assert(data_in != NULL);
    assert(data_out != NULL);
    assert(result != NULL);

    /* Initializations */
    iend = data_in + dlen_in;
    *result = IB_STRFLAG_ALIAS;
    *data_out = (uint8_t *)data_in;
    if (dlen_out != NULL) {
        *dlen_out = dlen_in;
    }
    obuf = NULL;    /* Output buffer; NULL until output buffer allocated */
    optr = NULL;    /* Output pointer; NULL until obuf is allocated */

    /*
     * Loop through all of the input, using the input pointer as loop
     * variable.
     */
    for (iptr = data_in;  iptr < iend;  ++iptr) {
        int c = *iptr;
        if (isupper(c)) {
            if (obuf == NULL) {
                /* Output buffer not previously allocated;
                 * allocate it now, and copy into it */
                size_t off;
                obuf = ib_mm_alloc(mm, dlen_in);
                if (obuf == NULL) {
                    return IB_EALLOC;
                }
                *data_out = obuf;
                *result = (IB_STRFLAG_NEWBUF|IB_STRFLAG_MODIFIED);
                off = (iptr - data_in);
                if (off == 1) {
                    *obuf = *data_in;
                }
                else if (off != 0) {
                    memcpy(obuf, data_in, off);
                }
                optr = obuf+off;
            }
            *optr = tolower(c);
        }
        else if (optr != NULL) {
            *optr = *iptr;
        }
        if (optr != NULL) {
            ++optr;
        }
    }

    return IB_OK;
}

/* Simple ASCII lowercase function (ex version); see string.h */
ib_status_t ib_strlower_ex(ib_strop_t op,
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

    switch(op) {
    case IB_STROP_INPLACE:
        rc = inplace(IB_STRFLAG_ALIAS, data_in, dlen_in, result);
        *data_out = data_in;
        *dlen_out = dlen_in;
        break;

    case IB_STROP_COPY:
        *data_out = ib_mm_memdup(mm, data_in, dlen_in);
        if (*data_out == NULL) {
            return IB_EALLOC;
        }
        *dlen_out = dlen_in;
        rc = inplace(IB_STRFLAG_NEWBUF, *data_out, dlen_in, result);
        break;

    case IB_STROP_COW:
        rc = copy_on_write(mm, data_in, dlen_in, data_out, dlen_out, result);
        break;

    default:
        return IB_EINVAL;
    }

    return rc;
}

/* ASCII lowercase function (string version); See string.h */
ib_status_t ib_strlower(ib_strop_t op,
                        ib_mm_t     mm,
                        char *str_in,
                        char **str_out,
                        ib_flags_t *result)
{
    size_t len;
    ib_status_t rc = IB_OK;
    char *out = NULL;

    assert(str_in != NULL);
    assert(str_out != NULL);
    assert(result != NULL);

    len = strlen(str_in);
    switch(op) {
    case IB_STROP_INPLACE:
        out = str_in;
        rc = inplace(IB_STRFLAG_ALIAS, (uint8_t*)str_in, len, result);
        break;

    case IB_STROP_COPY:
        out = ib_mm_strdup(mm, str_in);
        if (out == NULL) {
            return IB_EALLOC;
        }
        rc = inplace(IB_STRFLAG_NEWBUF, (uint8_t*)out, len, result);
        break;

    case IB_STROP_COW:
    {
#if ((__GNUC__==4) && (__GNUC_MINOR__<3))
        uint8_t *uint8ptr;
        rc = copy_on_write(mm,
                           (uint8_t *)str_in, len+1,
                           &uint8ptr, NULL, result);
        out = (char *)uint8ptr;
#else
        rc = copy_on_write(mm,
                           (uint8_t *)str_in, len+1,
                           (uint8_t **)&out, NULL, result);
#endif
        break;
    }

    default:
        return IB_EINVAL;
    }

    if (rc == IB_OK) {
        if (ib_flags_all(*result, IB_STRFLAG_MODIFIED)) {
            *(out+len) = '\0';
        }
        *str_out = out;
    }
    return rc;
}
