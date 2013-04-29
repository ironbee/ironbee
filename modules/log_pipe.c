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


#include <ironbee/module.h>

#include <assert.h>
#include <stdio.h>
#include <time.h>

/** Module name. */
#define MODULE_NAME        log_pipe
/** Stringified version of MODULE_NAME */
#define MODULE_NAME_STR    IB_XSTRINGIFY(MODULE_NAME)

IB_MODULE_DECLARE();

typedef struct log_pipe_cfg {
    const char *cmdline;
    ib_log_level_t log_level;
    FILE *pipe;
} log_pipe_cfg;

/* If we're compiling solely for a non-threaded server (like nginx or
 * apache+prefork) we can save a tiny bit of overhead.
 */
#ifndef NO_THREADS
#include <ironbee/lock.h>
#define MUTEX_LOCK ib_lock_lock(&log_pipe_mutex)
#define MUTEX_UNLOCK ib_lock_unlock(&log_pipe_mutex)
static ib_lock_t log_pipe_mutex;
static void log_pipe_mutex_init(ib_engine_t *ib, log_pipe_cfg *cfg)
{
    ib_mpool_t *mp;
    mp = ib_engine_pool_main_get(ib);
    if (ib_lock_init(&log_pipe_mutex) == IB_OK)
        ib_mpool_cleanup_register(mp, (void*)ib_lock_destroy, &log_pipe_mutex);
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
    if (rc = log_pipe_open((ib_engine_t*)ib, cfg), rc == IB_OK) {
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

/**
 * Main logging function: Ironbee callback to write a message to the pipe
 *
 * @param[in] ib  Ironbee engine
 * @param[in] level  Debug level
 * @param[in] file  File name
 * @param[in] line  Line number
 * @param[in] fmt Format string
 * @param[in] ap  Var args list to match the format
 * @param[in] dummy
 */
static void log_pipe_logger(const ib_engine_t *ib, ib_log_level_t level,
                            const char *file, int line, const char *fmt,
                            va_list ap, void *dummy)
{
    /* Just duplicate what's in all the server loggers */
    char buf[8192 + 1];
    int limit = 7000;
    ib_module_t *m;
    char timestr[26];
    int ec;
    log_pipe_cfg *cfg;
    time_t tm;

    assert(ib_engine_module_get((ib_engine_t *)ib, MODULE_NAME_STR, &m)
           == IB_OK);
    assert (m != NULL);
    assert(ib_context_module_config(ib_context_main(ib), m, &cfg) == IB_OK);
    assert (cfg != NULL);
    assert (cfg->pipe != NULL);

    if (level > cfg->log_level) {
        return;
    }

    /* TODO: configurable time format */
    time(&tm);
    ctime_r(&tm, timestr);
    timestr[24] = 0;  /* we don't want the newline */

    /* Buffer the log line. */
    ec = vsnprintf(buf, sizeof(buf), fmt, ap);
    MUTEX_LOCK;
    if (ec >= limit) {
        /* Mark as truncated, with a " ...". */
        memcpy(buf + (limit - 5), " ...", 5);

        /// @todo Do something about it
        if (fprintf(cfg->pipe, "%s: Log format truncated: limit (%d/%d)\n",
                    timestr, (int)ec, limit) < 0) {
            if (log_pipe_restart(ib, m, timestr, cfg) == IB_OK) {
                fprintf(cfg->pipe, "%s: Log format truncated: limit (%d/%d)\n",
                        timestr, (int)ec, limit);
            }
        }
    }

    if (fprintf(cfg->pipe, "%s %s [%s:%d]: %s\n", timestr,
                ib_log_level_to_string(level), file, line, buf) < 0) {
        /* On error, see if we can save anything.
         * There's no sensible error handling at this point.
         */
        if (log_pipe_restart(ib, m, timestr, cfg) == IB_OK) {
            fprintf(cfg->pipe, "%s %s [%s:%d]: %s\n", timestr,
                    ib_log_level_to_string(level), file, line, buf);
        }
    }
    MUTEX_UNLOCK;
}

/**
 * Ironbee callback to get current log level
 *
 * @param[in] ib  Ironbee engine
 * @param[in] dummy
 * @return  The current log level
 */
static ib_log_level_t log_pipe_get_level(const ib_engine_t *ib, void *dummy)
{
    log_pipe_cfg *cfg;
    ib_module_t *m;
    ib_context_t *ctx;

    assert(ib_engine_module_get((ib_engine_t*)ib, MODULE_NAME_STR, &m)
           == IB_OK);
    assert (m != NULL);
    /* This may get called after ctx has been invalidated, because
     * cleanup happens in a perverse order.
     */
    ctx = ib_context_main(ib);
    if (ctx == NULL) {
        return 4;
    }
    assert(ib_context_module_config(ctx, m, &cfg) == IB_OK);
    assert (cfg != NULL);

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
 * @param[in] data  config struct
 * @return  success or failure
 */
static ib_status_t log_pipe_open(ib_engine_t *ib, log_pipe_cfg *cfg)
{
    ib_mpool_t *mp;

    assert(cfg != NULL);

    if (cfg->cmdline == NULL) {
        ib_log_debug(ib, "Piped log not configured");
        return IB_OK;
    }
    mp = ib_engine_pool_main_get(ib);

    cfg->pipe = popen(cfg->cmdline, "w");
    if (cfg->pipe == NULL) {
        /* This'll get to the default logger - hopefully! */
        ib_log_critical(ib, "Failed to open pipe to %s!", cfg->cmdline);
        return IB_EOTHER;
    }
    ib_mpool_cleanup_register(mp, log_pipe_close, cfg);

    /* Now our pipe is up-and-running, register our own logger */
    ib_log_set_logger_fn(ib, log_pipe_logger, NULL);
    ib_log_set_loglevel_fn(ib, log_pipe_get_level, NULL);

    return IB_OK;
}

/**
 * Configuration function to read pipe's commandline and open the pipe
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

    assert(cp     != NULL);
    assert(cp->ib != NULL);
    assert(name   != NULL);
    assert(p1     != NULL);

    assert(ib_engine_module_get(cp->ib, MODULE_NAME_STR, &m)
           == IB_OK);
    assert (m != NULL);
    assert(ib_context_module_config(ib_context_main(cp->ib), m, &cfg) == IB_OK);
    assert (cfg != NULL);

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

    assert(cp     != NULL);
    assert(cp->ib != NULL);
    assert(name   != NULL);
    assert(p1     != NULL);

    assert(ib_engine_module_get(cp->ib, MODULE_NAME_STR, &m) == IB_OK);
    assert (m != NULL);
    assert(ib_context_module_config(ib_context_main(cp->ib), m, &cfg) == IB_OK);
    assert (cfg != NULL);

    cfg->log_level = ib_log_string_to_level(p1);

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
    NULL, NULL,                          /**< Context open function */
    NULL, NULL,                          /**< Context close function */
    NULL, NULL                           /**< Context destroy function */
);
