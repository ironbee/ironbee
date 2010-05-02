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
 * Parses a single v0 cookie.
 *
 * @param connp
 * @param data
 * @param len
 */
int htp_parse_single_cookie_v0(htp_connp_t *connp, char *data, size_t len) {
    if (len == 0) return HTP_OK;
    
    size_t pos = 0;

    // Look for '='
    while ((pos < len) && (data[pos] != '=')) pos++;
    if (pos == 0) return HTP_OK; // Ignore nameless cookies

    if (pos == len) {
        // Just a name and no value        
        bstr *name = bstr_memdup(data, pos);
        bstr *value = bstr_cstrdup("");
        
        if (connp->cfg->parameter_processor == NULL) {            
            table_add(connp->in_tx->request_cookies, name, value);
        } else {
            connp->cfg->parameter_processor(connp->in_tx->request_cookies, name, value);
        }
    } else {
        // Cookie has name and value        
        bstr *name = bstr_memdup(data, pos);
        bstr *value = bstr_memdup(data + pos + 1, len - pos - 1);

        if (connp->cfg->parameter_processor == NULL) {
            table_add(connp->in_tx->request_cookies, name, value);
        } else {
            connp->cfg->parameter_processor(connp->in_tx->request_cookies, name, value);
        }        
    }

    return HTP_OK;
}

/**
 * Parses request Cookie v0 header.
 *
 * @param connp
 */
int htp_parse_cookies_v0(htp_connp_t *connp) {
    htp_header_t *cookie_header = table_getc(connp->in_tx->request_headers, "cookie");
    if (cookie_header == NULL) return HTP_OK;

    char *data = bstr_ptr(cookie_header->value);
    size_t len = bstr_len(cookie_header->value);
    size_t pos = 0;

    while (pos < len) {
        // Ignore whitespace at the beginning
        while ((pos < len) && (isspace((int)data[pos]))) pos++;
        if (pos == len) return HTP_OK;

        size_t start = pos;

        // Find the end of the cookie
        while ((pos < len) && (data[pos] != ';')) pos++;

        htp_parse_single_cookie_v0(connp, data + start, pos - start);

        // Go over the semicolon
        if (pos != len) pos++;
    }

    return HTP_OK;
}
