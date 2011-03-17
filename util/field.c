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
 * @brief IronBee - Field Routines
 * @author Brian Rectanus <brectanus@qualys.com>
 */

#include "ironbee_config_auto.h"

#include <stdlib.h>
#include <string.h>

#include <ironbee/util.h>

#if defined(__cplusplus) && !defined(__STDC_FORMAT_MACROS)
/* C99 requires that inttypes.h only exposes PRI* macros
 * for C++ implementations if this is defined: */
#define __STDC_FORMAT_MACROS
#endif

#include <inttypes.h>

#include "ironbee_util_private.h"

ib_status_t ib_field_create_ex(ib_field_t **pf,
                               ib_mpool_t *mp,
                               const char *name,
                               size_t nlen,
                               ib_ftype_t type,
                               void *pval)
{
    IB_FTRACE_INIT(ib_field_create_ex);
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

    /*
     * Make a copy of the value.
     *
     * The value is stored within the private val structure and owned
     * by the field.  It is also possible to store the value
     * externally (see createn version).
     */
    /// @todo Make this a function
    (*pf)->val = ib_mpool_alloc(mp, sizeof(*((*pf)->val)));
    if ((*pf)->val == NULL) {
        rc = IB_EALLOC;
        goto failed;
    }

    /* What and how it is stored depends on the field type. */
    switch (type) {
        case IB_FTYPE_BYTESTR:
            if (pval != NULL) {
                rc = ib_bytestr_dup(&(*pf)->val->u.bytestr,
                                    mp,
                                    *(ib_bytestr_t **)pval);
                if (rc != IB_OK) {
                    goto failed;
                }
                ib_util_log_debug(9, "CREATE FIELD type=%d %" IB_BYTESTR_FMT "=\"%" IB_BYTESTR_FMT "\" (%p)", type, IB_BYTESTRSL_FMT_PARAM((*pf)->name,(*pf)->nlen), IB_BYTESTR_FMT_PARAM((*pf)->val->u.bytestr), (*pf)->val->u.bytestr);
            }
            else {
                (*pf)->val->u.bytestr = NULL;
            }
            (*pf)->val->pval = (void *)&((*pf)->val->u.bytestr);
            break;
        case IB_FTYPE_LIST:
            if (pval != NULL) {
                /// @todo Should do shallow copy
                (*pf)->val->u.list = *(ib_list_t **)pval;
            }
            else {
                rc = ib_list_create(&(*pf)->val->u.list, mp);
                if (rc != IB_OK) {
                    goto failed;
                }
            }
            (*pf)->val->pval = (void *)&((*pf)->val->u.list);
            break;
        case IB_FTYPE_NULSTR:
            if (pval != NULL) {
                size_t len = strlen(*(char **)pval) + 1;

                (*pf)->val->u.nulstr = (char *)ib_mpool_alloc(mp, len);
                if ((*pf)->val->u.nulstr == NULL) {
                    rc = IB_EALLOC;
                    goto failed;
                }
                memcpy((*pf)->val->u.nulstr, *(char **)pval, len);
            }
            else {
                /// @todo Or should this be ""???
                (*pf)->val->u.nulstr = NULL;
            }
            ib_util_log_debug(9, "CREATE FIELD type=%d %" IB_BYTESTR_FMT "=\"%s\" (%p)", type, IB_BYTESTRSL_FMT_PARAM((*pf)->name,(*pf)->nlen), (*pf)->val->u.nulstr, (*pf)->val->u.nulstr);
            (*pf)->val->pval = (void *)&((*pf)->val->u.nulstr);
            break;
        case IB_FTYPE_NUM:
            (*pf)->val->u.num =
                (pval != NULL) ? *(ib_num_t *)pval : 0;
            ib_util_log_debug(9, "CREATE FIELD type=%d %" IB_BYTESTR_FMT "=%d", type, IB_BYTESTRSL_FMT_PARAM((*pf)->name,(*pf)->nlen), (int)(*pf)->val->u.num);
            (*pf)->val->pval = (void *)&((*pf)->val->u.num);
            break;
        case IB_FTYPE_UNUM:
            (*pf)->val->u.unum =
                (pval != NULL) ? *(ib_num_t *)pval : 0;
            ib_util_log_debug(9, "CREATE FIELD type=%d %" IB_BYTESTR_FMT "=%d", type, IB_BYTESTRSL_FMT_PARAM((*pf)->name,(*pf)->nlen), (unsigned int)(*pf)->val->u.unum);
            (*pf)->val->pval = (void *)&((*pf)->val->u.unum);
            break;
        case IB_FTYPE_GENERIC:
        default:
            (*pf)->val->u.ptr =
                (pval != NULL) ? *(void **)pval : NULL;
            (*pf)->val->pval = (void *)&((*pf)->val->u.ptr);
            break;
    }

    IB_FTRACE_RET_STATUS(IB_OK);

failed:
    /* Make sure everything is cleaned up on failure. */
    *pf = NULL;

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_field_createn_ex(ib_field_t **pf,
                                ib_mpool_t *mp,
                                const char *name,
                                size_t nlen,
                                ib_ftype_t type,
                                void *pval)
{
    IB_FTRACE_INIT(ib_field_createn_ex);
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

    /*
     * Set the value directly (alias)
     *
     * Instead of pval pointing into the private val structure, it
     * points externally allowing custom manipulation by an external
     * entity.  This is used internally for configuration so that
     * a module can access its own structure, but the engine and
     * configuration can access the data via named fields in a hash.
     */
    /// @todo Make this a function
    /// @todo Make val->pval access not require allocating all of val
    (*pf)->val = ib_mpool_alloc(mp, sizeof(*((*pf)->val)));
    if ((*pf)->val == NULL) {
        rc = IB_EALLOC;
        goto failed;
    }
    (*pf)->val->pval = pval;
    switch (type) {
        case IB_FTYPE_BYTESTR:
            ib_util_log_debug(9, "CREATEN FIELD type=%d %" IB_BYTESTR_FMT "=\"%" IB_BYTESTR_FMT "\" (%p)", type, IB_BYTESTRSL_FMT_PARAM((*pf)->name,(*pf)->nlen), IB_BYTESTR_FMT_PARAM(*(ib_bytestr_t **)((*pf)->val->pval)), (*pf)->val->pval);
            break;
        case IB_FTYPE_LIST:
            break;
        case IB_FTYPE_NULSTR:
            ib_util_log_debug(9, "CREATEN FIELD type=%d %" IB_BYTESTR_FMT "=\"%s\" (%p)", type, IB_BYTESTRSL_FMT_PARAM((*pf)->name,(*pf)->nlen), *(char **)((*pf)->val->pval), (*pf)->val->pval);
            break;
        case IB_FTYPE_NUM:
            ib_util_log_debug(9, "CREATEN FIELD type=%d %" IB_BYTESTR_FMT "=%d (%p)", type, IB_BYTESTRSL_FMT_PARAM((*pf)->name,(*pf)->nlen), *(int **)((*pf)->val->pval), (*pf)->val->pval);
            break;
        case IB_FTYPE_UNUM:
            ib_util_log_debug(9, "CREATEN FIELD type=%d %" IB_BYTESTR_FMT "=%u (%p)", type, IB_BYTESTRSL_FMT_PARAM((*pf)->name,(*pf)->nlen), *(unsigned int **)((*pf)->val->pval), (*pf)->val->pval);
            break;
        case IB_FTYPE_GENERIC:
        default:
            break;
    }

    IB_FTRACE_RET_STATUS(IB_OK);

failed:
    /* Make sure everything is cleaned up on failure. */
    *pf = NULL;

    IB_FTRACE_RET_STATUS(rc);
}


ib_status_t ib_field_copy_ex(ib_field_t **pf,
                             ib_mpool_t *mp,
                             const char *name,
                             size_t nlen,
                             ib_field_t *src)
{
    IB_FTRACE_INIT(ib_field_copy_ex);
    void *val = ib_field_value(src);

    /// @todo Make this work for dynamic fields as well - copy the
    ///       generator functions not the value.
    ib_status_t rc = ib_field_create_ex(pf, mp, name, nlen, src->type, &val);
    IB_FTRACE_RET_STATUS(rc);

}

ib_status_t ib_field_alias_mem_ex(ib_field_t **pf,
                                  ib_mpool_t *mp,
                                  const char *name,
                                  size_t nlen,
                                  uint8_t *val,
                                  size_t vlen)
{
    IB_FTRACE_INIT(ib_field_alias_mem_ex);
    ib_status_t rc;
    char *name_copy;

    /* Allocate the field structure. */
    *pf = (ib_field_t *)ib_mpool_alloc(mp, sizeof(**pf));
    if (*pf == NULL) {
        rc = IB_EALLOC;
        goto failed;
    }
    (*pf)->mp = mp;
    (*pf)->type = IB_FTYPE_BYTESTR;

    /* Copy the name. */
    (*pf)->nlen = nlen;
    name_copy = (char *)ib_mpool_alloc(mp, nlen);
    if (name_copy == NULL) {
        rc = IB_EALLOC;
        goto failed;
    }
    memcpy(name_copy, name, nlen);
    (*pf)->name = (const char *)name_copy;

    /* Set the value as an aliased byte string. */
    (*pf)->val = ib_mpool_alloc(mp, sizeof(*((*pf)->val)));
    if ((*pf)->val == NULL) {
        rc = IB_EALLOC;
        goto failed;
    }
    rc = ib_bytestr_alias_mem(&(*pf)->val->u.bytestr, mp, val, vlen);
    if (rc != IB_OK) {
        goto failed;
    }
    (*pf)->val->pval = (void *)&((*pf)->val->u.bytestr);
    ib_util_log_debug(9, "ALIAS BYTESTR FIELD type=%d %" IB_BYTESTR_FMT "=\"%" IB_BYTESTR_FMT "\" (%p)", (*pf)->type, IB_BYTESTRSL_FMT_PARAM((*pf)->name,(*pf)->nlen), IB_BYTESTR_FMT_PARAM(*(ib_bytestr_t **)((*pf)->val->pval)), (*pf)->val->pval);

    IB_FTRACE_RET_STATUS(IB_OK);

failed:
    /* Make sure everything is cleaned up on failure. */
    *pf = NULL;

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_field_list_add(ib_field_t *f,
                              ib_field_t *fval)
{
    IB_FTRACE_INIT(ib_field_list_add);
    ib_status_t rc;

    if (f->type != IB_FTYPE_LIST) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    ib_util_log_debug(9, "Adding field %" IB_BYTESTR_FMT ":%" IB_BYTESTR_FMT "=\"%" IB_BYTESTR_FMT "\" (%p)", IB_BYTESTRSL_FMT_PARAM(f->name,f->nlen), IB_BYTESTRSL_FMT_PARAM(fval->name,fval->nlen), IB_BYTESTR_FMT_PARAM(*(ib_bytestr_t **)(fval->val->pval)), fval->val->pval);

    rc = ib_list_push(*(ib_list_t **)(f->val->pval), (void *)fval);
    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_field_setv(ib_field_t *f,
                          void *pval)
{
    IB_FTRACE_INIT(ib_field_setv);
    /* Set the value based on the field type. */
    switch (f->type) {
        case IB_FTYPE_BYTESTR:
            *(ib_bytestr_t **)(f->val->pval) = *(ib_bytestr_t **)pval;
            ib_util_log_debug(9, "SETV FIELD type=%d %" IB_BYTESTR_FMT "=\"%" IB_BYTESTR_FMT "\" (%p)", f->type, IB_BYTESTRSL_FMT_PARAM(f->name,f->nlen), IB_BYTESTR_FMT_PARAM(*(ib_bytestr_t **)(f->val->pval)), f->val->pval);
            break;
        case IB_FTYPE_LIST:
            *(ib_list_t **)(f->val->pval) = *(ib_list_t **)pval;
            break;
        case IB_FTYPE_NULSTR:
            *(char **)(f->val->pval) = *(char **)pval;
            ib_util_log_debug(9, "SETV FIELD type=%d %" IB_BYTESTR_FMT "=\"%s\" (%p)", f->type, IB_BYTESTRSL_FMT_PARAM(f->name,f->nlen), *(char **)(f->val->pval), f->val->pval);
            break;
        case IB_FTYPE_NUM:
            *(ib_num_t *)(f->val->pval) = *(ib_num_t *)pval;
            ib_util_log_debug(9, "SETV FIELD type=%d %" IB_BYTESTR_FMT "=%d (%p)", f->type, IB_BYTESTRSL_FMT_PARAM(f->name,f->nlen), *(int *)(f->val->pval), f->val->pval);
            break;
        case IB_FTYPE_UNUM:
            *(ib_unum_t *)(f->val->pval) = *(ib_unum_t *)pval;
            ib_util_log_debug(9, "SETV FIELD type=%d %" IB_BYTESTR_FMT "=%d (%p)", f->type, IB_BYTESTRSL_FMT_PARAM(f->name,f->nlen), *(unsigned int *)(f->val->pval), f->val->pval);
            break;
        case IB_FTYPE_GENERIC:
        default:
            /// @todo Perhaps unknown should be an error???
            *(void **)(f->val->pval) = *(void **)pval;
            break;
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

void *ib_field_value(ib_field_t *f)
{
    /* If there is not a stored value, then attempt to use the
     * fn_get call to retrieve the value.
     */
    if ((f->val->pval == NULL) && (f->val->fn_get != NULL)) {
        return f->val->fn_get(f, NULL, 0, f->val->fndata);
    }

    /* Non-pointer values are returned as pointers to those values. */
    switch (f->type) {
        case IB_FTYPE_NUM:
        case IB_FTYPE_UNUM:
            return (void *)f->val->pval;
    }

    return f->val->pval ? *(void **)f->val->pval : NULL;
}

void ib_field_dyn_set_data(ib_field_t *f,
                           void *data)
{
    IB_FTRACE_INIT(ib_field_dyn_set_data);
    f->val->fndata = data;
    IB_FTRACE_RET_VOID();
}

void ib_field_dyn_register_get(ib_field_t *f,
                               ib_field_get_fn_t fn_get)
{
    IB_FTRACE_INIT(ib_field_dyn_register_get);
    f->val->fn_get = fn_get;
    IB_FTRACE_RET_VOID();
}

#if 0
void ib_field_dyn_register_set(ib_field_t *f,
                               ib_field_set_fn_t fn_set)
{
    IB_FTRACE_INIT(ib_field_dyn_register_set);
    f->val->fn_set = fn_set;
    IB_FTRACE_RET_VOID();
}

void ib_field_dyn_register_rset(ib_field_t *f,
                                ib_field_rset_fn_t fn_rset)
{
    IB_FTRACE_INIT(ib_field_dyn_register_rset);
    f->val->fn_rset = fn_rset;
    IB_FTRACE_RET_VOID();
}
#endif
