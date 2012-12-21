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

#include <iconv.h>

#include "htp.h"
#include "htp_connection_private.h"
#include "htp_connection_parser_private.h"

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
        htp_log((X), HTP_LOG_MARK, HTP_LOG_ERROR, 0, "Request field over soft limit"); \
    } \
} else { \
    htp_log((X), HTP_LOG_MARK, HTP_LOG_ERROR, 0, "Request field over hard limit"); \
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
        htp_log((X), HTP_LOG_MARK, HTP_LOG_ERROR, 0, "Response field over soft limit"); \
    } \
} else { \
    htp_log((X), HTP_LOG_MARK, HTP_LOG_ERROR, 0, "Response field over hard limit"); \
    return HTP_ERROR; \
}

#ifndef CR
#define CR '\r'
#endif

#ifndef LF
#define LF '\n'
#endif

#define HTP_HEADER_LIMIT_HARD               18000
#define HTP_HEADER_LIMIT_SOFT               9000

#define HTP_VALID_STATUS_MIN                100
#define HTP_VALID_STATUS_MAX                999
    
// Data structures

struct htp_cfg_t {
    /** Hard field limit length. If the parser encounters a line that's longer
     *  than this value it will give up parsing. Do note that the line limit
     *  is not the same thing as header length limit. Because of header folding,
     *  a header can end up being longer than the line limit.
     */
    size_t field_limit_hard;

    /** Soft field limit length. If this limit is reached the parser will issue
     *  a warning but continue to run.
     */
    size_t field_limit_soft;

    /** Log level, which will be used when deciding whether to store or
     *  ignore the messages issued by the parser.
     */
    enum htp_log_level_t log_level;

    /** Whether to delete each transaction after the last hook is invoked. This
     *  feature should be used when parsing traffic streams in real time.
     */
    int tx_auto_destroy;

    /**
     * Server personality identifier.
     */
    enum htp_server_personality_t server_personality;

    /** The function used for request line parsing. Depends on the personality. */
    int (*parse_request_line)(htp_connp_t *connp);

    /** The function used for response line parsing. Depends on the personality. */
    int (*parse_response_line)(htp_connp_t *connp);

    /** The function used for request header parsing. Depends on the personality. */
    int (*process_request_header)(htp_connp_t *connp);

    /** The function used for response header parsing. Depends on the personality. */
    int (*process_response_header)(htp_connp_t *connp);

    /** The function to use to transform parameters after parsing. */
    int (*parameter_processor)(htp_table_t *params, bstr *name, bstr *value);


    // Path handling

    /** Should we treat backslash characters as path segment separators? */
    int path_backslash_separators;

    /** Should we treat paths as case insensitive? */
    int path_case_insensitive;

    /** Should we compress multiple path segment separators into one? */
    int path_compress_separators;

    /** How are we expected to react to control chars in the path? */
    enum htp_unwanted_t path_control_chars_unwanted;

    /** Should the parser convert UTF-8 into a single-byte stream, using best-fit? */
    int path_utf8_convert;

    /** Should we URL-decode encoded path segment separators? */
    int path_encoded_separators_decode;

    /** How are we expected to react to encoded path separators? */
    enum htp_unwanted_t path_encoded_separators_unwanted;

    /** Should we decode %u-encoded characters? */
    int path_u_encoding_decode;

    /** How are we expected to react to %u encoding in the path? */
    enum htp_unwanted_t path_u_encoding_unwanted;

    /** Handling of invalid URL encodings. */
    enum htp_url_encoding_handling_t path_invalid_encoding_handling;

    /** How are we expected to react to invalid URL encoding in the path? */
    enum htp_unwanted_t path_invalid_encoding_unwanted;

    /** Controls how invalid UTF-8 characters are handled. */
    enum htp_unwanted_t path_utf8_invalid_unwanted;

    /** Controls how encoded NUL bytes are handled. */
    int path_nul_encoded_terminates;

    /** How are we expected to react to an encoded NUL byte? */
    enum htp_unwanted_t path_nul_encoded_unwanted;

    /** Controls how raw NUL bytes are handled. */
    int path_nul_raw_terminates;

    /** TODO */
    enum htp_unwanted_t path_nul_raw_unwanted;
    
    /** TODO */
    enum htp_unwanted_t path_unicode_unwanted;

    /** The replacement character used when there is no best-fit mapping. */
    unsigned char bestfit_replacement_char;

    /** TODO */
    int params_u_encoding_decode;

    /** TODO */
    enum htp_unwanted_t params_u_encoding_unwanted;

    /** TODO */
    enum htp_url_encoding_handling_t params_invalid_encoding_handling;

    /** TODO */
    enum htp_unwanted_t params_invalid_encoding_unwanted;

    /** TODO */
    int params_nul_encoded_terminates;

    /** TODO */
    enum htp_unwanted_t params_nul_encoded_unwanted;

    /** TODO */
    int params_nul_raw_terminates;

    /** TODO */
    enum htp_unwanted_t params_nul_raw_unwanted;

    /** The best-fit map to use to decode %u-encoded characters. */
    unsigned char *bestfit_map;

    /** Whether to generate the request_uri_normalized field. */
    int generate_request_uri_normalized;

    /** Whether to automatically decompress compressed response bodies. */
    int response_decompression_enabled;

    char *request_encoding;

    char *internal_encoding;

    int parse_request_cookies;

    int parse_request_http_authentication;

    int extract_request_files;

    char *tmpdir;

    /** Whether the local port should be used as the outgoing connection port,
     *  usually when the local machine is the target of a firewall redirect
     *  (without dport alteration)
     *  This will be false (0) in cases where the local machine is:
     *  - explicitly set as the browser proxy
     *  - operating as a transparent proxy (eg using linux TRPOXY)
     *  - using a firewall redirect but with dport altered
     *  In cases where this is false, the remote port is used */
    int use_local_port;

    // Hooks

    /** Transaction start hook, invoked when the parser receives the first
     *  byte of a new transaction.
     */
    htp_hook_t *hook_request_start;

    /** Request line hook, invoked after a request line has been parsed. */
    htp_hook_t *hook_request_line;

    /** Request URI normalization hook, for overriding default normalization of URI. */
    htp_hook_t *hook_request_uri_normalize;

    /** Request headers hook, invoked after all request headers are seen. */
    htp_hook_t *hook_request_headers;

    /** Request body data hook, invoked every time body data is available. Each
     *  invocation will provide a htp_tx_data_t instance. Chunked data
     *  will be dechunked before the data is passed to this hook. Decompression
     *  is not currently implemented. At the end of the request body
     *  there will be a call with the data pointer set to NULL.
     */
    htp_hook_t *hook_request_body_data;

    /** TODO */
    htp_hook_t *hook_request_file_data;

    /** Request trailer hook, invoked after all trailer headers are seen,
     *  and if they are seen (not invoked otherwise).
     */
    htp_hook_t *hook_request_trailer;

    /** Request hook, invoked after a complete request is seen. */
    htp_hook_t *hook_request_complete;

    /** Response startup hook, invoked when a response transaction is found and
     *  processing started.
     */
    htp_hook_t *hook_response_start;

    /** Response line hook, invoked after a response line has been parsed. */
    htp_hook_t *hook_response_line;

    /** Response headers book, invoked after all response headers have been seen. */
    htp_hook_t *hook_response_headers;

    /** Response body data hook, invoked every time body data is available. Each
     *  invocation will provide a htp_tx_data_t instance. Chunked data
     *  will be dechunked before the data is passed to this hook. By default,
     *  compressed data will be decompressed, but decompression can be disabled
     *  in configuration. At the end of the response body there will be a call
     *  with the data pointer set to NULL.
     */
    htp_hook_t *hook_response_body_data;

    /** Response trailer hook, invoked after all trailer headers have been processed,
     *  and only if the trailer exists.
     */
    htp_hook_t *hook_response_trailer;

    /** Response hook, invoked after a response has been seen. There isn't a separate
     *  transaction hook, use this hook to do something whenever a transaction is
     *  complete.
     */
    htp_hook_t *hook_response_complete;

    /**
     * Log hook, invoked every time the library wants to log.
     */
    htp_hook_t *hook_log;

    /** Opaque user data associated with this configuration structure. */
    void *user_data;
};

    
// Parser states, in the order in which they are
// used as a single transaction is processed.

htp_status_t htp_connp_REQ_IDLE(htp_connp_t *connp);
htp_status_t htp_connp_REQ_LINE(htp_connp_t *connp);
htp_status_t htp_connp_REQ_PROTOCOL(htp_connp_t *connp);
htp_status_t htp_connp_REQ_HEADERS(htp_connp_t *connp);
htp_status_t htp_connp_REQ_CONNECT_CHECK(htp_connp_t *connp);
htp_status_t htp_connp_REQ_CONNECT_WAIT_RESPONSE(htp_connp_t *connp);
htp_status_t htp_connp_REQ_BODY_DETERMINE(htp_connp_t *connp);
htp_status_t htp_connp_REQ_BODY_IDENTITY(htp_connp_t *connp);
htp_status_t htp_connp_REQ_BODY_CHUNKED_LENGTH(htp_connp_t *connp);
htp_status_t htp_connp_REQ_BODY_CHUNKED_DATA(htp_connp_t *connp);
htp_status_t htp_connp_REQ_BODY_CHUNKED_DATA_END(htp_connp_t *connp);
htp_status_t htp_connp_REQ_FINALIZE(htp_connp_t *connp);

htp_status_t htp_connp_RES_IDLE(htp_connp_t *connp);
htp_status_t htp_connp_RES_LINE(htp_connp_t *connp);
htp_status_t htp_connp_RES_HEADERS(htp_connp_t *connp);
htp_status_t htp_connp_RES_BODY_DETERMINE(htp_connp_t *connp);
htp_status_t htp_connp_RES_BODY_IDENTITY(htp_connp_t *connp);
htp_status_t htp_connp_RES_BODY_CHUNKED_LENGTH(htp_connp_t *connp);
htp_status_t htp_connp_RES_BODY_CHUNKED_DATA(htp_connp_t *connp);
htp_status_t htp_connp_RES_BODY_CHUNKED_DATA_END(htp_connp_t *connp);
htp_status_t htp_connp_RES_FINALIZE(htp_connp_t *connp);

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
htp_status_t htp_parse_authority(bstr *authority, bstr **hostname, int *port, int *flags);
int htp_parse_uri_authority(htp_connp_t *connp, bstr *input, htp_uri_t **uri);
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
int64_t htp_parse_positive_integer_whitespace(unsigned char *data, size_t len, int base);
int htp_parse_status(bstr *status);
int htp_parse_authorization_digest(htp_connp_t *connp, htp_header_t *auth_header);
int htp_parse_authorization_basic(htp_connp_t *connp, htp_header_t *auth_header);


void htp_print_log(FILE *stream, htp_log_t *log);

void fprint_bstr(FILE *stream, const char *name, bstr *b);
void fprint_raw_data(FILE *stream, const char *name, const void *data, size_t len);
void fprint_raw_data_ex(FILE *stream, const char *name, const void *data, size_t offset, size_t len);

char *htp_connp_in_state_as_string(htp_connp_t *connp);
char *htp_connp_out_state_as_string(htp_connp_t *connp);
char *htp_tx_progress_as_string(htp_tx_t *tx);

bstr *htp_unparse_uri_noencode(htp_uri_t *uri);

int htp_treat_response_line_as_body(htp_tx_t *tx);

int htp_req_run_hook_body_data(htp_connp_t *connp, htp_tx_data_t *d);
int htp_res_run_hook_body_data(htp_connp_t *connp, htp_tx_data_t *d);

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

