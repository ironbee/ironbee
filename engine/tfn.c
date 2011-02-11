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

#include <ironbee/ironbee.h>
#include <ironbee/util.h>

#include "ironbee_private.h"


/* -- Transformation Routines -- */

ib_status_t ib_tfn_create(ib_engine_t *ib,
                          const char *name,
                          ib_tfn_fn_t transform,
                          void *fndata,
                          ib_tfn_t **ptfn)
{
    IB_FTRACE_INIT(ib_tfn_create);
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

ib_status_t ib_tfn_lookup(ib_engine_t *ib,
                          const char *name,
                          ib_tfn_t **ptfn)
{
    IB_FTRACE_INIT(ib_tfn_lookup);
    ib_status_t rc = ib_hash_get(ib->tfns, name, (void *)ptfn);
    IB_FTRACE_RET_STATUS(rc);
}

/// @todo Should also have a version that handles transforming a data field???
ib_status_t ib_tfn_transform(ib_tfn_t *tfn,
                             uint8_t *data_in,
                             size_t dlen_in,
                             uint8_t **data_out,
                             size_t *dlen_out,
                             ib_flags_t *pflags)
{
    IB_FTRACE_INIT(ib_tfn_transform);
    ib_status_t rc = tfn->transform(tfn->fndata, data_in, dlen_in, data_out, dlen_out, pflags);
    IB_FTRACE_RET_STATUS(rc);
}

