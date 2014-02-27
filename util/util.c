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
 * @brief IronBee --- Utility Functions
 * @author Brian Rectanus <brectanus@qualys.com>
 */

#include "ironbee_config_auto.h"

#include <ironbee/util.h>

#include <ironbee/uuid.h>

#ifdef HAVE_LIBCURL
#include <curl/curl.h>
#endif

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <libgen.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

/* -- Logging -- */

static struct _ibutil_logger_t {
    ib_util_fn_logger_t   callback;
    int                   level;
    void                 *cbdata;
} _ibutil_logger = { NULL, 0, NULL };

/**
 * Builtin Logger.
 */
static void _builtin_logger(void *cbdata, int level,
                            const char *file, const char *func, int line,
                            const char *fmt, va_list ap)
{
    FILE *fp = (FILE *)cbdata;
    char fmt2[1024 + 1];

    assert(fp != NULL);
    assert(fmt != NULL);

    if ((file != NULL) && (line > 0)) {
        int ec = snprintf(fmt2, 1024,
                          "[%d] (%s:%d) %s\n",
                          level, file, line, fmt);
        /* It is a coding error if this format string is too big. */
        assert(ec <= 1024);
    }
    else {
        int ec = snprintf(fmt2, 1024,
                          "[%d] %s\n",
                          level, fmt);
        /* It is a coding error if this format string is too big. */
        assert(ec <= 1024);
    }

    vfprintf(fp, fmt2, ap);
    fflush(fp);
}

ib_status_t ib_util_log_level(int level)
{
    _ibutil_logger.level = level;

    return IB_OK;
}

int ib_util_get_log_level(void)
{
    return _ibutil_logger.level;
}

ib_status_t ib_util_log_logger(ib_util_fn_logger_t callback,
                               void *cbdata)
{
    _ibutil_logger.callback = callback;
    _ibutil_logger.cbdata = cbdata;

    return IB_OK;
}

ib_util_fn_logger_t ib_util_get_log_logger(void)
{
    return _ibutil_logger.callback;
}

void ib_util_log_ex(int level,
                    const char *file, const char *func, int line,
                    const char *fmt, ...)
{
    va_list ap;

    if ((_ibutil_logger.callback == NULL) || (level > _ibutil_logger.level)) {
        return;
    }

    va_start(ap, fmt);
    /* Only pass on the file/line data if we are in DEBUG level or higher. */
    if (_ibutil_logger.level >= 7) {
        _ibutil_logger.callback(_ibutil_logger.cbdata,
                                level, file, func, line, fmt, ap);
    }
    else {
        _ibutil_logger.callback(_ibutil_logger.cbdata,
                                level, NULL, NULL, 0, fmt, ap);
    }

    va_end(ap);
}


/* -- Misc -- */

uint8_t *ib_util_copy_on_write(ib_mm_t mm,
                               const uint8_t *data_in,
                               const uint8_t *end_in,
                               size_t size,
                               uint8_t *cur_out,
                               uint8_t **data_out,
                               const uint8_t **end_out)
{
    assert(data_in != NULL);
    assert(data_out != NULL);
    assert(end_in != NULL);
    assert(end_in >= data_in);

    if (*data_out == NULL) {
        *data_out = ib_mm_alloc(mm, size);
        if (*data_out != NULL) {
            if (end_out != NULL) {
                *end_out = *data_out + size;
            }
            size_t off = end_in - data_in;
            if (off == 0) {
                /* Do nothing */
            }
            else if (off == 1) {
                **data_out = *data_in;
            }
            else {
                memcpy(*data_out, data_in, off);
            }
            cur_out = *data_out + off;
        }
    }

    return cur_out;
}

char *ib_util_memdup_to_string(const void *in, size_t len)
{
    assert(in != NULL);

    char *p;
    if (len == 0 || in == NULL) {
        return NULL;
    }

    p = malloc(len + 1);
    if (p == NULL) {
        return NULL;
    }
    memcpy(p, in, len);
    p[len] = '\0';

    return p;
}

FILE *ib_util_fdup(FILE *fh, const char *mode)
{
    int      fd;
    int      new_fd = -1;
    FILE    *new_fh = NULL;

    // Step 1: Get the file descriptor of the file handle
    fd = fileno(fh);
    if ( fd < 0 ) {
        return NULL;
    }

    // Step 2: Get a new file descriptor (via dup(2) )
    new_fd = dup(fd);
    if ( new_fd < 0 ) {
        return NULL;
    }

    // Step 3: Create a new file handle from the new file descriptor
    new_fh = fdopen(new_fd, mode);
    if ( new_fh == NULL ) {
        // Close the file descriptor if fdopen() fails!!
        close( new_fd );
    }

    // Done
    return new_fh;
}

/* -- Library Setup -- */

ib_status_t ib_util_initialize(void)
{
    ib_status_t rc;

    rc = ib_util_log_logger(_builtin_logger, stderr);
    if (rc != IB_OK) {
        rc = ib_util_log_logger(NULL, NULL);
        return rc;
    }

    ib_util_log_level(3);

    rc = ib_uuid_initialize();
    if (rc != IB_OK) {
        return rc;
    }

#ifdef HAVE_LIBCURL
    CURLcode crc = curl_global_init(CURL_GLOBAL_ALL);
    if (crc) {
        return IB_EOTHER;
    }
#endif

    return IB_OK;
}

void ib_util_shutdown(void)
{
    ib_uuid_shutdown();

#ifdef HAVE_LIBCURL
    curl_global_cleanup();
#endif
}
