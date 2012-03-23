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
 * @brief IronBee - Transformations
 *
 * @author Brian Rectanus <brectanus@qualys.com>
 */

#include "ironbee_config_auto.h"

#include <string.h>

#include <ironbee/engine.h>
#include <ironbee/mpool.h>
#include <ironbee/hash.h>
#include <ironbee/bytestr.h>
#include <ironbee/field.h>

#include "ironbee_private.h"

#include <assert.h>

/* -- Transformation Routines -- */

ib_status_t ib_tfn_create(ib_engine_t *ib,
                          const char *name,
                          ib_tfn_fn_t transform,
                          void *fndata,
                          ib_tfn_t **ptfn)
{
    IB_FTRACE_INIT();
    ib_status_t rc;
    ib_tfn_t *tfn;
    char *name_copy;
    size_t name_len = strlen(name) + 1;

    name_copy = (char *)ib_mpool_alloc(ib->mp, name_len);
    if (name_copy == NULL) {
        if (ptfn != NULL) {
            *ptfn = NULL;
        }
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }
    memcpy(name_copy, name, name_len);

    tfn = (ib_tfn_t *)ib_mpool_alloc(ib->mp, sizeof(*tfn));
    if (tfn == NULL) {
        if (ptfn != NULL) {
            *ptfn = NULL;
        }
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }
    tfn->name = name_copy;
    tfn->transform = transform;
    tfn->fndata = fndata;

    rc = ib_hash_set(ib->tfns, name_copy, tfn);
    if (rc != IB_OK) {
        if (ptfn != NULL) {
            *ptfn = NULL;
        }
        IB_FTRACE_RET_STATUS(rc);
    }

    if (ptfn != NULL) {
        *ptfn = tfn;
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t ib_tfn_lookup_ex(ib_engine_t *ib,
                             const char *name,
                             size_t nlen,
                             ib_tfn_t **ptfn)
{
    IB_FTRACE_INIT();
    ib_status_t rc = ib_hash_get_ex(ib->tfns, ptfn, (void *)name, nlen);
    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_tfn_transform(ib_tfn_t *tfn,
                             ib_mpool_t *pool,
                             uint8_t *data_in,
                             size_t dlen_in,
                             uint8_t **data_out,
                             size_t *dlen_out,
                             ib_flags_t *pflags)
{
    IB_FTRACE_INIT();
    ib_status_t rc = tfn->transform(tfn->fndata, pool, data_in, dlen_in, data_out, dlen_out, pflags);
    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_tfn_transform_field(ib_tfn_t *tfn,
                                   ib_field_t *f,
                                   ib_flags_t *pflags)
{
    IB_FTRACE_INIT();
    ib_bytestr_t *bs;
    char *str;
    uint8_t *data_out;
    size_t dlen_out;
    ib_status_t rc;

    switch(f->type) {
        case IB_FTYPE_BYTESTR:
            /* Cast away const to support FINPLACE.  This will do bad things
               with dynamic fields. */
            bs = (ib_bytestr_t *)ib_field_value_bytestr(f);

            rc = tfn->transform(tfn->fndata,
                                f->mp,
                                ib_bytestr_ptr(bs),
                                ib_bytestr_length(bs),
                                &data_out,
                                &dlen_out,
                                pflags);

            /* If it is modified and not done in place, then the
             * field value needs to be updated.
             */
            if (   IB_TFN_CHECK_FMODIFIED(*pflags)
                && !IB_TFN_CHECK_FINPLACE(*pflags))
            {
                ib_bytestr_t *bs_new;

                rc = ib_bytestr_alias_mem(&bs_new, f->mp, data_out, dlen_out);
                if (rc != IB_OK) {
                    IB_FTRACE_RET_STATUS(rc);
                }

                rc = ib_field_setv(f, bs_new);
            }

            IB_FTRACE_RET_STATUS(rc);

        case IB_FTYPE_NULSTR:
            /* Cast away const to support FINPLACE.  This will do bad things
               with dynamic fields. */
            str = (char *)ib_field_value_nulstr(f),

            rc = tfn->transform(tfn->fndata,
                                f->mp,
                                (uint8_t *)str,
                                strlen(str),
                                &data_out,
                                &dlen_out,
                                pflags);

            /* If it is modified and not done in place, then the
             * field value needs to be updated.
             *
             * NOTE: Anytime a transformation modifies data it
             *       MUST NUL terminate the data and it is a bug
             *       if this is not done.
             */
            if (   IB_TFN_CHECK_FMODIFIED(*pflags)
                && !IB_TFN_CHECK_FINPLACE(*pflags))
            {
                rc = ib_field_setv(f, data_out);
            }

            IB_FTRACE_RET_STATUS(rc);

        default:
            IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    assert(! "unreachable");
}

