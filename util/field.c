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
 * @brief IronBee &mdash; Field Routines
 * @author Brian Rectanus <brectanus@qualys.com>
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include "ironbee_config_auto.h"

#include <ironbee/field.h>

#include <ironbee/stream.h>
#include <ironbee/util.h>
#include <ironbee/bytestr.h>
#include <ironbee/debug.h>

#include "ironbee_util_private.h"

#include <assert.h>

#if ((__GNUC__==4) && (__GNUC_MINOR__==4))
#pragma GCC optimize ("O0")
#pragma message "Warning: GCC optimization turned on off GCC 4.4"
#endif

void ib_field_util_log_debug(  const char       *prefix,
    const ib_field_t *f
)
{
    IB_FTRACE_INIT();
    assert(prefix != NULL);
    assert(f != NULL);

    if (ib_field_is_dynamic(f)) {
        ib_util_log_debug(
            "%s is dynamic: fn_get=%p cbdata_get=%p fn_set=%p cbdata_set=%p",

          prefix,
            f->val->fn_get, f->val->cbdata_get,
            f->val->fn_set, f->val->cbdata_set
        );
    }

    ib_util_log_debug(
        "%s name=%.*s type=%d",
        prefix, f->nlen, f->name, f->type
    );

    if (ib_field_is_dynamic(f)) {
        IB_FTRACE_RET_VOID();
    }

    assert(f->val->pval);

    if (*(void **)(f->val->pval) == NULL) {
        ib_util_log_debug(
            "%s has no value.",
            prefix
        );
    }
    else {
        switch (f->type) {
        case IB_FTYPE_GENERIC:
        {
            void *v;
            ib_field_value(f, ib_ftype_generic_out(&v));
            ib_util_log_debug(
                "%s value=%p",
                prefix, v
            );
            break;
        }
        case IB_FTYPE_NUM:
        {
            ib_num_t v;
            ib_field_value(f, ib_ftype_num_out(&v));
            ib_util_log_debug(
                "%s value=%lld",
                prefix, v
            );
            break;
        }
        case IB_FTYPE_UNUM:
        {
            ib_unum_t v;
            ib_field_value(f, ib_ftype_unum_out(&v));
            ib_util_log_debug(
                "%s value=%llu",
                prefix, v
            );
            break;
        }
        case IB_FTYPE_NULSTR:
        {
            const char *v;
            ib_field_value(f, ib_ftype_nulstr_out(&v));
            ib_util_log_debug(
                "%s value=%s",
                prefix, v
            );
            break;
        }
        case IB_FTYPE_BYTESTR:
        {
            const ib_bytestr_t *v;
            ib_field_value(f, ib_ftype_bytestr_out(&v));
            ib_util_log_debug(
                "%s value=%" IB_BYTESTR_FMT,
                prefix,
                IB_BYTESTR_FMT_PARAM(v)
            );
            break;
        }
        case IB_FTYPE_LIST:
        {
            const ib_list_t* v;
            ib_field_value(f, ib_ftype_list_out(&v));
            ib_util_log_debug(
                "%s &value=%p",
                prefix, v
            );
            break;
        }
        case IB_FTYPE_SBUFFER:
        {
            const ib_stream_t* v;
            ib_field_value(f, ib_ftype_sbuffer_out(&v));
            ib_util_log_debug(
                "%s &value=%p",
                prefix, v
            );
            break;
        }
        default:
            ib_util_log_debug(
                "%s Unknown field type: %d",
                prefix, f->nlen, f->name, f->type
            );
        }
    }
}

ib_status_t ib_field_create(
    ib_field_t **pf,
    ib_mpool_t  *mp,
    const char  *name,
    size_t       nlen,
    ib_ftype_t   type,
    void        *in_pval
)
{
    IB_FTRACE_INIT();
    ib_status_t rc;

    rc = ib_field_create_alias(pf, mp, name, nlen, type, NULL);
    if (rc != IB_OK) {
        goto failed;
    }

    /* Point to internal memory */
    (*pf)->val->pval = &((*pf)->val->u);

    rc = ib_field_setv((*pf), in_pval);
    if (rc != IB_OK) {
        goto failed;
    }

    ib_field_util_log_debug("FIELD_CREATE", (*pf));

    IB_FTRACE_RET_STATUS(IB_OK);

failed:
    /* Make sure everything is cleaned up on failure. */
    *pf = NULL;

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_field_create_no_copy(
    ib_field_t **pf,
    ib_mpool_t  *mp,
    const char  *name,
    size_t       nlen,
    ib_ftype_t   type,
    void        *mutable_in_pval
)
{
    IB_FTRACE_INIT();
    ib_status_t rc;

    rc = ib_field_create_alias(pf, mp, name, nlen, type, NULL);
    if (rc != IB_OK) {
        goto failed;
    }

    /* Point to internal memory */
    (*pf)->val->pval = &((*pf)->val->u);

    rc = ib_field_setv_no_copy((*pf), mutable_in_pval);
    if (rc != IB_OK) {
        goto failed;
    }

    ib_field_util_log_debug("FIELD_CREATE_NO_COPY", (*pf));

    IB_FTRACE_RET_STATUS(IB_OK);

failed:
    /* Make sure everything is cleaned up on failure. */
    *pf = NULL;

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_field_create_alias(
    ib_field_t **pf,
    ib_mpool_t  *mp,
    const char  *name,
    size_t       nlen,
    ib_ftype_t   type,
    void        *storage_pval
)
{
    IB_FTRACE_INIT();
    ib_status_t rc;
    char *name_copy;

    /* Allocate the field structure. */
    *pf = (ib_field_t *)ib_mpool_alloc(mp, sizeof(**pf));
    if (*pf == NULL) {
        rc = IB_EALLOC;
        goto failed;
    }
    (*pf)->mp = mp;
    (*pf)->type = type;
    (*pf)->tfn = NULL;

    /* Copy the name. */
    (*pf)->nlen = nlen;
    name_copy = (char *)ib_mpool_alloc(mp, nlen);
    if (name_copy == NULL) {
        rc = IB_EALLOC;
        goto failed;
    }
    memcpy(name_copy, name, nlen);
    (*pf)->name = (const char *)name_copy;

    (*pf)->val = (ib_field_val_t *)ib_mpool_calloc(mp, 1, sizeof(*((*pf)->val)));
    if ((*pf)->val == NULL) {
        rc = IB_EALLOC;
        goto failed;
    }

    (*pf)->val->pval = storage_pval;

    ib_field_util_log_debug("FIELD_CREATE_ALIAS", (*pf));
    IB_FTRACE_RET_STATUS(IB_OK);

failed:
    /* Make sure everything is cleaned up on failure. */
    *pf = NULL;

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_field_create_dynamic(
    ib_field_t        **pf,
    ib_mpool_t         *mp,
    const char         *name,
    size_t              nlen,
    ib_ftype_t          type,
    ib_field_get_fn_t   fn_get,
    void               *cbdata_get,
    ib_field_set_fn_t   fn_set,
    void               *cbdata_set
)
{
    IB_FTRACE_INIT();
    ib_status_t rc;

    rc = ib_field_create_alias(pf, mp, name, nlen, type, NULL);

    (*pf)->val->fn_get     = fn_get;
    (*pf)->val->fn_set     = fn_set;
    (*pf)->val->cbdata_get = cbdata_get;
    (*pf)->val->cbdata_set = cbdata_set;

    ib_field_util_log_debug("FIELD_CREATE_DYNAMIC", (*pf));
    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_field_alias(
    ib_field_t **pf,
    ib_mpool_t  *mp,
    const char  *name,
    size_t       nlen,
    ib_field_t  *src
)
{
    IB_FTRACE_INIT();

    ib_status_t rc;

    if (ib_field_is_dynamic(src)) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    rc = ib_field_create_alias(
        pf, mp, name, nlen,
        src->type,
        src->val->pval
    );
    if (rc != IB_OK) {
        goto failed;
    }

    ib_field_util_log_debug("FIELD_ALIAS", (*pf));

    IB_FTRACE_RET_STATUS(IB_OK);

failed:
    *pf = NULL;
    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_field_copy(
    ib_field_t       **pf,
    ib_mpool_t        *mp,
    const char        *name,
    size_t             nlen,
    const ib_field_t  *src
)
{
    IB_FTRACE_INIT();

    ib_status_t rc;

    if (ib_field_is_dynamic(src)) {
        rc = ib_field_create_dynamic(
            pf, mp, name, nlen,
            src->type,
            src->val->fn_get,
            src->val->cbdata_get,
            src->val->fn_set,
            src->val->cbdata_set
        );
    }
    else {
        switch (src->type) {
        case IB_FTYPE_NUM:
        {
            ib_num_t v;
            rc = ib_field_value(src, ib_ftype_num_out(&v));
            if (rc != IB_OK) {
                goto failed;
            }
            rc = ib_field_create(
                pf, mp, name, nlen,
                src->type,
                ib_ftype_num_in(&v)
            );
            break;
        }
        case IB_FTYPE_UNUM:
        {
            ib_unum_t v;
            rc = ib_field_value(src, ib_ftype_unum_out(&v));
            if (rc != IB_OK) {
                goto failed;
            }
            rc = ib_field_create(
                pf, mp, name, nlen,
                src->type,
                ib_ftype_unum_in(&v)
            );
            break;
        }
        default:
        {
            void *v;
            rc = ib_field_value(src, &v);
            if (rc != IB_OK) {
                goto failed;
            }
            rc = ib_field_create(
                pf, mp, name, nlen,
                src->type,
                v
            );
        }
        }
    }

    if (rc != IB_OK) {
        goto failed;
    }

    ib_field_util_log_debug("FIELD_COPY", (*pf));

    IB_FTRACE_RET_STATUS(rc);

failed:
    *pf = NULL;
    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_field_create_bytestr_alias(
    ib_field_t **pf,
    ib_mpool_t  *mp,
    const char  *name,
    size_t       nlen,
    uint8_t     *val,
    size_t       vlen
)
{
    IB_FTRACE_INIT();
    ib_status_t rc;
    ib_bytestr_t *bs;

    rc = ib_bytestr_alias_mem(&bs, mp, val, vlen);
    if (rc != IB_OK) {
        goto failed;
    }

    rc = ib_field_create_no_copy(
        pf, mp, name, nlen,
        IB_FTYPE_BYTESTR,
        ib_ftype_bytestr_mutable_in(bs)
    );
    if (rc != IB_OK) {
        goto failed;
    }

    ib_field_util_log_debug("FIELD_CREATE_BYTESTR_ALIAS", (*pf));

    IB_FTRACE_RET_STATUS(IB_OK);

failed:
    /* Make sure everything is cleaned up on failure. */
    *pf = NULL;

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_field_list_add(
    ib_field_t *f,
    ib_field_t *fval
)
{
    IB_FTRACE_INIT();
    ib_status_t rc;
    ib_list_t *l = NULL;

    rc = ib_field_mutable_value_type(
        f,
        ib_ftype_list_mutable_out(&l),
        IB_FTYPE_LIST
    );
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = ib_list_push(l, (void *)fval);
    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_field_buf_add(
    ib_field_t *f,
    int         dtype,
    uint8_t    *buf,
    size_t      blen
)
{
    IB_FTRACE_INIT();
    ib_status_t rc;
    ib_stream_t *s = NULL;

    rc = ib_field_mutable_value_type(
        f,
        ib_ftype_sbuffer_mutable_out(&s),
        IB_FTYPE_SBUFFER
    );
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = ib_stream_push(s, IB_STREAM_DATA, dtype, buf, blen);
    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_field_make_static(
    ib_field_t* f
)
{
    IB_FTRACE_INIT();

    if (! ib_field_is_dynamic(f)) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    f->val->pval       = &(f->val->u);
    *(void**)(f->val->pval) = NULL;
    f->val->fn_get     = NULL;
    f->val->fn_set     = NULL;
    f->val->cbdata_get = NULL;
    f->val->cbdata_set = NULL;

    ib_field_util_log_debug("FIELD_MAKE_STATIC", f);

    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t ib_field_setv_no_copy(
    ib_field_t *f,
    void *mutable_in_pval
)
{
    IB_FTRACE_INIT();

    if (
      f->type == IB_FTYPE_NUM || f->type == IB_FTYPE_UNUM ||
      ib_field_is_dynamic(f)
    ) {
        IB_FTRACE_RET_STATUS(ib_field_setv(f, mutable_in_pval));
    }

    *(void **)(f->val->pval) = mutable_in_pval;


    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t ib_field_setv(
    ib_field_t *f,
    void *in_pval
)
{
    IB_FTRACE_INIT();

    ib_status_t rc = ib_field_setv_ex(f, in_pval, NULL, 0);
    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_field_setv_ex(
    ib_field_t *f,
    void       *in_pval,
    const void *arg,
    size_t      alen
)
{
    IB_FTRACE_INIT();

    ib_status_t rc;

    if (ib_field_is_dynamic(f)) {
        if (f->val->fn_set == NULL) {
            IB_FTRACE_RET_STATUS(IB_EINVAL);
        }
        IB_FTRACE_RET_STATUS(
            f->val->fn_set(f, arg, alen, in_pval, f->val->cbdata_set)
        );
    }

    /* No dynamic setter */
    if (arg != NULL) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    /* What and how it is stored depends on the field type. */
    switch (f->type) {
    case IB_FTYPE_BYTESTR:
    {
        const ib_bytestr_t *bs = (const ib_bytestr_t *)in_pval;
        if (bs != NULL) {
            rc = ib_bytestr_dup((ib_bytestr_t **)f->val->pval, f->mp, bs);
            if (rc != IB_OK) {
                goto failed;
            }
        }
        break;
    }
    case IB_FTYPE_LIST:
    {
        /// @todo Fix const once lists are const aware/copying.
        ib_list_t *l = (ib_list_t *)in_pval;
        if (l != NULL) {
            /// @todo Should do shallow copy
            *(ib_list_t **)f->val->pval = l;
        }
        else {
            rc = ib_list_create((ib_list_t **)f->val->pval, f->mp);
            if (rc != IB_OK) {
                goto failed;
            }
        }
        break;
    }
    case IB_FTYPE_SBUFFER:
    {
        /// @todo Fix once stream is const aware/copying
        ib_stream_t *s = (ib_stream_t *)in_pval;
        if (s != NULL) {
            /// @todo Should do shallow copy
            *(ib_stream_t **)f->val->pval = s;
        }
        else {
            rc = ib_stream_create((ib_stream_t **)f->val->pval, f->mp);
            if (rc != IB_OK) {
                goto failed;
            }
        }
        break;
    }
    case IB_FTYPE_NULSTR:
    {
        const char *s = (const char *)in_pval;
        if (s != NULL) {
            size_t len = strlen(s)+1;

            *(char **)(f->val->pval) = ib_mpool_memdup(f->mp, s, len);
            if (*(void **)(f->val->pval) == NULL) {
                rc = IB_EALLOC;
                goto failed;
            }
        }
        break;
    }
    case IB_FTYPE_NUM:
    {
        ib_num_t n = (in_pval != NULL) ? *(ib_num_t *)in_pval : 0;
        *(ib_num_t *)(f->val->pval) = n;
        break;
    }
    case IB_FTYPE_UNUM:
    {
        ib_unum_t u = (in_pval != NULL) ? *(ib_num_t *)in_pval : 0;
        *(ib_unum_t *)(f->val->pval) = u;
        break;
    }
    case IB_FTYPE_GENERIC:
    default:
    {
        void *p = in_pval;
        *(void **)(f->val->pval) = p;
        break;
    }
    }

    ib_field_util_log_debug("FIELD_SETV", f);

    IB_FTRACE_RET_STATUS(IB_OK);

failed:
    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_field_value_ex(
    const ib_field_t *f,
    void             *out_pval,
    const void       *arg,
    size_t            alen
)
{
    IB_FTRACE_INIT();

    /* If there is not a stored value, then attempt to use the
     * fn_get call to retrieve the value.
     */
    if (ib_field_is_dynamic(f)) {
        if (f->val->fn_get == NULL) {
            IB_FTRACE_RET_STATUS(IB_EINVAL);
        }
        IB_FTRACE_RET_STATUS(
            f->val->fn_get(f, out_pval, arg, alen, f->val->cbdata_get)
        );
    }

    if (arg != NULL) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    switch (f->type) {
    case IB_FTYPE_NUM:
    {
        ib_num_t *n = (ib_num_t *)out_pval;
        *n = *(ib_num_t *)(f->val->pval);
        break;
    }
    case IB_FTYPE_UNUM:
    {
        ib_unum_t *u = (ib_unum_t *)out_pval;
        *u = *(ib_unum_t *)(f->val->pval);
        break;
    }
    default:
        *(void **)out_pval = *(void **)(f->val->pval);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t ib_field_value_type_ex(
    const ib_field_t *f,
    void             *out_pval,
    ib_ftype_t        t,
    const void       *arg,
    size_t            alen
)
{
    IB_FTRACE_INIT();

    /* Compare the types */
    if (f->type != t) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    /* Return the value as normal. */
    IB_FTRACE_RET_STATUS(ib_field_value_ex(f, out_pval, arg, alen));
}

ib_status_t ib_field_value(
    const ib_field_t *f,
    void             *out_pval
)
{
    IB_FTRACE_INIT();
    IB_FTRACE_RET_STATUS(ib_field_value_ex(f, out_pval, NULL, 0));
}

ib_status_t ib_field_value_type(
    const ib_field_t *f,
    void             *out_pval,
    ib_ftype_t        t
)
{
    IB_FTRACE_INIT();

    ib_status_t rc;

    /* Compare the types */
    if (f->type != t) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    /* Return the value as normal. */
    rc = ib_field_value(f, out_pval);

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_field_mutable_value(
    ib_field_t *f,
    void       *mutable_out_pval
)
{
    IB_FTRACE_INIT();

    if (ib_field_is_dynamic(f)) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    if (f->val->pval == NULL) {
        IB_FTRACE_RET_STATUS(IB_ENOENT);
    }

    if (f->type == IB_FTYPE_NUM || f->type == IB_FTYPE_UNUM) {
        *(void**)mutable_out_pval = f->val->pval;
    }
    else {
        *(void**)mutable_out_pval = *(void **)f->val->pval;
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t ib_field_mutable_value_type(
    ib_field_t *f,
    void       *mutable_out_pval,
    ib_ftype_t  t
)
{
    IB_FTRACE_INIT();

    ib_status_t rc;

    /* Compare the types */
    if (f->type != t) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    /* Return the value as normal. */
    rc = ib_field_mutable_value(f, mutable_out_pval);

    IB_FTRACE_RET_STATUS(rc);
}

int ib_field_is_dynamic(const ib_field_t *f)
{
    IB_FTRACE_INIT();

    IB_FTRACE_RET_INT(f->val->pval == NULL ? 1 : 0);
}
