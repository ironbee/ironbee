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
 * @brief IronBee - Variable expansion related functions
 * @author Nick LeRoy <nleroy@qualys.com>
 */

#include "ironbee_config_auto.h"

#include <sys/types.h>
#include <limits.h>
#include <stdio.h>
#include <assert.h>

#include <ironbee/expand.h>
#include <ironbee/types.h>
#include <ironbee/hash.h>
#include <ironbee/field.h>
#include <ironbee/bytestr.h>
#include <ironbee/mpool.h>
#include <ironbee/debug.h>

#define NUM_BUF_LEN 64

/**
 * Join two memory blocks into a single buffer
 * @internal
 *
 * @param[in] mp Memory pool
 * @param[in] p1 Pointer to block 1
 * @param[in] l1 Length of block 1
 * @param[in] p2 Pointer to block 2
 * @param[in] l2 Length of block 2
 * @param[in] nul 1 if NUL byte should be tacked on, 0 if not
 * @param[out] out Pointer to output block
 * @param[out] olen Length of the output block
 *
 * @returns status code
 */
static ib_status_t join2(ib_mpool_t *mp,
                         const char *p1,
                         size_t l1,
                         const char *p2,
                         size_t l2,
                         ib_bool_t nul,
                         char **out,
                         size_t *olen)
{
    IB_FTRACE_INIT();
    size_t buflen = l1 + l2 + (nul == IB_TRUE ? 1 : 0);
    char *buf;
    char *p;

    /* Allocate the buffer */
    buf = (char *)ib_mpool_alloc(mp, buflen);
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
 * Join three memory blocks into a single buffer
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
 * @param[out] olen Length of the output block
 *
 * @returns status code
 */
static ib_status_t join3(ib_mpool_t *mp,
                         const char *p1,
                         size_t l1,
                         const char *p2,
                         size_t l2,
                         const char *p3,
                         size_t l3,
                         ib_bool_t nul,
                         char **out,
                         size_t *olen)
{
    IB_FTRACE_INIT();
    size_t buflen = l1 + l2 + l3 + (nul == IB_TRUE ? 1 : 0);
    char *buf;
    char *p;

    /* Allocate the buffer */
    buf = (char *)ib_mpool_alloc(mp, buflen);
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
 * Expand a string from the given hash.  See expand.h.
 */
ib_status_t expand_str(ib_mpool_t *mp,
                       const char *str,
                       const char *prefix,
                       const char *suffix,
                       ib_hash_t *hash,
                       char **result)
{
    IB_FTRACE_INIT();
    ib_status_t rc;
    char numbuf[NUM_BUF_LEN+1]; /* Buffer used to convert number to str */
    size_t prelen = SIZE_MAX;   /* Prefix string length */
    size_t postlen = SIZE_MAX;  /* Suffix string length */
    const char *buf = str;      /* Current buffer */

    /* Sanity checks */
    assert(mp != NULL);
    assert(str != NULL);
    assert(prefix != NULL);
    assert(suffix != NULL);
    assert(hash != NULL);
    assert(result != NULL);
    assert(NUM_BUF_LEN <= 256);

    /* Initialize the result to NULL */
    *result = NULL;

    /* Validate prefix and suffix */
    if ( (*prefix == '\0') || (*suffix == '\0') ) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    /* Loop til the cows come home */
    while (1) {
        const char *pre;        /* Pointer to found prefix string */
        const char *post;       /* Pointer to found suffix string */
        const char *name;       /* Pointer to the name between pre and post */
        size_t namelen;         /* Length of the name */
        char *new;              /* New buffer */
        size_t newlen;          /* Length of new buffer */
        const char *iptr;       /* Initial block (up to the prefix) */
        size_t ilen;            /* Length of the initial block */
        const char *fptr;       /* Final block (after the suffix) */
        size_t flen;            /* Length of the final block */
        ib_field_t *f;

        /* Look for the prefix in the string */
        pre = strstr(buf, prefix);
        if (pre == NULL) {
            break;
        }


        /* Lazy compute prefix length */
        if (prelen == SIZE_MAX) {
            prelen = strlen(prefix);
            assert (prelen != 0);
        }

        /* And the next matching suffix */
        post = strstr(pre+prelen, suffix);
        if (post == NULL) {
            break;
        }

        /* Lazy compute suffix length */
        if (postlen == SIZE_MAX) {
            postlen = strlen(suffix);
            assert (postlen != 0);
        }

        /* The name is the block between the two */
        name = (pre + prelen);
        namelen = (post - pre) - prelen;

        /* Length of the initial block */
        iptr = buf;
        ilen = (pre - buf);

        /* The final block */
        fptr = (post + postlen);
        flen = strlen(fptr);

        /* Zero length name? Expand it to "" */
        if (namelen == 0) {
            rc = join2(mp,
                       iptr, ilen,
                       fptr, flen,
                       IB_TRUE,
                       &new, &newlen);
            if (rc != IB_OK) {
                IB_FTRACE_RET_STATUS(rc);
            }
            buf = new;
            continue;
        }

        /* Search the hash */
        rc = ib_hash_get_ex(hash, &f, (void *)name, namelen);
        if (rc == IB_ENOENT) {
            /* Not in the hash; replace with "" */
            rc = join2(mp,
                       iptr, ilen,
                       fptr, flen,
                       IB_TRUE,
                       &new, &newlen);
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
                       iptr, ilen,
                       s, slen,
                       fptr, flen,
                       IB_TRUE,
                       &new, &newlen);
            if (rc != IB_OK) {
                IB_FTRACE_RET_STATUS(rc);
            }
        }
        else if (f->type == IB_FTYPE_BYTESTR) {
            /* Field is a byte string */
            ib_bytestr_t *bs = ib_field_value_bytestr(f);
            rc = join3(mp,
                       iptr, ilen,
                       (char *)ib_bytestr_ptr(bs), ib_bytestr_length(bs),
                       fptr, flen,
                       IB_TRUE,
                       &new, &newlen);
            if (rc != IB_OK) {
                IB_FTRACE_RET_STATUS(rc);
            }
        }
        else if (f->type == IB_FTYPE_NUM) {
            /* Field is a number; convert it to a string */
            ib_num_t *n = ib_field_value_num(f);
            snprintf(numbuf, NUM_BUF_LEN, "%ld", (long int)(*n) );
            rc = join3(mp,
                       iptr, ilen,
                       numbuf, strlen(numbuf),
                       fptr, flen,
                       IB_TRUE,
                       &new, &newlen);
            if (rc != IB_OK) {
                IB_FTRACE_RET_STATUS(rc);
            }
        }
        else if (f->type == IB_FTYPE_UNUM) {
            /* Field is an unsigned number; convert it to a string */
            ib_unum_t *n = ib_field_value_unum(f);
            snprintf(numbuf, NUM_BUF_LEN, "%lu", (unsigned long)(*n) );
            rc = join3(mp,
                       iptr, ilen,
                       numbuf, strlen(numbuf),
                       fptr, flen,
                       IB_TRUE,
                       &new, &newlen);
            if (rc != IB_OK) {
                IB_FTRACE_RET_STATUS(rc);
            }
        }
        else {
            /* Something else: replace with "" */
            rc = join2(mp,
                       iptr, ilen,
                       fptr, flen,
                       IB_TRUE,
                       &new, &newlen);
            if (rc != IB_OK) {
                IB_FTRACE_RET_STATUS(rc);
            }
        }
        buf = new;
    }

    *result = (char *)buf;
    IB_FTRACE_RET_STATUS(IB_OK);
}

/*
 * Test whether a given string would be expanded.  See expand.h.
 */
ib_status_t expand_test_str(const char *str,
                            const char *prefix,
                            const char *suffix,
                            ib_bool_t *result)
{
    IB_FTRACE_INIT();
    const char *pre;      /* Pointer to found prefix pattern */
    const char *post;     /* Pointer to found suffix pattern */

    /* Sanity checks */
    assert(str != NULL);
    assert(prefix != NULL);
    assert(suffix != NULL);
    assert(result != NULL);

    /* Initialize the result to no */
    *result = IB_FALSE;

    /* Validate prefix and suffix */
    if ( (*prefix == '\0') || (*suffix == '\0') ) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    /* Look for the prefix pattern */
    pre = strstr(str, prefix);
    if (pre == NULL) {
        IB_FTRACE_RET_STATUS(IB_OK);
    }

    /* And the next matching suffix pattern. */
    post = strstr(pre+strlen(prefix), suffix);
    if (post == NULL) {
        IB_FTRACE_RET_STATUS(IB_OK);
    }

    /* Yes, it looks expandable.  Done */
    *result = IB_TRUE;
    IB_FTRACE_RET_STATUS(IB_OK);
}
