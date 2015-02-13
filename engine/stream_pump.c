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
#include <ironbee/stream_io.h>
#include <ironbee/stream_pump.h>

#include <assert.h>

//! A list of processors and a context to execute them.
struct ib_stream_pump_t {
    //! Basic allocations.
    ib_mm_t mm;

    //! The stream pump that created this instance.
    ib_stream_processor_registry_t *registry;

    //! Processor to execute.
    ib_list_t *processors;

    //! The transaction for this pump.
    ib_tx_t *tx;

    //! IO System for handling data ownership.
    ib_stream_io_t *io;
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

    rc = ib_stream_io_create(&tmp_pump->io, mm);
    if (rc != IB_OK) {
        ib_log_alert_tx(tx, "Failed to create pump io system.");
        return rc;
    }

    tmp_pump->mm       = mm;
    tmp_pump->registry = registry;
    tmp_pump->tx       = tx;

    *pump = tmp_pump;
    return IB_OK;
}

/**
 * Execute all the pumps and cleanup @a io_tx.
 *
 * @param[in] pump The pump.
 * @param[in] io_tx IO Transaction.
 * @param[in] mm_eval A memory manager that is freed when pump
 *            evaluation concludes.
 *
 * @returns
 * - IB_OK On success.
 * - Other on error.
 */
static ib_status_t stream_pump_process(
    ib_stream_pump_t  *pump,
    ib_stream_io_tx_t *io_tx,
    ib_mm_t            mm_eval
)
{
    assert(pump != NULL);
    assert(pump->tx != NULL);
    assert(io_tx != NULL);

    ib_status_t     rc;
    ib_list_node_t *node;

    IB_LIST_LOOP(pump->processors, node) {
        ib_stream_processor_t *processor =
            (ib_stream_processor_t *)ib_list_node_data(node);

        /* Execute a processor on an IO transaction. */
        rc = ib_stream_processor_execute(processor, pump->tx, mm_eval, io_tx);

        /* If evaluation of a processor is OK, the tx may be reused. */
        if (rc == IB_OK) {
            rc = ib_stream_io_tx_reuse(io_tx);
            if (rc != IB_OK) {
                return rc;
            }
        }
        /* If the processor declined, redo the tx on the next processor. */
        else if (rc == IB_DECLINED) {
            rc = ib_stream_io_tx_redo(io_tx);
            if (rc != IB_OK) {
                return rc;
            }
        }
        /* Not OK. Not declined. Failure. */
        else {
            ib_log_alert_tx(
                pump->tx,
                "Error returned by processor instance \"%s.\"",
                ib_stream_processor_name(processor)
            );
            goto cleanup;
        }
    }

    /* If we exit by this path, there is no error.
     * Also, there is a chance that rc == IB_DECLINED.
     *
     * Always set rc to IB_OK because IB_DECLINED has no meaning as
     * this function's return value. */
    rc = IB_OK;

cleanup:
    ib_stream_io_tx_cleanup(io_tx);

    return rc;
}

/**
 * Setup the common parts of for processing a stream and call processors.
 *
 * @param[in] pump The pump.
 * @param[in] io_tx IO Transaction.
 *
 * @returns
 * - IB_OK On success.
 * - Other On error.
 */
static ib_status_t stream_pump_process_setup_and_run(
    ib_stream_pump_t  *pump,
    ib_stream_io_tx_t *io_tx
)
{
    assert(pump != NULL);
    assert(pump->tx != NULL);
    assert(io_tx != NULL);

    ib_status_t      rc;
    ib_mpool_lite_t *mp_eval = NULL;
    ib_mm_t          mm_eval;

    /* Create a temporary memory pool for this evaluation only. */
    rc = ib_mpool_lite_create(&mp_eval);
    if (rc != IB_OK) {
        ib_log_alert_tx(pump->tx, "Failed to create eval memory pool.");
        return rc;
    }
    /* Wrap the mpool in a memory manager. */
    mm_eval = ib_mm_mpool_lite(mp_eval);

    /* After the above setup, do the actual processing. */
    rc = stream_pump_process(pump, io_tx, mm_eval);
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
    ib_stream_io_tx_t          *io_tx;

    /* If the user asked us to operate on nothing, that's OK! Do nothing. */
    if (data == NULL || data_len == 0) {
        return IB_OK;
    }

    rc = ib_stream_io_tx_create(&io_tx, pump->io);
    if (rc != IB_OK) {
        ib_log_alert_tx(pump->tx, "Failed to create io transaction.");
        return rc;
    }

    rc = ib_stream_io_tx_data_add(io_tx, data, data_len);
    if (rc != IB_OK) {
        ib_log_alert_tx(pump->tx, "Failed to add data to io transaction.");
        return rc;
    }

    /* Setup and run the processor. */
    rc = stream_pump_process_setup_and_run(pump, io_tx);
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

    ib_status_t                 rc;
    ib_stream_io_tx_t          *io_tx;

    rc = ib_stream_io_tx_create(&io_tx, pump->io);
    if (rc != IB_OK) {
        ib_log_alert_tx(pump->tx, "Failed to create io transaction.");
        return rc;
    }

    rc = ib_stream_io_tx_flush_add(io_tx);
    if (rc != IB_OK) {
        ib_log_alert_tx(pump->tx, "Failed to add flush to io transaction.");
        return rc;
    }

    /* Setup and run the processor. */
    rc = stream_pump_process_setup_and_run(pump, io_tx);
    if (rc != IB_OK) {
        ib_log_alert_tx(pump->tx, "Failed to setup and run pump.");
        return rc;
    }

    return rc;

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

