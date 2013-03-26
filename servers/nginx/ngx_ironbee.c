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
#include <ironbee/state_notify.h>
#include <ironbee/util.h>

#include "ngx_ironbee.h"

static ngx_http_output_header_filter_pt  ngx_http_next_header_filter;
static ngx_http_output_body_filter_pt  ngx_http_next_body_filter;

static ib_engine_t *ironbee;
ib_engine_t *ngxib_engine(void)
{
    return ironbee;
}

#define STATUS_IS_ERROR(code) ( ((code) >= 200) && ((code) <  600) )
#define IB2NG(x) x
#define LOGGER_NAME "ironbee-nginx"

typedef struct ironbee_proc_t {
    ngx_str_t config_file;
    ngx_uint_t loglevel;
    ngx_flag_t use_ngxib_logger;
} ironbee_proc_t;

static ngx_command_t  ngx_ironbee_commands[] = {
    { ngx_string("ironbee_config_file"),
      NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_str_slot,
      NGX_HTTP_MAIN_CONF_OFFSET,
      offsetof(ironbee_proc_t, config_file),
      NULL },
    { ngx_string("ironbee_logger"),
      NGX_HTTP_MAIN_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_MAIN_CONF_OFFSET,
      offsetof(ironbee_proc_t, use_ngxib_logger),
      NULL },
    { ngx_string("ironbee_loglevel"),
      NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_num_slot,
      NGX_HTTP_MAIN_CONF_OFFSET,
      offsetof(ironbee_proc_t, loglevel),
      NULL },

      ngx_null_command
};

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
 * A body filter to intercept response body and feed it to Ironbee,
 * and to buffer the data if required by Ironbee configuration.
 *
 * @param[in]  r     The nginx request object.
 * @param[in]  in    The data to filter.
 * @return     status propagated from next filter, or OK/Error
 */
static ngx_int_t ironbee_body_out(ngx_http_request_t *r, ngx_chain_t *in)
{
    ngx_log_t *prev_log;
    ngxib_req_ctx *ctx;
    ngx_chain_t *link;
    ib_status_t rc;
    ib_num_t num;
    ngx_int_t rv = NGX_OK;
    ib_txdata_t itxdata;

    if (r->internal)
        return ngx_http_next_body_filter(r, in);

    prev_log = ngxib_log(r->connection->log);

    ctx = ngx_http_get_module_ctx(r, ngx_ironbee_module);
    assert((ctx != NULL) && (ctx->tx != NULL));
    ib_log_debug_tx(ctx->tx, "ironbee_body_out");
    if (in == NULL) {
        /* FIXME: could this happen in circumstances when we should
         * notify Ironbee of end-of-response ?
         */
        ib_log_debug_tx(ctx->tx, "ironbee_body_out: input was null");
        cleanup_return(prev_log) ngx_http_next_body_filter(r, in);
    }
    ctx = ngx_http_get_module_ctx(r, ngx_ironbee_module);
    if (ctx->output_filter_done) {
        ib_log_debug_tx(ctx->tx, "ironbee_body_out: already done");
        cleanup_return(prev_log) ngx_http_next_body_filter(r, in);
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
                /* If we're buffering, initialise the buffer */
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
            ib_log_debug_tx(ctx->tx, "ironbee_body_out: %d bytes", (int)itxdata.dlen);
        if (itxdata.dlen > 0) {
            ib_state_notify_response_body_data(ironbee, ctx->tx, &itxdata);
        }

        /* If Ironbee just signalled an error, switch to discard data mode,
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
        rc = ib_state_notify_postprocess(ironbee, ctx->tx);
        if ((rv == NGX_OK) && (rc != IB_OK)) {
            rv = NGX_HTTP_INTERNAL_SERVER_ERROR;
        }
        rc = ib_state_notify_logging(ironbee, ctx->tx);
        if ((rv == NGX_OK) && (rc != IB_OK)) {
            rv = NGX_HTTP_INTERNAL_SERVER_ERROR;
        }
    }
    cleanup_return(prev_log) rv;
}

/**
 * A header filter to intercept response line and headers and feed to Ironbee.
 *
 * @param[in]  r     The nginx request object.
 * @return     status propagated from next filter, or Error
 */
static ngx_int_t ironbee_headers_out(ngx_http_request_t *r)
{
    ngx_log_t *prev_log;
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

    prev_log = ngxib_log(r->connection->log);
    ngx_regex_malloc_init(r->pool);

    /* Notify Ironbee of request line and headers */
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
        ib_log_error_tx(ctx->tx, "Ironbee: bogus response status %d",
                        (int)r->headers_out.status);
        cleanup_return(prev_log) NGX_ERROR;
    }
    rc = ib_parsed_resp_line_create(ctx->tx, &rline, NULL, 0,
                                    proto, strlen(proto),
                                    status, status_len,
                                    reason, reason_len);
    if (rc != IB_OK)
        cleanup_return(prev_log) NGX_ERROR;

    ib_state_notify_response_started(ironbee, ctx->tx, rline);

    rc = ib_parsed_name_value_pair_list_wrapper_create(&ibhdrs, ctx->tx);
    if (rc != IB_OK)
        cleanup_return(prev_log) NGX_ERROR;

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

    /* Ironbee currently crashes if called here with no headers,
     * even perfectly correctly on a 204/304 response.
     */
    if (ibhdrs->size > 0) {
        rc = ib_state_notify_response_header_data(ironbee, ctx->tx, ibhdrs);
        if (rc != IB_OK)
            cleanup_return(prev_log) NGX_ERROR;
    }

    rc = ib_state_notify_response_header_finished(ironbee, ctx->tx);
    if (rc != IB_OK)
        cleanup_return(prev_log) NGX_ERROR;

    ctx->hdrs_out = 1;

    cleanup_return(prev_log) ngx_http_next_header_filter(r);
}

/**
 * nginx post-read-request handler to feed request line and headers to Ironbee.
 *
 * @param[in]  r     The nginx request object.
 * @return     Declined (ignoreme) or error status.
 */
static ngx_int_t ironbee_post_read_request(ngx_http_request_t *r)
{
    ngx_log_t *prev_log;
    ngxib_req_ctx *ctx;
    ironbee_proc_t *pconf;
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

    prev_log = ngxib_log(r->connection->log);
    ngx_regex_malloc_init(r->pool);

    ctx = ngx_pcalloc(r->pool, sizeof(ngxib_req_ctx));
    ctx->r = r;
    ngx_http_set_ctx(r, ctx, ngx_ironbee_module);
    pconf = ngx_http_get_module_main_conf(r, ngx_ironbee_module);

    iconn = ngxib_conn_get(ctx, ironbee);

    ib_tx_create(&ctx->tx, iconn, ctx);

    /* Notify Ironbee of request line and headers */
    rc = ib_parsed_req_line_create(ctx->tx, &rline,
                                   (const char*)r->request_line.data,
                                   r->request_line.len,
                                   (const char*)r->method_name.data,
                                   r->method_name.len,
                                   (const char*)r->unparsed_uri.data,
                                   r->unparsed_uri.len,
                                   (const char*)r->http_protocol.data,
                                   r->http_protocol.len);
    if (rc != IB_OK)
        cleanup_return(prev_log) NGX_ERROR;

    ib_state_notify_request_started(ironbee, ctx->tx, rline);

    rc = ib_parsed_name_value_pair_list_wrapper_create(&ibhdrs, ctx->tx);
    if (rc != IB_OK)
        cleanup_return(prev_log) NGX_ERROR;

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

    rc = ib_state_notify_request_header_data(ironbee, ctx->tx, ibhdrs);
    if (rc != IB_OK)
        cleanup_return(prev_log) NGX_ERROR;

    rc = ib_state_notify_request_header_finished(ironbee, ctx->tx);
    if (rc != IB_OK)
        cleanup_return(prev_log) NGX_ERROR;

    if (!ngxib_has_request_body(r, ctx)) {
        rc = ib_state_notify_request_finished(ironbee, ctx->tx);
        if (rc != IB_OK)
            cleanup_return(prev_log) NGX_ERROR;
    }
    ctx->hdrs_in = 1;
    if (STATUS_IS_ERROR(ctx->status)) {
        ctx->internal_errordoc = 1;
        cleanup_return(prev_log) ctx->status;
    }

    cleanup_return(prev_log) NGX_DECLINED;
}

/**
 * Ironbee initialisation function.  Sets up engine and logging,
 * and reads Ironbee config.
 *
 * @param[in]  cf     Configuration rec
 * @return     NGX_OK or error
 */
static ngx_int_t ironbee_init(ngx_conf_t *cf)
{
    ngx_log_t *prev_log;
    ib_context_t *ctx;
    ib_cfgparser_t *cp;
    ironbee_proc_t *proc;
    ib_status_t rc, rc1;

    prev_log = ngxib_log(cf->log);
    ngx_regex_malloc_init(cf->pool);

    ngx_log_error(NGX_LOG_NOTICE, cf->log, 0, "ironbee_init %d", getpid());

    proc = ngx_http_conf_get_module_main_conf(cf, ngx_ironbee_module);
    if (proc->loglevel == NGX_CONF_UNSET_UINT)
        proc->loglevel = 4; /* default */

    rc = ib_initialize();
    if (rc != IB_OK)
        cleanup_return(prev_log) IB2NG(rc);

    ib_util_log_level(proc->loglevel);

    rc = ib_engine_create(&ironbee, ngxib_server());
    if (rc != IB_OK)
        cleanup_return(prev_log) IB2NG(rc);

    if (proc->use_ngxib_logger)
        ib_log_set_logger_fn(ironbee, ngxib_logger, NULL);
    /* Using default log level function. */

    rc = ib_engine_init(ironbee);
    if (rc != IB_OK)
        cleanup_return(prev_log) IB2NG(rc);

    /* TODO: TS creates logfile at this point */

    ib_hook_conn_register(ironbee, conn_opened_event, ngxib_conn_init, NULL);

    rc = ib_cfgparser_create(&cp, ironbee);
    assert((cp != NULL) || (rc != IB_OK));
    if (rc != IB_OK)
        cleanup_return(prev_log) IB2NG(rc);

    rc = ib_engine_config_started(ironbee, cp);
    if (rc != IB_OK)
        cleanup_return(prev_log) IB2NG(rc);

    /* Get the main context, set some defaults */
    ctx = ib_context_main(ironbee);
    ib_context_set_num(ctx, "logger.log_level", proc->loglevel);

    /* FIXME - use the temp pool operation for this */
    char *buf = strndup((char*)proc->config_file.data, proc->config_file.len);
    rc = ib_cfgparser_parse(cp, buf);
    free(buf);
    rc1 = ib_engine_config_finished(ironbee);
    ib_cfgparser_destroy(cp);

    cleanup_return(prev_log) rc == IB_OK ? rc1 == IB_OK ? NGX_OK : IB2NG(rc1) : IB2NG(rc);
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

    /* Give ourself the chance to attach gdb */
    do {
        const char *csleeptime = getenv("sleeptime");
        if (csleeptime) {
            int sleeptime = atoi(csleeptime);
            sleep(sleeptime);
        }
    } while (0);

    main_cf = ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);
    if (main_cf == NULL)
        return NGX_ERROR;

    /* Register a handler to deal with request line and headers */
    req_handler = ngx_array_push(&main_cf->phases[NGX_HTTP_POST_READ_PHASE].handlers);
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
    conf->loglevel = NGX_CONF_UNSET_UINT;
    conf->use_ngxib_logger = NGX_CONF_UNSET;
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
    ngxib_log(cycle->log);
    ib_engine_destroy(ironbee);
    ngxib_log(NULL);
}


ngx_module_t  ngx_ironbee_module = {
    NGX_MODULE_V1,
    &ngx_ironbee_module_ctx,       /* module context */
    ngx_ironbee_commands,          /* module directives */
    NGX_HTTP_MODULE,               /* module type */
    NULL,                          /* init master */
    NULL,                          /* init module */
    NULL, //ironbee_init,                  /* init process */
    NULL,                          /* init thread */
    NULL,                          /* exit thread */
    NULL, //ironbee_exit,                  /* exit process */
    ironbee_exit,                          /* exit master */
    NGX_MODULE_V1_PADDING
};
