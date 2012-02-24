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
 * @brief IronBee - String related functions
 * @author Nick LeRoy <nleroy@qualys.com>
 */

#include "ironbee_config_auto.h"

#include <sys/types.h>
#include <limits.h>
#include <stdio.h>

#include <ironbee/expand.h>
#include <ironbee/types.h>
#include <ironbee/hash.h>
#include <ironbee/field.h>
#include <ironbee/bytestr.h>
#include <ironbee/mpool.h>
#include <ironbee/debug.h>

#define NUM_BUF_LEN 128

/**
 * Join 2 memory blocks into a single buffer
 * @internal
 *
 * @param[in] mp Memory pool
 * @param[in] p1 Pointer to block 1
 * @param[in] l1 Length of block 1
 * @param[in] p2 Pointer to block 2
 * @param[in] l2 Length of block 2
 * @param[in] nul 1 if NUL byte should be tacked on, 0 if not
 * @param[out] out Pointer to output block
 * @param[out] Length of the output block
 *
 * @returns status code
 */
static ib_status_t join2(ib_mpool_t *mp,
                         const uint8_t *p1,
                         size_t l1,
                         const uint8_t *p2,
                         size_t l2,
                         int nul,
                         uint8_t **out,
                         size_t *olen)
{
    IB_FTRACE_INIT();
    size_t buflen = l1 + l2 + nul;
    uint8_t *buf;
    uint8_t *p;

    /* Allocate the buffer */
    buf = (uint8_t *)ib_mpool_alloc(mp, buflen);
    if (buf == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    /* Copy the blocks in */
    p = buf;
    memcpy(p, p1, l1);
    p += l1;
    memcpy(p, p2, l2);
    p += l2;
    if (nul) {
        *p = '\0';
    }

    /* Done */
    *out = buf;
    *olen = buflen;
    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * Join 3 memory blocks into a single buffer
 * @internal
 *
 * @param[in] mp Memory pool
 * @param[in] p1 Pointer to block 1
 * @param[in] l1 Length of block 1
 * @param[in] p2 Pointer to block 2
 * @param[in] l2 Length of block 2
 * @param[in] p3 Pointer to block 3
 * @param[in] l3 Length of block 3
 * @param[in] nul 1 if NUL byte should be tacked on, 0 if not
 * @param[out] out Pointer to output block
 * @param[out] Length of the output block
 *
 * @returns status code
 */
static ib_status_t join3(ib_mpool_t *mp,
                         const uint8_t *p1,
                         size_t l1,
                         const uint8_t *p2,
                         size_t l2,
                         const uint8_t *p3,
                         size_t l3,
                         int nul,
                         uint8_t **out,
                         size_t *olen)
{
    IB_FTRACE_INIT();
    size_t buflen = l1 + l2 + l3 + nul;
    uint8_t *buf;
    uint8_t *p;

    /* Allocate the buffer */
    buf = ib_mpool_alloc(mp, buflen);
    if (buf == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    /* Copy the blocks in */
    p = buf;
    memcpy(p, p1, l1);
    p += l1;
    memcpy(p, p2, l2);
    p += l2;
    memcpy(p, p3, l3);
    p += l3;
    if (nul) {
        *p = '\0';
    }

    /* Done */
    *out = buf;
    *olen = buflen;
    IB_FTRACE_RET_STATUS(IB_OK);
}

/*
 * Expand a string from the given hash
 */
ib_status_t expand_str(ib_mpool_t *mp,
                       const char *str,
                       const char *startpat,
                       const char *endpat,
                       ib_hash_t *hash,
                       char **result)
{
    IB_FTRACE_INIT();
    ib_status_t rc;
    size_t pslen = SIZE_MAX;           /**< Start pattern length */
    size_t pelen = SIZE_MAX;           /**< End pattern length */
    const char *buf = str;             /**< Current buffer */

    /* Loop til the cows come home */
    while (1) {
        static char numbuf[NUM_BUF_LEN+1];
        const char *start;      /* Pointer to found start pattern */
        const char *end;        /* Pointer to found end pattern */
        const char *name;       /* Pointer to the name between start and end */
        size_t namelen;         /* Length of the name */
        char *new;              /* New buffer */
        size_t newlen;          /* Length of new buffer */
        const char *iptr;       /* Initial block (up to the start pattern) */
        size_t ilen;            /* Length of the initial block */
        const char *fptr;       /* Final block (after the end pattern) */
        size_t flen;            /* Length of the final block */
        ib_field_t *f;

        /* Look for the start pattern */
        start = strstr(buf, startpat);
        if (start == NULL) {
            break;
        }

        /* Lazy compute pslen */
        if (pslen == SIZE_MAX) {
            pslen = strlen(startpat);
        }

        /* And the next matching end pattern. */
        end = strstr(start+pslen, endpat);
        if (end == NULL) {
            break;
        }

        /* Lazy compute pelen */
        if (pelen == SIZE_MAX) {
            pelen = strlen(endpat);
        }

        /* The name is the block between the two */
        name = (start + pslen);
        namelen = (end - start) - pslen;

        /* Length of the initial block */
        iptr = buf;
        ilen = (start - buf);

        /* The final block */
        fptr = (end + pelen);
        flen = strlen(fptr);

        /* Search the hash */
        rc = ib_hash_get_ex(hash, &f, (void *)name, namelen);
        if (rc == IB_ENOENT) {
            /* Not in the hash; replace with "" */
            rc = join2(mp,
                       (const uint8_t *)iptr, ilen,
                       (const uint8_t *)fptr, flen,
                       1,
                       (uint8_t **)&new, &newlen);
            if (rc != IB_OK) {
                IB_FTRACE_RET_STATUS(rc);
            }
            buf = new;
            continue;
        }
        else if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }

        /*
         *  Found the field in the hash.  What we do depends on it's type.
         */
        if (f->type == IB_FTYPE_NULSTR) {
            /* Field is a NUL-terminated string */
            const char *s = ib_field_value_nulstr(f);
            size_t slen = strlen(s);
            rc = join3(mp,
                       (const uint8_t *)iptr, ilen,
                       (const uint8_t *)s, slen,
                       (const uint8_t *)fptr, flen,
                       1,
                       (uint8_t **)&new, &newlen);
            if (rc != IB_OK) {
                IB_FTRACE_RET_STATUS(rc);
            }
        }
        else if (f->type == IB_FTYPE_BYTESTR) {
            /* Field is a byte string */
            ib_bytestr_t *bs = ib_field_value_bytestr(f);
            rc = join3(mp,
                       (const uint8_t *)iptr, ilen,
                       ib_bytestr_ptr(bs), ib_bytestr_length(bs),
                       (const uint8_t *)fptr, flen,
                       1,
                       (uint8_t **)&new, &newlen);
            if (rc != IB_OK) {
                IB_FTRACE_RET_STATUS(rc);
            }
        }
        else if (f->type == IB_FTYPE_NUM) {
            /* Field is a number; convert it to a string */
            ib_num_t *n = ib_field_value_num(f);
            snprintf(numbuf, NUM_BUF_LEN, "%ld", (long int)(*n) );
            rc = join3(mp,
                       (const uint8_t *)iptr, ilen,
                       (const uint8_t *)numbuf, strlen(numbuf),
                       (const uint8_t *)fptr, flen,
                       1,
                       (uint8_t **)&new, &newlen);
            if (rc != IB_OK) {
                IB_FTRACE_RET_STATUS(rc);
            }
        }
        else if (f->type == IB_FTYPE_UNUM) {
            /* Field is an unsigned number; convert it to a string */
            ib_unum_t *n = ib_field_value_unum(f);
            snprintf(numbuf, NUM_BUF_LEN, "%lu", (unsigned long)(*n) );
            rc = join3(mp,
                       (const uint8_t *)iptr, ilen,
                       (const uint8_t *)numbuf, strlen(numbuf),
                       (const uint8_t *)fptr, flen,
                       1,
                       (uint8_t **)&new, &newlen);
            if (rc != IB_OK) {
                IB_FTRACE_RET_STATUS(rc);
            }
        }
        else {
            /* Something else: replace with "" */
            rc = join2(mp,
                       (const uint8_t *)iptr, ilen,
                       (const uint8_t *)fptr, flen,
                       1,
                       (uint8_t **)&new, &newlen);
            if (rc != IB_OK) {
                IB_FTRACE_RET_STATUS(rc);
            }
        }
        buf = new;
    }

    *result = (char *)buf;
    IB_FTRACE_RET_STATUS(IB_OK);
}
