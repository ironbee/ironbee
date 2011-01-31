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

#ifndef _HTP_URLENCODED_H
#define	_HTP_URLENCODED_H

typedef struct htp_urlenp_t htp_urlenp_t;
typedef struct htp_urlen_param_t htp_urlen_param_t;

#include "htp.h"

#define HTP_URLENP_DEFAULT_PARAMS_SIZE  32

#define HTP_URLENP_STATE_KEY            1
#define HTP_URLENP_STATE_VALUE          2

#define HTP_URLENCODED_MIME_TYPE        "application/x-www-form-urlencoded"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * This is the main URLENCODED parser structure. It is used to store
 * parser configuration, temporary parsing data, as well as the parameters.
 */
struct htp_urlenp_t {
    /** The transaction this parser belongs to. */
    htp_tx_t *tx;
    
    /** The character used to separate parameters. Defaults to & and should
     *  not be changed without good reason.
     */
    unsigned char argument_separator;

    /** Whether to perform URL-decoding on parameters. */
    int decode_url_encoding;        

    /** This table contains the list of parameters, indexed by name. */
    table_t *params;

    // Private fields; these are used during the parsing process only
    int _state;
    int _complete;
    bstr *_name;
    bstr_builder_t *_bb;
};

/**
 * Holds one application/x-www-form-urlencoded parameter.
 */
struct htp_urlen_param_t {
    /** Parameter name. */
    bstr *name;
    
    /** Parameter value. */
    bstr *value;
};

htp_urlenp_t *htp_urlenp_create(htp_tx_t *tx);
void htp_urlenp_destroy(htp_urlenp_t **urlenp);

void htp_urlenp_set_argument_separator(htp_urlenp_t *urlenp, unsigned char argument_separator);         
void htp_urlenp_set_decode_url_encoding(htp_urlenp_t *urlenp, int decode_url_encoding);
         
int  htp_urlenp_parse_partial(htp_urlenp_t *urlenp, unsigned char *data, size_t len);
int  htp_urlenp_parse_complete(htp_urlenp_t *urlenp, unsigned char *data, size_t len);
int  htp_urlenp_finalize(htp_urlenp_t *urlenp);

 int htp_ch_urlencoded_callback_request_line(htp_connp_t *connp);
 int htp_ch_urlencoded_callback_request_headers(htp_connp_t *connp);

#ifdef __cplusplus
}
#endif

#endif	/* _HTP_URLENCODED_H */

