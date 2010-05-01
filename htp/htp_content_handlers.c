/*
 * LibHTP (http://www.libhtp.org)
 * Copyright 2009,2010 Ivan Ristic <ivanr@webkreator.com>
 *
 * LibHTP is an open source product, released under terms of the General Public Licence
 * version 2 (GPLv2). Please refer to the file LICENSE, which contains the complete text
 * of the license.
 *
 * In addition, there is a special exception that allows LibHTP to be freely
 * used with any OSI-approved open source licence. Please refer to the file
 * LIBHTP_LICENSING_EXCEPTION for the full text of the exception.
 *
 */

#include "htp.h"


/**
 * Invoked to process a part of request body data.
 *
 * @param d
 */
int htp_ch_urlencoded_callback_request_body_data(htp_tx_data_t *d) {
    if (d->data != NULL) {
        htp_urlenp_parse_partial(d->tx->request_urlenp_body, d->data, d->len);
    } else {
        htp_urlenp_finalize(d->tx->request_urlenp_body);
    }

    return HOOK_OK;
}

/**
 * Determine if the request has a URLENCODED body, then
 * create and attach the URLENCODED parser if it does.
 */
int htp_ch_urlencoded_callback_request_headers(htp_connp_t *connp) {
    // Check the request content type to see if it matches our MIME type
    if ((connp->in_tx->request_content_type == NULL) || (bstr_cmpc(connp->in_tx->request_content_type, HTP_URLENCODED_MIME_TYPE) != 0)) {
        return HOOK_OK;
    }

    // Create parser instance
    connp->in_tx->request_urlenp_body = htp_urlenp_create();
    if (connp->in_tx->request_urlenp_body == NULL) {
        return HOOK_ERROR;
    }

    // Register request body data callbacks
    htp_tx_register_request_body_data(connp->in_tx, htp_ch_urlencoded_callback_request_body_data);

    return HOOK_OK;
}

/**
 * Parse query string, if available. This method is invoked after the
 * request line has been processed.
 *
 * @param connp
 */
int htp_ch_urlencoded_callback_request_line(htp_connp_t *connp) {
    // Parse query string, when available
    if ((connp->in_tx->parsed_uri->query != NULL) && (bstr_len(connp->in_tx->parsed_uri->query) > 0)) {
        connp->in_tx->request_urlenp_query = htp_urlenp_create();
        if (connp->in_tx->request_urlenp_query == NULL) {
            return HOOK_ERROR;
        }

        htp_urlenp_parse_complete(connp->in_tx->request_urlenp_query,
            (unsigned char *) bstr_ptr(connp->in_tx->parsed_uri->query),
            bstr_len(connp->in_tx->parsed_uri->query));
    }

    return HOOK_OK;
}

/**
 * Finalize MULTIPART processing.
 * 
 * @param d
 */
int htp_ch_multipart_callback_request_body_data(htp_tx_data_t *d) {
    if (d->data != NULL) {        
        htp_mpartp_parse(d->tx->request_mpartp, d->data, d->len);
    } else {
        htp_mpartp_finalize(d->tx->request_mpartp);
    }

    return HOOK_OK;
}

/**
 * Inspect request headers and register the MULTIPART request data hook
 * if it contains a multipart/form-data body.
 *
 * @param connp
 */
int htp_ch_multipart_callback_request_headers(htp_connp_t *connp) {            
    // Check the request content type to see if it matches our MIME type
    if ((connp->in_tx->request_content_type == NULL) || (bstr_cmpc(connp->in_tx->request_content_type, HTP_MULTIPART_MIME_TYPE) != 0)) {
        return HOOK_OK;
    }

    htp_header_t *ct = table_getc(connp->in_tx->request_headers, "content-type");
    // TODO Is NULL?

    char *boundary = NULL;

    int rc = htp_mpartp_extract_boundary(ct->value, &boundary);
    if (rc != HTP_OK) {        
        // TODO Invalid boundary
        return HOOK_OK;
    }
    
    // Create parser instance
    connp->in_tx->request_mpartp = htp_mpartp_create(boundary);
    if (connp->in_tx->request_mpartp == NULL) {
        free(boundary);
        return HOOK_ERROR;
    }

    free(boundary);

    // Register request body data callbacks
    htp_tx_register_request_body_data(connp->in_tx, htp_ch_multipart_callback_request_body_data);

    return HOOK_OK;
}
