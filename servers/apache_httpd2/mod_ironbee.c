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
 * @brief IronBee --- Apache 2.4 Module
 *
 * @author Nick Kew <nkew@qualys.com>
 */

#include <httpd.h>
#include <http_protocol.h>
#include <http_request.h>
#include <http_connection.h>
#include <http_config.h>
#include <http_log.h>
#include <util_filter.h>
#include <apr_strings.h>


#include <ironbee/engine.h>
#include <ironbee/config.h>
#include <ironbee/module.h> /* Only needed while config is in here. */
#include <ironbee/provider.h>
#include <ironbee/server.h>
#include <ironbee/core.h>
#include <ironbee/state_notify.h>
#include <ironbee/regex.h>
#include <ironbee/util.h>

/* Hack to detect 2.2 vs 2.4 server versions.
 * This is a 2.3.x version shortly after the module declaration syntax changed
 * and might leave some 2.3.x in limbo, but should hopefully do the job
 * of reliably distinguishing 2.2 vs 2.4.
 */
#define NEWVERSION AP_MODULE_MAGIC_AT_LEAST(20100919,1)

/* vacuous hack to pretend Apache's OK and Ironbee's IB_OK might be nonzero */
#define IB2AP(rc) (OK - IB_OK + (rc))

#define STATUS_IS_ERROR(code) ( ((code) >= 200) && ((code) <  600) )
#define STATUS_IS_OK(code)    ( ((code)  < 200) || ((code) >= 600) )

/*************    APACHE MODULE AND TYPES   *****************/
module AP_MODULE_DECLARE_DATA ironbee_module;

#define HDRS_IN IB_SERVER_REQUEST
#define HDRS_OUT IB_SERVER_RESPONSE
#define START_RESPONSE 0x04
#define NO_REQUEST_BODY 0x10

/* Flags to keep track of what's been notified, for functions that
 * could be called more than once in the event of a subrequest or
 * internal redirect, or in ap_discard_request_body
 */
#define NOTIFY_REQ_START   0x100
#define NOTIFY_REQ_END     0x200
#define NOTIFY_RESP_START  0x400
#define NOTIFY_RESP_END    0x800
#define INTERNAL_ERRORDOC  0x10000

typedef enum { IOBUF_NOBUF, IOBUF_DISCARD, IOBUF_BUFFER } iobuf_t;
typedef struct ironbee_req_ctx {
    ib_tx_t *tx;
    int status;
    int state;
    request_rec *r;
    /* make buffering info a request ctx field so the output header filter
     * can access it.
     */
    iobuf_t input_buffering;
    iobuf_t output_buffering;
} ironbee_req_ctx;
typedef struct ironbee_filter_ctx {
    apr_bucket_brigade *buffer;
    bool eos_sent;
} ironbee_filter_ctx;

typedef struct ironbee_svr_conf {
    int early;
} ironbee_svr_conf;

typedef struct ironbee_dir_conf {
    int filter_input;
    int filter_output;
} ironbee_dir_conf;

/*************    GENERAL GLOBALS        *************************/
static const char *ironbee_config_file = NULL;
static ib_engine_t *ironbee = NULL;
static int log_level_is_startup = APLOG_STARTUP;

/*************    IRONBEE-DRIVEN PROVIDERS/CALLBACKS/ETC ***********/

/* Application data for apr_table_do to apply regexp to a header */
typedef struct {
    ib_mpool_t *mp;
    apr_table_t *t;
    ib_rx_t *rx;
} edit_do;

/**
 * APR callback function to process one header according to a regexp
 *
 * @param[in] v - Application Data Pointer
 * @param[in] key - Header
 * @param[in] val - Header Value
 * @return 1 (continue iterating over APR table)
 */
static int edit_header(void *v, const char *key, const char *val)
{
    edit_do *ed = (edit_do *)v;
    char *repl;

    /* Note - Since we were passed an Ironbee regexp, we pass it an
     * ironbee tx pool from which repl gets allocated.  Everything else
     * uses apache's request pool.  That's OK, both have the same lifetime.
     */
    ib_rx_exec(ed->mp, ed->rx, val, &repl, NULL);
    if (repl == NULL) /* FIXME: do something? */
        return 1;

    apr_table_addn(ed->t, key, repl);
    return 1;
}

/**
 * Ironbee callback function to manipulate an HTTP header
 *
 * @param[in] tx - Ironbee transaction
 * @param[in] dir - Request/Response
 * @param[in] action - Requested header manipulation
 * @param[in] hdr - Header
 * @param[in] value - Header Value
 * @param[in] rx - Compiled regexp of value (if applicable)
 * @return status (OK, Declined if called too late, Error if called with
 *                 invalid data).  NOTIMPL should never happen.
 */
static ib_status_t ib_header_callback(ib_tx_t *tx, ib_server_direction_t dir,
                                      ib_server_header_action_t action,
                                      const char *hdr, const char *value,
                                      ib_rx_t *rx, void *cbdata)
{
    ironbee_req_ctx *ctx = tx->sctx;
    apr_table_t *headers = (dir == IB_SERVER_REQUEST)
                                ? ctx->r->headers_in : ctx->r->headers_out;

    if (ctx->state & HDRS_OUT ||
        (ctx->state & HDRS_IN && dir == IB_SERVER_REQUEST))
        return IB_DECLINED;  /* too late for requested op */

    switch (action) {
      case IB_HDR_SET:
        apr_table_set(headers, hdr, value);
        return IB_OK;
      case IB_HDR_UNSET:
        apr_table_unset(headers, hdr);
        return IB_OK;
      case IB_HDR_ADD:
        apr_table_add(headers, hdr, value);
        return IB_OK;
      case IB_HDR_MERGE:
      case IB_HDR_APPEND:
        apr_table_merge(headers, hdr, value);
        return IB_OK;
      case IB_HDR_EDIT:
        if (apr_table_get(headers, hdr)) {
            edit_do ed;

            /* Check we were passed something valid */
            if (rx == NULL) {
                if (rx = ib_rx_compile(tx->mp, value), rx == NULL) {
                    ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, ctx->r,
                                  "Failed to compile %s as regexp", value);
                    return IB_EINVAL;
                }
            }

            ed.mp = tx->mp;
            ed.rx = rx;
            ed.t = apr_table_make(ctx->r->pool, 5);
            if (!apr_table_do(edit_header, (void *) &ed, headers, hdr, NULL))
                return IB_EINVAL;
            apr_table_unset(headers, hdr);
            if (dir == IB_SERVER_REQUEST)
                ctx->r->headers_in = apr_table_overlay(ctx->r->pool,
                                                       headers, ed.t);
            else
                ctx->r->headers_out = apr_table_overlay(ctx->r->pool,
                                                        headers, ed.t);
        }
        return IB_OK;
    }
    return IB_ENOTIMPL;
}
/**
 * Ironbee callback function to set an HTTP error status.
 * This will divert processing into an ErrorDocument for the status.
 *
 * @param[in] tx - Ironbee transaction
 * @param[in] status - Status to set
 * @return OK, or Declined if called too late.  NOTIMPL should never happen.
 */
static ib_status_t ib_error_callback(ib_tx_t *tx, int status, void *cbdata)
{
    ironbee_req_ctx *ctx = tx->sctx;
    if (STATUS_IS_ERROR(status)) {
        if (STATUS_IS_ERROR(ctx->status)) {
            ap_log_rerror(APLOG_MARK, APLOG_WARNING, 0, ctx->r,
                          "Ignoring: status already set to %d", ctx->status);
            return IB_OK;
        }
        if (ctx->state & START_RESPONSE) {
            ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, ctx->r,
                          "Too late to change status=%d", status);
            return IB_DECLINED;
        }
        ctx->status = status;
        return IB_OK;
    }
    return IB_ENOTIMPL;
}

/**
 * Ironbee callback function to set an HTTP header for an ErrorDocument.
 *
 * @param[in] tx - Ironbee transaction
 * @param[in] hdr - Header to set
 * @param[in] val - Value to set
 * @return OK, or Declined if called too late, or EINVAL.
 */
static ib_status_t ib_errhdr_callback(ib_tx_t *tx, const char *hdr, const char *val, void *cbdata)
{
    ironbee_req_ctx *ctx = tx->sctx;
    if (ctx->state & START_RESPONSE)
        return IB_DECLINED;
    if (!hdr || !val)
        return IB_EINVAL;

    apr_table_set(ctx->r->err_headers_out, hdr, val);
    return IB_OK;
}

/**
 * Ironbee callback function to set an errordocument
 * Since httpd has its own internal ErrorDocument mechanism,
 * we use that for the time being and leave this NOTIMPL
 *
 * TODO: think about something along the lines of mod_choices's errordoc.
 *
 * @param[in] tx - Ironbee transaction
 * @param[in] data - Data to set
 * @return NOTIMPL, or Declined if called too late, or EINVAL.
 */
static ib_status_t ib_errdata_callback(ib_tx_t *tx, const char *data, void *cbdata)
{
    ironbee_req_ctx *ctx = tx->sctx;
    if (ctx->state & START_RESPONSE)
        return IB_DECLINED;
    if (!data)
        return IB_EINVAL;

/* Maybe implement something here?
    ctx->errdata = apr_pstrdup(ctx->r->pool, data);
    return IB_OK;
*/
    return IB_ENOTIMPL;
}

/**
 * The ironbee plugin
 */
static ib_server_t ibplugin = {
    IB_SERVER_HEADER_DEFAULTS,
    "httpd-ironbee",
    ib_header_callback,
    NULL,
    ib_error_callback,
    NULL,
    ib_errhdr_callback,
    NULL,
    ib_errdata_callback,
    NULL,
};

/* BOOTSTRAP: lift logger straight from the old mod_ironbee */
/**
 * IronBee callback: logger function.
 *
 * Performs IronBee logging for the ATS plugin.
 *
 * @param[in] ib IronBee engine.
 * @param[in] level Log level
 * @param[in] ib IronBee engine
 * @param[in] file File name
 * @param[in] line Line number
 * @param[in] fmt Format string
 * @param[in] ap Var args list to match the format
 * @param[in] cbdata Callback data.
 */
static
void ironbee_logger(
    const ib_engine_t *ib,
    ib_log_level_t     level,
    const char        *file,
    int                line,
    const char        *fmt,
    va_list            ap,
    void              *cbdata
)
{
    char *buf = NULL;
    int limit = 7000;
    int ap_level = APLOG_WARNING | log_level_is_startup;
    int ec;
    const size_t c_buf_size = 8192 + 1;
    buf = (char *)malloc(c_buf_size);
    if (buf == NULL) {
        return;
    }

    /* Buffer the log line. */
    ec = vsnprintf(buf, c_buf_size, fmt, ap);
    if (ec >= limit) {
        /* Mark as truncated, with a " ...". */
        memcpy(buf + (limit - 5), " ...", 5);

        /// @todo Do something about it
        ap_log_error(APLOG_MARK, ap_level, 0, NULL,
                     IB_PRODUCT_NAME ": Log format truncated: limit (%d/%d)",
                     (int)ec, limit);
    }

    /* Translate the log level. */
    switch (level) {
        case IB_LOG_EMERGENCY:
            ap_level = APLOG_EMERG;
            break;
        case IB_LOG_ALERT:
            ap_level = APLOG_ALERT;
            break;
        case IB_LOG_CRITICAL:
        case IB_LOG_ERROR:
            ap_level = APLOG_ERR;
            break;
        case IB_LOG_WARNING:
            //ap_level = APLOG_WARNING;
            ap_level = APLOG_DEBUG; /// @todo For now, so we get file/line
            break;
        case IB_LOG_DEBUG:
            ap_level = APLOG_DEBUG;
            break;
        default:
            ap_level = APLOG_DEBUG; /// @todo Make configurable
    }

    /// @todo Make configurable
    if (ap_level > APLOG_NOTICE) {
        ap_level = APLOG_NOTICE;
    }

    ap_level |= log_level_is_startup;

    /* Write it to the error log. */
    ap_log_error(APLOG_MARK, ap_level, 0, NULL, "ironbee: %s", buf);

    if (buf) {
        free(buf);
    }
}

/***********   APACHE PER-REQUEST FILTERS AND HOOKS  ************/

/**
 * APR Callback function to set a header in ib_parsed_header_wrapper
 *
 * @param[in] data - the header wrapper
 * @param[in] key - the header
 * @param[in] value - the value
 * @return 1 (continue)
 */
static int ironbee_sethdr(void *data, const char *key, const char *value)
{
    ib_parsed_name_value_pair_list_add((ib_parsed_header_wrapper_t*)data,
                                       key, strlen(key),
                                       value, strlen(value));
    return 1;
}

/**
 * APR cleanup function to notify end-of-tx and destroy Ironbee Transaction.
 * @param[in] data - the request
 * @return SUCCESS
 */
static apr_status_t ib_req_cleanup(void *data)
{
    ib_status_t rc;
    request_rec *r = data;
    ironbee_req_ctx *ctx = ap_get_module_config(r->request_config,
                                                &ironbee_module);

    if (!ib_tx_flags_isset(ctx->tx, IB_TX_FPOSTPROCESS)) {
        rc = ib_state_notify_postprocess(ironbee, ctx->tx);
        if (rc != IB_OK) {
            return IB2AP(rc);
        }
    }
    if (!ib_tx_flags_isset(ctx->tx, IB_TX_FLOGGING)) {
        rc = ib_state_notify_logging(ironbee, ctx->tx);
        if (rc != IB_OK) {
            return IB2AP(rc);
        }
    }
    ib_tx_destroy(ctx->tx);
    return APR_SUCCESS;
}

/**
 * HTTPD callback function to notify Ironbee of Request start and headers.
 * NOTE: This is called both in post_read_request and fixups hooks
 *       and will notify Ironbee in one but not both, according
 *       to the IronbeeRawHeaders configuration setting.
 * @param[in] r - The Request.
 * @return DECLINED (leave no footprint), or HTTP error set by Ironbee
 */
static int ironbee_headers_in(request_rec *r)
{
    const char *hdr;
    int early;
    ib_status_t rc = IB_OK;
    const char *rc_what = "no message set";
    ironbee_req_ctx *ctx = ap_get_module_config(r->request_config,
                                                &ironbee_module);
    ib_conn_t *iconn = ap_get_module_config(r->connection->conn_config,
                                            &ironbee_module);
    ironbee_svr_conf *scfg = ap_get_module_config(r->server->module_config,
                                                  &ironbee_module);

    /* Don't act in a subrequest or internal redirect */
    /* FIXME: this means 'clever' things like content aggregation
     * through SSI/ESI/mod_publisher could slip under the radar.
     * That's not a concern, but we do need to think through how
     * we're treating ErrorDocuments here.  Also test with mod_rewrite.
     */
    if (r->main || r->prev) {
        return DECLINED;
    }

    if (ctx) {
        early = 0;
    }
    else {
        early = 1;
        /* Create ironbee tx data and save it to Request ctx */
        ctx = apr_pcalloc(r->pool, sizeof(ironbee_req_ctx));
        ib_tx_create(&ctx->tx, iconn, ctx);
        /* Tie the tx lifetime to the Request */
        apr_pool_cleanup_register(r->pool, r, ib_req_cleanup,
                                  apr_pool_cleanup_null);
        ap_set_module_config(r->request_config, &ironbee_module, ctx);
        ctx->r = r;
    }

    /* We act either early or late, according to config.
     * So don't try to do both!
     */
    if (((scfg->early && early) || (!scfg->early && !early))
        && !(ctx->state & NOTIFY_REQ_START)) {
        /* Notify Ironbee of request line and headers */

        /* First construct and notify the request line */
        ib_parsed_req_line_t *rline;
        ib_parsed_header_wrapper_t *ibhdrs;

        ctx->state |= NOTIFY_REQ_START;

        rc = ib_parsed_req_line_create(ctx->tx, &rline,
                                       r->the_request, strlen(r->the_request),
                                       r->method, strlen(r->method),
                                       r->unparsed_uri, strlen(r->unparsed_uri),
                                       r->protocol, strlen(r->protocol));
        if (rc != IB_OK) {
            rc_what = "ib_parsed_req_line_create";
            goto finished;
        }

        rc = ib_state_notify_request_started(ironbee, ctx->tx, rline);
        if (rc != IB_OK) {
            rc_what = "ib_state_notify_request_started";
            goto finished;
        }

        /* Now the request headers */
        rc = ib_parsed_name_value_pair_list_wrapper_create(&ibhdrs, ctx->tx);
        if (rc != IB_OK) {
            rc_what = "ib_parsed_name_value_pair_list_wrapper_create";
            goto finished;
        }

        apr_table_do(ironbee_sethdr, ibhdrs, r->headers_in, NULL);

        if (ibhdrs->size > 0) {
            rc = ib_state_notify_request_header_data(ironbee, ctx->tx, ibhdrs);
            if (rc != IB_OK) {
                rc_what = "ib_state_notify_request_header_data";
                goto finished;
            }
        }

        rc = ib_state_notify_request_header_finished(ironbee, ctx->tx);
        if (rc != IB_OK) {
            rc_what = "ib_state_notify_request_header_finished";
            goto finished;
        }

        /* Determine whether we have a request body.
         * If not, notify ironbee end-of-request now and keep a record
         */
        hdr = apr_table_get(r->headers_in, "Content-Length");
        if (!hdr) {
            hdr = apr_table_get(r->headers_in, "Transfer-Encoding");
            if (!hdr || strcasecmp(hdr, "chunked")) {
                ctx->state |= NO_REQUEST_BODY | NOTIFY_REQ_END;
                rc = ib_state_notify_request_finished(ironbee, ctx->tx);
                if (rc != IB_OK) {
                    rc_what = "ib_state_notify_request_finished";
                    goto finished;
                }
            }
        }

finished:
        if (rc != IB_OK) {
            ap_log_rerror(
                APLOG_MARK, APLOG_ERR, 0, r,
                "%s failed with %d",
                rc_what, rc
            );
        }
    }


    /* Regardless of whether we process early or late, it's not too
     * late to set request headers until after the second call to us
     */
    if (!early)
        ctx->state |= HDRS_IN;

    /* If Ironbee has signalled an error, we can just return it now
     * to divert into the appropriate errordocument.
     */
    if (STATUS_IS_ERROR(ctx->status)) {
        ctx->state |= INTERNAL_ERRORDOC;
        return ctx->status;
    }

    /* Continue ... */
    return DECLINED;
}

/**
 * HTTPD filter function to notify Ironbee of Response headers
 * Removes itself from filter chain after the first call.
 *
 * @param[in] f - the filter struct
 * @param[in] bb - the bucket brigade (data)
 * @return status propagated from next filter in chain
 */
static apr_status_t ironbee_header_filter(ap_filter_t *f,
                                          apr_bucket_brigade *bb)
{
    ap_filter_t *nextf = f->next;
    ib_status_t rc;
    ib_parsed_resp_line_t *rline;
    ib_parsed_header_wrapper_t *ibhdrs;
    const char *cstatus;
    const char *reason;
    ironbee_req_ctx *ctx = ap_get_module_config(f->r->request_config,
                                                &ironbee_module);

    /* Shouldn't happen, but in case of multi-request wierdness ... */
    if (ctx->state & NOTIFY_RESP_START) {
        ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, f->r,
                      "Ignoring extra call to ironbee_header_filter!");
        ap_remove_output_filter(f);
        return ap_pass_brigade(nextf, bb);
    }
    ctx->state |= NOTIFY_RESP_START;

    /* Notify Ironbee of start of output */
    cstatus = apr_psprintf(f->r->pool, "%d", f->r->status);

    /* Status line may be set explicitly. If not, use default for code.  */
    reason = f->r->status_line;
    if (!reason) {
        reason = ap_get_status_line(f->r->status);
        if (reason)
            /* ap_get_status_line returned "nnn Reason", so skip 4 chars */
            reason += 4;
        else
            reason = "Other";
    }

    rc = ib_parsed_resp_line_create(ctx->tx, &rline, NULL, 0,
                                    "HTTP/1.1", 8,
                                    cstatus, strlen(cstatus),
                                    reason, strlen(reason));
    if (rc != IB_OK) {
        ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, f->r,
                      "ib_parsed_resp_line_create failed with %d", rc);
        goto header_filter_cleanup;
    }
    rc = ib_state_notify_response_started(ironbee, ctx->tx, rline);
    if (rc != IB_OK) {
        ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, f->r,
                      "ib_state_notify_response_started failed with %d", rc);
        goto header_filter_cleanup;
    }

    /* Notify Ironbee of output headers */
    rc = ib_parsed_name_value_pair_list_wrapper_create(&ibhdrs, ctx->tx);
    if (rc != IB_OK) {
        ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, f->r,
                      "ib_parsed_name_value_pair_list_wrapper_create failed with %d", rc);
        goto header_filter_cleanup;
    }
    apr_table_do(ironbee_sethdr, ibhdrs, f->r->headers_out, NULL);
    apr_table_do(ironbee_sethdr, ibhdrs, f->r->err_headers_out, NULL);
    if (ibhdrs->size > 0) {
        rc = ib_state_notify_response_header_data(ironbee, ctx->tx, ibhdrs);
        if (rc != IB_OK) {
            ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, f->r,
                          "ib_state_notify_response_header_data failed with %d", rc);
        }
    }
    rc = ib_state_notify_response_header_finished(ironbee, ctx->tx);
    if (rc != IB_OK) {
        ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, f->r,
                      "ib_state_notify_response_header_finished failed with %d", rc);
    }

    /* TODO: If Ironbee signals an error, deal with it here */

header_filter_cleanup:
    /* At this point we've burned our boats for setting output headers,
     * and started the response
     */
    ctx->state |= HDRS_OUT|START_RESPONSE;

    /* Our business is done.  Remove ourself from filter chain. */
    ap_remove_output_filter(f);

    if (ctx->output_buffering == IOBUF_BUFFER) {
        /* We expect to get called when ironbee_filter_out sends us
         * a lone flush bucket.  If that happens, we can skip passing
         * it any further, so ironbee output buffering works before
         * the response has been initiated.
         *
         * But we need to check, in case another filter has intervened
         * and inserted data or different metadata.
         *
         * TODO: think about making it a fatal error if someone has
         * inserted data at this point.  Any data here have skipped
         * scrutiny by ironbee!  This could make ironbee incompatible
         * with some module, though such a module would be unconventional
         * and possibly trojan.
         */
        apr_bucket *b;
        int our_brigade = 1;
        for (b = APR_BRIGADE_FIRST(bb);
             b != APR_BRIGADE_SENTINEL(bb);
             b = APR_BUCKET_NEXT(b)) {
            if (!APR_BUCKET_IS_FLUSH(b)) {
                our_brigade = 0;
                ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, f->r,
                              "Ironbee: can't hold back response headers");
                break;
            }
        }
        if (our_brigade) {
            ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, f->r,
                          "Ironbee: holding back response headers");
            return APR_SUCCESS;
        }
    }
    /* propagate to next filter unless we held back */
    return ap_pass_brigade(nextf, bb);
}

/**
 * HTTPD filter function to notify Ironbee of Response data,
 * and buffer data if required by Ironbee
 *
 * @param[in] f - the filter struct
 * @param[in] bb - the bucket brigade (data)
 * @return status propagated from next filter in chain
 */
static apr_status_t ironbee_filter_out(ap_filter_t *f, apr_bucket_brigade *bb)
{
    ib_status_t rc;
    apr_status_t rv = APR_SUCCESS;
    ironbee_filter_ctx *ctx = f->ctx;
    apr_size_t bytecount = 0;
    int eos_seen = 0;
    int growing = 0;
    apr_bucket *b;
    apr_bucket *bnext;
    ib_txdata_t itxdata;
    const char *buf;
    ironbee_req_ctx *rctx = ap_get_module_config(f->r->request_config,
                                                 &ironbee_module);

    if (ctx == NULL) {
        ib_num_t num;
        /* First call: initialise data out */

        f->ctx = ctx = apr_pcalloc(f->r->pool, sizeof(ironbee_filter_ctx));
        ctx->buffer = apr_brigade_create(f->r->pool, f->c->bucket_alloc);

        /* We trust our internally-generated errordocument */
        if (rctx->state & INTERNAL_ERRORDOC) {
            rctx->output_buffering = IOBUF_NOBUF;
        }
        /* Determine whether we're configured to buffer */
        else {
            rc = ib_context_get(rctx->tx->ctx, "buffer_res",
                                ib_ftype_num_out(&num), NULL);
            if (rc != IB_OK)
                ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, f->r,
                              "Can't determine output buffer configuration!");
            if (num == 0) {
                rctx->output_buffering = IOBUF_NOBUF;
            }
            else {
                /* If we're buffering, initialise the buffer */
                rctx->output_buffering = IOBUF_BUFFER;
            }
        }

        /* First send a flush down the chain to trigger
         * the header filter and notify ironbee of the headers,
         * as well as tell the client we're alive.
         */
        APR_BRIGADE_INSERT_TAIL(ctx->buffer,
                                apr_bucket_flush_create(f->c->bucket_alloc));
        rv = ap_pass_brigade(f->next, ctx->buffer);
        apr_brigade_cleanup(ctx->buffer);
        if (rv != APR_SUCCESS) {
            ap_log_rerror(APLOG_MARK, APLOG_ERR, rv, f->r,
                          "Filter error before Ironbee response body filter");
            return rv;
        }
    }

    for (b = APR_BRIGADE_FIRST(bb); b != APR_BRIGADE_SENTINEL(bb); b = bnext) {
        /* save pointer to next buxket, in case we clobber b */
        bnext = APR_BUCKET_NEXT(b);

        if (APR_BUCKET_IS_METADATA(b)) {
            if (APR_BUCKET_IS_EOS(b))
                eos_seen = 1;
            /* Skip the data reading on non-data bucket
             * We don't use a simple 'continue', because we still want to
             * preserve buckets and ordering if we're buffering below.
             */
            goto setaside_output;
        }

        /* Now read the bucket and feed to ironbee */
        growing = (b->length == (apr_size_t)-1) ? 1 : growing;
        apr_bucket_read(b, &buf, &itxdata.dlen, APR_BLOCK_READ);
        itxdata.data = (uint8_t*) buf;
        bytecount += itxdata.dlen;
        ib_state_notify_response_body_data(ironbee, rctx->tx, &itxdata);

        /* If Ironbee just signalled an error, switch to discard data mode,
         * dump anything we already have buffered,
         * and pass EOS down the chain immediately.
         *
         * We need to check INTERNAL_ERRORDOC explicitly as that otherwise
         * gives a false positive and swallows our own errordoc.
         */
        if ( (STATUS_IS_ERROR(rctx->status)) &&
             !(rctx->state & INTERNAL_ERRORDOC) &&
             (rctx->output_buffering != IOBUF_DISCARD) ) {
            if (rctx->output_buffering == IOBUF_BUFFER) {
                apr_brigade_cleanup(ctx->buffer);
            }
            rctx->output_buffering = IOBUF_DISCARD;
            APR_BRIGADE_INSERT_TAIL(ctx->buffer,
                                    apr_bucket_eos_create(f->c->bucket_alloc));
            rv = ap_pass_brigade(f->next, ctx->buffer);
        }

setaside_output:
        /* If we're buffering this, move it to our buffer and ensure
         * its lifetime is sufficient.  If we're discarding it then do.
         */
        if (rctx->output_buffering == IOBUF_BUFFER) {
            apr_bucket_setaside(b, f->r->pool);
            APR_BUCKET_REMOVE(b);
            APR_BRIGADE_INSERT_TAIL(ctx->buffer, b);
        }
        else if (rctx->output_buffering == IOBUF_DISCARD) {
            apr_bucket_destroy(b);
        }
    }

    if (rctx->output_buffering == IOBUF_NOBUF) {
        /* Normal operation - pass it down the chain */
        rv = ap_pass_brigade(f->next, bb);
    }
    else if (rctx->output_buffering == IOBUF_BUFFER && eos_seen) {
        /* We can pass on the buffered data all at once */
        rv = ap_pass_brigade(f->next, ctx->buffer);
    }
    else {
        /* We currently have nothing we can pass.  Just clean up any
         * data that got orphaned if we switched from NOBUF to DISCARD mode
         */
        apr_brigade_cleanup(bb);
    }

    return rv;
}

/**
 * HTTPD filter function to notify Ironbee of Request data,
 * and buffer data if required by Ironbee
 *
 * @param[in] f - the filter struct
 * @param[in] bb - the bucket brigade (data)
 * @return status propagated from next filter in chain
 */
static apr_status_t ironbee_filter_in(ap_filter_t *f,
                                      apr_bucket_brigade *bb,
                                      ap_input_mode_t mode,
                                      apr_read_type_e block,
                                      apr_off_t readbytes)
{
    apr_status_t rv = APR_SUCCESS;
    int eos_seen = 0;
    ib_status_t rc;
    ironbee_filter_ctx *ctx = f->ctx;
    ironbee_req_ctx *rctx = ap_get_module_config(f->r->request_config,
                                                 &ironbee_module);
    ironbee_dir_conf *dconf = ap_get_module_config(f->r->per_dir_config,
                                                   &ironbee_module);
    apr_bucket *b;
    apr_bucket *bnext;
    int growing = 0;
    const char *buf;
    ib_txdata_t itxdata;
    apr_status_t bytecount = 0;

    /* If this is a dummy call, bail out */
    if (rctx->state & NOTIFY_REQ_END) {
        ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, f->r,
                      "Extra call to ironbee_filter_in ignored");
        ap_remove_input_filter(f);
        return ap_get_brigade(f->next, bb, mode, block, readbytes);
    }

    if (ctx == NULL) {
        ib_num_t num;
        /* First call: initialise data out */

        /* Determine whether we're configured to buffer */
        ctx = f->ctx = apr_palloc(f->r->pool, sizeof(ironbee_filter_ctx));
        rc = ib_context_get(rctx->tx->ctx, "buffer_req",
                            ib_ftype_num_out(&num), NULL);
        if (rc != IB_OK)
            ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, f->r,
                          "Can't determine output buffer configuration!");
        rctx->input_buffering = (num == 0) ? IOBUF_NOBUF : IOBUF_BUFFER;
        /* If we're buffering, initialise the buffer */
        ctx->buffer = apr_brigade_create(f->r->pool, f->c->bucket_alloc);
        ctx->eos_sent = false;
    }

    /* If we're buffering, loop over all data before returning.
     * Else just take whatever one get_brigade gives us and return it
     */
    do {
        rv = ap_get_brigade(f->next, bb, mode, block, readbytes);

        for (b = APR_BRIGADE_FIRST(bb);
             b != APR_BRIGADE_SENTINEL(bb);
             b = bnext) {
            /* save pointer to next buxket, in case we clobber b */
            bnext = APR_BUCKET_NEXT(b);

            /* If we're not feeding the data to Ironbee,
             * all we need do is just check for EOS here
             */
            if (!dconf->filter_input) {
                if (APR_BUCKET_IS_EOS(b))
                    eos_seen = 1;
                continue;
            }
            if (APR_BUCKET_IS_METADATA(b)) {
                if (APR_BUCKET_IS_EOS(b))
                    eos_seen = 1;
                /* Skip the data reading on non-data bucket
                 * We don't use a simple 'continue', because we still want to
                 * preserve buckets and ordering if we're buffering below.
                 */
                goto setaside_input;
            }

            /* Now read the bucket and feed to ironbee */
            growing = (b->length == (apr_size_t)-1) ? 1 : growing;
            apr_bucket_read(b, &buf, &itxdata.dlen, APR_BLOCK_READ);
            itxdata.data = (uint8_t*) buf;
            bytecount += itxdata.dlen;
            ib_state_notify_request_body_data(ironbee, rctx->tx, &itxdata);

            /* If Ironbee just signalled an error, switch to discard data mode,
             * and dump anything we already have buffered,
             */
            if ( (STATUS_IS_ERROR(rctx->status)) &&
                 (rctx->input_buffering != IOBUF_DISCARD) ) {
                apr_brigade_cleanup(ctx->buffer);
                rctx->input_buffering = IOBUF_DISCARD;
                rctx->state |= INTERNAL_ERRORDOC;
                f->r->status = rctx->status;
                ap_send_error_response(f->r, rctx->status);
            }

setaside_input:
            /* If we're buffering this, move it to our buffer
             * If we're discarding it then do.
             */
            if (rctx->input_buffering == IOBUF_BUFFER) {
                APR_BUCKET_REMOVE(b);
                APR_BRIGADE_INSERT_TAIL(ctx->buffer, b);
            }
            else if (rctx->input_buffering == IOBUF_DISCARD) {
                apr_bucket_destroy(b);
            }
        }
    } while (!eos_seen && rctx->input_buffering == IOBUF_BUFFER);

    if (eos_seen && !ctx->eos_sent) {
        ib_state_notify_request_finished(ironbee, rctx->tx);
        ctx->eos_sent = true;
        /* We're done with the data.  Avoid risk of getting called again */
        ap_remove_input_filter(f);
        rctx->state |= NOTIFY_REQ_END;
    }

    /* If Ironbee just signalled an error, switch to discard data mode,
     * and dump anything we already have buffered,
     */
    if ( (STATUS_IS_ERROR(rctx->status)) &&
         (rctx->input_buffering != IOBUF_DISCARD) ) {
        apr_brigade_cleanup(ctx->buffer);
        rctx->input_buffering = IOBUF_DISCARD;
        rctx->state |= INTERNAL_ERRORDOC;
        f->r->status = rctx->status;
        ap_send_error_response(f->r, rctx->status);
    }

    if ((dconf->filter_input == 0) || (rctx->input_buffering == IOBUF_NOBUF)) {
        /* Normal operation - return status from get_data */
        return rv;
    }
    else if (rctx->input_buffering == IOBUF_BUFFER) {
        /* Return the data from our buffer to caller's brigade before return */
        APR_BRIGADE_CONCAT(bb, ctx->buffer);
        return rv;
    }
    else {
        /* Discarding input - return with nothing except EOS */
        apr_brigade_cleanup(bb);
        if (eos_seen) {
            APR_BRIGADE_INSERT_TAIL(bb, apr_bucket_eos_create(f->c->bucket_alloc));
        }
        return APR_EGENERAL; /* FIXME - is there a better error? */
    }
}
/**
 * HTTPD callback function to insert filters
 * @param[in] r - the Request
 */
static void ironbee_filter_insert(request_rec *r)
{
    ironbee_dir_conf *cfg = ap_get_module_config(r->per_dir_config,
                                                 &ironbee_module);
    ironbee_req_ctx *rctx = ap_get_module_config(r->request_config,
                                                 &ironbee_module);
    request_rec *rr = r;
    while (!rctx) {
        /* Oops, are we in a subrequest or internal redirect?
         * Find main config and set it here
         */
        if (rr->prev)
            rr = rr->prev;
        else if (rr->main)
            rr = rr->main;
        else {
            /* Whoops!  Even the head request has no ctx!
             * Something bad happened
             */
            ap_log_rerror(APLOG_MARK, APLOG_CRIT, 0, r,
                          "No request context found - Ironbee disabled!");
            return;
        }
        rctx = ap_get_module_config(rr->request_config, &ironbee_module);
    }
    /* Set ctx for this request, so we don't have to re-run checks in filters */
    if (rr != r) {
        ap_set_module_config(r->request_config, &ironbee_module, rctx);
    }
    if (cfg->filter_input && !(rctx->state & NO_REQUEST_BODY))
        ap_add_input_filter("ironbee", NULL, r, r->connection);
    else {
        /* We already fed Ironbee the headers.  If we're not
         * filtering input, we can notify Ironbee end-of-request
         * right here and now.
         */
        if (!(rctx->state & NOTIFY_REQ_END)) {
            ib_state_notify_request_finished(ironbee, rctx->tx);
            rctx->state |= NOTIFY_REQ_END;
        }
    }
    if (cfg->filter_output)
        ap_add_output_filter("ironbee", NULL, r, r->connection);
    ap_add_output_filter("ironbee-headers", NULL, r, r->connection);
}

/*****************  PER-CONNECTION STUFF *********************/

/**
 * Ironbee callback function to populate iconn struct
 * @param[in] ib - the ironbee engine
 * @param[in] event - the ironbee event
 * @param[in] conn - the ironbee connection
 * @return OK, or propagated error
 */
static ib_status_t ironbee_conn_init(ib_engine_t *ib,
                                     ib_state_event_type_t event,
                                     ib_conn_t *iconn,
                                     void *cbdata)
{
    /* Set connection parameters in ironbee */
    /* iconn->remote_ipstr
     * iconn->remote_port
     * iconn->local_ipstr
     * iconn->local_port
     * iconn->data fields for local and remote ip
     */
    ib_status_t rc;
    conn_rec *conn = iconn->server_ctx;

/* These fields differ between 2.2 and 2.4 because the latter
 * introduces the distinction between the HTTP Client (end user)
 * and TCP client (next hop - may be a downstream proxy).
 * The 2.4 conn_rec gives us the latter.
 */
#if NEWVERSION
    iconn->remote_ipstr = conn->client_ip;
    iconn->remote_port = conn->client_addr->port;
    iconn->local_ipstr = conn->local_ip;
    iconn->local_port = conn->local_addr->port;

    rc = ib_data_add_bytestr(iconn->data,
                             "remote_ip",
                             (uint8_t *)iconn->remote_ipstr,
                             strlen(conn->client_ip),
                             NULL);
#else
    iconn->remote_ipstr = conn->remote_ip;
    iconn->remote_port = conn->remote_addr->port;
    iconn->local_ipstr = conn->local_ip;
    iconn->local_port = conn->local_addr->port;

    rc = ib_data_add_bytestr(iconn->data,
                             "remote_ip",
                             (uint8_t *)iconn->remote_ipstr,
                             strlen(conn->remote_ip),
                             NULL);
#endif
    if (rc != IB_OK)
        return rc;

    rc = ib_data_add_bytestr(iconn->data,
                             "local_ip",
                             (uint8_t *)iconn->local_ipstr,
                             strlen(conn->local_ip),
                             NULL);
    if (rc != IB_OK)
        return rc;

    return IB_OK;
}

/**
 * APR callback function to notify Ironbee of connection closed
 * and destroy the ib_conn struct
 *
 * @param[in] arg - the ib_conn struct
 * @return APR_SUCCESS
 */
static apr_status_t ironbee_conn_cleanup(void *arg)
{
    ib_state_notify_conn_closed(ironbee, (ib_conn_t*)arg);
    ib_conn_destroy((ib_conn_t*)arg);
    return APR_SUCCESS;
}

/**
 * HTTPD callback function to notify Ironbee of new connection
 * @param[in] conn - the new connection
 * @param[in] csd - unused
 * @return DECLINED (leave no footprint in HTTPD)
 */
static int ironbee_pre_conn(conn_rec *conn, void *csd)
{
    ib_conn_t *iconn;
    ib_status_t rc;

    /* Create the Ironbee conn, with HTTPD conn in its app data */
    rc = ib_conn_create(ironbee, &iconn, conn);
    if (rc != IB_OK) {
        return IB2AP(rc); // FIXME - figure out what to do
    }
    /* Save it */
    ap_set_module_config(conn->conn_config, &ironbee_module, iconn);
    /* Tie the ib_conn lifetime to the conn */
    apr_pool_cleanup_register(conn->pool, iconn, ironbee_conn_cleanup,
                              apr_pool_cleanup_null);
    ib_state_notify_conn_opened(ironbee, iconn);
    return DECLINED;
}

/*****************  STARTUP / END  ***************************/

/**
 * APR callback function to destroy Ironbee engine
 * @param[in] data - the Ironbee engine
 * @return APR_SUCCESS
 */
static apr_status_t ironbee_engine_cleanup(void *data)
{
    ib_engine_destroy(ironbee);
    return APR_SUCCESS;
}

/* Bootstrap: copy initialisation from trafficserver plugin */
/**
 * HTTPD callback to initialise Ironbee
 * @param[in] pool - Process pool
 * @param[in] ptmp - Temp pool
 * @param[in] plog - Log pool
 * @param[in] s - Base server
 */
static int ironbee_init(apr_pool_t *pool, apr_pool_t *ptmp, apr_pool_t *plog,
                        server_rec *s)
{
    ib_status_t rc;
    ib_context_t *ctx;
    ib_cfgparser_t *cp;

    if (ironbee_config_file == NULL) {
        ap_log_error(APLOG_MARK, APLOG_STARTUP|APLOG_NOTICE, 0, s,
                     "Ironbee is loaded but not configured!");
        return OK ^ (-1);
    }

    rc = ib_initialize();
    if (rc != IB_OK) {
        return IB2AP(rc);
    }

    ib_util_log_level(4);

    rc = ib_engine_create(&ironbee, &ibplugin);
    if (rc != IB_OK) {
        return IB2AP(rc);
    }

    ib_log_set_logger_fn(ironbee, ironbee_logger, NULL);
    /* Use the default log level function */

    rc = ib_engine_init(ironbee);
    if (rc != IB_OK)
        return IB2AP(rc);
    /* Tie the Ironbee lifetime to the server */
    apr_pool_cleanup_register(pool, NULL, ironbee_engine_cleanup,
                              apr_pool_cleanup_null);

    /* TODO: TS creates logfile at this point */

    ib_hook_conn_register(ironbee, conn_opened_event, ironbee_conn_init, NULL);

    /* Parse the config file. */
    rc = ib_cfgparser_create(&cp, ironbee);
    if ( (rc != IB_OK) || (cp == NULL) ) {
        return IB2AP(rc);
    }
    rc = ib_engine_config_started(ironbee, cp);
    if (rc != IB_OK) {
        return IB2AP(rc);
    }
    ctx = ib_context_main(ironbee);

    ib_context_set_num(ctx, "logger.log_level", 4);

    if (ironbee_config_file == NULL) {
        ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s,
                     IB_PRODUCT_NAME ": No config specified with IronBeeConfig directive");
    }
    rc = ib_cfgparser_parse(cp, ironbee_config_file);
    if (rc != IB_OK) {
        ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s,
                     IB_PRODUCT_NAME ": Failed to parse IronBee configuration");
        ib_engine_config_finished(ironbee);
        ib_cfgparser_destroy(cp);
        return IB2AP(rc);
    }

    rc = ib_engine_config_finished(ironbee);
    if (rc != IB_OK) {
        ib_cfgparser_destroy(cp);
        return IB2AP(rc);
    }

    rc = ib_cfgparser_destroy(cp);
    if (rc != IB_OK) {
        return IB2AP(rc);
    }

    /* any more logging is no longer happening at startup */
    /* This will trigger after the first config pass.
     * But that's fine, we have the message.
     */
    log_level_is_startup = 0;
    return OK;
}

/**
 * HTTPD module function to insert hooks and declare filters
 * @param[in] pool - APR pool
 */
static void ironbee_hooks(apr_pool_t *pool)
{
    /* Our header processing uses the same hooks as mod_headers and
     * needs to order itself with reference to that module if loaded
     */
    static const char * const mod_headers[] = { "mod_headers.c", NULL };

    /* Ironbee needs its own initialisation and configuration */
    ap_hook_post_config(ironbee_init, NULL, NULL, APR_HOOK_MIDDLE);

    /* Connection hook to set up conn stuff for ironbee */
    ap_hook_pre_connection(ironbee_pre_conn, NULL, NULL, APR_HOOK_MIDDLE);

    /* Main input and output filters */
    /* Set filter level between resource and content_set */
    ap_register_input_filter("ironbee", ironbee_filter_in, NULL,
                             AP_FTYPE_CONTENT_SET-1);
    ap_register_output_filter("ironbee", ironbee_filter_out, NULL,
                              AP_FTYPE_CONTENT_SET-1);

    /* Inspect request headers either early or late as config option.
     *
     * Early: AFTER early phase of mod_headers.  but before anything else.
     * Thus mod_headers can be used to simulate stuff for debugging,
     * but we'll ignore any other modules playing with our headers
     * (including normal operation of mod_headers).
     *
     * Late: immediately before request processing, so we record
     * exactly what's going to the app/backend, including anything
     * set internally by Apache.
     */
    ap_hook_post_read_request(ironbee_headers_in, mod_headers, NULL, APR_HOOK_FIRST);
    ap_hook_fixups(ironbee_headers_in, mod_headers, NULL, APR_HOOK_LAST);

    /* We also need a mod_headers-like hack to inspect outgoing headers */
    ap_register_output_filter("ironbee-headers", ironbee_header_filter,
                              NULL, AP_FTYPE_CONTENT_SET+1);

    /* Use our own insert filter hook.  This is best going last so anything
     * 'clever' happening elsewhere isn't troubled with ordering it.
     * And after even mod_headers, so we record anything it sets too.
     */
    ap_hook_insert_filter(ironbee_filter_insert, mod_headers, NULL, APR_HOOK_LAST);

    /* We want to notify Ironbee of error docs, too */
    ap_hook_insert_error_filter(ironbee_filter_insert, mod_headers, NULL, APR_HOOK_LAST);
}

/******************** CONFIG STUFF *******************************/

/**
 * Function to initialise HTTPD server configuration for Ironbee module
 * @param[in] p - The Pool
 * @param[in] s - The Server
 * @return The created configuration struct
 */
static void *ironbee_svr_config(apr_pool_t *p, server_rec *s)
{
    ironbee_svr_conf *cfg = apr_palloc(p, sizeof(ironbee_svr_conf));
    cfg->early = -1;   /* unset */
    return cfg;
}
/**
 * Function to merge HTTPD server configurations for Ironbee module
 * @param[in] p - The Pool
 * @param[in] BASE - The base config
 * @param[in] ADD - The config to merge in
 * @return The new merged configuration struct
 */
static void *ironbee_svr_merge(apr_pool_t *p, void *BASE, void *ADD)
{
    ironbee_svr_conf *base = BASE;
    ironbee_svr_conf *add = ADD;
    ironbee_svr_conf *cfg = apr_palloc(p, sizeof(ironbee_svr_conf));
    cfg->early = (add->early == -1) ? base->early : add->early;
    return cfg;
}
/**
 * Function to initialise HTTPD per-dir configuration for Ironbee module
 * @param[in] p - The Pool
 * @param[in] dummy - unused
 * @return The created configuration struct
 */
static void *ironbee_dir_config(apr_pool_t *p, char *dummy)
{
    ironbee_dir_conf *cfg = apr_palloc(p, sizeof(ironbee_dir_conf));
    cfg->filter_input = cfg->filter_output = -1;
    return cfg;
}
/**
 * Function to merge HTTPD per-dir configurations for Ironbee module
 * @param[in] p - The Pool
 * @param[in] BASE - The base config
 * @param[in] ADD - The config to merge in
 * @return The new merged configuration struct
 */
static void *ironbee_dir_merge(apr_pool_t *p, void *BASE, void *ADD)
{
    ironbee_dir_conf *base = BASE;
    ironbee_dir_conf *add = ADD;
    ironbee_dir_conf *cfg = apr_palloc(p, sizeof(ironbee_dir_conf));
    cfg->filter_input = (add->filter_input == -1)
                                    ? base->filter_input : add->filter_input;
    cfg->filter_output = (add->filter_output == -1)
                                    ? base->filter_output : add->filter_output;
    return cfg;
}

/**
 * HTTPD configuration callback to implement IronbeeRawHeaders
 * @param[in] cmd - the cmd_parms struct
 * @param[in] x - unused
 * @param[in] flag - The value set in configuration
 * @return NULL (success)
 */
static const char *reqheaders_early(cmd_parms *cmd, void *x, int flag)
{
    ironbee_svr_conf *cfg = ap_get_module_config(cmd->server->module_config,
                                                 &ironbee_module);
    cfg->early = flag;
    return NULL;
}
/**
 * HTTPD configuration callback to specify Ironbee config file
 * @param[in] cmd - the cmd_parms struct
 * @param[in] x - unused
 * @param[in] fname - The filename
 * @return NULL (success)
 */
static const char *ironbee_configfile(cmd_parms *cmd, void *x, const char *fname)
{
    const char *errmsg = ap_check_cmd_context(cmd, GLOBAL_ONLY);
    if (errmsg)
        return errmsg;

    // TODO: check the file here (for robustness against typos/etc)
    ironbee_config_file = fname;

    return NULL;
}

/**
 * Module Directives
 */
static const command_rec ironbee_cmds[] = {
    AP_INIT_TAKE1("IronbeeConfigFile", ironbee_configfile, NULL, RSRC_CONF,
                 "Ironbee configuration file"),
    AP_INIT_FLAG("IronbeeRawHeaders", reqheaders_early, NULL, RSRC_CONF,
                 "Report incoming request headers or backend headers"),
    AP_INIT_FLAG("IronbeeFilterInput", ap_set_flag_slot,
                 (void*)APR_OFFSETOF(ironbee_dir_conf, filter_input),
                 ACCESS_CONF, "Filter Input Data through Ironbee"),
    AP_INIT_FLAG("IronbeeFilterOutput", ap_set_flag_slot,
                 (void*)APR_OFFSETOF(ironbee_dir_conf, filter_output),
                 ACCESS_CONF, "Filter Output Data through Ironbee"),
    {NULL, {NULL}, NULL, 0, 0, NULL}
};

/**
 * Declare the module.
 */
#if NEWVERSION
AP_DECLARE_MODULE(ironbee)
#else
module AP_MODULE_DECLARE_DATA ironbee_module
#endif
= {
    STANDARD20_MODULE_STUFF,
    ironbee_dir_config,
    ironbee_dir_merge,
    ironbee_svr_config,
    ironbee_svr_merge,
    ironbee_cmds,
    ironbee_hooks
};
