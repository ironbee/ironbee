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
 * @brief Ironbee - regexp-based edits on Request and Response data streams
 *
 * NOTE: Although this module notionally supports unlimited
 *       numbers of regexps, it will collapse in an ungainly
 *       heap if different regexps produce overlapping matches.
 *       This should be fixed in due course!
 */

#include <assert.h>
#include <ctype.h>
#include <regex.h>
#include <ironbee/engine_state.h>
#include <ironbee/array.h>
#include <ironbee/module.h>
#include <ironbee/context.h>

#define MAX_BUFFER 4096
#define MAX_RX_MATCH 10

/** Module name. */
#define MODULE_NAME        rxfilter
/** Stringified version of MODULE_NAME */
#define MODULE_NAME_STR    IB_XSTRINGIFY(MODULE_NAME)

/* Configuration: array of regexp ops to apply to request and response data */
typedef struct rxfilter_cfg_t rxfilter_cfg_t;
struct rxfilter_cfg_t {
    ib_array_t *req_edits;
    ib_array_t *resp_edits;
};

/* Context for our filter: position, data buffer, error status */
typedef struct rxfilter_buffer_t rxfilter_buffer_t;
struct rxfilter_buffer_t {
    size_t offs;
    ib_bytestr_t *data;
    ib_status_t errnum;
};

/* Request context: filter contexts for request and response */
typedef struct rxfilter_ctx_t rxfilter_ctx_t;
struct rxfilter_ctx_t {
    rxfilter_buffer_t reqbuf;
    rxfilter_buffer_t respbuf;
};

/* Regexp op definition */
typedef struct rxop_t rxop_t;
struct rxop_t {
    enum { RX_SUBS, RX_AFTER, RX_BEFORE, RX_DELETE } rxtype;
    regex_t rx;
    const char *repl;
};

/** Construct the replacement string for a regexp edit op
 *
 * @param[in] mm - memory pool
 * @param[in] src - source data
 * @param[in] pmatch - regexp match
 * @param[in] repl - replacement string template
 * @param[out] repl_len - length of replacement string
 * @return replacement string
 */
static char *rx_repl(ib_mm_t mm, const char *src, regmatch_t pmatch[],
                     const char *repl, size_t *repl_len)
{
    const char *p;
    char c;
    size_t len = 0;
    int ch;
    char *ret;
    char *retp;

    /* calculate length */
    p = repl;
    while (c = *p++, c != '\0') {
        if (c == '$' && isdigit(*p)) {
            ch = *p++ - '0';
        }
        else {
            ch = MAX_RX_MATCH;
        }

        if (ch < MAX_RX_MATCH) {
            len += pmatch[ch].rm_eo - pmatch[ch].rm_so;
        }
        else {
            if (c == '\\' && *p) {
                ++p;
            }
            ++len;
        }
    }
    *repl_len = len;
    retp = ret = ib_mm_alloc(mm, len);

    /* now fill the buffer we just allocated */
    p = repl;
    while (c = *p++, c != '\0') {
        if (c == '$' && isdigit(*p)) {
            ch = *p++ - '0';
        }
        else {
            ch = MAX_RX_MATCH;
        }

        if (ch < MAX_RX_MATCH) {
            len = pmatch[ch].rm_eo - pmatch[ch].rm_so;
            memcpy(retp, src + pmatch[ch].rm_so, len);
            retp += len;
        }
        else {
            if (c == '\\' && *p) {
                c = *++p;
            }
            *retp++ = c;
        }
    }

    return ret;
}

/**
 * Ironbee filter function to apply regexp edits
 * @param[in] ib - the engine
 * @param[in] tx - the transaction
 * @param[in] state - Request Data or Response Data stte
 * @param[in] data - data to process
 * @param[in] data_length - data length in bytes
 * @return success or error
 */
static ib_status_t rxfilter(ib_engine_t *ib,
                            ib_tx_t *tx,
                            ib_state_t state,
                            const char *data,
                            size_t data_length,
                            void *cbdata
)
{
    const ib_server_t *svr;
    rxfilter_cfg_t *cfg;
    rxfilter_ctx_t *ctx;
    rxfilter_buffer_t *rxbuf;
    ib_array_t *regexps;
    rxop_t *rx;
    ib_server_direction_t dir;
    char *buf;
    const char *p;
    size_t len;
    size_t hwm = 0;
    unsigned int i;
    size_t nelts;
    ib_status_t rc;
    int rv;
    regmatch_t pmatch[MAX_RX_MATCH];
    const char *repl;
    size_t repl_len;
    off_t start;
    size_t delbytes;
    ib_module_t *m;

    /* retrieve svr, ctx and cfg; initialise ctx if first call */
    svr = ib_engine_server_get(ib);

    rc = ib_engine_module_get(ib, MODULE_NAME_STR, &m);
    assert((rc == IB_OK) && (m != NULL));

    rc = ib_context_module_config(ib_context_main(ib), m, &cfg);
    assert((rc == IB_OK) && (cfg != NULL));

    /* returns ENOENT if module data not set */
    rc = ib_tx_get_module_data(tx, m, &ctx);
    if (rc == IB_ENOENT || ctx == NULL) {
        ctx = ib_mm_alloc(tx->mm, sizeof(rxfilter_ctx_t));
        ctx->reqbuf.offs = ctx->respbuf.offs = 0;
        ctx->reqbuf.errnum = ctx->respbuf.errnum = IB_OK;
        ctx->reqbuf.data = ctx->respbuf.data = NULL;
        rc = ib_tx_set_module_data(tx, m, ctx);
        assert (rc == IB_OK);
    }

    /* Select the req or resp fields according to event */
    switch (event) {
      case request_body_data_event:
        dir = IB_SERVER_REQUEST;
        rxbuf = &ctx->reqbuf;
        regexps = cfg->req_edits;
        break;
      case response_body_data_event:
        dir = IB_SERVER_RESPONSE;
        rxbuf = &ctx->respbuf;
        regexps = cfg->resp_edits;
        break;
      default:
        ib_log_error_tx(tx, "Bogus call to rxfilter");
        return IB_EINVAL;
    }

    if (rxbuf->errnum != IB_OK) {
        /* we had an error that we expect to repeat, so don't bother */
        return rxbuf->errnum;
    }
    if (regexps == NULL) {
        /* Nothing to do now, but keep byte count */
        rxbuf->offs += data_length;
        return IB_OK;
    }
    if (rxbuf->data == NULL) {
        /* first call.  No existing data */
        rc = ib_bytestr_dup_mem(&rxbuf->data, tx->mm, (const uint8_t*)data, data_length + 1);
    }
    else {
        /* merge with any buffered data and null-terminate */
        rc = ib_bytestr_append_mem(rxbuf->data, (const uint8_t*)data, data_length + 1);
    }
    if (rc != IB_OK) {
        rxbuf->errnum = rc;
        ib_log_error_tx(tx, "Error in rxfilter - aborting");
        return rc;
    }
    buf = (char*) ib_bytestr_ptr(rxbuf->data);
    len = ib_bytestr_length(rxbuf->data) - 1;
    buf[len] = '\0';

    /* apply regexps; keep high water mark */
    nelts = ib_array_elements(regexps);
    for (i = 0; i < nelts; ++i) {
        rc = ib_array_get(regexps, i, &rx);
        if (rc != IB_OK) {
            rxbuf->errnum = rc;
            ib_log_error_tx(tx, "Error in rxfilter - aborting");
            return rc;
        }
        p = buf;
        for (rv = regexec(&rx->rx, p, MAX_RX_MATCH, pmatch, 0);
             rv != REG_NOMATCH;
             rv = regexec(&rx->rx, p, MAX_RX_MATCH, pmatch, 0)) {
            if (rv != 0) {
                char ebuf[256];
                regerror(rv, &rx->rx, ebuf, 256);
                ib_log_error_tx(tx, "regexp error: %s", ebuf);
                break;
            }
            if (rx->rxtype == RX_DELETE) {
                repl = NULL;
                repl_len = 0;
            }
            else {
                repl = rx_repl(tx->mm, p, pmatch, rx->repl, &repl_len);
            }
            if (rx->rxtype == RX_AFTER) {
                start = rxbuf->offs + (p-buf) + pmatch[0].rm_eo;
            }
            else {
                start = rxbuf->offs + (p-buf) + pmatch[0].rm_so;
            }
            if (rx->rxtype == RX_AFTER || rx->rxtype == RX_BEFORE) {
                delbytes = 0;
            }
            else {
                delbytes = pmatch[0].rm_eo - pmatch[0].rm_so;
            }
            rc = svr->body_edit_fn(tx, dir, start, delbytes, repl, repl_len, NULL);
            if (rc != IB_OK) {
                /* FIXME - should probably be nonfatal.
                 * But we want to avoid huge reams of NOTIMPL
                 */
                rxbuf->errnum = rc;
                ib_log_error_tx(tx, "Edit error %d - aborting", rc);
                return rc;
            }
            p += pmatch[0].rm_eo;
            if ((size_t)(p - buf) > hwm) {
                hwm = p - buf;
            }
        }
    }

    /* Buffer any dangling data */
    /* Definition of dangling is the smallest of:
     *  1.  Data after the last regexp edit we just applied
     *  2.  Data after the last lineend (excludes matches that span lineends)
     *  3.  Max byte amount: FIXME - where to limit it?
     */
    p = strrchr(buf, '\n');
    if (p) {
        i = p - buf;
        if (i > hwm) {
            hwm = i;  /* or i+1? */
        }
    }
    if (len - hwm > MAX_BUFFER) {
        hwm = len - MAX_BUFFER;
    }
    /* Now forget hwm bytes, buffer the rest */
    rxbuf->offs += hwm;
    rc = ib_bytestr_setv(rxbuf->data, (uint8_t*)&buf[hwm], len - hwm);
    if (rc != IB_OK) {
        rxbuf->errnum = rc;
        ib_log_error_tx(tx, "Error in rxfilter - aborting");
        return rc;
    }

    return IB_OK;
}

/**
 * Initialisation: register our handler for Request and Response data events
 *
 * @param[in] ib - the engine
 * @param[in] m - the module
 * @return - Success
 */
static ib_status_t rxfilter_init(ib_engine_t *ib, ib_module_t *m, void *cbdata)
{
    ib_status_t rc;
    rc = ib_hook_txdata_register(ib, request_body_data_state, rxfilter, NULL);
    assert(rc == IB_OK);
    rc = ib_hook_txdata_register(ib, response_body_data_state, rxfilter, NULL);
    assert(rc == IB_OK);
    return rc;
}

/**
 * Parse a regexp op from ironbee.conf to internal rxop_t
 *
 * @param[in] cp - Configuration parser
 * @param[in] name - directive name
 * @param[in] param - directive value
 * @return success or parse or regcomp error
 */
static ib_status_t rxop_conf(ib_cfgparser_t *cp, const char *name,
                             const char *param, void *dummy)
{
    ib_module_t *m;
    rxfilter_cfg_t *cfg;
    ib_status_t rc;
    int rxflags;

    const char *startp;
    const char *endp;
    const char *rxstr;
    char sep;

    rxop_t *rxop = ib_mm_alloc(cp->mm, sizeof(rxop_t));

    /* parse regex expr */

    /* first char is op. */
    switch(param[0]) {
        case 'a': rxop->rxtype = RX_AFTER; break;
        case 'b': rxop->rxtype = RX_BEFORE; break;
        case 'd': rxop->rxtype = RX_DELETE; break;
        case 's': rxop->rxtype = RX_SUBS; break;
        default:
            ib_log_error(cp->ib, "Failed to parse %s as rx rule", param);
            return IB_EINVAL;
    }

    sep = param[1];
    if (!sep) {
        ib_log_error(cp->ib, "Failed to parse %s as rx rule", param);
        return IB_EINVAL;
    }
    startp = param+2;
    endp = strchr(startp, sep);
    if (!endp || startp == endp) {
        ib_log_error(cp->ib, "Failed to parse %s as rx rule", param);
        return IB_EINVAL;
    }
    rxstr = ib_mm_memdup_to_str(cp->mm, startp, endp-startp);

    /* Unless it's a DELETE, there's a replacement string next */
    if (rxop->rxtype == RX_DELETE) {
        rxop->repl = NULL;
    }
    else {
        startp = endp + 1;
        endp = strchr(startp, sep);
        if (!endp || startp == endp) {
            ib_log_error(cp->ib, "Failed to parse %s as rx rule", param);
            return IB_EINVAL;
        }
        rxop->repl = ib_mm_memdup_to_str(cp->mm, startp, endp-startp);
    }

    /* Flags after the last separator */
    rxflags = REG_EXTENDED;
    startp = endp + 1;
    if (strchr(startp, 'i')) {
        rxflags |= REG_ICASE;
    }

    if (regcomp (&rxop->rx, rxstr, rxflags) == 0) {
        rc = ib_mm_register_cleanup(cp->mm, (ib_mm_cleanup_fn_t)regfree, &rxop->rx);
        assert(rc == IB_OK);
    }
    else {
        ib_log_error(cp->ib, "Failed to compile '%s' as regexp", rxstr);
        return IB_EINVAL;
    }

    rc = ib_engine_module_get(cp->ib, MODULE_NAME_STR, &m);
    assert((rc == IB_OK) && (m != NULL));
    rc = ib_context_module_config(ib_context_main(cp->ib), m, &cfg);
    assert((rc == IB_OK) && (cfg != NULL));

    /* add it to {direction} list */
    /* FIXME - these asserts need to be replaced */
    if (!strcasecmp(name, "RxOpRequest")) {
        if (!cfg->req_edits) {
            rc = ib_array_create(&cfg->req_edits, cp->mm, 4, 4);
            assert((rc == IB_OK) && (cfg->req_edits != NULL));
        }
        rc = ib_array_appendn(cfg->req_edits, rxop);
    }
    else if (!strcasecmp(name, "RxOpResponse")) {
        if (!cfg->resp_edits) {
            rc = ib_array_create(&cfg->resp_edits, cp->mm, 4, 4);
            assert((rc == IB_OK) && (cfg->resp_edits != NULL));
        }
        rc = ib_array_appendn(cfg->resp_edits, rxop);
    }
    assert(rc == IB_OK);

    return IB_OK;
}

/* Declare directives to edit a Request or Response */
static IB_DIRMAP_INIT_STRUCTURE(rxfilter_config) = {
    IB_DIRMAP_INIT_PARAM1(
        "RxOpRequest",
        rxop_conf,
        NULL
    ),
    IB_DIRMAP_INIT_PARAM1(
        "RxOpResponse",
        rxop_conf,
        NULL
    ),
    IB_DIRMAP_INIT_LAST
};

static rxfilter_cfg_t rxfilter_cfg_ini = {
    NULL, NULL
};

IB_MODULE_INIT(
    IB_MODULE_HEADER_DEFAULTS,              /**< Default metadata */
    MODULE_NAME_STR,                        /**< Module name */
    IB_MODULE_CONFIG(&rxfilter_cfg_ini),    /**< Global config data */
    NULL,                                   /**< Configuration field map */
    rxfilter_config,                        /**< Config directive map */
    rxfilter_init, NULL,                    /**< Initialize function */
    NULL, NULL,                             /**< Finish function */
);
