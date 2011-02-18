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
 * @brief IronBee - Apache 2.x Plugin
 *
 * @author Brian Rectanus <brectanus@qualys.com>
 */

#include <apr.h>
#include <apr_strings.h>

#include <string.h>

#include <sys/un.h>
#include <arpa/inet.h>

#if defined(__cplusplus) && !defined(__STDC_CONSTANT_MACROS)
/* C99 requires that stdint.h only exposes constant macros
 * for C++ implementations if this is defined: */
#define __STDC_CONSTANT_MACROS
#endif


#if APR_HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <ironbee/engine.h>
#include <ironbee/plugin.h>
#include <ironbee/config.h>
#include <ironbee/module.h> /* Only needed while config is in here. */
#include <ironbee/provider.h>
#include <apache_httpd2.h>

#include <http_main.h>
#include <http_connection.h>
#include <http_request.h>

#include <apr_pools.h>

#define MODULE_NAME                IRONBEE
#define MODULE_NAME_STR            IB_XSTRINGIFY(MODULE_NAME)
#define MODULE_RELEASE             IB_VERSION
#define MODULE_NAME_FULL           (MODULE_NAME_STR " v" MODULE_RELEASE)

/// @todo Fix this:
#ifndef X_MODULE_BASE_PATH
#define X_MODULE_BASE_PATH IB_XSTRINGIFY(MODULE_BASE_PATH) "/"
#endif

#define IRONBEE_DEFAULT_BUFLEN       8192
#define IRONBEE_DEFAULT_FLUSHLEN     1024
#define IRONBEE_IP_MAXSIZE           40

#define IRONBEE_UNSET                ((int)(UINT_MAX))
#define IRONBEE_UNSET_P              ((void *)UINTPTR_MAX)

#define IRONBEE_CONNECT              0
#define IRONBEE_REQUEST              1
#define IRONBEE_RESPONSE             2
#define IRONBEE_DISCONNECT           3
#define IRONBEE_ABORT                4


/* -- Data Structures -- */

module AP_MODULE_DECLARE_DATA ironbee_module;

typedef struct ironbee_conn_context ironbee_conn_context;
typedef struct ironbee_tx_context ironbee_tx_context;

/**
 * @internal
 *
 * Context used for connection buffering/inspecting data.
 */
struct ironbee_conn_context {
    int               direction;
    ib_conndata_t    *qconndata;
};

/**
 * @internal
 *
 * Context used for transaction processing.
 */
struct ironbee_tx_context {
    ib_tx_t          *qtx;
};

/* Plugin Structure */
ib_plugin_t DLL_LOCAL ibplugin = {
    IB_PLUGIN_HEADER_DEFAULTS,
    "apache_2"
};

/* IronBee Handle */
ib_engine_t DLL_LOCAL *ironbee = NULL;


/* -- Logging -- */

/// @todo Change to use ib_provider_inst_t
static void ironbee_logger(server_rec *s, int level,
                           const char *prefix, const char *file, int line,
                           const char *fmt, va_list ap)
{
    char buf[8192 + 1];
    int ap_level;
    int ec;

    /* Buffer the log line. */
    ec = vsnprintf(buf, sizeof(buf), fmt, ap);
    if (ec >= (int)sizeof(buf)) {
        /// @todo Do something about it
        ap_log_error(file, line, APLOG_ALERT, 0, s,
                     MODULE_NAME_STR ": Log format exceeded limit (%d): %.*s",
                     (int)ec, (int)sizeof(buf), buf);
        abort(); /// @todo Testing
        return;
    }

    /* Translate the log level. */
    switch (level) {
        case 0:
            ap_level = APLOG_EMERG;
            break;
        case 1:
            ap_level = APLOG_ALERT;
            break;
        case 2:
            ap_level = APLOG_ERR;
            break;
        case 3:
            ap_level = APLOG_WARNING;
            break;
        case 4:
            ap_level = APLOG_NOTICE;
            break;
        case 9:
            ap_level = APLOG_DEBUG;
            break;
        default:
            ap_level = APLOG_NOTICE; /// @todo Make configurable
    }

    if ((s == NULL) && (ap_level > APLOG_NOTICE)) {
        ap_level = APLOG_NOTICE;
    }

    /* Write it to the error log. */
    ap_log_error(file, line, ap_level, 0, s,
                 MODULE_NAME_STR ": %s", buf);
}

static IB_PROVIDER_IFACE_TYPE(logger) ironbee_logger_iface = {
    IB_PROVIDER_IFACE_HEADER_DEFAULTS,
    (ib_log_logger_fn_t)ironbee_logger
};


/* -- Private Functions -- */

/**
 * @internal
 *
 * Handle the request/response data.
 *
 * This just notifies the engine of the data that is available.
 */
static void handle_data(conn_rec *c, ironbee_conn_context *ctx)
{
    ib_conndata_t *qcdata = ctx->qconndata;
    ib_conn_t *qconn = qcdata->conn;
    
    if (qcdata->dlen == 0) {
        return;
    }

    if (ctx->direction == IRONBEE_REQUEST) {
        /* Check if the current transaction is finished, but there is
         * extraneous data to read. This is skipped in the case of pipelined
         * requests where this extra data may be the next request.
         */
        if (   (qconn->tx != NULL)
            && !ib_tx_flags_isset(qconn->tx, IB_TX_FPIPELINED)
            &&  ib_tx_flags_isset(qconn->tx, IB_TX_FREQ_FINISHED))
        {
            /* Apache would not normally have parsed extraneous data.
             *
             * This can happen, for example, if a GET/HEAD request contained
             * a body. When this happens, Apache ignores this data during the
             * request processing, but the data is still seen by the
             * connection filter.
             */
            ib_tx_flags_set(qconn->tx, IB_TX_FERROR);
            /// @todo Add some flag???
            ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, c->base_server,
                         MODULE_NAME_STR ": EXTRANEOUS DATA "
                         "remote=%pI local=%pI nbytes=%ld %"
                         IB_BYTESTR_FMT,
                         (void *)c->remote_addr, (void *)c->local_addr,
                         (long int)qcdata->dlen,
                         IB_BYTESTRSL_FMT_PARAM(qcdata->data, qcdata->dlen));
        }
        ib_state_notify_conn_data_in(ironbee, qcdata);
    }
    else {
        ib_state_notify_conn_data_out(ironbee, qcdata);
    }

    /* Reset buffer. */
    qcdata->dlen = 0;
}

/**
 * @internal
 *
 * Buffers up the buckets in a per-connection buffer, handling the buffer
 * when a high watermark is met or EOS.
 *
 * This is needed as Apache will use a line oriented filter when it can
 * (request headers and HTML output) and this generates too many small
 * fragments to handle.  Instead this makes sure that the fragments are
 * reasonablly sized to avoid having to deal with potentially small
 * chunks of data.
 */
static void process_bucket(ap_filter_t *f, apr_bucket *b)
{
    conn_rec *c = f->c;
    ironbee_conn_context *ctx = f->ctx;
    ib_conndata_t *qcdata = ctx->qconndata;
    ironbee_config_t *modcfg =
        (ironbee_config_t *)ap_get_module_config(c->base_server->module_config,
                                               &ironbee_module);
    apr_size_t nbytes;
    apr_size_t blen;

//    ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, c->base_server,
//                 MODULE_NAME_STR ": bucket f=%s, b=%s len=%d rnum=%d",
//                 f->frec->name, b->type->name, (int)b->length, c->keepalives);

    /* Only interested in buckets with data, FLUSH, EOS and EOC. */
    if (APR_BUCKET_IS_EOS(b) || APR_BUCKET_IS_FLUSH(b)) {
        handle_data(c, ctx);
    }
    else if (APR_BUCKET_IS_METADATA(b)) {
        /* Detect an end of connection and process and pending data. */
        if (strcmp("EOC", b->type->name) == 0) {
            handle_data(c, ctx);
        }
        return;
    }

    /* 
     * The read may not get all the data or may get too much data.  If too
     * much data the bucket is split (b will become the new smaller bucket)
     * and the rest of the data will be read next time.
     */
    do {
        const char *bdata;
        size_t buf_remain = qcdata->dalloc - qcdata->dlen;
        nbytes = 0;
        blen = b->length;

        /* Need to manually split the bucket if too long. */
        if (blen > buf_remain) {
            ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, c->base_server,
                         MODULE_NAME_STR ": split bucket %d/%d",
                         (int)buf_remain, (int)(blen - buf_remain));
            apr_bucket_split(b, buf_remain);
        }

        if (blen &&
            (apr_bucket_read(b, &bdata, &nbytes, APR_BLOCK_READ) != APR_SUCCESS))
        {
            ap_log_error(APLOG_MARK, APLOG_ERR, 0, c->base_server,
                         MODULE_NAME_STR ": %s (%s): error reading %s data",
                         f->frec->name, b->type->name,
                         ((ctx->direction == IRONBEE_REQUEST) ? "request" : "response"));

            return;
        }

        if (nbytes) {
            /* Copy the data from the bucket to our buffer. */
            memcpy(qcdata->data + qcdata->dlen, bdata, nbytes);
            qcdata->dlen += nbytes;
        }

        /* Only handle the data if the buffer is full enough to flush. */
        if (qcdata->dlen >= modcfg->flush_size) {
            handle_data(c, ctx);
        }
    } while (blen < nbytes);
}

/**
 * @internal
 *
 * Called by the connection cleanup routine, handling a disconnect.
 */
static apr_status_t ironbee_disconnection(void *data)
{
    conn_rec *c= (conn_rec *)data;
    server_rec *s = c->base_server;
    ironbee_conn_context *ctx_in;
    ironbee_conn_context *ctx_out;

    if (data == NULL) {
        return OK;
    }

    /* Make sure to handle any pending input. */
    ctx_in = (ironbee_conn_context *)apr_table_get(c->notes, "IRONBEE_CTX_IN");
    if (ctx_in == NULL) {
        /* Do not bother if not connected. */
        abort(); /// @todo Testing
        return OK;
    }
    else if (ctx_in->qconndata->dlen) {
        ap_log_error(APLOG_MARK, APLOG_WARNING, 0, s,
                     MODULE_NAME_STR ": Handling any request data on disconnect");
        handle_data(c, ctx_in);
    }

    /* Make sure to handle any pending output. */
    ctx_out = (ironbee_conn_context *)apr_table_get(c->notes, "IRONBEE_CTX_OUT");
    if (ctx_out == NULL) {
        /* Do not bother if not connected. */
        abort(); /// @todo Testing
        return OK;
    }
    if (ctx_out->qconndata->dlen) {
        ap_log_error(APLOG_MARK, APLOG_WARNING, 0, s,
                     MODULE_NAME_STR ": Handling any response data on disconnect");
        handle_data(c, ctx_out);
    }

/// @todo Handle these differently in the future?  Conn structure flags?
#if 0
    if (c->aborted) {
    }
    else {
    }
#endif

    ib_state_notify_conn_closed(ironbee, ctx_in->qconndata->conn);

    /* Done with the connection. */
    /// @todo Perhaps the engine should do this instead via an event???
    ib_log_debug(ironbee, 9, "Destroying connection structure");
    ib_conn_destroy(ctx_in->qconndata->conn);

    return OK;
}

/**
 * @internal
 *
 * Cleanup.
 */
static apr_status_t ironbee_module_cleanup(void *data)
{
    ib_engine_destroy(ironbee);

    return APR_SUCCESS;
}

/**
 * @internal
 *
 * Called when the child process exits.
 */
static apr_status_t ironbee_child_exit(void *data)
{
    server_rec *s = (server_rec *)data;

    ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s,
                 MODULE_NAME_STR ": Child exit pid=%d",
                 (int)getpid());

    return APR_SUCCESS;
}

/**
 * @internal
 *
 * Called when the child process is created.
 */
static void ironbee_child_init(apr_pool_t *p, server_rec *s)
{
    ironbee_config_t *modcfg =
        (ironbee_config_t *)ap_get_module_config(s->module_config,
                                               &ironbee_module);

    if (!modcfg->enabled) {
        return;
    }

    ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s,
                 MODULE_NAME_STR ": Child init pid=%d",
                 (int)getpid());

    /* Register callback when child exits. */
    apr_pool_cleanup_register(p, s, ironbee_child_exit, apr_pool_cleanup_null);
}

/**
 * @internal
 *
 * Setup the connection structures, filters and a disconnect handler.
 */
static int ironbee_pre_connection(conn_rec *c, void *csd)
{
    ib_conn_t *qconn = NULL;
    ib_status_t rc;
    ironbee_conn_context *ctx_in;
    ironbee_conn_context *ctx_out;
    ironbee_config_t *modcfg =
        (ironbee_config_t *)ap_get_module_config(c->base_server->module_config,
                                               &ironbee_module);

    if (!modcfg->enabled) {
        return DECLINED;
    }

    ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, c->base_server,
                 MODULE_NAME_STR ": ironbee_pre_connection remote=%pI local=%pI",
                 (void *)c->remote_addr, (void *)c->local_addr);

    /* Ignore backend connections. The backend connection does not have
     * a handle to the scoreboard. */
    if (c->sbh == NULL) {
        ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, c->base_server,
                     MODULE_NAME_STR ": Skipping proxy connect");
        return DECLINED;
    }

    /* Create the connection structure. */
    /// @todo Perhaps the engine should do this instead via an event???
    ib_log_debug(ironbee, 9, "Creating connection structure");
    rc = ib_conn_create(ironbee, &qconn, c);
    if (rc != IB_OK) {
        return DECLINED;
    }

    /* Tell the engine a connection has started. */
    ib_state_notify_conn_opened(ironbee, qconn);

    /* Create the incoming context. */
    ctx_in = apr_pcalloc(c->pool, sizeof(*ctx_in));
    rc = ib_conn_data_create(qconn, &ctx_in->qconndata, modcfg->buf_size);
    if (rc != IB_OK) {
        return DECLINED;
    }
    ctx_in->direction = IRONBEE_REQUEST;
    apr_table_setn(c->notes, "IRONBEE_CTX_IN", (void *)ctx_in);

    /* Create the outgoing context. */
    ctx_out = apr_pcalloc(c->pool, sizeof(*ctx_out));
    rc = ib_conn_data_create(qconn, &ctx_out->qconndata, modcfg->buf_size);
    if (rc != IB_OK) {
        return DECLINED;
    }
    ctx_out->direction = IRONBEE_RESPONSE;
    apr_table_setn(c->notes, "IRONBEE_CTX_OUT", (void *)ctx_out);

    /* Register callback on disconnect. */
    /// @todo use apr_pool_pre_cleanup_register() when APR >= 1.3
    apr_pool_cleanup_register(c->pool, c, ironbee_disconnection, NULL);

    /* Add the connection level filters which generate I/O events. */
    ap_add_input_filter("IRONBEE_IN", ctx_in, NULL, c);
    ap_add_output_filter("IRONBEE_OUT", ctx_out, NULL, c);

    return OK;
}


/* -- IronBee Hooks -- */

/**
 * @internal
 *
 * Called to initialize data in a new connection.
 */
static ib_status_t ironbee_conn_init(ib_engine_t *ib,
                                   ib_conn_t *qconn, void *cbdata)
{
    //server_rec *s = cbdata;
    conn_rec *c = (conn_rec *)qconn->pctx;
    ib_status_t rc;

    ib_clog_debug(qconn->ctx, 9, "Initializing connection remote=%s:%d local=%s:%d",
                  c->remote_ip, c->remote_addr->port,
                  c->local_ip, c->local_addr->port);

    /*
     * Create connection fields
     */

    /* remote_ip */
    qconn->remote_ipstr = c->remote_ip;
    rc = ib_data_add_bytestr(qconn->dpi,
                             "remote_ip",
                             (uint8_t *)c->remote_ip,
                             strlen(c->remote_ip),
                             NULL);
    if (rc != IB_OK) {
        return rc;
    }

    /* remote_port */
    qconn->remote_port = c->remote_addr->port;
    rc = ib_data_add_num(qconn->dpi,
                         "remote_port",
                         c->remote_addr->port,
                         NULL);
    if (rc != IB_OK) {
        return rc;
    }

    /* local_ip */
    qconn->local_ipstr = c->local_ip;
    rc = ib_data_add_bytestr(qconn->dpi,
                             "local_ip",
                             (uint8_t *)c->remote_ip,
                             strlen(c->remote_ip),
                             NULL);
    if (rc != IB_OK) {
        return rc;
    }

    /* local_port */
    qconn->local_port = c->local_addr->port;
    rc = ib_data_add_num(qconn->dpi,
                         "local_port",
                         c->local_addr->port,
                         NULL);
    if (rc != IB_OK) {
        return rc;
    }

    return IB_OK;
}



/* -- Filters -- */

/**
 * @internal
 *
 * "Sniffs" the input (request) data from the connection stream and tries
 * to determine who closed a connection and why.
 */
static int ironbee_input_filter(ap_filter_t *f, apr_bucket_brigade *bb,
                                ap_input_mode_t mode, apr_read_type_e block,
                                apr_off_t readbytes)
{
    conn_rec *c = f->c;
    ironbee_conn_context *ctx = f->ctx;
    apr_bucket *b;
    apr_status_t rc;

    /// @todo Should we loop here to grab as much data as we can???
    rc = ap_get_brigade(f->next, bb, mode, block, readbytes);
    if (rc == APR_SUCCESS) {
        for (b = APR_BRIGADE_FIRST(bb); b != APR_BRIGADE_SENTINEL(bb); b = APR_BUCKET_NEXT(b)) {
            process_bucket(f, b);
        }
        /// @todo Configurable?  This is so that a large buffer does not
        ///       make the connection filter processing delayed until after
        ///       the transaction processing (the filter should see the
        ///       headers before the request_headers_event hook(s).

        handle_data(c, ctx);
    }
    else if (APR_STATUS_IS_TIMEUP(rc)) {
        ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, c->base_server,
                     MODULE_NAME_STR ": %s server closed connection (%d)",
                     f->frec->name, rc);

        /* Handle any data that is there and remove the filter. */
        handle_data(c, ctx);
        ap_remove_input_filter(f);
    }
    else if (APR_STATUS_IS_EOF(rc) || apr_get_os_error() == ECONNRESET) {
        ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, c->base_server,
                     MODULE_NAME_STR ": %s client closed connection (%d)",
                     f->frec->name, rc);

        /* Handle any data that is there and remove the filter. */
        handle_data(c, ctx);
        ap_remove_input_filter(f);
    }
    else {
        ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, c->base_server,
                     MODULE_NAME_STR ": %s returned %d (0x%08x) - %s",
                     f->frec->name, rc, rc, strerror(apr_get_os_error()));

        /* Handle any data that is there. */
        handle_data(c, ctx);
    }


    return APR_SUCCESS;
}

/**
 * @internal
 *
 * "Sniffs" the output (response) data from the connection stream.
 */
static int ironbee_output_filter (ap_filter_t *f, apr_bucket_brigade *bb)
{
    apr_bucket *b;

    for (b = APR_BRIGADE_FIRST(bb); b != APR_BRIGADE_SENTINEL(bb); b = APR_BUCKET_NEXT(b)) {
        if (APR_BUCKET_IS_EOS(b)) {
            /// @todo Do we need to do this? Maybe only for proxy.
            apr_bucket *flush = apr_bucket_flush_create(f->c->bucket_alloc);
            APR_BUCKET_INSERT_BEFORE(b, flush);
        }

        process_bucket(f, b);
    }

    return ap_pass_brigade(f->next, bb);
}


/* -- Configuration -- */

/**
 * @internal
 *
 * Called to create a configuration context.
 */
static void *ironbee_create_config(apr_pool_t *p, server_rec *s)
{
    ironbee_config_t *modcfg =
        (ironbee_config_t *)apr_pcalloc(p, sizeof(ironbee_config_t));

    if (modcfg == NULL) {
        return NULL;
    }

    modcfg->enabled = 0;
    modcfg->buf_size = IRONBEE_DEFAULT_BUFLEN;
    modcfg->flush_size = IRONBEE_DEFAULT_FLUSHLEN;

    return modcfg;
}

/**
 * @internal
 *
 * Called to merge the parent and child configuration contexts.
 */
static void *ironbee_merge_config(apr_pool_t *p, void *parent, void *child)
{
    ironbee_config_t *modcfgp = (ironbee_config_t *)parent;
    ironbee_config_t *modcfgc = (ironbee_config_t *)child;
    ironbee_config_t *modcfg = ironbee_create_config(p, NULL);

    if (modcfg == NULL) {
        return NULL;
    }

    modcfg->enabled = (modcfgc->enabled == IRONBEE_UNSET) ? modcfgp->enabled
                                                        : modcfgc->enabled;

    return modcfg;
}

/**
 * Initializes and configures the ironbee engine.
 */
static int ironbee_post_config(apr_pool_t *p, apr_pool_t *plog, apr_pool_t *ptmp,
                             server_rec *s)
{
    ironbee_config_t *modcfg =
        (ironbee_config_t *)ap_get_module_config(s->module_config,
                                                 &ironbee_module);
    ib_cfgparser_t *cp;
    void *init = NULL;
    ib_status_t rc;


    /* Init IB library. */
    rc = ib_initialize();
    if (rc != IB_OK) {
        ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s,
                     MODULE_NAME_STR ": Error initializing ib library");
        return OK;
    }

    ib_util_log_level(4);

    /* Detect first (validation) run vs real config run. */
    apr_pool_userdata_get(&init, "ironbee-init", s->process->pool);
    if (init == NULL) {
        apr_pool_userdata_set((const void *)1, "ironbee-init",
                              apr_pool_cleanup_null, s->process->pool);

        return OK;
    }

    /// @todo Tracefile needs removed
    //ib_trace_init("/tmp/ironbee.trace");
    ib_trace_init(NULL);

    /* Create the engine handle. */
    rc = ib_engine_create(&ironbee, &ibplugin);
    if (rc != IB_OK) {
        ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s,
                     MODULE_NAME_STR ": Error creating engine: %d", rc);
        return OK;
    }

    /* Register the logger. */
    rc = ib_provider_register(ironbee, IB_PROVIDER_NAME_LOGGER,
                              MODULE_NAME_STR, NULL,
                              &ironbee_logger_iface,
                              NULL);
    if (rc != IB_OK) {
        ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s,
                     MODULE_NAME_STR ": Error registering log provider: %d", rc);
        return OK;
    }

    /* Default logger */
    ib_context_set_string(ib_context_engine(ironbee),
                          IB_PROVIDER_NAME_LOGGER,
                          MODULE_NAME_STR);
    ib_context_set_num(ib_context_engine(ironbee),
                       IB_PROVIDER_NAME_LOGGER ".log_level",
                       4);


    rc = ib_engine_init(ironbee);
    if (rc != IB_OK) {
        ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s,
                     MODULE_NAME_STR ": Error initializing engine: %d", rc);
        return OK;
    }

    /* Register module cleanup. */
    apr_pool_cleanup_register(p, (void *)s, ironbee_module_cleanup,
                              apr_pool_cleanup_null);

    /* Register conn/tx init hooks. */
    ib_hook_register(ironbee, conn_opened_event,
                     (ib_void_fn_t)ironbee_conn_init, s);

    /* Configure the engine. */
    if (modcfg->config != NULL) {
        ib_context_t *ctx;

        /* Create and configure the main configuration context. */
        /// @todo This needs to go in the engine, not plugin
        ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s,
                     MODULE_NAME_STR ": Creating main context");
        ib_context_create_main(&ctx, ironbee);

        /* Set some defaults */
        ib_context_set_string(ctx, IB_PROVIDER_NAME_LOGGER, MODULE_NAME_STR);
        ib_context_set_num(ctx, "logger.log_level", 4);

        /* Parse the config file. */
        rc = ib_cfgparser_create(&cp, ironbee);
        if ((rc == IB_OK) && (cp != NULL)) {
            ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s,
                         MODULE_NAME_STR ": Parsing config: %s",
                         modcfg->config);
            ib_cfgparser_parse(cp, modcfg->config);
            ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s,
                         MODULE_NAME_STR ": Destroying config parser");
            ib_cfgparser_destroy(cp);
        }

        /* Initialize (and close) the main configuration context. */
        ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s,
                     MODULE_NAME_STR ": Initializing main context");
        rc = ib_context_init(ctx);
        if (rc != IB_OK) {
            ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s,
                         MODULE_NAME_STR ": Error initializing main context");
            return OK;
        }
    }
    else {
        ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s,
                     MODULE_NAME_STR ": No config specified with IronBeeConfig directive");
    }

    /* Destroy the temporary memory pool. */
    ib_engine_pool_temp_destroy(ironbee);

    return OK;
}

/**
 * @internal
 *
 * Called to handle the "IronBeeEnable" configuration directive.
 */
static const char *ironbee_cmd_ibenable(cmd_parms *cmd,
                                        void *dummy,
                                        int flag)
{
    ironbee_config_t *modcfg;

    if (cmd->server->is_virtual) {
        return MODULE_NAME_STR ": IronBeeEnable not allowed in VirtualHost";
    }

    modcfg = (ironbee_config_t *)ap_get_module_config(cmd->server->module_config,
                                                      &ironbee_module);
    if (modcfg == NULL) {
        return NULL;
    }

    modcfg->enabled = flag;

    return NULL;
}

/**
 * @internal
 *
 * Called to handle the "IronBeeConfig" configuration directive.
 */
static const char *ironbee_cmd_ibconfig(cmd_parms *cmd,
                                        void *dummy,
                                        const char *p1)
{
    ironbee_config_t *modcfg;

    if (cmd->server->is_virtual) {
        return MODULE_NAME_STR ": IronBeeConfig not allowed in VirtualHost";
    }

    modcfg = (ironbee_config_t *)ap_get_module_config(cmd->server->module_config,
                                                      &ironbee_module);
    if (modcfg == NULL) {
        return NULL;
    }

    modcfg->config = p1;

    return NULL;
}

/**
 * @internal
 *
 * Called to handle the "IronBeeBufferSize" configuration directive.
 */
static const char *ironbee_cmd_ibbuffersize(cmd_parms *cmd,
                                            void *dummy,
                                            const char *p1)
{
    ironbee_config_t *modcfg;
    long longval;

    if (cmd->server->is_virtual) {
        return MODULE_NAME_STR ": IronBeeBufferSize not allowed in VirtualHost";
    }

    modcfg = (ironbee_config_t *)ap_get_module_config(cmd->server->module_config,
                                                      &ironbee_module);
    if (modcfg == NULL) {
        return NULL;
    }

    longval = atol(p1);
    if (longval <= 0) {
        longval = IRONBEE_DEFAULT_BUFLEN;
    }

    modcfg->buf_size = (size_t)longval;

    return NULL;
}

/**
 * @internal
 *
 * Called to handle the "IronBeeBufferFlushSize" configuration directive.
 */
static const char *ironbee_cmd_ibbufferflushsize(cmd_parms *cmd,
                                                 void *dummy,
                                                 const char *p1)
{
    ironbee_config_t *modcfg;
    long longval;

    if (cmd->server->is_virtual) {
        return MODULE_NAME_STR ": IronBeeBufferFlushSize not allowed in VirtualHost";
    }

    modcfg = (ironbee_config_t *)ap_get_module_config(cmd->server->module_config,
                                                      &ironbee_module);
    if (modcfg == NULL) {
        return NULL;
    }

    longval = atol(p1);
    if (longval <= 0) {
        longval = IRONBEE_DEFAULT_FLUSHLEN;
    }

    modcfg->flush_size = (size_t)longval;

    return NULL;
}

/**
 * @internal
 *
 * Declares all configuration directives.
 */
static const command_rec ironbee_cmds[] = {
    AP_INIT_FLAG(
      "IronBeeEnable",
      ironbee_cmd_ibenable,
      NULL,
      RSRC_CONF,
      "enable ironbee module"
    ),
    AP_INIT_TAKE1(
      "IronBeeConfig",
      ironbee_cmd_ibconfig,
      NULL,
      RSRC_CONF,
      "specify ironbee configuration file"
    ),
    AP_INIT_TAKE1(
      "IronBeeBufferSize",
      ironbee_cmd_ibbuffersize,
      NULL,
      RSRC_CONF,
      "specify buffer size (bytes)"
    ),
    AP_INIT_TAKE1(
      "IronBeeBufferFlushSize",
      ironbee_cmd_ibbufferflushsize,
      NULL,
      RSRC_CONF,
      "specify buffer size (bytes) to trigger a flush"
    ),
    { NULL }
};


/* -- Misc Modules Stuff -- */

/**
 * @internal
 *
 * Register functions to handle filters and hooks.
 */
static void ironbee_register_hooks(apr_pool_t *p)
{
    /* Other modules:
     *   mod_ssl       = AP_FTYPE_CONNECTION + 5
     *   mod_expires   = AP_FTYPE_CONTENT_SET - 2
     *   mod_cache     = AP_FTYPE_CONTENT_SET - 1
     *   mod_deflate   = AP_FTYPE_CONTENT_SET - 1
     *   mod_headers   = AP_FTYPE_CONTENT_SET
     */

    ap_register_input_filter(
        "IRONBEE_IN",
        ironbee_input_filter,
        NULL,
        AP_FTYPE_CONNECTION
    );

    ap_register_output_filter(
        "IRONBEE_OUT",
        ironbee_output_filter,
        NULL,
        AP_FTYPE_CONNECTION
    );

    ap_hook_child_init(ironbee_child_init,
                       NULL, NULL,
                       APR_HOOK_FIRST);

    ap_hook_post_config(ironbee_post_config,
                        NULL, NULL,
                        APR_HOOK_MIDDLE);

    ap_hook_pre_connection(ironbee_pre_connection,
                           NULL, NULL,
                           APR_HOOK_LAST);
}

/**
 * @internal
 *
 * Declare the module.
 */
module AP_MODULE_DECLARE_DATA ironbee_module = {
    STANDARD20_MODULE_STUFF,
    NULL,                       /* per-directory config creator */
    NULL,                       /* dir config merger */
    ironbee_create_config,      /* server config creator */
    ironbee_merge_config,       /* server config merger */
    ironbee_cmds,               /* command table */
    ironbee_register_hooks      /* set up other request processing hooks */
};
