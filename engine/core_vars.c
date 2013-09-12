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
 * @brief IronBee - Core Module: Vars
 *
 * @author Brian Rectanus <brectanus@qualys.com>
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include "ironbee_config_auto.h"

#include "core_private.h"

#include <ironbee/capture.h>
#include <ironbee/context.h>
#include <ironbee/core.h>
#include <ironbee/engine.h>
#include <ironbee/engine_state.h>
#include <ironbee/field.h>
#include <ironbee/stream.h>

#include <assert.h>

/*
 * Important!
 *
 * Setting a var is slow (i.e., not O(1)).  As such, there is little to gain
 * from acquiring the var source ahead of time for set operations.  This
 * greatly simplifies this code.
 */

/* -- Field Generation Routines -- */

/* Keys to register as indexed. */
static const char *indexed_keys[] = {
    "ARGS",
    "FLAGS",
    "auth_password",
    "auth_type",
    "auth_username",
    "request_body_params",
    "request_content_type",
    "request_cookies",
    "request_filename",
    "request_headers",
    "request_host",
    "request_line",
    "request_method",
    "request_protocol",
    "request_uri",
    "request_uri_fragment",
    "request_uri_host",
    "request_uri_params",
    "request_uri_password",
    "request_uri_path",
    "request_uri_path_raw",
    "request_uri_port",
    "request_uri_query",
    "request_uri_raw",
    "request_uri_scheme",
    "request_uri_username",
    "response_content_type",
    "response_cookies",
    "response_headers",
    "response_line",
    "response_message",
    "response_protocol",
    "response_status",
    NULL
};

/* Placeholder for as-of-yet-initialized bytestring fields. */
static const char core_placeholder_value[] = "__core__placeholder__value__";

static const ib_tx_flag_map_t core_tx_flag_map[] = {
    {
        "suspicious",
        "FLAGS:suspicious",
        IB_TX_FSUSPICIOUS,
        false,
        false
    },
    {
        "inspectRequestHeader",
        "FLAGS:inspectRequestHeader",
        IB_TX_FINSPECT_REQHDR,
        false,
        false
    },
    {
        "inspectRequestBody",
        "FLAGS:inspectRequestBody",
        IB_TX_FINSPECT_REQBODY,
        false,
        false
    },
    {
        "inspectResponseHeader",
        "FLAGS:inspectResponseHeader",
        IB_TX_FINSPECT_RSPHDR,
        false,
        false
    },
    {
        "inspectResponseBody",
        "FLAGS:inspectResponseBody",
        IB_TX_FINSPECT_RSPBODY,
        false,
        false
    },
    {
        "inspectRequestParams",
        "FLAGS:inspectRequestParams",
        IB_TX_FINSPECT_REQPARAMS,
        false,
        false
    },
    {
        "inspectRequestUri",
        "FLAGS:inspectRequestUri",
        IB_TX_FINSPECT_REQURI,
        false,
        false
    },
    {
        "blockingMode",
        "FLAGS:blockingMode",
        IB_TX_FBLOCKING_MODE,
        false,
        false
    },

    /* End */
    { NULL, NULL, IB_TX_FNONE, true, false },
};

static
ib_status_t core_vars_placeholder_bytestr(
    ib_var_store_t *store,
    const char     *name
)
{
    ib_status_t rc;
    ib_var_source_t *source;
    ib_field_t *f;

    rc = ib_var_source_acquire(
        &source,
        ib_var_store_pool(store),
        ib_var_store_config(store),
        name, strlen(name)
    );
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_field_create_bytestr_alias(
        &f,
        ib_var_store_pool(store),
        name, strlen(name),
        (uint8_t *)core_placeholder_value,
        sizeof(core_placeholder_value)
    );
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_var_source_set(
        source,
        store,
        f
    );
    return rc;
}

static void core_gen_tx_bytestr_alias(ib_tx_t *tx,
                                            const char *name,
                                            ib_bytestr_t *val)
{

    assert(tx != NULL);
    assert(name != NULL);
    assert(val != NULL);

    ib_field_t *f;
    ib_var_source_t *source;
    ib_status_t rc;

    rc = ib_field_create_no_copy(
            &f,
            tx->mp,
            name, strlen(name),
            IB_FTYPE_BYTESTR,
            val
    );
    if (rc != IB_OK) {
        ib_log_warning_tx(tx, "Failed to create \"%s\" var: %s",
                          name, ib_status_to_string(rc));
        return;
    }

    rc = ib_var_source_acquire(
        &source,
        tx->mp,
        ib_var_store_config(tx->var_store),
        name, strlen(name)
    );
    if (rc != IB_OK) {
        ib_log_warning_tx(tx, "Failed to acquire \"%s\" var: %s",
                          name, ib_status_to_string(rc));
        return;
    }

    rc = ib_var_source_set(source, tx->var_store, f);
    if (rc != IB_OK) {
        ib_log_warning_tx(tx,
            "Failed add \"%s\" var to transaction: %s",
            name, ib_status_to_string(rc)
        );
    }
}

static void core_gen_tx_bytestr_alias2(
    ib_tx_t *tx,
    const char *name,
    const char *val, size_t val_length
)
{
    assert(tx != NULL);
    assert(name != NULL);
    assert(val != NULL);

    ib_status_t rc;
    ib_bytestr_t *bytestr;

    rc = ib_bytestr_alias_mem(
        &bytestr,
        tx->mp,
        (const uint8_t *)val,
        val_length
    );
    if (rc != IB_OK) {
        ib_log_warning_tx(tx, "Failed to create alias for \"%s\" var: %s",
                          name, ib_status_to_string(rc));
        return;
    }

    core_gen_tx_bytestr_alias(tx, name, bytestr);
}

static void core_gen_tx_numeric(ib_tx_t *tx,
                                      const char *name,
                                      ib_num_t val)
{
    assert(tx != NULL);
    assert(name != NULL);

    ib_field_t *f;
    ib_num_t num = val;
    ib_status_t rc;
    ib_var_source_t *source;

    rc = ib_field_create(&f, tx->mp,
         name, strlen(name),
         IB_FTYPE_NUM,
         &num);
    if (rc != IB_OK) {
        ib_log_warning_tx(tx, "Failed to create \"%s\" field: %s",
                          name, ib_status_to_string(rc));
        return;
    }

    rc = ib_var_source_acquire(
        &source,
        tx->mp,
        ib_var_store_config(tx->var_store),
        name, strlen(name)
    );
    if (rc != IB_OK) {
        ib_log_warning_tx(tx, "Failed to acquire \"%s\" var: %s",
                          name, ib_status_to_string(rc));
        return;
    }

    rc = ib_var_source_set(source, tx->var_store, f);
    if (rc != IB_OK) {
        ib_log_warning_tx(tx,
            "Failed add \"%s\" var to transaction: %s",
            name, ib_status_to_string(rc)
        );
    }
}

static void core_vars_gen_list(ib_tx_t *tx, const char *name)
{
    assert(tx != NULL);
    assert(name != NULL);

    ib_status_t rc;
    ib_var_source_t *source;

    rc = ib_var_source_acquire(
        &source,
        tx->mp,
        ib_var_store_config(tx->var_store),
        name, strlen(name)
    );
    if (rc != IB_OK) {
        ib_log_warning_tx(tx, "Failed to acquire \"%s\" var: %s",
                          name, ib_status_to_string(rc));
        return;
    }

    rc = ib_var_source_initialize(source, NULL, tx->var_store, IB_FTYPE_LIST);
    if (rc != IB_OK) {
        ib_log_warning_tx(tx,
            "Failed add \"%s\" var to transaction: %s",
            name, ib_status_to_string(rc)
        );
    }
}

static bool core_vars_is_set(ib_tx_t *tx, const char *name)
{
    assert(tx != NULL);
    assert(name != NULL);

    ib_status_t rc;
    ib_var_source_t *source;
    ib_field_t *f;

    rc = ib_var_source_acquire(
        &source,
        ib_var_store_pool(tx->var_store),
        ib_var_store_config(tx->var_store),
        name, strlen(name)
    );
    if (rc != IB_OK) {
        return false;
    }
    rc = ib_var_source_get(source, &f, tx->var_store);
    if (rc == IB_ENOENT || ! f) {
        return false;
    }

    return true;
}

/* -- Hooks -- */

// FIXME: This needs to go away and be replaced with dynamic fields
static ib_status_t core_gen_placeholder_fields(ib_engine_t *ib,
                                               ib_tx_t *tx,
                                               ib_state_event_type_t event,
                                               void *cbdata)
{
    assert(ib != NULL);
    assert(tx != NULL);
    assert(tx->var_store != NULL);
    assert(event == tx_started_event);

    ib_status_t rc;

    /* Core Request Fields */
    rc = core_vars_placeholder_bytestr(tx->var_store, "request_line");
    if (rc != IB_OK) {
        return rc;
    }

    rc = core_vars_placeholder_bytestr(tx->var_store, "request_method");
    if (rc != IB_OK) {
        return rc;
    }

    rc = core_vars_placeholder_bytestr(tx->var_store, "request_protocol");
    if (rc != IB_OK) {
        return rc;
    }

    rc = core_vars_placeholder_bytestr(tx->var_store, "request_uri");
    if (rc != IB_OK) {
        return rc;
    }

    rc = core_vars_placeholder_bytestr(tx->var_store, "request_uri_raw");
    if (rc != IB_OK) {
        return rc;
    }

    rc = core_vars_placeholder_bytestr(tx->var_store, "request_uri_scheme");
    if (rc != IB_OK) {
        return rc;
    }

    rc = core_vars_placeholder_bytestr(tx->var_store, "request_uri_username");
    if (rc != IB_OK) {
        return rc;
    }

    rc = core_vars_placeholder_bytestr(tx->var_store, "request_uri_password");
    if (rc != IB_OK) {
        return rc;
    }

    rc = core_vars_placeholder_bytestr(tx->var_store, "request_uri_host");
    if (rc != IB_OK) {
        return rc;
    }

    rc = core_vars_placeholder_bytestr(tx->var_store, "request_host");
    if (rc != IB_OK) {
        return rc;
    }

    rc = core_vars_placeholder_bytestr(tx->var_store, "request_uri_port");
    if (rc != IB_OK) {
        return rc;
    }

    rc = core_vars_placeholder_bytestr(tx->var_store, "request_uri_path");
    if (rc != IB_OK) {
        return rc;
    }

    rc = core_vars_placeholder_bytestr(tx->var_store, "request_uri_path_raw");
    if (rc != IB_OK) {
        return rc;
    }

    rc = core_vars_placeholder_bytestr(tx->var_store, "request_uri_query");
    if (rc != IB_OK) {
        return rc;
    }

    rc = core_vars_placeholder_bytestr(tx->var_store, "request_uri_fragment");
    if (rc != IB_OK) {
        return rc;
    }

    rc = core_vars_placeholder_bytestr(tx->var_store, "request_content_type");
    if (rc != IB_OK) {
        return rc;
    }

    rc = core_vars_placeholder_bytestr(tx->var_store, "request_filename");
    if (rc != IB_OK) {
        return rc;
    }

    rc = core_vars_placeholder_bytestr(tx->var_store, "auth_type");
    if (rc != IB_OK) {
        return rc;
    }

    rc = core_vars_placeholder_bytestr(tx->var_store, "auth_username");
    if (rc != IB_OK) {
        return rc;
    }

    rc = core_vars_placeholder_bytestr(tx->var_store, "auth_password");
    if (rc != IB_OK) {
        return rc;
    }

    /* Core Request Collections */
    core_vars_gen_list(tx, "request_headers");
    core_vars_gen_list(tx, "request_cookies");
    core_vars_gen_list(tx, "request_uri_params");
    core_vars_gen_list(tx, "request_body_params");

    /* ARGS collection */
    if (! core_vars_is_set(tx, "ARGS")) {
        core_vars_gen_list(tx, "ARGS");
    }

    /* Flags collection */
    if (! core_vars_is_set(tx, "FLAGS")) {
        core_vars_gen_list(tx, "FLAGS");
    }

    /* Initialize CAPTURE */
    {
        ib_field_t *capture;
        rc = ib_capture_acquire(tx, NULL, &capture);
        if (rc != IB_OK) {
            return rc;
        }
        assert(capture != NULL);
        rc = ib_capture_clear(capture);
        if (rc != IB_OK) {
            return rc;
        }
    }

    /* Core Response Fields */
    rc = core_vars_placeholder_bytestr(tx->var_store, "response_line");
    if (rc != IB_OK) {
        return rc;
    }

    rc = core_vars_placeholder_bytestr(tx->var_store, "response_protocol");
    if (rc != IB_OK) {
        return rc;
    }

    rc = core_vars_placeholder_bytestr(tx->var_store, "response_status");
    if (rc != IB_OK) {
        return rc;
    }

    rc = core_vars_placeholder_bytestr(tx->var_store, "response_message");
    if (rc != IB_OK) {
        return rc;
    }

    rc = core_vars_placeholder_bytestr(tx->var_store, "response_content_type");
    if (rc != IB_OK) {
        return rc;
    }

    /* Core Response Collections */
    core_vars_gen_list(tx, "response_headers");

    rc = core_vars_placeholder_bytestr(tx->var_store, "FIELD_NAME");
    if (rc != IB_OK) {
        return rc;
    }
    rc = core_vars_placeholder_bytestr(tx->var_store, "FIELD_NAME_FULL");
    if (rc != IB_OK) {
        return rc;
    }

    core_vars_gen_list(tx, "response_cookies");

    return rc;
}

static ib_status_t core_gen_flags_collection(ib_engine_t *ib,
                                             ib_tx_t *tx,
                                             ib_state_event_type_t event,
                                             void *cbdata)
{
    assert(ib != NULL);
    assert(tx != NULL);
    assert(tx->var_store != NULL);
    assert(event == tx_started_event);

    const ib_tx_flag_map_t *flag;

    for (flag = ib_core_vars_tx_flags();  flag->name != NULL;  ++flag) {
        core_gen_tx_numeric(
            tx,
            flag->tx_name,
            (tx->flags & flag->tx_flag ? 1 : 0)
        );
    }

    return IB_OK;
}

static ib_status_t core_slow_get(
    ib_field_t **f,
    ib_tx_t *tx,
    const char *name
)
{
    assert(f != NULL);
    assert(tx != NULL);
    assert(name != NULL);

    ib_status_t rc;
    ib_var_source_t *source;
    ib_field_t *value = NULL;

    rc = ib_var_source_acquire(
        &source,
        ib_var_store_pool(tx->var_store),
        ib_var_store_config(tx->var_store),
        name, strlen(name)
    );
    if (rc != IB_OK) {
        return rc;
    }
    rc = ib_var_source_get(source, &value, tx->var_store);
    if (rc != IB_OK) {
        return rc;
    }

    if (value == NULL) {
        return IB_EOTHER;
    }

    *f = value;
    return IB_OK;
}

/**
 * Create an alias list collection.
 *
 * @param ib Engine.
 * @param tx Transaction.
 * @param name Collection name
 * @param header Header list to alias
 *
 * @returns Status code
 */
static ib_status_t create_header_alias_list(
    ib_engine_t *ib,
    ib_tx_t *tx,
    const char *name,
    ib_parsed_header_wrapper_t *header)
{
    ib_field_t *f;
    ib_list_t *header_list;
    ib_status_t rc;
    ib_parsed_name_value_pair_list_t *nvpair;
    ib_var_source_t *source;

    assert(ib != NULL);
    assert(tx != NULL);
    assert(name != NULL);
    assert(header != NULL);

    /* Create the list */
    rc = ib_var_source_acquire(
        &source,
        ib_var_store_pool(tx->var_store),
        ib_var_store_config(tx->var_store),
        name, strlen(name)
    );
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_var_source_get(source, &f, tx->var_store);
    if (rc == IB_ENOENT || ! f) {
        rc = ib_var_source_initialize(
            source,
            &f,
            tx->var_store,
            IB_FTYPE_LIST
        );
        if (rc != IB_OK) {
            return rc;
        }
    }

    rc = ib_field_mutable_value(f, ib_ftype_list_mutable_out(&header_list));
    if (rc != IB_OK) {
        return rc;
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
            return rc;
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
            return rc;
        }

        /* Add the field to the list */
        rc = ib_list_push(header_list, f);
        if (rc != IB_OK) {
            ib_log_error_tx(tx, "Error adding alias of '%.*s' to %s list: %s",
                            (int)ib_bytestr_length(nvpair->name),
                            (const char *)ib_bytestr_ptr(nvpair->name),
                            name,
                            ib_status_to_string(rc));
            return rc;
        }
    }

    return IB_OK;
}

/*
 * Callback used to generate request header fields.
 */
static ib_status_t core_gen_request_header_fields(ib_engine_t *ib,
                                                  ib_tx_t *tx,
                                                  ib_state_event_type_t event,
                                                  void *cbdata)
{
    ib_field_t *f;
    ib_status_t rc;
    ib_conn_t *conn = tx->conn;

    assert(ib != NULL);
    assert(tx != NULL);
    assert(event == request_header_finished_event);

    core_gen_tx_bytestr_alias2(tx, "server_addr",
                               conn->local_ipstr,
                               strlen(conn->local_ipstr));

    core_gen_tx_numeric(tx, "server_port", conn->local_port);

    core_gen_tx_bytestr_alias2(tx, "remote_addr",
                               conn->remote_ipstr,
                               strlen(conn->remote_ipstr));


    core_gen_tx_numeric(tx, "remote_port", conn->remote_port);

    core_gen_tx_numeric(tx, "conn_tx_count",
                        tx->conn->tx_count);

    if (tx->request_line != NULL) {
        core_gen_tx_bytestr_alias(tx, "request_line",
                                  tx->request_line->raw);

        core_gen_tx_bytestr_alias(tx, "request_method",
                                  tx->request_line->method);

        core_gen_tx_bytestr_alias(tx, "request_uri_raw",
                                  tx->request_line->uri);

        core_gen_tx_bytestr_alias(tx, "request_protocol",
                                  tx->request_line->protocol);
    }

    /* Populate the ARGS collection. */
    rc = core_slow_get(&f, tx, "ARGS");
    if (rc == IB_OK) {
        ib_field_t *param_list;

        rc = core_slow_get(&param_list, tx, "request_uri_params");
        if (rc == IB_OK) {
            ib_list_t *field_list;
            ib_list_node_t *node = NULL;

            rc = ib_field_mutable_value(
                param_list,
                ib_ftype_list_mutable_out(&field_list)
            );
            if (rc != IB_OK) {
                return rc;
            }

            IB_LIST_LOOP(field_list, node) {
                ib_field_t *param = (ib_field_t *)ib_list_node_data(node);

                /* Add the field to the ARGS collection. */
                rc = ib_field_list_add(f, param);
                if (rc != IB_OK) {
                    ib_log_notice_tx(tx,
                                     "Failed to add parameter to "
                                     "ARGS collection: %s",
                                     ib_status_to_string(rc));
                }
            }
        }
    }

    /* Create the aliased request header list */
    if (tx->request_header != NULL) {
        rc = create_header_alias_list(ib,
                                      tx,
                                      "request_headers",
                                      tx->request_header);
        if (rc != IB_OK) {
            return rc;
        }
    }

    return IB_OK;
}

/*
 * Callback used to generate request body fields.
 */
static ib_status_t core_gen_request_body_fields(ib_engine_t *ib,
                                                ib_tx_t *tx,
                                                ib_state_event_type_t event,
                                                void *cbdata)
{
    ib_field_t *f;
    ib_status_t rc;

    assert(ib != NULL);
    assert(tx != NULL);
    assert(event == request_finished_event);

    /* Populate the ARGS collection. */
    rc = core_slow_get(&f, tx, "ARGS");
    if (rc == IB_OK) {
        ib_field_t *param_list;

        /* Add request body parameters to ARGS collection. */
        rc = core_slow_get(&param_list, tx, "request_body_params");
        if (rc == IB_OK) {
            ib_list_t *field_list;
            ib_list_node_t *node = NULL;

            rc = ib_field_mutable_value(
                param_list,
                ib_ftype_list_mutable_out(&field_list)
            );
            if (rc != IB_OK) {
                return rc;
            }

            IB_LIST_LOOP(field_list, node) {
                ib_field_t *param = (ib_field_t *)ib_list_node_data(node);

                /* Add the field to the ARGS collection. */
                rc = ib_field_list_add(f, param);
                if (rc != IB_OK) {
                    ib_log_notice_tx(tx,
                                     "Failed to add parameter to "
                                     "ARGS collection: %s",
                                     ib_status_to_string(rc));
                }
            }
        }
    }

    return IB_OK;
}

/*
 * Callback used to generate response header fields.
 */
static ib_status_t core_gen_response_header_fields(
    ib_engine_t           *ib,
    ib_tx_t               *tx,
    ib_state_event_type_t  event,
    void                  *cbdata
)
{
    ib_status_t rc;

    assert(ib != NULL);
    assert(tx != NULL);
    assert(event == response_header_finished_event);

    if (tx->response_line != NULL) {
        core_gen_tx_bytestr_alias(tx, "response_line",
                                        tx->response_line->raw);

        core_gen_tx_bytestr_alias(tx, "response_protocol",
                                  tx->response_line->protocol);

        core_gen_tx_bytestr_alias(tx, "response_status",
                                  tx->response_line->status);

        core_gen_tx_bytestr_alias(tx, "response_message",
                                  tx->response_line->msg);
    }

    /* Create the aliased response header list */
    if (tx->response_header != NULL) {
        rc = create_header_alias_list(ib,
                                      tx,
                                      "response_headers",
                                      tx->response_header);
        if (rc != IB_OK) {
            return rc;
        }
    }

    return IB_OK;
}

/*
 * Callback used to generate response body fields.
 */
static ib_status_t core_gen_response_body_fields(ib_engine_t *ib,
                                                 ib_tx_t *tx,
                                                 ib_state_event_type_t event,
                                                 void *cbdata)
{
    assert(ib != NULL);
    assert(tx != NULL);
    assert(event == response_finished_event);

    return IB_OK;
}


/* -- Initialization Routines -- */

/* Initialize libhtp config object for the context. */
ib_status_t ib_core_vars_ctx_init(ib_engine_t *ib,
                                    ib_module_t *mod,
                                    ib_context_t *ctx,
                                    void *cbdata)
{
    ib_core_cfg_t *corecfg;
    ib_status_t rc;

    assert(ib != NULL);
    assert(mod != NULL);
    assert(ctx != NULL);

    /* Get the core context config. */
    rc = ib_context_module_config(ctx, mod, (void *)&corecfg);
    if (rc != IB_OK) {
        ib_log_alert(ib,
            "Failed to fetch core module context config: %s",
            ib_status_to_string(rc)
        );
        return rc;
    }

    return IB_OK;
}

/* Initialize core field generation callbacks. */
ib_status_t ib_core_vars_init(ib_engine_t *ib,
                              ib_module_t *mod)
{
    assert(ib != NULL);
    assert(mod != NULL);

    ib_var_config_t *config;
    ib_status_t rc;

    ib_hook_tx_register(ib, tx_started_event,
                        core_gen_placeholder_fields, NULL);

    ib_hook_tx_register(ib, tx_started_event,
                        core_gen_flags_collection, NULL);

    ib_hook_tx_register(ib, request_header_finished_event,
                        core_gen_request_header_fields, NULL);

    ib_hook_tx_register(ib, request_finished_event,
                        core_gen_request_body_fields, NULL);

    ib_hook_tx_register(ib, response_header_finished_event,
                        core_gen_response_header_fields, NULL);

    ib_hook_tx_register(ib, response_finished_event,
                        core_gen_response_body_fields, NULL);

    config = ib_engine_var_config_get(ib);
    assert(config != NULL);
    for (
        const char **key = indexed_keys;
        *key != NULL;
        ++key
    )
    {
        rc = ib_var_source_register(
            NULL,
            config,
            *key, strlen(*key),
            IB_PHASE_NONE, IB_PHASE_NONE
        );
        if (rc != IB_OK) {
            ib_log_warning(ib,
                "Core vars failed to register \"%s\": %s",
                *key,
                ib_status_to_string(rc)
            );
        }
        /* Do not abort.  Everything should still work, just be a little
         * slower.
         */
    }

    return IB_OK;
}

/* Get the core TX flags */
const ib_tx_flag_map_t *ib_core_vars_tx_flags( )
{
    return core_tx_flag_map;
}
