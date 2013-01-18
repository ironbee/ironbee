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

/**
 * @file
 * @author Ivan Ristic <ivanr@webkreator.com>
 */

#include "htp_private.h"
#include "htp_transaction.h"

static bstr *copy_or_wrap_c(const char *input, enum htp_alloc_strategy_t alloc) {
    if (alloc == HTP_ALLOC_REUSE) {
        return bstr_wrap_c(input);
    } else {
        return bstr_dup_c(input);
    }
}

htp_tx_t *htp_tx_create(htp_connp_t *connp) {
    htp_tx_t *tx = calloc(1, sizeof (htp_tx_t));
    if (tx == NULL) return NULL;

    tx->connp = connp;
    tx->conn = connp->conn;
    tx->cfg = connp->cfg;
    tx->is_config_shared = HTP_CONFIG_SHARED;

    tx->request_protocol_number = HTP_PROTOCOL_UNKNOWN;
    tx->request_header_lines = htp_list_create(32);
    tx->request_headers = htp_table_create(32);
    tx->request_params = htp_table_create(32);
    tx->request_line_nul_offset = -1;

    tx->parsed_uri = calloc(1, sizeof (htp_uri_t));
    tx->parsed_uri->port_number = -1;
    tx->parsed_uri_incomplete = calloc(1, sizeof (htp_uri_t));

    tx->response_header_lines = htp_list_create(32);
    tx->response_headers = htp_table_create(32);
    
    return tx;
}

void htp_tx_destroy(htp_tx_t *tx) {
    bstr_free(&tx->request_line);
    bstr_free(&tx->request_line_raw);
    bstr_free(&tx->request_method);
    bstr_free(&tx->request_uri);
    bstr_free(&tx->request_uri_normalized);
    bstr_free(&tx->request_protocol);
    bstr_free(&tx->request_headers_sep);

    if (tx->parsed_uri != NULL) {
        bstr_free(&tx->parsed_uri->scheme);
        bstr_free(&tx->parsed_uri->username);
        bstr_free(&tx->parsed_uri->password);
        bstr_free(&tx->parsed_uri->hostname);
        bstr_free(&tx->parsed_uri->port);
        bstr_free(&tx->parsed_uri->path);
        bstr_free(&tx->parsed_uri->query);
        bstr_free(&tx->parsed_uri->fragment);

        free(tx->parsed_uri);
    }

    if (tx->parsed_uri_incomplete != NULL) {
        bstr_free(&tx->parsed_uri_incomplete->scheme);
        bstr_free(&tx->parsed_uri_incomplete->username);
        bstr_free(&tx->parsed_uri_incomplete->password);
        bstr_free(&tx->parsed_uri_incomplete->hostname);
        bstr_free(&tx->parsed_uri_incomplete->port);
        bstr_free(&tx->parsed_uri_incomplete->path);
        bstr_free(&tx->parsed_uri_incomplete->query);
        bstr_free(&tx->parsed_uri_incomplete->fragment);
        free(tx->parsed_uri_incomplete);
    }

    // Destroy request_header_lines
    if (tx->request_header_lines != NULL) {
        for (int i = 0, n = htp_list_size(tx->request_header_lines); i < n; i++) {
            htp_header_line_t *hl = htp_list_get(tx->request_header_lines, i);
            bstr_free(&hl->line);
            // No need to destroy hl->header because
            // htp_header_line_t does not own it.
            free(hl);
        }

        htp_list_destroy(&tx->request_header_lines);
    }

    // Destroy request_headers
    if (tx->request_headers != NULL) {
        htp_header_t *h = NULL;
        for (int i = 0, n = htp_table_size(tx->request_headers); i < n; i++) {
            htp_table_get_index(tx->request_headers, i, NULL, (void **) &h);
            bstr_free(&h->name);
            bstr_free(&h->value);
            free(h);
        }

        htp_table_destroy(&tx->request_headers);
    }

    if (tx->request_headers_raw != NULL) {
        bstr_free(&tx->request_headers_raw);
    }
    if (tx->response_headers_raw != NULL) {
        bstr_free(&tx->response_headers_raw);
    }

    bstr_free(&tx->response_line);
    bstr_free(&tx->response_line_raw);
    bstr_free(&tx->response_protocol);
    bstr_free(&tx->response_status);
    bstr_free(&tx->response_message);
    bstr_free(&tx->response_headers_sep);

    // Destroy response_header_lines
    if (tx->response_header_lines != NULL) {
        for (int i = 0, n = htp_list_size(tx->response_header_lines); i < n; i++) {
            htp_header_line_t *hl = htp_list_get(tx->response_header_lines, i);
            bstr_free(&hl->line);
            // No need to destroy hl->header because
            // htp_header_line_t does not own it.
            free(hl);
        }

        htp_list_destroy(&tx->response_header_lines);
    }

    // Destroy response headers
    if (tx->response_headers != NULL) {
        htp_header_t *h = NULL;
        for (int i = 0, n = htp_table_size(tx->response_headers); i < n; i++) {
            htp_table_get_index(tx->response_headers, i, NULL, (void **) &h);
            bstr_free(&h->name);
            bstr_free(&h->value);
            free(h);
        }

        htp_table_destroy(&tx->response_headers);
    }

    // Tell the connection to remove this transaction
    // from the list
    htp_conn_remove_tx(tx->conn, tx);

    // Invalidate the pointer to this transactions held
    // by the connection parser. This is to allow a transaction
    // to be destroyed from within the final response callback.
    if (tx->connp != NULL) {
        if (tx->connp->out_tx == tx) {
            tx->connp->out_tx = NULL;
        }
    }

    bstr_free(&tx->request_content_type);
    bstr_free(&tx->response_content_type);

    // Parsers

    htp_urlenp_destroy(&tx->request_urlenp_query);
    htp_urlenp_destroy(&tx->request_urlenp_body);
    htp_mpartp_destroy(&tx->request_mpartp);

    // Request parameters

    htp_param_t *param = NULL;
    for (int i = 0, n = htp_table_size(tx->request_params); i < n; i++) {
        htp_table_get_index(tx->request_params, i, NULL, (void **) &param);
        free(param->name);
        free(param->value);
        free(param);
    }

    htp_table_destroy(&tx->request_params);

    // Request cookies

    if (tx->request_cookies != NULL) {
        bstr *b = NULL;
        for (int i = 0, n = htp_table_size(tx->request_cookies); i < n; i++) {
            htp_table_get_index(tx->request_cookies, i, NULL, (void **) &b);
            bstr_free(&b);
        }

        htp_table_destroy(&tx->request_cookies);
    }

    htp_hook_destroy(tx->hook_request_body_data);

    // If we're using a private configuration, destroy it.
    if (tx->is_config_shared == HTP_CONFIG_PRIVATE) {
        htp_config_destroy(tx->cfg);
    }

    free(tx);
}

int htp_tx_get_is_config_shared(const htp_tx_t *tx) {
    return tx->is_config_shared;
}

void *htp_tx_get_user_data(const htp_tx_t *tx) {
    return tx->user_data;
}

void htp_tx_set_config(htp_tx_t *tx, htp_cfg_t *cfg, int is_cfg_shared) {
    if ((is_cfg_shared != HTP_CONFIG_PRIVATE) && (is_cfg_shared != HTP_CONFIG_SHARED)) return;

    // If we're using a private configuration, destroy it.
    if (tx->is_config_shared == HTP_CONFIG_PRIVATE) {
        htp_config_destroy(tx->cfg);
    }

    tx->cfg = cfg;
    tx->is_config_shared = is_cfg_shared;
}

void htp_tx_set_user_data(htp_tx_t *tx, void *user_data) {
    tx->user_data = user_data;
}

htp_status_t htp_tx_req_add_param(htp_tx_t *tx, htp_param_t *param) {
    if (tx->cfg->parameter_processor != NULL) {
        if (tx->cfg->parameter_processor(param) != HTP_OK) return HTP_ERROR;
    }

    return htp_table_addk(tx->request_params, param->name, param);
}

htp_param_t *htp_tx_req_get_param_c(htp_tx_t *tx, const char *name) {
    return (htp_param_t *)htp_table_get_c(tx->request_params, name);
}

htp_param_t *htp_tx_req_get_param_ex_c(htp_tx_t *tx, enum htp_data_source_t source, const char *name) {
    htp_param_t *p = NULL;

    for (int i = 0, n = htp_table_size(tx->request_params); i < n; i++) {
        htp_table_get_index(tx->request_params, i, NULL, (void **)&p);
        if (p->source != source) continue;

        if (bstr_cmp_c(p->name, name) == 0) return p;
    }

    return NULL;
}

int htp_tx_req_has_body(const htp_tx_t *tx) {
    if ((tx->request_transfer_coding == HTP_CODING_IDENTITY) || (tx->request_transfer_coding == HTP_CODING_CHUNKED)) {
        return 1;
    }

    return 0;
}

htp_status_t htp_tx_req_set_header_c(htp_tx_t *tx, const char *name, const char *value, enum htp_alloc_strategy_t alloc) {
    if ((name == NULL) || (value == NULL)) return HTP_ERROR;

    htp_header_t *h = calloc(1, sizeof (htp_header_t));
    if (h == NULL) {
        return HTP_ERROR;
    }

    h->name = copy_or_wrap_c(name, alloc);
    if (h->name == NULL) {
        free(h);
        return HTP_ERROR;
    }

    h->value = copy_or_wrap_c(value, alloc);
    if (h->value == NULL) {
        bstr_free(&h->name);
        free(h);
        return HTP_ERROR;
    }

    if (htp_table_add(tx->request_headers, h->name, h) != HTP_OK) {
        bstr_free(&h->name);
        bstr_free(&h->value);
        free(h);
        return HTP_ERROR;
    }

    return HTP_OK;
}

htp_status_t htp_tx_req_set_method_c(htp_tx_t *tx, const char *method, enum htp_alloc_strategy_t alloc) {
    if (method == NULL) return HTP_ERROR;
    tx->request_method = copy_or_wrap_c(method, alloc);
    if (tx->request_method == NULL) return HTP_ERROR;
    return HTP_OK;
}

void htp_tx_req_set_method_number(htp_tx_t *tx, enum htp_method_t method_number) {
    tx->request_method_number = method_number;
}

htp_status_t htp_tx_req_set_uri_c(htp_tx_t *tx, const char *uri, enum htp_alloc_strategy_t alloc) {
    if (uri == NULL) return HTP_ERROR;
    tx->request_uri = copy_or_wrap_c(uri, alloc);
    if (tx->request_uri == NULL) return HTP_ERROR;
    return HTP_OK;
}

htp_status_t htp_tx_req_set_query_string_c(htp_tx_t *tx, const char *query_string, enum htp_alloc_strategy_t alloc) {
    if (tx->parsed_uri == NULL) return HTP_ERROR;
    tx->parsed_uri->query = copy_or_wrap_c(query_string, alloc);
    if (tx->parsed_uri->query == NULL) return HTP_ERROR;
    return HTP_OK;
}

htp_status_t htp_tx_req_set_protocol_c(htp_tx_t *tx, const char *protocol, enum htp_alloc_strategy_t alloc) {
    if (protocol == NULL) return HTP_ERROR;
    tx->request_protocol = copy_or_wrap_c(protocol, alloc);
    if (tx->request_protocol == NULL) return HTP_ERROR;
    return HTP_OK;
}

void htp_tx_req_set_protocol_number(htp_tx_t *tx, int protocol_number) {
    tx->request_protocol_number = protocol_number;
}

void htp_tx_req_set_protocol_0_9(htp_tx_t *tx, int is_protocol_0_9) {
    if (is_protocol_0_9) {
        tx->is_protocol_0_9 = 1;
    } else {
        tx->is_protocol_0_9 = 0;
    }
}

static htp_status_t htp_tx_process_request_headers(htp_tx_t *tx) {   
    // Remember how many header lines there were before trailers
    tx->request_header_lines_no_trailers = htp_list_size(tx->request_header_lines);

    // Determine if we have a request body, and how it is packaged
    htp_header_t *cl = htp_table_get_c(tx->request_headers, "content-length");
    htp_header_t *te = htp_table_get_c(tx->request_headers, "transfer-encoding");

    // Check for the Transfer-Encoding header, which would indicate a chunked request body
    if (te != NULL) {
        // Make sure it contains "chunked" only
        if (bstr_cmp_c(te->value, "chunked") != 0) {
            // Invalid T-E header value
            htp_log(tx->connp, HTP_LOG_MARK, HTP_LOG_ERROR, 0,
                    "Invalid T-E value in request");

            // TODO Should this be a fatal error?
        }

        // Chunked encoding is a HTTP/1.1 feature. Check that some other protocol is not
        // used. The flag will also be set if the protocol could not be parsed.
        //
        // TODO IIS 7.0, for example, would ignore the T-E header when it
        //      it is used with a protocol below HTTP 1.1.
        if (tx->request_protocol_number < HTP_PROTOCOL_1_1) {
            tx->flags |= HTP_INVALID_CHUNKING;
            // TODO Log
        }

        // If the T-E header is present we are going to use it.
        tx->request_transfer_coding = HTP_CODING_CHUNKED;

        // We are still going to check for the presence of C-L
        if (cl != NULL) {
            // This is a violation of the RFC
            tx->flags |= HTP_REQUEST_SMUGGLING;
            // TODO Log
        }
    } else if (cl != NULL) {
        // We have a request body of known length
        tx->request_transfer_coding = HTP_CODING_IDENTITY;

        // Check for a folded C-L header
        if (cl->flags & HTP_FIELD_FOLDED) {
            tx->flags |= HTP_REQUEST_SMUGGLING;
            // TODO Log
        }

        // Check for multiple C-L headers
        if (cl->flags & HTP_FIELD_REPEATED) {
            tx->flags |= HTP_REQUEST_SMUGGLING;
            // TODO Log
        }

        // Get body length
        int i = htp_parse_content_length(cl->value);
        if (i < 0) {
            htp_log(tx->connp, HTP_LOG_MARK, HTP_LOG_ERROR, 0, "Invalid C-L field in request");
            return HTP_ERROR;
        }

        tx->request_content_length = i;
    } else {
        // No body
        tx->request_transfer_coding = HTP_CODING_NO_BODY;
    }

    // Check for PUT requests, which we need to treat as file uploads
    if (tx->request_method_number == HTP_M_PUT) {
        if (htp_tx_req_has_body(tx)) {
            // Prepare to treat PUT request body as a file
            tx->connp->put_file = calloc(1, sizeof (htp_file_t));
            if (tx->connp->put_file == NULL) return HTP_ERROR;
            tx->connp->put_file->source = HTP_FILE_PUT;
        } else {
            // TODO Warn about PUT request without a body
        }

        return HTP_OK;
    }

    // Host resolution
    htp_header_t *h = htp_table_get_c(tx->request_headers, "host");
    if (h == NULL) {
        // No host information in the headers

        // HTTP/1.1 requires host information in the headers
        if (tx->request_protocol_number >= HTP_PROTOCOL_1_1) {
            tx->flags |= HTP_HOST_MISSING;
            htp_log(tx->connp, HTP_LOG_MARK, HTP_LOG_WARNING, 0,
                    "Host information in request headers required by HTTP/1.1");
        }
    } else {
        // Host information available in the headers

        bstr *hostname;
        int port;
        int flags;              

        if (htp_parse_authority(h->value, &hostname, &port, &flags) != HTP_OK) return HTP_ERROR;

        // Is there host information in the URI?
        if (tx->parsed_uri->hostname == NULL) {
            // There is no host information in the URI. Place the
            // hostname from the headers into the parsed_uri structure.
            tx->parsed_uri->hostname = hostname;
            tx->parsed_uri->port_number = port;
        } else {            
            if ((bstr_cmp_nocase(hostname, tx->parsed_uri->hostname) != 0) || (port != tx->parsed_uri->port_number)) {
                // The host information is different in the
                // headers and the URI. The HTTP RFC states that
                // we should ignore the header copy.
                tx->flags |= HTP_AMBIGUOUS_HOST;
                htp_log(tx->connp, HTP_LOG_MARK, HTP_LOG_WARNING, 0, "Host information ambiguous");
            }
            
            bstr_free(&hostname);
        }
    }

    // Parse Content-Type
    htp_header_t *ct = htp_table_get_c(tx->request_headers, "content-type");
    if (ct != NULL) {
        tx->request_content_type = bstr_dup_lower(ct->value);
        if (tx->request_content_type == NULL) return HTP_ERROR;

        // Ignore parameters
        unsigned char *data = bstr_ptr(tx->request_content_type);
        size_t len = bstr_len(ct->value);
        size_t newlen = 0;
        while (newlen < len) {
            // TODO Some platforms may do things differently here
            if (htp_is_space(data[newlen]) || (data[newlen] == ';')) {
                bstr_adjust_len(tx->request_content_type, newlen);
                break;
            }

            newlen++;
        }
    }

    // Parse cookies
    if (tx->connp->cfg->parse_request_cookies) {
        htp_parse_cookies_v0(tx->connp);
    }

    // Parse authentication information
    if (tx->connp->cfg->parse_request_http_authentication) {
        htp_parse_authorization(tx->connp);
    }

    // Run hook REQUEST_HEADERS
    int rc = htp_hook_run_all(tx->connp->cfg->hook_request_headers, tx->connp);
    if (rc != HTP_OK) return rc;

    return HTP_OK;
}

htp_status_t htp_tx_req_process_body_data(htp_tx_t *tx, const unsigned char *data, size_t len) {
    // Keep track of actual data length
    tx->request_entity_len += len;

    // Send data to callbacks
    htp_tx_data_t d;
    d.tx = tx;
    d.data = (unsigned char *) data;
    d.len = len;

    int rc = htp_req_run_hook_body_data(tx->connp, &d);
    if (rc != HTP_OK) {
        htp_log(tx->connp, HTP_LOG_MARK, HTP_LOG_ERROR, 0,
                "Request body data callback returned error (%d)", rc);
        return HTP_ERROR;
    }

    return HTP_OK;
}

htp_status_t htp_tx_req_set_headers_clear(htp_tx_t *tx) {
    if (tx->request_headers == NULL) return HTP_ERROR;

    htp_header_t *h = NULL;
    for (int i = 0, n = htp_table_size(tx->request_headers); i < n; i++) {
        htp_table_get_index(tx->request_headers, i, NULL, (void **) &h);
        bstr_free(&h->name);
        bstr_free(&h->value);
        free(h);
    }

    htp_table_destroy(&tx->request_headers);

    tx->request_headers = htp_table_create(32);
    if (tx->request_headers == NULL) return HTP_ERROR;

    return HTP_OK;
}

htp_status_t htp_tx_res_set_status_line_c(htp_tx_t *tx, const char *line, enum htp_alloc_strategy_t alloc) {
    if (line == NULL) return HTP_ERROR;
    tx->response_line = copy_or_wrap_c(line, alloc);
    if (tx->response_line == NULL) return HTP_ERROR;

    // Parse response line
    if (tx->connp->cfg->parse_response_line(tx->connp) != HTP_OK) return HTP_ERROR;

    return HTP_OK;
}

void htp_tx_res_set_protocol_number(htp_tx_t *tx, int protocol_number) {
    tx->response_protocol_number = protocol_number;
}

void htp_tx_res_set_status_code(htp_tx_t *tx, int status_code) {
    tx->response_status_number = status_code;
}

htp_status_t htp_tx_res_set_status_message(htp_tx_t *tx, const char *message, enum htp_alloc_strategy_t alloc) {
    if (message == NULL) return HTP_ERROR;

    if (tx->response_message != NULL) {
        bstr_free(&tx->response_message);
    }

    tx->response_message = copy_or_wrap_c(message, alloc);
    if (tx->response_message == NULL) return HTP_ERROR;
    return HTP_OK;
}

htp_status_t htp_tx_state_response_line(htp_tx_t *tx) {
    #if 0
    // Commented-out until we determine which fields can be
    // unavailable in real-life

    // Unless we're dealing with HTTP/0.9, check that
    // the minimum amount of data has been provided.
    if (tx->is_protocol_0_9 != 0) {
        if ((tx->response_protocol == NULL) || (tx->response_status_number == -1) || (tx->response_message == NULL)) {
            return HTP_ERROR;
        }
    }
    #endif

    // Is the response line valid?
    if ((tx->response_protocol_number < 0)
            || (tx->response_status_number < 0)
            || (tx->response_status_number < HTP_VALID_STATUS_MIN)
            || (tx->response_status_number > HTP_VALID_STATUS_MAX)) {
        // TODO This should be STATUS_CODE_INVALID
        // Response line is invalid
        htp_log(tx->connp, HTP_LOG_MARK, HTP_LOG_WARNING, 0, "Invalid response line");

        tx->flags |= HTP_STATUS_LINE_INVALID;
    }

    // Run hook HTP_RESPONSE_LINE
    int rc = htp_hook_run_all(tx->connp->cfg->hook_response_line, tx->connp);
    if (rc != HTP_OK) return rc;

    return HTP_OK;
}

htp_status_t htp_tx_res_set_header_c(htp_tx_t *tx, const char *name, const char *value, enum htp_alloc_strategy_t alloc) {
    if ((name == NULL) || (value == NULL)) return HTP_ERROR;
    ;

    htp_header_t *h = calloc(1, sizeof (htp_header_t));
    if (h == NULL) return HTP_ERROR;

    h->name = copy_or_wrap_c(name, alloc);
    if (h->name == NULL) {
        free(h);
        return HTP_ERROR;
    }

    h->value = copy_or_wrap_c(value, alloc);
    if (h->value == NULL) {
        bstr_free(&h->name);
        free(h);
        return HTP_ERROR;
    }

    if (htp_table_add(tx->response_headers, h->name, h) != HTP_OK) {
        bstr_free(&h->name);
        bstr_free(&h->value);
        free(h);
        return HTP_ERROR;
    }

    return HTP_OK;
}

htp_status_t htp_tx_res_set_headers_clear(htp_tx_t *tx) {
    if (tx->response_headers == NULL) return HTP_ERROR;

    htp_header_t *h = NULL;
    for (int i = 0, n = htp_table_size(tx->response_headers); i < n; i++) {
        htp_table_get_index(tx->response_headers, i, NULL, (void **) &h);
        bstr_free(&h->name);
        bstr_free(&h->value);
        free(h);
    }

    htp_table_destroy(&tx->response_headers);

    tx->response_headers = htp_table_create(32);
    if (tx->response_headers == NULL) return HTP_ERROR;

    return HTP_OK;
}

static htp_status_t htp_tx_res_process_body_data_decompressor_callback(htp_tx_data_t *d) {
    #if HTP_DEBUG
    fprint_raw_data(stderr, __FUNCTION__, d->data, d->len);
    #endif

    // Keep track of actual response body length
    d->tx->response_entity_len += d->len;

    // Invoke all callbacks
    int rc = htp_res_run_hook_body_data(d->tx->connp, d);
    if (rc != HTP_OK) return HTP_ERROR;

    return HTP_OK;
}

htp_status_t htp_tx_res_process_body_data(htp_tx_t *tx, const unsigned char *data, size_t len) {
    htp_tx_data_t d;

    d.tx = tx;
    d.data = data;
    d.len = len;

    // Keep track of body size before decompression
    tx->response_message_len += d.len;

    if (tx->response_content_encoding != COMPRESSION_NONE) {
        // Send data buffer to the decompressor
        tx->connp->out_decompressor->decompress(tx->connp->out_decompressor, &d);

        if (data == NULL) {
            // Shut down the decompressor, if we used one
            tx->connp->out_decompressor->destroy(tx->connp->out_decompressor);
            tx->connp->out_decompressor = NULL;
        }
    } else {
        // When there's no decompression, response_entity_len
        // is identical to response_message_len.
        tx->response_entity_len += d.len;

        int rc = htp_res_run_hook_body_data(tx->connp, &d);
        if (rc != HTP_OK) return HTP_ERROR;
    }

    return HTP_OK;
}

htp_status_t htp_tx_state_request_complete(htp_tx_t *tx) {
    // Finalize request body
    if (htp_tx_req_has_body(tx)) {
        int rc = htp_tx_req_process_body_data(tx, NULL, 0);
        if (rc != HTP_OK) return rc;
    }

    // Run hook REQUEST
    int rc = htp_hook_run_all(tx->connp->cfg->hook_request_complete, tx->connp);
    if (rc != HTP_OK) return rc;

    // Clean-up
    if (tx->connp->put_file != NULL) {
        bstr_free(&tx->connp->put_file->filename);
        free(tx->connp->put_file);
        tx->connp->put_file = NULL;
    }

    // Update the transaction status, but only if it did already
    // move on. This may happen when we're processing a CONNECT
    // request and need to wait for the response to determine how
    // to continue to treat the rest of the TCP stream.
    if (tx->progress < HTP_REQUEST_COMPLETE) {
        tx->progress = HTP_REQUEST_COMPLETE;
    }

    return HTP_OK;
}

htp_status_t htp_tx_state_request_start(htp_tx_t *tx) {
    // Run hook TRANSACTION_START
    int rc = htp_hook_run_all(tx->connp->cfg->hook_request_start, tx->connp);
    if (rc != HTP_OK) return rc;

    // Change state into request line parsing
    tx->connp->in_state = htp_connp_REQ_LINE;
    tx->connp->in_tx->progress = HTP_REQUEST_LINE;

    return HTP_OK;
}

htp_status_t htp_tx_state_request_headers(htp_tx_t *tx) {        
    // Did this request arrive in multiple chunks?
    if (tx->connp->in_chunk_count != tx->connp->in_chunk_request_index) {
        tx->flags |= HTP_MULTI_PACKET_HEAD;
    }

    // If we're in TX_PROGRESS_REQ_HEADERS that means that this is the
    // first time we're processing headers in/ a request. Otherwise,
    // we're dealing with trailing headers.
    if (tx->progress > HTP_REQUEST_HEADERS) {
        // Run hook HTP_REQUEST_TRAILER
        int rc = htp_hook_run_all(tx->connp->cfg->hook_request_trailer, tx->connp);
        if (rc != HTP_OK) return rc;

        // Completed parsing this request; finalize it now
        tx->connp->in_state = htp_connp_REQ_FINALIZE;
    } else if (tx->progress >= HTP_REQUEST_LINE) {
        // Process request headers
        int rc = htp_tx_process_request_headers(tx);
        if (rc != HTP_OK) return rc;

        tx->connp->in_state = htp_connp_REQ_CONNECT_CHECK;
    } else {
        htp_log(tx->connp, HTP_LOG_MARK, HTP_LOG_WARNING, 0, "[Internal Error] Invalid tx progress: %d", tx->progress);

        return HTP_ERROR;
    }

    return HTP_OK;
}

htp_status_t htp_tx_state_request_line(htp_tx_t *tx) {        
    htp_connp_t *connp = tx->connp;

    if (connp->in_tx->request_method_number == HTP_M_CONNECT) {
        // Parse authority
        if (htp_parse_uri_authority(connp, connp->in_tx->request_uri, &(connp->in_tx->parsed_uri_incomplete)) != HTP_OK) {
            // Note: downstream responsible for error logging
            return HTP_ERROR;
        }
    } else {
        // Parse the request URI
        if (htp_parse_uri(connp->in_tx->request_uri, &(connp->in_tx->parsed_uri_incomplete)) != HTP_OK) {
            // Note: downstream responsible for error logging
            return HTP_ERROR;
        }

        // Keep the original URI components, but
        // create a copy which we can normalize and use internally
        if (htp_normalize_parsed_uri(connp, connp->in_tx->parsed_uri_incomplete, connp->in_tx->parsed_uri) != HTP_OK) {
            // Note: downstream responsible for error logging
            return HTP_ERROR;
        }
        
        // Run hook REQUEST_URI_NORMALIZE
        int rc = htp_hook_run_all(connp->cfg->hook_request_uri_normalize, connp);
        if (rc != HTP_OK) return rc;

        // Now is a good time to generate request_uri_normalized, before we finalize
        // parsed_uri (and lose the information which parts were provided in the request and
        // which parts we added).
        if (connp->cfg->generate_request_uri_normalized) {
            connp->in_tx->request_uri_normalized = htp_unparse_uri_noencode(connp->in_tx->parsed_uri);

            if (connp->in_tx->request_uri_normalized == NULL) {
                // There's no sense in logging anything on a memory allocation failure
                return HTP_ERROR;
            }

            #ifdef HTP_DEBUG
            fprint_raw_data(stderr, "request_uri_normalized",
                    (unsigned char *) bstr_ptr(connp->in_tx->request_uri_normalized),
                    bstr_len(connp->in_tx->request_uri_normalized));
            #endif
        }

        // Finalize parsed_uri

        // Scheme
        if (connp->in_tx->parsed_uri->scheme != NULL) {
            if (bstr_cmp_c(connp->in_tx->parsed_uri->scheme, "http") != 0) {
                // TODO Invalid scheme
            }
        } else {
            connp->in_tx->parsed_uri->scheme = bstr_dup_c("http");
            if (connp->in_tx->parsed_uri->scheme == NULL) {
                return HTP_ERROR;
            }
        }

        // Path
        if (connp->in_tx->parsed_uri->path == NULL) {
            connp->in_tx->parsed_uri->path = bstr_dup_c("/");
            if (connp->in_tx->parsed_uri->path == NULL) {
                return HTP_ERROR;
            }
        }
    }

    // Run hook REQUEST_LINE
    int rc = htp_hook_run_all(connp->cfg->hook_request_line, connp);
    if (rc != HTP_OK) return rc;

    // Move on to the next phase.
    connp->in_state = htp_connp_REQ_PROTOCOL;

    return HTP_OK;
}

htp_status_t htp_tx_state_response_complete(htp_tx_t *tx) {
    if (tx->connp->out_tx->progress != HTP_RESPONSE_COMPLETE) {
        tx->progress = HTP_RESPONSE_COMPLETE;

        // Run the last RESPONSE_BODY_DATA HOOK, but
        // only if there was a response body present.
        // TODO Use constant instead of -1
        if (tx->response_transfer_coding != HTP_CODING_NO_BODY) {
            htp_tx_res_process_body_data(tx, NULL, 0);
        }

        // Run hook RESPONSE
        return htp_hook_run_all(tx->connp->cfg->hook_response_complete, tx->connp);
    }

    return HTP_OK;
}

htp_status_t htp_tx_state_response_headers(htp_tx_t *tx) {
    // Check for compression
    if (tx->connp->cfg->response_decompression_enabled) {
        htp_header_t *ce = htp_table_get_c(tx->response_headers, "content-encoding");
        if (ce != NULL) {
            if ((bstr_cmp_c(ce->value, "gzip") == 0) || (bstr_cmp_c(ce->value, "x-gzip") == 0)) {
                tx->response_content_encoding = COMPRESSION_GZIP;
            } else if ((bstr_cmp_c(ce->value, "deflate") == 0) || (bstr_cmp_c(ce->value, "x-deflate") == 0)) {
                tx->response_content_encoding = COMPRESSION_DEFLATE;
            }
        }
    }

    // Run hook RESPONSE_HEADERS_COMPLETE
    int rc = htp_hook_run_all(tx->connp->cfg->hook_response_headers, tx->connp);
    if (rc != HTP_OK) return rc;

    // Initialize the decompression engine as necessary. We can deal with three
    // scenarios:
    //
    // 1. Decompression is enabled, compression indicated in headers, and we decompress.
    //
    // 2. As above, but the user disables decompression by setting response_content_encoding
    //    to COMPRESSION_NONE.
    //
    // 3. Decompression is disabled and we do not attempt to enable it, but the user
    //    forces decompression by setting response_content_encoding to one of the
    //    supported algorithms.
    if ((tx->response_content_encoding == COMPRESSION_GZIP) || (tx->response_content_encoding == COMPRESSION_DEFLATE)) {
        if (tx->connp->out_decompressor != NULL) {
            tx->connp->out_decompressor->destroy(tx->connp->out_decompressor);
            tx->connp->out_decompressor = NULL;
        }

        tx->connp->out_decompressor = (htp_decompressor_t *) htp_gzip_decompressor_create(tx->connp,
                tx->response_content_encoding);
        if (tx->connp->out_decompressor == NULL) return HTP_ERROR;
        tx->connp->out_decompressor->callback = htp_tx_res_process_body_data_decompressor_callback;
    } else if (tx->response_content_encoding != COMPRESSION_NONE) {
        // TODO Error message
        return HTP_ERROR;
    }

    return HTP_OK;
}

htp_status_t htp_tx_state_response_start(htp_tx_t *tx) {
    tx->connp->out_tx = tx;

    // Run hook RESPONSE_START
    int rc = htp_hook_run_all(tx->connp->cfg->hook_response_start, tx->connp);
    if (rc != HTP_OK) return rc;

    // Change state into response line parsing, except if we're following
    // a HTTP/0.9 request (no status line or response headers).
    if (tx->is_protocol_0_9) {
        tx->response_transfer_coding = HTP_CODING_IDENTITY;
        tx->progress = HTP_RESPONSE_BODY;
        tx->connp->out_state = htp_connp_RES_BODY_IDENTITY;
    } else {
        tx->connp->out_state = htp_connp_RES_LINE;
        tx->progress = HTP_RESPONSE_LINE;
    }

    return HTP_OK;
}

/**
 * Register callback for the transaction-specific REQUEST_BODY_DATA hook.
 *
 * @param[in] tx
 * @pram callback_fn
 */
void htp_tx_register_request_body_data(htp_tx_t *tx, int (*callback_fn)(htp_tx_data_t *)) {
    htp_hook_register(&tx->hook_request_body_data, (htp_callback_fn_t) callback_fn);
}

/**
 * Register callback for the transaction-specific RESPONSE_BODY_DATA hook.
 *
 * @param[in] tx
 * @pram callback_fn
 */
void htp_tx_register_response_body_data(htp_tx_t *tx, int (*callback_fn)(htp_tx_data_t *)) {
    htp_hook_register(&tx->hook_response_body_data, (htp_callback_fn_t) callback_fn);
}
