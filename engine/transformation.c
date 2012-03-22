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
#include <assert.h>

#include <ironbee/engine.h>
#include <ironbee/mpool.h>
#include <ironbee/hash.h>
#include <ironbee/bytestr.h>
#include <ironbee/field.h>
#include <ironbee/transformation.h>

#include "ironbee_private.h"


/* -- Transformation Routines -- */

ib_status_t ib_tfn_register(ib_engine_t *ib,
                            const char *name,
                            ib_tfn_fn_t fn_execute,
                            void *fndata)
{
    IB_FTRACE_INIT();

    assert(ib != NULL);
    assert(name != NULL);
    assert(fn_execute != NULL);

    ib_hash_t *tfn_hash = ib->tfns;
    ib_status_t rc;
    ib_tfn_t *tfn;
    char *name_copy;

    name_copy = ib_mpool_strdup(ib->mp, name);
    if (name_copy == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    tfn = (ib_tfn_t *)ib_mpool_alloc(ib->mp, sizeof(*tfn));
    if (tfn == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }
    tfn->name = name_copy;
    tfn->fn_execute = fn_execute;
    tfn->fndata = fndata;

    rc = ib_hash_set(tfn_hash, name_copy, tfn);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t ib_tfn_lookup_ex(ib_engine_t *ib,
                             const char *name,
                             size_t nlen,
                             ib_tfn_t **ptfn)
{
    IB_FTRACE_INIT();

    assert(ib != NULL);
    assert(name != NULL);
    assert(ptfn != NULL);

    ib_hash_t *tfn_hash = ib->tfns;
    ib_status_t rc = ib_hash_get(tfn_hash, ptfn, name);
    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_tfn_transform(ib_engine_t *ib,
                             ib_mpool_t *mp,
                             ib_tfn_t *tfn,
                             ib_field_t *fin,
                             ib_field_t **fout,
                             ib_flags_t *pflags)
{
    IB_FTRACE_INIT();

    assert(tfn != NULL);
    assert(mp != NULL);
    assert(fin != NULL);
    assert(fout != NULL);
    assert(pflags != NULL);

    ib_status_t rc = tfn->fn_execute(ib, mp, tfn->fndata, fin, fout, pflags);

    IB_FTRACE_RET_STATUS(rc);
}
