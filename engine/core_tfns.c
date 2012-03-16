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
 * @brief IronBee
 *
 * @author Nick LeRoy <nleroy@qualys.com>
 */

#include "ironbee_config_auto.h"

#include <assert.h>
#include <ctype.h>

#include <ironbee/engine.h>
#include <ironbee/mpool.h>
#include <ironbee/bytestr.h>
#include <ironbee/radix.h>
#include <ironbee/operator.h>
#include <ironbee/field.h>
#include <ironbee/string.h>

#include "ironbee_private.h"


/* -- Transformations -- */

/**
 * Simple ASCII lowercase function.
 * @internal
 *
 * @note For non-ASCII (utf8, etc) you should use case folding.
 */
static ib_status_t tfn_lowercase(void *fndata,
                                 ib_mpool_t *mp,
                                 ib_field_t *fin,
                                 ib_field_t **fout
                                 ib_flags_t *pflags)
{
    IB_FTRACE_INIT();
    ib_status_t rc;
    ib_bool_t modified = IB_FALSE;

    /* We only handle bytestr and nulstr non-dynamic fields */
    if (ib_field_is_dynamic(fin)) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    if (fin->type == IB_FTYPE_NULSTR) {
        char *p = ib_field_value_nulstr(fin);
        assert (p != NULL);
        *fout = fin;
        rc = ib_strlower(p, strlen(p), &modified);
    }
    else if (fin->type == IB_FTYPE_BYTESTR) {
        ib_bytestr_t *bs = ib_field_value_bytestr(fin);
        assert (bs != NULL);
        *fout = fin;
        rc = lowercase_data(ib_bytestr_ptr(bs),
                            ib_bytestr_length(bs),
                            &modified);
    }
    else {
        rc = IB_EINVAL;
    }

    if (modified) {
        (*pflags) |= IB_TFN_FMODIFIED;
    }
    if (rc == IB_OK) {
        (*pflags) |= IB_TFN_FINPLACE;
    }
    IB_FTRACE_RET_STATUS(rc);
}

/**
 * Simple ASCII trimLeft function.
 * @internal
 */
static ib_status_t tfn_trimleft(void *fndata,
                                ib_mpool_t *mp,
                                ib_field_t *fin,
                                ib_field_t **fout,
                                ib_flags_t *pflags)
{
    IB_FTRACE_INIT();
    ib_status_t rc;
    ib_bool_t modified = IB_FALSE;

    /* We only handle bytestr and nulstr non-dynamic fields */
    if (ib_field_is_dynamic(fin)) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    if (fin->type == IB_FTYPE_NULSTR) {
        char *p = ib_field_value_nulstr(fin);
        char *out;
        size_t outlen;
        assert (p != NULL);

        rc = ib_strtrim_left_data(p, strlen(p), &out, &outlen, &modified);
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }
        rc = ib_field_alias_mem_ex(
            fout, mp, fin->name, fin->nlen, IB_FTYPE_NULSTR, out, outlen);
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }
    }
    else if (fin->type == IB_FTYPE_BYTESTR) {
        ib_bytestr_t *bs = ib_field_value_bytestr(fin);
        assert (bs != NULL);

        rc = ib_strtrim_left(ib_bytestr_ptr(bs),
                             ib_bytestr_length(bs),
                             &out,
                             &outlen,
                             &modified);
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }
        rc = ib_bytestr_alias_mem(&bs, mp, out, outlen);
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }
        rc = ib_field_createn_ex(
            fout, mp, fin->name, fin->nlen, IB_FTYPE_BYTESTR, &bs);
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }

    }
    else {
        rc = IB_EINVAL;
    }

    if (modified) {
        (*pflags) |= IB_TFN_FMODIFIED;
    }
    if (rc == IB_OK) {
        (*pflags) |= IB_TFN_FINPLACE;
    }
    IB_FTRACE_RET_STATUS(rc);
}

/**
 * Simple ASCII trimRight function.
 * @internal
 */
static ib_status_t core_tfn_trimright(void *fndata,
                                      ib_mpool_t *mp,
                                      ib_field_t *fin,
                                      ib_field_t **fout,
                                      ib_flags_t *pflags)
{
    IB_FTRACE_INIT();
    ib_status_t rc;

    /* We only handle bytestr and nulstr non-dynamic fields */
    if (ib_field_is_dynamic(fin)) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    if (fin->type == IB_FTYPE_NULSTR) {
        char *p = ib_field_value_nulstr(fin);
        char *out;
        size_t outlen;
        assert (p != NULL);

        rc = trimright_data(p, strlen(p), &out, &outlen, pflags);
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }
        rc = ib_field_alias_mem_ex(
            fout, mp, fin->name, fin->nlen, IB_FTYPE_NULSTR, out, outlen);
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }
    }
    else if (fin->type == IB_FTYPE_BYTESTR) {
        ib_bytestr_t *bs = ib_field_value_bytestr(fin);
        assert (bs != NULL);

        rc = trimright_data(ib_bytestr_ptr(bs),
                            ib_bytestr_length(bs),
                            &out,
                            &outlen,
                            pflags);
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }
        rc = ib_bytestr_alias_mem(&bs, mp, out, outlen);
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }
        rc = ib_field_createn_ex(
            fout, mp, fin->name, fin->nlen, IB_FTYPE_BYTESTR, &bs);
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }

    }
    else {
        rc = IB_EINVAL;
    }

    IB_FTRACE_RET_STATUS(rc);
}

/**
 * Simple ASCII trim function.
 * @internal
 */
static ib_status_t core_tfn_trim(void *fndata,
                                 ib_mpool_t *pool,
                                 uint8_t *data_in,
                                 size_t dlen_in,
                                 uint8_t **data_out,
                                 size_t *dlen_out,
                                 ib_flags_t *pflags)
{
    ib_status_t rc;

    /* Just call the other trim functions. */
    rc = core_tfn_trimleft(fndata, pool, data_in, dlen_in, data_out, dlen_out,
                           pflags);
    if (rc != IB_OK) {
        return rc;
    }
    rc = core_tfn_trimleft(
        fndata, pool, *data_out, *dlen_out, data_out, dlen_out, pflags);
    return rc;
}


/**
 * Initialize the core transformations
 **/
ib_status_t ib_core_transformations_init(ib_engine_t *ib, ib_module_t *mod)
{
    IB_FTRACE_INIT();
    ib_status_t rc;

    /* Define transformations. */
    rc = ib_tfn_create(ib, "lowercase", core_tfn_lowercase, NULL, NULL);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = ib_tfn_create(ib, "trimLeft", core_tfn_trimleft, NULL, NULL);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = ib_tfn_create(ib, "trimRight", core_tfn_trimright, NULL, NULL);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = ib_tfn_create(ib, "trim", core_tfn_trim, NULL, NULL);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}
