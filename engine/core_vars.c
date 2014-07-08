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
#include <ironbee/string.h>
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
typedef struct {
    const char *name;
    ib_rule_phase_num_t initial;
    ib_rule_phase_num_t final;
} indexed_key_t;
indexed_key_t indexed_keys[] = {
{"ARGS",                  IB_PHASE_REQUEST_HEADER,  IB_PHASE_REQUEST},
{"FLAGS",                 IB_PHASE_NONE,            IB_PHASE_NONE},
{"auth_password",         IB_PHASE_REQUEST_HEADER,  IB_PHASE_REQUEST_HEADER},
{"auth_type",             IB_PHASE_REQUEST_HEADER,  IB_PHASE_REQUEST_HEADER},
{"auth_username",         IB_PHASE_REQUEST_HEADER,  IB_PHASE_REQUEST_HEADER},
{"request_body_params",   IB_PHASE_REQUEST,         IB_PHASE_REQUEST},
{"request_content_type",  IB_PHASE_REQUEST_HEADER,  IB_PHASE_REQUEST_HEADER},
{"request_cookies",       IB_PHASE_REQUEST_HEADER,  IB_PHASE_REQUEST_HEADER},
{"request_filename",      IB_PHASE_REQUEST_HEADER,  IB_PHASE_REQUEST_HEADER},
{"request_headers",       IB_PHASE_REQUEST_HEADER,  IB_PHASE_REQUEST_HEADER},
{"request_host",          IB_PHASE_REQUEST_HEADER,  IB_PHASE_REQUEST_HEADER},
{"request_line",          IB_PHASE_REQUEST_HEADER,  IB_PHASE_REQUEST_HEADER},
{"request_method",        IB_PHASE_REQUEST_HEADER,  IB_PHASE_REQUEST_HEADER},
{"request_protocol",      IB_PHASE_REQUEST_HEADER,  IB_PHASE_REQUEST_HEADER},
{"request_uri",           IB_PHASE_REQUEST_HEADER,  IB_PHASE_REQUEST_HEADER},
{"request_uri_fragment",  IB_PHASE_REQUEST_HEADER,  IB_PHASE_REQUEST_HEADER},
{"request_uri_host",      IB_PHASE_REQUEST_HEADER,  IB_PHASE_REQUEST_HEADER},
{"request_uri_params",    IB_PHASE_REQUEST_HEADER,  IB_PHASE_REQUEST_HEADER},
{"request_uri_password",  IB_PHASE_REQUEST_HEADER,  IB_PHASE_REQUEST_HEADER},
{"request_uri_path",      IB_PHASE_REQUEST_HEADER,  IB_PHASE_REQUEST_HEADER},
{"request_uri_path_raw",  IB_PHASE_REQUEST_HEADER,  IB_PHASE_REQUEST_HEADER},
{"request_uri_port",      IB_PHASE_REQUEST_HEADER,  IB_PHASE_REQUEST_HEADER},
{"request_uri_query",     IB_PHASE_REQUEST_HEADER,  IB_PHASE_REQUEST_HEADER},
{"request_uri_raw",       IB_PHASE_REQUEST_HEADER,  IB_PHASE_REQUEST_HEADER},
{"request_uri_scheme",    IB_PHASE_REQUEST_HEADER,  IB_PHASE_REQUEST_HEADER},
{"request_uri_username",  IB_PHASE_REQUEST_HEADER,  IB_PHASE_REQUEST_HEADER},
{"response_content_type", IB_PHASE_RESPONSE_HEADER, IB_PHASE_RESPONSE_HEADER},
{"response_cookies",      IB_PHASE_RESPONSE_HEADER, IB_PHASE_RESPONSE_HEADER},
{"response_headers",      IB_PHASE_RESPONSE_HEADER, IB_PHASE_RESPONSE_HEADER},
{"response_line",         IB_PHASE_RESPONSE_HEADER, IB_PHASE_RESPONSE_HEADER},
{"response_message",      IB_PHASE_RESPONSE_HEADER, IB_PHASE_RESPONSE_HEADER},
{"response_protocol",     IB_PHASE_RESPONSE_HEADER, IB_PHASE_RESPONSE_HEADER},
{"response_status",       IB_PHASE_RESPONSE_HEADER, IB_PHASE_RESPONSE_HEADER}
};

static const ib_tx_flag_map_t core_tx_flag_map[] = {
    {
        .name          = "suspicious",
        .tx_name       = "FLAGS:suspicious",
        .tx_flag       = IB_TX_FSUSPICIOUS,
        .read_only     = false,
        .default_value = false
    },
    {
        .name          = "inspectRequestHeader",
        .tx_name       = "FLAGS:inspectRequestHeader",
        .tx_flag       = IB_TX_FINSPECT_REQHDR,
        .read_only     = false,
        .default_value = false
    },
    {
        .name          = "inspectRequestBody",
        .tx_name       = "FLAGS:inspectRequestBody",
        .tx_flag       = IB_TX_FINSPECT_REQBODY,
        .read_only     = false,
        .default_value = false
    },
    {
        .name          = "inspectResponseHeader",
        .tx_name       = "FLAGS:inspectResponseHeader",
        .tx_flag       = IB_TX_FINSPECT_RESHDR,
        .read_only     = false,
        .default_value = false
    },
    {
        .name          = "inspectResponseBody",
        .tx_name       = "FLAGS:inspectResponseBody",
        .tx_flag       = IB_TX_FINSPECT_RESBODY,
        .read_only     = false,
        .default_value = false
    },
    {
        .name          = "inspectRequestParams",
        .tx_name       = "FLAGS:inspectRequestParams",
        .tx_flag       = IB_TX_FINSPECT_REQPARAMS,
        .read_only     = false,
        .default_value = false
    },
    {
        .name          = "inspectRequestUri",
        .tx_name       = "FLAGS:inspectRequestUri",
        .tx_flag       = IB_TX_FINSPECT_REQURI,
        .read_only     = false,
        .default_value = false
    },
    {
        .name          = "blockingMode",
        .tx_name       = "FLAGS:blockingMode",
        .tx_flag       = IB_TX_FBLOCKING_MODE,
        .read_only     = false,
        .default_value = false
    },
    {
        .name          = "block",
        .tx_name       = "FLAGS:block",
        .tx_flag       = IB_TX_FBLOCK_ADVISORY,
        .read_only     = false,
        .default_value = false
    },

    /* End */
    { NULL, NULL, IB_TX_FNONE, true, false },
};

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
            tx->mm,
            name, strlen(name),
            IB_FTYPE_BYTESTR,
            val
    );
    if (rc != IB_OK) {
        ib_log_notice_tx(tx, "Error creating \"%s\" var: %s",
                         name, ib_status_to_string(rc));
        return;
    }

    rc = ib_var_source_acquire(
        &source,
        tx->mm,
        ib_var_store_config(tx->var_store),
        name, strlen(name)
    );
    if (rc != IB_OK) {
        ib_log_notice_tx(tx, "Error acquiring \"%s\" var: %s",
                         name, ib_status_to_string(rc));
        return;
    }

    rc = ib_var_source_set(source, tx->var_store, f);
    if (rc != IB_OK) {
        ib_log_notice_tx(tx,
            "Error adding \"%s\" var to transaction: %s",
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
        tx->mm,
        (const uint8_t *)val,
        val_length
    );
    if (rc != IB_OK) {
        ib_log_notice_tx(tx, "Error creating alias for \"%s\" var: %s",
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

    rc = ib_field_create(&f, tx->mm,
                         name, strlen(name),
                         IB_FTYPE_NUM,
                         &num);
    if (rc != IB_OK) {
        ib_log_notice_tx(tx, "Error creating \"%s\" field: %s",
                         name, ib_status_to_string(rc));
        return;
    }

    rc = ib_var_source_acquire(
        &source,
        tx->mm,
        ib_var_store_config(tx->var_store),
        name, strlen(name)
    );
    if (rc != IB_OK) {
        ib_log_notice_tx(tx, "Error acquiring \"%s\" var: %s",
                         name, ib_status_to_string(rc));
        return;
    }

    rc = ib_var_source_set(source, tx->var_store, f);
    if (rc != IB_OK) {
        ib_log_notice_tx(tx,
            "Error adding \"%s\" var to transaction: %s",
            name, ib_status_to_string(rc)
        );
    }
}

/* -- Hooks -- */

static ib_status_t core_gen_flags_collection(ib_engine_t *ib,
                                             ib_tx_t *tx,
                                             ib_state_t state,
                                             void *cbdata)
{
    assert(ib != NULL);
    assert(tx != NULL);
    assert(tx->var_store != NULL);
    assert(state == tx_started_state);

    const ib_tx_flag_map_t *flag;

    for (flag = ib_core_vars_tx_flags();  flag->name != NULL;  ++flag) {
        if (tx->flags & flag->tx_flag) {
            ib_tx_flags_set(tx, flag->tx_flag);
        }
        else {
            ib_tx_flags_unset(tx, flag->tx_flag);
        }
    }

    return IB_OK;
}

/**
 * Generate early var values.
 *
 * This are typically all @ref ib_conn_t values exposed through vars.
 *
 * @param[in] ib Engine.
 * @param[in] tx Transaction.
 * @param[in] state The @ref tx_started_state.
 * @param[in] cbdata Callback data. Unused.
 *
 * @returns
 * - IB_OK On success.
 * - Other on failure.
 */
static ib_status_t core_gen_early_var_sources(ib_engine_t *ib,
                                              ib_tx_t *tx,
                                              ib_state_t state,
                                              void *cbdata)
{
    assert(ib != NULL);
    assert(tx != NULL);
    assert(tx->conn != NULL);
    assert(tx->var_store != NULL);
    assert(state == tx_started_state);

    ib_conn_t *conn = tx->conn;

    core_gen_tx_bytestr_alias2(tx, "server_addr", IB_S2SL(conn->local_ipstr));
    core_gen_tx_numeric(tx, "server_port", conn->local_port);
    core_gen_tx_bytestr_alias2(tx, "remote_addr", IB_S2SL(conn->remote_ipstr));
    core_gen_tx_numeric(tx, "remote_port", conn->remote_port);
    core_gen_tx_numeric(tx, "conn_tx_count", tx->conn->tx_count);

    return IB_OK;
}

static ib_status_t core_slow_get_collection(
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
        ib_var_store_mm(tx->var_store),
        ib_var_store_config(tx->var_store),
        name, strlen(name)
    );
    if (rc != IB_OK) {
        return rc;
    }
    rc = ib_var_source_get(source, &value, tx->var_store);
    if (rc == IB_ENOENT) {
        rc = ib_var_source_initialize(
            source,
            &value,
            tx->var_store,
            IB_FTYPE_LIST
        );
    }
    if (rc != IB_OK) {
        return rc;
    }

    if (value == NULL) {
        return IB_EOTHER;
    }

    if (value->type != IB_FTYPE_LIST) {
        return IB_EINVAL;
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
    ib_parsed_headers_t *header)
{
    ib_field_t *f;
    ib_list_t *header_list;
    ib_status_t rc;
    ib_parsed_header_t *nvpair;
    ib_var_source_t *source;

    assert(ib != NULL);
    assert(tx != NULL);
    assert(name != NULL);
    assert(header != NULL);

    /* Create the list */
    rc = ib_var_source_acquire(
        &source,
        ib_var_store_mm(tx->var_store),
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
                tx->mm,
                ib_bytestr_ptr(nvpair->value),
                ib_bytestr_length(nvpair->value)
            );
        }
        else {
            rc = ib_bytestr_dup_mem(&bs, tx->mm, (const uint8_t *)"", 0);
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
            tx->mm,
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
                                                  ib_state_t state,
                                                  void *cbdata)
{
    ib_field_t *f;
    ib_status_t rc;

    assert(ib != NULL);
    assert(tx != NULL);

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
    rc = core_slow_get_collection(&f, tx, "ARGS");
    if (rc == IB_OK) {
        ib_field_t *param_list;

        rc = core_slow_get_collection(&param_list, tx, "request_uri_params");
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
                                     "Error adding parameter to "
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
                                                ib_state_t state,
                                                void *cbdata)
{
    ib_field_t *f;
    ib_status_t rc;

    assert(ib != NULL);
    assert(tx != NULL);

    /* Populate the ARGS collection. */
    rc = core_slow_get_collection(&f, tx, "ARGS");
    if (rc == IB_OK) {
        ib_field_t *param_list;

        /* Add request body parameters to ARGS collection. */
        rc = core_slow_get_collection(&param_list, tx, "request_body_params");
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
                                     "Error adding parameter to "
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
    ib_state_t  event,
    void                  *cbdata
)
{
    ib_status_t rc;

    assert(ib != NULL);
    assert(tx != NULL);

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
                                                 ib_state_t state,
                                                 void *cbdata)
{
    assert(ib != NULL);
    assert(tx != NULL);

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
            "Error fetching core module context config: %s",
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

    ib_hook_tx_register(ib, tx_started_state,
                        core_gen_flags_collection, NULL);

    ib_hook_tx_register(ib, tx_started_state,
                        core_gen_early_var_sources, NULL);

    ib_hook_tx_register(ib, request_header_finished_state,
                        core_gen_request_header_fields, NULL);

    ib_hook_tx_register(ib, handle_request_state,
                        core_gen_request_body_fields, NULL);

    ib_hook_tx_register(ib, handle_response_header_state,
                        core_gen_response_header_fields, NULL);

    ib_hook_tx_register(ib, handle_response_state,
                        core_gen_response_body_fields, NULL);

    config = ib_engine_var_config_get(ib);
    assert(config != NULL);
    for (size_t i = 0; i < sizeof(indexed_keys)/sizeof(*indexed_keys); ++i) {
        indexed_key_t key_info = indexed_keys[i];
        rc = ib_var_source_register(
            NULL,
            config,
            IB_S2SL(key_info.name),
            key_info.initial, key_info.final
        );
        if (rc != IB_OK) {
            ib_log_notice(ib,
                "Error registering core var \"%s\": %s",
                key_info.name,
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
