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

#include <ironbee/engine.h>
#include <ironbee/plugin.h>
#include <ironbee/provider.h>
#include <ironbee/config.h>


struct runtime_settings {
    char *configfile;
    char *requestfile;
    char *responsefile;
};


/* Plugin Structure */
ib_plugin_t ibplugin = {
    IB_PLUGIN_HEADER_DEFAULTS,
    "ibcli"
};

static void usage()
{
    fprintf(stderr, "Usage: ibcli <options>\n");
    exit(1);
}

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
    // @todo These should be configurable
    iconn->local_port=8080;
    iconn->local_ipstr="127.0.0.1";
    iconn->remote_port=23424;
    iconn->remote_ipstr="10.10.10.10";

    return IB_OK;
}

static void runConnection(ib_engine_t* ib,
                          const char * requestfile,
                          const char * responsefile)
{

    int reqfd = -1;
    int respfd = -1;
    ib_conn_t *iconn = NULL;
    ib_conndata_t icdata;
    char buf[8192];
    ssize_t nbytes;
    
    if (! strcmp("-", requestfile)) {
        reqfd = STDIN_FILENO;
    }
    else {
        reqfd = open(requestfile, O_RDONLY);
        if (reqfd == -1) {
            fatal_error("Error opening request file '%s'", requestfile);
        }
    }

    if (responsefile) {
        respfd = open(responsefile, O_RDONLY);
        if (respfd == -1) {
            fatal_error("Error opening response file '%s'", responsefile);
        }
    }

    // Create a connection
    ib_conn_create(ib, &iconn, NULL);
    ib_state_notify_conn_opened(ib, iconn);
    icdata.ib = ib;
    icdata.mp = iconn->mp;
    icdata.conn = iconn;

    // read the request and pass it to ironbee
    while ((nbytes = read(reqfd, buf, 8192)) > 0) {
        icdata.dalloc = nbytes;
        icdata.dlen = nbytes;
        icdata.data = (uint8_t *)buf;
        ib_state_notify_conn_data_in(ib, &icdata);
    }

    if (respfd != -1) {
        // read the response and pass it to ironbee
        while ((nbytes = read(respfd, buf, 8192)) > 0) {
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
    struct runtime_settings settings = {NULL,NULL,NULL};
    struct option longopts[] = 
        {
            { "config", required_argument, 0, 0 },
            { "requestfile", required_argument, 0, 0 },
            { "responsefile", required_argument, 0, 0 },
            { 0, 0, 0, 0}
        };

    while (1) {
        int option_index = 0;
        int c = getopt_long(argc, argv, "", longopts, &option_index);
        if (c != 0) {
            break;
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

    /* Notify the engine that the config process has started. */
    ib_state_notify_cfg_started(ironbee);

    /* Parse the config file. */
    rc = ib_cfgparser_create(&cp, ironbee);
    if ((rc == IB_OK) && (cp != NULL)) {
        ib_cfgparser_parse(cp, settings.configfile);
        ib_cfgparser_destroy(cp);
    }

    /* Notify the engine that the config process is finished. */
    ib_state_notify_cfg_finished(ironbee);
    
    /* Pass connection data to the engine. */
    runConnection(ironbee, settings.requestfile, settings.responsefile);

    ib_engine_destroy(ironbee);
}
