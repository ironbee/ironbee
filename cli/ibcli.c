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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include <ironbee/engine.h>
#include <ironbee/plugin.h>
#include <ironbee/provider.h>
#include <ironbee/module.h>
#include <ironbee/config.h>
#include <ironbee/bytestr.h>
#include <ironbee_private.h>

// Set DEBUG_ARGS_ENABLE to non-zero enable the debug log command line
// handling.  It's currently disabled because DebugLog and DebugLogLevel
// directives in the configuration will overwrite the command-line version,
// which can cause the CLI to log back and forth between the two files.
#define DEBUG_ARGS_ENABLE 0


typedef struct {
    char *configfile;
    char *requestfile;
    char *responsefile;
    const char *localip;
    int localport;
    const char *remoteip;
    int remoteport;
    const char *user_agent;
#if DEBUG_ARGS_ENABLE
    const char *debuguri;
    int debuglevel;
#endif
} runtime_settings_t;

static runtime_settings_t settings =
{NULL,NULL,NULL,"192.168.1.1",8080,"10.10.10.10",23424,NULL,
#if DEBUG_ARGS_ENABLE
,NULL,-1
#endif
};

#define MAX_BUF      (64*1024)
#define MAX_LINE_BUF (16*1024)

/* Plugin Structure */
ib_plugin_t ibplugin = {
    IB_PLUGIN_HEADER_DEFAULTS,
    "ibcli"
};

static void usage(void)
{
    fprintf(stderr, "Usage: ibcli <options>\n");
    fprintf(stderr, "  Use --help for help\n");
    exit(1);
}

static void print_option(const char *opt, const char *param,
                         const char *desc, int required)
{
    char buf[64];
    const char *req = "";
    if ( param == NULL ) {
        snprintf( buf, sizeof(buf), "--%s", opt );
    }
    else {
        snprintf( buf, sizeof(buf), "--%s <%s>", opt, param );
   }
    if ( required ) {
        req = "[Required]";
    }
    printf( "  %-24s: %s %s\n", buf, desc, req );
}

static void help(void)
{
    printf("Usage: ibcli <options>\n");
    printf("Options:\n");
    print_option("config", "path", "Specify configuration file", 1 );
    print_option("requestfile", "path", "Specify request file", 1 );
    print_option("responsefile", "path", "Specify response file", 1 );
    print_option("local-ip", "x.x.x.x", "Specify local IP address", 0 );
    print_option("local-port", "num", "Specify local port", 0 );
    print_option("remote-ip", "x.x.x.x", "Specify remote IP address", 0 );
    print_option("remote-port", "num", "Specify remote port", 0 );
    print_option("user-agent", "string", "Specify user agent string", 0 );
#if DEBUG_ARGS_ENABLE
    print_option("debug-level", "path", "Specify debug log level", 0 );
    print_option("debug-log", "path", "Specify debug log file / URI", 0 );
#endif
    print_option("help", "NULL", "Print this help", 0 );
    exit(0);
}

#if DEBUG_ARGS_ENABLE
static void set_debug( ib_context_t *ctx )
{
    if (settings.debuglevel >= 0 ) {
        ib_context_set_num( ctx,
                            IB_PROVIDER_TYPE_LOGGER ".log_level",
                            settings.debuglevel);
    }
    if (settings.debuguri != NULL ) {
        ib_context_set_string( ctx,
                               IB_PROVIDER_TYPE_LOGGER ".log_uri",
                               settings.debuguri);
    }
}
#endif

static void fatal_error(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    exit(1);
}

static ib_status_t ironbee_conn_init(ib_engine_t *ib,
                                     ib_conn_t *iconn,
                                     void *cbdata)
{
    iconn->local_port=settings.localport;
    iconn->local_ipstr=settings.localip;
    iconn->remote_port=settings.remoteport;
    iconn->remote_ipstr=settings.remoteip;

    return IB_OK;
}

/**
 * @internal
 * Handle request_header events.
 *
 * Extract the "request_headers" field (a list) from the transactions's
 * data provider instance, then loop through the list, looking for the
 * "User-Agent"  field.  If found, the value is parsed and used to update the
 * connection object fields.
 *
 * @param[in] ib IronBee object
 * @param[in,out] tx Transaction object
 * @param[in] data Callback data (not used)
 *
 * @returns Status code
 */
static ib_status_t print_user_agent(ib_engine_t *ib,
                                    ib_tx_t *tx,
                                    void *data)
{
    IB_FTRACE_INIT(modua_handle_req_headers);
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
                     "request_headers_event: "
                     "Field list missing / incorrect type" );
        IB_FTRACE_RET_STATUS(IB_EUNKNOWN);
    }

    fputs(settings.user_agent, stdout);

    /* Loop through the list; we're looking for User-Agent */
    IB_LIST_LOOP(lst, node) {
        ib_field_t *field = (ib_field_t *)ib_list_node_data(node);

        /* Check the field name
         * Note: field->name is not always a null ('\0') terminated string */
        fprintf(stdout,
                "  User Agent %.*s = '%s'\n",
                (int)field->nlen, field->name, ib_field_value_nulstr(field) );
    }

    /* Done */
    IB_FTRACE_RET_STATUS(IB_OK);
}

static void runConnection(ib_engine_t* ib )
{

    FILE *reqfp = NULL;
    int respfd = -1;
    ib_conn_t *iconn = NULL;
    ib_conndata_t icdata;
    char buf[MAX_BUF];
    char *bufp = NULL;
    char linebuf[MAX_LINE_BUF];
    const char *linep;
    size_t nbytes = 0;

    /* Register the request headers event late */
    if (settings.user_agent != NULL) {
        ib_hook_register(ib, request_headers_event,
                         (ib_void_fn_t)print_user_agent, NULL);
    }

    if (! strcmp("-", settings.requestfile)) {
        reqfp = stdin;
    }
    else {
        reqfp = fopen(settings.requestfile, "rb");
        if (reqfp == NULL) {
            fatal_error("Error opening request file '%s'",
                        settings.requestfile);
        }
    }

    if (settings.responsefile) {
        respfd = open(settings.responsefile, O_RDONLY);
        if (respfd == -1) {
            fatal_error("Error opening response file '%s'",
                        settings.responsefile);
        }
    }

    // Create a connection
    ib_conn_create(ib, &iconn, NULL);
    ib_state_notify_conn_opened(ib, iconn);
    icdata.ib = ib;
    icdata.mp = iconn->mp;
    icdata.conn = iconn;

    /* Read the request file, assemble a buffer, pass it to IB */
    while ((linep = fgets(linebuf, sizeof(linebuf), reqfp)) != NULL) {
        size_t linelen;
        if (bufp == NULL) {
            bufp = buf;
            *bufp = '\0';
            nbytes = 0;
        }
        if (strncmp(linebuf, "User-Agent:", 10) == 0) {
            if (settings.user_agent != NULL) {
                linep = settings.user_agent;
            }
        }
        linelen = strlen(linep);
        if ((nbytes + linelen) >= sizeof(buf)) {
            icdata.dalloc = nbytes;
            icdata.dlen = nbytes;
            icdata.data = (uint8_t *)buf;
            ib_state_notify_conn_data_in(ib, &icdata);
            bufp = NULL;
        }
        else {
            strncat(buf, linep, sizeof(buf)-nbytes);
            nbytes += linelen;
        }
    }

    /* Send the last chunk (if there is one) */
    if ( (nbytes != 0) && (bufp != NULL) ) {
        icdata.dalloc = nbytes;
        icdata.dlen = nbytes;
        icdata.data = (uint8_t *)buf;
        ib_state_notify_conn_data_in(ib, &icdata);
    }

    if (respfd != -1) {
        // read the response and pass it to ironbee
        while ((nbytes = read(respfd, buf, sizeof(buf))) > 0) {
	        icdata.dalloc = nbytes;
            icdata.dlen = nbytes;
            icdata.data = (uint8_t *)buf;
            ib_state_notify_conn_data_out(ib, &icdata);
        }
    }


    ib_state_notify_conn_closed(ib, iconn);
}

int
main(int argc, char* argv[])
{
    ib_status_t rc;
    ib_engine_t *ironbee = NULL;
    ib_cfgparser_t *cp;

    struct option longopts[] =
        {
            { "config", required_argument, 0, 0 },
            { "requestfile", required_argument, 0, 0 },
            { "responsefile", required_argument, 0, 0 },
	    { "local-ip", required_argument, 0, 0 },
	    { "local-port", required_argument, 0, 0 },
	    { "remote-ip", required_argument, 0, 0 },
	    { "remote-port", required_argument, 0, 0 },
	    { "user-agent", required_argument, 0, 0 },
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
            settings.configfile = optarg;
        }
        else if (! strcmp("requestfile", longopts[option_index].name)) {
            settings.requestfile = optarg;
        }
        else if (! strcmp("responsefile", longopts[option_index].name)) {
            settings.responsefile = optarg;
        }
        else if (! strcmp("user-agent", longopts[option_index].name)) {
            static char buf[MAX_LINE_BUF];
            strcpy(buf, "User-Agent: ");
            strncat(buf, optarg, sizeof(buf)-(1+strlen(buf)));
            strncat(buf, "\r\n", sizeof(buf)-(1+strlen(buf)));
            settings.user_agent = buf;
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
            settings.debuglevel = level;
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
            settings.debuguri = buf;
        }
#endif
        else if (! strcmp("local-ip", longopts[option_index].name)) {
            settings.localip = optarg;
        }
        else if (! strcmp("local-port", longopts[option_index].name)) {
            settings.localport = (int) strtol(optarg, NULL, 10);
            if ( ( settings.localport == 0 ) && ( errno == EINVAL ) ) {
                fprintf(stderr,
                        "--local-port: invalid port number '%s'", optarg );
                usage();
            }
        }
        else if (! strcmp("remote-ip", longopts[option_index].name)) {
            settings.remoteip = optarg;
        }
        else if (! strcmp("remote-port", longopts[option_index].name)) {
            settings.remoteport = (int) strtol(optarg, NULL, 10);
            if ( ( settings.remoteport == 0 ) && ( errno == EINVAL ) ) {
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

    if (settings.configfile == NULL) {
        fprintf(stderr, "--config <file> is required\n");
        usage();
    }
    if (settings.requestfile == NULL) {
        fprintf(stderr, "--requestfile <file> is required\n");
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
        ib_cfgparser_parse(cp, settings.configfile);
        ib_cfgparser_destroy(cp);
    }

    // Set all contexts' debug flags from the command line args
    // We do this because they may have been overwritten by DebugLog
    // directives.
#if DEBUG_ARGS_ENABLE
    if ( (settings.debuglevel >= 0) || (settings.debuguri != NULL) ) {
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
    runConnection(ironbee);

    ib_engine_destroy(ironbee);
}
