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
 * @brief IronBee &mdash; Variable expansion related functions
 * @author Nick LeRoy <nleroy@qualys.com>
 */

#include "ironbee_config_auto.h"

#include <sys/types.h>
#include <limits.h>
#include <stdio.h>
#include <inttypes.h>
#include <assert.h>

#include <ironbee/expand.h>
#include <ironbee/types.h>
#include <ironbee/string.h>
#include <ironbee/hash.h>
#include <ironbee/field.h>
#include <ironbee/bytestr.h>
#include <ironbee/mpool.h>
#include <ironbee/debug.h>

#define NUM_BUF_LEN 64

/**
 * Join two memory blocks into a single buffer
 *
 * @param[in] mp Memory pool
 * @param[in] p1 Pointer to block 1
 * @param[in] l1 Length of block 1
 * @param[in] p2 Pointer to block 2
 * @param[in] l2 Length of block 2
 * @param[in] nul true if NUL byte should be tacked on, false if not
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
                         bool nul,
                         char **out,
                         size_t *olen)
{
    IB_FTRACE_INIT();
    size_t buflen = l1 + l2 + (nul == true ? 1 : 0);
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
    if (nul == true) {
        *p = '\0';
    }

    /* Done */
    *out = buf;
    *olen = buflen;
    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * Join three memory blocks into a single buffer
 *
 * @param[in] mp Memory pool
 * @param[in] p1 Pointer to block 1
 * @param[in] l1 Length of block 1
 * @param[in] p2 Pointer to block 2
 * @param[in] l2 Length of block 2
 * @param[in] p3 Pointer to block 3
 * @param[in] l3 Length of block 3
 * @param[in] nul true if NUL byte should be tacked on, false if not
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
                         bool nul,
                         char **out,
                         size_t *olen)
{
    IB_FTRACE_INIT();
    size_t buflen = l1 + l2 + l3 + (nul == true ? 1 : 0);
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
    if (nul == true) {
        *p = '\0';
    }

    /* Done */
    *out = buf;
    *olen = buflen;
    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * Join a field with strings before and after it
 *
 * @param[in] mp Memory pool
 * @param[in] f Field to join
 * @param[in] iptr Pointer to initial string
 * @param[in] ilen Length of @a iptr
 * @param[in] fptr Pointer to final string
 * @param[in] flen Length of @a fptr
 * @param[in] nul true if NUL byte should be tacked on, false if not
 * @param[out] out Pointer to output block
 * @param[out] olen Length of the output block
 *
 * @returns status code
 */
static ib_status_t join_parts(ib_mpool_t *mp,
                              const ib_field_t *f,
                              const char *iptr,
                              size_t ilen,
                              const char *fptr,
                              size_t flen,
                              bool nul,
                              char **out,
                              size_t *olen)
{
    IB_FTRACE_INIT();
    ib_status_t rc;
    char numbuf[NUM_BUF_LEN+1]; /* Buffer used to convert number to str */
    assert(NUM_BUF_LEN <= 256);

    switch(f->type) {
    case IB_FTYPE_NULSTR:
    {
        /* Field is a NUL-terminated string */
        const char *s;
        rc = ib_field_value(f, ib_ftype_nulstr_out(&s));
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }
        size_t slen = strlen(s);
        rc = join3(mp,
                   iptr, ilen,
                   s, slen,
                   fptr, flen,
                   nul,
                   out, olen);
        break;
    }

    case IB_FTYPE_BYTESTR:
    {
        /* Field is a byte string */
        const ib_bytestr_t *bs;
        rc = ib_field_value(f, ib_ftype_bytestr_out(&bs));
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }
        rc = join3(mp,
                   iptr, ilen,
                   (const char *)ib_bytestr_const_ptr(bs),
                   ib_bytestr_length(bs),
                   fptr, flen,
                   nul,
                   out, olen);
        break;
    }

    case IB_FTYPE_NUM:
    {
        /* Field is a number; convert it to a string */
        ib_num_t n;
        rc = ib_field_value(f, ib_ftype_num_out(&n));
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }
        snprintf(numbuf, NUM_BUF_LEN, "%"PRId64, n);
        rc = join3(mp,
                   iptr, ilen,
                   numbuf, strlen(numbuf),
                   fptr, flen,
                   nul,
                   out, olen);
        break;
    }

    case IB_FTYPE_UNUM:
    {
        /* Field is an unsigned number; convert it to a string */
        ib_unum_t n;
        rc = ib_field_value(f, ib_ftype_unum_out(&n));
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }
        snprintf(numbuf, NUM_BUF_LEN, "%"PRIu64, n);
        rc = join3(mp,
                   iptr, ilen,
                   numbuf, strlen(numbuf),
                   fptr, flen,
                   nul,
                   out, olen);
        break;
    }

    case IB_FTYPE_LIST:
    {
        /* Field is a list: use the first element in the list */
        const ib_list_t *list;
        const ib_list_node_t *node;
        const ib_field_t *element;

        rc = ib_field_value(f, ib_ftype_list_out(&list));
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }

        node = ib_list_first_const(list);
        if (node == NULL) {
            rc = join2(mp, iptr, ilen, fptr, flen, nul, out, olen);
            break;
        }

        element = (const ib_field_t *)ib_list_node_data_const(node);
        rc = join_parts(mp, element, iptr, ilen, fptr, flen, nul, out, olen);
        break;
    }

    default:
        /* Something else: replace with "" */
        rc = join2(mp, iptr, ilen, fptr, flen, nul, out, olen);
        break;
    }

    IB_FTRACE_RET_STATUS(rc);
}

/*
 * Expand a string from the given hash.  See expand.h.
 */
ib_status_t ib_expand_str(ib_mpool_t *mp,
                          const char *str,
                          const char *prefix,
                          const char *suffix,
                          bool recurse,
                          ib_hash_t *hash,
                          char **result)
{
    IB_FTRACE_INIT();
    ib_status_t rc;
    size_t len;

    assert(mp != NULL);
    assert(str != NULL);
    assert(prefix != NULL);
    assert(suffix != NULL);
    assert(hash != NULL);
    assert(result != NULL);

    /* Let ib_expand_str_ex() do the heavy lifting */
    rc = ib_expand_str_ex(mp, str, strlen(str),
                          prefix, suffix,
                          true, recurse,
                          hash,
                          result, &len);

    IB_FTRACE_RET_STATUS(rc);
}

/*
 * Expand a string from the given hash.  See expand.h.
 */
ib_status_t ib_expand_str_gen(ib_mpool_t *mp,
                              const char *str,
                              const char *prefix,
                              const char *suffix,
                              bool recurse,
                              ib_expand_lookup_fn_t lookup_fn,
                              const void *lookup_data,
                              char **result)
{
    IB_FTRACE_INIT();
    ib_status_t rc;
    size_t len;

    assert(mp != NULL);
    assert(str != NULL);
    assert(prefix != NULL);
    assert(suffix != NULL);
    assert(lookup_fn != NULL);
    assert(result != NULL);

    /* Let ib_expand_str_gen_ex() do the heavy lifting */
    rc = ib_expand_str_gen_ex(mp, str, strlen(str), prefix, suffix,
                              true, recurse,
                              lookup_fn, lookup_data, result, &len);

    IB_FTRACE_RET_STATUS(rc);
}

/**
 * Lookup a value in a hash.
 *
 * @param[in] data Hash to lookup in
 * @param[in] key Key to lookup
 * @param[in] keylen Length of @a key
 * @param[out] pf Pointer to output field.
 *
 * @returns Return values from ib_hash_get_ex()
 */
static ib_status_t hash_lookup(const void *data,
                               const char *key,
                               size_t keylen,
                               ib_field_t **pf)
{
    IB_FTRACE_INIT();
    assert(data != NULL);
    assert(key != NULL);
    assert(pf != NULL);

    ib_status_t rc;
    ib_hash_t *hash = (ib_hash_t *)data;

    rc = ib_hash_get_ex(hash, pf, key, keylen);
    IB_FTRACE_RET_STATUS(rc);
}

/*
 * Expand a string from the given a hash, ex version.  See expand.h.
 */
ib_status_t ib_expand_str_ex(ib_mpool_t *mp,
                             const char *str,
                             size_t str_len,
                             const char *prefix,
                             const char *suffix,
                             bool nul,
                             bool recurse,
                             const ib_hash_t *hash,
                             char **result,
                             size_t *result_len)
{
    IB_FTRACE_INIT();
    ib_status_t rc;

    rc = ib_expand_str_gen_ex(mp, str, str_len,
                              prefix, suffix,
                              nul, recurse,
                              hash_lookup, hash,
                              result, result_len);
    IB_FTRACE_RET_STATUS(rc);
};

/*
 * Expand a string from the given hash-like object, ex version.  See expand.h.
 */
ib_status_t ib_expand_str_gen_ex(ib_mpool_t *mp,
                                 const char *str,
                                 size_t str_len,
                                 const char *prefix,
                                 const char *suffix,
                                 bool nul,
                                 bool recurse,
                                 ib_expand_lookup_fn_t lookup_fn,
                                 const void *lookup_data,
                                 char **result,
                                 size_t *result_len)
{
    IB_FTRACE_INIT();
    ib_status_t rc;
    size_t pre_len;             /* Prefix string length */
    size_t suf_len = SIZE_MAX;  /* Suffix string length */
    const char *buf = str;      /* Current buffer */
    size_t buflen = str_len;    /* Length of the buffer */

    /* Sanity checks */
    assert(mp != NULL);
    assert(str != NULL);
    assert(prefix != NULL);
    assert(suffix != NULL);
    assert(lookup_fn != NULL);
    assert(result != NULL);
    assert(result_len != NULL);

    /* Initialize the result to NULL */
    *result = NULL;
    *result_len = 0;

    /* Validate prefix and suffix */
    if ( (*prefix == '\0') || (*suffix == '\0') ) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    /* Compute prefix length */
    pre_len = strlen(prefix);
    assert (pre_len != 0);

    /* Check for minimum string length */
    if (str_len < (pre_len+1) ) {
        *result = (char *)ib_mpool_memdup(mp, str, str_len);
        *result_len = str_len;
        IB_FTRACE_RET_STATUS(IB_OK);
    }

    /* Loop til the cows come home */
    while (1) {
        const char *pre = NULL; /* Pointer to found prefix string */
        size_t      pre_off = 0;/* Offset of prefix in the string */
        const char *suf = NULL; /* Pointer to found suffix string */
        const char *name;       /* Pointer to the name between pre and suffix */
        size_t      namelen;    /* Length of the name */
        char       *new;        /* New buffer */
        size_t      newlen;     /* Length of new buffer */
        const char *iptr;       /* Initial block (up to the prefix) */
        size_t      ilen;       /* Length of the initial block */
        const char *fptr;       /* Final block (after the suffix) */
        size_t      flen;       /* Length of the final block */
        ib_field_t *f;          /* Field */
        size_t      slen;       /* Length of the buffer to search for prefix */

        /* Look for the last prefix in the string with a matching suffix */
        slen = buflen;
        while ( (pre == NULL) && (slen >= pre_len) ) {
            if (recurse == true) {
                pre = ib_strrstr_ex(buf, slen, prefix, pre_len);
            }
            else {
                pre = ib_strstr_ex(buf, slen, prefix, pre_len);
            }
            if (pre == NULL) {
                break;
            }

            /* Lazy compute suffix length */
            if (suf_len == SIZE_MAX) {
                suf_len = strlen(suffix);
                assert (suf_len != 0);
            }

            /* And the next matching suffix */
            pre_off = pre - buf;
            suf = ib_strstr_ex(pre+pre_len,
                               buflen - (pre_off + pre_len),
                               suffix,
                               suf_len);
            if ( (recurse == true) && (suf == NULL) ) {
                slen = (pre - buf);
                pre = NULL;
            }
        }

        /* Did we find a matching pair? */
        if ( (pre == NULL) || (suf == NULL) ) {
            break;
        }

        /* The name is the block between the two */
        name = (pre + pre_len);
        namelen = (suf - pre) - pre_len;

        /* Length of the initial block */
        iptr = buf;
        ilen = (pre - buf);

        /* The final block */
        fptr = (suf + suf_len);
        flen = buflen - (pre_off + pre_len + namelen + suf_len);

        /* Zero length name? Expand it to "" */
        if (namelen == 0) {
            rc = join2(mp,
                       iptr, ilen,
                       fptr, flen,
                       true,
                       &new, &newlen);
            if (rc != IB_OK) {
                IB_FTRACE_RET_STATUS(rc);
            }
            buf = new;
            continue;
        }

        /* Search the hash */
        rc = lookup_fn(lookup_data, name, namelen, &f);
        if (rc == IB_ENOENT) {
            /* Not in the hash; replace with "" */
            rc = join2(mp,
                       iptr, ilen,
                       fptr, flen,
                       true,
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
        rc = join_parts(mp, f, iptr, ilen, fptr, flen, nul, &new, &newlen);
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }
        buf = new;
        buflen = newlen;
    }

    *result = (char *)buf;
    *result_len = buflen;
    IB_FTRACE_RET_STATUS(IB_OK);
}

/*
 * Test whether a given string would be expanded.  See expand.h.
 */
ib_status_t ib_expand_test_str(const char *str,
                               const char *prefix,
                               const char *suffix,
                               bool *result)
{
    IB_FTRACE_INIT();
    ib_status_t rc;

    rc = ib_expand_test_str_ex(str, strlen(str), prefix, suffix, result);

    IB_FTRACE_RET_STATUS(rc);
}

/*
 * Test whether a given string would be expanded, ex version.  See expand.h.
 */
ib_status_t ib_expand_test_str_ex(const char *str,
                                  size_t str_len,
                                  const char *prefix,
                                  const char *suffix,
                                  bool *result)
{
    IB_FTRACE_INIT();
    const char *pre;      /* Pointer to found prefix pattern */
    const char *suf;      /* Pointer to found suffix pattern */
    size_t pre_off;       /* Offset of prefix */
    size_t pre_len;       /* Length of prefix string */

    /* Sanity checks */
    assert(str != NULL);
    assert(prefix != NULL);
    assert(suffix != NULL);
    assert(result != NULL);

    /* Initialize the result to no */
    *result = false;

    /* Validate prefix and suffix */
    if ( (*prefix == '\0') || (*suffix == '\0') ) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    /* Look for the prefix pattern */
    pre_len = strlen(prefix);
    pre = ib_strstr_ex(str, str_len, prefix, pre_len);
    if (pre == NULL) {
        IB_FTRACE_RET_STATUS(IB_OK);
    }

    /* And a next matching suffix pattern. */
    pre_off = pre - str;
    suf = ib_strstr_ex(pre + pre_len,
                       str_len - (pre_off + pre_len),
                       suffix,
                       strlen(suffix));
    if (suf == NULL) {
        IB_FTRACE_RET_STATUS(IB_OK);
    }

    /* Yes, it looks expandable.  Done */
    *result = true;
    IB_FTRACE_RET_STATUS(IB_OK);
}
