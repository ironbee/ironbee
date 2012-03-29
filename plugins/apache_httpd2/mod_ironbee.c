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

#include <assert.h>

#include <ironbee/engine.h>
#include <ironbee/plugin.h>
#include <ironbee/config.h>
#include <ironbee/module.h> /* Only needed while config is in here. */
#include <ironbee/core.h>   /* Only needed while config is in here. */
#include <ironbee/provider.h>
#include <ironbee/debug.h>
#include <ironbee/util.h>

#include <apache_httpd2.h>

#include <http_main.h>
#include <http_connection.h>
#include <http_request.h>

#include <apr_pools.h>

#define MODULE_NAME                mod_ironbee
#define MODULE_NAME_STR            IB_XSTRINGIFY(MODULE_NAME)
#define MODULE_RELEASE             IB_VERSION
#define MODULE_NAME_FULL           MODULE_NAME_STR " " \
                                   IB_PRODUCT_VERSION_NAME \
                                   " (ABI " IB_XSTRINGIFY(IB_ABINUM) ")"

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

// APR uses char* instead of void* for it's generic pointer type.  This
// causes some false warnings.  We're okay, because the values we put in to
// APR are properly aligned.
#ifdef __clang__
#pragma clang diagnostic ignored "-Wcast-align"
#endif

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
    int                      direction;
    ib_conn_t               *iconn;
};

/**
 * @internal
 *
 * Context used for transaction processing.
 */
struct ironbee_tx_context {
    ib_tx_t                 *itx;
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
    int limit = 7000;
    int ap_level;
    int ec;

    /* Buffer the log line. */
    ec = vsnprintf(buf, sizeof(buf), fmt, ap);
    if (ec >= limit) {
        /* Mark as truncated, with a " ...". */
        memcpy(buf + (limit - 5), " ...", 5);

        /// @todo Do something about it
        ap_log_error(APLOG_MARK, APLOG_WARNING, 0, s,
                     IB_PRODUCT_NAME ": Log format truncated: limit (%d/%d)",
                     (int)ec, limit);
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
            ap_level = APLOG_DEBUG; /// @todo For now, so we get file/line
            break;
        case 9:
            ap_level = APLOG_DEBUG;
            break;
        default:
            ap_level = APLOG_DEBUG; /// @todo Make configurable
    }

    /// @todo Make configurable
    if ((s == NULL) && (ap_level > APLOG_NOTICE)) {
        ap_level = APLOG_NOTICE;
    }

    /* Write it to the error log. */
    ap_log_error(APLOG_MARK, ap_level, 0, s,
                 IB_PRODUCT_NAME ": %s%s", prefix?prefix:"", buf);
}

static IB_PROVIDER_IFACE_TYPE(logger) ironbee_logger_iface = {
    IB_PROVIDER_IFACE_HEADER_DEFAULTS,
    (ib_log_logger_fn_t)ironbee_logger
};


/* -- Private Functions -- */

/**
 * @internal
 *
 * Sends bucket data to ironbee for processing.
 */
static void process_bucket(ap_filter_t *f, apr_bucket *b)
{
    conn_rec *c = f->c;
    ironbee_conn_context *ctx = f->ctx;
    ib_conndata_t icdata;
    const char *bdata;
    apr_size_t nbytes;
    apr_status_t rc;

    if (APR_BUCKET_IS_METADATA(b)) {
        return;
    }

    /* Translate a bucket to a ib_conndata_t structure to be passed
     * to IronBee. */
    rc = apr_bucket_read(b, &bdata, &nbytes, APR_BLOCK_READ);
    if (rc != APR_SUCCESS) {
        ap_log_error(APLOG_MARK, APLOG_ERR, 0, c->base_server,
                     IB_PRODUCT_NAME ": %s (%s): error reading %s data",
                     f->frec->name, b->type->name,
                     ((ctx->direction == IRONBEE_REQUEST) ? "request" : "response"));
            return;
    }
    icdata.ib = ironbee;
    icdata.mp = ctx->iconn->mp;
    icdata.conn = ctx->iconn;
    icdata.dalloc = nbytes;
    icdata.dlen = nbytes;
    icdata.data = (uint8_t *)bdata;


    if (ctx->direction == IRONBEE_REQUEST) {
        ib_state_notify_conn_data_in(ironbee, &icdata);
    }
    else {
        ib_state_notify_conn_data_out(ironbee, &icdata);
    }
}

/**
 * @internal
 *
 * Called by the connection cleanup routine, handling a disconnect.
 */
static apr_status_t ironbee_disconnection(void *data)
{
    conn_rec *c= (conn_rec *)data;
    ironbee_conn_context *ctx_in;

    if (data == NULL) {
        return OK;
    }

    ctx_in = (ironbee_conn_context *)apr_table_get(c->notes, "IRONBEE_CTX_IN");
    ib_state_notify_conn_closed(ironbee, ctx_in->iconn);

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
                 IB_PRODUCT_NAME ": Child exit pid=%d",
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
                 IB_PRODUCT_NAME ": Child init pid=%d",
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
    ib_conn_t *iconn = NULL;
    ib_status_t rc;
    ironbee_conn_context *ctx_in;
    ironbee_conn_context *ctx_out;
    ironbee_config_t *modcfg =
        (ironbee_config_t *)ap_get_module_config(c->base_server->module_config,
                                               &ironbee_module);

    if (!modcfg->enabled) {
        return DECLINED;
    }

    /* Ignore backend connections. The backend connection does not have
     * a handle to the scoreboard. */
    if (c->sbh == NULL) {
        ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, c->base_server,
                     IB_PRODUCT_NAME ": Skipping proxy connect");
        return DECLINED;
    }

    ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, c->base_server,
                 IB_PRODUCT_NAME ": ironbee_pre_connection remote=%pI local=%pI",
                 (void *)c->remote_addr, (void *)c->local_addr);

    /* Create the connection structure. */
    /// @todo Perhaps the engine should do this instead via an event???
    ib_log_debug(ironbee, 9, "Creating connection structure");
    rc = ib_conn_create(ironbee, &iconn, c);
    if (rc != IB_OK) {
        return DECLINED;
    }

    /* Tell the engine a connection has started. */
    ib_state_notify_conn_opened(ironbee, iconn);

    /* Create the incoming context. */
    ctx_in = apr_pcalloc(c->pool, sizeof(*ctx_in));
    ctx_in->iconn = iconn;
    ctx_in->direction = IRONBEE_REQUEST;
    apr_table_setn(c->notes, "IRONBEE_CTX_IN", (void *)ctx_in);

    /* Create the outgoing context. */
    ctx_out = apr_pcalloc(c->pool, sizeof(*ctx_out));
    ctx_out->iconn = iconn;
    ctx_out->direction = IRONBEE_RESPONSE;
    apr_table_setn(c->notes, "IRONBEE_CTX_OUT", (void *)ctx_out);

    /* Register callback on disconnect. */
    /// @todo use apr_pool_pre_cleanup_register() when APR >= 1.3
    apr_pool_cleanup_register(c->pool, c, ironbee_disconnection, NULL);

    /* Add the connection level filters which generate I/O events. */
    ap_add_input_filter("IRONBEE_IN", ctx_in, NULL, c);
#ifdef IB_DEBUG
    ap_add_input_filter("IRONBEE_DBG_IN", ctx_in, NULL, c);
#endif
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
                                     ib_state_event_type_t event,
                                     ib_conn_t *iconn,
                                     void *cbdata)
{
    assert(event == conn_opened_event);

    //server_rec *s = cbdata;
    conn_rec *c = (conn_rec *)iconn->pctx;
    ib_status_t rc;

    ib_clog_debug(iconn->ctx, 9, "Initializing connection remote=%s:%d local=%s:%d",
                  c->remote_ip, c->remote_addr->port,
                  c->local_ip, c->local_addr->port);

    /*
     * Create connection fields
     */

    /* remote_ip */
    iconn->remote_ipstr = c->remote_ip;
    rc = ib_data_add_bytestr(iconn->dpi,
                             "remote_ip",
                             (uint8_t *)c->remote_ip,
                             strlen(c->remote_ip),
                             NULL);
    if (rc != IB_OK) {
        return rc;
    }

    /* remote_port */
    iconn->remote_port = c->remote_addr->port;
    rc = ib_data_add_num(iconn->dpi,
                         "remote_port",
                         c->remote_addr->port,
                         NULL);
    if (rc != IB_OK) {
        return rc;
    }

    /* local_ip */
    iconn->local_ipstr = c->local_ip;
    rc = ib_data_add_bytestr(iconn->dpi,
                             "local_ip",
                             (uint8_t *)c->remote_ip,
                             strlen(c->remote_ip),
                             NULL);
    if (rc != IB_OK) {
        return rc;
    }

    /* local_port */
    iconn->local_port = c->local_addr->port;
    rc = ib_data_add_num(iconn->dpi,
                         "local_port",
                         c->local_addr->port,
                         NULL);
    if (rc != IB_OK) {
        return rc;
    }

    return IB_OK;
}



/* -- Filters -- */

#ifdef IB_DEBUG
/**
 * @internal
 *
 * Just logs data that comes from the primary input filter.
 *
 * Anything this filter sees should be what Apache sees.
 */
static int ironbee_dbg_input_filter(ap_filter_t *f, apr_bucket_brigade *bb,
                                ap_input_mode_t mode, apr_read_type_e block,
                                apr_off_t readbytes)
{
    conn_rec *c = f->c;
    apr_bucket *b;
    apr_status_t rc;

#if 0
    ap_filter_t *xf = f;
    do {
        ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, c->base_server,
                     "DBGFILTER: type=%d mode=%d block=%d readbytes=%d %s", xf->frec->ftype, mode, block, (int)readbytes, xf->frec->name);
    } while((xf = xf->next) != NULL);
#endif

    rc = ap_get_brigade(f->next, bb, mode, block, readbytes);
    if (rc == APR_SUCCESS) {

        /* Process data. */
        for (b = APR_BRIGADE_FIRST(bb); b != APR_BRIGADE_SENTINEL(bb); b = APR_BUCKET_NEXT(b)) {
            ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, c->base_server,
                         IB_PRODUCT_NAME ": DBG bucket f=%s, b=%s len=%d",
                         f->frec->name, b->type->name, (int)b->length);
        }
    }

    return APR_SUCCESS;
}
#endif

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
    ib_conn_t *iconn = ctx->iconn;
    ib_core_cfg_t *corecfg;
    ib_stream_t *istream;
    apr_bucket *b;
    apr_status_t rc;
    int buffering = 0;

    /* Any mode not handled just gets passed through. */
    if ((mode != AP_MODE_GETLINE) && (mode != AP_MODE_READBYTES)) {
        return ap_get_brigade(f->next, bb, mode, block, readbytes);
    }

    /* Configure. */
    ib_context_module_config(iconn->ctx, ib_core_module(), (void *)&corecfg);
    if (corecfg != NULL) {
        buffering = (int)corecfg->buffer_req;
    }

    /* When buffering, data is removed from the brigade and handed
     * to IronBee. The filter must not return an empty brigade in this
     * case and keeps reading until there is processed data that comes
     * back from IronBee.
     */
    do {
        ib_tx_t *itx = iconn->tx;

        /* If there is any processed data, then send it now. */
        if (buffering && (itx != NULL)) {
            ib_sdata_t *sdata;

            /* Take any data from the drain (processed data) and
             * inject it back into the filter brigade.
             */
            ib_fctl_drain(itx->fctl, &istream);
            if ((istream != NULL) && (istream->nelts > 0)) {
                int done = 0;

                while (!done) {
                    apr_bucket *ibucket = NULL;

                    /// @todo Handle multi-bucket lines
                    if (mode == AP_MODE_GETLINE) {
                        done = 1;
                    }

                    ib_stream_pull(istream, &sdata);
                    if (sdata == NULL) {
                        /* No more data left. */
                        break;
                    }

                    switch (sdata->type) {
                        case IB_STREAM_DATA:
#ifdef IB_DEBUG
                            ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, c->base_server,
                                         IB_PRODUCT_NAME ": DATA[%d]: %.*s", (int)sdata->dlen, (int)sdata->dlen, (char *)sdata->data);
#endif

                            /// @todo Is this creating a copy?  Just need a reference.
                            ibucket = apr_bucket_heap_create(sdata->data,
                                                             sdata->dlen,
                                                             NULL,
                                                             bb->bucket_alloc);
                            break;
                        case IB_STREAM_FLUSH:
#ifdef IB_DEBUG
                            ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, c->base_server,
                                         IB_PRODUCT_NAME ": FLUSH");
#endif
                            ibucket = apr_bucket_flush_create(bb->bucket_alloc);
                            break;
                        case IB_STREAM_EOH:
#ifdef IB_DEBUG
                            ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, c->base_server,
                                         IB_PRODUCT_NAME ": EOH");
#endif
                            /// @todo Do something here???
                            break;
                        case IB_STREAM_EOB:
#ifdef IB_DEBUG
                            ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, c->base_server,
                                         IB_PRODUCT_NAME ": EOB");
#endif
                            /// @todo Do something here???
                            break;
                        case IB_STREAM_EOS:
#ifdef IB_DEBUG
                            ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, c->base_server,
                                         IB_PRODUCT_NAME ": EOS");
#endif
                            ibucket = apr_bucket_eos_create(bb->bucket_alloc);
                            break;
                        default:
                            ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, c->base_server,
                                         IB_PRODUCT_NAME ": UNKNOWN stream data type %d", sdata->type);
                    }

                    if (ibucket != NULL) {
                        APR_BRIGADE_INSERT_TAIL(bb, ibucket);
                    }
                }

                /* Need to send any processed data to avoid deadlock. */
                if (!APR_BRIGADE_EMPTY(bb)) {
                    return APR_SUCCESS;
                }
            }
        }

        /* Fetch data from the next filter. */
        if (buffering) {
            ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, c->base_server,
                         "FETCH BRIGADE (buffering)");

            /* Normally Apache will request the headers line-by-line, but
             * IronBee does not require this.  So, here the request is
             * fetched with READBYTES and IronBee will then break
             * it back up into lines when it is injected back into
             * the brigade after the data is processed.
             */
            rc = ap_get_brigade(f->next,
                                bb,
                                AP_MODE_READBYTES,
                                block,
                                HUGE_STRING_LEN);
        }
        else {
            ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, c->base_server,
                         "FETCH BRIGADE (non-buffering)");
            rc = ap_get_brigade(f->next, bb, mode, block, readbytes);
        }

        /* Check for any timeouts/disconnects/errors. */
        if (APR_STATUS_IS_TIMEUP(rc)) {
            ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, c->base_server,
                         IB_PRODUCT_NAME ": %s server closed connection (%d)",
                         f->frec->name, rc);

            ap_remove_input_filter(f);
            return rc;
        }
        else if (APR_STATUS_IS_EOF(rc) || apr_get_os_error() == ECONNRESET) {
            ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, c->base_server,
                         IB_PRODUCT_NAME ": %s client closed connection (%d)",
                         f->frec->name, rc);

            ap_remove_input_filter(f);
            return rc;
        }
        else if (rc != APR_SUCCESS) {
            ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, c->base_server,
                         IB_PRODUCT_NAME ": %s returned %s (0x%08x) - %s",
                         f->frec->name, ib_status_to_string(rc), rc, strerror(apr_get_os_error()));

            return rc;
        }

        /* Process data. */
        for (b = APR_BRIGADE_FIRST(bb);
             b != APR_BRIGADE_SENTINEL(bb);
             b = APR_BUCKET_NEXT(b))
        {
            if (buffering) {
                /// @todo setaside into our own pool to destroy later???
                apr_bucket_setaside(b, c->pool);
                process_bucket(f, b);
                APR_BUCKET_REMOVE(b);
            }
            else {
                process_bucket(f, b);
            }
        }
    } while (buffering);

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
#if 0
    conn_rec *c = f->c;
    ironbee_conn_context *ctx = f->ctx;
    ib_conn_t *iconn = ctx->iconn;
    ib_core_cfg_t *corecfg;
    int buffering = 0;

    /* Configure. */
    ib_context_module_config(iconn->ctx, ib_core_module(), (void *)&corecfg);
    if (corecfg != NULL) {
        buffering = (int)corecfg->buffer_res;
    }
#endif


    for (b = APR_BRIGADE_FIRST(bb); b != APR_BRIGADE_SENTINEL(bb); b = APR_BUCKET_NEXT(b)) {
#if 0
        /// @todo Should this be done?  Maybe only for proxy?
        if (APR_BUCKET_IS_EOS(b)) {
            /// @todo Do we need to do this? Maybe only for proxy.
            apr_bucket *flush = apr_bucket_flush_create(f->c->bucket_alloc);
            APR_BUCKET_INSERT_BEFORE(b, flush);
        }

        if (buffering) {
            /// @todo setaside into our own pool to destroy later???
            apr_bucket_setaside(b, c->pool);
            process_bucket(f, b);
            APR_BUCKET_REMOVE(b);
        }
        else {
#endif
            process_bucket(f, b);
#if 0
        }
#endif
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
    ib_provider_t *lpr;
    void *init = NULL;
    ib_status_t rc;


    /* Init IB library. */
    rc = ib_initialize();
    if (rc != IB_OK) {
        ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s,
                     IB_PRODUCT_NAME ": Error initializing ib library");
        return OK;
    }

    ib_util_log_level(4);

    /* Detect first (validation) run vs real config run. */
    apr_pool_userdata_get(&init, "ironbee-init", s->process->pool);
    if (init == NULL) {
        ap_log_error(APLOG_MARK, APLOG_INFO, 0, s,
                     MODULE_NAME_FULL " loading.");

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
                     IB_PRODUCT_NAME ": Error creating engine: %s", ib_status_to_string(rc));
        return OK;
    }

    /* Register the logger. */
    rc = ib_provider_register(ironbee, IB_PROVIDER_TYPE_LOGGER,
                              MODULE_NAME_STR, &lpr,
                              &ironbee_logger_iface,
                              NULL);
    if (rc != IB_OK) {
        ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s,
                     IB_PRODUCT_NAME ": Error registering log provider: %s", ib_status_to_string(rc));
        return OK;
    }
    ib_provider_data_set(lpr, (void *)s);

    /* Default logger */
    /// @todo Need to add a post set hook in core for this to work correctly
    ib_context_set_string(ib_context_engine(ironbee),
                          IB_PROVIDER_TYPE_LOGGER,
                          MODULE_NAME_STR);
    ib_context_set_num(ib_context_engine(ironbee),
                       IB_PROVIDER_TYPE_LOGGER ".log_level",
                       4);


    rc = ib_engine_init(ironbee);
    if (rc != IB_OK) {
        ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s,
                     IB_PRODUCT_NAME ": Error initializing engine: %s", ib_status_to_string(rc));
        return OK;
    }

    /* Register module cleanup. */
    apr_pool_cleanup_register(p, (void *)s, ironbee_module_cleanup,
                              apr_pool_cleanup_null);

    /* Register conn/tx init hooks. */
    ib_hook_conn_register(ironbee, conn_opened_event,
                          ironbee_conn_init, s);

    /* Configure the engine. */
    if (modcfg->config != NULL) {
        ib_context_t *ctx;

        /* Notify the engine that the config process has started. This
         * will also create a main configuration context.
         */
        ib_state_notify_cfg_started(ironbee);

        /* Get the main configuration context. */
        ctx = ib_context_main(ironbee);

        /* Set some defaults */
        ib_context_set_string(ctx, IB_PROVIDER_TYPE_LOGGER, MODULE_NAME_STR);
        ib_context_set_num(ctx, "logger.log_level", 4);

        /* Parse the config file. */
        rc = ib_cfgparser_create(&cp, ironbee);
        if ((rc == IB_OK) && (cp != NULL)) {
            ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s,
                         IB_PRODUCT_NAME ": Parsing config: %s",
                         modcfg->config);
            ib_cfgparser_parse(cp, modcfg->config);
            ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s,
                         IB_PRODUCT_NAME ": Destroying config parser");
            ib_cfgparser_destroy(cp);
        }

        /* Notify the engine that the config process has finished. This
         * will also close out the main configuration context.
         */
        ib_state_notify_cfg_finished(ironbee);
    }
    else {
        ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s,
                     IB_PRODUCT_NAME ": No config specified with IronBeeConfig directive");
    }

    ap_log_error(APLOG_MARK, APLOG_NOTICE, 0, s,
                 MODULE_NAME_FULL " configured.");

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
    if (cmd->server->is_virtual) {
        return MODULE_NAME_STR ": IronBeeEnable not allowed in VirtualHost";
    }
    dummy = ap_get_module_config(cmd->server->module_config, &ironbee_module);
    return ap_set_flag_slot(cmd, dummy, flag);
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
    if (cmd->server->is_virtual) {
        return MODULE_NAME_STR ": IronBeeConfig not allowed in VirtualHost";
    }
    dummy = ap_get_module_config(cmd->server->module_config, &ironbee_module);
    return ap_set_file_slot(cmd, dummy, p1);
}

/**
 * @internal
 *
 * "IronBeeBufferSize" and "IronBeeBufferFlushSize" configuration directives.
 */
static const char *ironbee_cmd_sz(cmd_parms *cmd, void *dummy, const char *p1)
{
    if (cmd->server->is_virtual) {
        return MODULE_NAME_STR ": IronBee directive not allowed in VirtualHost";
    }
    dummy = ap_get_module_config(cmd->server->module_config, &ironbee_module);
    return ap_set_int_slot(cmd, dummy, p1);
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
      (void*)APR_OFFSETOF(ironbee_config_t, enabled),
      RSRC_CONF,
      "enable ironbee module"
    ),
    AP_INIT_TAKE1(
      "IronBeeConfig",
      ironbee_cmd_ibconfig,
      (void*)APR_OFFSETOF(ironbee_config_t, config),
      RSRC_CONF,
      "specify ironbee configuration file"
    ),
    AP_INIT_TAKE1(
      "IronBeeBufferSize",
      ironbee_cmd_sz,
      (void*)APR_OFFSETOF(ironbee_config_t, buf_size),
      RSRC_CONF,
      "specify buffer size (bytes)"
    ),
    AP_INIT_TAKE1(
      "IronBeeBufferFlushSize",
      ironbee_cmd_sz,
      (void*)APR_OFFSETOF(ironbee_config_t, flush_size),
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
        AP_FTYPE_CONNECTION + 1
    );

#ifdef IB_DEBUG
    ap_register_input_filter(
        "IRONBEE_DBG_IN",
        ironbee_dbg_input_filter,
        NULL,
        AP_FTYPE_CONNECTION
    );
#endif

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
