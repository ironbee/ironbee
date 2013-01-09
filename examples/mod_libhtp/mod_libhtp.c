/***************************************************************************
 * Copyright (c) 2009-2010 Open Information Security Foundation
 * Copyright (c) 2010-2013 Qualys, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * - Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.

 * - Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.

 * - Neither the name of the Qualys, Inc. nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 ***************************************************************************/

#include "httpd.h"
#include "http_config.h"
#include "http_protocol.h"
#include "ap_config.h"

#include "htp.h"
#include "htp_transaction.h"

module AP_MODULE_DECLARE_DATA libhtp_module;

// XXX Handle all allocation failures
    
static int convert_method_number(int method_number) {
    // We can cheat here because LibHTP reuses Apache's
    // method identifiers. But we really shouldn't.
    if ((method_number >= 0)&&(method_number <= 26)) {
        return method_number;
    }
    
    // TODO Decouple this functions from Apache's internals.

    return HTP_M_UNKNOWN;
}

static int convert_protocol_number(int protocol_number) {
    // In Apache, 1.1 is stored as 1001. In LibHTP,
    // the same protocol number is stored as 101.
    return (protocol_number / 1000) * 100 + (protocol_number % 1000);
}

static apr_status_t transaction_cleanup(htp_tx_t *tx) {
    htp_tx_destroy(tx);
    return APR_SUCCESS;
}

static int libhtp_post_read_request(request_rec *r) {
    htp_connp_t *connp = ap_get_module_config(r->connection->conn_config, &libhtp_module);
    if (connp == NULL) return DECLINED;

    // Create a new LibHTP transaction
    htp_tx_t *tx = htp_connp_tx_create(connp);
    if (tx == NULL) return DECLINED;

    // Request begins
    htp_tx_state_request_start(tx);

    // Populate request line
    htp_tx_req_set_method_c(tx, r->method, HTP_ALLOC_REUSE);
    htp_tx_req_set_method_number(tx, convert_method_number(r->method_number));
    htp_tx_req_set_uri_c(tx, r->uri, HTP_ALLOC_REUSE);
    htp_tx_req_set_query_string_c(tx, r->args, HTP_ALLOC_REUSE);
    htp_tx_req_set_protocol_c(tx, r->protocol, HTP_ALLOC_REUSE);
    htp_tx_req_set_protocol_number(tx, convert_protocol_number(r->proto_num));
    htp_tx_req_set_protocol_0_9(tx, r->assbackwards);

    // Request line available
    htp_tx_state_request_line(tx);

    // Populate request headers
    size_t i;
    const apr_array_header_t *arr = apr_table_elts(r->headers_in);
    const apr_table_entry_t *te = (apr_table_entry_t *) arr->elts;
    for (i = 0; i < arr->nelts; i++) {
        htp_tx_req_set_header_c(tx, te[i].key, te[i].val, HTP_ALLOC_REUSE);
    }

    // Request headers available
    htp_tx_state_request_headers(tx);

    // Attach LibHTP's transaction to Apache's request
    ap_set_module_config(r->request_config, &libhtp_module, tx);
    apr_pool_cleanup_register(r->pool, (void *)tx,
            (apr_status_t (*)(void *))transaction_cleanup, apr_pool_cleanup_null);

    return DECLINED;
}

static apr_status_t connection_cleanup(htp_connp_t *connp) {
    htp_config_destroy(connp->cfg);
    htp_connp_destroy(connp);
    
    return APR_SUCCESS;
}

static int libhtp_pre_connection(conn_rec *c, void *csd) {
    // Configuration; normally you'd read the configuration from
    // a file, or some other type of storage, but, because this is
    // just an example, we have it hard-coded.
    htp_cfg_t *cfg = htp_config_create();
    if (cfg == NULL) return OK;
    htp_config_set_server_personality(cfg, HTP_SERVER_APACHE_2_2);
    htp_config_register_urlencoded_parser(cfg);
    htp_config_register_multipart_parser(cfg);

    // Connection parser
    htp_connp_t *connp = htp_connp_create(cfg);
    if (connp == NULL) {
        htp_config_destroy(cfg);
        free(connp);
        return OK;
    }

    // Open connection
    htp_connp_open(connp, c->remote_ip, /* XXX remote port */ 0, c->local_ip, /* XXX local port */0, NULL);

    ap_set_module_config(c->conn_config, &libhtp_module, connp);
    apr_pool_cleanup_register(c->pool, (void *)connp,
            (apr_status_t (*)(void *))connection_cleanup, apr_pool_cleanup_null);

    return OK;
}

static void libhtp_register_hooks(apr_pool_t *p) {
    ap_hook_pre_connection(libhtp_pre_connection, NULL, NULL, APR_HOOK_MIDDLE);
    ap_hook_post_read_request(libhtp_post_read_request, NULL, NULL, APR_HOOK_MIDDLE);
}

/* Dispatch list for API hooks */
module AP_MODULE_DECLARE_DATA libhtp_module = {
    STANDARD20_MODULE_STUFF,
    NULL, /* create per-dir    config structures */
    NULL, /* merge  per-dir    config structures */
    NULL, /* create per-server config structures */
    NULL, /* merge  per-server config structures */
    NULL, /* table of config file commands       */
    libhtp_register_hooks /* register hooks                      */
};

