/***************************************************************************
 * Copyright (c) 2009-2010, Open Information Security Foundation
 * Copyright (c) 2009-2012, Qualys, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * * Neither the name of the Qualys, Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
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

#include <stdio.h>
#include <stdlib.h>

#include "htp.h"
#include "htp_hybrid.h"
#include "htp_private.h"

static bstr *copy_or_wrap_c(const char *input, enum alloc_strategy alloc) {
    if (alloc == ALLOC_REUSE) {
        return bstr_wrap_c(input);
    } else {
        return bstr_dup_c(input);
    }
}

htp_tx_t *htp_txh_create(htp_connp_t *connp) {
    // Detect pipelining
    if (list_size(connp->conn->transactions) > connp->out_next_tx_index) {
        connp->conn->flags |= HTP_PIPELINED_CONNECTION;
    }

    htp_tx_t *tx = htp_tx_create(connp->cfg, CFG_SHARED, connp->conn);
    if (tx == NULL) return NULL;

    tx->connp = connp;
    tx->connp->in_tx = tx;

    list_add(connp->conn->transactions, tx);

    htp_connp_in_reset(connp);

    return tx;
}

void htp_txh_req_set_header_c(htp_tx_t *tx, const char *name, const char *value, enum alloc_strategy alloc) {
    if ((name == NULL) || (value == NULL)) return;

    htp_header_t *h = calloc(1, sizeof (htp_header_t));
    if (h == NULL) {
        return;
    }

    h->name = copy_or_wrap_c(name, alloc);
    if (h->name == NULL) {
        free(h);
        return;
    }

    h->value = copy_or_wrap_c(value, alloc);
    if (h->value == NULL) {
        bstr_free(&h->name);
        free(h);
        return;
    }

    table_add(tx->request_headers, h->name, h);
}

void htp_txh_req_set_method_c(htp_tx_t *tx, const char *method, enum alloc_strategy alloc) {
    tx->request_method = copy_or_wrap_c(method, alloc);
}

void htp_txh_req_set_method_number(htp_tx_t *tx, int method_number) {
    tx->request_method_number = method_number;
}

void htp_txh_req_set_uri_c(htp_tx_t *tx, const char *uri, enum alloc_strategy alloc) {
    tx->request_uri = copy_or_wrap_c(uri, alloc);
    // XXX Create parsed_uri
}

void htp_txh_req_set_query_string_c(htp_tx_t *tx, const char *query_string, enum alloc_strategy alloc) {
    if (tx->parsed_uri == NULL) return;

    tx->parsed_uri->query = copy_or_wrap_c(query_string, alloc);
}

void htp_txh_req_set_protocol_c(htp_tx_t *tx, const char *protocol, enum alloc_strategy alloc) {
    tx->request_protocol = copy_or_wrap_c(protocol, alloc);
}

void htp_txh_req_set_protocol_number(htp_tx_t *tx, int protocol_number) {
    // TODO Check that the number is valid
    tx->request_protocol_number = protocol_number;
}

void htp_txh_req_set_protocol_http_0_9(htp_tx_t *tx, int is_http_0_9) {
    if (is_http_0_9) {
        tx->protocol_is_simple = 1;
    } else {
        tx->protocol_is_simple = 0;
    }
}

int htp_txh_state_request_complete(htp_tx_t *tx) {
    // Finalize request body
    if (htp_tx_req_has_body(tx)) {
        int rc = htp_txh_req_process_body_data(tx, NULL, 0);
        if (rc != HTP_OK) return rc;
    }

    // Run hook REQUEST
    int rc = hook_run_all(tx->connp->cfg->hook_request_done, tx->connp);
    if (rc != HOOK_OK) return rc;

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
    if (tx->progress < TX_PROGRESS_WAIT) {
        tx->progress = TX_PROGRESS_WAIT;
    }   

    return HTP_OK;
}

int htp_txh_state_request_line(htp_tx_t *tx) {
    htp_connp_t *connp = tx->connp;

    if (connp->in_tx->request_method_number == HTP_M_CONNECT) {
        // Parse authority
        if (htp_parse_authority(connp, connp->in_tx->request_uri, &(connp->in_tx->parsed_uri_incomplete)) != HTP_OK) {
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
        int rc = hook_run_all(connp->cfg->hook_request_uri_normalize, connp);
        if (rc != HOOK_OK) return rc;

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

        // Port
        if (connp->in_tx->parsed_uri->port != NULL) {
            if (connp->in_tx->parsed_uri->port_number != -1) {
                // Check that the port in the URI is the same
                // as the port on which the client is talking
                // to the server
                if (connp->cfg->use_local_port) {
                    if (connp->in_tx->parsed_uri->port_number != connp->conn->local_port) {
                        // Incorrect port; use the real port instead
                        connp->in_tx->parsed_uri->port_number = connp->conn->local_port;
                        // TODO Log
                    }
                } else {
                    connp->in_tx->parsed_uri->port_number = connp->conn->remote_port;
                }
            } else {
                // Invalid port; use the real port instead
                if (connp->cfg->use_local_port) {
                    connp->in_tx->parsed_uri->port_number = connp->conn->local_port;
                } else {
                    connp->in_tx->parsed_uri->port_number = connp->conn->remote_port;
                }

                // TODO Log
            }
        } else {
            if (connp->cfg->use_local_port) {
                connp->in_tx->parsed_uri->port_number = connp->conn->local_port;
            } else {
                connp->in_tx->parsed_uri->port_number = connp->conn->remote_port;
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
    int rc = hook_run_all(connp->cfg->hook_request_line, connp);
    if (rc != HOOK_OK) return rc;

    // Move on to the next phase.
    connp->in_state = htp_connp_REQ_PROTOCOL;

    return HTP_OK;
}

static int htp_txh_process_request_headers(htp_tx_t *tx) {
    // Remember how many header lines there were before trailers
    tx->request_header_lines_no_trailers = list_size(tx->request_header_lines);

    // Determine if we have a request body, and how it is packaged
    htp_header_t *cl = table_get_c(tx->request_headers, "content-length");
    htp_header_t *te = table_get_c(tx->request_headers, "transfer-encoding");

    // Check for the Transfer-Encoding header, which would indicate a chunked request body
    if (te != NULL) {
        // Make sure it contains "chunked" only
        if (bstr_cmp_c(te->value, "chunked") != 0) {
            // Invalid T-E header value
            htp_log(tx->connp, HTP_LOG_MARK, HTP_LOG_ERROR, 0,
                    "Invalid T-E value in request");
        }

        // Chunked encoding is a HTTP/1.1 feature. Check that some other protocol is not
        // used. The flag will also be set if the protocol could not be parsed.
        //
        // TODO IIS 7.0, for example, would ignore the T-E header when it
        //      it is used with a protocol below HTTP 1.1.
        if (tx->request_protocol_number < HTTP_1_1) {
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
        // It seems that we have a request body of a known length
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
    htp_header_t *h = table_get_c(tx->request_headers, "host");
    if (h == NULL) {
        // No host information in the headers

        // HTTP/1.1 requires host information in the headers
        if (tx->request_protocol_number >= HTTP_1_1) {
            tx->flags |= HTP_HOST_MISSING;
            htp_log(tx->connp, HTP_LOG_MARK, HTP_LOG_WARNING, 0,
                    "Host information in request headers required by HTTP/1.1");
        }
    } else {
        // Host information available in the headers

        // Is there host information in the URI?
        if (tx->parsed_uri->hostname == NULL) {
            // There is no host information in the URI. Place the
            // hostname from the headers into the parsed_uri structure.
            htp_replace_hostname(tx->connp, tx->parsed_uri, h->value);
        } else if (bstr_cmp_nocase(h->value, tx->parsed_uri->hostname) != 0) {
            // The host information is different in the
            // headers and the URI. The HTTP RFC states that
            // we should ignore the headers copy.
            tx->flags |= HTP_AMBIGUOUS_HOST;
            htp_log(tx->connp, HTP_LOG_MARK, HTP_LOG_WARNING, 0, "Host information ambiguous");
        }
    }

    // Parse Content-Type
    htp_header_t *ct = table_get_c(tx->request_headers, "content-type");
    if (ct != NULL) {
        tx->request_content_type = bstr_dup_lower(ct->value);
        if (tx->request_content_type == NULL) return HTP_ERROR;

        // Ignore parameters
        char *data = bstr_ptr(tx->request_content_type);
        size_t len = bstr_len(ct->value);
        size_t newlen = 0;
        while (newlen < len) {
            // TODO Some platforms may do things differently here
            if (htp_is_space(data[newlen]) || (data[newlen] == ';')) {
                bstr_util_adjust_len(tx->request_content_type, newlen);
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
    int rc = hook_run_all(tx->connp->cfg->hook_request_headers, tx->connp);
    if (rc != HOOK_OK) return rc;

    return HTP_OK;
}

int htp_txh_state_request_headers(htp_tx_t *tx) {
    // Did this request arrive in multiple chunks?
    if (tx->connp->in_chunk_count != tx->connp->in_chunk_request_index) {
        tx->flags |= HTP_MULTI_PACKET_HEAD;
    }

    // If we're in TX_PROGRESS_REQ_HEADERS that means that this is the
    // first time we're processing headers in/ a request. Otherwise,
    // we're dealing with trailing headers.
    if (tx->progress > TX_PROGRESS_REQ_HEADERS) {
        // Run hook REQUEST_TRAILER
        int rc = hook_run_all(tx->connp->cfg->hook_request_trailer, tx->connp);
        if (rc != HOOK_OK) return rc;

        // Completed parsing this request; finalize it now
        tx->connp->in_state = htp_connp_REQ_FINALIZE;
    } else if (tx->progress >= TX_PROGRESS_REQ_LINE) {
        // Process request headers
        int rc = htp_txh_process_request_headers(tx);
        if (rc != HOOK_OK) return rc;

        tx->connp->in_state = htp_connp_REQ_CONNECT_CHECK;
    } else {
        htp_log(tx->connp, HTP_LOG_MARK, HTP_LOG_WARNING, 0, "[Internal Error] Invalid tx progress: %d", tx->progress);

        return HTP_ERROR;
    }

    return HTP_OK;
}

int htp_txh_state_request_start(htp_tx_t *tx) {
    // Run hook TRANSACTION_START
    int rc = hook_run_all(tx->connp->cfg->hook_transaction_start, tx->connp);
    if (rc != HOOK_OK) return rc;

    // Change state into request line parsing
    tx->connp->in_state = htp_connp_REQ_LINE;
    tx->connp->in_tx->progress = TX_PROGRESS_REQ_LINE;

    return HTP_OK;
}

int htp_txh_req_process_body_data(htp_tx_t *tx, const unsigned char *data, size_t len) {          
    // Keep track of actual data length
    tx->request_entity_len += len;

    // Send data to callbacks
    htp_tx_data_t d;
    d.tx = tx;
    d.data = (unsigned char *)data;
    d.len = len;

    int rc = htp_req_run_hook_body_data(tx->connp, &d);
    if (rc != HOOK_OK) {
        htp_log(tx->connp, HTP_LOG_MARK, HTP_LOG_ERROR, 0,
                "Request body data callback returned error (%d)", rc);
        return HTP_ERROR;
    }   

    return HTP_OK;
}