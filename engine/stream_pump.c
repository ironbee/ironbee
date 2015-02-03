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

#include <ironbee/list.h>
#include <ironbee/log.h>
#include <ironbee/mm_mpool_lite.h>
#include <ironbee/mpool_freeable.h>
#include <ironbee/mpool_lite.h>
#include <ironbee/stream_pump.h>

#include <assert.h>

//! A list of processors and a context to execute them.
struct ib_stream_pump_t {
    //! Basic allocations.
    ib_mm_t mm;

    //! The memory pool to create data segments out of.
    ib_mpool_freeable_t *mp;

    //! The stream pump that created this instance.
    ib_stream_processor_registry_t *registry;

    //! Processor to execute.
    ib_list_t *processors;

    //! The transaction for this pump.
    ib_tx_t *tx;
};

ib_status_t ib_stream_pump_create(
    ib_stream_pump_t               **pump,
    ib_stream_processor_registry_t  *registry,
    ib_tx_t                         *tx
)
{
    assert(pump != NULL);
    assert(registry != NULL);
    assert(tx != NULL);

    ib_stream_pump_t *tmp_pump;
    ib_status_t       rc;
    ib_mm_t           mm = tx->mm;

    tmp_pump = ib_mm_alloc(mm, sizeof(*tmp_pump));
    if (tmp_pump == NULL) {
        ib_log_alert_tx(tx, "Failed to allocate pump.");
        return IB_EALLOC;
    }

    rc = ib_list_create(&tmp_pump->processors, mm);
    if (rc != IB_OK) {
        ib_log_alert_tx(tx, "Failed to create processors list in pump.");
        return rc;
    }

    rc = ib_mpool_freeable_create(&tmp_pump->mp);
    if (rc != IB_OK) {
        ib_log_alert_tx(tx, "Failed to create freeable mpool in pump.");
        return rc;
    }

    tmp_pump->mm       = mm;
    tmp_pump->registry = registry;
    tmp_pump->tx       = tx;

    *pump = tmp_pump;
    return IB_OK;
}

/**
 * Clear a list of @ref ib_stream_processor_data_t calling destroy on each.
 *
 * @param[in] list The list of data segments.
 * @param[in] mp Memory pool tracking the allocations in @a list.
 */
static void stream_pump_process_clear_list(
    ib_list_t           *list,
    ib_mpool_freeable_t *mp
)
{
    assert(mp != NULL);
    assert(list != NULL);

    /* Ensure that the out list is empty. */
    if (ib_list_elements(list) > 0) {
        ib_list_node_t *node;

        /* Free all out list data segments. */
        IB_LIST_LOOP(list, node) {
            ib_stream_processor_data_destroy(
                (ib_stream_processor_data_t *)ib_list_node_data(node),
                mp
            );
        }

        /* Now clear the list. */
        ib_list_clear(list);
    }
}

/**
 * The core logic of executing the pump.
 *
 * This excludes most of the set-up logic like allocating
 * temporary memory pools and argument lists. As such
 * arguments to this function are editted liberally
 * and should not be used in future computation.
 *
 * @param[in] pump The pump.
 * @param[in] data_in A list of @ref ib_stream_processor_data_t.
 *            Elements in this list will have
 *            ib_stream_processor_data_destroy() called on them
 *            before this function returns.
 *            This list will likely be cleared and re-populated with
 *            useless data.
 * @param[in] data_out A list to put results in. This list
 *            must be initially empty. This list will likely
 *            be cleared and re-populated with useless data.
 * @param[in] mm_eval A memory manager that is freed when pump
 *            evaluation concludes.
 *
 * @returns
 * - IB_OK On success.
 * - Other on error.
 */
static ib_status_t stream_pump_process_impl(
    ib_stream_pump_t *pump,
    ib_list_t        *data_in,
    ib_list_t        *data_out,
    ib_mm_t           mm_eval
)
{
    assert(pump != NULL);
    assert(pump->mp != NULL);
    assert(pump->tx != NULL);
    assert(data_in != NULL);
    assert(data_out != NULL);

    ib_list_node_t *node;

    /**
     * Important things to observe about this loop:
     *
     * - Data lists are cleared. That is, each element
     *   has *_destroy() called on it, possibly freeing the memory
     *   but only if the reference count goes from 1 to 0.
     * - data_out is cleared at the end of each iteration.
     *   - When data_in and data_out swap (IB_OK is returned)
     *     the previous input is effecitvely the list being
     *     cleared.
     *   - When data_in and data_out do not swap (IB_DECLINED is returned)
     *     data_out is cleared. This list should be empty.
     * - data_in is cleared after the loop exits. Thus in the
     *   last iteration (or when there are no iterations)
     *   the input list is cleared.
     */
    IB_LIST_LOOP(pump->processors, node) {
        ib_status_t rc;

        ib_stream_processor_t *processor =
            (ib_stream_processor_t *)ib_list_node_data(node);

        rc = ib_stream_processor_execute(
            processor,
            pump->tx,
            pump->mp,
            mm_eval,
            data_in,
            data_out);

        /* If evaluation of a processor is OK, there is data in data_out. */
        if (rc == IB_OK) {
            ib_list_t *tmp = data_in;

            /* Data_out is now data_in. */
            data_in = data_out;

            /* Data_in is now used as the output list. */
            data_out = tmp;
        }
        /* If rc != IB_DECLINED (and rc != IB_OK) then there is an error. */
        else if (rc != IB_DECLINED) {
            ib_log_alert_tx(
                pump->tx,
                "Error returned by processor instance \"%s.\"",
                ib_stream_processor_name(processor)
            );
            return rc;
        }

        /* Always be sure the out list is empty. */
        stream_pump_process_clear_list(data_out, pump->mp);
    }

    /* When we are done processing, clear the in-list. */
    stream_pump_process_clear_list(data_in, pump->mp);

    return IB_OK;
}

/**
 * Setup the common parts of for procesing a stream and call process_impl.
 */
static ib_status_t stream_pump_process_setup_and_run(
    ib_stream_pump_t           *pump,
    ib_stream_processor_data_t *arg
)
{
    assert(pump != NULL);
    assert(pump->tx != NULL);
    assert(arg != NULL);

    ib_status_t      rc;
    ib_mpool_lite_t *mp_eval = NULL;
    ib_mm_t          mm_eval;
    ib_list_t       *data_out;
    ib_list_t       *data_in;

    /* Create a temporary memory pool for this evaluation only. */
    rc = ib_mpool_lite_create(&mp_eval);
    if (rc != IB_OK) {
        ib_log_alert_tx(pump->tx, "Failed to create eval memory pool.");
        return rc;
    }
    /* Wrap the mpool in a memory manager. */
    mm_eval = ib_mm_mpool_lite(mp_eval);

    /* Create a list to put the user's data in. */
    rc = ib_list_create(&data_in, mm_eval);
    if (rc != IB_OK) {
        ib_log_alert_tx(pump->tx, "Failed to create data in list.");
        goto exit_label;
    }

    /* Push in the data. */
    rc = ib_list_push(data_in, arg);
    if (rc != IB_OK) {
        ib_log_alert_tx(pump->tx, "Failed append data to data list.");
        goto exit_label;
    }

    /* Create the out data list. This will be empty when we call process_impl(). */
    rc = ib_list_create(&data_out, mm_eval);
    if (rc != IB_OK) {
        ib_log_alert_tx(pump->tx, "Failed to create data out list.");
        return rc;
    }

    /* After the above setup, do the actual processing. */
    rc = stream_pump_process_impl(pump, data_in, data_out, mm_eval);
    if (rc != IB_OK) {
        goto exit_label;
    }

exit_label:
    if (mp_eval != NULL) {
        ib_mpool_lite_destroy(mp_eval);
    }

    return rc;
}

ib_status_t ib_stream_pump_process(
    ib_stream_pump_t *pump,
    const uint8_t    *data,
    size_t            data_len
)
{
    assert(pump != NULL);

    ib_status_t                 rc;
    ib_stream_processor_data_t *arg = NULL;

    /* If the user asked us to operate on nothing, that's OK! Do nothing. */
    if (data == NULL || data_len == 0) {
        return IB_OK;
    }

    /* Copy the user's data. We don't know if they will free it or not. */
    rc = ib_stream_processor_data_cpy(&arg, pump->mp, data, data_len);
    if (rc != IB_OK) {
        ib_log_alert_tx(pump->tx, "Failed to wrap data in stream proc data.");
        return rc;
    }

    /* Setup and run the processor. */
    rc = stream_pump_process_setup_and_run(pump, arg);
    if (rc != IB_OK) {
        ib_log_alert_tx(pump->tx, "Failed to setup and run pump.");
        return rc;
    }

    return rc;
}

ib_status_t ib_stream_pump_flush(
    ib_stream_pump_t *pump
)
{
    assert(pump != NULL);
    assert(pump->tx != NULL);

    ib_status_t                 rc;
    ib_stream_processor_data_t *arg = NULL;

    rc = ib_stream_processor_data_flush_create(&arg, pump->mp);
    if (rc != IB_OK) {
        ib_log_alert_tx(pump->tx, "Failed to create flush data.");
        return rc;
    }

    /* Setup and run the processor. */
    rc = stream_pump_process_setup_and_run(pump, arg);
    if (rc != IB_OK) {
        ib_log_alert_tx(pump->tx, "Failed to setup and run pump.");
        return rc;
    }

    return IB_OK;
}

ib_status_t ib_stream_pump_processor_add(
    ib_stream_pump_t *pump,
    const char       *name
)
{
    assert(pump != NULL);
    assert(pump->tx != NULL);
    assert(pump->registry != NULL);
    assert(name != NULL);

    ib_status_t            rc;
    ib_stream_processor_t *processor;

    rc = ib_stream_processor_registry_processor_create(
        pump->registry,
        name,
        &processor,
        pump->tx
    );
    if (rc != IB_OK) {
        ib_log_alert_tx(pump->tx, "Failed to create processor \"%s\"", name);
        return rc;
    }

    rc = ib_list_push(pump->processors, processor);
    if (rc != IB_OK) {
        ib_log_alert_tx(pump->tx, "Failed to add processor \"%s\"", name);
        return rc;
    }

    return IB_OK;
}

ib_status_t ib_stream_pump_processor_insert(
    ib_stream_pump_t *pump,
    const char       *name,
    size_t            idx
)
{
    assert(pump != NULL);
    assert(pump->tx != NULL);
    assert(pump->registry != NULL);
    assert(name != NULL);

    ib_status_t            rc;
    ib_stream_processor_t *processor;

    rc = ib_stream_processor_registry_processor_create(
        pump->registry,
        name,
        &processor,
        pump->tx
    );
    if (rc != IB_OK) {
        ib_log_alert_tx(pump->tx, "Failed to create processor \"%s\"", name);
        return rc;
    }

    rc = ib_list_insert(pump->processors, processor, idx);
    if (rc != IB_OK) {
        ib_log_alert_tx(pump->tx, "Failed to add processor \"%s\"", name);
        return rc;
    }

    return IB_OK;
}

