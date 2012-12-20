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

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/time.h>

#include "htp_core.h"

#include "bstr.h"
#include "htp_config.h"
#include "htp_connection_parser.h"
#include "htp_decompressors.h"
#include "htp_hooks.h"
#include "htp_list.h"
#include "htp_multipart.h"
#include "htp_table.h"
#include "htp_transaction.h"
#include "htp_urlencoded.h"

// -- Defines -------------------------------------------------------------------------------------

#define HTP_FIELD_UNPARSEABLE               0x000001
#define HTP_FIELD_INVALID                   0x000002
#define HTP_FIELD_FOLDED                    0x000004
#define HTP_FIELD_REPEATED                  0x000008
#define HTP_FIELD_LONG                      0x000010
#define HTP_FIELD_NUL_BYTE                  0x000020
#define HTP_REQUEST_SMUGGLING               0x000040
#define HTP_INVALID_FOLDING                 0x000080
#define HTP_INVALID_CHUNKING                0x000100
#define HTP_MULTI_PACKET_HEAD               0x000200
#define HTP_HOST_MISSING                    0x000400
#define HTP_AMBIGUOUS_HOST                  0x000800
#define HTP_PATH_ENCODED_NUL                0x001000
#define HTP_PATH_INVALID_ENCODING           0x002000
#define HTP_PATH_INVALID                    0x004000
#define HTP_PATH_OVERLONG_U                 0x008000
#define HTP_PATH_ENCODED_SEPARATOR          0x010000
#define HTP_PATH_UTF8_VALID                 0x020000 /* At least one valid UTF-8 character and no invalid ones */
#define HTP_PATH_UTF8_INVALID               0x040000
#define HTP_PATH_UTF8_OVERLONG              0x080000
#define HTP_PATH_FULLWIDTH_EVASION          0x100000 /* Range U+FF00 - U+FFFF detected */
#define HTP_STATUS_LINE_INVALID             0x200000

#define HTP_PIPELINED_CONNECTION            1

/**
 * Enumerates the possible request and response body codings.
 */
enum htp_coding_t {
    /** Body coding not determined yet. */
    HTP_CODING_UNKNOWN = 0,

    /** No body. */
    HTP_CODING_NO_BODY = 1,

    /** Identity coding is used, which means that the body was sent as is. */
    HTP_CODING_IDENTITY = 2,

    /** Chunked encoding. */
    HTP_CODING_CHUNKED = 3,

    /** We could not recognize the encoding. */
    HTP_CODING_UNRECOGNIZED = 4
};

#define URL_DECODER_PRESERVE_PERCENT        0
#define URL_DECODER_REMOVE_PERCENT          1
#define URL_DECODER_DECODE_INVALID          2
#define URL_DECODER_STATUS_400              400

#define NONE                                0
#define NO                                  0
#define BESTFIT                             0
#define YES                                 1
#define TERMINATE                           1
#define STATUS_400                          400
#define STATUS_404                          401

/**
 * Enumerates the possible values for authentication type.
 */
enum htp_auth_type_t {
    /**
     * This is the default value that is used before
     * the presence of authentication is determined (e.g.,
     * before request headers are seen).
     */
    HTP_AUTH_UNKNOWN = 0,

    /** No authentication. */
    HTP_AUTH_NONE = 1,

    /** HTTP Basic authentication used. */
    HTP_AUTH_BASIC = 2,

    /** HTTP Digest authentication used. */
    HTP_AUTH_DIGEST = 3,

    /** Unrecognized authentication method. */
    HTP_AUTH_UNRECOGNIZED = 9
};


// -- Data structures -----------------------------------------------------------------------------

/**
 * This data structure is used to represent a single TCP connection.
 */
struct htp_conn_t {
    /** Remote IP address. */
    char *remote_addr;

    /** Remote port. */
    int remote_port;

    /** Local IP address. */
    char *local_addr;

    /** Local port. */
    int local_port;

    /**
     * Transactions carried out on this connection. The list may contain
     * NULL elements when some of the transactions are deleted (and then
     * removed from a connection by calling htp_conn_remove_tx().
     */
    htp_list_t *transactions;

    /** Log messages associated with this connection. */
    htp_list_t *messages;

    /** Parsing flags: PIPELINED_CONNECTION. */
    unsigned int flags;

    /** When was this connection opened? Can be NULL. */
    htp_time_t open_timestamp;

    /** When was this connection closed? Can be NULL. */
    htp_time_t close_timestamp;

    /** Inbound data counter. */
    size_t in_data_counter;

    /** Outbound data counter. */
    size_t out_data_counter;   
};

/**
 * Used to represent files that are seen during the processing of HTTP traffic. Most
 * commonly this refers to files seen in multipart/form-data payloads. In addition, PUT
 * request bodies can be treated as files.
 */
struct htp_file_t {
    /** Where did this file come from? Possible values: HTP_FILE_MULTIPART and HTP_FILE_PUT. */
    int source;

    /** File name, as provided (e.g., in the Content-Disposition multipart part header. */
    bstr *filename;   

    /** File length. */
    int64_t len;

    // TODO Content type is missing. This information is sometimes available in multipart payloads.

    /** The unique filename in which this file is stored on the filesystem, when applicable.*/
    char *tmpname;

    /** The file descriptor that is used for the external storage, when applicable. */
    int fd;
};

/**
 * Represents a chunk of file data.
 */
struct htp_file_data_t {
    /** File information. */
    htp_file_t *file;

    /** Pointer to the data buffer. */
    const unsigned char *data;

    /** Buffer length. */
    size_t len;
};

/**
 * Represents a single log entry.
 */
struct htp_log_t {
    /** The connection parser associated with this log message. */
    htp_connp_t *connp;

    /** The transaction associated with this log message, if any. */
    htp_tx_t *tx;

    /** Log message. */
    const char *msg;

    /** Message level. */
    enum htp_log_level_t level;

    /** Message code. */
    int code;

    /** File in which the code that emitted the message resides. */
    const char *file;

    /** Line number on which the code that emitted the message resides. */
    unsigned int line;
};

/**
 * Represents a single request or response header line. One header can span
 * many lines. Although applications care only about headers, at the moment
 * we also keep track of individual header lines. This will likely go away in
 * the near future, because no one really cares about it, yet storage takes
 * valuable resources.
 */
struct htp_header_line_t {
    /** Line contents. */
    bstr *line;

    /** Offset at which header name begins, if the line contains a header name. */
    size_t name_offset;

    /** Header name length, valid only if the line contains a header name. */
    size_t name_len;

    /** Offset at which header value begins, if the value begins on this line. */
    size_t value_offset;

    /** Header value length. */
    size_t value_len;

    /** How many NUL bytes are there in this header line? */
    unsigned int has_nulls;

    /** The offset of the first NUL byte, or -1. */
    int first_nul_offset;

    /**
     * Parsing flags; a combination of HTP_FIELD_INVALID, HTP_FIELD_LONG,
     * HTP_FIELD_NUL_BYTE, HTP_FIELD_REPEATED, and HTP_FIELD_FOLDED.
     */
    unsigned int flags;
    
    /** Header that uses this line. */
    htp_header_t *header;
};

/**
 * Represents a single request or response header.
 */
struct htp_header_t {
    /** Header name. */
    bstr *name;

    /** Header value. */
    bstr *value;   

    /** Parsing flags; a combination of: HTP_FIELD_INVALID, HTP_FIELD_FOLDED, HTP_FIELD_REPEATED. */
    unsigned int flags;
};

/**
 * Represents a single transaction, which is a combination of a request and a response.
 */
struct htp_tx_t {
    /** The connection parser associated with this transaction. */
    htp_connp_t *connp;

    /** The connection to which this transaction belongs. */
    htp_conn_t *conn;

    /** The configuration structure associated with this transaction. */
    htp_cfg_t *cfg;

    /**
     * Is the configuration structure shared with other transactions or connections? If
     * this field is set to HTP_CONFIG_PRIVATE, the transaction owns the configuration.
     */
    int is_config_shared;

    /** The user data associated with this transaction. */
    void *user_data;
    
    
    // Request fields

    /** TODO */
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
    enum htp_method_t request_method_number;

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

    /**
     * Protocol version as a number. Multiply the high version number by 100, then add the low
     * version number. You should prefer to work the pre-defined HTP_PROTOCOL_* constants.
     */
    int request_protocol_number;

    /** Is this request using HTTP/0.9? */
    int is_protocol_0_9;

    /**
     * This structure holds a parsed request_uri, with the missing information
     * added (e.g., adding port number from the TCP information) and the fields
     * normalized. This structure should be used to make decisions about a request.
     * To inspect raw data, either use request_uri, or parsed_uri_incomplete.
     */
    htp_uri_t *parsed_uri;

    /**
     * This structure holds the individual components parsed out of the request URI. No
     * attempt is made to normalize the contents or replace the missing pieces with
     * defaults. The purpose of this field is to allow you to look at the data as it
     * was supplied. Use parsed_uri when you need to act on data. Note that this field
     * will never have the port as a number.
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

    /**
     * The length of the request message-body. In most cases, this value
     * will be the same as request_entity_len. The values will be different
     * if request compression or chunking were applied. In that case,
     * request_message_len contains the length of the request body as it
     * has been seen over TCP; request_entity_len contains length after
     * de-chunking and decompression.
     */
    size_t request_message_len;

    /**
     * The length of the request entity-body. In most cases, this value
     * will be the same as request_message_len. The values will be different
     * if request compression or chunking were applied. In that case,
     * request_message_len contains the length of the request body as it
     * has been seen over TCP; request_entity_len contains length after
     * de-chunking and decompression.
     */
    size_t request_entity_len;

    /**
     * TODO The length of the data transmitted in a request body, minus the length
     * of the files (if any). At worst, this field will be equal to the entity
     * length if the entity encoding is not recognized. If we recognise the encoding
     * (e.g., if it is application/x-www-form-urlencoded or multipart/form-data), the
     * decoder may be able to separate the data from everything else, in which case
     * the value in this field will be lower.
     */
    size_t request_nonfiledata_len;

    /**
     * TODO The length of the files uploaded using multipart/form-data, or in a
     * request that uses PUT (in which case this field will be equal to the
     * entity length field). This field will be zero in all other cases.
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

    /**
     * Request transfer coding. Can be one of HTP_CODING_UNKNOWN (body presence not
     * determined yet), HTP_CODING_IDENTITY, HTP_CODING_CHUNKED, HTP_CODING_NO_BODY,
     * and HTP_CODING_UNRECOGNIZED.
     */
    enum htp_coding_t request_transfer_coding;

    /** Compression: COMPRESSION_NONE, COMPRESSION_GZIP or COMPRESSION_DEFLATE. */
    int request_content_encoding;

    /**
     * This field will contain the request content type when that information
     * is available in request headers. The contents of the field will be converted
     * to lowercase and any parameters (e.g., character set information) removed.
     */
    bstr *request_content_type;

    /**
     * Contains the value specified in the Content-Length header. Will be NULL
     * if the header was not supplied.
     */
    size_t request_content_length;

    /**
     * Transaction-specific REQUEST_BODY_DATA hook. Behaves as
     * the configuration hook with the same name.
     */
    htp_hook_t *hook_request_body_data;

    /**
     * Transaction-specific RESPONSE_BODY_DATA hook. Behaves as
     * the configuration hook with the same name.
     */
    htp_hook_t *hook_response_body_data;

    /**
     * Query string URLENCODED parser. Available only
     * when the query string is not NULL and not empty.
     */
    htp_urlenp_t *request_urlenp_query;

    /**
     * Request body URLENCODED parser. Available only when the request body is in the
     * application/x-www-form-urlencoded format and the parser was configured to run.
     */
    htp_urlenp_t *request_urlenp_body;

    /**
     * Request body MULTIPART parser. Available only when the body is in the
     * multipart/form-data format and the parser was configured to run.
     */
    htp_mpartp_t *request_mpartp;

    /** Parameters from the query string. */
    htp_table_t *request_params_query;

    /** TODO */
    int request_params_query_reused;

    /** Parameters from request body. */
    htp_table_t *request_params_body;

    /** TODO */
    int request_params_body_reused;

    /** Request cookies */
    htp_table_t *request_cookies;

    /** TODO */
    enum htp_auth_type_t request_auth_type;

    /** TODO */
    bstr *request_auth_username;

    /** TODO */
    bstr *request_auth_password;


    // Response fields

    /** How many empty lines did we ignore before reaching the status line? */
    unsigned int response_ignored_lines;

    /** Response line. */
    bstr *response_line;

    /** Response line including ws+line terminator(s). */
    bstr *response_line_raw;

    /** Response protocol, as text. */
    bstr *response_protocol;

    /**
     * Response protocol as number. Only available if we were
     * able to parse the protocol version.
     */
    int response_protocol_number;

    /** Response status code, as text. */
    bstr *response_status;

    /** Response status code, available only if we were able to parse it. */
    int response_status_number;

    /**
     * This field is set by the protocol decoder with it thinks that the
     * backend server will reject a request with a particular status code.
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

    /**
     * Contains raw response headers. This field is generated on demand, use
     * htp_tx_get_response_headers_raw() to get it.
     */
    bstr *response_headers_raw;

    /**
     * How many response header lines have been included in the raw
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

    /**
     * The length of the response message-body. In most cases, this value
     * will be the same as response_entity_len. The values will be different
     * if response compression or chunking were applied. In that case,
     * response_message_len contains the length of the response body as it
     * has been seen over TCP; response_entity_len contains the length after
     * de-chunking and decompression.
     */
    size_t response_message_len;

    /**
     * The length of the response entity-body. In most cases, this value
     * will be the same as response_message_len. The values will be different
     * if request compression or chunking were applied. In that case,
     * response_message_len contains the length of the response body as it
     * has been seen over TCP; response_entity_len contains length after
     * de-chunking and decompression.
     */
    size_t response_entity_len;
    
    /**
     * Response transfer coding. Can be one of HTP_CODING_UNKNOWN (body presence not
     * determined yet), HTP_CODING_IDENTITY, HTP_CODING_CHUNKED, HTP_CODING_NO_BODY,
     * and HTP_CODING_UNRECOGNIZED.
     */
    enum htp_coding_t response_transfer_coding;

    /** Compression; currently COMPRESSION_NONE or COMPRESSION_GZIP. */
    int response_content_encoding;   
    
    /**
     * This field will contain the response content type when that information
     * is available in response headers. The contents of the field will be converted
     * to lowercase and any parameters (e.g., character set information) removed.
     */
    bstr *response_content_type;

    
    // Common fields

    /**
     * Parsing flags; a combination of: HTP_INVALID_CHUNKING, HTP_INVALID_FOLDING,
     * HTP_REQUEST_SMUGGLING, HTP_MULTI_PACKET_HEAD, and HTP_FIELD_UNPARSEABLE.
     */
    unsigned int flags;

    /** Transaction progress. */
    enum htp_tx_progress_t progress;
};

/**
 * This structure is used to pass transaction data (for example
 * request and response body buffers) to callbacks.
 */
struct htp_tx_data_t {
    /** Transaction pointer. */
    htp_tx_t *tx;

    /** Pointer to the data buffer. */
    const unsigned char *data;

    /** Buffer length. */
    size_t len;
};

/**
 * URI structure. Each of the fields provides access to a single
 * URI element. Where an element is not present in a URI, the
 * corresponding field will be set to NULL or -1, depending on the
 * field type.
 */
struct htp_uri_t {
    /** Scheme, e.g., "http". */
    bstr *scheme;

    /** Username. */
    bstr *username;

    /** Password. */
    bstr *password;

    /** Hostname. */
    bstr *hostname;

    /** Port, as string. */
    bstr *port;

    /**
     * Port, as number. This field will contain HTP_PORT_NONE if there was
     * no port information in the URI and HTP_PORT_INVALID if the port information
     * was invalid (e.g., it's not a number or it falls out of range.
     */
    int port_number;

    /** The path part of this URI. */
    bstr *path;

    /** Query string. */
    bstr *query;

    /**
     * Fragment identifier. This field will rarely be available in a server-side
     * setting, but it's not impossible to see it. */
    bstr *fragment;
};

/**
 * Creates a new log entry and stores it with the connection. The file and line
 * parameters are typically auto-generated using the HTP_LOG_MARK macro.
*
 * @param[in] connp
 * @param[in] file
 * @param[in] line
 * @param[in] level
 * @param[in] code
 * @param[in] fmt
 * @param[in] ...
 */
void htp_log(htp_connp_t *connp, const char *file, int line, enum htp_log_level_t level, int code, const char *fmt, ...);
    
#ifdef __cplusplus
}
#endif

#endif	/* _HTP_H */
