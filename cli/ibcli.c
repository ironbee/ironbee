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
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

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
#define MAX_REQUEST_HEADERS  8

/* Header / value pairs */
typedef struct {
    const char *name;           /* Name of the header field */
    int         nlen;           /* Name length */
    const char *value;          /* Value of the header field */
    int         vlen;           /* Value length */
    int         used;           /* Has this field been used */
} request_header_t;

/* Runtime settings */
typedef struct {
    char *config_file;
    char *request_file;
    char *response_file;

    /* Local and remote IP address / port */
    const char *local_ip;
    int local_port;
    const char *remote_ip;
    int remote_port;

    /* Dump output */
    int dump_tx;
    int dump_user_agent;
    int dump_geoip;

    /* Request header fields */
    struct {
        int              num_headers;
        request_header_t headers[MAX_REQUEST_HEADERS];
    } request_headers;

    /* Debug arguments */
#if DEBUG_ARGS_ENABLE
    const char *debug_uri;
    int debug_level;
#endif
} runtime_settings_t;

static runtime_settings_t settings =
{
    NULL,                  /* config_file */
    NULL,                  /* request_file */
    NULL,                  /* response_file */
    "192.168.1.1",         /* local_ip */
    8080,                  /* local_port */
    "10.10.10.10",         /* remote_ip */
    23424,                 /* remote_port */
    0,                     /* dump_user_agent */
    0,                     /* dump_effective_ip */
    0,                     /* dump_geoip */
    { 0 },                 /* request_headers */
#if DEBUG_ARGS_ENABLE
    NULL,                  /* debug_uri */
    -1                     /* debug_level */
#endif
};

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
 * @param[in] values Array of possible option values or NULL
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
    print_option("local-ip", "x.x.x.x", "Specify local IP address", 0, NULL );
    print_option("local-port", "num", "Specify local port", 0, NULL );
    print_option("remote-ip", "x.x.x.x", "Specify remote IP address", 0, NULL );
    print_option("remote-port", "num", "Specify remote port", 0, NULL );
    print_option("dump", "name", "Dump specified field", 0,
                 "tx, user-agent, geop");
    print_option("request-header", "name=value",
                 "Specify request field & value", 0, NULL );
#if DEBUG_ARGS_ENABLE
    print_option("debug-level", "path", "Specify debug log level", 0, NULL );
    print_option("debug-log", "path", "Specify debug log file / URI", 0, NULL );
#endif
    print_option("help", NULL, "Print this help", 0, NULL );
    exit(0);
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
 * Initialize the connection.
 *
 * Sets the connection local/remote address/port
 *
 * @param[in] ib IronBee object
 * @param[in] tx Transaction object
 * @param[in] cbdata Callback data (not used)
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
                    (intmax_t)ib_field_value_num(field) );
            break;
        case IB_FTYPE_UNUM :         /**< Unsigned numeric value */
            printf( "  %s %.*s = %ju\n",
                    label, (int)field->nlen, field->name,
                    (uintmax_t)ib_field_value_unum(field) );
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

    /* fputs(settings.user_agent, stdout); */

    /* Loop through the list & print everything */
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
    ib_status_t rc;
    char *buf = NULL;     /* I/O buffer */
    size_t bufsize = 0;   /* Size of buffer */
    size_t buflen  = 0;   /* Actual size of data in the buffer */
    static char linebuf[MAX_LINE_BUF];

    /* Read the request header from the file, assembled the header, pass
     * it to IronBee */
    while (fgets(linebuf, sizeof(linebuf), fp) != NULL) {
        size_t linelen = strlen(linebuf);
        int    fnum;       /* Request header field number */

        /* Is this a header that we need to replace? */
        if( (*linebuf == '\0') || (isspace(*linebuf) != 0) ) {
            break;
        }

        for (fnum = 0; fnum < settings.request_headers.num_headers; ++fnum) {
            request_header_t *rh = &settings.request_headers.headers[fnum];

            /* Already used? */
            if (rh->used != 0) {
                continue;
            }

            /* Matching header field? */
            if (strncmp(linebuf, rh->name, rh->nlen) != 0) {
                continue;
            }

            /* Consume the request header field */
            ++(rh->used);

            /* Make sure that the name + value is not too big for the line
             * buffer (should realistically never happen) */
            linelen = rh->nlen + rh->vlen + 2;
            if (linelen > sizeof(linebuf)) {
                fprintf(stderr,
                        "WARNING: Clipping request header line '%s'\n",
                        rh->name);
                linelen = sizeof(linebuf);
            }
            linebuf[rh->nlen]   = ' ';
            linebuf[rh->nlen+1] = '\0';
            strncat(linebuf, rh->value, sizeof(linebuf)-(rh->nlen+2));
        }

        /* Allocate a buffer or increase our allocation as required */
        if (buf == NULL) {
            bufsize = MAX_LINE_BUF;
            buf     = malloc(bufsize);
            buflen  = 0;
        }
        else if ((buflen + linelen) > bufsize) {
            bufsize *= 2;
            buf      = realloc(buf, bufsize);
        }
        if (buf == NULL) {
            fprintf(stderr,
                    "Failed to allocate request buffer of size %zd", bufsize);
            exit(1);
        }

        /* Finally, copy the line into the buffer */
        memcpy(buf+buflen, linebuf, linelen);
        buflen += linelen;
    }

    /* No buffer means no header => bad */
    if (buf == NULL) {
        fprintf(stderr, "WARNING: No request header found in file\n");
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    /* Send it */
    icdata->dalloc = bufsize;
    icdata->dlen   = buflen;
    icdata->data   = (uint8_t *)buf;
    rc = ib_state_notify_conn_data_in(ib, icdata);
    if (rc != IB_OK) {
        fprintf(stderr, "Failed to send header: %d\n", rc);
    }

    /* Free the buffer */
    free(buf);

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
 *
 * @returns status
 */
static ib_status_t send_file(ib_engine_t* ib,
                             ib_conndata_t *icdata,
                             void *buf,
                             size_t bufsize,
                             FILE *fp,
                             data_direction_t io)
{
    IB_FTRACE_INIT(send_file);
    size_t      nbytes = 0;     /* # bytes currently in the buffer */
    ib_status_t rc;
    const char *dname = (io == DATA_IN) ? "input" : "output";

    /* Read a chunk & send it */
    if ( (nbytes = fread(buf, bufsize, 1, fp)) > 0) {
        icdata->dalloc = bufsize;
        icdata->dlen = nbytes;
        icdata->data = (uint8_t *)buf;
        if (io == DATA_IN) {
            rc = ib_state_notify_conn_data_in(ib, icdata);
        }
        else {
            rc = ib_state_notify_conn_data_out(ib, icdata);
        }
        if (rc != IB_OK) {
            fprintf(stderr,
                    "Failed to send %s data to IronBee: %d\n", dname, rc);
            IB_FTRACE_RET_STATUS(rc);
        }
    }
    IB_FTRACE_RET_STATUS(IB_OK);
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
    FILE          *reqfp  = NULL;
    FILE          *respfp = NULL;
    ib_conn_t     *conn = NULL;
    ib_conndata_t  conn_data;
    ib_status_t    rc;
    char          *buf = NULL;     /* I/O buffer */

    /* Register the event handlers late so they run after the relevant
     * module's event handler */
    if (settings.dump_tx != 0) {
        rc = ib_hook_register(ib, handle_context_tx_event,
                              (ib_void_fn_t)print_tx, NULL);
        if (rc != IB_OK) {
            fprintf(stderr, "Failed to register tx handler: %d\n", rc);
        }
    }
    if (settings.dump_user_agent != 0) {
        rc = ib_hook_register(ib, request_headers_event,
                              (ib_void_fn_t)print_user_agent, NULL);
        if (rc != IB_OK) {
            fprintf(stderr, "Failed to register user_agent handler: %d\n", rc);
        }
    }
    if (settings.dump_geoip != 0) {
        rc = ib_hook_register(ib, handle_context_tx_event,
                              (ib_void_fn_t)print_geoip, NULL);
        if (rc != IB_OK) {
            fprintf(stderr, "Failed to register geoip handler: %d\n", rc);
        }
    }

    /* Open the files that we'll use for I/O */
    if (! strcmp("-", settings.request_file)) {
        reqfp = stdin;
    }
    else {
        reqfp = fopen(settings.request_file, "rb");
        if (reqfp == NULL) {
            fatal_error("Error opening request file '%s'",
                        settings.request_file);
        }
    }

    if (settings.response_file) {
        respfp = fopen(settings.response_file, "rb");
        if (respfp == NULL) {
            fatal_error("Error opening response file '%s'",
                        settings.response_file);
        }
    }

    // Create a connection
    ib_conn_create(ib, &conn, NULL);
    ib_state_notify_conn_opened(ib, conn);
    conn_data.ib   = ib;
    conn_data.mp   = conn->mp;
    conn_data.conn = conn;

    /* Read the request header from the file, assembled the header, pass
     * it to IronBee */
    rc = send_header(ib, &conn_data, reqfp);

    /* Allocate a buffer for the remainder of the I/O */
    buf = ib_mpool_alloc(conn->mp, MAX_BUF);
    if (buf == NULL) {
        fprintf(stderr, "Failed to allocate I/O buffer (%d bytes)\n", MAX_BUF);
        ib_state_notify_conn_closed(ib, conn);
        IB_FTRACE_RET_VOID();
    }

    /* Read and send the rest of the file (if any) */
    rc = send_file(ib, &conn_data, buf, MAX_BUF, reqfp, DATA_IN);
    if (rc != IB_OK) {
        fprintf(stderr, "Failed to read/send input data: %d\n", rc);
    }

    /* Read and send the rest of the file (if any) */
    rc = send_file(ib, &conn_data, buf, MAX_BUF, respfp, DATA_OUT);
    if (rc != IB_OK) {
        fprintf(stderr, "Failed to read/send output data: %d\n", rc);
    }

    /* Close the connection */
    ib_state_notify_conn_closed(ib, conn);

    /* Done */
    IB_FTRACE_RET_VOID();
}

/**
 * @internal
 * Add a request header field / value.
 *
 * Attempts to add the specified request header name and value to the list
 * of header fields.
 *
 * @param[in] name Request header field name
 * @param[in] value Request header field value
 *
 * @returns status
 */
static ib_status_t add_request_header(const char *name, const char *value)
{
    int  num;
    if (settings.request_headers.num_headers >= MAX_REQUEST_HEADERS) {
        fprintf(stderr,
                "Unable to add request header %s: no space in array",
                name);
        return IB_EALLOC;
    }
    num = ++settings.request_headers.num_headers;
    settings.request_headers.headers[num].name  = name;
    settings.request_headers.headers[num].nlen  = strlen(name);
    settings.request_headers.headers[num].value = value;
    settings.request_headers.headers[num].vlen  = strlen(value);
    settings.request_headers.headers[num].used  = 0;

    return IB_OK;
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
    ib_cfgparser_t *cp;

    struct option longopts[] =
        {
            { "config", required_argument, 0, 0 },
            { "request-file", required_argument, 0, 0 },
            { "response-file", required_argument, 0, 0 },
	    { "local-ip", required_argument, 0, 0 },
	    { "local-port", required_argument, 0, 0 },
	    { "remote-ip", required_argument, 0, 0 },
	    { "remote-port", required_argument, 0, 0 },

	    { "request-header", required_argument, 0, 0 },
	    { "dump", required_argument, 0, 0 },

#if DEBUG_ARGS_ENABLE
	    { "debug-level", required_argument, 0, 0 },
	    { "debug-log", required_argument, 0, 0 },
#endif
            { "help", no_argument, 0, 0 },
            { 0, 0, 0, 0}
        };

    ib_trace_init(NULL);
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

        if (! strcmp("config", longopts[option_index].name)) {
            settings.config_file = optarg;
        }
        else if (! strcmp("request-file", longopts[option_index].name)) {
            settings.request_file = optarg;
        }
        else if (! strcmp("response-file", longopts[option_index].name)) {
            settings.response_file = optarg;
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
#if 0
        else if (! strcmp("user-agent", longopts[option_index].name)) {
            static char buf[MAX_LINE_BUF];
            strcpy(buf, "User-Agent: ");
            strncat(buf, optarg, sizeof(buf)-(1+strlen(buf)));
            strncat(buf, "\r\n", sizeof(buf)-(1+strlen(buf)));
            settings.user_agent = buf;
        }
#endif
        else if (! strcmp("request-header", longopts[option_index].name)) {
            size_t vlen = 0;       /* Value length */
            char *name;
            char *value;
            char *colon;

            /* Get a pointer to the first colon, count the number of
             * characters following it */
            colon = strchr(optarg, ':');
            if (colon != NULL) {
                vlen = strlen(colon+1);
            }

            /* Simple checks */
            if ( (colon == NULL) || (colon == optarg) || (vlen == 0) ) {
                fprintf(stderr,
                        "No value in request-header parameter '%s'",
                        optarg);
                usage();
            }

            /* Allocate space for and copy the name & value */
            name  = strndup(optarg, (size_t)(1+colon-optarg));
            value = (char *)malloc(vlen+3);
            if ( (name == NULL) || (value == NULL) ) {
                fprintf(stderr,
                        "Failed to allocate buffer for request header '%s'",
                        optarg);
                exit(1);
            }
            strcpy(value, colon+1);
            strcat(value, "\r\n");
            rc = add_request_header(name, value);
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
    if (settings.request_file == NULL) {
        fprintf(stderr, "--request_file <file> is required\n");
        usage();
    }

    rc = ib_initialize();
    if (rc != IB_OK) {
        fatal_error("Error initializing ironbee library");
    }

    rc = ib_engine_create(&ironbee, &ibplugin);
    if (rc != IB_OK) {
        fatal_error("Error creating engine: %d\n", rc);
    }

    rc = ib_engine_init(ironbee);
    if (rc != IB_OK) {
        fatal_error("Error initializing engine: %d\n", rc);
    }
    ib_hook_register(ironbee, conn_opened_event,
                     (ib_void_fn_t)ironbee_conn_init, NULL);

    // Set the engine's debug flags from the command line args
#if DEBUG_ARGS_ENABLE
    set_debug( ib_context_engine(ironbee) );
#endif

    /* Notify the engine that the config process has started. */
    ib_state_notify_cfg_started(ironbee);

    // Set the main context's debug flags from the command line args
#if DEBUG_ARGS_ENABLE
    set_debug( ib_context_main(ironbee) );
#endif

    /* Parse the config file. */
    rc = ib_cfgparser_create(&cp, ironbee);
    if ((rc == IB_OK) && (cp != NULL)) {
        ib_cfgparser_parse(cp, settings.config_file);
        ib_cfgparser_destroy(cp);
    }

    // Set all contexts' debug flags from the command line args
    // We do this because they may have been overwritten by DebugLog
    // directives.
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

    /* Notify the engine that the config process is finished. */
    ib_state_notify_cfg_finished(ironbee);

    /* Pass connection data to the engine. */
    run_connection(ironbee);

    ib_engine_destroy(ironbee);
}
