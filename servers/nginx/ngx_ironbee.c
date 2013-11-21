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
 * @brief IronBee --- nginx 1.3 module
 *
 * @author Nick Kew <nkew@qualys.com>
 */

#include <assert.h>
#include <ctype.h>

#include <ironbee/config.h>
#include <ironbee/context.h>
#include <ironbee/engine_manager.h>
#include <ironbee/state_notify.h>
#include <ironbee/util.h>

#include "ngx_ironbee.h"

static ngx_http_output_header_filter_pt  ngx_http_next_header_filter;
static ngx_http_output_body_filter_pt  ngx_http_next_body_filter;

#define STATUS_IS_ERROR(code) ( ((code) >= 200) && ((code) <  600) )
#define IB2NG(x) x
#define LOGGER_NAME "ironbee-nginx"

typedef struct ironbee_proc_t {
    ngx_str_t config_file;
    ngx_uint_t log_level;
    ngx_flag_t use_ngxib_logger;
    ngx_uint_t max_engines;
} ironbee_proc_t;

static ngx_command_t  ngx_ironbee_commands[] =
{
    {
        ngx_string("ironbee_config_file"),
        NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
        ngx_conf_set_str_slot,
        NGX_HTTP_MAIN_CONF_OFFSET,
        offsetof(ironbee_proc_t, config_file),
        NULL
    },

    {
        ngx_string("ironbee_logger"),
        NGX_HTTP_MAIN_CONF|NGX_CONF_FLAG,
        ngx_conf_set_flag_slot,
        NGX_HTTP_MAIN_CONF_OFFSET,
        offsetof(ironbee_proc_t, use_ngxib_logger),
        NULL
    },

    {
        ngx_string("ironbee_log_level"),
        NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
        ngx_conf_set_num_slot,
        NGX_HTTP_MAIN_CONF_OFFSET,
        offsetof(ironbee_proc_t, log_level),
        NULL
    },

    {
        ngx_string("ironbee_max_engines"),
        NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
        ngx_conf_set_num_slot,
        NGX_HTTP_MAIN_CONF_OFFSET,
        offsetof(ironbee_proc_t, max_engines),
        NULL
    },

      ngx_null_command
};

/**
 * Static module data
 */
static module_data_t module_data =
{
    NULL,          /* .manager */
    0,             /* .active */
    NULL,          /* .log */
    NGX_LOG_INFO,  /* .log_level */
};

ib_status_t ngxib_acquire_engine(
    ib_engine_t  **pengine,
    ngx_log_t     *log
)
{
    module_data_t *mod_data = &module_data;
    ib_status_t    rc;

    /* No manager? Decline the request */
    if (mod_data->manager == NULL) {
        ngx_log_error(NGX_LOG_ERR, log, 0, "acquire_engine: No manager!");
        return IB_DECLINED;
    }

    rc = ib_manager_engine_acquire(mod_data->manager, pengine);
    if (rc != IB_OK) {
        ngx_log_error(NGX_LOG_ERR, log, 0,
                      "Failed to acquire engine from manager: %s!",
                      ib_status_to_string(rc));
    }
    return rc;
}

ib_status_t ngxib_release_engine(
    ib_engine_t  *engine,
    ngx_log_t    *log
)
{
    module_data_t *mod_data = &module_data;
    ib_status_t    rc;
    assert(mod_data->manager != NULL);

    rc = ib_manager_engine_release(mod_data->manager, engine);
    if (rc != IB_OK) {
        ngx_log_error(NGX_LOG_ERR, log, 0,
                      "Failed to release engine to manager: %s!",
                      ib_status_to_string(rc));
    }
    return rc;
}


/**
 * Function to free a chain buffer.
 * This specifically releases a buffer created and populated by ironbee_body_out.
 * It is not for general-purpose use with an arbitrary chain, where it would
 * likely crash and burn.
 *
 * @param[in]  pool  The pool used to allocate buffers being released.
 * @param[in]  chain The chain buffer to free.
 */
static void free_chain(ngx_pool_t *pool, ngx_chain_t *chain)
{
    if (chain) {
        free_chain(pool, chain->next);
        if (chain->buf->last != chain->buf->pos) {
            ngx_pfree(pool, chain->buf->pos);
        }
        ngx_pfree(pool, chain->buf);
        ngx_pfree(pool, chain);
    }
}

/**
 * A body filter to intercept response body and feed it to IronBee,
 * and to buffer the data if required by IronBee configuration.
 *
 * @param[in]  r     The nginx request object.
 * @param[in]  in    The data to filter.
 * @return     status propagated from next filter, or OK/Error
 */
static ngx_int_t ironbee_body_out(ngx_http_request_t *r, ngx_chain_t *in)
{
    ngxib_req_ctx *ctx;
    ngx_chain_t *link;
    ib_status_t rc;
    ib_num_t num;
    ngx_int_t rv = NGX_OK;
    ib_txdata_t itxdata;

    if (r->internal)
        return ngx_http_next_body_filter(r, in);

    ctx = ngx_http_get_module_ctx(r, ngx_ironbee_module);
    assert((ctx != NULL) && (ctx->tx != NULL));
    ib_log_debug_tx(ctx->tx, "ironbee_body_out");
    if (in == NULL) {
        /* FIXME: could this happen in circumstances when we should
         * notify IronBee of end-of-response ?
         */
        ib_log_debug_tx(ctx->tx, "ironbee_body_out: input was null");
        cleanup_return ngx_http_next_body_filter(r, in);
    }
    ctx = ngx_http_get_module_ctx(r, ngx_ironbee_module);
    if (ctx->output_filter_done) {
        ib_log_debug_tx(ctx->tx, "ironbee_body_out: already done");
        cleanup_return ngx_http_next_body_filter(r, in);
    }
    if (!ctx->output_filter_init) {
        ctx->output_filter_init = 1;

        if (ctx->internal_errordoc) {
            /* If it's our own errordoc, pass it straight through */
            /* Should we log anything here?  The error will already
             * have been logged.
             */
            ctx->output_buffering = IOBUF_NOBUF;
            ctx->response_buf = NULL;
            ib_log_debug_tx(ctx->tx, "ironbee_body_out: in internal errordoc");
        }
        else {
            /* Determine whether we're configured to buffer */
            rc = ib_context_get(ctx->tx->ctx, "buffer_res",
                                ib_ftype_num_out(&num), NULL);
            ib_log_debug_tx(ctx->tx, "ironbee_body_out: buffer_res is %d", (int)num);
            if (rc != IB_OK)
                ib_log_error_tx(ctx->tx,
                                "Can't determine output buffer configuration!");
            if (num == 0) {
                ib_log_debug_tx(ctx->tx, "ironbee_body_out: NOBUF");
                ctx->output_buffering = IOBUF_NOBUF;
                ctx->response_buf = NULL;
            }
            else {
                /* If we're buffering, initialize the buffer */
                ib_log_debug_tx(ctx->tx, "ironbee_body_out: BUFFER");
                ctx->output_buffering = IOBUF_BUFFER;
            }
        }
    }

    ngx_regex_malloc_init(r->pool);

    for (link = in; link != NULL; link = link->next) {
        /* Feed the data to ironbee */
        itxdata.data = link->buf->pos;
        itxdata.dlen = link->buf->last - link->buf->pos;
        ib_log_debug_tx(ctx->tx, "ironbee_body_out: %d bytes",
                        (int)itxdata.dlen);
        if (itxdata.dlen > 0) {
            ib_state_notify_response_body_data(ctx->tx->ib, ctx->tx, &itxdata);
        }

        /* If IronBee just signaled an error, switch to discard data mode,
         * and dump anything we already have buffered,
         */
        if ( (STATUS_IS_ERROR(ctx->status)) &&
             !ctx->internal_errordoc &&
             (ctx->output_buffering != IOBUF_DISCARD) ) {
            ib_log_debug_tx(ctx->tx, "ironbee_body_out: error %d", ctx->status);
            free_chain(r->pool, ctx->response_buf);
            ctx->response_buf = NULL;
            ctx->output_buffering = IOBUF_DISCARD;
        }
        else if (ctx->output_buffering == IOBUF_BUFFER) {
            /* Copy any data to our buffer */
            if (ctx->response_buf == NULL) {
                ctx->response_buf = ngx_pcalloc(r->pool, sizeof(ngx_chain_t));
                ctx->response_ptr = ctx->response_buf;
            }
            else {
                ctx->response_ptr->next = ngx_pcalloc(r->pool, sizeof(ngx_chain_t));
                ctx->response_ptr = ctx->response_ptr->next;
            }
            /* Not sure if any data types need setaside, but let's be safe */
#if NO_COPY_REQUIRED
            ctx->response_ptr->buf = link->buf;
#else
            if (itxdata.dlen > 0) {
                ctx->response_ptr->buf = ngx_create_temp_buf(r->pool, itxdata.dlen);
                memcpy(ctx->response_ptr->buf->pos, link->buf->pos, itxdata.dlen);
                ctx->response_ptr->buf->last += itxdata.dlen;
            }
            else {
                ctx->response_ptr->buf = ngx_palloc(r->pool, sizeof(ngx_buf_t));
                memcpy(ctx->response_ptr->buf, link->buf, sizeof(ngx_buf_t));
            }
#endif
        }

        if (link->buf->last_buf) {
            ib_log_debug_tx(ctx->tx, "ironbee_body_out: last_buf");
            ctx->output_filter_done = 1;
        }
    }

    if (ctx->output_buffering == IOBUF_NOBUF) {
        /* Normal operation - pass it down the chain */
        ib_log_debug_tx(ctx->tx, "ironbee_body_out: passing on");
        ctx->start_response = 1;
        rv = ngx_http_next_body_filter(r, in);
    }
    else if (ctx->output_buffering == IOBUF_BUFFER) {
        ib_log_debug_tx(ctx->tx, "ironbee_body_out: buffering");
        if (ctx->output_filter_done) {
            /* We can pass on the buffered data all at once */
            ib_log_debug_tx(ctx->tx, "ironbee_body_out: passing buffer");
            ctx->start_response = 1;
            rv = ngx_http_next_body_filter(r, ctx->response_buf);
        }
    }
    else if (ctx->output_buffering == IOBUF_DISCARD) {
        ib_log_debug_tx(ctx->tx, "ironbee_body_out: discarding");
        if (ctx->output_filter_done) {
            /* Pass a last bucket with no data */
            //ngx_log_error(NGX_LOG_DEBUG, r->connection->log, 0,
            //              "ironbee_body_out: passing NULL last-buffer");
            //ctx->response_buf = ngx_pcalloc(r->pool, sizeof(ngx_chain_t));
            //ctx->response_buf->buf = ngx_calloc_buf(r->pool);
            //ctx->response_buf->buf->last_buf = ctx->response_buf->buf->last_in_chain = 1;
            //rv = ngx_http_next_body_filter(r, ctx->response_buf);
            /* FIXME: Is setting rv enough to serve error page */
            rv = ctx->status;
        }
    }
    if (ctx->output_filter_done) {
        ib_log_debug_tx(ctx->tx, "ironbee_body_out: notify_postprocess");
        rc = ib_state_notify_postprocess(ctx->tx->ib, ctx->tx);
        if ((rv == NGX_OK) && (rc != IB_OK)) {
            rv = NGX_HTTP_INTERNAL_SERVER_ERROR;
        }
        rc = ib_state_notify_logging(ctx->tx->ib, ctx->tx);
        if ((rv == NGX_OK) && (rc != IB_OK)) {
            rv = NGX_HTTP_INTERNAL_SERVER_ERROR;
        }
    }
    cleanup_return rv;
}

/**
 * A header filter to intercept response line and headers and feed to IronBee.
 *
 * @param[in]  r     The nginx request object.
 * @return     status propagated from next filter, or Error
 */
static ngx_int_t ironbee_headers_out(ngx_http_request_t *r)
{
    ngxib_req_ctx *ctx;
    ib_parsed_resp_line_t *rline;
    ib_parsed_header_wrapper_t *ibhdrs;
    ib_status_t rc;

    ngx_list_part_t *part;
    ngx_table_elt_t *hdr;
    unsigned int i;
    char proto[12];
    char *status;
    const char *reason;
    int status_len, reason_len;

    /* FIXME: needs more logic here to catch error pages */
    if (r->internal)
        return ngx_http_next_header_filter(r);

    ctx = ngx_http_get_module_ctx(r, ngx_ironbee_module);
    assert((ctx != NULL) && (ctx->tx != NULL));

    ngx_regex_malloc_init(r->pool);

    /* Notify IronBee of request line and headers */
    sprintf(proto, "HTTP/%d.%d", r->http_major, r->http_minor);
    if (r->headers_out.status_line.len) {
        status = (char*)r->headers_out.status_line.data;
        status_len = strcspn(status, " \t");
        for (reason = status+status_len; isspace(*reason); ++reason);
        reason_len = r->headers_out.status_line.len - (reason-status);
    }
    else if (r->headers_out.status >= 100 && r->headers_out.status < 600) {
        status = ngx_palloc(r->pool, 4);
        /* cast to int, because ngx_int_t requires different format args
         * on different platforms.  We're already limited to 3-digit numbers.
         */
        sprintf(status, "%d", (int)r->headers_out.status);
        status_len = 3;
        reason = "";
        reason_len = 0;
    }
    else {
        ib_log_error_tx(ctx->tx, "IronBee: bogus response status %d",
                        (int)r->headers_out.status);
        cleanup_return NGX_ERROR;
    }
    rc = ib_parsed_resp_line_create(&rline, ctx->tx, NULL, 0,
                                    proto, strlen(proto),
                                    status, status_len,
                                    reason, reason_len);
    if (rc != IB_OK)
        cleanup_return NGX_ERROR;

    ib_state_notify_response_started(ctx->tx->ib, ctx->tx, rline);

    rc = ib_parsed_name_value_pair_list_wrapper_create(&ibhdrs, ctx->tx);
    if (rc != IB_OK)
        cleanup_return NGX_ERROR;

    for (part = &r->headers_out.headers.part; part != NULL; part = part->next) {
        hdr = part->elts;
        for (i = 0; i < part->nelts; ++i) {
            ib_parsed_name_value_pair_list_add(ibhdrs,
                                               (const char*)hdr->key.data,
                                               hdr->key.len,
                                               (const char*)hdr->value.data,
                                               hdr->value.len);
            ++hdr;
        }
    }

    /* IronBee currently crashes if called here with no headers,
     * even perfectly correctly on a 204/304 response.
     */
    if (ibhdrs->size > 0) {
        rc = ib_state_notify_response_header_data(ctx->tx->ib, ctx->tx, ibhdrs);
        if (rc != IB_OK)
            cleanup_return NGX_ERROR;
    }

    rc = ib_state_notify_response_header_finished(ctx->tx->ib, ctx->tx);
    if (rc != IB_OK)
        cleanup_return NGX_ERROR;

    ctx->hdrs_out = 1;

    cleanup_return ngx_http_next_header_filter(r);
}

/**
 * nginx post-read-request handler to feed request line and headers to IronBee.
 *
 * @param[in]  r     The nginx request object.
 * @return     Declined (ignored) or error status.
 */
static ngx_int_t ironbee_post_read_request(ngx_http_request_t *r)
{
    ngxib_req_ctx *ctx;
    ib_conn_t *iconn;
    ib_parsed_req_line_t *rline;
    ib_parsed_header_wrapper_t *ibhdrs;
    ib_status_t rc;

    ngx_list_part_t *part;
    ngx_table_elt_t *hdr;
    unsigned int i;

    /* Don't process internal requests */
    if (r->internal)
        return NGX_DECLINED;

    ngx_regex_malloc_init(r->pool);

    ctx = ngx_pcalloc(r->pool, sizeof(ngxib_req_ctx));
    ctx->r = r;
    ngx_http_set_ctx(r, ctx, ngx_ironbee_module);

    iconn = ngxib_conn_get(ctx);

    ib_tx_create(&ctx->tx, iconn, ctx);

    /* Notify IronBee of request line and headers */
    rc = ib_parsed_req_line_create(&rline, ctx->tx,
                                   (const char*)r->request_line.data,
                                   r->request_line.len,
                                   (const char*)r->method_name.data,
                                   r->method_name.len,
                                   (const char*)r->unparsed_uri.data,
                                   r->unparsed_uri.len,
                                   (const char*)r->http_protocol.data,
                                   r->http_protocol.len);
    if (rc != IB_OK)
        cleanup_return NGX_ERROR;

    ib_state_notify_request_started(ctx->tx->ib, ctx->tx, rline);

    rc = ib_parsed_name_value_pair_list_wrapper_create(&ibhdrs, ctx->tx);
    if (rc != IB_OK)
        cleanup_return NGX_ERROR;

    for (part = &r->headers_in.headers.part; part != NULL; part = part->next) {
        hdr = part->elts;
        for (i = 0; i < part->nelts; ++i) {
            ib_parsed_name_value_pair_list_add(ibhdrs,
                                               (const char*)hdr->key.data,
                                               hdr->key.len,
                                               (const char*)hdr->value.data,
                                               hdr->value.len);
            ++hdr;
        }
    }

    rc = ib_state_notify_request_header_data(ctx->tx->ib, ctx->tx, ibhdrs);
    if (rc != IB_OK)
        cleanup_return NGX_ERROR;

    rc = ib_state_notify_request_header_finished(ctx->tx->ib, ctx->tx);
    if (rc != IB_OK)
        cleanup_return NGX_ERROR;

    if (!ngxib_has_request_body(r, ctx)) {
        rc = ib_state_notify_request_finished(ctx->tx->ib, ctx->tx);
        if (rc != IB_OK)
            cleanup_return NGX_ERROR;
    }
    ctx->hdrs_in = 1;
    if (STATUS_IS_ERROR(ctx->status)) {
        ctx->internal_errordoc = 1;
        cleanup_return ctx->status;
    }

    cleanup_return NGX_DECLINED;
}

/**
 * IronBee initialization function.  Sets up engine and logging,
 * and reads IronBee config.
 *
 * @param[in]  cf     Configuration rec
 * @return     NGX_OK or error
 */
static ngx_int_t ironbee_init(ngx_conf_t *cf)
{
    ironbee_proc_t *proc;
    ib_status_t rc;
    module_data_t *mod_data = &module_data;
    char *buf;

    /* We still use the global-log hack to initialise */
    ngx_regex_malloc_init(cf->pool);

    ngx_log_error(NGX_LOG_NOTICE, cf->log, 0, "ironbee_init %d", getpid());

    proc = ngx_http_conf_get_module_main_conf(cf, ngx_ironbee_module);
    if (proc->log_level == NGX_CONF_UNSET_UINT) {
        proc->log_level = IB_LOG_NOTICE;
    }
    if (proc->max_engines == NGX_CONF_UNSET_UINT) {
        proc->max_engines = IB_MANAGER_DEFAULT_MAX_ENGINES;
    }
    if (proc->use_ngxib_logger == NGX_CONF_UNSET) {
        proc->use_ngxib_logger =1;
    }

    /* initialise fields in mod_data */
    mod_data->ib_log_active = proc->use_ngxib_logger;
    mod_data->log = cf->log;
    mod_data->log_level = proc->log_level;

    rc = ib_initialize();
    if (rc != IB_OK) {
        cleanup_return IB2NG(rc);
    }

    /* Create the IronBee engine manager */
    rc = ib_manager_create(&(mod_data->manager),  /* Engine Manager */
                           ngxib_server(),        /* Server object */
                           proc->max_engines);    /* Max engines */
    if (rc != IB_OK) {
        cleanup_return IB2NG(rc);
    }

    rc = ib_manager_register_module_fn(
        mod_data->manager,
        ngxib_module,
        mod_data);
    if (rc != IB_OK) {
        cleanup_return IB2NG(rc);
    }

    /* Null manager here would be a bug, as per RNS-CR-143 comments */
    assert(mod_data->manager != NULL);

    /* FIXME - use the temp pool operation for this */
    buf = strndup((char*)proc->config_file.data, proc->config_file.len);

    /* Create the initial engine */
    rc = ib_manager_engine_create(mod_data->manager, buf);
    if (rc != IB_OK) {
        free(buf);
        cleanup_return IB2NG(rc);
    }
    free(buf);

    cleanup_return rc == IB_OK ? NGX_OK : IB2NG(rc);
}

/**
 * nginx post-config handler to insert our handlers.
 *
 * @param[in]  cf     Configuration rec
 * @return     Return value from ironbee_init, or error
 */
static ngx_int_t ngxib_post_conf(ngx_conf_t *cf)
{
    ngx_http_core_main_conf_t *main_cf;
    ngx_http_handler_pt *req_handler;
    ironbee_proc_t *ipcf;

    /* Step aside if not configured in nginx */
    ipcf = ngx_http_conf_get_module_main_conf(cf, ngx_ironbee_module);
    if (ipcf->config_file.len == 0) {
        return NGX_OK;
    }

    /* Give ourself the chance to attach gdb */
    do {
        const char *csleeptime = getenv("sleeptime");
        if (csleeptime) {
            int sleeptime = atoi(csleeptime);
            sleep(sleeptime);
        }
    } while (0);

    main_cf = ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);
    assert (main_cf != NULL);

    /* Register a handler to deal with request line and headers */
    req_handler = ngx_array_push(&main_cf->phases[NGX_HTTP_POST_READ_PHASE].handlers);
    if (req_handler == NULL) {
        return NGX_ERROR;
    }
    *req_handler = ironbee_post_read_request;

    /* Register dummy handler to pull input */
    /* Don't use content phase.  That's "special", and often gets overridden
     * (it's always overridden when proxying).  The last phase we can insert
     * a handler into is ACCESS, but that leaves us with a return value that
     * has a special meaning, so we can't use it without side-effect.
     * Try preaccess, and if that fails try rewrite.
     * (ref: http://www.nginxguts.com/2011/01/phases/).
     */
    //req_handler = ngx_array_push(&main_cf->phases[NGX_HTTP_CONTENT_PHASE].handlers);
    req_handler = ngx_array_push(&main_cf->phases[NGX_HTTP_PREACCESS_PHASE].handlers);
    if (req_handler == NULL) {
        return NGX_ERROR;
    }
    *req_handler = ngxib_handler;

    /* Insert headers_out filter */
    ngx_http_next_header_filter = ngx_http_top_header_filter;
    ngx_http_top_header_filter = ironbee_headers_out;

    /* Insert body_out filter */
    ngx_http_next_body_filter = ngx_http_top_body_filter;
    ngx_http_top_body_filter = ironbee_body_out;

    return ironbee_init(cf);
}

/**
 * function to create module configuration rec
 *
 * @param[in]  cf     Configuration rec
 * @return     module configuration rec
 */
static void *create_main_conf(ngx_conf_t *cf)
{
    ironbee_proc_t *conf = ngx_pcalloc(cf->pool, sizeof(ironbee_proc_t));
    if (conf != NULL) {
        conf->log_level = NGX_CONF_UNSET_UINT;
        conf->use_ngxib_logger = NGX_CONF_UNSET;
        conf->max_engines = NGX_CONF_UNSET_UINT;
    }
    return conf;
}
#define init_main_conf NULL
#define create_svr_conf NULL
#define merge_svr_conf NULL
#define create_loc_conf NULL
#define merge_loc_conf NULL
static ngx_http_module_t  ngx_ironbee_module_ctx = {
    NULL,                          /* preconfiguration */
    ngxib_post_conf,               /* postconfiguration */

    create_main_conf,              /* create main configuration */
    init_main_conf,                /* init main configuration */

    create_svr_conf,               /* create server configuration */
    merge_svr_conf,                /* merge server configuration */

    create_loc_conf,               /* create location configuration */
    merge_loc_conf                 /* merge location configuration */
};


/**
 * Cleanup function to log nginx process exit and destroy ironbee engine
 *
 * @param[in]  cycle     nginx cycle rec
 */
static void ironbee_exit(ngx_cycle_t *cycle)
{
    ngx_log_error(NGX_LOG_NOTICE, cycle->log, 0, "ironbee_exit %d", getpid());
    /* FIXME: this fails under gdb */
    if (module_data.manager != NULL) {
        ib_manager_destroy(module_data.manager);
        module_data.manager = NULL;
    }
}


ngx_module_t  ngx_ironbee_module = {
    NGX_MODULE_V1,
    &ngx_ironbee_module_ctx,       /* module context */
    ngx_ironbee_commands,          /* module directives */
    NGX_HTTP_MODULE,               /* module type */
    NULL,                          /* init master */
    NULL,                          /* init module */
    NULL,                          /* init process */
    NULL,                          /* init thread */
    NULL,                          /* exit thread */
    NULL,                          /* exit process */
    ironbee_exit,                  /* exit master */
    NGX_MODULE_V1_PADDING
};
