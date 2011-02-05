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

#if 0

/**
 *
 */
int htp_header_parse_internal_strict(unsigned char *data, size_t len, htp_header_t *h) {
    size_t name_start, name_end;
    size_t value_start, value_end;

    // Deal with the name first
    name_start = name_end = 0;

    // Find where the header name ends
    while (name_end < len) {
        if (htp_is_lws(data[name_end]) || data[name_end] == ':') break;
        name_end++;
    }

    if (name_end == 0) {
        // Empty header name
        return -1;
    }

    if (name_end == len) {
        // TODO
        return -1;
    }

    // Is there any LWS before colon?
    size_t pos = name_end;
    while (pos < len) {
        if (!htp_is_lws(data[pos])) break;
        pos++;
        // TODO
        // return -1;
    }

    if (pos == len) {
        // TODO
        return -1;
    }

    // The next character must be a colon
    if (data[pos] != ':') {
        // TODO
        return -1;
    }

    // Move over the colon
    pos++;

    // Again, ignore any LWS
    while (pos < len) {
        if (!htp_is_lws(data[pos])) break;
        pos++;
    }

    if (pos == len) {
        // TODO
        return -1;
    }

    value_start = value_end = pos;

    while (value_end < len) {
        if (htp_is_lws(data[value_end])) break;
        value_end++;
    }

    h->name_offset = name_start;
    h->name_len = name_end - name_start;
    h->value_offset = value_start;
    h->value_len = value_end - value_start;

    return 1;
}
 */

/**
 *
 */
htp_header_t *htp_connp_header_parse(htp_connp_t *reqp, unsigned char *data, size_t len) {
    htp_header_t *h = calloc(1, sizeof (htp_header_t));
    if (h == NULL) return NULL;

    // Parse the header line    
    if (reqp->impl_header_parse(data, len, h) < 0) {
        // Invalid header line
        h->is_parsed = 0;
        h->name = bstr_dup_mem(data, len);

        return h;
    }

    // Now extract the name and the value
    h->name = bstr_dup_mem(data + h->name_offset, h->name_len);
    h->value = bstr_dup_mem(data + h->value_offset, h->value_len);
    h->is_parsed = 1;

    // Because header names are case-insensitive, we will convert
    // the name to lowercase to use it as a lookup key.
    h->name_lowercase = bstr_to_lowercase(h->name);

    return h;
}

#endif
