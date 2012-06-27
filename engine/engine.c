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
 * @brief IronBee
 *
 * @author Brian Rectanus <brectanus@qualys.com>
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include "ironbee_config_auto.h"

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>
#include <inttypes.h>

#include <sys/types.h> /* getpid */
#include <arpa/inet.h> /* htonl */
#include <unistd.h>

#include <ironbee/engine.h>
#include <ironbee/mpool.h>
#include <ironbee/hash.h>
#include <ironbee/cfgmap.h>
#include <ironbee/debug.h>
#include <ironbee/module.h>
#include <ironbee/core.h>
#include <ironbee/server.h>
#include <ironbee/state_notify.h>

#include "ironbee_private.h"
#include "engine_private.h"
#include "state_notify_private.h"

/* -- Constants -- */

/** Constant String Values */
const ib_default_string_t ib_default_string = {
    "",          /* empty */
    "unknown",   /* unknown */
    "core",      /* core */
    "/",         /* root_path */
    "/",         /* uri_root_path */
};


/* -- Internal Structures -- */

#if 0
typedef struct ib_field_callback_data_t ib_field_callback_data_t;
#endif

/**
 * List of callback data types for event id to type lookups.
 */
static const ib_state_hook_type_t ib_state_event_hook_types[] = {
    /* Engine States */
    IB_STATE_HOOK_CONN,     /**< conn_started_event */
    IB_STATE_HOOK_CONN,     /**< conn_finished_event */
    IB_STATE_HOOK_TX,       /**< tx_started_event */
    IB_STATE_HOOK_TX,       /**< tx_process_event */
    IB_STATE_HOOK_TX,       /**< tx_finished_event */

    /* Handler States */
    IB_STATE_HOOK_CONN,     /**< handle_context_conn_event */
    IB_STATE_HOOK_CONN,     /**< handle_connect_event */
    IB_STATE_HOOK_TX,       /**< handle_context_tx_event */
    IB_STATE_HOOK_TX,       /**< handle_request_header_event */
    IB_STATE_HOOK_TX,       /**< handle_request_event */
    IB_STATE_HOOK_TX,       /**< handle_response_header_event */
    IB_STATE_HOOK_TX,       /**< handle_response_event */
    IB_STATE_HOOK_CONN,     /**< handle_disconnect_event */
    IB_STATE_HOOK_TX,       /**< handle_postprocess_event */

    /* Server States */
    IB_STATE_HOOK_NULL,     /**< cfg_started_event */
    IB_STATE_HOOK_NULL,     /**< cfg_finished_event */
    IB_STATE_HOOK_CONN,     /**< conn_opened_event */
    IB_STATE_HOOK_CONNDATA, /**< conn_data_in_event */
    IB_STATE_HOOK_CONNDATA, /**< conn_data_out_event */
    IB_STATE_HOOK_CONN,     /**< conn_closed_event */

    /* Parser States */
    IB_STATE_HOOK_REQLINE,  /**< request_started_event */
    IB_STATE_HOOK_HEADER,   /**< request_header_data_event */
    IB_STATE_HOOK_TX,       /**< request_header_finished_event */
    IB_STATE_HOOK_TXDATA,   /**< request_body_data_event */
    IB_STATE_HOOK_TX,       /**< request_finished_event */
    IB_STATE_HOOK_RESPLINE, /**< response_started_event */
    IB_STATE_HOOK_HEADER,   /**< response_header_data_event */
    IB_STATE_HOOK_TX,       /**< response_header_finished_event */
    IB_STATE_HOOK_TXDATA,   /**< response_body_data_event */
    IB_STATE_HOOK_TX        /**< response_finished_event */
};

/* -- Internal Routines -- */

ib_status_t ib_check_hook(
    ib_engine_t* ib,
    ib_state_event_type_t event,
    ib_state_hook_type_t hook_type
) {
    IB_FTRACE_INIT();
    static const size_t num_events =
        sizeof(ib_state_event_hook_types) / sizeof(ib_state_hook_type_t);
    ib_state_hook_type_t expected_hook_type;

    if (event > num_events) {
        ib_log_error( ib,
            "Event/hook mismatch: Unknown event type: %d", event
        );
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    expected_hook_type = ib_state_event_hook_types[event];
    if ( expected_hook_type != hook_type ) {
        ib_log_debug(ib,
                     "Event/hook mismatch: "
                     "Event type %s expected %d but received %d",
                     ib_state_event_name(event),
                     expected_hook_type, hook_type);
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

static ib_status_t ib_register_hook(
    ib_engine_t* ib,
    ib_state_event_type_t event,
    ib_hook_t* hook
) {
    IB_FTRACE_INIT();

    ib_hook_t *last = ib->hook[event];

    /* Insert the hook at the end of the list */
    if (last == NULL) {
        ib_log_debug3(ib, "Registering %s hook: %p",
               ib_state_event_name(event),
               hook->callback.as_void);

        ib->hook[event] = hook;

        IB_FTRACE_RET_STATUS(IB_OK);
    }
    while (last->next != NULL) {
        last = last->next;
    }

    last->next = hook;

    ib_log_debug3(ib, "Registering %s hook after %p: %p",
           ib_state_event_name(event), last->callback.as_void,
           hook->callback.as_void);

    IB_FTRACE_RET_STATUS(IB_OK);
}

static ib_status_t ib_unregister_hook(
    ib_engine_t* ib,
    ib_state_event_type_t event,
    ib_void_fn_t cb
) {
    IB_FTRACE_INIT();
    ib_hook_t *prev = NULL;
    ib_hook_t *hook = ib->hook[event];

    /* Remove the first matching hook */
    while (hook != NULL) {
        if (hook->callback.as_void == cb) {
            if (prev == NULL) {
                ib->hook[event] = hook->next;
            }
            else {
                prev->next = hook->next;
            }
            IB_FTRACE_RET_STATUS(IB_OK);
        }
        prev = hook;
        hook = hook->next;
    }

    IB_FTRACE_RET_STATUS(IB_ENOENT);
}

/* -- Main Engine Routines -- */

ib_status_t ib_engine_create(ib_engine_t **pib, ib_server_t *server)
{
    IB_FTRACE_INIT();
    ib_mpool_t *pool;
    ib_status_t rc;

    /* Create primary memory pool */
    rc = ib_mpool_create(&pool, "Engine", NULL);
    if (rc != IB_OK) {
        rc = IB_EALLOC;
        goto failed;
    }

    /* Create the main structure in the primary memory pool */
    *pib = (ib_engine_t *)ib_mpool_calloc(pool, 1, sizeof(**pib));
    if (*pib == NULL) {
        rc = IB_EALLOC;
        goto failed;
    }
    (*pib)->mp = pool;

    /* Create temporary memory pool */
    /// @todo Need to tune the pool size
    rc = ib_mpool_create_ex(&((*pib)->temp_mp),
                            "Engine/Temp",
                            (*pib)->mp,
                            8192);
    if (rc != IB_OK) {
        goto failed;
    }

    /* Create the config memory pool */
    /// @todo Need to tune the pool size
    rc = ib_mpool_create_ex(&((*pib)->config_mp),
                            "Engine/Config",
                            (*pib)->mp,
                            8192);
    if (rc != IB_OK) {
        goto failed;
    }

    /* Create an array to hold config contexts */
    /// @todo Need good defaults here
    rc = ib_array_create(&((*pib)->contexts), (*pib)->mp, 16, 16);
    if (rc != IB_OK) {
        goto failed;
    }

    /* Create an engine config context and use it as the
     * main context until the engine can be configured.
     */
    rc = ib_context_create(&((*pib)->ectx), *pib, NULL, "engine", "main",
                           NULL, NULL, NULL);
    if (rc != IB_OK) {
        goto failed;
    }
    (*pib)->ctx = (*pib)->ectx;

    /* Check server for ABI compatibility with this engine */
    if (server == NULL) {
        ib_log_error(*pib,  "Error in ib_create: server info required");
        rc = IB_EINVAL;
        goto failed;
    }
    if (server->vernum > IB_VERNUM) {
        ib_log_alert(*pib,
                     "Server %s (built against engine version %s) is not "
                     "compatible with this engine (version %s): "
                     "ABI %d > %d",
                     server->filename, server->version, IB_VERSION, server->abinum, IB_ABINUM);
        rc = IB_EINCOMPAT;
        goto failed;
    }
    (*pib)->server = server;

    /* Sensor info. */
    (*pib)->sensor_name = IB_DSTR_UNKNOWN;
    (*pib)->sensor_version = IB_PRODUCT_VERSION_NAME;
    (*pib)->sensor_hostname = IB_DSTR_UNKNOWN;

    /* Create an array to hold loaded modules */
    /// @todo Need good defaults here
    rc = ib_array_create(&((*pib)->modules), (*pib)->mp, 16, 8);
    if (rc != IB_OK) {
        goto failed;
    }

    /* Create an array to hold filters */
    /// @todo Need good defaults here
    rc = ib_array_create(&((*pib)->filters), (*pib)->mp, 16, 8);
    if (rc != IB_OK) {
        goto failed;
    }

    /* Create a hash to hold configuration directive mappings by name */
    rc = ib_hash_create_nocase(&((*pib)->dirmap), (*pib)->mp);
    if (rc != IB_OK) {
        goto failed;
    }

    /* Create a hash to hold provider apis by name */
    rc = ib_hash_create_nocase(&((*pib)->apis), (*pib)->mp);
    if (rc != IB_OK) {
        goto failed;
    }

    /* Create a hash to hold providers by name */
    rc = ib_hash_create_nocase(&((*pib)->providers), (*pib)->mp);
    if (rc != IB_OK) {
        goto failed;
    }

    /* Create a hash to hold transformations by name */
    rc = ib_hash_create_nocase(&((*pib)->tfns), (*pib)->mp);
    if (rc != IB_OK) {
        goto failed;
    }

    /* Create a hash to hold operators by name */
    rc = ib_hash_create_nocase(&((*pib)->operators), (*pib)->mp);
    if (rc != IB_OK) {
        goto failed;
    }

    /* Create a hash to hold actions by name */
    rc = ib_hash_create_nocase(&((*pib)->actions), (*pib)->mp);
    if (rc != IB_OK) {
        goto failed;
    }

    /* Initialize the core static module. */
    /// @todo Probably want to do this in a less hard-coded manner.
    rc = ib_module_init(ib_core_module(), *pib);
    if (rc != IB_OK) {
        ib_log_alert(*pib,  "Error in ib_module_init");
        goto failed;
    }

    IB_FTRACE_RET_STATUS(rc);

failed:
    /* Make sure everything is cleaned up on failure */
    if (*pib != NULL) {
        ib_mpool_destroy((*pib)->mp);
    }
    *pib = NULL;

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_engine_init(ib_engine_t *ib)
{
    IB_FTRACE_INIT();
    ib_status_t rc;

    rc = ib_context_open(ib->ectx);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = ib_context_close(ib->ectx);
    IB_FTRACE_RET_STATUS(rc);
}

/* Create a main context to operate in. */
ib_status_t ib_engine_context_create_main(ib_engine_t *ib)
{
    IB_FTRACE_INIT();
    ib_context_t *ctx;
    ib_status_t rc;

    rc = ib_context_create(&ctx, ib, ib->ectx,
                           "main", "main",
                           NULL, NULL, NULL);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    ib->ctx = ctx;

    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t ib_engine_module_get(ib_engine_t *ib,
                                 const char * name,
                                 ib_module_t **pm)
{
    IB_FTRACE_INIT();
    size_t n;
    size_t i;
    ib_module_t *m;

    /* Return the first module matching the name. */
    IB_ARRAY_LOOP(ib->modules, n, i, m) {
        if (strcmp(name, m->name) == 0) {
            *pm = m;
            IB_FTRACE_RET_STATUS(IB_OK);
        }
    }

    *pm = NULL;

    IB_FTRACE_RET_STATUS(IB_ENOENT);
}

ib_mpool_t *ib_engine_pool_main_get(ib_engine_t *ib)
{
    IB_FTRACE_INIT();
    IB_FTRACE_RET_PTR(ib_mpool_t, ib->mp);
}

ib_mpool_t *ib_engine_pool_config_get(ib_engine_t *ib)
{
    IB_FTRACE_INIT();
    IB_FTRACE_RET_PTR(ib_mpool_t, ib->mp);
}

ib_mpool_t *ib_engine_pool_temp_get(ib_engine_t *ib)
{
    IB_FTRACE_INIT();
    IB_FTRACE_RET_PTR(ib_mpool_t, ib->temp_mp);
}

void ib_engine_pool_temp_destroy(ib_engine_t *ib)
{
    IB_FTRACE_INIT();
    ib_mpool_destroy(ib->temp_mp);
    ib->temp_mp = NULL;
    IB_FTRACE_RET_VOID();
}

void ib_engine_destroy(ib_engine_t *ib)
{
    IB_FTRACE_INIT();
    if (ib) {
        size_t ne;
        size_t idx;
        ib_context_t *ctx;
        ib_module_t *cm = ib_core_module();
        ib_module_t *m;

        /// @todo Destroy filters

        ib_log_debug3(ib, "Destroying configuration contexts...");
        IB_ARRAY_LOOP_REVERSE(ib->contexts, ne, idx, ctx) {
            if (   (ctx != ib->ctx)
                && (ctx != ib->ectx) )
            {
                ib_context_destroy(ctx);
            }
        }
        if (ib->ctx != ib->ectx) {
            ib_log_debug3(ib, "Destroying main configuration context...");
            ib_context_destroy(ib->ctx);
            ib->ctx = NULL;
        }
        ib_log_debug3(ib, "Destroying engine configuration context...");
        ib_context_destroy(ib->ectx);
        ib->ectx = ib->ctx = NULL;

        ib_log_debug3(ib, "Unloading modules...");
        IB_ARRAY_LOOP_REVERSE(ib->modules, ne, idx, m) {
            if (m != cm) {
                ib_module_unload(m);
            }
        }

        ib_log_debug3(ib, "Destroy IB handle (%d,%d,%s,%s): %p",
               ib->server->vernum, ib->server->abinum,
               ib->server->filename, ib->server->name, ib);

        ib_mpool_destroy(ib->mp);
    }
    IB_FTRACE_RET_VOID();
}

ib_status_t ib_conn_create(ib_engine_t *ib,
                           ib_conn_t **pconn, void *server_ctx)
{
    IB_FTRACE_INIT();
    ib_mpool_t *pool;
    ib_status_t rc;
    char namebuf[64];

    /* Create a sub-pool for each connection and allocate from it */
    /// @todo Need to tune the pool size
    rc = ib_mpool_create_ex(&pool, "Connection", ib->mp, 2048);
    if (rc != IB_OK) {
        ib_log_alert(ib, "Failed to create connection memory pool: %s", ib_status_to_string(rc));
        rc = IB_EALLOC;
        goto failed;
    }
    *pconn = (ib_conn_t *)ib_mpool_calloc(pool, 1, sizeof(**pconn));
    if (*pconn == NULL) {
        ib_log_alert(ib, "Failed to allocate memory for connection");
        rc = IB_EALLOC;
        goto failed;
    }

    /* Mark time. */
    (*pconn)->t.started = ib_clock_get_time();

    /* Name the connection pool */
    snprintf(namebuf, sizeof(namebuf), "Connection/%p", (void *)(*pconn));
    ib_mpool_setname(pool, namebuf);

    (*pconn)->ib = ib;
    (*pconn)->mp = pool;
    (*pconn)->ctx = ib->ctx;
    (*pconn)->server_ctx = server_ctx;

    rc = ib_hash_create_nocase(&((*pconn)->data), (*pconn)->mp);
    if (rc != IB_OK) {
        rc = IB_EALLOC;
        goto failed;
    }

    IB_FTRACE_RET_STATUS(IB_OK);

failed:
    /* Make sure everything is cleaned up on failure */
    if (*pconn != NULL) {
        ib_mpool_destroy((*pconn)->mp);
    }
    *pconn = NULL;

    IB_FTRACE_RET_STATUS(rc);
}

void ib_conn_parser_context_set(ib_conn_t *conn,
                                void *parser_ctx)
{
    IB_FTRACE_INIT();
    assert(conn != NULL);

    conn->parser_ctx = parser_ctx;

    IB_FTRACE_RET_VOID();
}

void *ib_conn_parser_context_get(ib_conn_t *conn)
{
    IB_FTRACE_INIT();
    assert(conn != NULL);

    IB_FTRACE_RET_PTR(void, conn->parser_ctx);
}


ib_status_t ib_conn_data_create(ib_conn_t *conn,
                                ib_conndata_t **pconndata,
                                size_t dalloc)
{
    IB_FTRACE_INIT();
    ib_engine_t *ib = conn->ib;
    ib_mpool_t *pool;
    ib_status_t rc;

    /* Create a sub-pool for data buffers */
    /// @todo Need to tune the pool size
    rc = ib_mpool_create_ex(&pool, NULL, conn->mp, 8192);
    if (rc != IB_OK) {
        ib_log_alert(ib,
                     "Failed to create connection data memory pool: %s", ib_status_to_string(rc));
        rc = IB_EALLOC;
        goto failed;
    }
    *pconndata = (ib_conndata_t *)ib_mpool_calloc(pool, 1, sizeof(**pconndata));
    if (*pconndata == NULL) {
        ib_log_alert(ib, "Failed to allocate memory for connection data");
        rc = IB_EALLOC;
        goto failed;
    }

    (*pconndata)->conn = conn;

    (*pconndata)->dlen = 0;
    (*pconndata)->data = (uint8_t *)ib_mpool_calloc(pool, 1, dalloc);
    if ((*pconndata)->data == NULL) {
        ib_log_alert(ib, "Failed to allocate memory for connection data buffer");
        rc = IB_EALLOC;
        goto failed;
    }

    IB_FTRACE_RET_STATUS(IB_OK);

failed:
    /* Make sure everything is cleaned up on failure */
    *pconndata = NULL;

    IB_FTRACE_RET_STATUS(rc);
}

void ib_conn_destroy(ib_conn_t *conn)
{
    /// @todo Probably need to update state???
    if ( conn != NULL && conn->mp != NULL ) {
        ib_mpool_destroy(conn->mp);
        /* Don't do this: conn->mp = NULL; conn is now freed memory! */
    }
}

ib_status_t ib_tx_generate_id(ib_tx_t *tx,
                              ib_mpool_t *mp)
{
    IB_FTRACE_INIT();

    ib_uuid_t uuid;
    ib_status_t rc;
    char *str;

    rc = ib_uuid_create_v4(&uuid);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Convert to a hex-string representation */
    str = (char *)ib_mpool_alloc(mp, IB_UUID_HEX_SIZE);
    if (str == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }
    tx->id = str;

    rc = ib_uuid_bin_to_ascii(str, &uuid);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t ib_tx_create(ib_tx_t **ptx,
                         ib_conn_t *conn,
                         void *sctx)
{
    IB_FTRACE_INIT();
    ib_mpool_t *pool;
    ib_status_t rc;
    char namebuf[64];
    ib_tx_t *tx = NULL;
    ib_core_cfg_t *corecfg;

    ib_engine_t *ib = conn->ib;

    rc = ib_context_module_config(
        ib->ctx,
        ib_core_module(),
        (void *)&corecfg
    );

    if ( rc != IB_OK ) {
        ib_log_alert(ib, "Failed to retrieve core module configuration.");
    }

    /* Create a sub-pool from the connection memory pool for each
     * transaction and allocate from it
     */
    /// @todo Need to tune the pool size
    rc = ib_mpool_create_ex(&pool, NULL, conn->mp, 8192);
    if (rc != IB_OK) {
        ib_log_alert(ib, "Failed to create transaction memory pool: %s", ib_status_to_string(rc));
        rc = IB_EALLOC;
        goto failed;
    }
    tx = (ib_tx_t *)ib_mpool_calloc(pool, 1, sizeof(*tx));
    if (tx == NULL) {
        ib_log_alert(ib, "Failed to allocate memory for transaction");
        rc = IB_EALLOC;
        goto failed;
    }

    /* Name the transaction pool */
    snprintf(namebuf, sizeof(namebuf), "TX/%p", (void *)tx);
    ib_mpool_setname(pool, namebuf);

    tx->t.started = ib_clock_get_time();
    tx->ib = ib;
    tx->mp = pool;
    tx->ctx = ib->ctx;
    tx->sctx = sctx;
    tx->conn = conn;
    tx->er_ipstr = conn->remote_ipstr;
    tx->hostname = IB_DSTR_EMPTY;
    tx->path = IB_DSTR_URI_ROOT_PATH;
    tx->block_status = corecfg->block_status;

    conn->tx_count++;
    ib_tx_generate_id(tx, tx->mp);

    /* Create the generic data store. */
    rc = ib_hash_create_nocase(&(tx->data), tx->mp);
    if (rc != IB_OK) {
        rc = IB_EALLOC;
        goto failed;
    }

    /* Create a filter controller. */
    rc = ib_fctl_tx_create(&(tx->fctl), tx, tx->mp);
    if (rc != IB_OK) {
        goto failed;
    }

    /**
     * After this, we have generally succeeded and are now outputting
     * the transaction to the conn object and the ptx pointer.
     */

    /* Add transaction to the connection list */
    if (conn->tx_first == NULL) {
        conn->tx_first = tx;
        conn->tx = tx;
        conn->tx_last = tx;
        ib_log_debug3_tx(tx, "First transaction: %p", tx);
    }
    else {
        conn->tx = tx;
        conn->tx_last->next = tx;
        conn->tx_last = tx;

        /* If there are more than one transactions, then this is a pipeline
         * request and needs to be marked as such.
         */
        if (conn->tx_first->next == tx) {
            ib_tx_flags_set(conn->tx_first, IB_TX_FPIPELINED);
        }
        ib_tx_flags_set(tx, IB_TX_FPIPELINED);

        ib_log_debug3_tx(tx, "Found a pipelined transaction: %p", tx);
    }

    /* Only when we are successful, commit changes to output variable. */
    *ptx = tx;
    ib_log_debug3_tx(tx, "TX CREATE p=%p id=%s", tx, tx->id);

    IB_FTRACE_RET_STATUS(IB_OK);

failed:
    /* Make sure everything is cleaned up on failure */
    if (tx != NULL) {
        ib_mpool_destroy(tx->mp);
    }
    tx = NULL;

    IB_FTRACE_RET_STATUS(rc);
}

void ib_tx_destroy(ib_tx_t *tx)
{
    ib_tx_t *curr;

    /// @todo It should always be the first one in the list,
    ///       so this should not be needed and should cause an error
    ///       or maybe for us to throw a flag???
    assert(tx != NULL);
    assert(tx->conn != NULL);
    assert(tx->conn->tx_first == tx);

    ib_log_debug3_tx(tx, "TX DESTROY p=%p id=%s", tx, tx->id);

    /* Keep track of the first/current tx. */
    tx->conn->tx_first = tx->next;
    tx->conn->tx = tx->next;

    for (curr = tx->conn->tx_first; curr != NULL; curr = curr->next) {
        if (curr == tx) {
            curr->next = curr->next ? curr->next->next : NULL;
            break;
        }
    }

    /* Keep track of the last tx. */
    if (tx->conn->tx_last == tx) {
        tx->conn->tx_last = NULL;
    }

    /// @todo Probably need to update state???
    ib_mpool_destroy(tx->mp);
}

ib_status_t ib_site_create(ib_site_t **psite,
                           ib_engine_t *ib,
                           const char *name)
{
    IB_FTRACE_INIT();
    ib_mpool_t *pool = ib->config_mp;
    ib_status_t rc;

    /* Create the main structure in the config memory pool */
    *psite = (ib_site_t *)ib_mpool_calloc(pool, 1, sizeof(**psite));
    if (*psite == NULL) {
        rc = IB_EALLOC;
        IB_FTRACE_RET_STATUS(rc);
    }
    (*psite)->ib = ib;
    (*psite)->mp = pool;
    (*psite)->name = ib_mpool_strdup(pool, name);


    /* Remaining fields are NULL via calloc. */

    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t ib_site_address_add(ib_site_t *site,
                                const char *ip)
{
    IB_FTRACE_INIT();
    ib_status_t rc;

    /* Create a list if this is the first item. */
    if (site->ips == NULL) {
        rc = ib_list_create(&site->ips, site->mp);
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }
    }

    /// @todo: use regex
    rc = ib_list_push(site->ips, (void *)ip);
    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_site_address_validate(ib_site_t *site,
                                     const char *ip)
{
    IB_FTRACE_INIT();
    IB_FTRACE_RET_STATUS(IB_ENOTIMPL);
}

ib_status_t ib_site_hostname_add(ib_site_t *site,
                                 const char *host)
{
    IB_FTRACE_INIT();
    ib_status_t rc;

    /* Create a list if this is the first item. */
    if (site->hosts == NULL) {
        rc = ib_list_create(&site->hosts, site->mp);
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }
    }

    /// @todo: use regex
    rc = ib_list_push(site->hosts, (void *)host);
    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_site_hostname_validate(ib_site_t *site,
                                      const char *host)
{
    IB_FTRACE_INIT();
    IB_FTRACE_RET_STATUS(IB_ENOTIMPL);
}

ib_status_t ib_site_loc_create(ib_site_t *site,
                               ib_loc_t **ploc,
                               const char *path)
{
    IB_FTRACE_INIT();
    ib_loc_t *loc;
    ib_status_t rc;

    if (ploc != NULL) {
        *ploc = NULL;
    }

    /* Create a list if this is the first item. */
    if (site->locations == NULL) {
        rc = ib_list_create(&site->locations, site->mp);
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }
    }

    /* Create the location structure in the site memory pool */
    loc = (ib_loc_t *)ib_mpool_calloc(site->mp, 1, sizeof(*loc));
    if (loc == NULL) {
        rc = IB_EALLOC;
        IB_FTRACE_RET_STATUS(rc);
    }
    loc->site = site;
    loc->path = path;
    loc->path = ib_mpool_strdup(site->mp, path);

    if (ploc != NULL) {
        *ploc = loc;
    }

    rc = ib_list_push(site->locations, (void *)loc);
    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_site_loc_create_default(ib_site_t *site,
                                       ib_loc_t **ploc)
{
    IB_FTRACE_INIT();
    ib_loc_t *loc;
    ib_status_t rc;

    if (ploc != NULL) {
        *ploc = NULL;
    }

    /* Create the location structure in the site memory pool */
    loc = (ib_loc_t *)ib_mpool_calloc(site->mp, 1, sizeof(*loc));
    if (loc == NULL) {
        rc = IB_EALLOC;
        IB_FTRACE_RET_STATUS(rc);
    }
    loc->site = site;

    /* This is empty vs normal IB_DSTR_URI_ROOT_PATH so that we accept
     * any path, including relative paths.
     */
    loc->path = IB_DSTR_EMPTY;

    if (ploc != NULL) {
        *ploc = loc;
    }

    site->default_loc = loc;
    IB_FTRACE_RET_STATUS(IB_OK);
}

/* -- State Routines -- */

/**
 * List of state names for id to name lookups.
 *
 * @warning Remember to update ib_state_event_type_t.
 *
 */
static const char *ib_state_event_name_list[] = {
    /* Engine States */
    IB_STRINGIFY(conn_started_event),
    IB_STRINGIFY(conn_finished_event),
    IB_STRINGIFY(tx_started_event),
    IB_STRINGIFY(tx_process_event),
    IB_STRINGIFY(tx_finished_event),

    /* Handler States */
    IB_STRINGIFY(handle_context_conn_event),
    IB_STRINGIFY(handle_connect_event),
    IB_STRINGIFY(handle_context_tx_event),
    IB_STRINGIFY(handle_request_header_event),
    IB_STRINGIFY(handle_request_event),
    IB_STRINGIFY(handle_response_header_event),
    IB_STRINGIFY(handle_response_event),
    IB_STRINGIFY(handle_disconnect_event),
    IB_STRINGIFY(handle_postprocess_event),

    /* Server States */
    IB_STRINGIFY(cfg_started_event),
    IB_STRINGIFY(cfg_finished_event),
    IB_STRINGIFY(conn_opened_event),
    IB_STRINGIFY(conn_data_in_event),
    IB_STRINGIFY(conn_data_out_event),
    IB_STRINGIFY(conn_closed_event),

    /* Parser States */
    IB_STRINGIFY(request_started_event),
    IB_STRINGIFY(request_header_data_event),
    IB_STRINGIFY(request_header_finished_event),
    IB_STRINGIFY(request_body_data_event),
    IB_STRINGIFY(request_finished_event),
    IB_STRINGIFY(response_started_event),
    IB_STRINGIFY(response_header_data_event),
    IB_STRINGIFY(response_header_finished_event),
    IB_STRINGIFY(response_body_data_event),
    IB_STRINGIFY(response_finished_event),

    NULL
};

const char *ib_state_event_name(ib_state_event_type_t event)
{
    IB_FTRACE_INIT();
    IB_FTRACE_RET_CONSTSTR(ib_state_event_name_list[event]);
}
/* -- Hook Routines -- */

ib_state_hook_type_t ib_state_hook_type(ib_state_event_type_t event)
{
    static const size_t num_events =
        sizeof(ib_state_event_hook_types) / sizeof(ib_state_hook_type_t);

    if (event > num_events) {
        return IB_STATE_HOOK_INVALID;
    }

    return ib_state_event_hook_types[event];
}

ib_status_t DLL_PUBLIC ib_hook_null_register(
    ib_engine_t *ib,
    ib_state_event_type_t event,
    ib_state_null_hook_fn_t cb,
    void *cdata
) {
    IB_FTRACE_INIT();
    ib_status_t rc;

    rc = ib_check_hook(ib, event, IB_STATE_HOOK_NULL);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    ib_hook_t *hook = (ib_hook_t *)ib_mpool_alloc(ib->mp, sizeof(*hook));
    if (hook == NULL) {
        ib_log_emergency(ib, "Error in ib_mpool_calloc");
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    hook->callback.null = cb;
    hook->cdata = cdata;
    hook->next = NULL;

    rc = ib_register_hook(ib, event, hook);

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t DLL_PUBLIC ib_null_hook_unregister(
    ib_engine_t *ib,
    ib_state_event_type_t event,
    ib_state_null_hook_fn_t cb
) {
    IB_FTRACE_INIT();
    ib_status_t rc;

    rc = ib_check_hook(ib, event, IB_STATE_HOOK_NULL);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = ib_unregister_hook(ib, event, (ib_void_fn_t)cb);

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t DLL_PUBLIC ib_hook_conn_register(
    ib_engine_t *ib,
    ib_state_event_type_t event,
    ib_state_conn_hook_fn_t cb,
    void *cdata
) {
    IB_FTRACE_INIT();
    ib_status_t rc;

    rc = ib_check_hook(ib, event, IB_STATE_HOOK_CONN);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    ib_hook_t *hook = (ib_hook_t *)ib_mpool_alloc(ib->mp, sizeof(*hook));
    if (hook == NULL) {
        ib_log_emergency(ib, "Error in ib_mpool_calloc");
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    hook->callback.conn = cb;
    hook->cdata = cdata;
    hook->next = NULL;

    rc = ib_register_hook(ib, event, hook);

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t DLL_PUBLIC ib_conn_hook_unregister(
    ib_engine_t *ib,
    ib_state_event_type_t event,
    ib_state_conn_hook_fn_t cb
) {
    IB_FTRACE_INIT();
    ib_status_t rc;

    rc = ib_check_hook(ib, event, IB_STATE_HOOK_CONN);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = ib_unregister_hook(ib, event, (ib_void_fn_t)cb);

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t DLL_PUBLIC ib_hook_conndata_register(
    ib_engine_t *ib,
    ib_state_event_type_t event,
    ib_state_conndata_hook_fn_t cb,
    void *cdata
) {
    IB_FTRACE_INIT();
    ib_status_t rc;

    rc = ib_check_hook(ib, event, IB_STATE_HOOK_CONNDATA);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    ib_hook_t *hook = (ib_hook_t *)ib_mpool_alloc(ib->mp, sizeof(*hook));
    if (hook == NULL) {
        ib_log_emergency(ib, "Error in ib_mpool_calloc");
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    hook->callback.conndata = cb;
    hook->cdata = cdata;
    hook->next = NULL;

    rc = ib_register_hook(ib, event, hook);

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t DLL_PUBLIC ib_conndata_hook_unregister(
    ib_engine_t *ib,
    ib_state_event_type_t event,
    ib_state_conndata_hook_fn_t cb
) {
    IB_FTRACE_INIT();
    ib_status_t rc;

    rc = ib_check_hook(ib, event, IB_STATE_HOOK_CONNDATA);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = ib_unregister_hook(ib, event, (ib_void_fn_t)cb);

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t DLL_PUBLIC ib_hook_tx_register(
    ib_engine_t *ib,
    ib_state_event_type_t event,
    ib_state_tx_hook_fn_t cb,
    void *cdata
) {
    IB_FTRACE_INIT();
    ib_status_t rc;

    rc = ib_check_hook(ib, event, IB_STATE_HOOK_TX);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    ib_hook_t *hook = (ib_hook_t *)ib_mpool_alloc(ib->mp, sizeof(*hook));
    if (hook == NULL) {
        ib_log_emergency(ib, "Error in ib_mpool_calloc");
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    hook->callback.tx = cb;
    hook->cdata = cdata;
    hook->next = NULL;

    rc = ib_register_hook(ib, event, hook);

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t DLL_PUBLIC ib_tx_hook_unregister(
    ib_engine_t *ib,
    ib_state_event_type_t event,
    ib_state_tx_hook_fn_t cb
) {
    IB_FTRACE_INIT();

    ib_status_t rc;

    rc = ib_check_hook(ib, event, IB_STATE_HOOK_TX);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = ib_unregister_hook(ib, event, (ib_void_fn_t)cb);

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t DLL_PUBLIC ib_hook_txdata_register(
    ib_engine_t *ib,
    ib_state_event_type_t event,
    ib_state_txdata_hook_fn_t cb,
    void *cdata
) {
    IB_FTRACE_INIT();
    ib_status_t rc;

    rc = ib_check_hook(ib, event, IB_STATE_HOOK_TXDATA);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    ib_hook_t *hook = (ib_hook_t *)ib_mpool_alloc(ib->mp, sizeof(*hook));
    if (hook == NULL) {
        ib_log_emergency(ib, "Error in ib_mpool_calloc");
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    hook->callback.txdata = cb;
    hook->cdata = cdata;
    hook->next = NULL;

    rc = ib_register_hook(ib, event, hook);

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t DLL_PUBLIC ib_txdata_hook_unregister(
    ib_engine_t *ib,
    ib_state_event_type_t event,
    ib_state_txdata_hook_fn_t cb
) {
    IB_FTRACE_INIT();
    ib_status_t rc;

    rc = ib_check_hook(ib, event, IB_STATE_HOOK_TXDATA);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = ib_unregister_hook(ib, event, (ib_void_fn_t)cb);

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t DLL_PUBLIC ib_hook_parsed_header_data_register(
    ib_engine_t *ib,
    ib_state_event_type_t event,
    ib_state_header_data_fn_t cb,
    void *cdata)
{
    IB_FTRACE_INIT();
    ib_status_t rc;

    rc = ib_check_hook(ib, event, IB_STATE_HOOK_HEADER);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    ib_hook_t *hook = (ib_hook_t *)ib_mpool_alloc(ib->mp, sizeof(*hook));
    if (hook == NULL) {
        ib_log_emergency(ib, "Error in ib_mpool_calloc");
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    hook->callback.headerdata = cb;
    hook->cdata = cdata;
    hook->next = NULL;

    rc = ib_register_hook(ib, event, hook);

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t DLL_PUBLIC ib_hook_parsed_header_data_unregister(
    ib_engine_t *ib,
    ib_state_event_type_t event,
    ib_state_header_data_fn_t cb)
{
    IB_FTRACE_INIT();
    ib_status_t rc;

    rc = ib_check_hook(ib, event, IB_STATE_HOOK_HEADER);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = ib_unregister_hook(ib, event, (ib_void_fn_t)cb);

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t DLL_PUBLIC ib_hook_parsed_req_line_register(
    ib_engine_t *ib,
    ib_state_event_type_t event,
    ib_state_request_line_fn_t cb,
    void *cdata)
{
    IB_FTRACE_INIT();
    ib_status_t rc;

    rc = ib_check_hook(ib, event, IB_STATE_HOOK_REQLINE);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    ib_hook_t *hook = (ib_hook_t *)ib_mpool_alloc(ib->mp, sizeof(*hook));
    if (hook == NULL) {
        ib_log_emergency(ib, "Error in ib_mpool_calloc");
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    hook->callback.requestline = cb;
    hook->cdata = cdata;
    hook->next = NULL;

    rc = ib_register_hook(ib, event, hook);

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t DLL_PUBLIC ib_hook_parsed_req_line_unregister(
    ib_engine_t *ib,
    ib_state_event_type_t event,
    ib_state_request_line_fn_t cb)
{
    IB_FTRACE_INIT();
    ib_status_t rc;

    rc = ib_check_hook(ib, event, IB_STATE_HOOK_REQLINE);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = ib_unregister_hook(ib, event, (ib_void_fn_t)cb);

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t DLL_PUBLIC ib_hook_parsed_resp_line_register(
    ib_engine_t *ib,
    ib_state_event_type_t event,
    ib_state_response_line_fn_t cb,
    void *cdata)
{
    IB_FTRACE_INIT();

    ib_status_t rc;

    rc = ib_check_hook(ib, event, IB_STATE_HOOK_RESPLINE);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    ib_hook_t *hook = (ib_hook_t *)ib_mpool_alloc(ib->mp, sizeof(*hook));
    if (hook == NULL) {
        ib_log_emergency(ib, "Error in ib_mpool_calloc");
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    hook->callback.responseline = cb;
    hook->cdata = cdata;
    hook->next = NULL;

    rc = ib_register_hook(ib, event, hook);

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t DLL_PUBLIC ib_hook_parsed_resp_line_unregister(
    ib_engine_t *ib,
    ib_state_event_type_t event,
    ib_state_response_line_fn_t cb)
{
    IB_FTRACE_INIT();

    ib_status_t rc;

    rc = ib_check_hook(ib, event, IB_STATE_HOOK_RESPLINE);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = ib_unregister_hook(ib, event, (ib_void_fn_t)cb);

    IB_FTRACE_RET_STATUS(rc);
}


/* -- Connection Handling -- */

/* -- Transaction Handling -- */

/* -- Configuration Contexts -- */

ib_status_t ib_context_create(ib_context_t **pctx,
                              ib_engine_t *ib,
                              ib_context_t *parent,
                              const char *ctx_type,
                              const char *ctx_name,
                              ib_context_fn_t fn_ctx,
                              ib_context_site_fn_t fn_ctx_site,
                              void *fn_ctx_data)
{
    IB_FTRACE_INIT();
    ib_mpool_t *pool;
    ib_status_t rc;
    ib_context_t *ctx = NULL;
    char *full;
    size_t full_len;

    /* Create memory subpool */
    /// @todo Should we be doing this???
    rc = ib_mpool_create(&pool, NULL, ib->mp);
    if (rc != IB_OK) {
        rc = IB_EALLOC;
        goto failed;
    }

    /* Create the main structure */
    ctx = (ib_context_t *)ib_mpool_calloc(pool, 1, sizeof(*ctx));
    if (ctx == NULL) {
        rc = IB_EALLOC;
        goto failed;
    }

    ctx->ib = ib;
    ctx->mp = pool;
    ctx->parent = parent;
    ctx->ctx_type = ctx_type;
    ctx->ctx_name = ctx_name;
    ctx->fn_ctx = fn_ctx;
    ctx->fn_ctx_site = fn_ctx_site;
    ctx->fn_ctx_data = fn_ctx_data;

    /* Generate the full name of the context */
    full_len = 2;
    if (ctx_type != NULL) {
        full_len += strlen(ctx_type);
    }
    if (ctx_name != NULL) {
        full_len += strlen(ctx_name);
    }
    full = (char *)ib_mpool_alloc(pool, full_len);
    if (full == NULL) {
        rc = IB_EALLOC;
        goto failed;
    }
    if (ctx_type != NULL) {
        strcpy(full, ctx_type);
    }
    else {
        *full = '\0';
    }
    strcat(full, "/");
    if (ctx_name != NULL) {
        strcat(full, ctx_name);
    }
    ctx->ctx_full = full;

    /* Create a cfgmap to hold the configuration */
    rc = ib_cfgmap_create(&(ctx->cfg), ctx->mp);
    if (rc != IB_OK) {
        goto failed;
    }

    /* Create an array to hold the module config data */
    rc = ib_array_create(&(ctx->cfgdata), ctx->mp, 16, 8);
    if (rc != IB_OK) {
        goto failed;
    }

    /* Create a list to hold the enabled filters */
    rc = ib_list_create(&(ctx->filters), ctx->mp);
    if (rc != IB_OK) {
        goto failed;
    }

    rc = ib_array_appendn(ib->contexts, ctx);
    if (rc != IB_OK) {
        goto failed;
    }

    rc = ib_context_set_auditlog_index(ctx, "ironbee-index.log");
    if (rc != IB_OK) {
        goto failed;
    }

    /* Register the modules */
    /// @todo Later on this needs to be triggered by ActivateModule or similar
    if (ib->modules) {
        ib_module_t *m;
        size_t n;
        size_t i;
        IB_ARRAY_LOOP(ib->modules, n, i, m) {
            ib_log_debug3(ib, "Registering module=\"%s\" idx=%zd",
                          m->name, m->idx);
            rc = ib_module_register_context(m, ctx);
            if (rc != IB_OK) {
                goto failed;
            }
        }
    }
    else {
        /* Register the core module by default. */
        rc = ib_module_register_context(ib_core_module(), ctx);
        if (rc != IB_OK) {
            goto failed;
        }
    }

    /* Commit the new ctx to pctx. */
    *pctx = ctx;

    IB_FTRACE_RET_STATUS(IB_OK);

failed:
    /* Make sure everything is cleaned up on failure */
    if (ctx != NULL) {
        ib_mpool_destroy(ctx->mp);
    }

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_context_open(ib_context_t *ctx)
{
    IB_FTRACE_INIT();
    ib_engine_t *ib = ctx->ib;
    ib_context_data_t *cfgdata;
    ib_status_t rc;
    size_t ncfgdata;
    size_t i;

    ib_log_debug3(ib, "Opening context ctx=%p '%s'", ctx, ctx->ctx_full);

    IB_ARRAY_LOOP(ctx->cfgdata, ncfgdata, i, cfgdata) {
        if (cfgdata == NULL) {
            continue;
        }
        ib_module_t *m = cfgdata->module;

        if (m->fn_ctx_open != NULL) {
            rc = m->fn_ctx_open(ib, m, ctx, m->cbdata_ctx_open);
            if (rc != IB_OK) {
                /// @todo Log the error???  Fail???
                ib_log_error(ib, "Failed to call context open: %s", ib_status_to_string(rc));
                IB_FTRACE_RET_STATUS(rc);
            }
        }
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t ib_context_set_auditlog_index(ib_context_t *ctx, const char* idx)
{
    IB_FTRACE_INIT();

    ib_status_t rc;

    assert(ctx != NULL);
    assert(ctx->ib != NULL);
    assert(ctx->mp != NULL);
    assert(idx != NULL);

    /* Check if a new audit log structure must be allocated:
     *   1. if auditlog == NULL or
     *   2. if the allocated audit log belongs to another context we may
     *      not change its auditlog->index value (or auditlog->index_fp).
     *      We must make a new auditlog that the passed in ib_context_t
     *      ctx owns.  */
    if (ctx->auditlog == NULL || ctx->auditlog->owner != ctx)
    {

        ctx->auditlog = (ib_auditlog_cfg_t *)
            ib_mpool_calloc(ctx->mp, 1, sizeof(*ctx->auditlog));

        if (ctx->auditlog == NULL) {
            IB_FTRACE_RET_STATUS(IB_EALLOC);
        }

        /* Set owner. */
        ctx->auditlog->owner = ctx;

        /* Set index_fp_lock. */
        rc = ib_lock_init(&ctx->auditlog->index_fp_lock);

        if (rc!=IB_OK) {
            ib_log_debug2(ctx->ib,
                "Failed to initialize lock for audit index %s", idx);

            IB_FTRACE_RET_STATUS(rc);
        }

        /* Set index. */
        ctx->auditlog->index = ib_mpool_strdup(ctx->mp, idx);

        if (ctx->auditlog->index == NULL) {
            IB_FTRACE_RET_STATUS(IB_EALLOC);
        }
    }
    /* Else the auditlog struct is initialized and owned by this ctx. */
    else {
        rc = ib_lock_lock(&ctx->auditlog->index_fp_lock);

        if (rc!=IB_OK) {
            ib_log_debug2(ctx->ib, "Failed lock to audit index %s", idx);
            IB_FTRACE_RET_STATUS(rc);
        }

        /* Check that we aren't re-setting a value in the same context. */
        if ( ! strcmp(idx, ctx->auditlog->index) ) {

            ib_lock_unlock(&ctx->auditlog->index_fp_lock);

            ib_log_debug2(ctx->ib,
                         "Re-setting log same value. No action: %s",
                         idx);

            IB_FTRACE_RET_STATUS(IB_OK);
        }

        /* Replace the old index value with the new index value. */
        ctx->auditlog->index = ib_mpool_strdup(ctx->mp, idx);

        if (ctx->auditlog->index == NULL) {
            IB_FTRACE_RET_STATUS(IB_EALLOC);
        }

        /* Fail on alloc error. */
        if (ctx->auditlog->index == NULL) {
            ib_lock_unlock(&ctx->auditlog->index_fp_lock);
            IB_FTRACE_RET_STATUS(IB_EALLOC);
        }

        /* Close the audit log file if it is open. */
        if (ctx->auditlog->index_fp != NULL) {
            fclose(ctx->auditlog->index_fp);
            ctx->auditlog->index_fp = NULL;
        }

        ib_lock_unlock(&ctx->auditlog->index_fp_lock);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t ib_context_close(ib_context_t *ctx)
{
    IB_FTRACE_INIT();
    ib_engine_t *ib = ctx->ib;
    ib_context_data_t *cfgdata;
    ib_status_t rc;
    size_t ncfgdata;
    size_t i;

    ib_log_debug3(ib, "Closing context ctx=%p '%s'", ctx, ctx->ctx_full);

    IB_ARRAY_LOOP(ctx->cfgdata, ncfgdata, i, cfgdata) {
        if (cfgdata == NULL) {
            continue;
        }
        ib_module_t *m = cfgdata->module;

        if (m->fn_ctx_close != NULL) {
            rc = m->fn_ctx_close(ib, m, ctx, m->cbdata_ctx_close);
            if (rc != IB_OK) {
                /// @todo Log the error???  Fail???
                ib_log_error(ib, "Failed to call context init: %s", ib_status_to_string(rc));
                IB_FTRACE_RET_STATUS(rc);
            }
        }
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_context_t *ib_context_parent_get(const ib_context_t *ctx)
{
    IB_FTRACE_INIT();
    IB_FTRACE_RET_PTR(ib_context_t, ctx->parent);
}

void ib_context_parent_set(ib_context_t *ctx,
                           ib_context_t *parent)
{
    IB_FTRACE_INIT();
    ctx->parent = parent;
    IB_FTRACE_RET_VOID();
}

ib_site_t *ib_context_site_get(const ib_context_t *ctx)
{
    IB_FTRACE_INIT();
    ib_status_t rc;
    ib_site_t *site;

    ib_log_debug2(ctx->ib, "ctx=%p '%s'; fn_ctx_site=%p",
                  ctx, ctx->ctx_full, ctx->fn_ctx_site);

    if (ctx->fn_ctx_site == NULL) {
        IB_FTRACE_RET_PTR(ib_site_t, NULL);
    }

    /* Call the registered site lookup function. */
    rc = ctx->fn_ctx_site(ctx, &site, ctx->fn_ctx_data);
    if (rc != IB_OK) {
        IB_FTRACE_RET_PTR(ib_site_t, NULL);
    }

    IB_FTRACE_RET_PTR(ib_site_t, site);
}

const char *ib_context_type_get(const ib_context_t *ctx)
{
    IB_FTRACE_INIT();
    assert(ctx != NULL);

    if (ctx->ctx_type == NULL) {
        IB_FTRACE_RET_CONSTSTR("");
    }
    else {
        IB_FTRACE_RET_CONSTSTR(ctx->ctx_type);
    }
}

const char *ib_context_name_get(const ib_context_t *ctx)
{
    IB_FTRACE_INIT();
    assert(ctx != NULL);

    if (ctx->ctx_name == NULL) {
        IB_FTRACE_RET_CONSTSTR("");
    }
    else {
        IB_FTRACE_RET_CONSTSTR(ctx->ctx_name);
    }
}

const char *ib_context_full_get(const ib_context_t *ctx)
{
    IB_FTRACE_INIT();
    assert(ctx != NULL);

    IB_FTRACE_RET_CONSTSTR(ctx->ctx_full);
}

void ib_context_destroy(ib_context_t *ctx)
{
    IB_FTRACE_INIT();
    ib_engine_t *ib;
    ib_context_data_t *cfgdata;
    ib_status_t rc;
    size_t ncfgdata, i;

    if (ctx == NULL) {
        IB_FTRACE_RET_VOID();
    }

    ib = ctx->ib;

    ib_log_debug3(ib, "Destroying context ctx=%p '%s'", ctx, ctx->ctx_full);

    /* Run through the context modules to call any ctx_fini functions. */
    /// @todo Not sure this is needed anymore
    IB_ARRAY_LOOP(ctx->cfgdata, ncfgdata, i, cfgdata) {
        if (cfgdata == NULL) {
            continue;
        }
        ib_module_t *m = cfgdata->module;

        if (m->fn_ctx_destroy != NULL) {
            ib_log_debug3(ib,
                          "Finishing context ctx=%p '%s' for module=%s (%p)",
                          ctx, ctx->ctx_full, m->name, m);
            rc = m->fn_ctx_destroy(ib, m, ctx, m->cbdata_ctx_destroy);
            if (rc != IB_OK) {
                /// @todo Log the error???  Fail???
                ib_log_error(ib, "Failed to call context fini: %s", ib_status_to_string(rc));
            }
        }
    }

    ib_mpool_destroy(ctx->mp);

    IB_FTRACE_RET_VOID();
}

ib_context_t *ib_context_engine(const ib_engine_t *ib)
{
    return ib->ectx;
}

ib_context_t *ib_context_main(const ib_engine_t *ib)
{
    return ib->ctx;
}

ib_engine_t *ib_context_get_engine(const ib_context_t *ctx)
{
    return ctx->ib;
}

ib_status_t ib_context_init_cfg(ib_context_t *ctx,
                                void *base,
                                const ib_cfgmap_init_t *init)
{
    IB_FTRACE_INIT();
    ib_status_t rc;

    ib_log_debug3(ctx->ib, "Initializing context %s base=%p", ib_context_full_get(ctx), base);

    if (init == NULL) {
        IB_FTRACE_RET_STATUS(IB_OK);
    }

    rc = ib_cfgmap_init(ctx->cfg, base, init);

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_context_module_config(ib_context_t *ctx,
                                     ib_module_t *m,
                                     void *pcfg)
{
    IB_FTRACE_INIT();
    ib_context_data_t *cfgdata;
    ib_status_t rc;

    rc = ib_array_get(ctx->cfgdata, m->idx, (void *)&cfgdata);
    if (rc != IB_OK) {
        *(void **)pcfg = NULL;
        IB_FTRACE_RET_STATUS(rc);
    }

    if (cfgdata == NULL) {
        *(void **)pcfg = NULL;
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    *(void **)pcfg = cfgdata->data;

    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t ib_context_set(ib_context_t *ctx,
                           const char *name,
                           void *val)
{
    IB_FTRACE_INIT();
    ib_status_t rc = ib_cfgmap_set(ctx->cfg, name, val);
    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_context_set_num(ib_context_t *ctx,
                               const char *name,
                               ib_num_t val)
{
    IB_FTRACE_INIT();
    ib_status_t rc = ib_cfgmap_set(ctx->cfg, name, ib_ftype_num_in(&val));
    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_context_set_string(ib_context_t *ctx,
                                  const char *name,
                                  const char *val)
{
    IB_FTRACE_INIT();
    ib_status_t rc = ib_cfgmap_set(ctx->cfg, name, ib_ftype_nulstr_in(val));
    IB_FTRACE_RET_STATUS(rc);
}


ib_status_t ib_context_get(ib_context_t *ctx,
                           const char *name,
                           void *pval, ib_ftype_t *ptype)
{
    IB_FTRACE_INIT();
    ib_status_t rc = ib_cfgmap_get(ctx->cfg, name, pval, ptype);
    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_context_siteloc_chooser(const ib_context_t *ctx,
                                       ib_ctype_t type,
                                       void *ctxdata,
                                       void *cbdata)
{
    IB_FTRACE_INIT();
    ib_loc_t *loc;
    ib_tx_t *tx;
    size_t numips;
    ib_list_node_t *ipnode;
    const char *ip;
    size_t numhosts;
    ib_list_node_t *hostnode;
    const char *host;
    const char *txhost;
    size_t txhostlen;
    const char *path;
    const char *txpath;

    if (type != IB_CTYPE_TX) {
        /// @todo Perhaps we should attempt to find a single site if it is
        ///       a connection and use it if there is only one choice???
        IB_FTRACE_RET_STATUS(IB_DECLINED);
    }
    if (cbdata == NULL) {
        /// @todo No site/location associated with this context
        IB_FTRACE_RET_STATUS(IB_DECLINED);
    }

    tx = (ib_tx_t *)ctxdata;
    loc = (ib_loc_t *)cbdata;
    txhost = tx->hostname;
    txhostlen = strlen(txhost);
    txpath = tx->path;

    ib_log_debug3_tx(tx, "CHOOSER: ctx=%p '%s' tx=%p loc=%p",
                  ctx, ctx->ctx_full, tx, loc);

    /*
     * Check for a matching IP address, then a matching hostname and
     * finally a matching path. If one of the IP, host or location lists
     * is NULL, then this means ANY and should always match.
     */
    /// @todo This needs to be MUCH more efficient!!!
    numips = loc->site->ips ? ib_list_elements(loc->site->ips) : 1;
    ipnode = loc->site->ips ? ib_list_first(loc->site->ips) : NULL;
    ip = ipnode ? (const char *)ib_list_node_data(ipnode) : NULL;
    while (numips--) {
        /// @todo IP should be IP:Port combo
        ib_log_debug2_tx(tx, "Checking IP %s against context %s",
                     tx->conn->local_ipstr, ip?ip:"ANY");
        if ((ip == NULL) || (strcmp(ip, tx->conn->local_ipstr) == 0)) {
            numhosts = loc->site->hosts ? ib_list_elements(loc->site->hosts) : 1;
            hostnode = loc->site->hosts ? ib_list_first(loc->site->hosts) : NULL;
            host = hostnode ? (const char *)ib_list_node_data(hostnode) : NULL;

            while (numhosts--) {
                size_t hostlen = host?strlen(host):0;
                off_t cmpoffset = txhostlen - hostlen;
                const char *cmphost = (cmpoffset >= 0)?txhost + cmpoffset:NULL;
                if (cmphost != NULL) {
                    ib_log_debug2_tx(tx, "Checking Host \"%s\" (effective=\"%s\") against context %s",
                                 txhost, cmphost, (host&&*host)?host:"ANY");
                    if ((host == NULL) || (strcmp(host, cmphost) == 0)) {
                        path = loc->path;

                        ib_log_debug2_tx(tx,
                                      "Checking Location path '%s' "
                                      "against context (%s) path '%s'",
                                      txpath, ctx->ctx_full, path?path:"ANY");

                        if ((path == NULL) || (strncmp(path, txpath, strlen(path)) == 0)) {
                            ib_log_debug2_tx(tx,
                                          "Site \"%s:%s\" matched ctx=%p '%s'",
                                          loc->site->name, loc->path,
                                          ctx, ctx->ctx_full);
                            IB_FTRACE_RET_STATUS(IB_OK);
                        }
                    }
                }
                else if ((cmphost == NULL) &&
                         ((host == NULL) || (*host == '\0')))
                {
                    /* Match default (wildcard) site for missing host. */
                    ib_log_debug2_tx(tx,
                                  "Site \"%s:%s\" matched ctx=%p '%s'",
                                  loc->site->name, loc->path,
                                  ctx, ctx->ctx_full);
                    IB_FTRACE_RET_STATUS(IB_OK);
                }
                else {
                    ib_log_debug2_tx(tx,
                                  "Skipping Host \"%s\" "
                                  "check against context %s '%s'",
                                  txhost, host?host:"ANY", ctx->ctx_full);
                }
                if (numhosts > 0) {
                    hostnode = ib_list_node_next(hostnode);
                    host = hostnode ? (const char *)ib_list_node_data(hostnode) : NULL;
                }
            }
        }
        if (numips > 0) {
            ipnode = ib_list_node_next(ipnode);
            ip = ipnode ? (const char *)ib_list_node_data(ipnode) : NULL;
        }
    }

    IB_FTRACE_RET_STATUS(IB_ENOENT);
}

ib_status_t ib_context_site_lookup(const ib_context_t *ctx,
                                   ib_site_t **psite,
                                   void *cbdata)
{
    IB_FTRACE_INIT();
    ib_loc_t *loc;

    if (cbdata == NULL) {
        /// @todo No site/location associated with this context
        IB_FTRACE_RET_STATUS(IB_DECLINED);
    }

    loc = (ib_loc_t *)cbdata;
    if (psite != NULL) {
        *psite = loc->site;
    }

    if (loc->site != NULL) {
        IB_FTRACE_RET_STATUS(IB_OK);
    }

    IB_FTRACE_RET_STATUS(IB_ENOENT);
}
