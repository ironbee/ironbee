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
 * @file
 * @brief IronBee - Utility Functions
 * @author Brian Rectanus <brectanus@qualys.com>
 */

#include "ironbee_config_auto.h"

#include <stdio.h>
#include <apr_lib.h>
#include <apr_general.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <libgen.h>

#include <ironbee/util.h>

#include "ironbee_util_private.h"


/* -- Logging -- */

static struct _ibutil_logger_t {
    ib_util_fn_logger_t   callback;
    int                   level;
    void                 *cbdata;
} _ibutil_logger;

/**
 * @internal
 *
 * Builtin Logger.
 */
static void _builtin_logger(FILE *fh, int level,
                            const char *prefix, const char *file, int line,
                            const char *fmt, va_list ap)
{
    char fmt2[1024 + 1];

    /// @todo Builtin logger only logs level<4
    if (level > 4) {
        return;
    }

    if ((file != NULL) && (line > 0)) {
        int ec = snprintf(fmt2, 1024,
                          "%s[%d] (%s:%d) %s\n",
                          (prefix?prefix:""), level, file, line, fmt);
        if (ec > 1024) {
            /// @todo Do something better
            abort();
        }
    }
    else {
        int ec = snprintf(fmt2, 1024,
                          "%s[%d] %s\n",
                          (prefix?prefix:""), level, fmt);
        if (ec > 1024) {
            /// @todo Do something better
            abort();
        }
    }

    vfprintf(fh, fmt2, ap);
    fflush(fh);
}

ib_status_t ib_util_log_level(int level)
{
    _ibutil_logger.level = level;

    return IB_OK;
}

ib_status_t ib_util_log_logger(ib_util_fn_logger_t callback,
                               void *cbdata)
{
    _ibutil_logger.callback = callback;
    _ibutil_logger.cbdata = cbdata;

    return IB_OK;
}

void ib_util_log_ex(int level,
                    const char *prefix, const char *file, int line,
                    const char *fmt, ...)
{
    va_list ap;

    if ((_ibutil_logger.callback == NULL) || (level > _ibutil_logger.level)) {
        return;
    }

    va_start(ap, fmt);
    _ibutil_logger.callback(_ibutil_logger.cbdata, level,
                            prefix, file, line, fmt, ap);
    va_end(ap);
}


/* -- Misc -- */

ib_status_t ib_util_mkpath(const char *path, mode_t mode)
{
    char *ppath = NULL;
    char *cpath = NULL;
    ib_status_t rc;

    if (strcmp(path, ".") == 0 || strcmp(path, "/") == 0) {
        return IB_OK;
    }

    /* Attempt to create the dir.  If it returns ENOENT, then 
     * recursivly attempt to create the parent dir(s) until
     * they are all created.
     */
    if ((mkdir(path, mode) == -1) && (errno == ENOENT)) {
        int ec;

        /* Some implementations may modify the path argument,
         * so make a copy first. */
        if ((cpath = strdup(path)) == NULL) {
            return IB_EALLOC;
        }

        if ((ppath = dirname(cpath)) == NULL) {
            rc = IB_EINVAL;
            goto cleanup;
        }

        rc = ib_util_mkpath(ppath, mode);
        if (rc != IB_OK) {
            goto cleanup;
        }

        /* Parent path was created, so try again. */
        ec = mkdir(path, mode);
        if (ec == -1) {
            ec = errno;
            ib_util_log_error(3, "Failed to create path \"%s\": %s (%d)",
                              path, strerror(ec), ec);
            rc = IB_EINVAL;
            goto cleanup;
        }
    }

    rc = IB_OK;

cleanup:
    if (cpath != NULL) {
        free(cpath);
    }

    return rc;
}


/* -- Library Setup -- */

ib_status_t ib_initialize(void)
{
    ib_status_t rc;

    if (apr_initialize() != APR_SUCCESS) {
        return IB_EUNKNOWN;
    }

    rc = ib_util_log_logger((ib_util_fn_logger_t)_builtin_logger, stderr);
    if (rc != IB_OK) {
        rc = ib_util_log_logger(NULL, NULL);
        return rc;
    }

    ib_util_log_level(3);

    return IB_OK;
}

void ib_shutdown(void)
{
    apr_terminate();
}

