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
#include <ironbee/engine.h>
#include <ironbee/core.h>
#include <ironbee_private.h>

#include <assert.h>


/* -- Field Generation Routines -- */

/* Placeholder for as-of-yet-initialized bytestring fields. */
static const uint8_t core_placeholder_value[] = {
    '_', '_', 'c', 'o', 'r', 'e', '_', '_',
    'p', 'l', 'a', 'c', 'e', 'h', 'o', 'l',
    'd', 'e', 'r', '_', '_', 'v', 'a', 'l',
    'u', 'e', '_', '_',  0,   0,   0,   0
};

static ib_status_t core_field_placeholder_bytestr(ib_provider_inst_t *dpi,
                                                  const char *name)
{
    IB_FTRACE_INIT();
    ib_status_t rc = ib_data_add_bytestr_ex(dpi,
                                            (const char *)name,
                                            strlen(name),
                                            (uint8_t *)core_placeholder_value,
                                            0,
                                            NULL);
    if (rc != IB_OK) {
        ib_log_error(dpi->pr->ib,
                     "Failed to generate \"%s\" placeholder field: %s",
                     name, ib_status_to_string(rc));
    }

    IB_FTRACE_RET_STATUS(rc);
}

static void core_gen_tx_bytestr_alias_field(ib_tx_t *tx,
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

    ib_log_debug(tx->ib, "TX FIELD: \"%s\"=\"%.*s\"",
                 name,
                 (int)ib_bytestr_length(val),
                 (char *)ib_bytestr_const_ptr(val));

    rc = ib_data_add(tx->dpi, f);
    if (rc != IB_OK) {
        ib_log_warning(tx->ib, "Failed add \"%s\" field to transaction data store: %s",
                       name, ib_status_to_string(rc));
    }
}

static void core_gen_tx_numeric_field(ib_tx_t *tx,
                                      const char *name,
                                      ib_num_t val)
{
    ib_field_t *f;

    assert(tx != NULL);
    assert(name != NULL);

    ib_num_t num = val;
    ib_status_t rc = ib_field_create(&f, tx->mp,
                                     name, strlen(name),
                                     IB_FTYPE_NUM,
                                     &num);
    if (rc != IB_OK) {
        ib_log_warning(tx->ib, "Failed to create \"%s\" field: %s",
                     name, ib_status_to_string(rc));
        return;
    }

    ib_log_debug(tx->ib, "TX FIELD: \"%s\"=%d",
                 name,
                 (int)num);

    rc = ib_data_add(tx->dpi, f);
    if (rc != IB_OK) {
        ib_log_warning(tx->ib, "Failed add \"%s\" field to transaction data store: %s",
                       name, ib_status_to_string(rc));
    }
}

#if 0
// Currently unused
static void core_gen_conn_bytestr_alias_field(ib_conn_t *conn,
                                              const char *name,
                                              ib_bytestr_t *val)
{
    ib_field_t *f;

    assert(conn != NULL);
    assert(name != NULL);
    assert(val != NULL);

    ib_status_t rc = ib_field_create_no_copy(&f, conn->mp,
                                             name, strlen(name),
                                             IB_FTYPE_BYTESTR,
                                             val);
    if (rc != IB_OK) {
        ib_log_warning(conn->ib, "Failed to create \"%s\" field: %s",
                     name, ib_status_to_string(rc));
        return;
    }

    ib_log_debug(conn->ib, "CONN FIELD: \"%s\"=\"%.*s\"",
                 name,
                 (int)ib_bytestr_length(val),
                 (char *)ib_bytestr_const_ptr(val));

    rc = ib_data_add(conn->dpi, f);
    if (rc != IB_OK) {
        ib_log_warning(conn->ib, "Failed add \"%s\" field to connection data store: %s",
                       name, ib_status_to_string(rc));
    }
}

static void core_gen_conn_numeric_field(ib_conn_t *conn,
                                        const char *name,
                                        ib_num_t val)
{
    ib_field_t *f;

    assert(conn != NULL);
    assert(name != NULL);

    ib_num_t num = val;
    ib_status_t rc = ib_field_create(&f, conn->mp,
                                     name, strlen(name),
                                     IB_FTYPE_NUM,
                                     &num);
    if (rc != IB_OK) {
        ib_log_warning(conn->ib, "Failed to create \"%s\" field: %s",
                     name, ib_status_to_string(rc));
        return;
    }

    ib_log_debug(conn->ib, "CONN FIELD: \"%s\"=%d",
                 name,
                 (int)num);

    rc = ib_data_add(conn->dpi, f);
    if (rc != IB_OK) {
        ib_log_warning(conn->ib, "Failed add \"%s\" field to transaction data store: %s",
                       name, ib_status_to_string(rc));
    }
}
#endif


/* -- Hooks -- */

// FIXME: This needs to go away and be replaced with dynamic fields
static ib_status_t core_gen_placeholder_fields(ib_engine_t *ib,
                                               ib_tx_t *tx,
                                               ib_state_event_type_t event,
                                               void *cbdata)
{
    IB_FTRACE_INIT();

    assert(ib != NULL);
    assert(tx != NULL);
    assert(tx->dpi != NULL);
    assert(event == tx_started_event);

    ib_status_t rc;

    /* Core Request Fields */
    rc = ib_data_add_stream(tx->dpi, "request_body", NULL);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = core_field_placeholder_bytestr(tx->dpi, "request_line");
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = core_field_placeholder_bytestr(tx->dpi, "request_method");
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = core_field_placeholder_bytestr(tx->dpi, "request_protocol");
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = core_field_placeholder_bytestr(tx->dpi, "request_uri");
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = core_field_placeholder_bytestr(tx->dpi, "request_uri_raw");
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = core_field_placeholder_bytestr(tx->dpi, "request_uri_scheme");
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = core_field_placeholder_bytestr(tx->dpi, "request_uri_username");
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = core_field_placeholder_bytestr(tx->dpi, "request_uri_password");
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = core_field_placeholder_bytestr(tx->dpi, "request_uri_host");
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = core_field_placeholder_bytestr(tx->dpi, "request_host");
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = core_field_placeholder_bytestr(tx->dpi, "request_uri_port");
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = core_field_placeholder_bytestr(tx->dpi, "request_uri_path");
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = core_field_placeholder_bytestr(tx->dpi, "request_uri_query");
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = core_field_placeholder_bytestr(tx->dpi, "request_uri_fragment");
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = core_field_placeholder_bytestr(tx->dpi, "request_content_type");
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = core_field_placeholder_bytestr(tx->dpi, "request_filename");
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = core_field_placeholder_bytestr(tx->dpi, "auth_type");
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = core_field_placeholder_bytestr(tx->dpi, "auth_username");
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = core_field_placeholder_bytestr(tx->dpi, "auth_password");
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Core Request Collections */
    rc = ib_data_add_list(tx->dpi, "request_headers", NULL);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = ib_data_add_list(tx->dpi, "request_cookies", NULL);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = ib_data_add_list(tx->dpi, "request_uri_params", NULL);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Core Response Fields */
    rc = ib_data_add_stream(tx->dpi, "response_body", NULL);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = core_field_placeholder_bytestr(tx->dpi, "response_line");
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = core_field_placeholder_bytestr(tx->dpi, "response_protocol");
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = core_field_placeholder_bytestr(tx->dpi, "response_status");
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = core_field_placeholder_bytestr(tx->dpi, "response_message");
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = core_field_placeholder_bytestr(tx->dpi, "response_content_type");
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Core Response Collections */
    rc = ib_data_add_list(tx->dpi, "response_headers", NULL);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = ib_data_add_list(tx->dpi, "response_cookies", NULL);

    IB_FTRACE_RET_STATUS(rc);
}

/*
 * Callback used to generate connection fields.
 */
static ib_status_t core_gen_connect_fields(ib_engine_t *ib,
                                           ib_state_event_type_t event,
                                           ib_conn_t *conn,
                                           void *cbdata)
{
    IB_FTRACE_INIT();
    ib_status_t rc;

    ib_log_debug3(ib, "core_gen_connect_fields");

    assert(ib != NULL);
    assert(conn != NULL);
    assert(event == handle_connect_event);

    rc = ib_data_add_bytestr(conn->dpi,
                             "server_addr",
                             (uint8_t *)conn->local_ipstr,
                             strlen(conn->local_ipstr),
                             NULL);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = ib_data_add_num(conn->dpi,
                         "server_port",
                         conn->local_port,
                         NULL);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = ib_data_add_bytestr(conn->dpi,
                             "remote_addr",
                             (uint8_t *)conn->remote_ipstr,
                             strlen(conn->remote_ipstr),
                             NULL);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = ib_data_add_num(conn->dpi,
                         "remote_port",
                         conn->remote_port,
                         NULL);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }


    IB_FTRACE_RET_STATUS(IB_OK);
}


/**
 * Create an alias list collection.
 *
 * @param ib Engine.
 * @param tx Transaction.
 * @param event Event type.
 * @param cbdata Callback data
 *
 * @returns Status code
 */
static ib_status_t create_header_alias_list(
    ib_engine_t *ib,
    ib_tx_t *tx,
    const char *name,
    ib_parsed_header_wrapper_t *header)
{
    IB_FTRACE_INIT();
    ib_field_t *f;
    ib_list_t *header_list;
    ib_status_t rc;
    ib_parsed_name_value_pair_list_t *nvpair;

    assert(ib != NULL);
    assert(tx != NULL);
    assert(name != NULL);
    assert(header != NULL);

    /* Create the list */
    rc = ib_data_add_list(tx->dpi, name, &f);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }
    rc = ib_field_mutable_value(f, ib_ftype_list_mutable_out(&header_list));
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Loop through the list & alias everything */
    for(nvpair = header->head;  nvpair != NULL;  nvpair = nvpair->next) {
        assert(nvpair);
        assert(nvpair->value);
        ib_bytestr_t *bs = NULL;
        if (ib_bytestr_ptr(nvpair->value) != NULL) {
            rc = ib_bytestr_alias_mem(
                &bs,
                tx->mp,
                ib_bytestr_ptr(nvpair->value),
                ib_bytestr_length(nvpair->value)
            );
        }
        else {
            rc = ib_bytestr_dup_mem(&bs, tx->mp, (const uint8_t *)"", 0);
        }
        if (rc != IB_OK) {
            ib_log_error_tx(
                tx,
                "Error creating bytestring of '%.*s' for %s: %s",
                (int)ib_bytestr_length(nvpair->name),
                (const char *)ib_bytestr_ptr(nvpair->name),
                name,
                ib_status_to_string(rc)
            );
            IB_FTRACE_RET_STATUS(rc);
        }

        /* Create a byte string field */
        rc = ib_field_create(
            &f,
            tx->mp,
            (const char *)ib_bytestr_const_ptr(nvpair->name),
            ib_bytestr_length(nvpair->name),
            IB_FTYPE_BYTESTR,
            ib_ftype_bytestr_in(bs)
        );
        if (rc != IB_OK) {
            ib_log_error_tx(tx,
                            "Error creating field of '%.*s' for %s: %s",
                            (int)ib_bytestr_length(nvpair->name),
                            (const char *)ib_bytestr_ptr(nvpair->name),
                            name,
                            ib_status_to_string(rc));
            IB_FTRACE_RET_STATUS(rc);
        }

        /* Add the field to the list */
        rc = ib_list_push(header_list, f);
        if (rc != IB_OK) {
            ib_log_error_tx(tx, "Error adding alias of '%.*s' to %s list: %s",
                            (int)ib_bytestr_length(nvpair->name),
                            (const char *)ib_bytestr_ptr(nvpair->name),
                            name,
                            ib_status_to_string(rc));
            IB_FTRACE_RET_STATUS(rc);
        }
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

/*
 * Callback used to generate request fields.
 */
static ib_status_t core_gen_request_header_fields(ib_engine_t *ib,
                                                  ib_tx_t *tx,
                                                  ib_state_event_type_t event,
                                                  void *cbdata)
{
    IB_FTRACE_INIT();
    ib_field_t *f;
    ib_status_t rc;

    ib_log_debug3_tx(tx, "core_gen_request_header_fields");

    assert(ib != NULL);
    assert(tx != NULL);
    assert(event == handle_context_tx_event);

    /**
     * Alias connection remote and server addresses
     */

    rc = ib_data_get(tx->conn->dpi, "server_addr", &f);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }
    rc = ib_data_add(tx->dpi, f);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = ib_data_get(tx->conn->dpi, "server_port", &f);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }
    rc = ib_data_add(tx->dpi, f);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = ib_data_get(tx->conn->dpi, "remote_addr", &f);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }
    rc = ib_data_add(tx->dpi, f);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = ib_data_get(tx->conn->dpi, "remote_port", &f);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }
    rc = ib_data_add(tx->dpi, f);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    core_gen_tx_numeric_field(tx, "conn_tx_count",
                              tx->conn->tx_count);

    core_gen_tx_bytestr_alias_field(tx, "request_line",
                                    tx->request_line->raw);

    core_gen_tx_bytestr_alias_field(tx, "request_method",
                                    tx->request_line->method);

    core_gen_tx_bytestr_alias_field(tx, "request_uri_raw",
                                    tx->request_line->uri);

    core_gen_tx_bytestr_alias_field(tx, "request_protocol",
                                    tx->request_line->protocol);

    /* Alias ARGS fields */
    // FIXME: This needs to include request_body_params as well, so
    //        it should probably be a new collection with aliases to
    //        each param.
    rc = ib_data_get(tx->dpi, "request_uri_params", &f);
    if (rc == IB_OK) {
        rc = ib_data_add_named(tx->dpi, f, "args", 4);
        if (rc != IB_OK) {
            ib_log_error_tx(tx, "Failed to alias ARGS: %s", ib_status_to_string(rc));
        }
        rc = ib_data_add_named(tx->dpi, f, "args_get", 8);
        if (rc != IB_OK) {
            ib_log_error_tx(tx, "Failed to alias ARGS_GET: %s", ib_status_to_string(rc));
        }
    }

    /* Create the aliased request header list */
    if (tx->request_header != NULL) {
        rc = create_header_alias_list(ib,
                                      tx,
                                      "request_headers",
                                      tx->request_header);
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }
    }

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
    ib_status_t rc;

    ib_log_debug3_tx(tx, "core_gen_response_header_fields");

    assert(ib != NULL);
    assert(tx != NULL);
    assert(event == response_header_finished_event);

    if (tx->response_line != NULL) {
        core_gen_tx_bytestr_alias_field(tx, "response_line",
                                        tx->response_line->raw);

        core_gen_tx_bytestr_alias_field(tx, "response_protocol",
                                        tx->response_line->protocol);

        core_gen_tx_bytestr_alias_field(tx, "response_status",
                                        tx->response_line->status);

        core_gen_tx_bytestr_alias_field(tx, "response_message",
                                        tx->response_line->msg);
    }

    /* Create the aliased response header list */
    if (tx->response_header != NULL) {
        rc = create_header_alias_list(ib,
                                      tx,
                                      "response_headers",
                                      tx->response_header);
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }
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


    ib_hook_conn_register(ib, handle_connect_event,
                          core_gen_connect_fields, NULL);

    ib_hook_tx_register(ib, tx_started_event,
                        core_gen_placeholder_fields, NULL);

    ib_hook_tx_register(ib, handle_context_tx_event,
                        core_gen_request_header_fields, NULL);

    ib_hook_tx_register(ib, response_header_finished_event,
                        core_gen_response_header_fields, NULL);

    IB_FTRACE_RET_STATUS(IB_OK);
}
