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

#ifndef _HTP_H
#define	_HTP_H

#include "htp_definitions.h"

typedef struct htp_cfg_t htp_cfg_t;
typedef struct htp_conn_t htp_conn_t;
typedef struct htp_connp_t htp_connp_t;
typedef struct htp_file_t htp_file_t;
typedef struct htp_file_data_t htp_file_data_t;
typedef struct htp_header_t htp_header_t;
typedef struct htp_header_line_t htp_header_line_t;
typedef struct htp_log_t htp_log_t;
typedef struct htp_tx_data_t htp_tx_data_t;
typedef struct htp_tx_t htp_tx_t;
typedef struct htp_uri_t htp_uri_t;
typedef struct timeval htp_time_t;

#include <ctype.h>
#include <iconv.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "bstr.h"
#include "htp_list.h"
#include "htp_table.h"
#include "htp_hooks.h"
#include "htp_decompressors.h"
#include "htp_urlencoded.h"
#include "htp_multipart.h"

// -- Defines -------------------------------------------------------------------------------------

#define HTP_BASE_VERSION_TEXT	"master"

#define HTP_PROTOCOL_UNKNOWN    -1
#define HTTP_0_9                9
#define HTTP_1_0                100
#define HTTP_1_1                101

#define HTP_LOG_MARK                __FILE__,__LINE__

#define HTP_LOG_ERROR               1
#define HTP_LOG_WARNING             2
#define HTP_LOG_NOTICE              3
#define HTP_LOG_INFO                4
#define HTP_LOG_DEBUG               5
#define HTP_LOG_DEBUG2              6

#define HTP_HEADER_MISSING_COLON            1
#define HTP_HEADER_INVALID_NAME             2
#define HTP_HEADER_LWS_AFTER_FIELD_NAME     3
#define HTP_LINE_TOO_LONG_HARD              4
#define HTP_LINE_TOO_LONG_SOFT              5

#define HTP_HEADER_LIMIT_HARD               18000
#define HTP_HEADER_LIMIT_SOFT               9000

#define HTP_VALID_STATUS_MIN    100
#define HTP_VALID_STATUS_MAX    999

#define LOG_NO_CODE             0

#define HTP_M_UNKNOWN              -1

// The following request method are defined in Apache 2.2.13, in httpd.h.
#define HTP_M_GET                   0
#define HTP_M_PUT                   1
#define HTP_M_POST                  2
#define HTP_M_DELETE                3
#define HTP_M_CONNECT               4
#define HTP_M_OPTIONS               5
#define HTP_M_TRACE                 6
#define HTP_M_PATCH                 7
#define HTP_M_PROPFIND              8
#define HTP_M_PROPPATCH             9
#define HTP_M_MKCOL                 10
#define HTP_M_COPY                  11
#define HTP_M_MOVE                  12
#define HTP_M_LOCK                  13
#define HTP_M_UNLOCK                14
#define HTP_M_VERSION_CONTROL       15
#define HTP_M_CHECKOUT              16
#define HTP_M_UNCHECKOUT            17
#define HTP_M_CHECKIN               18
#define HTP_M_UPDATE                19
#define HTP_M_LABEL                 20
#define HTP_M_REPORT                21
#define HTP_M_MKWORKSPACE           22
#define HTP_M_MKACTIVITY            23
#define HTP_M_BASELINE_CONTROL      24
#define HTP_M_MERGE                 25
#define HTP_M_INVALID               26

// Interestingly, Apache does not define M_HEAD
#define HTP_M_HEAD                  1000

#define HTP_FIELD_UNPARSEABLE           0x000001
#define HTP_FIELD_INVALID               0x000002
#define HTP_FIELD_FOLDED                0x000004
#define HTP_FIELD_REPEATED              0x000008
#define HTP_FIELD_LONG                  0x000010
#define HTP_FIELD_NUL_BYTE              0x000020
#define HTP_REQUEST_SMUGGLING           0x000040
#define HTP_INVALID_FOLDING             0x000080
#define HTP_INVALID_CHUNKING            0x000100
#define HTP_MULTI_PACKET_HEAD           0x000200
#define HTP_HOST_MISSING                0x000400
#define HTP_AMBIGUOUS_HOST              0x000800
#define HTP_PATH_ENCODED_NUL            0x001000
#define HTP_PATH_INVALID_ENCODING       0x002000
#define HTP_PATH_INVALID                0x004000
#define HTP_PATH_OVERLONG_U             0x008000
#define HTP_PATH_ENCODED_SEPARATOR      0x010000

#define HTP_PATH_UTF8_VALID             0x020000 /* At least one valid UTF-8 character and no invalid ones */
#define HTP_PATH_UTF8_INVALID           0x040000
#define HTP_PATH_UTF8_OVERLONG          0x080000
#define HTP_PATH_FULLWIDTH_EVASION      0x100000 /* Range U+FF00 - U+FFFF detected */

#define HTP_STATUS_LINE_INVALID         0x200000

#define HTP_PIPELINED_CONNECTION    1

#define HTP_SERVER_MINIMAL          0
#define HTP_SERVER_GENERIC          1
#define HTP_SERVER_IDS              2
#define HTP_SERVER_IIS_4_0          4   /* Windows NT 4.0 */
#define HTP_SERVER_IIS_5_0          5   /* Windows 2000 */
#define HTP_SERVER_IIS_5_1          6   /* Windows XP Professional */
#define HTP_SERVER_IIS_6_0          7   /* Windows 2003 */
#define HTP_SERVER_IIS_7_0          8   /* Windows 2008 */
#define HTP_SERVER_IIS_7_5          9   /* Windows 7 */
#define HTP_SERVER_TOMCAT_6_0       10  /* Unused */
#define HTP_SERVER_APACHE           11
#define HTP_SERVER_APACHE_2_2       12

#define HTP_CODING_NO_BODY          -1
#define HTP_CODING_UNKNOWN          0
#define HTP_CODING_IDENTITY         1
#define HTP_CODING_CHUNKED          2

#define TX_PROGRESS_NEW             0
#define TX_PROGRESS_REQ_LINE        1
#define TX_PROGRESS_REQ_HEADERS     2
#define TX_PROGRESS_REQ_BODY        3
#define TX_PROGRESS_REQ_TRAILER     4
#define TX_PROGRESS_WAIT            5
#define TX_PROGRESS_RES_LINE        6
#define TX_PROGRESS_RES_HEADERS     7
#define TX_PROGRESS_RES_BODY        8
#define TX_PROGRESS_RES_TRAILER     9
#define TX_PROGRESS_DONE            10

#define STREAM_STATE_NEW            0
#define STREAM_STATE_OPEN           1
#define STREAM_STATE_CLOSED         2
#define STREAM_STATE_ERROR          3
#define STREAM_STATE_TUNNEL         4
#define STREAM_STATE_DATA_OTHER     5
#define STREAM_STATE_STOP           6
#define STREAM_STATE_DATA           9

#define URL_DECODER_PRESERVE_PERCENT            0
#define URL_DECODER_REMOVE_PERCENT              1
#define URL_DECODER_DECODE_INVALID              2
#define URL_DECODER_STATUS_400                  400

#define NONE        0
#define NO          0
#define BESTFIT     0
#define YES         1
#define TERMINATE   1
#define STATUS_400  400
#define STATUS_404  401

#define HTP_AUTH_NONE       0
#define HTP_AUTH_BASIC      1
#define HTP_AUTH_DIGEST     2
#define HTP_AUTH_UNKNOWN    9

#define HTP_FILE_MULTIPART  1
#define HTP_FILE_PUT        2

#define CFG_NOT_SHARED  0
#define CFG_SHARED      1

#ifdef __cplusplus
extern "C" {
#endif

// -- Data structures -----------------------------------------------------------------------------

struct htp_connp_t {
    // General fields
    
    /** Current parser configuration structure. */
    htp_cfg_t *cfg;   

    /** The connection structure associated with this parser. */
    htp_conn_t *conn;

    /** Opaque user data associated with this parser. */
    void *user_data;   

    /** On parser failure, this field will contain the error information. Do note, however,
     *  that the value in this field will only be valid immediately after an error condition,
     *  but it is not guaranteed to remain valid if the parser is invoked again.
     */
    htp_log_t *last_error;

    // Request parser fields

    /** Parser inbound status. Starts as HTP_OK, but may turn into HTP_ERROR. */
    unsigned int in_status;

    /** Parser output status. Starts as HTP_OK, but may turn into HTP_ERROR. */
    unsigned int out_status;
    
    unsigned int out_data_other_at_tx_end;

    /** The time when the last request data chunk was received. Can be NULL. */
    htp_time_t in_timestamp;

    /** Pointer to the current request data chunk. */
    unsigned char *in_current_data;

    /** The length of the current request data chunk. */
    int64_t in_current_len;

    /** The offset of the next byte in the request data chunk to consume. */
    int64_t in_current_offset;

    /** How many data chunks does the inbound connection stream consist of? */
    size_t in_chunk_count;

    /** The index of the first chunk used in the current request. */
    size_t in_chunk_request_index;

    /** The offset, in the entire connection stream, of the next request byte. */
    int64_t in_stream_offset;

    /** The value of the request byte currently being processed. */
    int in_next_byte;

    /** Pointer to the request line buffer. */
    unsigned char *in_line;

    /** Size of the request line buffer. */
    size_t in_line_size;

    /** Length of the current request line. */
    size_t in_line_len;    

    /** Ongoing inbound transaction. */
    htp_tx_t *in_tx;   

    /** The request header line currently being processed. */
    htp_header_line_t *in_header_line;

    /** The index, in the structure holding all request header lines, of the
     *  line with which the current header begins. The header lines are
     *  kept in the transaction structure.
     */
    int in_header_line_index;

    /** How many lines are there in the current request header? */
    int in_header_line_counter;

    /**
     * The request body length declared in a valid request headers. The key here
     * is "valid". This field will not be populated if a request contains both
     * a Transfer-Encoding header and a Content-Length header.
     */
    int64_t in_content_length;

    /** Holds the remaining request body length that we expect to read. This
     *  field will be available only when the length of a request body is known
     *  in advance, i.e. when request headers contain a Content-Length header.
     */
    int64_t in_body_data_left;

    /** Holds the amount of data that needs to be read from the
     *  current data chunk. Only used with chunked request bodies.
     */
    int in_chunked_length;

    /** Current request parser state. */
    int (*in_state)(htp_connp_t *);

    // Response parser fields

    /** Response counter, incremented with every new response. This field is
     *  used to match responses to requests. The expectation is that for every
     *  response there will already be a transaction (request) waiting.
     */
    size_t out_next_tx_index;

    /** The time when the last response data chunk was received. Can be NULL. */
    htp_time_t out_timestamp;

    /** Pointer to the current response data chunk. */
    unsigned char *out_current_data;

    /** The length of the current response data chunk. */
    int64_t out_current_len;

    /** The offset of the next byte in the response data chunk to consume. */
    int64_t out_current_offset;

    /** The offset, in the entire connection stream, of the next response byte. */
    int64_t out_stream_offset;

    /** The value of the response byte currently being processed. */
    int out_next_byte;

    /** Pointer to the response line buffer. */
    unsigned char *out_line;

    /** Size of the response line buffer. */
    size_t out_line_size;

    /** Length of the current response line. */
    size_t out_line_len;       
        
    /** Ongoing outbound transaction */
    htp_tx_t *out_tx;

    /** The response header line currently being processed. */
    htp_header_line_t *out_header_line;

    /** The index, in the structure holding all response header lines, of the
     *  line with which the current header begins. The header lines are
     *  kept in the transaction structure.
     */
    int out_header_line_index;

    /** How many lines are there in the current response header? */
    int out_header_line_counter;

    /**
     * The length of the current response body as presented in the
     * Content-Length response header.
     */
    int64_t out_content_length;

    /** The remaining length of the current response body, if known. */
    int64_t out_body_data_left;

    /** Holds the amount of data that needs to be read from the
     *  current response data chunk. Only used with chunked response bodies.
     */
    int out_chunked_length;

    /** Current response parser state. */
    int (*out_state)(htp_connp_t *);

    /** Response decompressor used to decompress response body data. */
    htp_decompressor_t *out_decompressor;

    htp_file_t *put_file;
};

struct htp_file_t {
    /** Where did this file come from? */
    int source;

    /** File name. */
    bstr *filename;   

    /** Current file length. */
    size_t len;

    /** The unique filename in which this file is stored. */
    char *tmpname;

    /** The file descriptor that is used for the external storage. */
    int fd;
};

struct htp_file_data_t {
    /** File information. */
    htp_file_t *file;

    /** Pointer to the data buffer. */
    const unsigned char *data;

    /** Buffer length. */
    size_t len;
};

struct htp_log_t {
    /** The connection parser associated with this log message. */
    htp_connp_t *connp;

    /** The transaction associated with this log message, if any. */
    htp_tx_t *tx;

    /** Log message. */
    const char *msg;

    /** Message level. */
    int level;

    /** Message code. */
    int code;

    /** File in which the code that emitted the message resides. */
    const char *file;

    /** Line number on which the code that emitted the message resides. */
    unsigned int line;
};

struct htp_header_line_t {
    /** Header line data. */
    bstr *line;

    /** Offset at which header name begins, if applicable. */
    size_t name_offset;

    /** Header name length, if applicable. */
    size_t name_len;

    /** Offset at which header value begins, if applicable. */
    size_t value_offset;

    /** Value length, if applicable. */
    size_t value_len;

    /** How many NUL bytes are there on this header line? */
    unsigned int has_nulls;

    /** The offset of the first NUL byte, or -1. */
    int first_nul_offset;

    /** Parsing flags: HTP_FIELD_INVALID, HTP_FIELD_LONG, HTP_FIELD_NUL_BYTE,
     *                 HTP_FIELD_REPEATED, HTP_FIELD_FOLDED */
    unsigned int flags;
    
    /** Header that uses this line. */
    htp_header_t *header;
};

struct htp_header_t {
    /** Header name. */
    bstr *name;

    /** Header value. */
    bstr *value;   

    /** Parsing flags: HTP_FIELD_INVALID, HTP_FIELD_FOLDED, HTP_FIELD_REPEATED */
    unsigned int flags;
};

struct htp_tx_t {
    /** The connection parsed associated with this transaction. */
    htp_connp_t *connp;

    /** The connection to which this transaction belongs. */
    htp_conn_t *conn;

    /** The configuration structure associated with this transaction. */
    htp_cfg_t *cfg;

    /** Is the configuration structure shared with other transactions or connections? As
     *  a rule of thumb transactions will initially share their configuration structure, but
     *  copy-on-write may be used when an attempt to modify configuration is detected.
     */
    int is_cfg_shared;

    /** The user data associated with this transaction. */
    void *user_data;
    
    // Request
    unsigned int request_ignored_lines;

    /** The first line of this request. */
    bstr *request_line;

    /** The first line of this request including ws+line terminator(s). */
    bstr *request_line_raw;

    /** How many NUL bytes are there in the request line? */
    int request_line_nul;

    /** The offset of the first NUL byte. */
    int request_line_nul_offset;

    /** Request method. */
    bstr *request_method;

    /** Request method, as number. Available only if we were able to recognize the request method. */
    int request_method_number;

    /** Request URI, raw, as given to us on the request line. */
    bstr *request_uri;

    /**
     * Normalized request URI as a single string. The availability of this
     * field depends on configuration. Use htp_config_set_generate_request_uri_normalized()
     * to ask for the field to be generated.
     */
    bstr *request_uri_normalized;

    /** Request protocol, as text. */
    bstr *request_protocol;

    /** Protocol version as a number: -1 means unknown, 9 (HTTP_0_9) means 0.9,
     *  100 (HTTP_1_0) means 1.0 and 101 (HTTP_1_1) means 1.1.
     */
    int request_protocol_number;

    /** Is this request using a short-style HTTP/0.9 request? */
    int protocol_is_simple;

    /** This structure holds a parsed request_uri, with the missing information
     *  added (e.g., adding port number from the TCP information) and the fields
     *  normalized. This structure should be used to make decisions about a request.
     *  To inspect raw data, either use request_uri, or parsed_uri_incomplete.
     */
    htp_uri_t *parsed_uri;

    /** This structure holds the individual components parsed out of the request URI. No
     *  attempt is made to normalize the contents or replace the missing pieces with
     *  defaults. The purpose of this field is to allow you to look at the data as it
     *  was supplied. Use parsed_uri when you need to act on data. Note that this field
     *  will never have the port as a number.
     */
    htp_uri_t *parsed_uri_incomplete;
    
    /* HTTP 1.1 RFC
     * 
     * 4.3 Message Body
     * 
     * The message-body (if any) of an HTTP message is used to carry the
     * entity-body associated with the request or response. The message-body
     * differs from the entity-body only when a transfer-coding has been
     * applied, as indicated by the Transfer-Encoding header field (section
     * 14.41).
     *
     *     message-body = entity-body
     *                  | <entity-body encoded as per Transfer-Encoding>
     */

    /** The length of the request message-body. In most cases, this value
     *  will be the same as request_entity_len. The values will be different
     *  if request compression or chunking were applied. In that case,
     *  request_message_len contains the length of the request body as it
     *  has been seen over TCP; request_entity_len contains length after
     *  de-chunking and decompression.
     */
    size_t request_message_len;

    /** The length of the request entity-body. In most cases, this value
     *  will be the same as request_message_len. The values will be different
     *  if request compression or chunking were applied. In that case,
     *  request_message_len contains the length of the request body as it
     *  has been seen over TCP; request_entity_len contains length after
     *  de-chunking and decompression.
     */
    size_t request_entity_len;

    /** TODO The length of the data transmitted in a request body, minus the length
     *  of the files (if any). At worst, this field will be equal to the entity
     *  length if the entity encoding is not recognized. If we recognise the encoding
     *  (e.g., if it is application/x-www-form-urlencoded or multipart/form-data), the
     *  decoder may be able to separate the data from everything else, in which case
     *  the value in this field will be lower.
     */
    size_t request_nonfiledata_len;

    /** TODO The length of the files uploaded using multipart/form-data, or in a
     *  request that uses PUT (in which case this field will be equal to the
     *  entity length field). This field will be zero in all other cases.
     */
    size_t request_filedata_len;        

    /** Original request header lines. This list stores instances of htp_header_line_t. */
    htp_list_t *request_header_lines;

    /** How many request headers were there before trailers? */
    size_t request_header_lines_no_trailers;

    /** Parsed request headers. */
    htp_table_t *request_headers;

    /** Contains raw request headers. This field is generated on demand, use
     *  htp_tx_get_request_headers_raw() to get it.
     */
    bstr *request_headers_raw;

    /** How many request header lines have been included in the raw
     *  buffer (above).
     */
    size_t request_headers_raw_lines;

    /** Contains request header separator. */
    bstr *request_headers_sep;

    /** Request transfer coding. Can be one of HTP_CODING_UNKNOWN (body presence not
     *  determined yet), HTP_CODING_IDENTITY, HTP_CODING_CHUNKED, or HTP_CODING_NO_BODY.
     */
    int request_transfer_coding;

    /** Compression: COMPRESSION_NONE, COMPRESSION_GZIP or COMPRESSION_DEFLATE. */
    int request_content_encoding;

    /** This field will contain the request content type when that information
     *  is available in request headers. The contents of the field will be converted
     *  to lowercase and any parameters (e.g., character set information) removed.
     */
    bstr *request_content_type;

    /** Contains the value specified in the Content-Length header. Will be NULL
     *  if the header was not supplied.
     */
    size_t request_content_length;

    /** Transaction-specific REQUEST_BODY_DATA hook. Behaves as
     *  the configuration hook with the same name.
     */
    htp_hook_t *hook_request_body_data;

    /** Transaction-specific RESPONSE_BODY_DATA hook. Behaves as
     *  the configuration hook with the same name.
     */
    htp_hook_t *hook_response_body_data;

    /** Query string URLENCODED parser. Available only
     *  when the query string is not NULL and not empty.
      */
    htp_urlenp_t *request_urlenp_query;

    /** Request body URLENCODED parser. Available only when
     *  the request body is in the application/x-www-form-urlencoded format.
     */
    htp_urlenp_t *request_urlenp_body;

    /** Request body MULTIPART parser. Available only when the
     *  body is in the multipart/form-data format and when the parser
     *  was invoked in configuration.
     */
    htp_mpartp_t *request_mpartp;

    /** Parameters from the query string. */
    htp_table_t *request_params_query;
    int request_params_query_reused;

    /** Parameters from request body. */
    htp_table_t *request_params_body;
    int request_params_body_reused;

    /** Request cookies */
    htp_table_t *request_cookies;

    int request_auth_type;
    bstr *request_auth_username;
    bstr *request_auth_password;

    // Response

    /** How many empty lines did we ignore before reaching the status line? */
    unsigned int response_ignored_lines;

    /** Response line. */
    bstr *response_line;

    /** Response line including ws+line terminator(s). */
    bstr *response_line_raw;

    /** Response protocol, as text. */
    bstr *response_protocol;

    /** Response protocol as number. Only available if we were
     *  able to parse the protocol version.
     */
    int response_protocol_number;

    /** Response status code, as text. */
    bstr *response_status;

    /** Response status code, available only if we were able to parse it. */
    int response_status_number;

    /** This field is set by the protocol decoder with it thinks that the
     *  backend server will reject a request with a particular status code.
     */
    int response_status_expected_number;

    /** The message associated with the response status code. */
    bstr *response_message;

    /** Have we seen the server respond with a 100 response? */
    int seen_100continue;   

    /** Original response header lines. */
    htp_list_t *response_header_lines;

    /** Parsed response headers. */
    htp_table_t *response_headers;

    /** Contains raw response headers. This field is generated on demand, use
     *  htp_tx_get_response_headers_raw() to get it.
     */
    bstr *response_headers_raw;

    /** How many response header lines have been included in the raw
      * buffer (above).
      */
    size_t response_headers_raw_lines;

    /** Contains response header separator. */
    bstr *response_headers_sep;

    /* HTTP 1.1 RFC
     * 
     * 4.3 Message Body
     * 
     * The message-body (if any) of an HTTP message is used to carry the
     * entity-body associated with the request or response. The message-body
     * differs from the entity-body only when a transfer-coding has been
     * applied, as indicated by the Transfer-Encoding header field (section
     * 14.41).
     *
     *     message-body = entity-body
     *                  | <entity-body encoded as per Transfer-Encoding>
     */

    /** The length of the response message-body. In most cases, this value
     *  will be the same as response_entity_len. The values will be different
     *  if response compression or chunking were applied. In that case,
     *  response_message_len contains the length of the response body as it
     *  has been seen over TCP; response_entity_len contains the length after
     *  de-chunking and decompression.
     */
    size_t response_message_len;

    /** The length of the response entity-body. In most cases, this value
     *  will be the same as response_message_len. The values will be different
     *  if request compression or chunking were applied. In that case,
     *  response_message_len contains the length of the response body as it
     *  has been seen over TCP; response_entity_len contains length after
     *  de-chunking and decompression.
     */
    size_t response_entity_len;
    
    /** Response transfer coding: IDENTITY or CHUNKED. Only available on responses that have bodies. */
    int response_transfer_coding;

    /** Compression; currently COMPRESSION_NONE or COMPRESSION_GZIP. */
    int response_content_encoding;   
    
    /** This field will contain the response content type when that information
     *  is available in response headers. The contents of the field will be converted
     *  to lowercase and any parameters (e.g., character set information) removed.
     */
    bstr *response_content_type;

    // Common

    /** Parsing flags: HTP_INVALID_CHUNKING, HTP_INVALID_FOLDING,
     *  HTP_REQUEST_SMUGGLING, HTP_MULTI_PACKET_HEAD, HTP_FIELD_UNPARSEABLE.
     */
    unsigned int flags;

    /** Transaction progress. Look for the TX_PROGRESS_* constants for more information. */
    unsigned int progress;
};

/** This structure is used to pass transaction data to callbacks. */
struct htp_tx_data_t {
    /** Transaction pointer. */
    htp_tx_t *tx;

    /** Pointer to the data buffer. */
    const unsigned char *data;

    /** Buffer length. */
    size_t len;
};

/** URI structure. Each of the fields provides access to a single
 *  URI element. A typical URI will look like this:
 *  http://username:password@hostname.com:8080/path?query#fragment.
 */
struct htp_uri_t {
    /** Scheme */
    bstr *scheme;

    /** Username */
    bstr *username;

    /** Password */
    bstr *password;

    /** Hostname */
    bstr *hostname;

    /** Port, as string */
    bstr *port;

    /** Port, as number, but only if the port is valid. */
    int port_number;

    /** The path part of this URI */
    bstr *path;

    /** Query string */
    bstr *query;

    /** Fragment identifier */
    bstr *fragment;
};

// -- Functions -----------------------------------------------------------------------------------

const char *htp_get_version(void);


#include "htp_config.h"
#include "htp_connection.h"


// Connection parser

htp_connp_t *htp_connp_create(htp_cfg_t *cfg);
void htp_connp_open(htp_connp_t *connp, const char *remote_addr, int remote_port, const char *local_addr, int local_port, htp_time_t *timestamp);
void htp_connp_close(htp_connp_t *connp, htp_time_t *timestamp);
void htp_connp_destroy(htp_connp_t *connp);
void htp_connp_destroy_all(htp_connp_t *connp);
void htp_connp_in_reset(htp_connp_t *connp);

 void htp_connp_set_user_data(htp_connp_t *connp, void *user_data);
void *htp_connp_get_user_data(htp_connp_t *connp);


   int htp_connp_req_data(htp_connp_t *connp, htp_time_t *timestamp, unsigned char *data, size_t len);
size_t htp_connp_req_data_consumed(htp_connp_t *connp);
   int htp_connp_res_data(htp_connp_t *connp, htp_time_t *timestamp, unsigned char *data, size_t len);
size_t htp_connp_res_data_consumed(htp_connp_t *connp);

      void htp_connp_clear_error(htp_connp_t *connp);
htp_log_t *htp_connp_get_last_error(htp_connp_t *connp);


// Transaction

htp_tx_t *htp_tx_create(htp_cfg_t *cfg, int is_cfg_shared, htp_conn_t *conn);
     void htp_tx_destroy(htp_tx_t *tx);
     void htp_tx_set_config(htp_tx_t *tx, htp_cfg_t *cfg, int is_cfg_shared);

     void htp_tx_set_user_data(htp_tx_t *tx, void *user_data);
    void *htp_tx_get_user_data(htp_tx_t *tx);
    
#ifdef __cplusplus
}
#endif

#endif	/* _HTP_H */
