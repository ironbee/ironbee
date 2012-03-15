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
 * @internal
 * Simple ASCII lowercase function.
 *
 * @note For non-ASCII (utf8, etc) you should use case folding.
 */
static ib_status_t core_tfn_lowercase(void *fndata,
                                      ib_mpool_t *pool,
                                      uint8_t *data_in,
                                      size_t dlen_in,
                                      uint8_t **data_out,
                                      size_t *dlen_out,
                                      ib_flags_t *pflags)
{
    size_t i = 0;
    int modified = 0;

    /* This is an in-place transformation which does not change
     * the data length.
     */
    *data_out = data_in;
    *dlen_out = dlen_in;
    (*pflags) |= IB_TFN_FINPLACE;

    while(i < dlen_in) {
        int c = data_in[i];
        (*data_out)[i] = tolower(c);
        if (c != (*data_out)[i]) {
            modified++;
        }
        i++;
    }

    if (modified != 0) {
        (*pflags) |= IB_TFN_FMODIFIED;
    }

    return IB_OK;
}

/**
 * @internal
 * Simple ASCII trimLeft function.
 */
static ib_status_t core_tfn_trimleft(void *fndata,
                                     ib_mpool_t *pool,
                                     uint8_t *data_in,
                                     size_t dlen_in,
                                     uint8_t **data_out,
                                     size_t *dlen_out,
                                     ib_flags_t *pflags)
{
    size_t i = 0;

    /* This is an in-place transformation which may change
     * the data length.
     */
    (*pflags) |= IB_TFN_FINPLACE;

    while(i < dlen_in) {
        if (isspace(data_in[i]) == 0) {
            *data_out = data_in + i;
            *dlen_out = dlen_in - i;
            (*pflags) |= IB_TFN_FMODIFIED;
            return IB_OK;
        }
        i++;
    }
    *dlen_out = 0;
    *data_out = data_in;

    return IB_OK;
}

/**
 * @internal
 * Simple ASCII trimRight function.
 */
static ib_status_t core_tfn_trimright(void *fndata,
                                      ib_mpool_t *pool,
                                      uint8_t *data_in,
                                      size_t dlen_in,
                                      uint8_t **data_out,
                                      size_t *dlen_out,
                                      ib_flags_t *pflags)
{
    size_t i = dlen_in - 1;

    /* This is an in-place transformation which may change
     * the data length.
     */
    *data_out = data_in;
    (*pflags) |= IB_TFN_FINPLACE;

    while(i > 0) {
        if (isspace(data_in[i]) == 0) {
            (*pflags) |= IB_TFN_FMODIFIED;
            (*data_out)[*dlen_out] = '\0';
            *dlen_out = i + 1;
            return IB_OK;
        }
        i--;
    }
    *dlen_out = 0;

    return IB_OK;
}

/**
 * @internal
 * Simple ASCII trim function.
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
