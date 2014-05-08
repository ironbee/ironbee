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
 * @brief IronBee --- Field Routines
 *
 * @author Brian Rectanus <brectanus@qualys.com>
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include "ironbee_config_auto.h"

#include <ironbee/field.h>

#include <ironbee/bytestr.h>
#include <ironbee/engine.h>
#include <ironbee/escape.h>
#include <ironbee/log.h>
#include <ironbee/mm.h>
#include <ironbee/string.h>
#include <ironbee/stream.h>
#include <ironbee/util.h>

#include <assert.h>
#include <inttypes.h>

#if ((__GNUC__==4) && (__GNUC_MINOR__==4))
#pragma GCC optimize ("O0")
#pragma message "Warning: GCC optimization turned on for GCC 4.4. This code may malfunction."
#endif

/** The union of values. */
typedef union {
    ib_num_t       num;           /**< Generic numeric value */
    ib_float_t     fnum;          /**< Floating type value. */
    ib_time_t      time;          /**< Milliseconds since epoch. */
    ib_bytestr_t  *bytestr;       /**< Byte string value */
    char          *nulstr;        /**< NUL string value */
    ib_list_t     *list;          /**< List of fields */
    ib_stream_t   *stream;        /**< Stream buffer */
    void          *ptr;           /**< Pointer value */
} ib_field_val_union_t;

/**
 * Field value structure.
 *
 * This allows for multiple types of values to be stored within a field.
 */
struct ib_field_val_t {
    ib_field_get_fn_t     fn_get;        /**< Function to get a value. */
    ib_field_set_fn_t     fn_set;        /**< Function to set a value. */
    void                 *cbdata_get;    /**< Data passed to fn_get. */
    void                 *cbdata_set;    /**< Data passed to fn_get. */
    void                 *pval;          /**< Address where value is stored */
    ib_field_val_union_t  u;             /**< Union of value types */
};

const char *ib_field_type_name(
    ib_ftype_t ftype
)
{
    switch(ftype) {
    case IB_FTYPE_GENERIC:
        return "GENERIC";
    case IB_FTYPE_NUM:
        return "NUM";
    case IB_FTYPE_TIME:
        return "TIME";
    case IB_FTYPE_FLOAT:
        return "FLOAT";
    case IB_FTYPE_NULSTR:
        return "NULSTR";
    case IB_FTYPE_BYTESTR:
        return "BYTESTR";
    case IB_FTYPE_LIST:
        return "LIST";
    case IB_FTYPE_SBUFFER:
        return "SBUFFER";
    default:
        return "Unknown";
    }
}

static ib_status_t field_from_string_internal(
    ib_mm_t mm,
    const char *name,
    size_t nlen,
    const char *vstr,
    size_t vlen,
    bool vstr_is_nulstr,
    ib_field_t **pfield)
{
    assert(name != NULL);
    assert(vstr != NULL);
    assert(pfield != NULL);

    ib_status_t conv;
    ib_status_t rc = IB_OK;
    ib_field_t *field;

    *pfield = NULL;

    /* Try to convert to an integer */
    if (*pfield == NULL) {
        ib_num_t num_val;
        if (vstr_is_nulstr) {
            conv = ib_string_to_num(vstr, 0, &num_val);
        }
        else {
            conv = ib_string_to_num_ex(vstr, vlen, 0, &num_val);
        }
        if (conv == IB_OK) {
            rc = ib_field_create(&field, mm,
                                 name, nlen,
                                 IB_FTYPE_NUM,
                                 ib_ftype_num_in(&num_val));
            *pfield = field;
        }
    }

    /* Try to convert to a float */
    if (*pfield == NULL) {
        ib_float_t float_val;
        if (vstr_is_nulstr) {
            conv = ib_string_to_float(vstr, &float_val);
        }
        else {
            conv = ib_string_to_float_ex(vstr, vlen, &float_val);
        }
        if (conv == IB_OK) {
            rc = ib_field_create(&field, mm,
                                 name, nlen,
                                 IB_FTYPE_FLOAT,
                                 ib_ftype_float_in(&float_val));
            *pfield = field;
        }
    }

    /* Finally, assume that it's a string */
    if (*pfield == NULL) {
        if (vstr_is_nulstr) {
            rc = ib_field_create(&field, mm,
                                 name, nlen,
                                 IB_FTYPE_NULSTR,
                                 ib_ftype_nulstr_in(vstr));
        }
        else {
            ib_bytestr_t *bs;
            rc = ib_bytestr_dup_mem(&bs, mm, (const uint8_t *)vstr, vlen);
            if (rc != IB_OK) {
                return rc;
            }
            rc = ib_field_create(&field, mm,
                                 name, nlen,
                                 IB_FTYPE_BYTESTR,
                                 ib_ftype_bytestr_in(bs));
        }
        *pfield = field;
    }

    return rc;
}

ib_status_t ib_field_from_string(
    ib_mm_t mm,
    const char *name,
    size_t nlen,
    const char *vstr,
    ib_field_t **pfield)
{
    return field_from_string_internal(mm,
                                      name, nlen,
                                      vstr, 0, true,
                                      pfield);
}

ib_status_t ib_field_from_string_ex(
    ib_mm_t mm,
    const char *name,
    size_t nlen,
    const char *vstr,
    size_t vlen,
    ib_field_t **pfield)
{
    return field_from_string_internal(mm,
                                      name, nlen,
                                      vstr, vlen, false,
                                      pfield);
}

void ib_field_util_log_debug(
    const char       *prefix,
    const ib_field_t *f
)
{
    assert(prefix != NULL);
    assert(f != NULL);

    ib_status_t rc;

    if (ib_util_get_log_level() < IB_LOG_DEBUG) {
        return;
    }

    if (ib_field_is_dynamic(f)) {
        ib_util_log_debug(
            "%s is dynamic: fn_get=%p cbdata_get=%p fn_set=%p cbdata_set=%p",

          prefix,
            f->val->fn_get, f->val->cbdata_get,
            f->val->fn_set, f->val->cbdata_set
        );
    }

    ib_util_log_debug("%s name=%.*s type=%d",
                      prefix, (int)f->nlen, f->name, f->type
    );

    if (ib_field_is_dynamic(f)) {
        return;
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
            rc = ib_field_value(f, ib_ftype_generic_out(&v));
            if (rc == IB_OK) {
                ib_util_log_debug("%s value=%p", prefix, v);
            }
            break;
        }
        case IB_FTYPE_TIME:
        {
            ib_time_t v;
            rc = ib_field_value(f, ib_ftype_time_out(&v));
            if (rc == IB_OK) {
                ib_util_log_debug("%s value=%"PRIu64, prefix, v);
            }
            break;
        }
        case IB_FTYPE_NUM:
        {
            ib_num_t v;
            rc = ib_field_value(f, ib_ftype_num_out(&v));
            if (rc == IB_OK) {
                ib_util_log_debug("%s value=%"PRId64, prefix, v);
            }
            break;
        }
        case IB_FTYPE_FLOAT:
        {
            ib_float_t v;
            rc = ib_field_value(f, ib_ftype_float_out(&v));
            if (rc == IB_OK) {
                ib_util_log_debug("%s value=%Lf", prefix, v);
            }
            break;
        }
        case IB_FTYPE_NULSTR:
        {
            const char *v;
            rc = ib_field_value(f, ib_ftype_nulstr_out(&v));
            if (rc == IB_OK) {
                ib_util_log_debug("%s value=%s", prefix, v);
            }
            break;
        }
        case IB_FTYPE_BYTESTR:
        {
            const ib_bytestr_t *v;
            rc = ib_field_value(f, ib_ftype_bytestr_out(&v));
            if (rc == IB_OK) {
                ib_util_log_debug("%s value=%" IB_BYTESTR_FMT,
                                  prefix, IB_BYTESTR_FMT_PARAM(v));
            }
            break;
        }
        case IB_FTYPE_LIST:
        {
            const ib_list_t* v;
            rc = ib_field_value(f, ib_ftype_list_out(&v));
            if (rc == IB_OK) {
                ib_util_log_debug("%s &value=%p", prefix, v);
            }
            break;
        }
        case IB_FTYPE_SBUFFER:
        {
            const ib_stream_t* v;
            rc = ib_field_value(f, ib_ftype_sbuffer_out(&v));
            if (rc == IB_OK) {
                ib_util_log_debug("%s &value=%p", prefix, v);
            }
            break;
        }
        default:
            ib_util_log_debug("%s Unknown field type: %u",
                              prefix, f->type
            );
        }
    }
}

ib_status_t ib_field_create(
    ib_field_t **pf,
    ib_mm_t      mm,
    const char  *name,
    size_t       nlen,
    ib_ftype_t   type,
    void        *in_pval
)
{
    ib_status_t rc;

    rc = ib_field_create_alias(pf, mm, name, nlen, type, NULL);
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

    return IB_OK;

failed:
    /* Make sure everything is cleaned up on failure. */
    *pf = NULL;

    return rc;
}

ib_status_t ib_field_create_no_copy(
    ib_field_t **pf,
    ib_mm_t      mm,
    const char  *name,
    size_t       nlen,
    ib_ftype_t   type,
    void        *mutable_in_pval
)
{
    ib_status_t rc;

    rc = ib_field_create_alias(pf, mm, name, nlen, type, NULL);
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

    return IB_OK;

failed:
    /* Make sure everything is cleaned up on failure. */
    *pf = NULL;

    return rc;
}

ib_status_t ib_field_create_alias(
    ib_field_t **pf,
    ib_mm_t      mm,
    const char  *name,
    size_t       nlen,
    ib_ftype_t   type,
    void        *storage_pval
)
{
    ib_status_t rc;
    char *name_copy;

    /* Allocate the field structure. */
    *pf = (ib_field_t *)ib_mm_alloc(mm, sizeof(**pf));
    if (*pf == NULL) {
        rc = IB_EALLOC;
        goto failed;
    }
    (*pf)->mm = mm;
    (*pf)->type = type;
    (*pf)->tfn = NULL;

    /* Copy the name. */
    (*pf)->nlen = nlen;
    name_copy = (char *)ib_mm_alloc(mm, nlen);
    if (name_copy == NULL) {
        rc = IB_EALLOC;
        goto failed;
    }
    memcpy(name_copy, name, nlen);
    (*pf)->name = (const char *)name_copy;

    (*pf)->val = (ib_field_val_t *)ib_mm_calloc(mm,
        1, sizeof(*((*pf)->val))
    );
    if ((*pf)->val == NULL) {
        rc = IB_EALLOC;
        goto failed;
    }

    (*pf)->val->pval = storage_pval;

    ib_field_util_log_debug("FIELD_CREATE_ALIAS", (*pf));
    return IB_OK;

failed:
    /* Make sure everything is cleaned up on failure. */
    *pf = NULL;

    return rc;
}

ib_status_t ib_field_create_dynamic(
    ib_field_t        **pf,
    ib_mm_t             mm,
    const char         *name,
    size_t              nlen,
    ib_ftype_t          type,
    ib_field_get_fn_t   fn_get,
    void               *cbdata_get,
    ib_field_set_fn_t   fn_set,
    void               *cbdata_set
)
{
    ib_status_t rc;

    rc = ib_field_create_alias(pf, mm, name, nlen, type, NULL);
    if (rc != IB_OK) {
        return rc;
    }

    (*pf)->val->fn_get     = fn_get;
    (*pf)->val->fn_set     = fn_set;
    (*pf)->val->cbdata_get = cbdata_get;
    (*pf)->val->cbdata_set = cbdata_set;

    ib_field_util_log_debug("FIELD_CREATE_DYNAMIC", (*pf));
    return IB_OK;
}

ib_status_t ib_field_alias(
    ib_field_t **pf,
    ib_mm_t      mm,
    const char  *name,
    size_t       nlen,
    const ib_field_t  *src
)
{
    ib_status_t rc;

    if (ib_field_is_dynamic(src)) {
        return IB_EINVAL;
    }

    rc = ib_field_create_alias(
        pf, mm, name, nlen,
        src->type,
        src->val->pval
    );
    if (rc != IB_OK) {
        goto failed;
    }

    ib_field_util_log_debug("FIELD_ALIAS", (*pf));

    return IB_OK;

failed:
    *pf = NULL;
    return rc;
}

ib_status_t ib_field_copy(
    ib_field_t       **pf,
    ib_mm_t            mm,
    const char        *name,
    size_t             nlen,
    const ib_field_t  *src
)
{
    ib_status_t rc;

    if (ib_field_is_dynamic(src)) {
        rc = ib_field_create_dynamic(
            pf, mm, name, nlen,
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
                pf, mm, name, nlen,
                src->type,
                ib_ftype_num_in(&v)
            );
            break;
        }
        case IB_FTYPE_TIME:
        {
            ib_time_t v;
            rc = ib_field_value(src, ib_ftype_time_out(&v));
            if (rc != IB_OK) {
                goto failed;
            }
            rc = ib_field_create(
                pf, mm, name, nlen,
                src->type,
                ib_ftype_time_in(&v)
            );
            break;
        }
        case IB_FTYPE_FLOAT:
        {
            ib_float_t v;
            rc = ib_field_value(src, ib_ftype_float_out(&v));
            if (rc != IB_OK) {
                goto failed;
            }
            rc = ib_field_create(
                pf, mm, name, nlen,
                src->type,
                ib_ftype_float_in(&v)
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
                pf, mm, name, nlen,
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

    return rc;

failed:
    *pf = NULL;
    return rc;
}

ib_status_t ib_field_create_bytestr_alias(
    ib_field_t    **pf,
    ib_mm_t         mm,
    const char     *name,
    size_t          nlen,
    const uint8_t  *val,
    size_t          vlen
)
{
    ib_status_t rc;
    ib_bytestr_t *bs;

    rc = ib_bytestr_alias_mem(&bs, mm, val, vlen);
    if (rc != IB_OK) {
        goto failed;
    }

    rc = ib_field_create_no_copy(
        pf, mm, name, nlen,
        IB_FTYPE_BYTESTR,
        ib_ftype_bytestr_mutable_in(bs)
    );
    if (rc != IB_OK) {
        goto failed;
    }

    ib_field_util_log_debug("FIELD_CREATE_BYTESTR_ALIAS", (*pf));

    return IB_OK;

failed:
    /* Make sure everything is cleaned up on failure. */
    *pf = NULL;

    return rc;
}

ib_status_t ib_field_list_add_const(
    ib_field_t *f,
    const ib_field_t *fval
)
{
    ib_status_t rc;
    ib_list_t *l = NULL;

    rc = ib_field_mutable_value_type(
        f,
        ib_ftype_list_mutable_out(&l),
        IB_FTYPE_LIST
    );
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_list_push(l, (void *)fval);
    return rc;
}

ib_status_t ib_field_list_add(
    ib_field_t *f,
    ib_field_t *fval
)
{
    ib_status_t rc;
    ib_list_t *l = NULL;

    rc = ib_field_mutable_value_type(
        f,
        ib_ftype_list_mutable_out(&l),
        IB_FTYPE_LIST
    );
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_list_push(l, (void *)fval);
    return rc;
}

ib_status_t ib_field_buf_add(
    ib_field_t *f,
    int         dtype,
    uint8_t    *buf,
    size_t      blen
)
{
    ib_status_t rc;
    ib_stream_t *s = NULL;

    rc = ib_field_mutable_value_type(
        f,
        ib_ftype_sbuffer_mutable_out(&s),
        IB_FTYPE_SBUFFER
    );
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_stream_push(s, IB_STREAM_DATA, buf, blen);
    return rc;
}

ib_status_t ib_field_make_static(
    ib_field_t* f
)
{
    if (! ib_field_is_dynamic(f)) {
        return IB_EINVAL;
    }

    f->val->pval       = &(f->val->u);
    *(void**)(f->val->pval) = NULL;
    f->val->fn_get     = NULL;
    f->val->fn_set     = NULL;
    f->val->cbdata_get = NULL;
    f->val->cbdata_set = NULL;

    ib_field_util_log_debug("FIELD_MAKE_STATIC", f);

    return IB_OK;
}

ib_status_t ib_field_setv_no_copy(
    ib_field_t *f,
    void *mutable_in_pval
)
{
    if (
        f->type == IB_FTYPE_NUM ||
        f->type == IB_FTYPE_FLOAT ||
        f->type == IB_FTYPE_TIME ||
        ib_field_is_dynamic(f)
    ) {
        return ib_field_setv(f, mutable_in_pval);
    }

    *(void **)(f->val->pval) = mutable_in_pval;

    return IB_OK;
}

ib_status_t ib_field_setv(
    ib_field_t *f,
    void *in_pval
)
{
    ib_status_t rc = ib_field_setv_ex(f, in_pval, NULL, 0);

    return rc;
}

ib_status_t ib_field_setv_ex(
    ib_field_t *f,
    void       *in_pval,
    const void *arg,
    size_t      alen
)
{
    ib_status_t rc;

    if (ib_field_is_dynamic(f)) {
        if (f->val->fn_set == NULL) {
            return IB_EINVAL;
        }
        return f->val->fn_set(f, arg, alen, in_pval, f->val->cbdata_set);
    }

    /* No dynamic setter */
    if (arg != NULL) {
        return IB_EINVAL;
    }

    /* What and how it is stored depends on the field type. */
    switch (f->type) {
    case IB_FTYPE_BYTESTR:
    {
        const ib_bytestr_t *bs = (const ib_bytestr_t *)in_pval;
        if (bs != NULL) {
            rc = ib_bytestr_dup((ib_bytestr_t **)f->val->pval, f->mm, bs);
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
            rc = ib_list_create(
                (ib_list_t **)f->val->pval,
                f->mm
            );
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
            rc = ib_stream_create((ib_stream_t **)f->val->pval, f->mm);
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
            *(char **)(f->val->pval) = ib_mm_strdup(f->mm, s);
            if (*(void **)(f->val->pval) == NULL) {
                rc = IB_EALLOC;
                goto failed;
            }
        }
        break;
    }
    case IB_FTYPE_TIME:
    {
        ib_time_t n = (in_pval != NULL) ? *(ib_time_t *)in_pval : 0;
        *(ib_time_t *)(f->val->pval) = n;
        break;
    }
    case IB_FTYPE_NUM:
    {
        ib_num_t n = (in_pval != NULL) ? *(ib_num_t *)in_pval : 0;
        *(ib_num_t *)(f->val->pval) = n;
        break;
    }
    case IB_FTYPE_FLOAT:
    {
        ib_float_t fl = (in_pval != NULL) ? *(ib_float_t *)in_pval : 0;
        *(ib_float_t *)(f->val->pval) = fl;
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

    return IB_OK;

failed:
    return rc;
}

ib_status_t ib_field_value_ex(
    const ib_field_t *f,
    void             *out_pval,
    const void       *arg,
    size_t            alen
)
{
    /* If there is not a stored value, then attempt to use the
     * fn_get call to retrieve the value.
     */
    if (ib_field_is_dynamic(f)) {
        if (f->val->fn_get == NULL) {
            return IB_EINVAL;
        }
        return f->val->fn_get(f, out_pval, arg, alen, f->val->cbdata_get);
    }

    if (arg != NULL) {
        return IB_EINVAL;
    }

    switch (f->type) {
    case IB_FTYPE_TIME:
    {
        ib_time_t *n = (ib_time_t *)out_pval;
        *n = *(ib_time_t *)(f->val->pval);
        break;
    }
    case IB_FTYPE_NUM:
    {
        ib_num_t *n = (ib_num_t *)out_pval;
        *n = *(ib_num_t *)(f->val->pval);
        break;
    }
    case IB_FTYPE_FLOAT:
    {
        ib_float_t *fl = (ib_float_t *)out_pval;
        *fl = *(ib_float_t *)(f->val->pval);
        break;
    }
    default:
        *(void **)out_pval = *(void **)(f->val->pval);
    }

    return IB_OK;
}

ib_status_t ib_field_value_type_ex(
    const ib_field_t *f,
    void             *out_pval,
    ib_ftype_t        t,
    const void       *arg,
    size_t            alen
)
{
    /* Compare the types */
    if (f->type != t) {
        return IB_EINVAL;
    }

    /* Return the value as normal. */
    return ib_field_value_ex(f, out_pval, arg, alen);
}

ib_status_t ib_field_value(
    const ib_field_t *f,
    void             *out_pval
)
{
    return ib_field_value_ex(f, out_pval, NULL, 0);
}

ib_status_t ib_field_value_type(
    const ib_field_t *f,
    void             *out_pval,
    ib_ftype_t        t
)
{
    ib_status_t rc;

    /* Compare the types */
    if (f->type != t) {
        return IB_EINVAL;
    }

    /* Return the value as normal. */
    rc = ib_field_value(f, out_pval);

    return rc;
}

ib_status_t ib_field_mutable_value(
    ib_field_t *f,
    void       *mutable_out_pval
)
{
    if (ib_field_is_dynamic(f)) {
        return IB_EINVAL;
    }

    if (f->val->pval == NULL) {
        return IB_ENOENT;
    }

    switch(f->type) {
        case IB_FTYPE_NUM:
        case IB_FTYPE_FLOAT:
        case IB_FTYPE_TIME:
            *(void**)mutable_out_pval = f->val->pval;
            break;
        default:
            *(void**)mutable_out_pval = *(void **)f->val->pval;
    }

    return IB_OK;
}

ib_status_t ib_field_mutable_value_type(
    ib_field_t *f,
    void       *mutable_out_pval,
    ib_ftype_t  t
)
{
    ib_status_t rc;

    /* Compare the types */
    if (f->type != t) {
        return IB_EINVAL;
    }

    /* Return the value as normal. */
    rc = ib_field_mutable_value(f, mutable_out_pval);

    return rc;
}

int ib_field_is_dynamic(const ib_field_t *f)
{
    return f->val->pval == NULL ? 1 : 0;
}

ib_status_t ib_field_convert(
    ib_mm_t            mm,
    const ib_ftype_t   desired_type,
    const ib_field_t  *in_field,
    ib_field_t       **out_field
)
{
    assert(in_field != NULL);

    ib_status_t rc;

    /* Holder values for in_field values before being set in out_field. */
    size_t sz;
    const char *str;
    const ib_bytestr_t *bstr;
    ib_num_t num;
    ib_time_t tme;
    ib_float_t flt;
    void *new_field_value;

    if (in_field->type == desired_type) {
        *out_field = NULL;
        return IB_OK;
    }

    switch (in_field->type) {
    case IB_FTYPE_NULSTR:

        /* Extract string.  Note that a zero-length nulstr field can
         * have a NULL value in the union. */
        if ( (in_field->val->u.nulstr == NULL) &&
             (in_field->val->pval == NULL) )
        {
            str = "";
        }
        else {
            rc = ib_field_value(in_field, ib_ftype_nulstr_out(&str));
            if (rc != IB_OK){
                return rc;
            }
        }

        switch (desired_type) {
        case IB_FTYPE_BYTESTR:
            rc = ib_bytestr_dup_nulstr((ib_bytestr_t **)&bstr, mm, str);
            if (rc != IB_OK) {
                return rc;
            }
            new_field_value = ib_ftype_bytestr_in(bstr);
            break;
        case IB_FTYPE_TIME:
            rc = ib_string_to_time(str, &tme);
            if (rc != IB_OK) {
                return rc;
            }
            new_field_value = ib_ftype_time_in(&tme);
            break;
        case IB_FTYPE_NUM:
            rc = ib_string_to_num(str, 0, &num);
            if (rc != IB_OK) {
                return rc;
            }
            new_field_value = ib_ftype_num_in(&num);
            break;
        case IB_FTYPE_FLOAT:
            rc = ib_string_to_float(str, &flt);
            if (rc != IB_OK) {
                return rc;
            }
            new_field_value = ib_ftype_float_in(&flt);
            break;
        default:
            return IB_EINVAL;
        }
        break;

    case IB_FTYPE_BYTESTR:

        /* Extract bytestr. */
        rc = ib_field_value(in_field, ib_ftype_bytestr_out(&bstr));
        if (rc != IB_OK){
            return rc;
        }
        str = (const char *)ib_bytestr_const_ptr(bstr);
        sz = ib_bytestr_length(bstr);

        /* Convert byte str. */
        switch(desired_type) {
        case IB_FTYPE_NULSTR:
            str = ib_mm_memdup_to_str(mm, str, sz);
            if (!str) {
                return rc;
            }
            new_field_value = ib_ftype_nulstr_in(str);
            break;
        case IB_FTYPE_TIME:
            rc = ib_string_to_time_ex(str, sz, &tme);
            if (rc != IB_OK) {
                return rc;
            }
            new_field_value = ib_ftype_time_in(&tme);
            break;
        case IB_FTYPE_NUM:
            rc = ib_string_to_num_ex(str, sz, 0, &num);
            if (rc != IB_OK) {
                return rc;
            }
            new_field_value = ib_ftype_num_in(&num);
            break;
        case IB_FTYPE_FLOAT:
            rc = ib_string_to_float_ex(str, sz, &flt);
            if (rc != IB_OK) {
                return rc;
            }
            new_field_value = ib_ftype_float_in(&flt);
            break;
        default:
            return IB_EINVAL;
        }
        break;

    case IB_FTYPE_TIME:

        /* Extract time. */
        rc = ib_field_value(in_field, ib_ftype_time_out(&tme));
        if (rc != IB_OK){
            return rc;
        }

        switch (desired_type) {
        case IB_FTYPE_NULSTR:
            str = ib_time_to_string(mm, tme);
            if (! str) {
                return IB_EINVAL;
            }
            new_field_value = ib_ftype_nulstr_in(str);
            break;
        case IB_FTYPE_BYTESTR:
            str = ib_time_to_string(mm, tme);
            if (! str) {
                return IB_EINVAL;
            }
            rc = ib_bytestr_dup_nulstr((ib_bytestr_t **)&bstr, mm, str);
            if (rc != IB_OK){
                return rc;
            }
            new_field_value = ib_ftype_bytestr_in(bstr);
            break;
        case IB_FTYPE_FLOAT:
            flt = (ib_float_t)tme;
            /* Check that our assignment is within error=1, or fail. */
            if (llabs(tme - flt) > 1) {
                return IB_EINVAL;
            }
            new_field_value = ib_ftype_float_in(&flt);
            break;
        default:
            return IB_EINVAL;
        }
        break;

    case IB_FTYPE_NUM:

        /* Extract unum. */
        rc = ib_field_value(in_field, ib_ftype_num_out(&num));
        if (rc != IB_OK){
            return rc;
        }

        switch (desired_type) {
        case IB_FTYPE_NULSTR:
            str = ib_num_to_string(mm, num);
            if (! str) {
                return IB_EINVAL;
            }
            new_field_value = ib_ftype_nulstr_in(str);
            break;
        case IB_FTYPE_BYTESTR:
            str = ib_num_to_string(mm, num);
            if (! str) {
                return IB_EINVAL;
            }
            rc = ib_bytestr_dup_nulstr((ib_bytestr_t **)&bstr, mm, str);
            if (rc != IB_OK){
                return rc;
            }
            new_field_value = ib_ftype_bytestr_in(bstr);
            break;
        case IB_FTYPE_TIME:
            if (num < 0) {
                return IB_EINVAL;
            }
            tme = (ib_time_t)num;
            new_field_value = ib_ftype_time_in(&tme);
            break;
        case IB_FTYPE_FLOAT:
            flt = (ib_float_t)num;
            if (llabs(flt - num) > 1) {
                return IB_EINVAL;
            }
            new_field_value = ib_ftype_float_in(&flt);
            break;
        default:
            return IB_EINVAL;
        }
        break;

    case IB_FTYPE_FLOAT:

        /* Extract unum. */
        rc = ib_field_value(in_field, ib_ftype_float_out(&flt));
        if (rc != IB_OK){
            return rc;
        }

        switch (desired_type) {
        case IB_FTYPE_NULSTR:
            str = ib_float_to_string(mm, flt);
            if (!str) {
                return IB_EINVAL;
            }
            new_field_value = ib_ftype_nulstr_in(str);
            break;
        case IB_FTYPE_BYTESTR:
            str = ib_float_to_string(mm, flt);
            if (!str) {
                return IB_EINVAL;
            }
            rc = ib_bytestr_dup_nulstr((ib_bytestr_t **)&bstr, mm, str);
            if (rc != IB_OK){
                return rc;
            }
            new_field_value = ib_ftype_bytestr_in(bstr);
            break;
        case IB_FTYPE_TIME:
            if (flt < 0) {
                return IB_EINVAL;
            }
            tme = (ib_float_t)flt;
            new_field_value = ib_ftype_time_in(&tme);
            break;
        case IB_FTYPE_NUM:
            num = (ib_float_t)flt;
            new_field_value = ib_ftype_num_in(&num);
            break;
        default:
            return IB_EINVAL;
        }
        break;

    default:
        return IB_EINVAL;
    }

    rc = ib_field_create(
        out_field,
        mm,
        in_field->name,
        in_field->nlen,
        desired_type,
        new_field_value
    );
    return rc;
}
