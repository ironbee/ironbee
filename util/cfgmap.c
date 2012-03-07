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
 * @brief IronBee - Utility Configuration Tree Functions
 * @author Brian Rectanus <brectanus@qualys.com>
 */

#include "ironbee_config_auto.h"

#include <ironbee/cfgmap.h>

#include <ironbee/debug.h>
#include <ironbee/util.h>
#include <ironbee/bytestr.h>

#include "ironbee_util_private.h"

ib_status_t ib_cfgmap_create(ib_cfgmap_t **pcm,
                             ib_mpool_t *pool)
{
    IB_FTRACE_INIT();
    ib_hash_t *hash;
    ib_status_t rc;

    /* Underlying hash structure. */
    rc = ib_hash_create_nocase(&hash, pool);
    if (rc != IB_OK) {
        rc = IB_EALLOC;
        goto failed;
    }
    pool = ib_hash_pool(hash);

    *pcm = (ib_cfgmap_t *)ib_mpool_alloc(pool, sizeof(**pcm));
    if (*pcm == NULL) {
        rc = IB_EALLOC;
        goto failed;
    }

    (*pcm)->mp = pool;
    (*pcm)->hash = hash;

    IB_FTRACE_RET_STATUS(IB_OK);

failed:
    /* Make sure everything is cleaned up on failure. */
    *pcm = NULL;

    IB_FTRACE_RET_STATUS(rc);
}

/**
 * Translate field getter to cfgmap getter.
 * @internal
 *
 * @sa ib_field_get_fn_t.
 */
static void *ib_cfgmap_handle_get(
    ib_field_t *f,
    const void *arg,
    size_t alen,
    void *data
)
{
    IB_FTRACE_INIT();

    if (arg != NULL) {
        IB_FTRACE_RET_PTR(void, NULL);
    }

    ib_cfgmap_init_t *rec = (ib_cfgmap_init_t*)data;

    IB_FTRACE_RET_PTR(
        void,
        rec->fn_get(rec->name, rec->type, rec->cbdata_get)
    );
}

/**
 * Translate field setter to cfgmap setter.
 * @internal
 *
 * @sa ib_field_set_fn_t.
 */
static ib_status_t ib_cfgmap_handle_set(
    ib_field_t *field,
    const void *arg, size_t alen,
    void *val,
    void *data
)
{
    IB_FTRACE_INIT();

    if (arg != NULL) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    ib_cfgmap_init_t *rec = (ib_cfgmap_init_t*)data;

    IB_FTRACE_RET_STATUS(
        rec->fn_set(rec->name, rec->type, val, rec->cbdata_get)
    );
}

ib_status_t ib_cfgmap_init(ib_cfgmap_t *cm,
                           const void *base,
                           const ib_cfgmap_init_t *init,
                           int usedefaults)
{
    IB_FTRACE_INIT();
    ib_cfgmap_init_t *rec = (ib_cfgmap_init_t *)init;
    ib_field_t *f;
    ib_status_t rc;

    /* Add tree entries that just point into the base structure. */
    ib_util_log_debug(9, "Initializing: t=%p init=%p", cm, init);
    while (rec->name != NULL) {
        if (rec->fn_set != NULL && rec->fn_get != NULL) {
            ib_util_log_debug(9, "INIT: %s type=%d set=%p/%p get=%p/%p",
                              rec->name, rec->type,
                              rec->fn_set, rec->cbdata_set,
                              rec->fn_get, rec->cbdata_get);

            /* Create a field with data that points to the base+offset. */
            rc = ib_field_createn(&f, cm->mp, rec->name, rec->type, NULL);
            if (rc != IB_OK) {
                IB_FTRACE_RET_STATUS(rc);
            }
            ib_field_dyn_register_get(f, ib_cfgmap_handle_get, rec);
            ib_field_dyn_register_set(f, ib_cfgmap_handle_set, rec);
        }
        else {
            if (rec->fn_get != NULL || rec->fn_set != NULL) {
                IB_FTRACE_RET_STATUS(IB_EINVAL);
            }

            void *val = (void *)(((uint8_t *)base) + rec->offset);

            ib_util_log_debug(9, "INIT: %s type=%d base=%p offset=%d dlen=%d %p",
                              rec->name, rec->type, base,
                              (int)rec->offset, (int)rec->dlen, val);

            /* Copy the default value if required. */
            if (usedefaults) {
                memcpy(val, &rec->defval, rec->dlen);
            }

            /* Create a field with data that points to the base+offset. */
            rc = ib_field_createn(&f, cm->mp, rec->name, rec->type, val);
            if (rc != IB_OK) {
                IB_FTRACE_RET_STATUS(rc);
            }
        }

        /* Add the field. */
        rc = ib_hash_set(cm->hash, rec->name, (void *)f);
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }

        ++rec;
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t ib_cfgmap_set(ib_cfgmap_t *cm,
                          const char *name,
                          void *pval)
{
    IB_FTRACE_INIT();
    ib_field_t *f;
    ib_status_t rc;

    rc = ib_hash_get(cm->hash, &f, name);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = ib_field_setv(f, pval);

    /// @todo Remove this extra debugging
    switch (f->type) {
        case IB_FTYPE_BYTESTR:
            ib_util_log_debug(8, "SET FIELD type=%d %" IB_BYTESTR_FMT "=\"%" IB_BYTESTR_FMT "\" (%p)", f->type, IB_BYTESTRSL_FMT_PARAM(f->name,f->nlen), IB_BYTESTR_FMT_PARAM(*(ib_bytestr_t **)pval), *(void **)pval);
            break;
        case IB_FTYPE_NULSTR:
            ib_util_log_debug(8, "SET FIELD type=%d %" IB_BYTESTR_FMT "=\"%s\" (%p)", f->type, IB_BYTESTRSL_FMT_PARAM(f->name,f->nlen), *(char **)pval, *(void **)pval);
            break;
        case IB_FTYPE_NUM:
            ib_util_log_debug(8, "SET FIELD type=%d %" IB_BYTESTR_FMT "=%d (%p)", f->type, IB_BYTESTRSL_FMT_PARAM(f->name,f->nlen), *(int *)pval, *(void **)pval);
            break;
        case IB_FTYPE_UNUM:
            ib_util_log_debug(8, "SET FIELD type=%d %" IB_BYTESTR_FMT "=%d (%p)", f->type, IB_BYTESTRSL_FMT_PARAM(f->name,f->nlen), *(unsigned int *)pval, *(void **)pval);
            break;
    }

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_cfgmap_get(ib_cfgmap_t *cm,
                          const char *name,
                          void *pval, ib_ftype_t *ptype)
{
    IB_FTRACE_INIT();
    ib_field_t *f;
    ib_status_t rc;

    rc = ib_hash_get(cm->hash, &f, name);
    if (rc != IB_OK) {
        if (ptype != NULL) {
            *ptype = IB_FTYPE_GENERIC;
        }
        IB_FTRACE_RET_STATUS(rc);
    }

    switch (f->type) {
        case IB_FTYPE_BYTESTR:
            *(ib_bytestr_t **)pval = ib_field_value_bytestr(f);
            ib_util_log_debug(8, "GET FIELD type=%d %" IB_BYTESTR_FMT "=\"%" IB_BYTESTR_FMT "\" (%p)", f->type, IB_BYTESTRSL_FMT_PARAM(f->name,f->nlen), IB_BYTESTR_FMT_PARAM(*(ib_bytestr_t **)pval), *(void **)pval);
            break;
        case IB_FTYPE_LIST:
            *(ib_list_t **)pval = ib_field_value_list(f);
            break;
        case IB_FTYPE_NULSTR:
            *(char **)pval = ib_field_value_nulstr(f);
            ib_util_log_debug(8, "GET FIELD type=%d %" IB_BYTESTR_FMT "=\"%s\" (%p)", f->type, IB_BYTESTRSL_FMT_PARAM(f->name,f->nlen), *(char **)pval, *(void **)pval);
            break;
        case IB_FTYPE_NUM:
            *(ib_num_t *)pval = *(ib_field_value_num(f));
            ib_util_log_debug(8, "GET FIELD type=%d %" IB_BYTESTR_FMT "=%d (%p)", f->type, IB_BYTESTRSL_FMT_PARAM(f->name,f->nlen), *(int *)pval, *(void **)pval);
            break;
        case IB_FTYPE_UNUM:
            *(ib_unum_t *)pval = *(ib_field_value_unum(f));
            ib_util_log_debug(8, "GET FIELD type=%d %" IB_BYTESTR_FMT "=%d (%p)", f->type, IB_BYTESTRSL_FMT_PARAM(f->name,f->nlen), *(unsigned int *)pval, *(void **)pval);
            break;
        case IB_FTYPE_GENERIC:
        default:
            *(void **)pval = ib_field_value(f);
            break;
    }

    if (ptype != NULL) {
        *ptype = f->type;
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

