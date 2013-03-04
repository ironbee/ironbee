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

#ifndef HTP_CONFIG_PRIVATE_H
#define	HTP_CONFIG_PRIVATE_H

#ifdef	__cplusplus
extern "C" {
#endif

struct htp_cfg_t {
    /**
     * Hard field limit length. If the parser encounters a line that's longer
     * than this value it will give up parsing.
     */
    size_t field_limit_hard;

    /**
     * Soft field limit length. If this limit is reached the parser will issue
     * a warning but continue to run.
     */
    size_t field_limit_soft;

    /**
     * Log level, which will be used when deciding whether to store or
     * ignore the messages issued by the parser.
     */
    enum htp_log_level_t log_level;

    /**
     * Whether to delete each transaction after the last hook is invoked. This
     * feature should be used when parsing traffic streams in real time.
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
    int (*process_request_header)(htp_connp_t *connp, unsigned char *data, size_t len);

    /** The function used for response header parsing. Depends on the personality. */
    int (*process_response_header)(htp_connp_t *connp, unsigned char *data, size_t len);

    /** The function to use to transform parameters after parsing. */
    int (*parameter_processor)(htp_param_t *param);


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

    /** Determines server response to a raw NUL byte in the path. */
    enum htp_unwanted_t path_nul_raw_unwanted;

    /** The replacement character used when there is no best-fit mapping. */
    unsigned char bestfit_replacement_char;

    /** Should %u encoding characters be decoded. */
    int params_u_encoding_decode;

    /** Determines server response to %u encoding in the parameters. */
    enum htp_unwanted_t params_u_encoding_unwanted;

    /** Determines server handling of invalid URL encoding. */
    enum htp_url_encoding_handling_t params_invalid_encoding_handling;

    /** Determines server response to invalid URL encoding in the parameters.  */
    enum htp_unwanted_t params_invalid_encoding_unwanted;

    /** Determines if an encoded NUL byte terminates URL-encoded parameters. */
    int params_nul_encoded_terminates;

    /** Determines server response to an encoded NUL byte in the parameters. */
    enum htp_unwanted_t params_nul_encoded_unwanted;

    /** Determines if a raw NUL byte terminates the parameters. */
    int params_nul_raw_terminates;

    /** Determines server response to a raw NUL byte in the parameters. */
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

    // Hooks

    /**
     * Transaction start hook, invoked when the parser receives the first
     * byte of a new transaction.
     */
    htp_hook_t *hook_request_start;

    /** Request line hook, invoked after a request line has been parsed. */
    htp_hook_t *hook_request_line;

    /** Request URI normalization hook, for overriding default normalization of URI. */
    htp_hook_t *hook_request_uri_normalize;

    /** Request headers hook, invoked after all request headers are seen. */
    htp_hook_t *hook_request_headers;

    /**
     * Request body data hook, invoked every time body data is available. Each
     * invocation will provide a htp_tx_data_t instance. Chunked data
     * will be dechunked before the data is passed to this hook. Decompression
     * is not currently implemented. At the end of the request body
     * there will be a call with the data pointer set to NULL.
     */
    htp_hook_t *hook_request_body_data;

    /**
     * Request file data hook, which is invoked whenever request file data is
     * available. Currently used only by the Multipart parser.
     */
    htp_hook_t *hook_request_file_data;

    /**
     * Request trailer hook, invoked after all trailer headers are seen,
     * and if they are seen (not invoked otherwise).
     */
    htp_hook_t *hook_request_trailer;

    /** Request hook, invoked after a complete request is seen. */
    htp_hook_t *hook_request_complete;

    /**
     * Response startup hook, invoked when a response transaction is found and
     * processing started.
     */
    htp_hook_t *hook_response_start;

    /** Response line hook, invoked after a response line has been parsed. */
    htp_hook_t *hook_response_line;

    /** Response headers book, invoked after all response headers have been seen. */
    htp_hook_t *hook_response_headers;

    /**
     * Response body data hook, invoked every time body data is available. Each
     * invocation will provide a htp_tx_data_t instance. Chunked data
     * will be dechunked before the data is passed to this hook. By default,
     * compressed data will be decompressed, but decompression can be disabled
     * in configuration. At the end of the response body there will be a call
     * with the data pointer set to NULL.
     */
    htp_hook_t *hook_response_body_data;

    /**
     * Response trailer hook, invoked after all trailer headers have been processed,
     * and only if the trailer exists.
     */
    htp_hook_t *hook_response_trailer;

    /**
     * Response hook, invoked after a response has been seen. There isn't a separate
     * transaction hook, use this hook to do something whenever a transaction is
     * complete.
     */
    htp_hook_t *hook_response_complete;

    /**
     * Log hook, invoked every time the library wants to log.
     */
    htp_hook_t *hook_log;

    /** Opaque user data associated with this configuration structure. */
    void *user_data;
};

#ifdef	__cplusplus
}
#endif

#endif	/* HTP_CONFIG_PRIVATE H */

