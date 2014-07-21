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

#ifndef _IB_HOOK_PRIVATE_H_
#define _IB_HOOK_PRIVATE_H_

/**
 * @file
 * @brief IronBee --- Hook Private Declarations
 *
 * @author Brian Rectanus <brectanus@qualys.com>
 */

#include <ironbee/engine_state.h>
#include <ironbee/engine_types.h>
#include <ironbee/state_notify.h>
#include <ironbee/types.h>

/**
 * Internal hook structure
 */
typedef struct ib_hook_t ib_hook_t;
struct ib_hook_t {
    union {
        /*! Comparison only */
        ib_void_fn_t                as_void;

        ib_state_null_hook_fn_t     null;
        ib_state_conn_hook_fn_t     conn;
        ib_state_tx_hook_fn_t       tx;
        ib_state_txdata_hook_fn_t   txdata;
        ib_state_header_data_fn_t   headerdata;
        ib_state_request_line_fn_t  requestline;
        ib_state_response_line_fn_t responseline;
        ib_state_ctx_hook_fn_t      ctx;
    } callback;
    void               *cbdata;            /**< Data passed to the callback */
};

/**
 * Check that @a state is appropriate for @a hook_type.
 *
 * @param[in] ib IronBee Engine.
 * @param[in] state The state..
 * @param[in] hook_type The hook that is proposed to match the @a state.
 * @returns IB_OK or IB_EINVAL if @a state is not suitable for @a hook_type.
 */
ib_status_t ib_hook_check(ib_engine_t *ib,
                          ib_state_t state,
                          ib_state_hook_type_t hook_type);


/* States that are only notified internally. */

/**
 * Notify the state machine that a configuration context has opened
 *
 * @param ib Engine handle
 * @param ctx Configuration context
 *
 * @returns Status code
 */
ib_status_t ib_state_notify_context_open(ib_engine_t *ib,
                                         ib_context_t *ctx);

/**
 * Notify the state machine that a configuration context is closing
 *
 * @param ib Engine handle
 * @param ctx Configuration context
 *
 * @returns Status code
 */
ib_status_t ib_state_notify_context_close(ib_engine_t *ib,
                                          ib_context_t *ctx);

/**
 * Notify the state machine that a configuration context is being destroyed
 *
 * @param ib Engine handle
 * @param ctx Configuration context
 *
 * @returns Status code
 */
ib_status_t ib_state_notify_context_destroy(ib_engine_t *ib,
                                            ib_context_t *ctx);


#endif /* IB_HOOK_PRIVATE_H */
