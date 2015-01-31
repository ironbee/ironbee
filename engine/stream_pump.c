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
 * @brief IronBee --- Stream Pump Implementation
 *
 * @author Sam Baskinger <sbaskinger@qualys.com>
 */

#include "ironbee_config_auto.h"

#include <ironbee/bytestr.h>
#include <ironbee/lock.h>
#include <ironbee/list.h>
#include <ironbee/hash.h>
#include <ironbee/mpool_freeable.h>
#include <ironbee/stream_pump.h>
#include <ironbee/mpool_lite.h>
#include <ironbee/mm_mpool_lite.h>

#include <assert.h>

struct ib_stream_pump_inst_t {
    //! Basic allocations.
    ib_mm_t mm;

    //! The memory pool to create data segments out of.
    ib_mpool_freeable_t *mp;

    //! The stream pump that created this instance.
    ib_stream_pump_t *stream_pump;

    //! Initial filter instances. Processing starts here.
    ib_list_t *filter_insts;
};

/**
 * How is data evaluated. This is a processing plan.
 *
 * @param[in] pump The pump.
 * @param[in] mm_eval A memory manager with a lifetime of ONLY this evaluation
 *            of the pump. When a function using this memory manager
 *            returns, the allocated data should be considered free'ed.
 * @param[in] filter_insts A list of @ref ib_filter_inst_t
 *            to process the data through.
 * @param[in] data A list of @ref ib_filter_data_t to process.
 * @param[in] type The type of data.
 * @param[in] cbdata.
 *
 * @returns
 * - IB_OK On success.
 * - IB_DECLINE If the type is not handled, or some other non-fatal error.
 *              Processing down the pipeline does not continue if this is
 *              returned and it is not reported as an error.
 * - Other on an error that stops processing and is reported as an error.
 */
typedef ib_status_t (*stream_pump_eval_fn)(
    const ib_stream_pump_t *pump,
    ib_mm_t                 mm_eval,
    const ib_list_t        *filter_insts,
    const ib_list_t        *data,
    void                   *cbdata
);

struct ib_stream_pump_t {
    //! Memory pool used for engine-lifetime allocations.
    ib_mm_t mm;

    //! Hash of types to @ref ib_list_t s of @ref ib_filter_t s.
    ib_hash_t *filter_by_type;

    //! Hash of names to @ref ib_filter_t s.
    ib_hash_t *filter_by_name;
};

/**
 * Destroy an @ref ib_stream_pump_t when its mm is destroyed.
 *
 * @param[out] cbdata The @ref ib_stream_pump_t.
 */
static void stream_pump_inst_destroy(void *cbdata)
{
    assert(cbdata != NULL);

    ib_stream_pump_inst_t *inst = (ib_stream_pump_inst_t *)cbdata;

    assert(inst->mp != NULL);

    ib_mpool_freeable_destroy(inst->mp);
}


ib_status_t ib_stream_pump_create(
    ib_stream_pump_t **pump,
    ib_mm_t            mm
)
{
    assert(pump != NULL);

    ib_stream_pump_t *tmp_pump;
    ib_status_t       rc;

    tmp_pump = ib_mm_alloc(mm, sizeof(*tmp_pump));
    if (tmp_pump == NULL) {
        return IB_EALLOC;
    }

    rc = ib_hash_create_nocase(&tmp_pump->filter_by_type, mm);
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_hash_create_nocase(&tmp_pump->filter_by_name, mm);
    if (rc != IB_OK) {
        return rc;
    }

    tmp_pump->mm = mm;

    *pump = tmp_pump;
    return IB_OK;
}

ib_status_t ib_stream_pump_inst_create(
    ib_stream_pump_inst_t **stream_pump_inst,
    ib_stream_pump_t       *stream_pump,
    ib_mm_t                 mm
)
{
    assert(stream_pump_inst != NULL);
    assert(stream_pump != NULL);

    ib_stream_pump_inst_t *inst;
    ib_status_t            rc;

    inst = ib_mm_alloc(mm, sizeof(*inst));
    if (inst == NULL) {
        return IB_EALLOC;
    }

    rc = ib_mpool_freeable_create(&inst->mp);
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_mm_register_cleanup(mm, stream_pump_inst_destroy, inst);
    if (rc != IB_OK) {
        ib_mpool_freeable_destroy(inst->mp);
        return rc;
    }

    rc = ib_list_create(&inst->filter_insts, mm);
    if (rc != IB_OK) {
        return rc;
    }

    inst->stream_pump = stream_pump;
    inst->mm          = mm;

    *stream_pump_inst = inst;
    return IB_OK;

}

ib_status_t ib_stream_pump_inst_add(
    ib_stream_pump_inst_t *pump,
    ib_filter_inst_t      *filter
)
{
    assert(pump != NULL);
    assert(pump->filter_insts != NULL);
    assert(filter != NULL);

    return ib_list_push(pump->filter_insts, filter);
}

ib_status_t ib_stream_pump_inst_process(
    ib_stream_pump_inst_t *pump,
    const uint8_t         *data,
    size_t                 data_len
)
{
    assert(pump != NULL);

    ib_status_t            rc;
    ib_mpool_lite_t       *mp_eval = NULL;
    ib_list_t             *args;
    ib_filter_data_t      *arg = NULL;

    if (data == NULL || data_len == 0) {
        return IB_OK;
    }

    rc = ib_mpool_lite_create(&mp_eval);
    if (rc != IB_OK) {
        // FIXME - log this
        return rc;
    }

    rc = ib_filter_data_cpy(&arg, pump->mp, data, data_len);
    if (rc != IB_OK) {
        // FIXME - log this
        goto exit_label;
    }

    rc = ib_list_create(&args, ib_mm_mpool_lite(mp_eval));
    if (rc != IB_OK) {
        // FIXME - log this
        goto exit_label;
    }

    rc = ib_list_push(args, arg);
    if (rc != IB_OK) {
        // FIXME - log this
        goto exit_label;
    }

    rc = ib_filter_insts_process(
        pump->filter_insts,
        pump->mp,
        ib_mm_mpool_lite(mp_eval),
        args
    );

exit_label:
    if (arg != NULL) {
        ib_filter_data_destroy(arg, pump->mp);
    }

    if (mp_eval != NULL) {
        ib_mpool_lite_destroy(mp_eval);
    }

    return rc;
}

ib_status_t ib_stream_pump_inst_flush(
    ib_stream_pump_inst_t *pump_inst
)
{
    assert(pump_inst != NULL);

    ib_status_t       rc;
    ib_mpool_lite_t  *mp_eval = NULL;
    ib_list_t        *args;
    ib_filter_data_t *arg = NULL;

    rc = ib_mpool_lite_create(&mp_eval);
    if (rc != IB_OK) {
        // FIXME - log this
        return rc;
    }

    rc = ib_filter_data_flush_create(&arg, pump_inst->mp);
    if (rc != IB_OK) {
        // FIXME - log this
        goto exit_label;
    }

    rc = ib_list_create(&args, ib_mm_mpool_lite(mp_eval));
    if (rc != IB_OK) {
        // FIXME - log this
        goto exit_label;
    }

    rc = ib_list_push(args, arg);
    if (rc != IB_OK) {
        // FIXME - log this
        goto exit_label;
    }

    rc = ib_filter_insts_process(
        pump_inst->filter_insts,
        pump_inst->mp,
        ib_mm_mpool_lite(mp_eval),
        args
    );

exit_label:
    if (arg != NULL) {
        ib_filter_data_destroy(arg, pump_inst->mp);
    }

    if (mp_eval != NULL) {
        ib_mpool_lite_destroy(mp_eval);
    }

    return rc;
}

ib_status_t ib_stream_pump_add(
    ib_stream_pump_t        *pump,
    ib_filter_t             *filter
)
{
    assert(pump != NULL);
    assert(filter != NULL);

    ib_status_t  rc;
    ib_list_t   *filters;
    const char  *name = ib_filter_name(filter);
    const char  *type = ib_filter_type(filter);

    /* Ensure that we do not clobber an already defined name. */
    rc = ib_hash_get(pump->filter_by_name, NULL, name);
    if (rc == IB_OK) {
        return IB_EINVAL;
    }
    else if (rc != IB_ENOENT) {
        return rc;
    }

    rc = ib_hash_set(pump->filter_by_name, name, filter);
    if (rc != IB_OK) {
        return rc;
    }

    /* Get or create the filters list. */
    rc = ib_hash_get(pump->filter_by_type, &filters, type);
    if (rc == IB_ENOENT) {
        rc = ib_list_create(&filters, pump->mm);
        if (rc != IB_OK) {
            return rc;
        }

        rc = ib_hash_set(pump->filter_by_type, type, filters);
        if (rc != IB_OK) {
            return rc;
        }
    }
    else if (rc != IB_OK) {
        return rc;
    }

    /* Append the filter to the type list. */
    rc = ib_list_push(filters, filter);
    if (rc != IB_OK) {
        return rc;
    }

    return IB_OK;
}

ib_status_t DLL_PUBLIC ib_stream_pump_filter_find(
    ib_stream_pump_t         *pump,
    const char               *name,
    ib_filter_t             **filter
)
{
    assert(pump != NULL);
    assert(filter != NULL);
    assert(name != NULL);
    assert(pump->filter_by_name != NULL);

    return ib_hash_get(pump->filter_by_name, filter, name);
}

ib_status_t DLL_PUBLIC ib_stream_pump_filters_find(
    ib_stream_pump_t *pump,
    const char       *type,
    ib_list_t        *filters
)
{
    assert(pump != NULL);
    assert(filters != NULL);
    assert(type != NULL);
    assert(pump->filter_by_type != NULL);

    ib_list_t      *list;
    ib_status_t     rc;
    ib_list_node_t *node;

    rc = ib_hash_get(pump->filter_by_type, &list, type);
    if (rc != IB_OK) {
        return rc;
    }

    /* This should not be, but check to be safe. */
    if (ib_list_elements(list) == 0) {
        return IB_ENOENT;
    }

    /* Copy the list data to the output list. */
    IB_LIST_LOOP(list, node) {
        rc = ib_list_push(filters, ib_list_node_data(node));
        if (rc != IB_OK) {
            return rc;
        }
    }

    return IB_OK;
}

ib_status_t DLL_PUBLIC ib_stream_pump_inst_name_create(
    ib_stream_pump_inst_t *pump_inst,
    const char            *name,
    void                  *arg,
    ib_filter_inst_t     **filter_inst
)
{
    assert(pump_inst != NULL);
    assert(name != NULL);
    assert(filter_inst != NULL);

    ib_filter_inst_t *tmp_inst;
    ib_filter_t      *filter;
    ib_status_t       rc;

    rc = ib_stream_pump_filter_find(pump_inst->stream_pump, name, &filter);
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_filter_inst_create(&tmp_inst, pump_inst->mm, filter, arg);
    if (rc != IB_OK) {
        return rc;
    }

    *filter_inst = tmp_inst;
    return IB_OK;
}

ib_status_t DLL_PUBLIC ib_stream_pump_inst_name_add(
    ib_stream_pump_inst_t *pump_inst,
    const char            *name,
    void                  *arg
)
{
    assert(pump_inst != NULL);
    assert(name != NULL);

    ib_filter_inst_t *filter_inst;
    ib_status_t       rc;

    rc = ib_stream_pump_inst_name_create(pump_inst, name, arg, &filter_inst);
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_stream_pump_inst_add(pump_inst, filter_inst);
    if (rc != IB_OK) {
        return rc;
    }

    return IB_OK;
}

