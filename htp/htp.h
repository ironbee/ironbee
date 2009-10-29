
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

// TODO Handle configuration changes through copy-on-write, which will ensure that we only
//      perform the expensive configuration copying when it's really necessary.
//
//      Similarly, make it possible to completely change the configuration of a parser
//      whenever that's necessary.
//
//      For example, we need a hook that will potentially change the configuration after
//      we parse request headers, but before we parse parameters.


// -- Defines -------------------------------------------------------------------------------------

#define HTP_DEBUG               1

#define HTP_ERROR              -1
#define HTP_OK                  0
#define HTP_DATA                1

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

#define BODY_NONE       0
#define BODY_IDENTITY   1
#define BODY_CHUNKED    2

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

    // TODO Flag to control whether we keep raw header lines as
    //      well as headers.

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
    htp_connp_t *connp;
    const char *remote_addr;
    int remote_port;
    list_t *transactions;
    list_t *messages;
    // TODO pipeline connection flag
    unsigned int flags;
    // TODO data counters (before and after SSL?)
};

struct htp_connp_t {
    htp_cfg_t *cfg;
    htp_conn_t *conn;
    void *user_data;

    // TODO Flag that we've failed in parsing yet continued
    //      doing our best.

    /** On parser failure, this field will contain the error information. */
    htp_log_t *last_error;

    // Request parser fields

    unsigned int status; // TODO Consider using two status fields, one for each direction

    /** The time when the last request data chunk was received. */
    htp_time_t in_timestamp;
    unsigned char *in_current_data;
    size_t in_current_len;
    size_t in_current_offset;
    size_t in_stream_offset;
    int in_next_byte;

    size_t in_line_len;
    size_t in_line_size;
    unsigned char *in_line;

    /** Ongoing inbound transaction */
    htp_tx_t *in_tx;   
    
    htp_header_line_t *in_header_line;    
    int in_header_line_index;
    int in_header_line_counter;

    size_t in_content_length;
    size_t in_body_data_left;

    int in_chunked_length;
      
    int (*in_state)(htp_connp_t *);

    // Response parser fields

    int out_next_tx_index;

    /** The time when the last response data chunk was received. */
    htp_time_t out_timestamp;
    unsigned char *out_current_data;
    size_t out_current_len;
    size_t out_current_offset;
    size_t out_stream_offset;
    int out_next_byte;

    size_t out_line_len;
    size_t out_line_size;
    unsigned char *out_line;
        
    /** Ongoing outbound transaction */
    htp_tx_t *out_tx;

    htp_header_line_t *out_header_line;
    int out_header_line_index;
    int out_header_line_counter;

    size_t out_content_length;
    size_t out_body_data_left;

    int out_chunked_length;

    int (*out_state)(htp_connp_t *);
};

struct htp_log_t {
    const char *msg;
    int level;
    int code;
    const char *file;
    unsigned int line;
};

struct htp_header_line_t {
    bstr *line;
    size_t name_offset;
    size_t name_len;
    size_t value_offset;
    size_t value_len;
    unsigned int has_nulls;
    int first_nul_offset;

    /** HTP_FIELD_INVALID_NOT_FATAL, HTP_FIELD_INVALID_FATAL, HTP_FIELD_LONG */
    unsigned int flags;
    
    /** Header that used this line. */
    htp_header_t *header;
};

struct htp_header_t {    
    bstr *name;
    bstr *value;   

    /** HTP_FIELD_INVALID_NOT_FATAL, HTP_FIELD_FOLDED, HTP_FIELD_REPEATED */
    unsigned int flags;
};

struct htp_tx_t {
    htp_conn_t *conn;
    htp_cfg_t *cfg;
    int is_cfg_shared;
    void *user_data;
    
    // Request
    unsigned int request_ignored_lines;

    bstr *request_line;
    int request_line_nul;
    int request_line_nul_offset;
    bstr *request_method;
    int request_method_number;
    bstr *request_uri;
    bstr *request_protocol;
    int protocol_is_simple;
    
    /** Protocol version as a number: -1 if not available 9 (HTTP_0_9) for 0.9,
     *  100 (HTTP_1_0) for 1.0 and 101 (HTTP_1_1) for 1.1.
     */
    int request_protocol_number;
    
    bstr *query_string;

    list_t *request_header_lines;
    table_t *request_headers;    

    int body_encoding;
    int body_status;
    bstr *body;
    size_t body_length;

    // Response
    unsigned int response_ignored_lines;

    bstr *response_line;    
    bstr *response_protocol;
    int response_protocol_number;
    bstr *response_status;
    int response_status_number;
    bstr *response_message;

    //int response_body_len_declared;
    int response_body_len_actual;

    list_t *response_header_lines;
    table_t *response_headers;
    
    // Common

    list_t *messages;
    int highest_log_level;
    unsigned int flags;
    unsigned int progress;
};

struct htp_tx_data_t {
    htp_tx_t *tx;
    char *data;
    size_t len;
};


// -- Functions -----------------------------------------------------------------------------------


#define HTP_SERVER_STRICT           0
#define HTP_SERVER_PERMISSIVE       1
#define HTP_SERVER_APACHE_2_2       2
#define HTP_SERVER_IIS_5_1          3
#define HTP_SERVER_IIS_7_5          4

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

// TODO There will be a callback that will be invoked whenever an error
//      occurs. (By the way, an error is raised only when the parser can
//      no longer parse a connection.) The callback can look at the error
//      and ask the parser to continue to scan looking for the next message
//      boundary.

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

int htp_parse_content_length(bstr *b);
int htp_parse_chunked_length(char *data, size_t len);
int htp_parse_positive_integer_whitespace(char *data, size_t len, int base);

void htp_log(htp_connp_t *connp, const char *file, int line, int level, int code, const char *fmt, ...);

#endif	/* _HTP_H */

