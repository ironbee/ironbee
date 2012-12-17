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

#ifndef _HTP_PRIVATE_H
#define	_HTP_PRIVATE_H

#ifdef	__cplusplus
extern "C" {
#endif

#include "htp.h"

#define IN_TEST_NEXT_BYTE_OR_RETURN(X) \
if ((X)->in_current_offset >= (X)->in_current_len) { \
    return HTP_DATA; \
}

#define IN_NEXT_BYTE(X) \
if ((X)->in_current_offset < (X)->in_current_len) { \
    (X)->in_next_byte = (X)->in_current_data[(X)->in_current_offset]; \
    (X)->in_current_offset++; \
    (X)->in_stream_offset++; \
} else { \
    (X)->in_next_byte = -1; \
}

#define IN_NEXT_BYTE_OR_RETURN(X) \
if ((X)->in_current_offset < (X)->in_current_len) { \
    (X)->in_next_byte = (X)->in_current_data[(X)->in_current_offset]; \
    (X)->in_current_offset++; \
    (X)->in_stream_offset++; \
} else { \
    return HTP_DATA; \
}

#define IN_COPY_BYTE_OR_RETURN(X) \
if ((X)->in_current_offset < (X)->in_current_len) { \
    (X)->in_next_byte = (X)->in_current_data[(X)->in_current_offset]; \
    (X)->in_current_offset++; \
    (X)->in_stream_offset++; \
} else { \
    return HTP_DATA; \
} \
\
if ((X)->in_line_len < (X)->in_line_size) { \
    (X)->in_line[(X)->in_line_len] = (X)->in_next_byte; \
    (X)->in_line_len++; \
    if (((X)->in_line_len == HTP_HEADER_LIMIT_SOFT)&&(!((X)->in_tx->flags & HTP_FIELD_LONG))) { \
        (X)->in_tx->flags |= HTP_FIELD_LONG; \
        htp_log((X), HTP_LOG_MARK, HTP_LOG_ERROR, HTP_LINE_TOO_LONG_SOFT, "Request field over soft limit"); \
    } \
} else { \
    htp_log((X), HTP_LOG_MARK, HTP_LOG_ERROR, HTP_LINE_TOO_LONG_HARD, "Request field over hard limit"); \
    return HTP_ERROR; \
}

#define OUT_TEST_NEXT_BYTE_OR_RETURN(X) \
if ((X)->out_current_offset >= (X)->out_current_len) { \
    return HTP_DATA; \
}

#define OUT_NEXT_BYTE(X) \
if ((X)->out_current_offset < (X)->out_current_len) { \
    (X)->out_next_byte = (X)->out_current_data[(X)->out_current_offset]; \
    (X)->out_current_offset++; \
    (X)->out_stream_offset++; \
} else { \
    (X)->out_next_byte = -1; \
}

#define OUT_NEXT_BYTE_OR_RETURN(X) \
if ((X)->out_current_offset < (X)->out_current_len) { \
    (X)->out_next_byte = (X)->out_current_data[(X)->out_current_offset]; \
    (X)->out_current_offset++; \
    (X)->out_stream_offset++; \
} else { \
    return HTP_DATA; \
}

#define OUT_COPY_BYTE_OR_RETURN(X) \
if ((X)->out_current_offset < (X)->out_current_len) { \
    (X)->out_next_byte = (X)->out_current_data[(X)->out_current_offset]; \
    (X)->out_current_offset++; \
    (X)->out_stream_offset++; \
} else { \
    return HTP_DATA; \
} \
\
if ((X)->out_line_len < (X)->out_line_size) { \
    (X)->out_line[(X)->out_line_len] = (X)->out_next_byte; \
    (X)->out_line_len++; \
    if (((X)->out_line_len == HTP_HEADER_LIMIT_SOFT)&&(!((X)->out_tx->flags & HTP_FIELD_LONG))) { \
        (X)->out_tx->flags |= HTP_FIELD_LONG; \
        htp_log((X), HTP_LOG_MARK, HTP_LOG_ERROR, HTP_LINE_TOO_LONG_SOFT, "Response field over soft limit"); \
    } \
} else { \
    htp_log((X), HTP_LOG_MARK, HTP_LOG_ERROR, HTP_LINE_TOO_LONG_HARD, "Response field over hard limit"); \
    return HTP_ERROR; \
}

    
// Parser states, in the order in which they are
// used as a single transaction is processed.

int htp_connp_REQ_IDLE(htp_connp_t *connp);
int htp_connp_REQ_LINE(htp_connp_t *connp);
int htp_connp_REQ_PROTOCOL(htp_connp_t *connp);
int htp_connp_REQ_HEADERS(htp_connp_t *connp);
int htp_connp_REQ_CONNECT_CHECK(htp_connp_t *connp);
int htp_connp_REQ_CONNECT_WAIT_RESPONSE(htp_connp_t *connp);
int htp_connp_REQ_BODY_DETERMINE(htp_connp_t *connp);
int htp_connp_REQ_BODY_IDENTITY(htp_connp_t *connp);
int htp_connp_REQ_BODY_CHUNKED_LENGTH(htp_connp_t *connp);
int htp_connp_REQ_BODY_CHUNKED_DATA(htp_connp_t *connp);
int htp_connp_REQ_BODY_CHUNKED_DATA_END(htp_connp_t *connp);
int htp_connp_REQ_FINALIZE(htp_connp_t *connp);

int htp_connp_RES_IDLE(htp_connp_t *connp);
int htp_connp_RES_LINE(htp_connp_t *connp);
int htp_connp_RES_HEADERS(htp_connp_t *connp);
int htp_connp_RES_BODY_DETERMINE(htp_connp_t *connp);
int htp_connp_RES_BODY_IDENTITY(htp_connp_t *connp);
int htp_connp_RES_BODY_CHUNKED_LENGTH(htp_connp_t *connp);
int htp_connp_RES_BODY_CHUNKED_DATA(htp_connp_t *connp);
int htp_connp_RES_BODY_CHUNKED_DATA_END(htp_connp_t *connp);
int htp_connp_RES_FINALIZE(htp_connp_t *connp);

// Parsing functions

int htp_parse_request_line_generic(htp_connp_t *connp);
int htp_parse_request_header_generic(htp_connp_t *connp, htp_header_t *h, unsigned char *data, size_t len);
int htp_process_request_header_generic(htp_connp_t *);

int htp_parse_request_header_apache_2_2(htp_connp_t *connp, htp_header_t *h, unsigned char *data, size_t len);
int htp_parse_request_line_apache_2_2(htp_connp_t *connp);
int htp_process_request_header_apache_2_2(htp_connp_t *);

int htp_parse_response_line_generic(htp_connp_t *connp);
int htp_parse_response_header_generic(htp_connp_t *connp, htp_header_t *h, unsigned char *data, size_t len);
int htp_process_response_header_generic(htp_connp_t *connp);


// Utility functions

int htp_convert_method_to_number(bstr *);
int htp_is_lws(int c);
int htp_is_separator(int c);
int htp_is_text(int c);
int htp_is_token(int c);
int htp_chomp(unsigned char *data, size_t *len);
int htp_is_space(int c);

int htp_parse_protocol(bstr *protocol);

int htp_is_line_empty(unsigned char *data, size_t len);
int htp_is_line_whitespace(unsigned char *data, size_t len);

int htp_connp_is_line_folded(unsigned char *data, size_t len);
int htp_connp_is_line_terminator(htp_connp_t *connp, unsigned char *data, size_t len);
int htp_connp_is_line_ignorable(htp_connp_t *connp, unsigned char *data, size_t len);

int htp_parse_uri(bstr *input, htp_uri_t **uri);
int htp_parse_authority(htp_connp_t *connp, bstr *input, htp_uri_t **uri);
int htp_normalize_parsed_uri(htp_connp_t *connp, htp_uri_t *parsed_uri_incomplete, htp_uri_t *parsed_uri);
bstr *htp_normalize_hostname_inplace(bstr *input);
void htp_replace_hostname(htp_connp_t *connp, htp_uri_t *parsed_uri, bstr *hostname);
int htp_is_uri_unreserved(unsigned char c);

int htp_decode_path_inplace(htp_cfg_t *cfg, htp_tx_t *tx, bstr *path);

void htp_uriencoding_normalize_inplace(bstr *s);

 int htp_prenormalize_uri_path_inplace(bstr *s, int *flags, int case_insensitive, int backslash, int decode_separators, int remove_consecutive);
void htp_normalize_uri_path_inplace(bstr *s);

void htp_utf8_decode_path_inplace(htp_cfg_t *cfg, htp_tx_t *tx, bstr *path);
void htp_utf8_validate_path(htp_tx_t *tx, bstr *path);

int htp_parse_content_length(bstr *b);
int htp_parse_chunked_length(unsigned char *data, size_t len);
int htp_parse_positive_integer_whitespace(unsigned char *data, size_t len, int base);
int htp_parse_status(bstr *status);
int htp_parse_authorization_digest(htp_connp_t *connp, htp_header_t *auth_header);
int htp_parse_authorization_basic(htp_connp_t *connp, htp_header_t *auth_header);

void htp_log(htp_connp_t *connp, const char *file, int line, int level, int code, const char *fmt, ...);
void htp_print_log(FILE *stream, htp_log_t *log);

void fprint_bstr(FILE *stream, const char *name, bstr *b);
void fprint_raw_data(FILE *stream, const char *name, const void *data, size_t len);
void fprint_raw_data_ex(FILE *stream, const char *name, const void *data, size_t offset, size_t len);

char *htp_connp_in_state_as_string(htp_connp_t *connp);
char *htp_connp_out_state_as_string(htp_connp_t *connp);
char *htp_tx_progress_as_string(htp_tx_t *tx);

bstr *htp_unparse_uri_noencode(htp_uri_t *uri);

int htp_treat_response_line_as_body(htp_tx_t *tx);

bstr *htp_tx_generate_request_headers_raw(htp_tx_t *tx);
bstr *htp_tx_get_request_headers_raw(htp_tx_t *tx);

bstr *htp_tx_generate_response_headers_raw(htp_tx_t *tx);
bstr *htp_tx_get_response_headers_raw(htp_tx_t *tx);

int htp_tx_req_has_body(htp_tx_t *tx);

int htp_req_run_hook_body_data(htp_connp_t *connp, htp_tx_data_t *d);
int htp_res_run_hook_body_data(htp_connp_t *connp, htp_tx_data_t *d);

void htp_tx_register_request_body_data(htp_tx_t *tx, int (*callback_fn)(htp_tx_data_t *));
void htp_tx_register_response_body_data(htp_tx_t *tx, int (*callback_fn)(htp_tx_data_t *));

int htp_ch_urlencoded_callback_request_body_data(htp_tx_data_t *d);
int htp_ch_urlencoded_callback_request_headers(htp_connp_t *connp);
int htp_ch_urlencoded_callback_request_line(htp_connp_t *connp);
int htp_ch_multipart_callback_request_body_data(htp_tx_data_t *d);
int htp_ch_multipart_callback_request_headers(htp_connp_t *connp);

int htp_php_parameter_processor(htp_table_t *params, bstr *name, bstr *value);

int htp_transcode_params(htp_connp_t *connp, htp_table_t **params, int destroy_old);
int htp_transcode_bstr(iconv_t cd, bstr *input, bstr **output);

int htp_parse_single_cookie_v0(htp_connp_t *connp, unsigned char *data, size_t len);
int htp_parse_cookies_v0(htp_connp_t *connp);
int htp_parse_authorization(htp_connp_t *connp);

int htp_decode_urlencoded_inplace(htp_cfg_t *cfg, htp_tx_t *tx, bstr *input);

bstr *htp_extract_quoted_string_as_bstr(unsigned char *data, size_t len, size_t *endoffset);



htp_header_t *htp_connp_header_parse(htp_connp_t *, unsigned char *, size_t);

#ifdef	__cplusplus
}
#endif

#endif	/* _HTP_PRIVATE_H */

