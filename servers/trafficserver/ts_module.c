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
 * @brief IronBee --- Apache Traffic Server Plugin
 *
 * @author Nick Kew <nkew@qualys.com>
 */

#include "ironbee_config_auto.h"

#include <assert.h>
#include <ts/ts.h>

#if defined(__cplusplus) && !defined(__STDC_FORMAT_MACROS)
/* C99 requires that inttypes.h only exposes PRI* macros
 * for C++ implementations if this is defined: */
#define __STDC_FORMAT_MACROS
#endif

#include <ironbee/engine_manager_control_channel.h>
#include <ironbee/core.h>

/* TxLog private "API" */
#include "../../modules/txlog.h"


#include "ts_ib.h"

static const size_t CONTROL_CHANNEL_POLL_INTERVAL = 2000;

/**
 * Plugin global data
 */
typedef struct module_data_t module_data_t;
struct module_data_t {
    TSTextLogObject  logger;         /**< TrafficServer log object */
    ib_manager_t    *manager;        /**< IronBee engine manager object */

    //! The manager control channel for manager.
    ib_engine_manager_control_channel_t *manager_ctl;
    size_t           max_engines;    /**< Max # of simultaneous engines */
    const char      *config_file;    /**< IronBee configuration file */
    const char      *log_file;       /**< IronBee log file */
    int              log_level;      /**< IronBee log level */
    bool             log_disable;    /**< Disable logging? */

    const char      *txlogfile;
    TSTextLogObject  txlogger;

    bool allow_at_startup;  /**< Allow requests unchecked before ib fully loaded */
};

/* Global module data */
static module_data_t module_data =
{
    NULL,                            /* .logger */
    NULL,                            /* .manager */
    NULL,                            /* .manager_ctl. */
    IB_MANAGER_DEFAULT_MAX_ENGINES,  /* .max_engines */
    NULL,                            /* .config_file */
    NULL,                            /* .log_file */
    IB_LOG_WARNING,                  /* .log_level */
    false,                           /* .log_disable */
    DEFAULT_TXLOG,
    NULL,
    false
};

/* API for ts_event.c */
ib_status_t tsib_manager_engine_acquire(ib_engine_t **ib)
{
    return module_data.manager == NULL
           ? IB_EALLOC
           : ib_manager_engine_acquire(module_data.manager, ib);
}
ib_status_t tsib_manager_engine_cleanup(void)
{
    return module_data.manager == NULL
           ? IB_OK
           : ib_manager_engine_cleanup(module_data.manager);
}
ib_status_t tsib_manager_engine_create(void)
{
    return module_data.manager == NULL
           ? IB_EALLOC
           : ib_manager_engine_create(module_data.manager, module_data.config_file);
}
ib_status_t tsib_manager_engine_release(ib_engine_t *ib)
{
    return module_data.manager == NULL
           ? IB_OK
           : ib_manager_engine_release(module_data.manager, ib);
}
/**
 * Engine Manager Control Channel continuation.
 *
 * This polls and takes action on commands to IronBee.
 *
 * @param[in] contp Pointer to the continuation.
 * @param[in] event Event from ATS. Unused.
 * @param[in] edata Event data. Unused.
 *
 * @returns
 * - 0 On success.
 * - -1 On error.
 */
static int manager_ctl(TSCont contp, TSEvent event, void *edata)
{
    module_data_t *mod_data = (module_data_t *)(TSContDataGet(contp));

    if (ib_engine_manager_control_ready(mod_data->manager_ctl)) {
        ib_status_t rc;

        rc = ib_engine_manager_control_recv(mod_data->manager_ctl, false);
        if (rc != IB_EAGAIN && rc != IB_OK) {
            TSError("[ironbee] Error processing message: %s",
                    ib_status_to_string(rc));
            return -1;
        }
    }

    return 0;
}


/**
 * Log a message to the server plugin.
 *
 * @param[in] ib_logger The IronBee logger.
 * @param[in] rec The record to use in logging.
 * @param[in] log_msg The user's log message.
 * @param[in] log_msg_sz The user's log message size.
 * @param[out] writer_record Unused. We always return IB_DECLINED.
 * @param[in] cbdata The server plugin module data used for logging.
 *
 * @returns
 * - IB_DECLINED when everything goes well.
 * - IB_OK is not returned.
 * - Other on error.
 */
static ib_status_t logger_format(
    ib_logger_t           *ib_logger,
    const ib_logger_rec_t *rec,
    const uint8_t         *log_msg,
    const size_t           log_msg_sz,
    void                  *writer_record,
    void                  *cbdata
)
{
    assert(ib_logger != NULL);
    assert(rec != NULL);
    assert(log_msg != NULL);
    assert(cbdata != NULL);

    if (cbdata == NULL) {
        return IB_DECLINED;
    }

    module_data_t   *mod_data = (module_data_t *)cbdata;
    TSTextLogObject  logger = mod_data->logger;

    if (logger == NULL) {
        return IB_DECLINED;
    }
    if (log_msg == NULL || log_msg_sz == 0) {
        TSTextLogObjectFlush(logger);
    }
    else {

        ib_logger_standard_msg_t *std_msg = NULL;

        ib_status_t rc = ib_logger_standard_formatter(
            ib_logger,
            rec,
            log_msg,
            log_msg_sz,
            &std_msg,
            NULL);
        if (rc != IB_OK) {
            return rc;
        }

        TSTextLogObjectWrite(
            logger,
            "%s %.*s",
            std_msg->prefix,
            (int)std_msg->msg_sz,
            (const char *)std_msg->msg);

        ib_logger_standard_msg_free(ib_logger, std_msg, cbdata);
    }

    return IB_DECLINED;
}

/**
 * Perform a flush when closing the log.
 *
 * Performs flush for IronBee ATS plugin logging.
 *
 * @param[in] logger IronBee logger. Unused.
 * @param[in] cbdata Callback data.
 */
static ib_status_t logger_close(
    ib_logger_t       *ib_logger,
    void              *cbdata)
{
    if (cbdata == NULL) {
        return IB_OK;
    }
    module_data_t   *mod_data = (module_data_t *)cbdata;
    TSTextLogObject  logger = mod_data->logger;

    if (logger != NULL) {
        TSTextLogObjectFlush(logger);
    }

    return IB_OK;
}

/**
 * Handle a single log record. This is a @ref ib_logger_standard_msg_t.
 *
 * @param[in] element A @ref ib_logger_standard_msg_t holding
 *            a serialized transaction log to be written to the
 *            Traffic Server transaction log.
 * @param[in] cbdata A @ref module_data_t.
 */
static void txlog_record_element(
    void *element,
    void *cbdata
)
{
    assert(element != NULL);
    assert(cbdata != NULL);

    ib_logger_standard_msg_t *msg      = (ib_logger_standard_msg_t *)element;
    module_data_t            *mod_data = (module_data_t *)cbdata;

    /* FIXME - expand msg->msg with Traffic Server variables. */
    /* I don't understand what is TBD here! */

    if (!mod_data->txlogger) {
        /* txlogging off  */
        return;
    }

    /* write log file. */
    if (msg->msg != NULL) {
        /* In practice, this is always NULL for txlogs. */
        if (msg->prefix != NULL) {
            TSTextLogObjectWrite(mod_data->txlogger, "%s %.*s", msg->prefix,
                                 (int)msg->msg_sz, (const char *)msg->msg);
        }
        else {
            TSTextLogObjectWrite(mod_data->txlogger, "%.*s",
                                 (int)msg->msg_sz, (const char *)msg->msg);
        }
        /* FIXME: once debugged, take this out for speed */
        TSTextLogObjectFlush(mod_data->txlogger);
    }
}

/**
 * Transaction Log record handler.
 *
 * @param[in] logger The logger.
 * @param[in] writer The log writer.
 * @param[in] cbdata Callback data. @ref module_data_t.
 *
 * @returns
 * - IB_OK On success.
 * - Other on failure.
 */
static ib_status_t txlog_record(
    ib_logger_t        *logger,
    ib_logger_writer_t *writer,
    void               *cbdata
)
{
    module_data_t *mod_data = (module_data_t *)cbdata;
    assert(logger != NULL);
    assert(writer != NULL);
    assert(cbdata != NULL);

    ib_status_t    rc;

    if (!mod_data->txlogger) {
        /* txlogging off  */
        return IB_OK;
    }

    rc = ib_logger_dequeue(logger, writer, txlog_record_element, cbdata);
    if (rc != IB_OK) {
        return rc;
    }

    return IB_OK;
}

/**
 * Register loggers to the IronBee engine.
 *
 * @param[out] manager The manager.
 * @param[in] ib The unconfigured engine.
 * @param[in] cbdata @ref module_data_t.
 *
 * @returns
 * - IB_OK On success.
 * - Other on fatal errors.
 */
static ib_status_t engine_preconfig_fn(
    ib_manager_t *manager,
    ib_engine_t  *ib,
    void         *cbdata
)
{
    assert(manager != NULL);
    assert(ib != NULL);
    assert(cbdata != NULL);

    ib_status_t         rc;
    ib_logger_format_t *iblog_format;
    module_data_t      *mod_data = (module_data_t *)cbdata;

    /* Clear all existing loggers. */
    rc = ib_logger_writer_clear(ib_engine_logger_get(ib));
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_logger_format_create(
        ib_engine_logger_get(ib),
        &iblog_format,
        logger_format,
        mod_data,
        NULL,
        NULL);
    if (rc != IB_OK) {
        return rc;
    }

    /* Register the IronBee logger. */
    ib_logger_writer_add(
        ib_engine_logger_get(ib),
        NULL,                      /* Open. */
        NULL,                      /* Callback data. */
        logger_close,              /* Close. */
        mod_data,                  /* Callback data. */
        NULL,                      /* Reopen. */
        NULL,                      /* Callback data. */
        iblog_format,              /* Format - This does all the work. */
        NULL,                      /* Record. */
        NULL                       /* Callback data. */
    );

    return IB_OK;
}

/**
 * Register loggers to the IronBee engine.
 *
 * @param[out] manager The manager.
 * @param[in] ib The configured engine.
 * @param[in] cbdata @ref module_data_t.
 *
 * @returns
 * - IB_OK On success.
 * - Other on fatal errors.
 */
static ib_status_t engine_postconfig_fn(
    ib_manager_t *manager,
    ib_engine_t  *ib,
    void         *cbdata
)
{
    assert(manager != NULL);
    assert(ib != NULL);
    assert(cbdata != NULL);

    int rv;
    ib_status_t         rc;
    module_data_t      *mod_data = (module_data_t *)cbdata;
    ib_logger_format_t *txlog_format;

    rc = ib_logger_fetch_format(
        ib_engine_logger_get(ib),
        TXLOG_FORMAT_FN_NAME,
        &txlog_format);
    if (rc == IB_OK) {
        /* Register the IronBee Transaction Log logger. */
        ib_logger_writer_add(
            ib_engine_logger_get(ib),
            NULL,                      /* Open. */
            NULL,                      /* Callback data. */
            NULL,                      /* Close. */
            NULL,                      /* Callback data. */
            NULL,                      /* Reopen. */
            NULL,                      /* Callback data. */
            txlog_format,              /* Format - This does all the work. */
            txlog_record,              /* Record. */
            mod_data                   /* Callback data. */
        );
        /* Open logfile for txlog */
        rv = TSTextLogObjectCreate(mod_data->txlogfile,
                                   0,
                                   &mod_data->txlogger);
        if (rv != TS_SUCCESS) {
            mod_data->txlogger = NULL;
            TSError("[ironbee] Failed to create transaction log \"%s\": %d",
                    mod_data->txlogfile,
                    rv);
        }
        else {
            /* 60 seconds */
            TSTextLogObjectRollingIntervalSecSet(mod_data->txlogger, 60);
            /* 5 MB - This API seems not to exist yet (TS-3059). */
            //TSTextLogObjectRollingSizeMbSet(mod_data->txlogger, 5);
            /* 3:00 am */
            TSTextLogObjectRollingOffsetHrSet(mod_data->txlogger, 3);
            /* 3 = time or size */
            TSTextLogObjectRollingEnabledSet(mod_data->txlogger, 3);
        }
    }
    else {
        ib_log_notice(ib, "No transaction logger available.");
    }

    return IB_OK;
}

/**
 * Handle ATS shutdown for IronBee plugin.
 *
 * Registered via atexit() during initialization, destroys the IB engine,
 * etc.
 *
 */
static void ibexit(void)
{
    module_data_t *mod_data = &module_data;

    TSDebug("ironbee", "ibexit()");
    if (mod_data->manager != NULL) {
        ib_manager_destroy(mod_data->manager);
    }
    if (mod_data->logger != NULL) {
        TSTextLogObjectFlush(mod_data->logger);
        TSTextLogObjectDestroy(mod_data->logger);
        mod_data->logger = NULL;
    }
    if (mod_data->txlogger != NULL) {
        TSTextLogObjectFlush(mod_data->txlogger);
        TSTextLogObjectDestroy(mod_data->txlogger);
    }
    if (mod_data->log_file != NULL) {
        free((void *)mod_data->log_file);
        mod_data->log_file = NULL;
    }
    ib_shutdown();
    TSDebug("ironbee", "ibexit() done");
}

/**
 * Function and struct to read a TS-style argc/argv commandline into
 * a config struct.  This struct is only used for ironbee_init, and
 * serves to enable new/revised options without disrupting the API or
 * load syntax.
 *
 * @param[in,out] mod_data Module data
 * @param[in] argc Command-line argument count
 * @param[in] argv Command-line argument list
 * @return  Success/Failure parsing the config line
 */
static ib_status_t read_ibconf(
    module_data_t *mod_data,
    int            argc,
    const char    *argv[]
)
{
    int c;

    /* defaults */
    mod_data->log_level = 4;

    /* const-ness mismatch looks like an oversight, so casting should be fine */
    while (c = getopt(argc, (char**)argv, "l:Lv:d:m:x:"), c != -1) {
        switch(c) {
        case 'L':
            mod_data->log_disable = true;
            break;
        case 'l':
            mod_data->log_file = strdup(optarg);
            break;
        case 'v':
            mod_data->log_level =
                ib_logger_string_to_level(optarg, IB_LOG_WARNING);
            break;
        case 'm':
            mod_data->max_engines = atoi(optarg);
            break;
        case 'x':
            mod_data->txlogfile = strdup(optarg);
            break;
        case '0':
            mod_data->allow_at_startup = true;
            break;
        default:
            TSError("[ironbee] Unrecognised option -%c ignored.", optopt);
            break;
        }
    }

    /* Default log file */
    if (mod_data->log_file == NULL) {
        mod_data->log_file = strdup(DEFAULT_LOG);
        if (mod_data->log_file == NULL) {
            return IB_EALLOC;
        }
    }

    /* keep the config file as a non-opt argument for back-compatibility */
    if (optind == argc-1) {
        mod_data->config_file = strdup(argv[optind]);
        if (mod_data->config_file == NULL) {
            return IB_EALLOC;
        }

        TSDebug("ironbee", "Configuration file: \"%s\"", mod_data->config_file);
        return IB_OK;
    }
    else {
        TSError("[ironbee] Exactly one configuration file name required.");
        return IB_EINVAL;
    }
}
/**
 * Initialize IronBee for ATS.
 *
 * Performs IB initializations for the ATS plugin.
 *
 * @param[in] mod_data Global module data
 *
 * @returns status
 */
static int ironbee_init(module_data_t *mod_data)
{
    /* grab from httpd module's post-config */
    ib_status_t rc;
    int rv;

    /* Create the channel. This is destroyed when the manager is destroyed. */
    rc = ib_engine_manager_control_channel_create(
        &(mod_data->manager_ctl),
        ib_manager_mm(mod_data->manager),
        mod_data->manager);
    if (rc != IB_OK) {
        TSError("[ironbee] Error creating IronBee control channel: %s",
            ib_status_to_string(rc));
        return rc;
    }

    /* Register the control commands (enable, disable, etc).
     * Failure is not fatal. */
    rc = ib_engine_manager_control_manager_ctrl_register(mod_data->manager_ctl);
    if (rc != IB_OK) {
        TSError("[ironbee] Failed to register ctrl commands to ctrl channel.");
    }

    /* Register the diagnostic commands (version and valgrind).
     * Failure is not fatal.
     * The valgrind command does nothing when not compiled w/ valgrind. */
    rc = ib_engine_manager_control_manager_diag_register(mod_data->manager_ctl);
    if (rc != IB_OK) {
        TSError("[ironbee] Failed to register diag commands to ctrl channel.");
    }

    /* Start the channel. This is stopped when it is destroyed. */
    rc = ib_engine_manager_control_channel_start(mod_data->manager_ctl);
    if (rc != IB_OK) {
        TSError("[ironbee] Error starting IronBee control channel: %s",
            ib_status_to_string(rc));
        /* Note: this is not a fatal error. */
    }
    /* If we started the channel, schedule it for periodic execution. */
    else {
        TSCont cont = TSContCreate(manager_ctl, TSMutexCreate());
        TSContDataSet(cont, mod_data);
        TSContScheduleEvery(
           cont,                          /* Manager control continuation. */
           CONTROL_CHANNEL_POLL_INTERVAL, /* Millisecons. */
           TS_THREAD_POOL_TASK            /* Task thread pool. */
        );
    }

    rc = ib_manager_engine_preconfig_fn_add(
        mod_data->manager,
        engine_preconfig_fn,
        mod_data);
    if (rc != IB_OK) {
        TSError("[ironbee] Error registering server preconfig function: %s",
                ib_status_to_string(rc));
        return rc;
    }

    rc = ib_manager_engine_postconfig_fn_add(
        mod_data->manager,
        engine_postconfig_fn,
        mod_data);
    if (rc != IB_OK) {
        TSError("[ironbee] Error registering server postconfig function: %s",
                ib_status_to_string(rc));
        return rc;
    }

    /* Create the initial engine */
    TSDebug("ironbee", "Creating initial IronBee engine");
    rc = ib_manager_engine_create(mod_data->manager, mod_data->config_file);
    if (rc != IB_OK) {
        TSError("[ironbee] Error creating initial IronBee engine: %s",
                ib_status_to_string(rc));
        return rc;
    }

    /* Register our at exit function */
    rv = atexit(ibexit);
    if (rv != 0) {
        TSError("[ironbee] Error registering IronBee exit handler: %s", strerror(rv));
        return IB_EOTHER;
    }

    TSDebug("ironbee", "IronBee Ready");
    return rc;
}

static int check_ts_version(void)
{

    const char *ts_version = TSTrafficServerVersionGet();
    int result = 0;

    if (ts_version) {
        int major_ts_version = 0;
        int minor_ts_version = 0;
        int patch_ts_version = 0;

        if (sscanf(ts_version, "%d.%d.%d", &major_ts_version, &minor_ts_version, &patch_ts_version) != 3) {
            return 0;
        }

        /* Need at least TS 3.0 */
        if (major_ts_version >= 3) {
            result = 1;
        }

    }

    return result;
}

/**
 * Initialize the IronBee ATS plugin.
 *
 * Performs initializations required by ATS.
 *
 * @param[in] argc Command-line argument count
 * @param[in] argv Command-line argument list
 */
static void *ibinit(void *x)
{
    TSCont cont = x;
    ib_status_t rc;

    rc = ironbee_init(&module_data);
    if (rc != IB_OK) {
        TSError("[ironbee] initialization failed: %s",
                ib_status_to_string(rc));
        goto Lerror;
    }

    /* connection initialization & cleanup */
    TSHttpHookAdd(TS_HTTP_SSN_START_HOOK, cont);

    /* now all's up and running, flag it to our READ_REQUEST_HDR hook */
    TSContDataSet(cont, &module_data);

    /* Register our continuation for management update for traffic_line -x
     * Note that this requires Trafficserver 3.3.5 or later, or else
     * apply the patch from bug TS-2036
     */
    TSMgmtUpdateRegister(cont, "ironbee");

    return NULL;

Lerror:
    TSError("[ironbee] Unable to initialize plugin (disabled).");

    return NULL;
}
/** Create and return top-level cont with no transient data
 *  Sets up engine manager and kill-or-continue txn hook before launching
 *  potentially-slow mainconfiguration in separate thread.
 */
static ib_status_t tsib_pre_init(TSCont *contp)
{
    int rv;
    ib_status_t rc;
    TSCont cont;

    assert(contp != NULL);

    /* create a cont to fend off traffic while we read config */
    *contp = cont = TSContCreate(ironbee_plugin, TSMutexCreate());
    if (cont == NULL) {
        TSError("[ironbee] failed to create initial continuation: disabled");
        return IB_EUNKNOWN;
    }
    if (module_data.allow_at_startup) {
        /* SSN_START doesn't use contdata; READ_REQUEST_HDR only needs non-null flag.
         * Using &module_data might let us clean up some tsib_api stuff in future.
         */
        TSContDataSet(cont, &module_data);
    }
    else {
        /* NULL contdata signals the READ_REQUEST_HDR hook to reject requests */
        TSContDataSet(cont, NULL);
    }
    TSHttpHookAdd(TS_HTTP_READ_REQUEST_HDR_HOOK, cont);

    if (!module_data.log_disable) {
        /* success is documented as TS_LOG_ERROR_NO_ERROR but that's undefined.
         * It's actually a TS_SUCCESS (proxy/InkAPI.cc line 6641).
         */
        printf("Logging to \"%s\"\n", module_data.log_file);
        rv = TSTextLogObjectCreate(module_data.log_file,
                                   TS_LOG_MODE_ADD_TIMESTAMP,
                                   &module_data.logger);
        if (rv != TS_SUCCESS) {
            TSError("[ironbee] Error creating log file.");
            return IB_EUNKNOWN;
        }
    }

    /* Initialize IronBee (including util) */
    rc = ib_initialize();
    if (rc != IB_OK) {
        TSError("[ironbee] Error initializing IronBee: %s",
                ib_status_to_string(rc));
        return rc;
    }

    /* Create the IronBee engine manager */
    TSDebug("ironbee", "Creating IronBee engine manager");
    rc = ib_manager_create(&(module_data.manager),   /* Engine Manager */
                           &ibplugin,                /* Server object */
                           module_data.max_engines); /* Default max */
    if (rc != IB_OK) {
        TSError("[ironbee] Error creating IronBee engine manager: %s",
                ib_status_to_string(rc));
    }
    return rc;
}
void TSPluginInit(int argc, const char *argv[])
{
    TSPluginRegistrationInfo info;
    TSThread init_thread;
    TSCont cont;

    info.plugin_name = (char *)"ironbee";
    info.vendor_name = (char *)"Qualys, Inc";
    info.support_email = (char *)"ironbee-users@lists.sourceforge.com";

    if (TSPluginRegister(TS_SDK_VERSION_3_0, &info) != TS_SUCCESS) {
        TSError("[ironbee] Plugin registration failed.  IronBee disabled");
        return;
    }

    if (!check_ts_version()) {
        TSError("[ironbee] Plugin requires Traffic Server 3.0 or later.  IronBee disabled");
        return;
    }

    if (read_ibconf(&module_data, argc, argv) != IB_OK) {
        TSError("[ironbee] Bad Ironbee options.  IronBee disabled");
        return;
    }

    if (tsib_pre_init(&cont) != IB_OK) {
        TSError("[ironbee] Pre-config failed.  IronBee disabled");
        return;
    }

    /* Launch potentially-slow config in its own thread */
    init_thread = TSThreadCreate(ibinit, cont);
    assert(init_thread != NULL);
}
