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

/**
 * @file ibcli.c
 * @brief simple command line tool for driving ironbee
 *
 * @author Craig Forbes <cforbes@qualys.com>
 */

#include "ironbee_config_auto.h"

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <getopt.h>
#include <ctype.h>
#include <sys/types.h>
#include <inttypes.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <glob.h>
#include <libgen.h>

#include <ironbee/engine.h>
#include <ironbee/plugin.h>
#include <ironbee/provider.h>
#include <ironbee/module.h>
#include <ironbee/config.h>
#include <ironbee/mpool.h>
#include <ironbee/bytestr.h>
#include <ironbee_private.h>

/* Set DEBUG_ARGS_ENABLE to non-zero enable the debug log command line
 * handling.  It's currently disabled because DebugLog and DebugLogLevel
 * directives in the configuration will overwrite the command-line version,
 * which can cause the CLI to log back and forth between the two files. */
#define DEBUG_ARGS_ENABLE 0

/* Data transfer direction */
typedef enum {
    DATA_IN,
    DATA_OUT
} data_direction_t;

/* Max number of request headers that can be specified on the command line */
#define MAX_REQUEST_HEADERS     8
#define MAX_FILES            1024

/* Header / value pairs */
typedef struct {
    const char *name;           /* Field name buffer */
    size_t      name_len;       /* Name length */
    const char *buf;            /* Field string buffer; NULL means remove */
    int         buf_len;        /* Total buffer length */
    int         used;           /* Has this field been used */
} request_header_t;

/* Runtime settings */
typedef struct {
    char *config_file;
    glob_t req_files;
    glob_t rsp_files;

    /* Local and remote IP address / port */
    const char *local_ip;
    int local_port;
    const char *remote_ip;
    int remote_port;

    /* Trace */
    int trace;
    uint64_t trace_request_cnt;
    uint64_t trace_response_cnt;

    /* Dump output */
    int dump_tx;
    int dump_user_agent;
    int dump_geoip;

    /* Request header fields */
    struct {
        int              num_headers;
        request_header_t headers[MAX_REQUEST_HEADERS];
    } request_headers;

    /* Max # of transactions */
    int max_transactions;

    /* Verbose */
    int verbose;

    /* Debug arguments */
#if DEBUG_ARGS_ENABLE
    const char *debug_uri;
    int debug_level;
#endif
} runtime_settings_t;

static runtime_settings_t settings =
{
    NULL,                  /* config_file */
    { 0 },                 /* req_files */
    { 0 },                 /* rsp_files */
    "192.168.1.1",         /* local_ip */
    8080,                  /* local_port */
    "10.10.10.10",         /* remote_ip */
    23424,                 /* remote_port */
    0,                     /* trace */
    0,                     /* trace_request_count */
    0,                     /* trace_response_count */
    0,                     /* dump_user_agent */
    0,                     /* dump_effective_ip */
    0,                     /* dump_geoip */
    { 0 },                 /* request_headers */
    -1,                    /* Max # of transactions to run */
    0,                     /* Verbose level */
#if DEBUG_ARGS_ENABLE
    NULL,                  /* debug_uri */
    -1                     /* debug_level */
#endif
};

typedef struct {
    char     *buf;         /* Acutal buffer */
    size_t    len;         /* Length of data in the buffer */
    size_t    size;        /* Current size of buffer */
} reqhdr_buf_t;

#define MAX_BUF      (64*1024)
#define MAX_LINE_BUF (16*1024)

/* Plugin Structure */
ib_plugin_t ibplugin = {
    IB_PLUGIN_HEADER_DEFAULTS,
    "ibcli"
};

/**
 * @internal
 * Print usage.
 *
 * Print terse usage and exit.
 *
 * @returns void
 */
static void usage(void)
{
    fprintf(stderr, "Usage: ibcli <options>\n");
    fprintf(stderr, "  Use --help for help\n");
    exit(1);
}

/**
 * @internal
 * Print help for a single command line option.
 *
 * Prints a pretty help message for a single command line option.
 *
 * @param[in] opt Option string
 * @param[in] param Parameter value or NULL
 * @param[in] desc Option description
 * @param[in] required 1:Required, 0:Not required
 * @param[in] values String of possible option values or NULL
 *
 * @returns void
 */
static void print_option(const char *opt,
                         const char *param,
                         const char *desc,
                         int required,
                         const char *values)
{
    char buf[64];
    const char *req = "";
    if (param == NULL) {
        snprintf( buf, sizeof(buf), "--%s", opt );
    }
    else {
        snprintf( buf, sizeof(buf), "--%s <%s>", opt, param );
   }
    if (required) {
        req = "[Required]";
    }
    printf( "  %-30s: %s %s\n", buf, desc, req );
    if (values != NULL) {
        printf( "    Valid %ss: %s\n", param, values );
    }
}

/**
 * @internal
 * Print help message.
 *
 * Print pretty help message.
 *
 * @returns void
 */
static void help(void)
{
    printf("Usage: ibcli <options>\n");
    printf("Options:\n");

    print_option("config", "path", "Specify configuration file", 1, NULL );
    print_option("request-file", "path", "Specify request file", 1, NULL );
    print_option("response-file", "path", "Specify response file", 1, NULL );
    print_option("max-transactions", "num",
                 "Specify max # of transactions to run", 0, NULL );
    print_option("verbose", "num", "Specify verbose level", 0, NULL );
    print_option("local-ip", "x.x.x.x", "Specify local IP address", 0, NULL );
    print_option("local-port", "num", "Specify local port", 0, NULL );
    print_option("remote-ip", "x.x.x.x", "Specify remote IP address", 0, NULL );
    print_option("remote-port", "num", "Specify remote port", 0, NULL );
    print_option("trace", NULL, "Enable tracing", 0, NULL );
    print_option("dump", "name", "Dump specified field", 0,
                 "tx, user-agent, geop");
    print_option("request-header", "name: value",
                 "Specify request field & value", 0, NULL );
    print_option("request-header", "-name:",
                 "Specify request field to delete", 0, NULL );
#if DEBUG_ARGS_ENABLE
    print_option("debug-level", "path", "Specify debug log level", 0, NULL );
    print_option("debug-log", "path", "Specify debug log file / URI", 0, NULL );
#endif
    print_option("help", NULL, "Print this help", 0, NULL );
    exit(0);
}

/**
 * @internal
 * Add a request header field / value.
 *
 * Attempts to add the specified request header name and value to the list
 * of header fields.
 *
 * @param[in] str Header string from the command line
 * @param[in] len Length of the string
 * @param[in] name_len Length of the name portion of the string
 * @param[in] delete 1:Delete the header field; 0:Don't delete
 *
 * @returns status
 */
static ib_status_t add_request_header(const char *str,
                                      size_t str_len,
                                      size_t name_len,
                                      int delete)
{
    int     num;
    char   *buf;
    size_t  buf_len;

    if (settings.request_headers.num_headers >= MAX_REQUEST_HEADERS) {
        fprintf(stderr,
                "Unable to add request header field: max # is %i: ",
                 MAX_REQUEST_HEADERS);
        return IB_EALLOC;
    }

    /* Allocate space and copy to the buffer */
    buf_len = (delete != 0) ? str_len : str_len + 2;  /* '\r' and '\n' */
    buf = (char *) malloc(buf_len+1);  /* Extra byte for '\0' */
    if (buf == NULL) {
        fprintf(stderr,
                "Failed to allocate buffer for request header field %-*s",
                (int)name_len, buf);
        return IB_EALLOC;
    }

    /* Account for it */
    num = settings.request_headers.num_headers++;

    /* Add it in */
    strcpy(buf, str);
    if (delete == 0) {
        strcat(buf, "\r\n");
        settings.request_headers.headers[num].buf     = buf;
        settings.request_headers.headers[num].buf_len = buf_len;
    }
    else {
        settings.request_headers.headers[num].buf     = NULL;
        settings.request_headers.headers[num].buf_len = 0;
    }

    settings.request_headers.headers[num].name     = buf;
    settings.request_headers.headers[num].name_len = name_len;
    settings.request_headers.headers[num].used     = 0;

    return IB_OK;
}

/**
 * @internal
 * Handle a fatal error.
 *
 * Print a meaningful message and exit.
 *
 * @param[in] fmt printf format
 *
 * @returns void
 */
static void fatal_error(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    exit(1);
}

/**
 * @internal
 * Command line processing.
 *
 * Command line processing for the IronBee CLI
 *
 * @param[in] argc Command line argument count
 * @param[in] argv Command line argument array
 *
 * @returns status
 */
static ib_status_t command_line(int argc, char *argv[])
{
    ib_status_t rc;
    static struct option longopts[] = {
        { "config", required_argument, 0, 0 },
        { "request-file", required_argument, 0, 0 },
        { "response-file", required_argument, 0, 0 },
        { "max-transactions", required_argument, 0, 0 },
        { "verbose", required_argument, 0, 0 },
        { "local-ip", required_argument, 0, 0 },
        { "local-port", required_argument, 0, 0 },
        { "remote-ip", required_argument, 0, 0 },
        { "remote-port", required_argument, 0, 0 },

        { "request-header", required_argument, 0, 0 },
        { "trace", no_argument, 0, 0 },
        { "dump", required_argument, 0, 0 },

#if DEBUG_ARGS_ENABLE
        { "debug-level", required_argument, 0, 0 },
        { "debug-log", required_argument, 0, 0 },
#endif
        { "help", no_argument, 0, 0 },
        { 0, 0, 0, 0}
    };
    size_t num_req = 0;
    size_t num_rsp = 0;

    /* Loop through the command line args */
    while (1) {
        int option_index = 0;
        int c = getopt_long(argc, argv, "", longopts, &option_index);
        if (c != 0) {
            if ( c == 'h' ) {
                help( );
            }
            else {
                break;
            }
        }

        if (! strcmp("verbose", longopts[option_index].name)) {
            settings.verbose = atoi(optarg);
        }
        else if (! strcmp("config", longopts[option_index].name)) {
            settings.config_file = optarg;
        }
        else if (! strcmp("request-file", longopts[option_index].name)) {
            static int glob_flags = 0;
            int globrc = glob(optarg, glob_flags, NULL, &settings.req_files);
            if (globrc != 0) {
                fatal_error("Failed to glob on requests: %d (errno %d)\n",
                            globrc, errno);
            }
            else if (settings.req_files.gl_pathc == 0) {
                fprintf(stderr,
                        "No files match request glob pattern %s\n",
                        optarg);
                usage();
            }
            glob_flags |= GLOB_APPEND; /* Append on 2nd+ runs */
            num_req = settings.req_files.gl_pathc;
        }
        else if (! strcmp("response-file", longopts[option_index].name)) {
            static int glob_flags = 0;
            int globrc = glob(optarg, glob_flags, NULL, &settings.rsp_files);
            if (globrc != 0) {
                fatal_error("Failed to glob on responses: %d (errno %d)\n",
                            globrc, errno);
            }
            else if (settings.rsp_files.gl_pathc == 0) {
                fprintf(stderr,
                        "No files response match glob pattern %s\n",
                        optarg);
                usage();
            }
            glob_flags |= GLOB_APPEND; /* Append 2nd+ runs */
            num_rsp = settings.rsp_files.gl_pathc;
        }
        else if (! strcmp("max-transactions", longopts[option_index].name)) {
            settings.max_transactions = atoi(optarg);
        }
        else if (! strcmp("trace", longopts[option_index].name)) {
            settings.trace = 1;
        }
        else if (! strcmp("dump", longopts[option_index].name)) {
            if (strcasecmp(optarg, "geoip") == 0) {
                settings.dump_geoip = 1;
            }
            else if (strcasecmp(optarg, "user-agent") == 0) {
                settings.dump_user_agent = 1;
            }
            else if (strcasecmp(optarg, "tx") == 0) {
                settings.dump_tx = 1;
            }
            else {
                fprintf(stderr, "Unknown dump: %s", optarg);
                usage();
            }
        }
        else if (! strcmp("request-header", longopts[option_index].name)) {
            size_t nlen = 0;       /* Name length */
            size_t vlen = 0;       /* Value length */
            int    delete = 0;     /* Delete the field? */
            char *colon;

            /* Delete? */
            if (*optarg == '-') {
                delete = 1;
                optarg++;
            }

            /* Get a pointer to the first colon, count the number of
             * characters following it */
            colon = strchr(optarg, ':');
            if (colon != NULL) {
                vlen = strlen(colon+1);
                nlen = colon - optarg;
            }

            /* Simple checks */
            if (nlen == 0) {
                fprintf(stderr,
                        "Mal-formed request-header parameter '%s'",
                        optarg);
                usage();
            }

            /* Add it to the header */
            rc = add_request_header(optarg, nlen+1+vlen, nlen, delete);
            if (rc != IB_OK) {
                usage( );
            }
        }

#if DEBUG_ARGS_ENABLE
        else if (! strcmp("debug-level", longopts[option_index].name)) {
            int level =  (int) strtol(optarg, NULL, 10);
            if ( ( level == 0 ) && ( errno == EINVAL ) ) {
                fprintf(stderr,
                        "--debug-level: invalid level number '%s'", optarg );
                usage();
            }
            else if ( (level < 0) || (level < 9) ) {
                fprintf(stderr,
                        "--debug-level: Level %d out of range (0-9)", level );
                usage();
            }
            settings.debug_level = level;
        }
        else if (! strcmp("debug-log", longopts[option_index].name)) {
            static char  buf[256];
            const char  *s = optarg;

            // Create a file URI from the file path
            if ( strstr(s, "://") == NULL )  {
                snprintf( buf, sizeof(buf), "file://%s", s );
            }
            else if ( strncmp(s, "file://", 7) != 0 ) {
                fprintf( stderr, "--debug-log: Unsupport URI \"%s\"", s );
                usage( );
            }
            else {
                strncpy(buf, s, sizeof(buf));
            }
            settings.debug_uri = buf;
        }
#endif
        else if (! strcmp("local-ip", longopts[option_index].name)) {
            settings.local_ip = optarg;
        }
        else if (! strcmp("local-port", longopts[option_index].name)) {
            settings.local_port = (int) strtol(optarg, NULL, 10);
            if ( ( settings.local_port == 0 ) && ( errno == EINVAL ) ) {
                fprintf(stderr,
                        "--local-port: invalid port number '%s'", optarg );
                usage();
            }
        }
        else if (! strcmp("remote-ip", longopts[option_index].name)) {
            settings.remote_ip = optarg;
        }
        else if (! strcmp("remote-port", longopts[option_index].name)) {
            settings.remote_port = (int) strtol(optarg, NULL, 10);
            if ( ( settings.remote_port == 0 ) && ( errno == EINVAL ) ) {
                fprintf(stderr,
                        "--remote-port: invalid port number '%s'", optarg );
                usage();
            }
        }
        else if (! strcmp("help", longopts[option_index].name)) {
            help( );
        }
        else {
            usage( );
        }
    }

    if (settings.config_file == NULL) {
        fprintf(stderr, "--config <file> is required\n");
        usage();
    }
    else if (num_req == 0) {
        fprintf(stderr, "At least one request file is required\n");
        usage();
    }
    else if (num_rsp == 0) {
        fprintf(stderr, "At least one response file is required\n");
        usage();
    }
    else if ( (num_req != num_rsp) && ( (num_req != 1) && (num_rsp != 1) ) ) {
        fprintf(stderr,
                "# request files (%zd) and response files (%zd) mismatch\n",
                num_req, num_rsp);
        usage();
    }
    return IB_OK;
}

#if DEBUG_ARGS_ENABLE
/**
 * @internal
 * Set debug option values.
 *
 * Set the values from --debug-{level,uri} options.
 *
 * @param[in] ctx IronBee context
 *
 * @returns void
 */
static void set_debug( ib_context_t *ctx )
{
    if (settings.debug_level >= 0 ) {
        ib_context_set_num( ctx,
                            IB_PROVIDER_TYPE_LOGGER ".log_level",
                            settings.debug_level);
    }
    if (settings.debug_uri != NULL ) {
        ib_context_set_string( ctx,
                               IB_PROVIDER_TYPE_LOGGER ".log_uri",
                               settings.debug_uri);
    }
}
#endif

/**
 * @internal
 * Initialize the connection.
 *
 * Sets the connection local/remote address/port
 *
 * @param[in] ib IronBee object (not used)
 * @param[in] tx Transaction object
 * @param[in] cbdata Callback data (not used)
 *
 * @note The ib and cbdata parameters are unused
 *
 * @returns void
 */
static ib_status_t ironbee_conn_init(ib_engine_t *ib,
                                     ib_conn_t *iconn,
                                     void *cbdata)
{
    iconn->local_port=settings.local_port;
    iconn->local_ipstr=settings.local_ip;
    iconn->remote_port=settings.remote_port;
    iconn->remote_ipstr=settings.remote_ip;

    return IB_OK;
}

/**
 * @internal
 * Trace request processing.
 *
 * @param[in] ib IronBee object
 * @param[in] txdata Transaction data object
 * @param[in] cbdata Callback data
 *
 * @returns Status code
 */
static ib_status_t trace_tx_request(ib_engine_t *ib,
                                    ib_txdata_t *txdata,
                                    void *cbdata)
{
    IB_FTRACE_INIT(trace_tx_request);
    if (txdata->dtype == IB_DTYPE_HTTP_LINE) {
        settings.trace_request_cnt++;
        fprintf(stderr, "REQUEST[%d]: %.*s\n",
                (int)settings.trace_request_cnt,
                (int)(txdata->data[txdata->dlen] == '\n' ?
                      txdata->dlen : txdata->dlen - 1),
                txdata->data);
    }
    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * @internal
 * Trace request processing.
 *
 * @param[in] ib IronBee object
 * @param[in] txdata Transaction data object
 * @param[in] cbdata Callback data
 *
 * @returns Status code
 */
static ib_status_t trace_tx_response(ib_engine_t *ib,
                                     ib_txdata_t *txdata,
                                     void *cbdata)
{
    IB_FTRACE_INIT(trace_tx_response);
    if (txdata->dtype == IB_DTYPE_HTTP_LINE) {
        settings.trace_response_cnt++;
        fprintf(stderr, "RESPONSE[%d]: %.*s\n",
                (int)settings.trace_response_cnt,
                (int)(txdata->data[txdata->dlen] == '\n' ?
                      txdata->dlen : txdata->dlen - 1),
                txdata->data);
    }
    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * @internal
 * Print a field.
 *
 * Prints a field name and value; handles various field types.
 *
 * @param[in] label Label string
 * @param[in] field Field to print
 *
 * @returns void
 */
static void print_field(const char *label,
                        ib_field_t *field)
{
    ib_bytestr_t *bs = NULL;

    /* Check the field name
     * Note: field->name is not always a null ('\0') terminated string */
    switch (field->type) {
        case IB_FTYPE_NUM :          /**< Numeric value */
            printf( "  %s %.*s = %jd\n",
                    label, (int)field->nlen, field->name,
                    *(intmax_t *)ib_field_value_num(field) );
            break;
        case IB_FTYPE_UNUM :         /**< Unsigned numeric value */
            printf( "  %s %.*s = %ju\n",
                    label, (int)field->nlen, field->name,
                    *(uintmax_t *)ib_field_value_unum(field) );
            break;
        case IB_FTYPE_NULSTR :       /**< NUL terminated string value */
            printf( "  %s %.*s = '%s'\n",
                    label, (int)field->nlen, field->name,
                    ib_field_value_nulstr(field) );
            break;
        case IB_FTYPE_BYTESTR :      /**< Binary data value */
            bs = ib_field_value_bytestr(field);
            printf( "  %s %.*s = '%.*s'\n",
                    label, (int)field->nlen, field->name,
                    (int)ib_bytestr_length(bs), ib_bytestr_ptr(bs) );
            break;
    }
}

/**
 * @internal
 * Print transaction details.
 *
 * Extract the address & ports from the transaction & print them.
 *
 * @param[in] ib IronBee object
 * @param[in] tx Transaction object
 * @param[in] data Callback data (not used)
 *
 * @returns Status code
 */
static ib_status_t print_tx(ib_engine_t *ib,
                            ib_tx_t *tx,
                            void *data)
{
    IB_FTRACE_INIT(print_tx);

    printf( "%s:%d -> %s:%d\n",
            tx->er_ipstr, tx->conn->remote_port,
            tx->conn->local_ipstr, tx->conn->local_port );

    /* Done */
    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * @internal
 * Print user agent fields
 *
 * Extract the user agent fields from the data provider instance, and print
 * fields.
 *
 * @param[in] ib IronBee object
 * @param[in] tx Transaction object
 * @param[in] data Callback data (not used)
 *
 * @note The data parameter is unused
 *
 * @returns Status code
 */
static ib_status_t print_user_agent(ib_engine_t *ib,
                                    ib_tx_t *tx,
                                    void *data)
{
    IB_FTRACE_INIT(print_user_agent);
    ib_field_t *req = NULL;
    ib_status_t rc = IB_OK;
    ib_list_t *lst = NULL;
    ib_list_node_t *node = NULL;

    /* Extract the request headers field from the provider instance */
    rc = ib_data_get(tx->dpi, "User-Agent", &req);
    if ( (req == NULL) || (rc != IB_OK) ) {
        ib_log_debug(ib, 4,
                     "print_user_agent: No user agent info available" );
        IB_FTRACE_RET_STATUS(IB_EUNKNOWN);
    }

    /* The field value *should* be a list, extract it as such */
    lst = ib_field_value_list(req);
    if (lst == NULL) {
        ib_log_debug(ib, 4,
                     "print_user_agent: "
                     "Field list missing / incorrect type" );
        IB_FTRACE_RET_STATUS(IB_EUNKNOWN);
    }

    /* Loop through the list & print everything */
    printf("User Agent information:\n");
    IB_LIST_LOOP(lst, node) {
        ib_field_t *field = (ib_field_t *)ib_list_node_data(node);
        print_field("User Agent", field);
    }

    /* Done */
    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * @internal
 * Print GeoIP fields
 *
 * Extract the GeoIP fields from the data provider instance, and print
 * fields.
 *
 * @param[in] ib IronBee object
 * @param[in] tx Transaction object
 * @param[in] data Callback data (not used)
 *
 * @note The data parameter is unused
 *
 * @returns Status code
 */
static ib_status_t print_geoip(ib_engine_t *ib,
                               ib_tx_t *tx,
                               void *data)
{
    IB_FTRACE_INIT(print_geoip);
    ib_field_t *req = NULL;
    ib_status_t rc = IB_OK;
    ib_list_t *lst = NULL;
    ib_list_node_t *node = NULL;
    int count = 0;

    /* Extract the request headers field from the provider instance */
    rc = ib_data_get(tx->dpi, "GEOIP", &req);
    if ( (req == NULL) || (rc != IB_OK) ) {
        ib_log_debug(ib, 4, "print_geoip: No GeoIP info available" );
        IB_FTRACE_RET_STATUS(IB_EUNKNOWN);
    }

    /* The field value *should* be a list, extract it as such */
    lst = ib_field_value_list(req);
    if (lst == NULL) {
        ib_log_debug(ib, 4,
                     "print_geoip: Field list missing / incorrect type" );
        IB_FTRACE_RET_STATUS(IB_EUNKNOWN);
    }

    /* Loop through the list & print everything */
    IB_LIST_LOOP(lst, node) {
        ib_field_t *field = (ib_field_t *)ib_list_node_data(node);
        if (count++ == 0) {
            printf("GeoIP data:\n");
        }
        print_field("", field);
    }
    if (count == 0) {
        printf("No GeoIP data found\n");
    }

    /* Done */
    IB_FTRACE_RET_STATUS(IB_OK);
}


/**
 * @internal
 * Add a line to the request header buffer.
 *
 * This function adds a line to header.  This function uses malloc() &
 * realloc() instead of the IronBee memory pool because the memory pool
 * currently doesn't provide a realloc() equivilent.
 *
 * @param[in] buf Request header buffer
 * @param[in] linebuf Line to copy in
 * @param[in] linelen Length of line
 *
 * @returns status
 */
static ib_status_t append_req_hdr_buf(reqhdr_buf_t *buf,
                                      const char *linebuf,
                                      size_t linelen)
{

    /* Allocate a buffer or increase our allocation as required */
    if (buf->buf == NULL) {
        buf->buf  = malloc(MAX_LINE_BUF);
        buf->size = MAX_LINE_BUF;
        buf->len  = 0;
    }
    else if ((buf->len + linelen) > buf->size) {
        buf->size *= 2;
        buf->buf   = realloc(buf, buf->size);
    }

    /* Allocation failed? */
    if (buf->buf == NULL) {
        fprintf(stderr,
                "Failed to allocate request buffer of size %zd", buf->size);
        return IB_EALLOC;
    }

    /* Copy the line into the buffer */
    memcpy(buf->buf+buf->len, linebuf, linelen);
    buf->len += linelen;

    return IB_OK;
}

/**
 * @internal
 * Register event handlers.
 *
 * Register event handlers to print specific pieces of data; which ones are
 * registered is controlled via command line arguments.
 *
 * @param[in] ib IronBee object
 *
 * @returns status
 */
static ib_status_t register_handlers(ib_engine_t* ib)
{
    IB_FTRACE_INIT(register_handlers);
    ib_status_t rc;
    ib_status_t status = IB_OK;

    /* Register the trace handlers */
    if (settings.trace) {
        if (settings.verbose > 2) {
            printf("Registering trace handlers\n");
        }

        /* Register the request trace handler. */
        rc = ib_hook_register(ib, tx_data_in_event,
                              (ib_void_fn_t)trace_tx_request, NULL);
        if (rc != IB_OK) {
            fprintf(stderr, "Failed to register tx request handler: %d\n", rc);
            status = rc;
        }

        /* Register the response trace handler. */
        rc = ib_hook_register(ib, tx_data_out_event,
                              (ib_void_fn_t)trace_tx_response, NULL);
        if (rc != IB_OK) {
            fprintf(stderr, "Failed to register tx response handler: %d\n", rc);
            status = rc;
        }
    }

    /* Register the tx handler */
    if (settings.dump_tx != 0) {
        if (settings.verbose > 2) {
            printf("Registering tx handlers\n");
        }
        rc = ib_hook_register(ib, handle_context_tx_event,
                              (ib_void_fn_t)print_tx, NULL);
        if (rc != IB_OK) {
            fprintf(stderr, "Failed to register tx handler: %d\n", rc);
            status = rc;
        }
    }
    /* Register the user agent handler */
    if (settings.dump_user_agent != 0) {
        if (settings.verbose > 2) {
            printf("Registering user agent handlers\n");
        }
        rc = ib_hook_register(ib, request_headers_event,
                              (ib_void_fn_t)print_user_agent, NULL);
        if (rc != IB_OK) {
            fprintf(stderr, "Failed to register user_agent handler: %d\n", rc);
            status = rc;
        }
    }
    /* Register the GeoIP handler */
    if (settings.dump_geoip != 0) {
        if (settings.verbose > 2) {
            printf("Registering GeoIP handlers\n");
        }
        rc = ib_hook_register(ib, handle_context_tx_event,
                              (ib_void_fn_t)print_geoip, NULL);
        if (rc != IB_OK) {
            fprintf(stderr, "Failed to register geoip handler: %d\n", rc);
            status = rc;
        }
    }
    IB_FTRACE_RET_STATUS(status);
}

/**
 * @internal
 * Simulate the reception of request header.
 *
 * Do the work to simulate the recieving of the header, replacing / adding
 * fields to it, etc.  This function uses malloc() & realloc() instead of the
 * IronBee memory pool because the memory pool currently doesn't provide a
 * realloc() equivilent.
 *
 * @param[in] ib IronBee engine to send header to
 * @param[in] icdata IronBee connection data
 * @param[in] fp File pointer to read from
 *
 * @returns status
 */
static ib_status_t send_header(ib_engine_t* ib,
                               ib_conndata_t *icdata,
                               FILE *fp)
{
    IB_FTRACE_INIT(send_header);
    ib_status_t  rc;
    reqhdr_buf_t rbuf;            /* Request header buffer for I/O */
    int          fnum;            /* Request header field number */
    static char  linebuf[MAX_LINE_BUF];
    const char  *lineptr;

    /* Initialize our buffer to zero */
    rbuf.buf = NULL;

    /* Reset the request header used flags */
    for (fnum = 0; fnum < settings.request_headers.num_headers; ++fnum) {
        settings.request_headers.headers[fnum].used = 0;
    }

    /* Read the request header from the file, assembled the header, pass
     * it to IronBee */
    while (fgets(linebuf, sizeof(linebuf), fp) != NULL) {
        size_t linelen = strlen(linebuf);

        /* By default, lineptr points at the line buffer */
        lineptr = linebuf;

        /* Is this a header that we need to replace? */
        if ( (*linebuf == '\0') || (isspace(*linebuf) != 0) ) {
            break;
        }

        /* Walk through the header fields, see if we have a replacement */
        for (fnum = 0; fnum < settings.request_headers.num_headers; ++fnum) {
            request_header_t *rhf = &settings.request_headers.headers[fnum];

            /* Already used? */
            if (rhf->used != 0) {
                continue;
            }

            /* Matching header field? */
            if (strncmp(linebuf, rhf->name, rhf->name_len) != 0) {
                continue;
            }

            /* Consume the request header field */
            ++(rhf->used);

            /* Point at the request header field buffer */
            lineptr = rhf->buf;
            linelen = rhf->buf_len;
        }

        /* Add the line to the request header */
        if (lineptr != NULL) {
            rc = append_req_hdr_buf(&rbuf, lineptr, linelen);
            if (rc != IB_OK) {
                IB_FTRACE_RET_STATUS(rc);
            }
        }
    }

    /* Walk through the unused header fields, add them to the buffer */
    for (fnum = 0; fnum < settings.request_headers.num_headers; ++fnum) {
        request_header_t *rhf = &settings.request_headers.headers[fnum];

        /* Already used? */
        if (rhf->used != 0) {
            continue;
        }

        /* Consume the request header field */
        ++(rhf->used);

        /* Add it */
        if (rhf->buf != NULL) {
            rc = append_req_hdr_buf(&rbuf, rhf->buf, rhf->buf_len);
            if (rc != IB_OK) {
                IB_FTRACE_RET_STATUS(rc);
            }
        }
    }

    /* No buffer means no header => bad */
    if (rbuf.buf == NULL) {
        fprintf(stderr, "WARNING: No request header found in file\n");
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    /* Add a empty line */
    rc = append_req_hdr_buf(&rbuf, "\r\n", 2);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Send it */
    icdata->dalloc = rbuf.size;
    icdata->dlen   = rbuf.len;
    icdata->data   = (uint8_t *)rbuf.buf;
    rc = ib_state_notify_conn_data_in(ib, icdata);
    if (rc != IB_OK) {
        fprintf(stderr, "Failed to send header: %d\n", rc);
    }

    /* Free the buffer */
    free(rbuf.buf);

    /* Done */
    IB_FTRACE_RET_STATUS(rc);
}

/**
 * @internal
 * Send a file to IB as either connection input data or connection output data
 *
 * Reads from the file pointer into the buffer, calls the appropriate
 * ib_state_notify_conn_data_* function.
 *
 * @param[in] ib IronBee object
 * @param[in] icdata IronBee connection data
 * @param[in] buf Buffer to use for I/O
 * @param[in] bufsize Size of buf
 * @param[in] fp File pointer to read from
 * @param[in] direction Data direction: Input (DATA_IN) or output (DATA_OUT)
 *
 * @returns status
 */
static ib_status_t send_file(ib_engine_t* ib,
                             ib_conndata_t *icdata,
                             void *buf,
                             size_t bufsize,
                             FILE *fp,
                             data_direction_t direction)
{
    IB_FTRACE_INIT(send_file);
    size_t      nbytes = 0;     /* # bytes currently in the buffer */
    ib_status_t rc;
    const char *ioname = (direction == DATA_IN) ? "input" : "output";

    /* Read a chunk & send it */
    if ( (nbytes = fread(buf, 1, bufsize, fp)) > 0) {
        icdata->dalloc = bufsize;
        icdata->dlen = nbytes;
        icdata->data = (uint8_t *)buf;

        if (direction == DATA_IN) {
            rc = ib_state_notify_conn_data_in(ib, icdata);
        }
        else {
            rc = ib_state_notify_conn_data_out(ib, icdata);
        }
        if (rc != IB_OK) {
            fprintf(stderr,
                    "Failed to send %s data to IronBee: %d\n", ioname, rc);
            IB_FTRACE_RET_STATUS(rc);
        }
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * @internal
 * Run connection.
 *
 * Do the work to simulate a transaction on a connection
 *
 * @param[in] ib IronBee object
 *
 * @returns void
 */
static ib_status_t run_transaction(ib_engine_t* ib,
                                   ib_conn_t *conn,
                                   void *buf,
                                   size_t bufsize,
                                   size_t trans_num,
                                   const char *req_file,
                                   const char *rsp_file)
{
    IB_FTRACE_INIT(run_transaction);
    FILE          *reqfp  = NULL;
    FILE          *rspfp = NULL;
    ib_conndata_t  conn_data;
    ib_status_t    rc;

    /* Open the request and response files that we'll use for this 
     * transaction */
    if (settings.verbose >= 2) {
        printf("Transaction #%zd:\n"
               "  req=%s\n"
               "  rsp=%s\n",
               trans_num, req_file, rsp_file);
    }
    else if (settings.verbose >= 1) {
        char *req = strdup(req_file);
        char *rsp = strdup(rsp_file);
        printf("Transaction #%zd: req=%s rsp=%s\n",
               trans_num, basename(req), basename(rsp) );
    }
    reqfp = fopen(req_file, "rb");
    if (reqfp == NULL) {
        fprintf(stderr, "Error opening request file '%s'\n", req_file);
        rc = IB_EOTHER;
        goto end;
    }
    rspfp = fopen(rsp_file, "rb");
    if (rspfp == NULL) {
        fprintf(stderr, "Error opening response file '%s'\n", rsp_file);
        rc = IB_EOTHER;
        goto end;
    }

    // Fill in the connection data object
    conn_data.ib   = ib;
    conn_data.mp   = conn->mp;
    conn_data.conn = conn;

    /* Read the request header from the file, assembled the header, pass
     * it to IronBee */
    rc = send_header(ib, &conn_data, reqfp);
    if (rc != IB_OK) {
        fprintf(stderr, "Failed to read/send header data: %d\n", rc);
        goto end;
    }

    /* Read and send the rest of the file (if any) */
    rc = send_file(ib, &conn_data, buf, MAX_BUF, reqfp, DATA_IN);
    if (rc != IB_OK) {
        fprintf(stderr, "Failed to read/send input data: %d\n", rc);
        goto end;
    }

    /* Read and send the rest of the file (if any) */
    rc = send_file(ib, &conn_data, buf, MAX_BUF, rspfp, DATA_OUT);
    if (rc != IB_OK) {
        fprintf(stderr, "Failed to read/send output data: %d\n", rc);
        goto end;
    }
 
    /* If we have mde it this far everything is okay */
    rc = IB_OK;

end:
    /* Close files. */
    if (reqfp != NULL)
        fclose(reqfp);
    if (rspfp != NULL)
        fclose(rspfp);

    /* Done */
    IB_FTRACE_RET_STATUS(rc);
}

/**
 * @internal
 * Run connection.
 *
 * Do the work to simulate a connection and it's traffic
 *
 * @param[in] ib IronBee object
 *
 * @returns void
 */
static void run_connection(ib_engine_t* ib)
{
    IB_FTRACE_INIT(run_connection);
    ib_status_t    rc;
    ib_conn_t     *conn = NULL;
    char          *buf = NULL;      /* I/O buffer */
    size_t         trans_num;       /* Transaction number */
    size_t         max_trans;       /* Max # of transactions */
    size_t         nreq = settings.req_files.gl_pathc;
    size_t         nrsp = settings.rsp_files.gl_pathc;


    /* Register the event handlers late so they run after the relevant
     * module's event handler */
    rc = register_handlers(ib);
    if (rc != IB_OK) {
        fprintf(stderr, "Failed to register one or more handlers\n");
    }

    // Create a connection
    ib_conn_create(ib, &conn, NULL);
    ib_state_notify_conn_opened(ib, conn);

    /* Allocate a buffer for the remainder of the I/O */
    buf = ib_mpool_alloc(conn->mp, MAX_BUF);
    if (buf == NULL) {
        fprintf(stderr, "Failed to allocate I/O buffer (%d bytes)\n", MAX_BUF);
        goto end;
    }

    /* Loop through our files, send them */
    max_trans = (nreq > nrsp) ? nreq : nrsp;
    if ( (settings.max_transactions > 0)
         && (max_trans > (size_t)settings.max_transactions) ) {
        max_trans = (size_t)settings.max_transactions;
    }
    for (trans_num = 0;  trans_num < max_trans;  trans_num++) {
        size_t req_num = (nreq == 1) ? 0 : trans_num;
        size_t rsp_num = (nrsp == 1) ? 0 : trans_num;

        /* Run the transaction */
        rc = run_transaction(ib,
                             conn,
                             buf,
                             MAX_BUF,
                             trans_num+1,
                             settings.req_files.gl_pathv[req_num],
                             settings.rsp_files.gl_pathv[rsp_num]);
        if (rc != IB_OK) {
            fprintf(stderr, "run_transaction failed: %d\n", rc);
            break;
        }
    }

end:
    /* Close the connection */
    ib_state_notify_conn_closed(ib, conn);

    /* Print trace request/response count */
    if (settings.trace) {
        fprintf(stderr, "Trace Request Count: %" PRIu64 
                " Trace Response Count : %" PRIu64 "\n",
                settings.trace_request_cnt, settings.trace_response_cnt);
    }

    IB_FTRACE_RET_VOID();
}


/**
 * @internal
 * Perform clean up operations.
 *
 * Clean up, free memory, etc.
 *
 * @returns void
 */
static void clean_up( void )
{
    int num;

    globfree(&settings.req_files);
    globfree(&settings.rsp_files);

    /* Free request header buffers */
    for (num = 0; num < settings.request_headers.num_headers; ++num) {
        free((void*)settings.request_headers.headers[num].buf);
        settings.request_headers.headers[num].buf = NULL;
    }
}


/**
 * @internal
 * Main.
 *
 * Main program for the IronBee command line client.
 *
 * @param[in] argc Command line argument count
 * @param[in] argv Command line argument array
 *
 * @returns exit status
 */
int main(int argc, char* argv[])
{
    ib_status_t rc;
    ib_engine_t *ironbee = NULL;
    ib_context_t *ctx;
    ib_cfgparser_t *cp;

    ib_trace_init(NULL);

    /* Process the command line */
    rc = command_line(argc, argv);
    if (rc != IB_OK) {
        fatal_error("Error processing command line");
    }

    /* Initialize IronBee */
    rc = ib_initialize();
    if (rc != IB_OK) {
        fatal_error("Error initializing ironbee library");
    }

    /* Create an IronBee engine */
    rc = ib_engine_create(&ironbee, &ibplugin);
    if (rc != IB_OK) {
        fatal_error("Error creating engine: %d\n", rc);
    }

    /* Initialize the engine */
    rc = ib_engine_init(ironbee);
    if (rc != IB_OK) {
        fatal_error("Error initializing engine: %d\n", rc);
    }

    /* Register a connection open event handler */
    ib_hook_register(ironbee, conn_opened_event,
                     (ib_void_fn_t)ironbee_conn_init, NULL);

    /* Set the engine's debug flags from the command line args */
#if DEBUG_ARGS_ENABLE
    set_debug( ib_context_engine(ironbee) );
#endif

    /* Notify the engine that the config process has started. */
    ib_state_notify_cfg_started(ironbee);

    /* Set the main context's debug flags from the command line args */
#if DEBUG_ARGS_ENABLE
    set_debug( ib_context_main(ironbee) );
#endif

    /* Parse the config file. */
    rc = ib_cfgparser_create(&cp, ironbee);
    if ((rc == IB_OK) && (cp != NULL)) {
        ib_cfgparser_parse(cp, settings.config_file);
        ib_cfgparser_destroy(cp);
    }

    /* Set all contexts' debug flags from the command line args
     * We do this because they may have been overwritten by DebugLog
     * directives. */ 
#if DEBUG_ARGS_ENABLE
    if ( (settings.debug_level >= 0) || (settings.debug_uri != NULL) ) {
        ib_context_t *ctx = NULL;
        size_t nctx;
        size_t i;
        IB_ARRAY_LOOP( ironbee->contexts, nctx, i, ctx ) {

            set_debug( ctx );
        }
    }
#endif

    /* Sanity checks */
    ctx = ib_context_main(ironbee);
    if ( ctx == NULL ) {
        fatal_error("Failed to get main context\n");
    }
    if ( ib_context_get_engine(ctx) != ironbee ) {
        fatal_error("ib_context_get_engine returned invalid engine pointer\n");
    }

    /* Notify the engine that the config process is finished. */
    ib_state_notify_cfg_finished(ironbee);

    /* Pass connection data to the engine. */
    run_connection(ironbee);

    /* Done */
    ib_engine_destroy(ironbee);

    /* Free up memory, etc. */
    clean_up( );

    return 0;
}
