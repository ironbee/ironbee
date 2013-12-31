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
 * @brief Module to hook txlog into ATS
 *
 * @author Nick Kew <nkew@qualys.com>
 */

#include <ts/ts.h>

/* txlog pretends to be a module, and has an API which is private
 * but exposed here.
 */
#include "ironbee_config_auto.h"
#include "txlog.h"
#include <assert.h>
#include <ironbee/module.h>
#include <ironbee/context.h>

#define MODULE_NAME	ATS-TXLOG
#define MODULE_NAME_STR	IB_XSTRINGIFY(MODULE_NAME)

IB_MODULE_DECLARE();

typedef struct ats_txlog_cfg_t {
    const char *logfile;
    TSTextLogObject logger;
} ats_txlog_cfg_t;
 
#if 0

/* FIXME: is this something we should have?
 *
 * It's writing without ever having called open, so we can't use these.
 * Just open the logger in the init function instead
 */
static ib_status_t txlog_close(ib_logger_t *logger, void *data)
{
    ats_txlog_cfg_t *cfg = data;
    assert(cfg != NULL && cfg->logfile != NULL);

    if (cfg->logger != NULL) {
        TSTextLogObjectFlush(cfg->logger);
        TSTextLogObjectDestroy(cfg->logger);
        cfg->logger = NULL;
    }
    return IB_OK;
}
static ib_status_t txlog_open(ib_logger_t *logger, void *data)
{
    ats_txlog_cfg_t *cfg = data;
    int rv;

    assert(cfg != NULL && cfg->logfile != NULL);

    rv = TSTextLogObjectCreate(cfg->logfile,
                               TS_LOG_MODE_ADD_TIMESTAMP,
                               &cfg->logger);
    if (rv != TS_SUCCESS) {
        /* FIXME: this is a hack 'cos we haven't got an engine */
        fprintf(stderr, "Failed to create txlog at %s\n", cfg->logfile);
        cfg->logger = NULL;
        return IB_EUNKNOWN;
    }

    return IB_OK;
}
static ib_status_t txlog_reopen(ib_logger_t *logger, void *data)
{
    (void)txlog_close(logger, data);
    return txlog_open(logger, data);
}
#else
#define txlog_open NULL
#define txlog_reopen NULL
#define txlog_close NULL
#endif

/**
 * Write log record to disk. 
 */
static void txlog_writer(void *element, void *data) {
    ib_logger_standard_msg_t *msg = (ib_logger_standard_msg_t *)element;

    ats_txlog_cfg_t *cfg = data;
    assert(cfg != NULL && cfg->logger != NULL);

    /* Tx log line is here. Log this. */
    if (msg->msg != NULL) {
        /* In practice, this is always NULL for txlogs. */
        if (msg->prefix != NULL) {
            TSTextLogObjectWrite(cfg->logger, "%s %.*s", msg->prefix,
                                 (int)msg->msg_sz, (const char *)msg->msg);
        }
        else {
            TSTextLogObjectWrite(cfg->logger, "%.*s",
                                 (int)msg->msg_sz, (const char *)msg->msg);
        }
        /* FIXME: once debugged, take this out for speed */
        TSTextLogObjectFlush(cfg->logger);
    }

    /* Done. Free this. */
    ib_logger_standard_msg_free(msg);
}

/**
 * Function that gets called when log records need to be written to disk.
 */
static ib_status_t txlog_record(ib_logger_t *logger,
                                ib_logger_writer_t *writer,
                                void *data) {

    /* Pull all records out of the logger's record queue and apply the writer function to them. */
    return ib_logger_dequeue(logger, writer, txlog_writer, data);
}

static ib_status_t ats_txlog_init(ib_engine_t *ib, ib_module_t *m, void *x)
{
    const ib_txlog_module_cfg_t *txlog_cfg;
    ats_txlog_cfg_t *cfg;
    ib_status_t rc;
    int rv;

    assert(ib != NULL);
    rc = ib_context_module_config(ib_context_main(ib), m, &cfg);
    assert((rc == IB_OK) && (cfg != NULL));
 
    /* txlog relies on a "module".
     * This code relies on module being loaded before we initialise.
     */
    rc = ib_txlog_get_config(ib, ib_context_main(ib), &txlog_cfg);

    if (rc == IB_OK) {
        ib_logger_writer_add(
            ib_engine_logger_get(ib),
            txlog_open, cfg,                      /* Open. */
            txlog_close, cfg,                     /* Close. */
            txlog_reopen, cfg,                    /* Reopen. */
            txlog_cfg->logger_format_fn, cfg,     /* Format. */
            txlog_record, cfg                     /* Record. */
        );
    }
    else {
        ib_log_error(ib, "Can't initialise txlog logging (is txlog loaded?)");
    }

    rv = TSTextLogObjectCreate(cfg->logfile,
                               TS_LOG_MODE_ADD_TIMESTAMP,
                               &cfg->logger);
    if (rv != TS_SUCCESS) {
        ib_log_error(ib, "Failed to create txlog at %s", cfg->logfile);
        cfg->logger = NULL;
        return IB_EUNKNOWN;
    }
    return IB_OK;
}

static ib_status_t txlogfile(ib_cfgparser_t *cp, const char *name,
                             const char *p1, void *dummy)
{
    ats_txlog_cfg_t *cfg;
    ib_status_t rc;
    ib_module_t *m;

    rc = ib_engine_module_get(cp->ib, MODULE_NAME_STR, &m);
    assert((rc == IB_OK) && (m != NULL));
    rc = ib_context_module_config(ib_context_main(cp->ib), m, &cfg);
    assert((rc == IB_OK) && (cfg != NULL));

    cfg->logfile = p1;
    return rc;
}
static IB_DIRMAP_INIT_STRUCTURE(ats_txlog_config) = {
    IB_DIRMAP_INIT_PARAM1(
        "TXLogFile",
        txlogfile,
        NULL
    ),

    /* End */
    IB_DIRMAP_INIT_LAST
};
static ats_txlog_cfg_t ats_txlog_cfg_ini = {
    "IronbeeTxLog",
    NULL
};
IB_MODULE_INIT(
    IB_MODULE_HEADER_DEFAULTS,           /**< Default metadata */
    MODULE_NAME_STR,                     /**< Module name */
    IB_MODULE_CONFIG(&ats_txlog_cfg_ini),/**< Global config data */
    NULL,                                /**< Configuration field map */
    ats_txlog_config,                    /**< Config directive map */
    ats_txlog_init, NULL,                /**< Initialize function */
    NULL, NULL,                          /**< Finish function */
);
