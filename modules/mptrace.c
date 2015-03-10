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
 * @brief IronBee --- Memory Pool Tracing Module
 *
 * This module write memory pool reports to stderr. It is expensive
 * and is only intended for developers diagnosing issues or tuning.
 *
 * @author Brian Rectanus <brectanus@qualys.com>
 */

#include <ironbee/engine_state.h>
#include <ironbee/module.h>
#include <ironbee/mpool.h>

/* NOTE: Need access to the engine mpool (ib->mp), not just the mm. */
#include "engine_private.h"

#include <assert.h>
#include <stdio.h>


/* Define the module name as well as a string version of it. */
#define MODULE_NAME     mptrace
#define MODULE_NAME_STR IB_XSTRINGIFY(MODULE_NAME)

/* Declare the public module symbol. */
IB_MODULE_DECLARE();

/**
 * Handle printing generic memory pool reports.
 *
 * @param[in] mp Memory pool
 */
static void mptrace_mpool_report(const ib_mpool_t *mp) {
    assert(mp != NULL);

    char *report = ib_mpool_analyze(mp);

    fprintf(stderr,
            "\n"
            "*** IronBee Memory Pool %p Report Begin ***\n"
            "%s"
            "*** IronBee Memory Pool %p Report End ***\n",
            mp, report, mp);

    free(report);
}

/**
 * Handle connection reports.
 *
 * @param[in] ib IronBee object
 * @param[in] conn Connection object
 * @param[in] state Engine state
 * @param[in] cbdata Callback data (unused)
 *
 * @returns Status code
 */
static ib_status_t mptrace_conn_report(ib_engine_t *ib,
                                       ib_conn_t   *conn,
                                       ib_state_t   state,
                                       void        *cbdata)
{
    assert(ib != NULL);
    assert(conn != NULL);

    mptrace_mpool_report(conn->mp);

    return IB_OK;
}

/**
 * Handle transaction reports.
 *
 * @param[in] ib IronBee object
 * @param[in] tx Transaction object
 * @param[in] state Engine state
 * @param[in] cbdata Callback data (unused)
 *
 * @returns Status code
 */
static ib_status_t mptrace_tx_report(ib_engine_t *ib,
                                     ib_tx_t     *tx,
                                     ib_state_t   state,
                                     void        *cbdata)
{
    assert(ib != NULL);
    assert(tx != NULL);

    mptrace_mpool_report(tx->mp);

    return IB_OK;
}

/**
 * Handle module initialization.
 *
 * @param[in] ib IronBee engine
 * @param[in] module Module object
 * @param[in] cbdata Callback data (unused)
 *
 * @returns Status code
 */
static ib_status_t mptrace_init(ib_engine_t *ib,
                                ib_module_t *module,
                                void        *cbdata)
{
    assert(ib != NULL);
    assert(module != NULL);

    ib_status_t rc;

    ib_log_notice(ib,
                  "Loading diagnostic %s module."
                  " This should not be done on a production system.",
                  MODULE_NAME_STR);

    rc = ib_hook_conn_register(ib, conn_finished_state, mptrace_conn_report, module);
    if (rc != IB_OK) {
        return rc;
    }
    rc = ib_hook_tx_register(ib, tx_finished_state, mptrace_tx_report, module);
    if (rc != IB_OK) {
        return rc;
    }

    return IB_OK;
}

/**
 * Handle module shutdown.
 *
 * @param[in] ib IronBee Engine
 * @param[in] module Module object
 * @param[in] cbdata Callback data (unused).
 *
 * @returns Status code
 */
static ib_status_t mptrace_fini(ib_engine_t *ib,
                                ib_module_t *module,
                                void        *cbdata)
{
    assert(ib != NULL);
    assert(module != NULL);

    mptrace_mpool_report(ib->mp);

    return IB_OK;
}

/**
 * Module structure.
 *
 * This structure defines some metadata, config data and various functions.
 */
IB_MODULE_INIT(
    IB_MODULE_HEADER_DEFAULTS,               /* Default metadata */
    MODULE_NAME_STR,                         /* Module name */
    IB_MODULE_CONFIG_NULL,                   /* Global config data */
    NULL,                                    /* Module config map */
    NULL,                                    /* Module directive map */
    mptrace_init,                            /* Initialize function */
    NULL,                                    /* Callback data */
    mptrace_fini,                            /* Finish function */
    NULL,                                    /* Callback data */
);
