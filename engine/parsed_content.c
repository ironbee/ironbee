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
 * @brief IronBee interface for handling parsed content.
 *
 * @author Sam Baskinger <sbaskinger@qualys.com>
 */

#include "ironbee_config_auto.h"

/* System includes. */
#include <assert.h>

/* Include engine structs, etc. */
#include <ironbee_private.h>

/* Public IronBee includes. */
#include <ironbee/debug.h>
#include <ironbee/parsed_content.h>
#include <ironbee/mpool.h>

DLL_PUBLIC ib_status_t ib_parsed_tx_create(ib_engine_t *ib_engine,
                                           ib_parsed_tx_t **transaction,
                                           void *user_data)
{
    IB_FTRACE_INIT();
    assert(ib_engine != NULL);
    assert(ib_engine->mp != NULL);

    ib_parsed_tx_t *tx_tmp;

    tx_tmp = ib_mpool_calloc(ib_engine->mp, 1, sizeof(*tx_tmp));

    if ( tx_tmp == NULL ) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    /* Set user_data. */
    tx_tmp->user_data = user_data;

    /* Commit back built object. */
    *transaction = tx_tmp;

    IB_FTRACE_RET_STATUS(IB_OK);
}

DLL_PUBLIC ib_status_t ib_parsed_tx_req_begin(
    ib_parsed_tx_t *transaction,
    ib_parsed_req_method_t *method,
    ib_parsed_req_path_t *path,
    ib_parsed_req_version_t *version)
{
    IB_FTRACE_INIT();
    IB_FTRACE_RET_STATUS(IB_ENOTIMPL); /* FIXME - implement. */
}

DLL_PUBLIC ib_status_t ib_parsed_tx_req_header(
    ib_parsed_tx_t *transaction,
    ib_parsed_header_name_t *name,
    ib_parsed_header_value_t *value)
{
    IB_FTRACE_INIT();
    IB_FTRACE_RET_STATUS(IB_ENOTIMPL); /* FIXME - implement. */
}

DLL_PUBLIC ib_status_t ib_parsed_tx_resp_header(
    ib_parsed_tx_t *transaction,
    ib_parsed_header_name_t *name,
    ib_parsed_header_value_t *value)
{
    IB_FTRACE_INIT();
    IB_FTRACE_RET_STATUS(IB_ENOTIMPL); /* FIXME - implement. */
}

DLL_PUBLIC ib_status_t ib_parsed_tx_req_end(ib_parsed_tx_t *transaction)
{
    IB_FTRACE_INIT();
    IB_FTRACE_RET_STATUS(IB_ENOTIMPL); /* FIXME - implement. */
}

DLL_PUBLIC ib_status_t ib_parsed_tx_resp_begin(
    ib_parsed_tx_t *transaction,
    ib_parsed_resp_status_code_t *code,
    ib_parsed_resp_status_msg_t *msg)
{
    IB_FTRACE_INIT();
    IB_FTRACE_RET_STATUS(IB_ENOTIMPL); /* FIXME - implement. */
}

DLL_PUBLIC ib_status_t ib_parsed_tx_resp_body(ib_parsed_tx_t *transaction,
                                              ib_parsed_data_t *data)
{
    IB_FTRACE_INIT();
    IB_FTRACE_RET_STATUS(IB_ENOTIMPL); /* FIXME - implement. */
}

DLL_PUBLIC ib_status_t ib_parsed_tx_req_body(ib_parsed_tx_t *transaction,
                                             ib_parsed_data_t *data)
{
    IB_FTRACE_INIT();
    IB_FTRACE_RET_STATUS(IB_ENOTIMPL); /* FIXME - implement. */
}

DLL_PUBLIC ib_status_t ib_parsed_tx_resp_end(ib_parsed_tx_t *transaction)
{
    IB_FTRACE_INIT();
    IB_FTRACE_RET_STATUS(IB_ENOTIMPL); /* FIXME - implement. */
}

DLL_PUBLIC ib_status_t ib_parsed_tx_req_trailer(
    ib_parsed_tx_t *transaction,
    ib_parsed_trailer_name_t *name,
    ib_parsed_trailer_value_t *value)
{
    IB_FTRACE_INIT();
    IB_FTRACE_RET_STATUS(IB_ENOTIMPL); /* FIXME - implement. */
}

DLL_PUBLIC ib_status_t ib_parsed_tx_resp_trailer(
    ib_parsed_tx_t *transaction,
    ib_parsed_trailer_name_t *name,
    ib_parsed_trailer_value_t *value)
{
    IB_FTRACE_INIT();
    IB_FTRACE_RET_STATUS(IB_ENOTIMPL); /* FIXME - implement. */
}

DLL_PUBLIC void* ib_parsed_tx_get_user_data(const ib_parsed_tx_t *transaction)
{
    assert(transaction != NULL);
    return transaction->user_data;
}

DLL_PUBLIC ib_status_t ib_parsed_tx_destroy(const ib_parsed_tx_t *transaction)
{
    IB_FTRACE_INIT();
    // nop.
    IB_FTRACE_RET_STATUS(IB_OK);
}
