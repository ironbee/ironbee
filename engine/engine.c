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
 */

#include "ironbee_config_auto.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include <sys/time.h> /* gettimeofday */
#include <sys/types.h> /* getpid */
#include <unistd.h>



#include <ironbee/engine.h>
#include <ironbee/util.h>
#include <ironbee/module.h>
#include <ironbee/plugin.h>

#include "ironbee_private.h"


/* -- Internal Structures -- */

#if 0
typedef struct ib_field_callback_data_t ib_field_callback_data_t;
#endif


/* -- Internal Routines -- */

/**
 * @internal
 * Find the config context by executing context functions.
 *
 * @param ib Engine
 * @param type Context type
 * @param data Data (type based on context type)
 * @param pctx Address which context is written
 *
 * @returns Status code
 */
static ib_status_t _ib_context_get(ib_engine_t *ib,
                                   ib_ctype_t type,
                                   void *data,
                                   ib_context_t **pctx)
{
    IB_FTRACE_INIT(_ib_context_get);
    ib_context_t *ctx;
    ib_status_t rc;
    size_t nctx, i;

    *pctx = NULL;

    /* Run through the config context functions to select the context. */
    IB_ARRAY_LOOP(ib->contexts, nctx, i, ctx) {
        ib_log_debug(ib, 9, "Processing context %d=%p", (int)i, ctx);
        /* A NULL function is a null context, so skip it */
        if ((ctx == NULL) || (ctx->fn_ctx == NULL)) {
            continue;
        }

        rc = ctx->fn_ctx(ctx, type, data, ctx->fn_ctx_data);
        if (rc == IB_OK) {
            ib_log_debug(ib, 9, "Selected context %d=%p", (int)i, ctx);
            *pctx = ctx;
            break;
        }
        else if (rc != IB_DECLINED) {
            /// @todo Log the error???
        }
    }
    if (*pctx == NULL) {
        ib_log_debug(ib, 9, "Using engine context");
        *pctx = ib_context_main(ib);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}


/* -- Main Engine Routines -- */

ib_status_t ib_engine_create(ib_engine_t **pib, void *plugin)
{
    IB_FTRACE_INIT(ib_create);
    ib_mpool_t *pool;
    ib_plugin_t *p = (ib_plugin_t *)plugin;
    ib_status_t rc;

    /* Create primary memory pool */
    rc = ib_mpool_create(&pool, NULL);
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
    rc = ib_mpool_create(&((*pib)->temp_mp), pool);
    if (rc != IB_OK) {
        goto failed;
    }

    /* Create the config memory pool */
    rc = ib_mpool_create(&((*pib)->config_mp), (*pib)->mp);
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
    rc = ib_context_create(&((*pib)->ectx), *pib, NULL, NULL, NULL);
    if (rc != IB_OK) {
        goto failed;
    }
    (*pib)->ctx = (*pib)->ectx;

    /* Check plugin for ABI compatibility with this engine */
    if (p == NULL) {
        ib_log_error(*pib, 1, "Error in ib_create: plugin info required");
        rc = IB_EINVAL;
        goto failed;
    }
    if (p->vernum > IB_VERNUM) {
        ib_log_error(*pib, 0,
                     "Plugin %s (built against engine version %s) is not "
                     "compatible with this engine (version %s): "
                     "ABI %d > %d",
                     p->filename, p->version, IB_VERSION, p->abinum, IB_ABINUM);
        rc = IB_EINCOMPAT;
        goto failed;
    }
    (*pib)->plugin = p;

    /* Sensor info. */
    /// @todo Fetch real values
    (*pib)->sensor_id = "SensorId";
    (*pib)->sensor_version = IB_PRODUCT_NAME "/" IB_VERSION " "
                             "(embedded; PluginName/1.2.3)";
    (*pib)->sensor_hostname = "sensor.hostname.com";

    /* Create an array to hold loaded modules */
    /// @todo Need good defaults here
    rc = ib_array_create(&((*pib)->modules), (*pib)->mp, 16, 8);
    if (rc != IB_OK) {
        goto failed;
    }

    /* Create a hash to hold configuration directive mappings by name */
    rc = ib_hash_create(&((*pib)->dirmap), (*pib)->mp);
    if (rc != IB_OK) {
        goto failed;
    }

    /* Create a hash to hold provider apis by name */
    rc = ib_hash_create(&((*pib)->apis), (*pib)->mp);
    if (rc != IB_OK) {
        goto failed;
    }

    /* Create a hash to hold providers by name */
    rc = ib_hash_create(&((*pib)->providers), (*pib)->mp);
    if (rc != IB_OK) {
        goto failed;
    }

    /* Create a hash to hold transformations by name */
    rc = ib_hash_create(&((*pib)->tfns), (*pib)->mp);
    if (rc != IB_OK) {
        goto failed;
    }

    /* Initialize the core static module. */
    /// @todo Probably want to do this in a less hard-coded manner.
    rc = ib_module_init(ib_core_module(), *pib);
    if (rc != IB_OK) {
        ib_log_error(*pib, 0, "Error in ib_module_init");
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
    IB_FTRACE_INIT(ib_init);
    ib_status_t rc = ib_context_init(ib->ectx);
    IB_FTRACE_RET_STATUS(rc);
}

static ib_status_t ib_engine_context_create_main(ib_engine_t *ib)
{
    IB_FTRACE_INIT(ib_engine_context_create_main);
    ib_context_t *ctx;
    ib_status_t rc;
    
    rc = ib_context_create(&ctx, ib, ib->ectx, NULL, NULL);
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
    IB_FTRACE_INIT(ib_engine_module_get);
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
    IB_FTRACE_INIT(ib_engine_pool_main_get);
    IB_FTRACE_RET_PTR(ib_mpool_t, ib->mp);
}

ib_mpool_t *ib_engine_pool_config_get(ib_engine_t *ib)
{
    IB_FTRACE_INIT(ib_engine_pool_config_get);
    IB_FTRACE_RET_PTR(ib_mpool_t, ib->mp);
}

ib_mpool_t *ib_engine_pool_temp_get(ib_engine_t *ib)
{
    IB_FTRACE_INIT(ib_engine_pool_temp_get);
    IB_FTRACE_RET_PTR(ib_mpool_t, ib->temp_mp);
}

void ib_engine_pool_temp_destroy(ib_engine_t *ib)
{
    IB_FTRACE_INIT(ib_engine_pool_temp_destroy);
    ib_mpool_destroy(ib->temp_mp);
    ib->temp_mp = NULL;
    IB_FTRACE_RET_VOID();
}

void ib_engine_destroy(ib_engine_t *ib)
{
    IB_FTRACE_INIT(ib_destroy);
    if (ib) {
        ib_log(ib, 9, "Destroy IB handle (%d,%d,%s,%s): %p",
               ib->plugin->vernum, ib->plugin->abinum,
               ib->plugin->filename, ib->plugin->name, ib);

        ib_mpool_destroy(ib->mp);
    }
    IB_FTRACE_RET_VOID();
}

ib_status_t ib_conn_create(ib_engine_t *ib,
                           ib_conn_t **pconn, void *pctx)
{
    IB_FTRACE_INIT(ib_conn_create);
    ib_mpool_t *pool;
    struct timeval tv;
    uint16_t pid16 = (uint16_t)(getpid() & 0xffff);
    ib_status_t rc;
    
    /* Create a sub-pool for each connection and allocate from it */
    rc = ib_mpool_create(&pool, ib->mp);
    if (rc != IB_OK) {
        ib_log_error(ib, 0, "Failed to create connection memory pool: %d", rc);
        rc = IB_EALLOC;
        goto failed;
    }
    *pconn = (ib_conn_t *)ib_mpool_calloc(pool, 1, sizeof(**pconn));
    if (*pconn == NULL) {
        ib_log_error(ib, 0, "Failed to allocate memory for connection");
        rc = IB_EALLOC;
        goto failed;
    }

    (*pconn)->ib = ib;
    (*pconn)->mp = pool;
    (*pconn)->ctx = ib->ctx;
    (*pconn)->pctx = pctx;

    /// @todo Need to avoid gettimeofday and set from parser
    gettimeofday(&tv, NULL);
    (*pconn)->started.tv_sec = tv.tv_sec;
    (*pconn)->started.tv_usec = tv.tv_usec;

    /* Setup the base uuid structure which is used to generate
     * transaction IDs.
     */
    (*pconn)->base_uuid.node[0] = (pid16 >> 8) & 0xff;
    (*pconn)->base_uuid.node[1] = (pid16 & 0xff);
    /// @todo Set to a real system unique id (default ipv4 address???)
    (*pconn)->base_uuid.node[2] = 0x01; 
    (*pconn)->base_uuid.node[3] = 0x23; 
    (*pconn)->base_uuid.node[4] = 0x45; 
    (*pconn)->base_uuid.node[5] = 0x67; 
    /// @todo This needs set to thread ID or some other identifier
    (*pconn)->base_uuid.clk_seq_hi_res = 0x8f;
    (*pconn)->base_uuid.clk_seq_low = 0xff;

    /* Create the core data provider instance */
    rc = ib_provider_instance_create(ib,
                                     IB_PROVIDER_TYPE_DATA,
                                     "core",
                                     &((*pconn)->dpi),
                                     (*pconn)->mp,
                                     NULL);
    if (rc != IB_OK) {
        goto failed;
    }

    rc = ib_hash_create(&((*pconn)->data), (*pconn)->mp);
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

ib_status_t ib_conn_data_create(ib_conn_t *conn,
                                ib_conndata_t **pconndata,
                                size_t dalloc)
{
    IB_FTRACE_INIT(ib_conn_data_create);
    ib_engine_t *ib = conn->ib;
    ib_mpool_t *pool;
    ib_status_t rc;
    
    /* Create a sub-pool for data buffers */
    rc = ib_mpool_create(&pool, conn->mp);
    if (rc != IB_OK) {
        ib_log_error(ib, 0, "Failed to create connection data memory pool: %d", rc);
        rc = IB_EALLOC;
        goto failed;
    }
    *pconndata = (ib_conndata_t *)ib_mpool_calloc(pool, 1, sizeof(**pconndata));
    if (*pconndata == NULL) {
        ib_log_error(ib, 0, "Failed to allocate memory for connection data");
        rc = IB_EALLOC;
        goto failed;
    }

    (*pconndata)->ib = ib;
    (*pconndata)->mp = pool;
    (*pconndata)->conn = conn;

    (*pconndata)->dlen = 0;
    (*pconndata)->dalloc = dalloc;
    (*pconndata)->data = (uint8_t *)ib_mpool_calloc(pool, 1, dalloc);
    if ((*pconndata)->data == NULL) {
        ib_log_error(ib, 0, "Failed to allocate memory for connection data buffer");
        rc = IB_EALLOC;
        goto failed;
    }

    IB_FTRACE_RET_STATUS(IB_OK);

failed:
    /* Make sure everything is cleaned up on failure */
    if (*pconndata != NULL) {
        ib_mpool_destroy((*pconndata)->mp);
    }
    *pconndata = NULL;

    IB_FTRACE_RET_STATUS(rc);
}

void ib_conn_destroy(ib_conn_t *conn)
{
    /// @todo Probably need to update state???
    ib_mpool_destroy(conn->mp);
}

/**
 * @internal
 * Merge the base_uuid with tx data and generate the tx id string.
 */
static void ib_tx_generate_id(ib_tx_t *tx)
{
    ib_uuid_t uuid;

    /* Start with the base values. */
    uuid.clk_seq_hi_res = tx->conn->base_uuid.clk_seq_hi_res;
    uuid.clk_seq_low = tx->conn->base_uuid.clk_seq_low;
    memcpy(uuid.node, tx->conn->base_uuid.node, sizeof(uuid.node));

    /* Set the tx specific values */
    uuid.time_low = tx->started.tv_sec;
    uuid.time_mid = tx->conn->started.tv_usec + tx->conn->tx_count;
    uuid.time_hi_and_ver = (uint16_t)tx->started.tv_usec & 0x0fff;
    uuid.time_hi_and_ver |= (4 << 12);

    /* Convert to a hex-string representation */
    tx->id = (const char *)ib_mpool_alloc(tx->mp, IB_UUID_HEX_SIZE);
    if (tx->id == NULL) {
        return;
    }

    snprintf((char *)tx->id, IB_UUID_HEX_SIZE,
            "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
            uuid.time_low,
            uuid.time_mid,
            uuid.time_hi_and_ver,
            uuid.clk_seq_hi_res,
            uuid.clk_seq_low,
            uuid.node[0],
            uuid.node[1],
            uuid.node[2],
            uuid.node[3],
            uuid.node[4],
            uuid.node[5]);
}

ib_status_t ib_tx_create(ib_engine_t *ib,
                         ib_tx_t **ptx,
                         ib_conn_t *conn,
                         void *pctx)
{
    IB_FTRACE_INIT(ib_tx_create);
    ib_mpool_t *pool;
    struct timeval tv;
    ib_status_t rc;
    
    /* Create a sub-pool from the connection memory pool for each
     * transaction and allocate from it
     */
    rc = ib_mpool_create(&pool, conn->mp);
    if (rc != IB_OK) {
        ib_log_error(ib, 0, "Failed to create transaction memory pool: %d", rc);
        rc = IB_EALLOC;
        goto failed;
    }
    *ptx = (ib_tx_t *)ib_mpool_calloc(pool, 1, sizeof(**ptx));
    if ((*ptx) == NULL) {
        ib_log_error(ib, 0, "Failed to allocate memory for transaction");
        rc = IB_EALLOC;
        goto failed;
    }

    (*ptx)->ib = ib;
    (*ptx)->mp = pool;
    (*ptx)->ctx = ib->ctx;
    (*ptx)->pctx = pctx;
    (*ptx)->conn = conn;

    /* Update transaction count. */
    conn->tx_count++;

    /// @todo Need to avoid gettimeofday and set from parser tx time, but
    ///       it currently only has second accuracy.
    gettimeofday(&tv, NULL);
    (*ptx)->started.tv_sec = tv.tv_sec;
    (*ptx)->started.tv_usec = tv.tv_usec;

    ib_tx_generate_id(*ptx);

    /* Create the core data provider instance */
    rc = ib_provider_instance_create(ib,
                                     IB_PROVIDER_TYPE_DATA,
                                     "core",
                                     &((*ptx)->dpi),
                                     (*ptx)->mp,
                                     NULL);
    if (rc != IB_OK) {
        goto failed;
    }


    rc = ib_hash_create(&((*ptx)->data), (*ptx)->mp);
    if (rc != IB_OK) {
        rc = IB_EALLOC;
        goto failed;
    }

    /* Add transaction to the connection list */
    if (conn->tx == NULL) {
        conn->tx = *ptx;
        conn->tx_last = *ptx;
    }
    else {
        conn->tx_last->next = *ptx;
        conn->tx_last = *ptx;

        /* If there are more than one transactions, then this is a pipeline
         * request and needs to be marked as such.
         */
        ib_tx_flags_set(conn->tx, IB_TX_FPIPELINED);
        ib_tx_flags_set(*ptx, IB_TX_FPIPELINED);

        ib_log_debug(ib, 9, "Found a pipelined transaction.");
    }

    IB_FTRACE_RET_STATUS(IB_OK);

failed:
    /* Make sure everything is cleaned up on failure */
    if (*ptx != NULL) {
        ib_mpool_destroy((*ptx)->mp);
    }
    *ptx = NULL;

    IB_FTRACE_RET_STATUS(rc);
}

void ib_tx_destroy(ib_tx_t *tx)
{
    ib_tx_t *conn_tx = tx->conn->tx;

    /* Remove transaction from the connection list */
    if (conn_tx == tx) {
        tx->conn->tx = tx->conn->tx->next;
    }
    else {
        /// @todo It should always be the first one in the list, 
        ///       so this should not be needed and should cause an error
        ///       or maybe for us to throw a flag???
        while (conn_tx->next != NULL) {
            if (conn_tx->next == tx) {
                conn_tx->next = conn_tx->next->next;
                break;
            }
            conn_tx = conn_tx->next;
        }
        if (conn_tx == NULL) {
            abort(); /// @todo Testing - should never happen
        }
    }

    /// @todo Probably need to update state???
    ib_mpool_destroy(tx->mp);
}


/* -- State Routines -- */

/**
 * @internal
 * List of state names for id to name lookups.
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
    IB_STRINGIFY(handle_request_headers_event),
    IB_STRINGIFY(handle_request_event),
    IB_STRINGIFY(handle_response_headers_event),
    IB_STRINGIFY(handle_response_event),
    IB_STRINGIFY(handle_disconnect_event),
    IB_STRINGIFY(handle_postprocess_event),

    /* Plugin States */
    IB_STRINGIFY(cfg_started_event),
    IB_STRINGIFY(cfg_finished_event),
    IB_STRINGIFY(conn_opened_event),
    IB_STRINGIFY(conn_data_in_event),
    IB_STRINGIFY(conn_data_out_event),
    IB_STRINGIFY(conn_closed_event),

    /* Parser States */
    IB_STRINGIFY(tx_data_in_event),
    IB_STRINGIFY(tx_data_out_event),
    IB_STRINGIFY(request_started_event),
    IB_STRINGIFY(request_headers_event),
    IB_STRINGIFY(request_body_event),
    IB_STRINGIFY(request_finished_event),
    IB_STRINGIFY(response_started_event),
    IB_STRINGIFY(response_headers_event),
    IB_STRINGIFY(response_body_event),
    IB_STRINGIFY(response_finished_event),

    NULL
};

const char *ib_state_event_name(ib_state_event_type_t event)
{
    IB_FTRACE_INIT(ib_state_event_name);
    IB_FTRACE_RET_CONSTSTR(ib_state_event_name_list[event]);
}


/**
 * @internal
 * Notify the engine that an event has occurred.
 *
 * This is a generic function that handles all types.
 *
 * @param ib Engine
 * @param event Event
 * @param param Parameter (type is event specific)
 *
 * @returns Status code
 */
static ib_status_t ib_state_notify(ib_engine_t *ib,
                                   ib_state_event_type_t event,
                                   void *param)
{
    IB_FTRACE_INIT(ib_state_notify);
    ib_hook_t *hook = NULL;
    ib_status_t rc = IB_OK;

    hook = ib->ectx->hook[event];

    ib_log_debug(ib, 4, "EVENT: %s", ib_state_event_name(event));

    while (hook != NULL) {
        ib_state_hook_fn_t cb = (ib_state_hook_fn_t)hook->callback;
        rc = cb(ib, param, hook->cdata);
        if (rc != IB_OK) {
            /// @todo Or should we go on???
            ib_log_error(ib, 4, "Hook returned error: %s=%d",
                         ib_state_event_name(event), rc);
            break;
        }

        hook = hook->next;
    }

    IB_FTRACE_RET_STATUS(rc);
}

/**
 * @internal
 * Notify the engine that a connection event has occurred.
 *
 * @param ib Engine
 * @param event Event
 * @param conn Connection
 *
 * @returns Status code
 */
static ib_status_t ib_state_notify_conn(ib_engine_t *ib,
                                        ib_state_event_type_t event,
                                        ib_conn_t *conn)
{
    IB_FTRACE_INIT(ib_state_notify_conn);
    ib_hook_t *hook = NULL;
    ib_status_t rc = IB_OK;
    
    rc = ib_state_notify(ib, event, conn);
    if ((rc != IB_OK) || (conn->ctx == NULL)) {
        IB_FTRACE_RET_STATUS(rc);
    }

    if (conn->ctx != ib->ctx) {
        hook = conn->ctx->hook[event];
    }

    while (hook != NULL) {
        ib_state_hook_fn_t cb = (ib_state_hook_fn_t)hook->callback;
        rc = cb(ib, conn, hook->cdata);
        if (rc != IB_OK) {
            /// @todo Or should we go on???
            ib_log_error(ib, 4, "Hook returned error: %s=%d",
                         ib_state_event_name(event), rc);
            break;
        }

        hook = hook->next;
    }

    IB_FTRACE_RET_STATUS(rc);
}

/**
 * @internal
 * Notify the engine that a connection data event has occurred.
 *
 * @param ib Engine
 * @param event Event
 * @param conndata Connection data
 *
 * @returns Status code
 */
static ib_status_t ib_state_notify_conn_data(ib_engine_t *ib,
                                             ib_state_event_type_t event,
                                             ib_conndata_t *conndata)
{
    IB_FTRACE_INIT(ib_state_notify_conn_data);
    ib_conn_t *conn = conndata->conn;
    ib_hook_t *hook = NULL;
    ib_status_t rc = IB_OK;
    
    rc = ib_state_notify(ib, event, conndata);
    if ((rc != IB_OK) || (conn->ctx == NULL)) {
        IB_FTRACE_RET_STATUS(rc);
    }

    if (conn->ctx != ib->ctx) {
        hook = conn->ctx->hook[event];
    }

    while (hook != NULL) {
        ib_state_hook_fn_t cb = (ib_state_hook_fn_t)hook->callback;
        rc = cb(ib, conndata, hook->cdata);
        if (rc != IB_OK) {
            /// @todo Or should we go on???
            ib_log_error(ib, 4, "Hook returned error: %s=%d",
                         ib_state_event_name(event), rc);
            break;
        }

        hook = hook->next;
    }

    IB_FTRACE_RET_STATUS(rc);
}

/**
 * @internal
 * Notify the engine that a transaction data event has occurred.
 *
 * @param ib Engine
 * @param event Event
 * @param txdata Connection data
 *
 * @returns Status code
 */
static ib_status_t ib_state_notify_tx_data(ib_engine_t *ib,
                                           ib_state_event_type_t event,
                                           ib_txdata_t *txdata)
{
    IB_FTRACE_INIT(ib_state_notify_tx_data);
    ib_tx_t *tx = txdata->tx;
    ib_hook_t *hook = NULL;
    ib_status_t rc = IB_OK;
    
    rc = ib_state_notify(ib, event, txdata);
    if ((rc != IB_OK) || (tx->ctx == NULL)) {
        IB_FTRACE_RET_STATUS(rc);
    }

    if (tx->ctx != ib->ctx) {
        hook = tx->ctx->hook[event];
    }

    while (hook != NULL) {
        ib_state_hook_fn_t cb = (ib_state_hook_fn_t)hook->callback;
        rc = cb(ib, txdata, hook->cdata);
        if (rc != IB_OK) {
            /// @todo Or should we go on???
            ib_log_error(ib, 4, "Hook returned error: %s=%d",
                         ib_state_event_name(event), rc);
            break;
        }

        hook = hook->next;
    }

    IB_FTRACE_RET_STATUS(rc);
}

/**
 * @internal
 * Notify the engine that a transaction event has occurred.
 *
 * @param ib Engine
 * @param event Event
 * @param tx Transaction
 *
 * @returns Status code
 */
static ib_status_t ib_state_notify_tx(ib_engine_t *ib,
                                      ib_state_event_type_t event,
                                      ib_tx_t *tx)
{
    IB_FTRACE_INIT(ib_state_notify_tx);
    ib_hook_t *hook = NULL;
    ib_status_t rc = IB_OK;
    
    rc = ib_state_notify(ib, event, tx);
    if ((rc != IB_OK) || (tx->ctx == NULL)) {
        IB_FTRACE_RET_STATUS(rc);
    }

    if (tx->ctx != ib->ctx) {
        hook = tx->ctx->hook[event];
    }

    while (hook != NULL) {
        ib_state_hook_fn_t cb = (ib_state_hook_fn_t)hook->callback;
        rc = cb(ib, tx, hook->cdata);
        if (rc != IB_OK) {
            /// @todo Or should we go on???
            ib_log_error(ib, 4, "Hook returned error: %s=%d",
                         ib_state_event_name(event), rc);
            break;
        }

        hook = hook->next;
    }

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_state_notify_cfg_started(ib_engine_t *ib)
{
    IB_FTRACE_INIT(ib_state_notify_cfg_started);
    ib_status_t rc;

    /* Create and configure the main configuration context. */
    ib_engine_context_create_main(ib);

    /// @todo Create a temp mem pool???

    rc = ib_state_notify(ib, cfg_started_event, NULL);

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_state_notify_cfg_finished(ib_engine_t *ib)
{
    IB_FTRACE_INIT(ib_state_notify_cfg_finished);
    ib_status_t rc;

    /* Initialize (and close) the main configuration context. */
    rc = ib_context_init(ib->ctx);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Run the hooks. */
    rc = ib_state_notify(ib, cfg_finished_event, NULL);

    /* Destroy the temporary memory pool. */
    ib_engine_pool_temp_destroy(ib);

    IB_FTRACE_RET_STATUS(rc);
}

/**
 * Notify engine of additional events when notification of a
 * @ref conn_opened_event occurs.
 *
 * When the event is notified, additional events are notified immediately
 * prior to it:
 *
 *  - @ref conn_started_event
 *
 * And immediately following it:
 *
 *  - @ref handle_context_conn_event
 *  - @ref handle_connect_event
 */
ib_status_t ib_state_notify_conn_opened(ib_engine_t *ib,
                                        ib_conn_t *conn)
{
    IB_FTRACE_INIT(ib_state_notify_conn_opened);
    if (ib_conn_flags_isset(conn, IB_CONN_FOPENED)) {
        ib_log_error(ib, 4, "Attempted to notify previously notified event: %s",
                     ib_state_event_name(conn_opened_event));
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    ib_conn_flags_set(conn, IB_CONN_FOPENED);

    ib_status_t rc = ib_state_notify_conn(ib, conn_started_event, conn);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = ib_state_notify_conn(ib, conn_opened_event, conn);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Select the connection context to use. */
    rc = _ib_context_get(ib, IB_CTYPE_CONN, conn, &conn->ctx);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = ib_state_notify_conn(ib, handle_context_conn_event, conn);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = ib_state_notify_conn(ib, handle_connect_event, conn);
    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_state_notify_conn_data_in(ib_engine_t *ib,
                                         ib_conndata_t *conndata)
{
    IB_FTRACE_INIT(ib_state_notify_conn_data_in);
    ib_conn_t *conn = conndata->conn;
    ib_provider_inst_t *pi = ib_parser_provider_get_instance(conn->ctx);
    IB_PROVIDER_IFACE_TYPE(parser) *iface = pi?(IB_PROVIDER_IFACE_TYPE(parser) *)pi->pr->iface:NULL;
    ib_status_t rc;

    if ((conndata->conn->flags & IB_CONN_FSEENDATAIN) == 0) {
        ib_conn_flags_set(conndata->conn, IB_CONN_FSEENDATAIN);
    }

    /* Notify data handlers before the parser. */
    rc = ib_state_notify_conn_data(ib, conn_data_in_event, conndata);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Run the data through the parser. */
    if (iface == NULL) {
        /// @todo Probably should not need this check
        ib_log_error(ib, 0, "Failed to fetch parser interface on data in");
        IB_FTRACE_RET_STATUS(IB_EUNKNOWN);
    }
    rc = iface->data_in(pi, conndata);

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_state_notify_conn_data_out(ib_engine_t *ib,
                                          ib_conndata_t *conndata)
{
    IB_FTRACE_INIT(ib_state_notify_conn_data_out);
    ib_conn_t *conn = conndata->conn;
    ib_provider_inst_t *pi = ib_parser_provider_get_instance(conn->ctx);
    IB_PROVIDER_IFACE_TYPE(parser) *iface = pi?(IB_PROVIDER_IFACE_TYPE(parser) *)pi->pr->iface:NULL;
    ib_status_t rc;

    if ((conndata->conn->flags & IB_CONN_FSEENDATAOUT) == 0) {
        ib_conn_flags_set(conndata->conn, IB_CONN_FSEENDATAOUT);
    }

    /* Notify data handlers before the parser. */
    rc = ib_state_notify_conn_data(ib, conn_data_out_event, conndata);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Run the data through the parser. */
    if (iface == NULL) {
        /// @todo Probably should not need this check
        ib_log_error(ib, 0, "Failed to fetch parser interface on data out");
        IB_FTRACE_RET_STATUS(IB_EUNKNOWN);
    }
    rc = iface->data_out(pi, conndata);

    IB_FTRACE_RET_STATUS(rc);
}

/**
 * @ref conn_closed_event occurs.
 *
 * When the event is notified, additional events are notified immediately
 * prior to it:
 *
 *  - @ref handle_disconnect_event
 *  - @ref conn_finished_event
 */
ib_status_t ib_state_notify_conn_closed(ib_engine_t *ib,
                                        ib_conn_t *conn)
{
    IB_FTRACE_INIT(ib_state_notify_conn_closed);
    ib_status_t rc;

    if (ib_conn_flags_isset(conn, IB_CONN_FCLOSED)) {
        ib_log_error(ib, 4, "Attempted to notify previously notified event: %s",
                     ib_state_event_name(conn_closed_event));
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    ib_conn_flags_set(conn, IB_CONN_FCLOSED);

    rc = ib_state_notify_conn(ib, conn_closed_event, conn);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = ib_state_notify_conn(ib, handle_disconnect_event, conn);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = ib_state_notify_conn(ib, conn_finished_event, conn);

    IB_FTRACE_RET_STATUS(rc);
}

/**
 * @ref tx_data_in_event occurs.
 *
 * When the event is notified, additional events are notified immediately
 * prior to it:
 *
 *  - @ref tx_started_event
 */
ib_status_t ib_state_notify_tx_data_in(ib_engine_t *ib,
                                       ib_txdata_t *txdata)
{
    IB_FTRACE_INIT(ib_state_notify_tx_data_in);
    ib_status_t rc;

    if ((txdata->tx->flags & IB_TX_FSEENDATAIN) == 0) {
        ib_tx_flags_set(txdata->tx, IB_TX_FSEENDATAIN);
    }

    rc = ib_state_notify_tx_data(ib, tx_data_in_event, txdata);
    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_state_notify_tx_data_out(ib_engine_t *ib,
                                        ib_txdata_t *txdata)
{
    IB_FTRACE_INIT(ib_state_notify_tx_data_out);
    ib_status_t rc;

    if ((txdata->tx->flags & IB_TX_FSEENDATAOUT) == 0) {
        ib_tx_flags_set(txdata->tx, IB_TX_FSEENDATAOUT);
    }

    rc = ib_state_notify_tx_data(ib, tx_data_out_event, txdata);
    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_state_notify_request_started(ib_engine_t *ib,
                                            ib_tx_t *tx)
{
    IB_FTRACE_INIT(ib_state_notify_request_started);
    ib_status_t rc;

    if (ib_tx_flags_isset(tx, IB_TX_FREQ_STARTED)) {
        ib_log_error(ib, 4, "Attempted to notify previously notified event: %s",
                     ib_state_event_name(request_started_event));
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    rc = ib_state_notify_tx(ib, tx_started_event, tx);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    ib_tx_flags_set(tx, IB_TX_FREQ_STARTED);

    rc = ib_state_notify_tx(ib, request_started_event, tx);
    IB_FTRACE_RET_STATUS(rc);
}

/**
 * @ref request_headers_event occurs.
 *
 * When the event is notified, additional events are notified immediately
 * prior to it:
 *
 *  - @ref request_started_event (if not already notified)
 *
 * And immediately following it:
 *
 *  - @ref handle_context_tx_event
 *  - @ref handle_request_headers_event
 */
ib_status_t ib_state_notify_request_headers(ib_engine_t *ib,
                                            ib_tx_t *tx)
{
    IB_FTRACE_INIT(ib_state_notify_request_headers);
    ib_status_t rc;

    if (ib_tx_flags_isset(tx, IB_TX_FREQ_SEENHEADERS)) {
        ib_log_error(ib, 4, "Attempted to notify previously notified event: %s",
                     ib_state_event_name(request_headers_event));
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    if ((tx->flags & IB_TX_FREQ_STARTED) == 0) {
        ib_log_debug(ib, 9, "Automatically triggering optional %s",
                     ib_state_event_name(request_started_event));
        ib_state_notify_request_started(ib, tx);
    }

    ib_tx_flags_set(tx, IB_TX_FREQ_SEENHEADERS);

    rc = ib_state_notify_tx(ib, request_headers_event, tx);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Select the transaction context to use. */
    rc = _ib_context_get(ib, IB_CTYPE_TX, tx, &tx->ctx);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = ib_state_notify_tx(ib, handle_context_tx_event, tx);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = ib_state_notify_tx(ib, handle_request_headers_event, tx);
    IB_FTRACE_RET_STATUS(rc);
}

/**
 * @ref request_body_event occurs.
 *
 * When the event is notified, additional events are notified immediately
 * following it:
 *
 *  - @ref handle_request_event
 */
static ib_status_t ib_state_notify_request_body_ex(ib_engine_t *ib,
                                                   ib_tx_t *tx)
{
    IB_FTRACE_INIT(ib_state_notify_request_body_ex);
    ib_status_t rc = ib_state_notify_tx(ib, request_body_event, tx);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = ib_state_notify_tx(ib, handle_request_event, tx);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_state_notify_request_body(ib_engine_t *ib,
                                         ib_tx_t *tx)
{
    IB_FTRACE_INIT(ib_state_notify_request_body);
    ib_status_t rc;

    if (ib_tx_flags_isset(tx, IB_TX_FREQ_SEENBODY)) {
        ib_log_error(ib, 4, "Attempted to notify previously notified event: %s",
                     ib_state_event_name(request_body_event));
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    ib_tx_flags_set(tx, IB_TX_FREQ_SEENBODY);

    rc = ib_state_notify_request_body_ex(ib, tx);
    IB_FTRACE_RET_STATUS(rc);
}

/**
 * @ref request_finished_event occurs.
 *
 * When the event is notified, additional events are notified
 * immediately prior to it:
 *
 *  - @ref request_body_event (only if not already notified)
 *
 * And immediately following it:
 *
 *  - @ref tx_process_event
 */
ib_status_t ib_state_notify_request_finished(ib_engine_t *ib,
                                             ib_tx_t *tx)
{
    IB_FTRACE_INIT(ib_state_notify_request_finished);
    ib_status_t rc;

    if (ib_tx_flags_isset(tx, IB_TX_FREQ_FINISHED)) {
        ib_log_error(ib, 4, "Attempted to notify previously notified event: %s",
                     ib_state_event_name(request_finished_event));
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    ib_tx_flags_set(tx, IB_TX_FREQ_FINISHED);

    /* Still need to notify request_body_event, if it has not yet
     * been triggered, however, it is an error if it was not
     * triggered for a request that should have had a body.
     */
    if ((tx->flags & IB_TX_FREQ_SEENBODY) == 0) {
        if ((tx->flags & IB_TX_FREQ_NOBODY) == 0) {
            ib_tx_flags_set(tx, IB_TX_FERROR);
        }
        rc = ib_state_notify_request_body_ex(ib, tx);
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }
    }

    rc = ib_state_notify_tx(ib, request_finished_event, tx);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = ib_state_notify_tx(ib, tx_process_event, tx);
    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_state_notify_response_started(ib_engine_t *ib,
                                             ib_tx_t *tx)
{
    IB_FTRACE_INIT(ib_state_notify_response_started);
    struct timeval tv;
    ib_status_t rc;

    /// @todo Need to avoid gettimeofday and set from parser
    gettimeofday(&tv, NULL);
    tx->tv_response.tv_sec = tv.tv_sec;
    tx->tv_response.tv_usec = tv.tv_usec;

    if (ib_tx_flags_isset(tx, IB_TX_FRES_STARTED)) {
        ib_log_error(ib, 4, "Attempted to notify previously notified event: %s",
                     ib_state_event_name(response_started_event));
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    ib_tx_flags_set(tx, IB_TX_FRES_STARTED);

    rc = ib_state_notify_tx(ib, response_started_event, tx);
    IB_FTRACE_RET_STATUS(rc);
}

/**
 * @ref response_headers_event occurs.
 *
 * When the event is notified, additional events are notified
 * immediately prior to it:
 *
 *  - @ref response_started_event (only if not already notified)
 *
 * And immediately following it:
 *
 *  - @ref handle_response_headers_event
 */
ib_status_t ib_state_notify_response_headers(ib_engine_t *ib,
                                             ib_tx_t *tx)
{
    IB_FTRACE_INIT(ib_state_notify_response_headers);
    ib_status_t rc;

    if (ib_tx_flags_isset(tx, IB_TX_FRES_SEENHEADERS)) {
        ib_log_error(ib, 4, "Attempted to notify previously notified event: %s",
                     ib_state_event_name(response_headers_event));
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    if ((tx->flags & IB_TX_FRES_STARTED) == 0) {
        ib_log_debug(ib, 9, "Automatically triggering optional %s",
                     ib_state_event_name(response_started_event));
        ib_state_notify_response_started(ib, tx);
    }

    ib_tx_flags_set(tx, IB_TX_FRES_SEENHEADERS);

    rc = ib_state_notify_tx(ib, response_headers_event, tx);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = ib_state_notify_tx(ib, handle_response_headers_event, tx);
    IB_FTRACE_RET_STATUS(rc);
}

/**
 * @ref response_body_event  occurs.
 *
 * When the event is notified, additional events are notified
 * immediately following it:
 *
 *  - @ref handle_response_event
 */
ib_status_t ib_state_notify_response_body(ib_engine_t *ib,
                                          ib_tx_t *tx)
{
    IB_FTRACE_INIT(ib_state_notify_response_body);
    ib_status_t rc;

    if (ib_tx_flags_isset(tx, IB_TX_FRES_SEENBODY)) {
        ib_log_error(ib, 4, "Attempted to notify previously notified event: %s",
                     ib_state_event_name(response_body_event));
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    ib_tx_flags_set(tx, IB_TX_FRES_SEENBODY);

    rc = ib_state_notify_tx(ib, response_body_event, tx);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = ib_state_notify_tx(ib, handle_response_event, tx);
    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_state_notify_response_finished(ib_engine_t *ib,
                                              ib_tx_t *tx)
{
    IB_FTRACE_INIT(ib_state_notify_response_finished);
    ib_status_t rc;

    if (ib_tx_flags_isset(tx, IB_TX_FRES_FINISHED)) {
        ib_log_error(ib, 4, "Attempted to notify previously notified event: %s",
                     ib_state_event_name(response_finished_event));
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    ib_tx_flags_set(tx, IB_TX_FRES_FINISHED);

    rc = ib_state_notify_tx(ib, response_finished_event, tx);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = ib_state_notify_tx(ib, handle_postprocess_event, tx);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = ib_state_notify_tx(ib, tx_finished_event, tx);
    IB_FTRACE_RET_STATUS(rc);
}


/* -- Hook Routines -- */

ib_status_t ib_hook_register(ib_engine_t *ib,
                             ib_state_event_type_t event,
                             ib_void_fn_t cb, void *cdata)
{
    IB_FTRACE_INIT(ib_hook_register);
    ib_hook_t *last = ib->ectx->hook[event];
    ib_hook_t *hook = (ib_hook_t *)ib_mpool_alloc(ib->mp, sizeof(*hook));

    if (hook == NULL) {
        ib_log_abort(ib, "Error in ib_mpool_calloc");
    }

    hook->callback = cb;
    hook->cdata = cdata;
    hook->next = NULL;

    /* Insert the hook at the end of the list */
    if (last == NULL) {
        ib_log(ib, 9, "Registering %s hook: %p",
               ib_state_event_name(event), cb);

        ib->ectx->hook[event] = hook;

        IB_FTRACE_RET_STATUS(IB_OK);
    }
    while (last->next != NULL) {
        last = last->next;
    }

    last->next = hook;

    ib_log(ib, 9, "Registering %s hook after %p: %p",
           ib_state_event_name(event), last->callback, cb);

    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t ib_hook_unregister(ib_engine_t *ib,
                               ib_state_event_type_t event,
                               ib_void_fn_t cb)
{
    IB_FTRACE_INIT(ib_hook_unregister);
    ib_hook_t *prev = NULL;
    ib_hook_t *hook = ib->ectx->hook[event];

    /* Remove the first matching hook */
    while (hook != NULL) {
        if (hook->callback == cb) {
            if (prev == NULL) {
                ib->ectx->hook[event] = hook->next;
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


ib_status_t ib_hook_register_context(ib_context_t *ctx,
                                     ib_state_event_type_t event,
                                     ib_void_fn_t cb, void *cdata)
{
    IB_FTRACE_INIT(ib_hook_register_context);
    ib_engine_t *ib = ctx->ib;
    ib_hook_t *last = ctx->hook[event];
    ib_hook_t *hook = (ib_hook_t *)ib_mpool_alloc(ctx->mp, sizeof(*hook));

    ib_log_debug(ib, 4, "ib_hook_register_context(%p,%d,%p,%p)",
                 ctx, event, (intptr_t)cb, cdata);

    if (hook == NULL) {
        ib_log_abort(ib, "Error in ib_mpool_calloc");
    }

    hook->callback = cb;
    hook->cdata = cdata;
    hook->next = NULL;

    /* Insert the hook at the end of the list */
    if (last == NULL) {
        ib_log(ib, 9, "Registering %s ctx hook: %p",
               ib_state_event_name(event), cb);

        ctx->hook[event] = hook;

        IB_FTRACE_RET_STATUS(IB_OK);
    }
    while (last->next != NULL) {
        last = last->next;
    }

    ib_log(ib, 9, "Registering %s ctx hook: %p",
           ib_state_event_name(event), cb);

    last->next = hook;

    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t ib_hook_unregister_context(ib_context_t *ctx,
                                       ib_state_event_type_t event,
                                       ib_void_fn_t cb)
{
    IB_FTRACE_INIT(ib_hook_unregister_context);
    //ib_engine_t *ib = ctx->ib;
    ib_hook_t *prev = NULL;
    ib_hook_t *hook = ctx->hook[event];

    /* Remove the first matching hook */
    while (hook != NULL) {
        if (hook->callback == cb) {
            if (prev == NULL) {
                ctx->hook[event] = hook->next;
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


/* -- Connection Handling -- */


/* -- Transaction Handling -- */


/* -- Module Routines -- */

/// @todo Probably need to load into a given context???
ib_status_t ib_module_init(ib_module_t *m, ib_engine_t *ib)
{
    IB_FTRACE_INIT(ib_module_init);
    ib_status_t rc;

    /* Keep track of the module index. */
    m->idx = ib_array_elements(ib->modules);
    m->ib = ib;

    ib_log_debug(ib, 4, "Initializing module %s (%d): %s",
                 m->name, m->idx, m->filename);

    /* Zero the config structure if there is one. */
    if (m->gclen > 0) {
        memset((void *)m->gcdata, 0, m->gclen);
    }

    /* Register directives */
    if (m->dm_init != NULL) {
        ib_config_register_directives(ib, m->dm_init);
    }

    rc = ib_array_setn(ib->modules, m->idx, m);
    if (rc != IB_OK) {
        ib_log_error(ib, 1, "Failed to register module %s %d", m->name, rc);
        IB_FTRACE_RET_STATUS(rc);
    }

    if (ib->ctx != NULL) {
        ib_log_debug(ib, 4, "Registering module \"%s\" with main context %p",
                     m->name, ib->ctx);
        ib_module_register_context(m, ib->ctx);
    }
    else {
        ib_log_debug(ib, 4, "No main context to registering module \"%s\"",
                     m->name);
    }

    /* Init and register the module */
    if (m->fn_init != NULL) {
        rc = m->fn_init(ib, m);
        if (rc != IB_OK) {
            ib_log_error(ib, 1, "Failed to initialize module %s %d",
                         m->name, rc);
            /// @todo Need to be able to delete the entry???
            ib_array_setn(ib->modules, m->idx, NULL);
            IB_FTRACE_RET_STATUS(rc);
        }
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t ib_module_create(ib_module_t **pm,
                             ib_engine_t *ib)
{
    IB_FTRACE_INIT(ib_module_create);

    *pm = (ib_module_t *)ib_mpool_calloc(ib->config_mp, 1, sizeof(**pm));
    if (*pm == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t ib_module_load(ib_module_t **pm,
                           ib_engine_t *ib,
                           const char *file)
{
    IB_FTRACE_INIT(ib_module_load);
    ib_status_t rc;
    ib_dso_t *dso;

    if (ib == NULL) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    /* Load module and fetch the module structure */
    ib_log_debug(ib, 4, "Loading module: %s", file);
    rc = ib_dso_open(&dso, file, ib->config_mp);
    if (rc != IB_OK) {
        ib_log_error(ib, 1, "Failed to load module %s: %d", file, rc);
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = ib_dso_sym_find(dso, IB_MODULE_SYM_NAME, (ib_dso_sym_t **)pm);
    if (rc != IB_OK) {
        ib_log_error(ib, 1, "Failed to load module %s: no symbol named %s", 
                     file, IB_MODULE_SYM_NAME);
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Check module for ABI compatibility with this engine */
    if ((*pm)->vernum > IB_VERNUM) {
        ib_log_error(ib, 0,
                     "Module %s (built against engine version %s) is not "
                     "compatible with this engine (version %s): "
                     "ABI %d > %d",
                     file, (*pm)->version, IB_VERSION, (*pm)->abinum, IB_ABINUM);
        IB_FTRACE_RET_STATUS(IB_EINCOMPAT);
    }

    ib_log_debug(ib, 9,
                 "Loaded module %s: "
                 "vernum=%d abinum=%d version=%s index=%d filename=%s",
                 (*pm)->name,
                 (*pm)->vernum, (*pm)->abinum, (*pm)->version,
                 (*pm)->idx, (*pm)->filename);

    rc = ib_module_init(*pm, ib);
    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_module_unload(ib_module_t *m)
{
    IB_FTRACE_INIT(ib_module_unload);
    if (m == NULL) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    /// @todo Implement

    /* Deregister directives */

    IB_FTRACE_RET_STATUS(IB_ENOTIMPL);
}

ib_status_t ib_module_register_context(ib_module_t *m,
                                       ib_context_t *ctx)
{
    IB_FTRACE_INIT(ib_module_register_context);
    ib_context_data_t *cfgdata;
    ib_status_t rc;

    /* Create a module context data structure. */
    cfgdata = (ib_context_data_t *)ib_mpool_alloc(ctx->mp, sizeof(*cfgdata));
    if (cfgdata == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }
    cfgdata->module = m;
    
    /* Set default values from parent values. */

    /* Add module config entries to config context, first copying the
     * global values, then overriding using default values from the
     * configuration mapping.
     *
     * NOTE: Not all configuration data is required to be in the
     * mapping, which is why the initial memcpy is required.
     */
    if (m->gclen > 0) {
        ib_context_t *p_ctx = ctx->parent;
        ib_context_data_t *p_cfgdata;

        cfgdata->data = ib_mpool_alloc(ctx->mp, m->gclen);
        if (cfgdata->data == NULL) {
            IB_FTRACE_RET_STATUS(IB_EALLOC);
        }

        /* Copy values from parent context if available, otherwise
         * use the module global values as defaults.
         */
        if (p_ctx != NULL) {
            rc = ib_array_get(p_ctx->cfgdata, m->idx, &p_cfgdata);
            if (rc == IB_OK) {
                memcpy(cfgdata->data, p_cfgdata->data, m->gclen);
            }
            else {
                /* No parent context config, so use globals. */
                memcpy(cfgdata->data, m->gcdata, m->gclen);
            }
        }
        else {
            memcpy(cfgdata->data, m->gcdata, m->gclen);
        }
    }
    ib_context_init_cfg(ctx, cfgdata->data, m->cm_init);

    /* Keep track of module specific context data using the
     * module index as the key so that the location is deterministic.
     */
    rc = ib_array_setn(ctx->cfgdata, m->idx, cfgdata);
    IB_FTRACE_RET_STATUS(rc);
}


/* -- Configuration Contexts -- */

ib_status_t ib_context_create(ib_context_t **pctx,
                              ib_engine_t *ib,
                              ib_context_t *parent,
                              ib_context_fn_t fn_ctx,
                              void *fn_ctx_data)
{
    IB_FTRACE_INIT(ib_context_create);
    ib_mpool_t *pool;
    ib_status_t rc;

    /* Create memory subpool */
    /// @todo Should we be doing this???
    rc = ib_mpool_create(&pool, ib->mp);
    if (rc != IB_OK) {
        rc = IB_EALLOC;
        goto failed;
    }

    /* Create the main structure */
    *pctx = (ib_context_t *)ib_mpool_calloc(pool, 1, sizeof(**pctx));
    if (*pctx == NULL) {
        rc = IB_EALLOC;
        goto failed;
    }

    /* Copy initial values from parent. */
    if (parent != NULL) {
        memcpy(*pctx, parent, sizeof(*pctx));
    }

    (*pctx)->mp = pool;
    (*pctx)->ib = ib;
    (*pctx)->parent = parent;
    (*pctx)->fn_ctx = fn_ctx;
    (*pctx)->fn_ctx_data = fn_ctx_data;

    /* Create a cfgmap to hold the configuration */
    rc = ib_cfgmap_create(&((*pctx)->cfg), (*pctx)->mp);
    if (rc != IB_OK) {
        goto failed;
    }

    /* Create an array to hold the module config data */
    rc = ib_array_create(&((*pctx)->cfgdata), (*pctx)->mp, 16, 8);
    if (rc != IB_OK) {
        goto failed;
    }

    rc = ib_array_appendn(ib->contexts, *pctx);
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
            ib_log_debug(ib, 9, "Registering module=\"%s\" idx=%d",
                         m->name, m->idx);
            rc = ib_module_register_context(m, *pctx);
            if (rc != IB_OK) {
                goto failed;
            }
        }
    }
    else {
        /* Register the core module by default. */
        rc = ib_module_register_context(ib_core_module(), *pctx);
        if (rc != IB_OK) {
            goto failed;
        }
    }

    IB_FTRACE_RET_STATUS(IB_OK);

failed:
    /* Make sure everything is cleaned up on failure */
    if (*pctx != NULL) {
        ib_mpool_destroy((*pctx)->mp);
    }
    *pctx = NULL;

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_context_init(ib_context_t *ctx)
{
    IB_FTRACE_INIT(ib_context_init);
    ib_engine_t *ib = ctx->ib;
    ib_context_data_t *cfgdata;
    ib_status_t rc;
    size_t ncfgdata, i;

    ib_log_debug(ib, 9, "Initializing context ctx=%p", ctx);

    /* Run through the context modules to call any ctx_init functions. */
    /// @todo Not sure this is needed anymore
    IB_ARRAY_LOOP(ctx->cfgdata, ncfgdata, i, cfgdata) {
        if (cfgdata == NULL) {
            continue;
        }
        ib_module_t *m = cfgdata->module;

        if (m->fn_ctx_init != NULL) {
            rc = m->fn_ctx_init(ib, m, ctx);
            if (rc != IB_OK) {
                /// @todo Log the error???  Fail???
                ib_log_error(ib, 4, "Failed to call context init: %d", rc);
                IB_FTRACE_RET_STATUS(rc);
            }
        }
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_context_t *ib_context_parent_get(ib_context_t *ctx)
{
    IB_FTRACE_INIT(ib_context_parent_get);
    IB_FTRACE_RET_PTR(ib_context_t, ctx->parent);
}

void ib_context_parent_set(ib_context_t *ctx,
                           ib_context_t *parent)
{
    IB_FTRACE_INIT(ib_context_parent_set);
    ctx->parent = parent;
    IB_FTRACE_RET_VOID();
}

void ib_context_destroy(ib_context_t *ctx)
{
    IB_FTRACE_INIT(ib_context_destroy);
    ib_mpool_destroy(ctx->mp);
    IB_FTRACE_RET_VOID();
}

ib_context_t *ib_context_engine(ib_engine_t *ib)
{
    return ib->ectx;
}

ib_context_t *ib_context_main(ib_engine_t *ib)
{
    return ib->ctx;
}

ib_status_t ib_context_init_cfg(ib_context_t *ctx,
                                const void *base,
                                const ib_cfgmap_init_t *init)
{
    IB_FTRACE_INIT(ib_context_init_cfg);
    ib_status_t rc;

    ib_clog_debug(ctx, 9, "Initializing context config %p base=%p", ctx, base);

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
    IB_FTRACE_INIT(ib_context_module_config);
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
    IB_FTRACE_INIT(ib_context_set);
    ib_status_t rc = ib_cfgmap_set(ctx->cfg, name, val);
    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_context_set_num(ib_context_t *ctx,
                           const char *name,
                           uint64_t val)
{
    IB_FTRACE_INIT(ib_context_set_num);
    ib_status_t rc = ib_cfgmap_set(ctx->cfg, name, (void *)&val);
    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_context_set_string(ib_context_t *ctx,
                              const char *name,
                              const char *val)
{
    IB_FTRACE_INIT(ib_context_set_string);
    ib_status_t rc = ib_cfgmap_set(ctx->cfg, name, (void *)&val);
    IB_FTRACE_RET_STATUS(rc);
}


ib_status_t ib_context_get(ib_context_t *ctx,
                            const char *name,
                            void *pval, ib_ftype_t *ptype)
{
    IB_FTRACE_INIT(ib_context_get);
    ib_status_t rc = ib_cfgmap_get(ctx->cfg, name, pval, ptype);
    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_context_siteloc_chooser(ib_context_t *ctx,
                                       ib_ctype_t type,
                                       void *ctxdata,
                                       void *cbdata)
{
    IB_FTRACE_INIT(ib_context_siteloc_chooser);
    ib_engine_t *ib;
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
    txhost = tx->hostname;
    txhostlen = strlen(txhost);
    txpath = tx->path;
    ib = tx->ib;
    loc = (ib_loc_t *)cbdata;

    ib_log_debug(ib, 9, "CHOOSER: ctx=%p tx=%p loc=%p", ctx, tx, loc);

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
        ib_log_debug(ib, 4, "Checking IP %s against context %s",
                     tx->conn->local_ipstr, ip?ip:"ANY");
        if ((ip == NULL) || (strcmp(ip, tx->conn->local_ipstr) == 0)) {
            numhosts = loc->site->hosts ? ib_list_elements(loc->site->hosts) : 1;
            hostnode = loc->site->hosts ? ib_list_first(loc->site->hosts) : NULL;
            host = hostnode ? (const char *)ib_list_node_data(hostnode) : NULL;

            while (numhosts--) {
                size_t hostlen = host?strlen(host):0;
                off_t cmpoffset = txhostlen - hostlen;
                const char *cmphost = (cmpoffset > 0)?txhost + cmpoffset:NULL;
                if (cmphost != NULL) {
                    ib_log_debug(ib, 4, "Checking Host \"%s\" (effective=\"%s\") against context %s",
                                 txhost, cmphost, (host&&*host)?host:"ANY");
                    if ((host == NULL) || (strcmp(host, cmphost) == 0)) {
                        path = loc->path;

                        ib_log_debug(ib, 4, "Checking Location %s against context %s",
                                     txpath, path?path:"ANY");
                        if ((path == NULL) || (strncmp(path, txpath, strlen(path)) == 0)) {
                            ib_log_debug(ib, 4, "Site \"%s:%s\" matched ctx=%p",
                                         loc->site->name, loc->path, ctx);
                            IB_FTRACE_RET_STATUS(IB_OK);
                        }
                    }
                }
                else {
                    ib_log_debug(ib, 4, "Skipping Host %s check against context %s",
                                 txhost, host?host:"ANY");
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

