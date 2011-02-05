/***************************************************************************
 * Copyright 2009-2010 Open Information Security Foundation
 * Copyright 2010-2011 Qualys, Inc.
 *
 * Licensed to You under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 ***************************************************************************/

/**
 * @file
 * @author Ivan Ristic <ivanr@webkreator.com>
 */

#include "htp.h"
#include "htp_base64.h"

/**
 * Determines protocol number from a textual representation (i.e., "HTTP/1.1"). This
 * function will only understand a properly formatted protocol information. It does
 * not try to be flexible.
 * 
 * @param protocol
 * @return Protocol version or PROTOCOL_UKNOWN.
 */
int htp_parse_protocol(bstr *protocol) {
    // TODO This function uses a very strict approach to parsing, whereas
    //      browsers will typically be more flexible, allowing whitespace
    //      before and after the forward slash, as well as allowing leading
    //      zeroes in the numbers. We should be able to parse such malformed
    //      content correctly (but emit a warning).
    if (bstr_len(protocol) == 8) {
        char *ptr = bstr_ptr(protocol);
        if ((ptr[0] == 'H') && (ptr[1] == 'T') && (ptr[2] == 'T') && (ptr[3] == 'P')
            && (ptr[4] == '/') && (ptr[6] == '.')) {
            // Check the version numbers
            if (ptr[5] == '0') {
                if (ptr[7] == '9') {
                    return HTTP_0_9;
                }
            } else if (ptr[5] == '1') {
                if (ptr[7] == '0') {
                    return HTTP_1_0;
                } else if (ptr[7] == '1') {
                    return HTTP_1_1;
                }
            }
        }
    }

    return PROTOCOL_UNKNOWN;
}

/**
 * Determines the numerical value of a response status given as a string.
 *
 * @param status
 * @return Status code on success, or -1 on error.
 */
int htp_parse_status(bstr *status) {
    return htp_parse_positive_integer_whitespace((unsigned char *) bstr_ptr(status), bstr_len(status), 10);
}

/**
 * Parses Digest Authorization request header.
 *
 * @param connp
 * @param auth_header
 */
int htp_parse_authorization_digest(htp_connp_t *connp, htp_header_t *auth_header) {    
    // Extract the username
    int i = bstr_index_of_c(auth_header->value, "username=");
    if (i == -1) return HTP_ERROR;   

    char *data = bstr_ptr(auth_header->value);
    size_t len = bstr_len(auth_header->value);
    size_t pos = i + 9;

    // Ignore whitespace
    while ((pos < len) && (isspace((int) data[pos]))) pos++;   

    if (data[pos] == '"') {
        connp->in_tx->request_auth_username = htp_extract_quoted_string_as_bstr(data + pos, len - pos, NULL);        
    } else {
        return HTP_ERROR;
    }   

    return HTP_OK;
}

/**
 * Parses Basic Authorization request header.
 * 
 * @param connp
 * @param auth_header
 */
int htp_parse_authorization_basic(htp_connp_t *connp, htp_header_t *auth_header) {
    char *data = bstr_ptr(auth_header->value);
    size_t len = bstr_len(auth_header->value);
    size_t pos = 5;

    // Ignore whitespace
    while ((pos < len) && (isspace((int) data[pos]))) pos++;
    if (pos == len) return HTP_ERROR;

    // Decode base64-encoded data
    bstr *decoded = htp_base64_decode_mem(data + pos, len - pos);
    if (decoded == NULL) return HTP_ERROR;

    // Now extract the username and password
    int i = bstr_index_of_c(decoded, ":");
    if (i == -1) {
        bstr_free(&decoded);    
        return HTP_ERROR;
    }

    connp->in_tx->request_auth_username = bstr_dup_ex(decoded, 0, i);
    if (connp->in_tx->request_auth_username == NULL) {
        bstr_free(&decoded);
        return HTP_ERROR;
    }
    
    connp->in_tx->request_auth_password = bstr_dup_ex(decoded, i + 1, bstr_len(decoded) - i - 1);
    if (connp->in_tx->request_auth_password) {
        bstr_free(&decoded);
        bstr_free(&connp->in_tx->request_auth_username);
        return HTP_ERROR;
    }

    bstr_free(&decoded);

    return HTP_OK;
}

/**
 * Parses Authorization request header.
 *
 * @param connp
 */
int htp_parse_authorization(htp_connp_t *connp) {
    htp_header_t *auth_header = table_get_c(connp->in_tx->request_headers, "authorization");
    if (auth_header == NULL) return HTP_OK;

    if (bstr_begins_withc_nocase(auth_header->value, "basic")) {
        // Basic authentication
        connp->in_tx->request_auth_type = HTP_AUTH_BASIC;
        return htp_parse_authorization_basic(connp, auth_header);
    } else if (bstr_begins_withc_nocase(auth_header->value, "digest")) {
        // Digest authentication
        connp->in_tx->request_auth_type = HTP_AUTH_DIGEST;
        return htp_parse_authorization_digest(connp, auth_header);
    } else {
        // TODO Report unknown Authorization header
        connp->in_tx->request_auth_type = HTP_AUTH_UNKNOWN;
    }

    return HTP_OK;
}
