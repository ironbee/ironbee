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
 * @brief IronBee - Core Module Fields
 *
 * @author Brian Rectanus <brectanus@qualys.com>
 */

#include "ironbee_config_auto.h"

#include <ironbee/debug.h>
#include <ironbee/core.h>
#include <ironbee_private.h>

#include <assert.h>


/* -- Field Generation Routines -- */

static inline void core_gen_bytestr_alias_field(ib_tx_t *tx,
                                                const char *name,
                                                ib_bytestr_t *val)
{
    ib_field_t *f;

    assert(tx != NULL);
    assert(name != NULL);
    assert(val != NULL);

    ib_status_t rc = ib_field_create_no_copy(&f, tx->mp,
                                             name, strlen(name),
                                             IB_FTYPE_BYTESTR,
                                             val);
    if (rc != IB_OK) {
        ib_log_warning(tx->ib, "Failed to create \"%s\" field: %s",
                     name, ib_status_to_string(rc));
        return;
    }

    ib_log_debug(tx->ib, "FIELD: \"%s\"=\"%.*s\"",
                 name,
                 (int)ib_bytestr_length(val),
                 (char *)ib_bytestr_const_ptr(val));

    rc = ib_data_add(tx->dpi, f);
    if (rc != IB_OK) {
        ib_log_warning(tx->ib, "Failed add \"%s\" field to data store: %s",
                       name, ib_status_to_string(rc));
    }
}


/* -- Hooks -- */

/*
 * Callback used to generate request fields.
 */
static ib_status_t core_gen_request_header_fields(ib_engine_t *ib,
                                                  ib_tx_t *tx,
                                                  ib_state_event_type_t event,
                                                  void *cbdata)
{
    IB_FTRACE_INIT();

    assert(ib != NULL);
    assert(tx != NULL);
    assert(event == handle_context_tx_event);

    ib_log_debug(ib, "core_gen_request_header_fields");

    core_gen_bytestr_alias_field(tx, "request_line",
                                 tx->request_line->raw);

    core_gen_bytestr_alias_field(tx, "request_method",
                                 tx->request_line->method);

    core_gen_bytestr_alias_field(tx, "request_uri_raw",
                                 tx->request_line->uri);

    core_gen_bytestr_alias_field(tx, "request_protocol",
                                 tx->request_line->protocol);

    IB_FTRACE_RET_STATUS(IB_OK);
}

/*
 * Callback used to generate response fields.
 */
static ib_status_t core_gen_response_header_fields(ib_engine_t *ib,
                                                   ib_tx_t *tx,
                                                   ib_state_event_type_t event,
                                                   void *cbdata)
{
    IB_FTRACE_INIT();

    assert(ib != NULL);
    assert(tx != NULL);
    assert(event == response_header_finished_event);

    ib_log_debug(ib, "core_gen_response_header_fields");

    if (tx->response_line != NULL) {
        core_gen_bytestr_alias_field(tx, "response_line",
                                     tx->response_line->raw);

        core_gen_bytestr_alias_field(tx, "response_protocol",
                                     tx->response_line->protocol);

        core_gen_bytestr_alias_field(tx, "response_status",
                                     tx->response_line->status);

        core_gen_bytestr_alias_field(tx, "response_message",
                                     tx->response_line->msg);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}


/* -- Initialization Routines -- */

/* Initialize libhtp config object for the context. */
ib_status_t ib_core_fields_ctx_init(ib_engine_t *ib,
                                    ib_module_t *mod,
                                    ib_context_t *ctx,
                                    void *cbdata)
{
    IB_FTRACE_INIT();
    ib_core_cfg_t *corecfg;
    ib_status_t rc;

    assert(ib != NULL);
    assert(mod != NULL);
    assert(ctx != NULL);

    /* Get the core context config. */
    rc = ib_context_module_config(ctx, mod, (void *)&corecfg);
    if (rc != IB_OK) {
        ib_log_alert(ib,
                     "Failed to fetch core module context config: %s", ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

/* Initialize core field generation callbacks. */
ib_status_t ib_core_fields_init(ib_engine_t *ib,
                                ib_module_t *mod)
{
    IB_FTRACE_INIT();

    assert(ib != NULL);
    assert(mod != NULL);


    ib_hook_tx_register(ib, handle_context_tx_event,
                        core_gen_request_header_fields, NULL);

    ib_hook_tx_register(ib, response_header_finished_event,
                        core_gen_response_header_fields, NULL);

    IB_FTRACE_RET_STATUS(IB_OK);
}
