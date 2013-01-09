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

#include "htp.h"
#include "htp_private.h"

/**
 * Invoked to process a part of request body data.
 *
 * @param[in] d
 */
htp_status_t htp_ch_urlencoded_callback_request_body_data(htp_tx_data_t *d) {
    htp_tx_t *tx = d->tx;

    if (d->data != NULL) {
        // Process one chunk of data
        htp_urlenp_parse_partial(tx->request_urlenp_body, d->data, d->len);
    } else {
        // Finalize parsing
        htp_urlenp_finalize(tx->request_urlenp_body);

        // Add all parameters to the transaction
        bstr *name = NULL;
        bstr *value = NULL;
        for (int i = 0, n = htp_table_size(tx->request_urlenp_body->params); i < n; i++) {
            htp_table_get_index(tx->request_urlenp_body->params, i, &name, (void **) &value);

            htp_param_t *param = calloc(1, sizeof (htp_param_t));
            if (param == NULL) return HTP_ERROR;
            param->name = name;
            param->value = value;
            param->source = HTP_SOURCE_BODY;
            param->parser_id = HTP_PARSER_URLENCODED;
            param->parser_data = NULL;

            if (htp_tx_req_add_param(tx, param) != HTP_OK) {
                free(param);
                return HTP_ERROR;
            }
        }

        // All the parameter data is now owned by the transaction, and
        // the parser table used to store it is no longer needed. The
        // line below will destroy just the table, leaving keys intact.
        htp_table_destroy_ex(&tx->request_urlenp_body->params);
    }

    return HTP_OK;
}

/**
 * Determine if the request has a URLENCODED body, then
 * create and attach the URLENCODED parser if it does.
 */
htp_status_t htp_ch_urlencoded_callback_request_headers(htp_connp_t *connp) {
    htp_tx_t *tx = connp->in_tx;
    
    // Check the request content type to see if it matches our MIME type
    if ((tx->request_content_type == NULL) || (bstr_cmp_c(tx->request_content_type, HTP_URLENCODED_MIME_TYPE) != 0)) {
        #ifdef HTP_DEBUG
        fprintf(stderr, "htp_ch_urlencoded_callback_request_headers: Body not URLENCODED\n");
        #endif

        return HTP_OK;
    }

    #ifdef HTP_DEBUG
    fprintf(stderr, "htp_ch_urlencoded_callback_request_headers: Parsing URLENCODED body\n");
    #endif

    // Create parser instance
    tx->request_urlenp_body = htp_urlenp_create(tx);
    if (tx->request_urlenp_body == NULL) return HTP_ERROR;

    // Register a request body data callback
    htp_tx_register_request_body_data(tx, htp_ch_urlencoded_callback_request_body_data);

    return HTP_OK;
}

/**
 * Parse query string, if available. This method is invoked after the
 * request line has been processed.
 *
 * @param[in] connp
 */
htp_status_t htp_ch_urlencoded_callback_request_line(htp_connp_t *connp) {
    htp_tx_t *tx = connp->in_tx;

    // Parse query string, when available
    if ((tx->parsed_uri->query != NULL) && (bstr_len(tx->parsed_uri->query) > 0)) {
        tx->request_urlenp_query = htp_urlenp_create(tx);
        if (tx->request_urlenp_query == NULL) return HTP_ERROR;

        htp_urlenp_parse_complete(tx->request_urlenp_query, bstr_ptr(tx->parsed_uri->query), bstr_len(tx->parsed_uri->query));

        // Add all parameters to the transaction
        bstr *name = NULL;
        bstr *value = NULL;
        for (int i = 0, n = htp_table_size(tx->request_urlenp_query->params); i < n; i++) {
            htp_table_get_index(tx->request_urlenp_query->params, i, &name, (void **) &value);

            htp_param_t *param = calloc(1, sizeof (htp_param_t));
            if (param == NULL) return HTP_ERROR;
            param->name = name;
            param->value = value;
            param->source = HTP_SOURCE_QUERY_STRING;
            param->parser_id = HTP_PARSER_URLENCODED;
            param->parser_data = NULL;

            if (htp_tx_req_add_param(tx, param) != HTP_OK) {
                free(param);
                return HTP_ERROR;
            }
        }

        // All the parameter data is now owned by the transaction, and
        // the parser table used to store it is no longer needed. The
        // line below will destroy just the table, leaving keys intact.
        htp_table_destroy_ex(&tx->request_urlenp_query->params);
    }

    return HTP_OK;
}

/**
 * Finalize MULTIPART processing.
 * 
 * @param[in] d
 */
htp_status_t htp_ch_multipart_callback_request_body_data(htp_tx_data_t *d) {
    htp_tx_t *tx = d->tx;

    if (d->data != NULL) {
        // Process one chunk of data
        htp_mpartp_parse(d->tx->request_mpartp, d->data, d->len);
    } else {
        // Finalize parsing
        htp_mpartp_finalize(tx->request_mpartp);

        for (int i = 0, n = htp_list_size(tx->request_mpartp->parts); i < n; i++) {
            htp_mpart_part_t *part = htp_list_get(tx->request_mpartp->parts, i);

            // Use text parameters
            if (part->type == MULTIPART_PART_TEXT) {
                //htp_tx_req_add_body_param(d->tx, part->name, part->value);

                htp_param_t *param = calloc(1, sizeof (htp_param_t));
                if (param == NULL) return HTP_ERROR;
                param->name = part->name;
                param->value = part->value;
                param->source = HTP_SOURCE_BODY;
                param->parser_id = HTP_PARSER_MULTIPART;
                param->parser_data = part;

                if (htp_tx_req_add_param(tx, param) != HTP_OK) {
                    free(param);
                    return HTP_ERROR;
                }
            }
        }

        // Tell the parser that it no longer owns names and values
        // of MULTIPART_PART_TEXT parts.
        tx->request_mpartp->gave_up_data = 1;
    }

    return HTP_OK;
}

/**
 * Inspect request headers and register the MULTIPART request data hook
 * if it contains a multipart/form-data body.
 *
 * @param[in] connp
 */
htp_status_t htp_ch_multipart_callback_request_headers(htp_connp_t *connp) {
    htp_tx_t *tx = connp->in_tx;

    // Check the request content type to see if it matches our MIME type
    if ((tx->request_content_type == NULL) || (bstr_cmp_c(tx->request_content_type, HTP_MULTIPART_MIME_TYPE) != 0)) {
        #ifdef HTP_DEBUG
        fprintf(stderr, "htp_ch_multipart_callback_request_headers: Body not MULTIPART\n");
        #endif

        return HTP_OK;
    }

    #ifdef HTP_DEBUG
    fprintf(stderr, "htp_ch_multipart_callback_request_headers: Parsing MULTIPART body\n");
    #endif

    htp_header_t *ct = htp_table_get_c(tx->request_headers, "content-type");
    if (ct == NULL) return HTP_OK;

    char *boundary = NULL;

    int rc = htp_mpartp_extract_boundary(ct->value, &boundary);
    if (rc != HTP_OK) {
        // TODO Invalid boundary
        return HTP_OK;
    }

    // Create parser instance
    tx->request_mpartp = htp_mpartp_create(connp->cfg, boundary);
    if (tx->request_mpartp == NULL) {
        free(boundary);
        return HTP_ERROR;
    }

    if (tx->cfg->extract_request_files) {
        tx->request_mpartp->extract_files = 1;
        tx->request_mpartp->extract_dir = connp->cfg->tmpdir;
    }

    free(boundary);

    // Register a request body data callback
    htp_tx_register_request_body_data(tx, htp_ch_multipart_callback_request_body_data);

    return HTP_OK;
}
