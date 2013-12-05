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
 * @brief IronBee --- Pipe module.
 *
 * This is a proof-of-concept for implementing an ironbee logger as module.
 * Tested at its simplest by just piping to cat.
 *
 * It mostly seems to work well, with a couple of limitations:
 *   - Another logger will get all configuration messages arising
 *     before the PipedLog directive activates our logger
 *   - Other than at startup, we have no access to a pool we can
 *     use without leaking.  This is a problem of the log API in
 *     general.
 *   - Certain errors can't be handled by just logging an error
 *     (that way recursive madness lies).
 *
 * As regards operational use, this module has more serious issues.
 * If the piped program disappears, we restart it with a small
 * memory leak.  Robustness in adverse conditions (such as a
 * piped program that can't consume data at the rate we send)
 * is completely untested: it might in principle get into a
 * nasty loop of write-fail / restart piped program.
 * Not a problem so long as this remains a proof-of-concept.
 */


#include <ironbee/context.h>

#ifndef NO_THREADS
#include <ironbee/lock.h>
#endif

#include <ironbee/module.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/** Module name. */
#define MODULE_NAME        log_pipe
/** Stringified version of MODULE_NAME */
#define MODULE_NAME_STR    IB_XSTRINGIFY(MODULE_NAME)

IB_MODULE_DECLARE();

typedef struct log_pipe_cfg {
    const char *cmdline;
    ib_logger_level_t log_level;
    FILE *pipe;
} log_pipe_cfg;

/* If we're compiling solely for a non-threaded server (like nginx or
 * apache+prefork) we can save a tiny bit of overhead.
 */
#ifndef NO_THREADS
#define MUTEX_LOCK ib_lock_lock(&log_pipe_mutex)
#define MUTEX_UNLOCK ib_lock_unlock(&log_pipe_mutex)
static ib_lock_t log_pipe_mutex;
static void log_pipe_mutex_init(ib_engine_t *ib, log_pipe_cfg *cfg)
{
    ib_mpool_t *mp;
    mp = ib_engine_pool_main_get(ib);
    if (ib_lock_init(&log_pipe_mutex) == IB_OK)
        ib_mpool_cleanup_register(mp, (void *)ib_lock_destroy, &log_pipe_mutex);
}
#else
#define MUTEX_LOCK
#define MUTEX_UNLOCK
#define log_pipe_mutex_init(ib,cfg)
#endif


static ib_status_t log_pipe_open(ib_engine_t *ib, log_pipe_cfg *cfg);

/**
 * Handles write errors by stopping and restarting the piped logger
 *
 * @param[in] ib  Ironbee engine
 * @param[in] m  module struct
 * @param[in] timestr  timestamp
 * @param[in] cfg  the configuration record
 * @return  status from log_pipe_open
 */
static ib_status_t log_pipe_restart(const ib_engine_t *ib, ib_module_t *m,
                                    const char *timestr, log_pipe_cfg *cfg)
{
    ib_status_t rc;

    /* Try and log an emergency error to stderr */
    fputs("IRONBEE: Piped Log Error. Trying to restart!\n", stderr);

    pclose(cfg->pipe);
    cfg->pipe = NULL;
    if (rc = log_pipe_open((ib_engine_t *)ib, cfg), rc == IB_OK) {
        /* OK, we should be back up&logging ... */
        fprintf(cfg->pipe, "%s: %s\n", timestr,
                "LOG ERROR.  Piped log restarted!");
    }
    else {
        /* Nothing sensible we can do. */
        /* FIXME: should we consider this a fatal error?
         * A library can't just go and exit, nor can we throw()
         */
    }
    return rc;
}

typedef struct log_pipe_log_rec_t {
    ib_logger_level_t  level;
    char              *file;
    int                line;
    char               timestr[26];
    char               buf[8192+1];
    int                ec;
} log_pipe_log_rec_t;

ib_status_t log_pipe_format(
    ib_logger_t           *logger,
    const ib_logger_rec_t *rec,
    const uint8_t         *log_msg,
    const size_t           log_msg_sz,
    void                  *writer_record,
    void                  *data
)
{
    /* Just duplicate what's in all the server loggers */
    ib_module_t *m;
    log_pipe_cfg *cfg;
    time_t tm;
    ib_status_t rc;
    ib_engine_t *ib = (ib_engine_t *)data;
    log_pipe_log_rec_t *log_pipe_log_rec = malloc(sizeof(*log_pipe_log_rec));

    if (log_pipe_log_rec == NULL) {
        return IB_EALLOC;
    }

    rc = ib_engine_module_get((ib_engine_t *)ib, MODULE_NAME_STR, &m);
    assert((rc == IB_OK) && (m != NULL));
    rc = ib_context_module_config(ib_context_main(ib), m, &cfg);
    assert((rc == IB_OK) && (cfg != NULL) && (cfg->pipe != NULL));

    if (rec->level > cfg->log_level) {
        free(log_pipe_log_rec);
        return IB_DECLINED;
    }

    log_pipe_log_rec->level = rec->level;
    log_pipe_log_rec->line  = rec->line_number;
    log_pipe_log_rec->file  = strdup(rec->file);
    if (log_pipe_log_rec->file == NULL) {
        free(log_pipe_log_rec);
        return IB_EALLOC;
    }

    /* TODO: configurable time format */
    time(&tm);
    ctime_r(&tm, log_pipe_log_rec->timestr);
    log_pipe_log_rec->timestr[24] = 0;  /* we don't want the newline */

    /* Buffer the log line. */
    log_pipe_log_rec->ec = snprintf(
        log_pipe_log_rec->buf,
        8192+1,
        "%.*s",
        (int)log_msg_sz,
        log_msg);

    *(log_pipe_log_rec_t **)writer_record = log_pipe_log_rec;
    return IB_OK;
}

/**
 * Callback data for log_pipe_writer().
 */
typedef struct log_pipe_writer_data_t {
    log_pipe_cfg *cfg;    /**< Configuration. */
    ib_module_t  *module; /**< This module structure. */
    ib_engine_t  *ib;     /**< IronBee engine. */
} log_pipe_writer_data_t;

/**
 * Do the writing of a single record.
 *
 * @param[in] record The log record.
 * @param[in] cbdata A @ref log_pipe_writer_data_t pointer used for writing.
 */
static void log_pipe_writer(void *record, void *cbdata) {
    assert(record != NULL);
    assert(cbdata != NULL);

    const int               LIMIT = 7000;
    log_pipe_writer_data_t *writer_data = (log_pipe_writer_data_t *)cbdata;
    log_pipe_log_rec_t     *rec = (log_pipe_log_rec_t *)record;
    log_pipe_cfg           *cfg = writer_data->cfg;
    ib_module_t            *m   = writer_data->module;
    ib_engine_t            *ib  = writer_data->ib;

    MUTEX_LOCK;
    if (rec->ec >= LIMIT) {
        /* Mark as truncated, with a " ...". */
        memcpy((rec->buf) + (LIMIT - 5), " ...", 5);

        /// @todo Do something about it
        if (
            fprintf(
                cfg->pipe, "%s: Log format truncated: limit (%d/%d)\n",
                rec->timestr, (int)rec->ec, LIMIT) < 0
            )
        {
            if (log_pipe_restart(ib, m, rec->timestr, cfg) == IB_OK) {
                fprintf(
                    cfg->pipe,
                    "%s: Log format truncated: limit (%d/%d)\n",
                    rec->timestr,
                    (int)rec->ec,
                    LIMIT);
            }
        }
    }

    if (
        fprintf(
            cfg->pipe,
            "%s %s [%s:%d]: %s\n", rec->timestr,
            ib_logger_level_to_string(rec->level),
            rec->file,
            rec->line,
            rec->buf) < 0
    )
    {
        /* On error, see if we can save anything.
         * There's no sensible error handling at this point.
         */
        if (log_pipe_restart(ib, m, rec->timestr, cfg) == IB_OK) {
            fprintf(
                cfg->pipe,
                "%s %s [%s:%d]: %s\n",
                rec->timestr,
                ib_logger_level_to_string(rec->level),
                rec->file,
                rec->line,
                rec->buf);
        }
    }
    MUTEX_UNLOCK;
    free(rec->file);
    free(rec);
}

ib_status_t log_pipe_record(
    ib_logger_t        *logger,
    ib_logger_writer_t *writer,
    void               *data
)
{
    assert(logger != NULL);
    assert(writer != NULL);
    assert(data != NULL);

    ib_status_t             rc;
    log_pipe_writer_data_t  writer_data;
    log_pipe_cfg           *cfg;
    ib_module_t            *m;
    ib_engine_t            *ib = (ib_engine_t *)data;

    rc = ib_engine_module_get((ib_engine_t *)ib, MODULE_NAME_STR, &m);
    assert((rc == IB_OK) && (m != NULL));

    rc = ib_context_module_config(ib_context_main(ib), m, &cfg);
    assert((rc == IB_OK) && (cfg != NULL) && (cfg->pipe != NULL));

    writer_data.ib     = ib;
    writer_data.cfg    = cfg;
    writer_data.module = m;

    rc = ib_logger_dequeue(logger, writer, log_pipe_writer, &writer_data);

    return rc;
}

/**
 * Ironbee callback to get current log level
 *
 * @param[in] ib  Ironbee engine
 * @return  The current log level
 */
static ib_logger_level_t log_pipe_get_level(const ib_engine_t *ib)
{
    log_pipe_cfg *cfg;
    ib_module_t *m;
    ib_context_t *ctx;
    ib_status_t rc;

    rc = ib_engine_module_get((ib_engine_t *)ib, MODULE_NAME_STR, &m);
    assert((rc == IB_OK) && (m != NULL));

    /* This may get called after ctx has been invalidated, because
     * cleanup happens in a perverse order.
     */
    ctx = ib_context_main(ib);
    if (ctx == NULL) {
        return 4;
    }
    rc = ib_context_module_config(ib_context_main(ib), m, &cfg);
    assert((rc == IB_OK) && (cfg != NULL));

    return cfg->log_level;
}

/**
 * Function to close a pipe (registered as pool cleanup)
 *
 * @param[in] data  config struct
 */
static void log_pipe_close(void *data)
{
    log_pipe_cfg *cfg = data;
    assert(cfg != NULL);
    if (cfg->pipe != NULL) {
        if (pclose(cfg->pipe) == -1) {
            /* Just hope some logger is functioning! */
            //ib_log_error(ib, "Failed to retrieve piped log config!");
        }
        cfg->pipe = NULL;
    }
}
/**
 * Function to open a pipe named in the config
 *
 * @param[in] ib  Ironbee engine
 * @param[in] cfg config struct
 * @return  success or failure
 */
static ib_status_t log_pipe_open(ib_engine_t *ib, log_pipe_cfg *cfg)
{
    ib_mpool_t *mp;

    assert(cfg != NULL);

    if (cfg->cmdline == NULL) {
        ib_log_notice(ib, "Piped log not configured");
        return IB_OK;
    }
    mp = ib_engine_pool_main_get(ib);

    cfg->pipe = popen(cfg->cmdline, "w");
    if (cfg->pipe == NULL) {
        /* This will get to the default logger - hopefully! */
        ib_log_critical(ib, "Failed to open pipe to %s!", cfg->cmdline);
        return IB_EOTHER;
    }
    ib_mpool_cleanup_register(mp, log_pipe_close, cfg);

    ib_logger_t *logger = ib_engine_logger_get(ib);

    /* Now our pipe is up-and-running, register our own logger */
    ib_logger_writer_clear(logger);
    ib_logger_writer_add(
        logger,
        NULL, /* Open */
        NULL,
        NULL, /* Close */
        NULL,
        NULL, /* Reopen */
        NULL,
        log_pipe_format,
        ib,
        log_pipe_record,
        ib
    );
    ib_logger_level_set(logger, log_pipe_get_level(ib));

    return IB_OK;
}

/**
 * Configuration function to read pipe's command line and open the pipe
 *
 * @param[in] cp  Config parser
 * @param[in] name  unused
 * @param[in] p1  The configured command to execute
 * @param[in] dummy unused
 */
static ib_status_t log_pipe_program(ib_cfgparser_t *cp, const char *name,
                                    const char *p1, void *dummy)
{
    log_pipe_cfg *cfg;
    ib_module_t *m;
    ib_status_t rc;

    assert(cp     != NULL);
    assert(cp->ib != NULL);
    assert(name   != NULL);
    assert(p1     != NULL);

    rc = ib_engine_module_get(cp->ib, MODULE_NAME_STR, &m);
    assert((rc == IB_OK) && (m != NULL));
    rc = ib_context_module_config(ib_context_main(cp->ib), m, &cfg);
    assert((rc == IB_OK) && (cfg != NULL));

    cfg->cmdline = p1;

    log_pipe_mutex_init(cp->ib, cfg);
    return log_pipe_open(cp->ib, cfg);
}
/**
 * Configuration function to set log level
 * @param[in] cp  Config parser
 * @param[in] name  unused
 * @param[in] p1  Log level to set
 * @param[in] dummy unused
 */
static ib_status_t log_pipe_set_level(ib_cfgparser_t *cp, const char *name,
                                      const char *p1, void *dummy)
{
    log_pipe_cfg *cfg;
    ib_module_t *m;
    ib_status_t rc;

    assert(cp     != NULL);
    assert(cp->ib != NULL);
    assert(name   != NULL);
    assert(p1     != NULL);

    rc = ib_engine_module_get(cp->ib, MODULE_NAME_STR, &m);
    assert((rc == IB_OK) && (m != NULL));
    rc = ib_context_module_config(ib_context_main(cp->ib), m, &cfg);
    assert((rc == IB_OK) && (cfg != NULL));

    cfg->log_level = ib_logger_string_to_level(p1, IB_LOG_WARNING);

    return IB_OK;
}

static IB_DIRMAP_INIT_STRUCTURE(log_pipe_config) = {
    IB_DIRMAP_INIT_PARAM1(
        "PipedLog",
        log_pipe_program,
        NULL
    ),
    IB_DIRMAP_INIT_PARAM1(
        "PipedLogLevel",
        log_pipe_set_level,
        NULL
    ),

    /* End */
    IB_DIRMAP_INIT_LAST
};

static log_pipe_cfg log_pipe_cfg_ini = {
    NULL,
    4,
    NULL
};

IB_MODULE_INIT(
    IB_MODULE_HEADER_DEFAULTS,           /**< Default metadata */
    MODULE_NAME_STR,                     /**< Module name */
    IB_MODULE_CONFIG(&log_pipe_cfg_ini), /**< Global config data */
    NULL,                                /**< Configuration field map */
    log_pipe_config,                     /**< Config directive map */
    NULL, NULL,                          /**< Initialize function */
    NULL, NULL,                          /**< Finish function */
);
