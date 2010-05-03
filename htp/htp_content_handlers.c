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
        // Process one chunk of data
        htp_urlenp_parse_partial(d->tx->request_urlenp_body, d->data, d->len);
    } else {
        // Finalize parsing
        htp_urlenp_finalize(d->tx->request_urlenp_body);

        if (d->tx->connp->cfg->parameter_processor == NULL) {
            // We are going to use the parser table directly
            d->tx->request_params_body = d->tx->request_urlenp_body->params;
            d->tx->request_params_body_reused = 1;

            htp_transcode_params(d->tx->connp, &d->tx->request_params_body, 0);
        } else {
            // We have a parameter processor defined, which means we'll
            // need to create a new table
            d->tx->request_params_body = table_create(table_size(d->tx->request_urlenp_body->params));

            // Transform parameters and store them into the new table
            bstr *name, *value;
            table_iterator_reset(d->tx->request_urlenp_body->params);
            while ((name = table_iterator_next(d->tx->request_urlenp_body->params, (void **) & value)) != NULL) {
                d->tx->connp->cfg->parameter_processor(d->tx->request_params_query, name, value);
                // TODO Check return code
            }

            htp_transcode_params(d->tx->connp, &d->tx->request_params_body, 1);
        }       
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
    connp->in_tx->request_urlenp_body = htp_urlenp_create(connp->in_tx);
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
        connp->in_tx->request_urlenp_query = htp_urlenp_create(connp->in_tx);
        if (connp->in_tx->request_urlenp_query == NULL) {
            return HOOK_ERROR;
        }       

        htp_urlenp_parse_complete(connp->in_tx->request_urlenp_query,
            (unsigned char *) bstr_ptr(connp->in_tx->parsed_uri->query),
            bstr_len(connp->in_tx->parsed_uri->query));       

        if (connp->cfg->parameter_processor == NULL) {
            // We are going to use the parser table directly
            connp->in_tx->request_params_query = connp->in_tx->request_urlenp_query->params;
            connp->in_tx->request_params_query_reused = 1;

            htp_transcode_params(connp, &connp->in_tx->request_params_query, 0);
        } else {            
            // We have a parameter processor defined, which means we'll
            // need to create a new table
            connp->in_tx->request_params_query = table_create(table_size(connp->in_tx->request_urlenp_query->params));

            // Transform parameters and store them into the new table
            bstr *name, *value;
            table_iterator_reset(connp->in_tx->request_urlenp_query->params);
            while ((name = table_iterator_next(connp->in_tx->request_urlenp_query->params, (void **) & value)) != NULL) {
                connp->cfg->parameter_processor(connp->in_tx->request_params_query, name, value);
                // TODO Check return code
            }

            htp_transcode_params(connp, &connp->in_tx->request_params_query, 1);
        }       
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
        // Process one chunk of data
        htp_mpartp_parse(d->tx->request_mpartp, d->data, d->len);
    } else {
        // Finalize parsing
        htp_mpartp_finalize(d->tx->request_mpartp);

        d->tx->request_params_body = table_create(list_size(d->tx->request_mpartp->parts));
        // TODO RC

        // Extract parameters
        htp_mpart_part_t *part = NULL;
        list_iterator_reset(d->tx->request_mpartp->parts);
        while ((part = (htp_mpart_part_t *) list_iterator_next(d->tx->request_mpartp->parts)) != NULL) {
            // Only use text parameters
            if (part->type == MULTIPART_PART_TEXT) {                
                if (d->tx->connp->cfg->parameter_processor == NULL) {
                    table_add(d->tx->request_params_body, part->name, part->value);
                } else {
                    d->tx->connp->cfg->parameter_processor(d->tx->request_params_body, part->name, part->value);
                }
            }
        }
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

    if (connp->cfg->extract_request_files) {
        connp->in_tx->request_mpartp->extract_files = 1;
        connp->in_tx->request_mpartp->extract_dir = connp->cfg->tmpdir;
    }

    free(boundary);

    // Register request body data callbacks
    htp_tx_register_request_body_data(connp->in_tx, htp_ch_multipart_callback_request_body_data);

    return HOOK_OK;
}
