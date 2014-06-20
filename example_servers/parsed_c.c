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
 ****************************************************************************/

/**
 * @file
 * @brief IronBee --- Example Server: Parsed C Edition
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 *
 * This example demonstrates a minimalistic server.  It creates an Engine,
 * loads a configuration file of the users choice, and then feeds some basic
 * traffic to it.  To keep the code simple, the traffic is hardcoded.  This
 * example is single threaded, although some multithreaded considerations are
 * commented on.
 *
 * This is intended as an example only.  IronBee comes with a command line
 * interface, clipp, with a wide feature set including a variety of input
 * formats and multithreaded support.
 *
 * For setup and teardown, see main() and load_configuration().
 *
 * For sending input to IronBee, see send_to_ironbee() and convert_headers().
 *
 * For server callbacks, see server_close(), server_error(),
 * server_error_header(), server_error_data(), and server_header().
 *
 * For an example server in C++, see parsed_cpp.cpp.  The C++ edition makes
 * use of ParserSuite to parse raw HTTP and feed it to IronBee.
 **/

#include <ironbee/config.h>       /* For ib_cfgparser_* */
#include <ironbee/engine.h>       /* For many things */
#include <ironbee/state_notify.h> /* For ib_state_notify_* */
#include <ironbee/string.h>       /* For IB_S2SL */

/*
 * Input Definition
 *
 * Below are the structures and constants that define the input this example
 * sends to IronBee.  It sends a single connection consisting of a single
 * transaction.
 *
 * For ease of expression, these structs use NUL-terminated strings.  This is
 * a limitation of this example, not of IronBee.  The macro IB_S2SL() from
 * ironbee/string.h is used to convert the C literals to the pointer/length
 * pairs that IronBee expects: `IB_S2SL(x) => x, strlen(x)`
 */

/**
 * Request Line
 *
 * IronBee expects four strings for the request line.  The raw request line
 * and the three components.  The construction routines will construct raw
 * from the other three if missing.  However, if possible, you should pass in
 * raw yourself.
 **/
typedef struct request_line_t request_line_t;
struct request_line_t
{
    const char *raw;
    const char *method;
    const char *uri;
    const char *protocol;
};

/**
 * Response Line
 *
 * See discussion of @ref request_line_t.
 **/
typedef struct response_line_t response_line_t;
struct response_line_t
{
    const char *raw;
    const char *protocol;
    const char *status;
    const char *message;
};

/**
 * Header
 *
 * A header is simply a key and a value.  Any further parsing or
 * interpretation is handled by modules.
 **/
typedef struct header_t header_t;
struct header_t
{
    const char *key;
    const char *value;
};

/**
 * Maximum number of headers.
 *
 * This constant is used to declare fixed sized arrays of headers, allowing
 * for easier initialization.
 **/
#define MAX_HEADERS 10

/**
 * Request
 *
 * A request is the request line, some number of headers, and body text.  In
 * this example, all headers are delivered in a single event, and the body
 * data is also delivered in a single event.  IronBee supports splitting up
 * headers or data across multiple events.
 **/
typedef struct request_t request_t;
struct request_t
{
    request_line_t line;
    header_t       headers[MAX_HEADERS + 1]; /* End with { NULL, NULL } */
    const char*    body;
};

/**
 * Response
 *
 * See discussion of @ref request_t.
 **/
typedef struct response_t response_t;
struct response_t
{
    response_line_t line;
    header_t        headers[MAX_HEADERS + 1]; /* End with { NULL, NULL } */
    const char*     body;
};

/**
 * Example Request
 *
 * This is the example request sent to IronBee.
 **/
static const request_t c_request = {
    .line = {
        .raw      = "POST /hello/world HTTP/1.1",
        .method   = "POST",
        .uri      = "/hello/world",
        .protocol = "HTTP/1.1"
    },
    .headers = {
        { "Host",           "hello.world"          },
        { "Content-Length", "11"                   },
        { "User-Agent",     "IronBeeExampleServer" },
        { NULL,             NULL                   }
    },
    .body = "Hello World"
};

/**
 * Example Response
 *
 * This is the example response sent to IronBee.
 **/
static const response_t c_response = {
    .line = {
        .raw      = "HTTP/1.1 200 OK",
        .protocol = "HTTP/1.1",
        .status   = "200",
        .message  = "OK"
    },
    .headers = {
        { "Content-Length", "7"          },
        { "Content-Type",   "text/plain" },
        { NULL,             NULL         }
    },
    .body = "Goodbye"
};

/* Helper Functions */

/**
 * Load a configuration file.
 *
 * IronBee supports loading configuration from strings or files.  This
 * function handles loading a file.  It initializes a configuration parser,
 * tells the engine about it, parses the file, and cleans up.
 *
 * @param[in] engine IronBee engine to configure.
 * @param[in] path   Path to configuration file.
 * @return
 * - IB_OK on success.
 * - Error code on any failure.
 **/
ib_status_t load_configuration(
    ib_engine_t *engine,
    const char  *path
);

/**
 * Send a connection to IronBee.
 *
 * This function is the main notification code.  It creates a connection and
 * then a transaction consisting of @a request and @a response.  It
 * exemplifies how to turn the example sturctures defined in this file into
 * the parameters to the IronBee state notification routines.
 *
 * @param[in] engine   Engine to send to.
 * @param[in] request  Request to send
 * @param[in] response Response to send.
 * @return
 * - IB_OK on success.
 * - Error code on any failure.
 **/
ib_status_t send_to_ironbee(
    ib_engine_t      *engine,
    const request_t  *request,
    const response_t *response
);

/**
 * Convert list of @ref header_t to IronBee representation.
 *
 * This routine converts a list of headers at @a src into the IronBee
 * equivalent, a @ref ib_parsed_headers_t.
 *
 * @param[out] headers Constructed headers list.
 * @param[in]  src     Headers to construct from.
 * @param[in]  mm      Memory manager to use.
 * @return
 * - IB_OK on success.
 * - Error code on any failure.
 **/
ib_status_t convert_headers(
    ib_parsed_headers_t **headers,
    const header_t       *src,
    ib_mm_t               mm
);

/* Server Callbacks
 *
 * Server callbacks allow IronBee to communicate back to the server.  They
 * have little purpose for passive use of IronBee but are vital for inline
 * use.
 *
 * All callbacks should return IB_DECLINED if they do not wish to do what is
 * asked of them.  Callbacks do not need to be specified.  Any missing
 * callbacks implicitly return IB_ENOTIMPL.
 *
 * There are also callbacks for modifying bodies in flight.  These are an
 * advanced optional feature and are not included in this example.
 *
 * See server.h for additional documentation.
 */

/**
 * IronBee requests that server close connection.
 *
 * @param[in] conn   Connection to close.
 * @param[in] tx     Transaction.
 * @param[in] cbdata Callback data.
 * @return
 * - IB_OK on success.
 * - IB_DECLINED on refusal.
 * - Other status code on error.
 **/
ib_status_t server_close(
    ib_conn_t *conn,
    ib_tx_t   *tx,
    void      *cbdata
);

/**
 * IronBee requests that server respond with an error status.
 *
 * This call may be followed by one or more calls to server_error_header() to
 * set headers for the error response and a server_error_data() call to set
 * the body.
 *
 * @param[in] tx     Transaction to respond to.
 * @param[in] status Status code to use.
 * @param[in] cbdata Callback data.
 * @return
 * - IB_OK on success.
 * - IB_DECLINED on refusal.
 * - Other status code on error.
 **/
ib_status_t server_error(
    ib_tx_t *tx,
    int      status,
    void    *cbdata
);

/**
 * IronBee requests that server provide a certain header in error response.
 *
 * @param[in] tx           Transaction.
 * @param[in] name         Name of header.
 * @param[in] name_length  Length of @a name.
 * @param[in] value        Value of header.
 * @param[in] value_length Length of @a value.
 * @param[in] cbdata       Callback data.
 * @return
 * - IB_OK on success.
 * - IB_DECLINED on refusal.
 * - Other status code on error.
 **/
ib_status_t server_error_header(
    ib_tx_t    *tx,
    const char *name,
    size_t      name_length,
    const char *value,
    size_t      value_length,
    void       *cbdata
);

/**
 * IronBee requests that server provide a certain body in error response.
 *
 * @param[in] tx          Transaction.
 * @param[in] data        Data of body.
 * @param[in] data_length Length of @a data.
 * @param[in] cbdata      Callback data.
 * @return
 * - IB_OK on success.
 * - IB_DECLINED on refusal.
 * - Other status code on error.
 **/
ib_status_t server_error_data(
    ib_tx_t    *tx,
    const char *data,
    size_t      data_length,
    void       *cbdata
);

/**
 * IronBee requests that server modify headers before further processing.
 *
 * @param[in] tx           Transaction.
 * @param[in] dir          Which of request and response to modify headers of.
 * @param[in] action       Type of modification to do.  Options are set
 *                         (replace), append (add), merge (add if missing),
 *                         and unset (remove).
 * @param[in] name         Name of header.
 * @param[in] name_length  Length of @a name.
 * @param[in] value        Value of header.
 * @param[in] value_length Length of @a value.
 * @param[in] cbdata       Callback data.
 * @return
 * - IB_OK on success.
 * - IB_DECLINED on refusal.
 * - Other status code on error.
 **/
ib_status_t server_header(
    ib_tx_t                   *tx,
    ib_server_direction_t      dir,
    ib_server_header_action_t  action,
    const char                *name,
    size_t                     name_length,
    const char                *value,
    size_t                     value_length,
    void                      *cbdata
);

/**
 * IronBee requets that server modify stream.
 *
 * @param[in] tx The transaction.
 * @param[in] dir The direction.
 * @param[in] start Start of text to replace.
 * @param[in] bytes Length of text to replace.
 * @param[in] repl Replacement text.
 * @param[in] repl_len Length of @a repl.
 * @param[in] cbdata Callback data.
 * @return
 * - IB_OK on success.
 * - IB_DECLINED on refusal.
 * - Other status code on error.
 **/
ib_status_t server_body_edit(
    ib_tx_t                   *tx,
    ib_server_direction_t      dir,
    off_t                      start,
    size_t                     bytes,
    const char                *repl,
    size_t                     repl_len,
    void                      *cbdata
);

/* Implementation */

int main(int argc, char **argv)
{
   /* Create server object.
    * The ib_server_t is a struct that communicates server information to the
    * IronBee engine.  Besides some basic information, it has a variety of
    * callbacks to allow the engine to communicate to the server.
    *
    * In this example, the callbacks only produce output.
    */
    ib_server_t server = {
        /* Defaults include IronBee version information and the path to
         * this file. */
        IB_SERVER_HEADER_DEFAULTS,
        /* Name of server */
        "example_servers/parsed_c",
        /* Callbacks with callback data */
        server_header,       NULL,
        server_error,        NULL,
        server_error_header, NULL,
        server_error_data,   NULL,
        server_close,        NULL,
        server_body_edit,    NULL
    };

    ib_engine_t *engine;
    ib_status_t  rc;

    if (argc != 2) {
        printf("Usage: %s <configuration>\n", argv[0]);
        return 1;
    }

    /* Initialize IronBee */
    ib_initialize();

    /* Create Engine */
    rc = ib_engine_create(&engine, &server);
    if (rc != IB_OK) {
        printf("Error creating engine: %s\n", ib_status_to_string(rc));
        return 1;
    }

    /* Load configuration */
    rc = load_configuration(engine, argv[1]);
    if (rc != IB_OK) {
        return 1;
    }

    /* Send some traffic to the engine. */
    rc = send_to_ironbee(engine, &c_request, &c_response);
    if (rc != IB_OK) {
        return 1;
    }

    /* Destroy engine */
    ib_engine_destroy(engine);

    /* Shutdown IronBee */
    ib_shutdown();
}

ib_status_t load_configuration(ib_engine_t *engine, const char *path)
{
    /* This example shows how to configure from a file.  The configuration
     * parse includes a variety of ways to configure, including the ability
     * to build up a parsed configuration and then apply to the engine.
     *
     * See config.h
     */
    ib_cfgparser_t *parser;
    ib_status_t rc;

    rc = ib_cfgparser_create(&parser, engine);
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_engine_config_started(engine, parser);
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_cfgparser_parse(parser, path);
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_engine_config_finished(engine);
    if (rc != IB_OK) {
        return rc;
    }

    ib_cfgparser_destroy(parser);

    return IB_OK;
}

ib_status_t send_to_ironbee(
    ib_engine_t      *engine,
    const request_t  *request,
    const response_t *response
)
{
    ib_conn_t   *conn;
    ib_tx_t     *tx;
    ib_status_t  rc;

    /*
     * Create Connection
     *
     * A connection is some TCP/IP information and a sequence of transactions.
     * Its primary purpose is to associate transactions.
     *
     * IronBee allows for multithreading so long as a single connection (and
     * its tranactions) is only be used in one thread at a time.  Thus, a
     * typical multithreaded IronBee setup would pass connections off to
     * worker  threads, but never use multiple threads for a single
     * connection.
     */
    rc = ib_conn_create(engine, &conn, NULL);
    if (rc != IB_OK) {
        ib_log_error(engine, "Could not create connection: %s",
                     ib_status_to_string(rc));
        return rc;
    }

    /* IronBee also supports IPv6 addresses. */
    conn->local_ipstr  = "1.2.3.4";
    conn->local_port   = 80;
    conn->remote_ipstr = "5.6.7.8";
    conn->remote_port  = 1234;

    /*
     * Connection Opened
     *
     * Here is our first state-notify call.  All communication of data and
     * events to IronBee is via state notify calls.  We begin with
     * ib_state_notify_conn_opened() which tells IronBee that a new connection
     * has been established.  Its dual is ib_state_notify_conn_closed() which
     * we call at the end.
     */
    rc = ib_state_notify_conn_opened(engine, conn);
    if (rc != IB_OK) {
        ib_log_error(engine, "Error notifying connection opened: %s",
                     ib_status_to_string(rc));
        return rc;
    }

    /*
     * Create Transaction
     *
     * The tx object holds all per-transaction information.  Besides using it
     * to indicate which transaction we are providing data for, it will allow
     * us to control the lifetime of all our created objects.  We do this by
     * allocating all memory from `tx->mm`, a memory manager whose lifetime is
     * equal to that of the transaction.
     */
    rc = ib_tx_create(&tx, conn, NULL);
    if (rc != IB_OK) {
        ib_log_error(engine, "Could not create transaction: %s",
                     ib_status_to_string(rc));
        return rc;
    }

    /*
     * The next several sections go through the typical transaction lifecycle:
     *
     * - Request Started which provides the request line.
     * - Request Header which provides headers.  May be repeated.
     * - Request Header Finishes indicating no more headers.
     * - Request Body which provides body data.  May be repeated.
     * - Request Finished indicating the end of the request.
     * - A similar sequence of events for the response.
     *
     * Each transaction is a single request and response.  A connection may
     * contain multiple transactions.
     */

    /* Request Started */
    {
        ib_parsed_req_line_t *req_line;

        rc = ib_parsed_req_line_create(
            &req_line, tx->mm,
            IB_S2SL(request->line.raw),
            IB_S2SL(request->line.method),
            IB_S2SL(request->line.uri),
            IB_S2SL(request->line.protocol)
        );
        if (rc != IB_OK) {
            ib_log_error(engine, "Could not create request line: %s",
                         ib_status_to_string(rc));
            return rc;
        }

        rc = ib_state_notify_request_started(engine, tx, req_line);
        if (rc != IB_OK) {
            ib_log_error(engine, "Error notifying request started: %s",
                         ib_status_to_string(rc));
            return rc;
        }
    }

    /* Request Header */
    {
        ib_parsed_headers_t *headers;

        rc = convert_headers(&headers, request->headers, tx->mm);
        if (rc != IB_OK) {
            ib_log_error(engine, "Error converting request headers: %s",
                         ib_status_to_string(rc));
            return rc;
        }

        rc = ib_state_notify_request_header_data(engine, tx, headers);
        if (rc != IB_OK) {
            ib_log_error(engine, "Error notifying request headers: %s",
                         ib_status_to_string(rc));
            return rc;
        }
    }

    /* Request Header Finished */
    rc = ib_state_notify_request_header_finished(engine, tx);
    if (rc != IB_OK) {
        ib_log_error(engine,
                     "Error notifying request headers finished: %s",
                     ib_status_to_string(rc));
        return rc;
    }

    /* Request Body */
    rc = ib_state_notify_request_body_data(engine, tx,
                                           IB_S2SL(request->body));
    if (rc != IB_OK) {
        ib_log_error(engine,
                     "Error notifying request body: %s",
                     ib_status_to_string(rc));
        return rc;
    }

    /* Request Finished */
    rc = ib_state_notify_request_finished(engine, tx);
    if (rc != IB_OK) {
        ib_log_error(engine,
                     "Error notifying request finished: %s",
                     ib_status_to_string(rc));
        return rc;
    }

    /* Respone Started */
    {
        ib_parsed_resp_line_t *resp_line;

        rc = ib_parsed_resp_line_create(
            &resp_line, tx->mm,
            IB_S2SL(response->line.raw),
            IB_S2SL(response->line.protocol),
            IB_S2SL(response->line.status),
            IB_S2SL(response->line.message)
        );
        if (rc != IB_OK) {
            ib_log_error(engine, "Could not create response line: %s",
                         ib_status_to_string(rc));
            return rc;
        }

        rc = ib_state_notify_response_started(engine, tx, resp_line);
        if (rc != IB_OK) {
            ib_log_error(engine, "Error notifying response started: %s",
                         ib_status_to_string(rc));
            return rc;
        }
    }

    /* Respone Headers */
    {
        ib_parsed_headers_t *headers;

        rc = convert_headers(&headers, response->headers, tx->mm);
        if (rc != IB_OK) {
            ib_log_error(engine, "Error converting response headers: %s",
                         ib_status_to_string(rc));
            return rc;
        }

        rc = ib_state_notify_response_header_data(engine, tx, headers);
        if (rc != IB_OK) {
            ib_log_error(engine, "Error notifying response headers: %s",
                         ib_status_to_string(rc));
            return rc;
        }
    }

    /* Response Header Finished */
    rc = ib_state_notify_response_header_finished(engine, tx);
    if (rc != IB_OK) {
        ib_log_error(engine,
                     "Error notifying response headers finished: %s",
                     ib_status_to_string(rc));
        return rc;
    }

    /* Response Body */
    rc = ib_state_notify_response_body_data(engine, tx,
                                            IB_S2SL(response->body));
    if (rc != IB_OK) {
        ib_log_error(engine,
                     "Error notifying response body: %s",
                     ib_status_to_string(rc));
        return rc;
    }

    /* Response Finished */
    rc = ib_state_notify_response_finished(engine, tx);
    if (rc != IB_OK) {
        ib_log_error(engine,
                     "Error notifying response finished: %s",
                     ib_status_to_string(rc));
        return rc;
    }

    /* Transaction Done */
    ib_tx_destroy(tx);

    /* Connection Closed */
    rc = ib_state_notify_conn_closed(engine, conn);
    if (rc != IB_OK) {
        ib_log_error(engine, "Error notifying connection closed: %s",
                     ib_status_to_string(rc));
        return rc;
    }

    /* Connection Done */
    ib_conn_destroy(conn);

    return IB_OK;
}

ib_status_t convert_headers(
    ib_parsed_headers_t **headers,
    const header_t       *src,
    ib_mm_t               mm
)
{
    ib_parsed_headers_t *local_headers;
    ib_status_t rc;

    rc = ib_parsed_headers_create(&local_headers, mm);
    if (rc != IB_OK) {
        return rc;
    }

    for (
        const header_t* header = src;
        header->key != NULL;
        ++header
    ) {
        rc = ib_parsed_headers_add(
            local_headers,
            IB_S2SL(header->key),
            IB_S2SL(header->value)
        );
        if (rc != IB_OK) {
            return rc;
        }
    }

    *headers = local_headers;
    return IB_OK;
}

ib_status_t server_close(
    ib_conn_t *conn,
    ib_tx_t   *tx,
    void      *cbdata
)
{
    printf("SERVER: CLOSE %s\n", tx->id);
    return IB_OK;
}

ib_status_t server_body_edit(
    ib_tx_t               *tx,
    ib_server_direction_t  dir,
    off_t                  start,
    size_t                 bytes,
    const char            *repl,
    size_t                 repl_len,
    void                  *cbdata
)
{
    printf("SERVER: BODY EDIT: %s %s %zd %zd %.*s\n",
        tx->id,
        (dir == IB_SERVER_REQUEST ? "request" : "response"),
        bytes, start,
        (int)repl_len, repl
    );
    return IB_OK;
}

ib_status_t server_error(
    ib_tx_t *tx,
    int      status,
    void    *cbdata
)
{
    printf("SERVER: ERROR: %s %d\n", tx->id, status);
    return IB_OK;
}

ib_status_t server_error_header(
    ib_tx_t    *tx,
    const char *name,
    size_t      name_length,
    const char *value,
    size_t      value_length,
    void       *cbdata
)
{
    printf(
        "SERVER: ERROR HEADER: %s %.*s %.*s\n",
        tx->id,
        (int)name_length, name,
        (int)value_length, value
    );
    return IB_OK;
}

ib_status_t server_error_data(
    ib_tx_t    *tx,
    const char *data,
    size_t      data_length,
    void       *cbdata
)
{
    printf("SERVER: ERROR DATA: %s %.*s\n", tx->id, (int)data_length, data);
    return IB_OK;
}

ib_status_t server_header(
    ib_tx_t                   *tx,
    ib_server_direction_t      dir,
    ib_server_header_action_t  action,
    const char                *name,
    size_t                     name_length,
    const char                *value,
    size_t                     value_length,
    void                      *cbdata
)
{
    const char *action_string;
    switch (action) {
        case IB_HDR_SET:
            action_string = "SET";
            break;
        case IB_HDR_APPEND:
            action_string = "APPEND";
            break;
        case IB_HDR_MERGE:
            action_string = "MERGE";
            break;
        case IB_HDR_ADD:
            action_string = "ADD";
            break;
        case IB_HDR_UNSET:
            action_string = "UNSET";
            break;
        default:
            action_string = "unknown";
    };

    printf(
        "SERVER: HEADER: %s %s %s %.*s %.*s\n",
        tx->id,
        (dir == IB_SERVER_REQUEST ? "request" : "response"),
        action_string,
        (int)name_length, name,
        (int)value_length, value
    );

    return IB_OK;
}
