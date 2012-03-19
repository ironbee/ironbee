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
    /* Set by ib_cfgmap_init() */
    (*pcm)->base = NULL;

    IB_FTRACE_RET_STATUS(IB_OK);

failed:
    /* Make sure everything is cleaned up on failure. */
    *pcm = NULL;

    IB_FTRACE_RET_STATUS(rc);
}

/**
 * Holds data for config map setters and getters.
 **/
struct ib_cfgmap_handlers_data
{
    const ib_cfgmap_init_t *init;
    void                   *base;
};
typedef struct ib_cfgmap_handlers_data ib_cfgmap_handlers_data;

/**
 * Translate field getter to cfgmap getter.
 * @internal
 *
 * @sa ib_field_get_fn_t.
 */
static const void *ib_cfgmap_handle_get(
    const ib_field_t *field,
    const void *arg,
    size_t alen,
    void *cbdata
)
{
    IB_FTRACE_INIT();

    if (arg != NULL) {
        IB_FTRACE_RET_PTR(void, NULL);
    }

    ib_cfgmap_handlers_data *data = (ib_cfgmap_handlers_data*)cbdata;

    IB_FTRACE_RET_PTR(
        void,
        data->init->fn_get(
            data->base,
            field,
            data->init->cbdata_get
        )
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
    const void *val,
    void *cbdata
)
{
    IB_FTRACE_INIT();

    if (arg != NULL) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    ib_cfgmap_handlers_data *data = (ib_cfgmap_handlers_data*)cbdata;

    IB_FTRACE_RET_STATUS(
        data->init->fn_set(
            data->base,
            field,
            val,
            data->init->cbdata_get
        )
    );
}

ib_status_t ib_cfgmap_init(ib_cfgmap_t *cm,
                           void *base,
                           const ib_cfgmap_init_t *init)
{
    IB_FTRACE_INIT();
    ib_cfgmap_init_t *rec = (ib_cfgmap_init_t *)init;
    ib_field_t *f;
    ib_status_t rc;

    /* Add tree entries that just point into the base structure. */
    ib_util_log_debug(9, "Initializing: t=%p init=%p", cm, init);
    cm->base = base;
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
            ib_cfgmap_handlers_data *data =
                (ib_cfgmap_handlers_data*)ib_mpool_alloc(
                    cm->mp,
                    sizeof(*data)
                );
            data->base = base;
            data->init = init;
            ib_field_dyn_register_get(f, ib_cfgmap_handle_get, data);
            ib_field_dyn_register_set(f, ib_cfgmap_handle_set, data);
        }
        else {
            if (rec->fn_get != NULL || rec->fn_set != NULL) {
                IB_FTRACE_RET_STATUS(IB_EINVAL);
            }

            void *val = (void *)(((uint8_t *)base) + rec->offset);

            ib_util_log_debug(9, "INIT: %s type=%d base=%p offset=%d dlen=%d %p",
                              rec->name, rec->type, base,
                              (int)rec->offset, (int)rec->dlen, val);

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
                          const void *pval)
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
    if (rc == IB_OK) {
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
    }

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_cfgmap_get(const ib_cfgmap_t *cm,
                          const char *name,
                          const void *pval, ib_ftype_t *ptype)
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
            *(const ib_bytestr_t **)pval = ib_field_value_bytestr(f);
            ib_util_log_debug(8, "GET FIELD type=%d %" IB_BYTESTR_FMT "=\"%" IB_BYTESTR_FMT "\" (%p)", f->type, IB_BYTESTRSL_FMT_PARAM(f->name,f->nlen), IB_BYTESTR_FMT_PARAM(*(ib_bytestr_t **)pval), *(void **)pval);
            break;
        case IB_FTYPE_LIST:
            *(const ib_list_t **)pval = ib_field_value_list(f);
            break;
        case IB_FTYPE_NULSTR:
            *(const char **)pval = ib_field_value_nulstr(f);
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
            *(const void **)pval = ib_field_value(f);
            break;
    }

    if (ptype != NULL) {
        *ptype = f->type;
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

