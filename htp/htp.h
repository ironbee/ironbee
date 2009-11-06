
#ifndef _HTP_H
#define	_HTP_H

#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "bstr.h"
#include "dslib.h"
#include "hooks.h"


// TODO We're currently using a fixed-size line buffer. Although one buffer isn't very big,
//      they add up if we want to supports tens of thousands of concurrent connections. For
//      example 20K connections x 20K buffer = 400M. We can perhaps start with a 2K buffer
//      (configurable) and grow as needed.

// TODO Memory allocation strategies. We want to support two strategies:
//
//      #1 Supply a pair of functions (alloc and free) along with a void * pointer.
//
//      #2 Use memory pools for all allocations. Desired functions:
//
//          - create pool (w/hierarchy), destroy pool, clear pool
//          - alloc (calloc?), free
//          - register callback
//
//      The plan is to have a simple memory pool implementation that does not pool memory
//      but only tracks what is allocated so that it can free it all in one go. The library
//      users can provide an external implementation to use if they so wish.

// TODO Consider enums where appropriate.

// TODO The plan for SSL handling is as follows:
//
//      - For fully encrypted streams, upstream is free to decrypt SSL and feed the
//        parser just the data.
//
//      - On-demand SSL is not used with HTTP in practice but, in principle, the idea
//        is to have the parser return the HTP_TLS_UPGRADE code. Upon detecting the
//        code, upstream would handle the upgrade (either by passively decrypting the
//        traffic stream or handling SSL/TLS directly) and provide plain text data
//        to the HTTP parser on every subsequent invocation.
//




// -- Defines -------------------------------------------------------------------------------------

#define HTP_DEBUG               1

#define HTP_ERROR              -1
#define HTP_OK                  0
#define HTP_DATA                1
#define HTP_DECLINED            2

#define PROTOCOL_UNKNOWN        -1
#define HTTP_0_9                9
#define HTTP_1_0                100
#define HTTP_1_1                101

#define LOG_MARK                __FILE__,__LINE__

#define LOG_ERROR               1
#define LOG_WARNING             2
#define LOG_NOTICE              3
#define LOG_INFO                4
#define LOG_DEBUG               5
#define LOG_DEBUG2              6

#define HTP_HEADER_MISSING_COLON            1
#define HTP_HEADER_INVALID_NAME             2
#define HTP_HEADER_LWS_AFTER_FIELD_NAME     3
#define HTP_LINE_TOO_LONG_HARD              4
#define HTP_LINE_TOO_LONG_SOFT              5

#define HTP_HEADER_LIMIT_HARD               18000
#define HTP_HEADER_LIMIT_SOFT               9000

#define LOG_NO_CODE             0

#define CR '\r'
#define LF '\n'

// TODO Document source for each method
#define M_UNKNOWN              -1

// The following are defined in Apache 2.2.13, in httpd.h.
#define M_GET                   0
#define M_PUT                   1
#define M_POST                  2
#define M_DELETE                3
#define M_CONNECT               4
#define M_OPTIONS               5
#define M_TRACE                 6
#define M_PATCH                 7
#define M_PROPFIND              8
#define M_PROPPATCH             9
#define M_MKCOL                 10
#define M_COPY                  11
#define M_MOVE                  12
#define M_LOCK                  13
#define M_UNLOCK                14
#define M_VERSION_CONTROL       15
#define M_CHECKOUT              16
#define M_UNCHECKOUT            17
#define M_CHECKIN               18
#define M_UPDATE                19
#define M_LABEL                 20
#define M_REPORT                21
#define M_MKWORKSPACE           22
#define M_MKACTIVITY            23
#define M_BASELINE_CONTROL      24
#define M_MERGE                 25
#define M_INVALID               26

// Interestingly, Apache does not define M_HEAD
#define M_HEAD                  1000

#define HTP_FIELD_UNPARSEABLE           1
#define HTP_FIELD_INVALID               2
#define HTP_FIELD_FOLDED                4
#define HTP_FIELD_REPEATED              8
#define HTP_FIELD_LONG                  16
#define HTP_FIELD_NUL_BYTE              32
#define HTP_REQUEST_SMUGGLING           64
#define HTP_INVALID_FOLDING             128
#define HTP_INVALID_CHUNKING            256
#define HTP_MULTI_PACKET_HEAD           512

#define PIPELINED_CONNECTION    1

#define HTP_SERVER_STRICT           0
#define HTP_SERVER_PERMISSIVE       1
#define HTP_SERVER_APACHE_2_2       2
#define HTP_SERVER_IIS_5_1          3
#define HTP_SERVER_IIS_7_5          4

#define NONE                        0
#define IDENTITY                    1
#define CHUNKED                     2

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

// TODO At some point test the performance of these macros and
//      determine if it makes more sense to implement the same
//      functionality as functions

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
        htp_log((X), LOG_MARK, LOG_ERROR, HTP_LINE_TOO_LONG_SOFT, "Request field over soft limit"); \
    } \
} else { \
    htp_log((X), LOG_MARK, LOG_ERROR, HTP_LINE_TOO_LONG_HARD, "Request field over hard limit"); \
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
        htp_log((X), LOG_MARK, LOG_ERROR, HTP_LINE_TOO_LONG_SOFT, "Response field over soft limit"); \
    } \
} else { \
    htp_log((X), LOG_MARK, LOG_ERROR, HTP_LINE_TOO_LONG_HARD, "Response field over hard limit"); \
    return HTP_ERROR; \
}

typedef uint32_t htp_time_t;

// -- Data structures -----------------------------------------------------------------------------

typedef struct htp_cfg_t htp_cfg_t;
typedef struct htp_conn_t htp_conn_t;
typedef struct htp_connp_t htp_connp_t;
typedef struct htp_header_t htp_header_t;
typedef struct htp_header_line_t htp_header_line_t;
typedef struct htp_log_t htp_log_t;
typedef struct htp_tx_data_t htp_tx_data_t;
typedef struct htp_tx_t htp_tx_t;
typedef struct htp_uri_t htp_uri_t;

struct htp_cfg_t {    
    size_t field_limit_hard;
    // TODO The soft limit here relates to line length, not header (field) length. When
    //      folding is used a field can be constructed from several lines, breaking the
    //      soft limit.
    size_t field_limit_soft;
    // TODO Message headers soft limit
    // TODO Message headers hard limit

    // TODO Do we want to limit the size of the request headers part?

    // TODO Option to detect evasion using request chunked encoding
    // TODO Soft chunk length limit/flag

    int log_level;   

    int spersonality;

    int (*parse_request_line)(htp_connp_t *connp);
    int (*parse_response_line)(htp_connp_t *connp);
    int (*process_request_header)(htp_connp_t *connp);
    int (*process_response_header)(htp_connp_t *connp);   

    // TODO There will be two types of hook: connection and transaction hooks. If we want to allow
    //      a hook to disconnect itself (as we should) then we need to make sure the disconnect is
    //      applied to the correct scope. For example, a transaction hook that requires disconnection
    //      should not be invoked for the same transaction, but should be invoked for the subsequent
    //      transaction. This tells me that we need to keep a prototype of transaction hooks and to
    //      make a copy of it whenever a new transaction begins.

    htp_hook_t *hook_transaction_start;
    htp_hook_t *hook_request_line;
    htp_hook_t *hook_request_headers;
    htp_hook_t *hook_request_body_data;
    htp_hook_t *hook_request_trailer;    
    htp_hook_t *hook_request;

    htp_hook_t *hook_response_line;
    htp_hook_t *hook_response_headers;
    htp_hook_t *hook_response_body_data;
    htp_hook_t *hook_response_trailer;
    htp_hook_t *hook_response;

    void *user_data;
};

struct htp_conn_t {
    /** Connection parser associated with this connection. */
    htp_connp_t *connp;

    /** Remote IP address. */
    const char *remote_addr;

    /** Remote port. */
    int remote_port;

    /** Transactions carried out on this connection. */
    list_t *transactions;

    /** Log messages associated with this connection. */
    list_t *messages;   

    /** Parsing flags: PIPELINED_CONNECTION. */
    unsigned int flags;
    
    // TODO data counters (before and after SSL?)
};

struct htp_connp_t {
    // General fields
    
    /** Current parser configuration structure. */
    htp_cfg_t *cfg;

    /** The connection structure associated with this parser. */
    htp_conn_t *conn;

    /** Opaque user data associated with this parser. */
    void *user_data;   

    /** On parser failure, this field will contain the error information. */
    htp_log_t *last_error;

    // Request parser fields

    /** Parser status. Starts as HTP_OK, but may turn into HTP_ERROR. */
    unsigned int status; // TODO Consider using two status fields, one for each direction

    /** The time when the last request data chunk was received. */
    htp_time_t in_timestamp;

    /** Pointer to the current request data chunk. */
    unsigned char *in_current_data;

    /** The length of the current request data chunk. */
    size_t in_current_len;

    /** The offset of the next byte in the request data chunk to consume. */
    size_t in_current_offset;

    /** How many data chunks does the inbound connection stream consist of? */
    size_t in_chunk_count;

    /** The index of the first chunk used in the current request. */
    size_t in_chunk_request_index;

    /** The offset, in the entire connection stream, of the next request byte. */
    size_t in_stream_offset;

    /** The value of the request byte currently being processed. */
    int in_next_byte;

    /** Pointer to the request line buffer. */
    unsigned char *in_line;

    /** Size of the request line buffer. */
    size_t in_line_size;

    /** Lenght of the current request line. */
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
     * a Transfer-Encoding header and a Content-Lenght header.
     */
    size_t in_content_length;

    /** Holds the remaining request body length that we expect to read. This
     *  field will be available only when the length of a request body is known
     *  in advance, i.e. when request headers contain a Content-Length header.
     */
    size_t in_body_data_left;

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
    int out_next_tx_index;

    /** The time when the last response data chunk was received. */
    htp_time_t out_timestamp;

    /** Pointer to the current response data chunk. */
    unsigned char *out_current_data;

    /** The length of the current response data chunk. */
    size_t out_current_len;

    /** The offset of the next byte in the response data chunk to consume. */
    size_t out_current_offset;

    /** The offset, in the entire connection stream, of the next response byte. */
    size_t out_stream_offset;

    /** The value of the response byte currently being processed. */
    int out_next_byte;

    /** Pointer to the response line buffer. */
    unsigned char *out_line;

    /** Size of the response line buffer. */
    size_t out_line_size;

    /** Lenght of the current response line. */
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
    size_t out_content_length;

    /** The remaining length of the current response body, if known. */
    size_t out_body_data_left;

    /** Holds the amount of data that needs to be read from the
     *  current response data chunk. Only used with chunked response bodies.
     */
    int out_chunked_length;

    /** Current response parser state. */
    int (*out_state)(htp_connp_t *);
};

struct htp_log_t {
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

    /** Parsing flags: HTP_FIELD_INVALID_NOT_FATAL, HTP_FIELD_INVALID_FATAL, HTP_FIELD_LONG */
    unsigned int flags;
    
    /** Header that uses this line. */
    htp_header_t *header;
};

struct htp_header_t {
    /** Header name. */
    bstr *name;

    /** Header value. */
    bstr *value;   

    /** Parsing flags: HTP_FIELD_INVALID_NOT_FATAL, HTP_FIELD_FOLDED, HTP_FIELD_REPEATED */
    unsigned int flags;
};

struct htp_tx_t {
    /** The connection to which this transaction belongs. */
    htp_conn_t *conn;

    /** The configuration structure associated with this transaction. */
    htp_cfg_t *cfg;

    /** Is the configuration structure shared with other transactions or connections? */
    int is_cfg_shared;

    /** The user data associated with this transaction. */
    void *user_data;
    
    // Request
    unsigned int request_ignored_lines;

    /** The first line of this request. */
    bstr *request_line;

    /** How many NUL bytes are there in the request line? */
    int request_line_nul;

    /** The offset of the first NUL byte. */
    int request_line_nul_offset;

    /** Request method. */
    bstr *request_method;

    /** Request method, as number. Available only if we were able to recognize the request method. */
    int request_method_number;

    /** Request URI. */
    bstr *request_uri;

    /** Request protocol, as text. */
    bstr *request_protocol;

    /** Protocol version as a number: -1 means unknown, 9 (HTTP_0_9) means 0.9,
     *  100 (HTTP_1_0) means 1.0 and 101 (HTTP_1_1) means 1.1.
     */
    int request_protocol_number;

    /** Is this request using a short-style HTTP/0.9 request? */
    int protocol_is_simple;

    // htp_uri_t *parsed_uri;
    htp_uri_t *parsed_uri_incomplete;

    /** Request query string. This field is an alias for parsed_uri_incomplete.query. */
    bstr *query_string;        

    /** The actual message length (the length _after_ transformations
     *  have been applied). This field will change as a request body is being
     *  received, with the final value available once the entire body has
     *  been received.
     */
    size_t request_message_len;

    /** The actual entity length (the length _before_ transformations
     *  have been applied). This field will change as a request body is being
     *  received, with the final value available once the entire body has
     *  been received.
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

    /** Original request header lines. */
    list_t *request_header_lines;

    /** Parsed request headers. */
    table_t *request_headers;

    /** Request transfer coding: IDENTITY or CHUNKED. Only available on requests that have bodies. */
    int request_transfer_coding;

    /** TODO Compression. */
    int request_content_encoding;

    // Response

    /** How many empty lines did we ignore before reaching the status line? */
    unsigned int response_ignored_lines;

    /** Response line. */
    bstr *response_line;

    /** Response protocol, as text. */
    bstr *response_protocol;

    /** Response protocol as number. Only available if we were
     *  able to parse the protocol version.
     */
    int response_protocol_number;

    /** Response status code, as text. */
    bstr *response_status;

    /** Reponse status code, available only if we were able to parse it. */
    int response_status_number;

    /** The message associated with the response status code. */
    bstr *response_message;

    /** Have we seen the server respond with a 100 response? */
    int seen_100continue;   

    /** Original response header lines. */
    list_t *response_header_lines;

    /** Parsed response headers. */
    table_t *response_headers;

    /** The actual message length (the length _after_ transformations
     *  have been applied). This field will change as a request body is being
     *  received, with the final value available once the entire body has
     *  been received.
     */
    size_t response_message_len;

    /** The actual entity length (the length _before_ transformations
     *  have been applied). This field will change as a request body is being
     *  received, with the final value available once the entire body has
     *  been received.
     */
    size_t response_entity_len;

    /** Response transfer coding: IDENTITY or CHUNKED. Only available on responses that have bodies. */
    int response_transfer_coding;

    /** TODO Compression. */
    int response_content_encoding;
    
    // Common

    /** Log messages associated with this transaction. */
    list_t *messages;

    /** The highest log message seen. */
    int highest_log_level;

    /** Parsing flags: HTP_INVALID_CHUNKING, HTP_INVALID_FOLDING,
     *  HTP_REQUEST_SMUGGLING, HTP_MULTI_PACKET_HEAD.
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
    char *data;

    /** Buffer length. */
    size_t len;
};

/** URI structure. Each of the fields provides access to a single
 *  URI element. A typical URI will look like this:
 *  http://username:password@hostname.com:8080/path?query#fragment. Only
 *  the fields corresponding to the elements present in the URI will be
 *  populated.
 */
struct htp_uri_t {
    bstr *scheme;
    bstr *hostname;
    bstr *username;
    bstr *password;
    bstr *port;
    int port_number;
    bstr *path;
    bstr *query;
    bstr *fragment;
};


// -- Functions -----------------------------------------------------------------------------------

htp_cfg_t *htp_config_copy(htp_cfg_t *cfg);
htp_cfg_t *htp_config_create();
int htp_config_server_personality(htp_cfg_t *cfg, int personality);

void htp_config_register_transaction_start(htp_cfg_t *cfg, int (*callback_fn)(htp_connp_t *), int priority);
void htp_config_register_request_line(htp_cfg_t *cfg, int (*callback_fn)(htp_connp_t *), int priority);
void htp_config_register_request_headers(htp_cfg_t *cfg, int (*callback_fn)(htp_connp_t *), int priority);
void htp_config_register_request_body_data(htp_cfg_t *cfg, int (*callback_fn)(htp_tx_data_t *), int priority);
void htp_config_register_request_trailer(htp_cfg_t *cfg, int (*callback_fn)(htp_connp_t *), int priority);
void htp_config_register_request(htp_cfg_t *cfg, int (*callback_fn)(htp_connp_t *), int priority);

void htp_config_register_response_line(htp_cfg_t *cfg, int (*callback_fn)(htp_connp_t *), int priority);
void htp_config_register_response_headers(htp_cfg_t *cfg, int (*callback_fn)(htp_connp_t *), int priority);
void htp_config_register_response_body_data(htp_cfg_t *cfg, int (*callback_fn)(htp_tx_data_t *), int priority);
void htp_config_register_response_trailer(htp_cfg_t *cfg, int (*callback_fn)(htp_connp_t *), int priority);
void htp_config_register_response(htp_cfg_t *cfg, int (*callback_fn)(htp_connp_t *), int priority);

htp_connp_t *htp_connp_create(htp_cfg_t *cfg);
// TODO Is below all right for IPv6 too?
void htp_connp_open(htp_connp_t *connp, const char *remote_addr, int remote_port, const char *local_addr, int local_port);
void htp_connp_close(htp_connp_t *connp);
void htp_connp_destroy(htp_connp_t *connp);
void htp_connp_destroy_all(htp_connp_t *connp);

 void htp_connp_set_user_data(htp_connp_t *connp, void *user_data);
void *htp_connp_get_user_data(htp_connp_t *connp);

htp_conn_t *htp_conn_create();
       void htp_conn_destroy(htp_conn_t *conn);
       // void htp_conn_destroy_all(htp_conn_t *conn);

int htp_connp_req_data(htp_connp_t *connp, htp_time_t timestamp, unsigned char *data, size_t len);
// int htp_connp_req_data_missing(htp_connp_t *connp, htp_time_t timestamp, size_t len);
int htp_connp_res_data(htp_connp_t *connp, htp_time_t timestamp, unsigned char *data, size_t len);
// int htp_connp_res_data_missing(htp_connp_t *connp, htp_time_t timestamp, size_t len);

      void htp_connp_clear_error(htp_connp_t *connp);
htp_log_t *htp_connp_get_last_error(htp_connp_t *connp);

htp_header_t *htp_connp_header_parse(htp_connp_t *, unsigned char *, size_t);

#define CFG_NOT_SHARED  0
#define CFG_SHARED      1

htp_tx_t *htp_tx_create(htp_cfg_t *cfg, int is_cfg_shared, htp_conn_t *conn);
     void htp_tx_destroy(htp_tx_t *tx);
     void htp_tx_set_config(htp_tx_t *tx, htp_cfg_t *cfg, int is_cfg_shared);

     void htp_tx_set_user_data(htp_tx_t *tx, void *user_data);
    void *htp_tx_get_user_data(htp_tx_t *tx);

// Parse functions

int htp_parse_request_header_apache_2_2(htp_connp_t *connp, htp_header_t *h, char *data, size_t len);
int htp_parse_request_line_apache_2_2(htp_connp_t *connp);
int htp_process_request_header_apache_2_2(htp_connp_t *);

int htp_parse_response_line_generic(htp_connp_t *connp);
int htp_process_response_header_generic(htp_connp_t *connp);

// Parser states

int htp_connp_REQ_IDLE(htp_connp_t *connp);
int htp_connp_REQ_FIRST_LINE(htp_connp_t *connp);
int htp_connp_REQ_PROTOCOL(htp_connp_t *connp);
int htp_connp_REQ_HEADERS(htp_connp_t *connp);
int htp_connp_REQ_BODY_DETERMINE(htp_connp_t *connp);
int htp_connp_REQ_BODY_IDENTITY(htp_connp_t *connp);
int htp_connp_REQ_BODY_CHUNKED_LENGTH(htp_connp_t *connp);
int htp_connp_REQ_BODY_CHUNKED_DATA(htp_connp_t *connp);
int htp_connp_REQ_BODY_CHUNKED_DATA_END(htp_connp_t *connp);
int htp_connp_REQ_BODY_CHUNKED_TRAILER(htp_connp_t *connp);

int htp_connp_RES_IDLE(htp_connp_t *connp);
int htp_connp_RES_FIRST_LINE(htp_connp_t *connp);
int htp_connp_RES_HEADERS(htp_connp_t *connp);
int htp_connp_RES_BODY_DETERMINE(htp_connp_t *connp);
int htp_connp_RES_BODY_IDENTITY(htp_connp_t *connp);
int htp_connp_RES_BODY_CHUNKED_LENGTH(htp_connp_t *connp);
int htp_connp_RES_BODY_CHUNKED_DATA(htp_connp_t *connp);
int htp_connp_RES_BODY_CHUNKED_DATA_END(htp_connp_t *connp);
int htp_connp_RES_BODY_CHUNKED_TRAILER(htp_connp_t *connp);


// Utility functions

int htp_convert_method_to_number(bstr *);
int htp_is_lws(int c);
int htp_is_separator(int c);
int htp_is_text(int c);
int htp_is_token(int c);
int htp_chomp(unsigned char *data, size_t *len);
int htp_is_space(int c);

int htp_parse_protocol(bstr *protocol);

int htp_is_line_empty(char *data, int len);
int htp_is_line_whitespace(char *data, int len);

int htp_connp_is_line_folded(htp_connp_t *connp, char *data, size_t len);
int htp_connp_is_line_terminator(htp_connp_t *connp, char *data, size_t len);
int htp_connp_is_line_ignorable(htp_connp_t *connp, char *data, size_t len);

int htp_parse_uri(bstr *input, htp_uri_t **uri);
int htp_parse_content_length(bstr *b);
int htp_parse_chunked_length(char *data, size_t len);
int htp_parse_positive_integer_whitespace(char *data, size_t len, int base);

void htp_log(htp_connp_t *connp, const char *file, int line, int level, int code, const char *fmt, ...);

#endif	/* _HTP_H */

