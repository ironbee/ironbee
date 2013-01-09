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
 * @brief IronBee --- Transformations
 *
 * @author Brian Rectanus <brectanus@qualys.com>
 */

#include "ironbee_config_auto.h"

#include <ironbee/transformation.h>

#include "engine_private.h"

#include <ironbee/bytestr.h>
#include <ironbee/engine.h>
#include <ironbee/field.h>
#include <ironbee/hash.h>
#include <ironbee/mpool.h>

#include <assert.h>
#include <string.h>

/* -- Transformation Routines -- */

ib_status_t ib_tfn_register(ib_engine_t *ib,
                            const char *name,
                            ib_tfn_fn_t fn_execute,
                            ib_flags_t flags,
                            void *fndata)
{
    assert(ib != NULL);
    assert(name != NULL);
    assert(fn_execute != NULL);

    ib_hash_t *tfn_hash = ib->tfns;
    ib_status_t rc;
    ib_tfn_t *tfn;
    char *name_copy;

    name_copy = ib_mpool_strdup(ib->mp, name);
    if (name_copy == NULL) {
        return IB_EALLOC;
    }

    tfn = (ib_tfn_t *)ib_mpool_alloc(ib->mp, sizeof(*tfn));
    if (tfn == NULL) {
        return IB_EALLOC;
    }
    tfn->name = name_copy;
    tfn->fn_execute = fn_execute;
    tfn->tfn_flags = flags;
    tfn->fndata = fndata;

    rc = ib_hash_set(tfn_hash, name_copy, tfn);
    if (rc != IB_OK) {
        return rc;
    }

    return IB_OK;
}

ib_status_t ib_tfn_lookup_ex(ib_engine_t *ib,
                             const char *name,
                             size_t nlen,
                             ib_tfn_t **ptfn)
{
    assert(ib != NULL);
    assert(name != NULL);
    assert(ptfn != NULL);

    ib_hash_t *tfn_hash = ib->tfns;
    ib_status_t rc = ib_hash_get(tfn_hash, ptfn, name);
    return rc;
}

ib_status_t ib_tfn_transform(ib_engine_t *ib,
                             ib_mpool_t *mp,
                             const ib_tfn_t *tfn,
                             const ib_field_t *fin,
                             ib_field_t **fout,
                             ib_flags_t *pflags)
{
    assert(tfn != NULL);
    assert(mp != NULL);
    assert(fin != NULL);
    assert(fout != NULL);
    assert(pflags != NULL);

    ib_status_t rc = tfn->fn_execute(ib, mp, tfn->fndata, fin, fout, pflags);

    return rc;
}

ib_status_t ib_tfn_data_get_ex(
    ib_engine_t *ib,
    ib_data_t   *data,
    const char  *name,
    size_t       nlen,
    ib_field_t **pf,
    const char  *tfn
)
{
    assert(data != NULL);

    char *fullname;
    size_t fnlen;
    size_t tlen;
    ib_status_t rc;

    /* No tfn just means a normal get. */
    if (tfn == NULL) {
        rc = ib_data_get_ex(data, name, nlen, pf);
        return rc;
    }

    /* Build the full name with tfn: "name.t(tfn)" */
    tlen = strlen(tfn);
    fnlen = nlen + tlen + 4; /* Additional ".t()" bytes */
    fullname = (char *)ib_mpool_alloc(ib_data_pool(data), fnlen);
    memcpy(fullname, name, nlen);
    memcpy(fullname + nlen, ".t(", fnlen - nlen);
    memcpy(fullname + nlen + 3, tfn, fnlen - nlen - 3);
    fullname[fnlen - 1] = ')';

    /* See if there is already a transformed version, otherwise
     * one needs to be created.
     */
    rc = ib_data_get_ex(data, fullname, fnlen, pf);
    if (rc == IB_ENOENT) {
        const char *tname;
        size_t i;

        /* Get the non-tfn field. */
        rc = ib_data_get_ex(data, name, nlen, pf);
        if (rc != IB_OK) {
            return rc;
        }

        /* Currently this only works for string type fields. */
        if (   ((*pf)->type != IB_FTYPE_NULSTR)
            && ((*pf)->type != IB_FTYPE_BYTESTR))
        {
            return IB_EINVAL;
        }


        /* Copy the field, noting the tfn. */
        rc = ib_field_copy(pf, ib_data_pool(data), fullname, fnlen, *pf);
        if (rc != IB_OK) {
            return rc;
        }
        (*pf)->tfn = (char *)ib_mpool_memdup(ib_data_pool(data), tfn, tlen + 1);


        /* Transform. */
        tname = tfn;
        for (i = 0; i <= tlen; ++i) {
            ib_tfn_t *t;
            ib_flags_t flags;

            if ((tfn[i] == ',') || (i == tlen)) {
                size_t len = (tfn + i) - tname;

                rc = ib_tfn_lookup_ex(ib, tname, len, &t);
                if (rc == IB_OK) {
                    rc = ib_tfn_transform(ib, ib_data_pool(data), t, *pf, pf, &flags);
                    if (rc != IB_OK) {
                        /// @todo What to do here?  Fail or ignore?
                    }
                }
                else {
                    /// @todo What to do here?  Fail or ignore?
                }
                tname = tfn + i + 1;

            }
        }

        /* Store the transformed field. */
        rc = ib_data_set(data, *pf, name, nlen);
        if (rc != IB_OK) {
            return rc;
        }
    }

    return rc;
}

ib_status_t ib_tfn_data_get(
    ib_engine_t *ib,
    ib_data_t   *data,
    const char  *name,
    ib_field_t **pf,
    const char  *tfn
)
{
    return ib_tfn_data_get_ex(ib, data, name, strlen(name), pf, tfn);
}
