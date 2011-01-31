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
 *****************************************************************************/

#include "htp.h"

/**
 * Parses a single v0 request cookie and places the results into tx->request_cookies.
 *
 * @param connp
 * @param data
 * @param len
 * @return HTP_OK on success, HTP_ERROR on error.
 */
int htp_parse_single_cookie_v0(htp_connp_t *connp, char *data, size_t len) {
    if (len == 0) return HTP_OK;
    
    size_t pos = 0;

    // Look for '='
    while ((pos < len) && (data[pos] != '=')) pos++;
    if (pos == 0) return HTP_OK; // Ignore nameless cookies

    bstr *name = bstr_dup_mem(data, pos);
    if (name == NULL) return HTP_ERROR;

    bstr *value = NULL;
    if (pos == len) {
        // Cookie is empty
        value = bstr_dup_c("");
    } else {
        // Cookie is not empty
        value = bstr_dup_mem(data + pos + 1, len - pos - 1);
    }

    if (value == NULL) {
        bstr_free(&name);
        return HTP_ERROR;
    }

    // Add cookie
    if (connp->cfg->parameter_processor == NULL) {
        // Add cookie directly
        table_addn(connp->in_tx->request_cookies, name, value);
    } else {
        // Add cookie through parameter processor
        connp->cfg->parameter_processor(connp->in_tx->request_cookies, name, value);
    }

    return HTP_OK;
}

/**
 * Parses Cookie request header in v0 format.
 *
 * @param connp
 * @return HTP_OK on success, HTP_ERROR on error
 */
int htp_parse_cookies_v0(htp_connp_t *connp) {
    htp_header_t *cookie_header = table_get_c(connp->in_tx->request_headers, "cookie");
    if (cookie_header == NULL) return HTP_OK;

    // Create a new table to store cookies
    connp->in_tx->request_cookies = table_create(4);
    if (connp->in_tx->request_cookies == NULL) return HTP_ERROR;

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

        if (htp_parse_single_cookie_v0(connp, data + start, pos - start) != HTP_OK) {
            return HTP_ERROR;
        }

        // Go over the semicolon
        if (pos != len) pos++;
    }

    return HTP_OK;
}
