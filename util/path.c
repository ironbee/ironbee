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

#include <ironbee/path.h>

#include <ironbee/debug.h>
#include <ironbee/string.h>
#include <ironbee/util.h>

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <libgen.h>
#include <stdio.h>
#include <string.h>

#include <sys/stat.h>

ib_status_t ib_util_mkpath(const char *path, mode_t mode)
{
    IB_FTRACE_INIT();
    char *ppath = NULL;
    char *cpath = NULL;
    ib_status_t rc;

    if (strcmp(path, ".") == 0 || strcmp(path, "/") == 0) {
        IB_FTRACE_RET_STATUS(IB_OK);
    }

    /* Attempt to create the dir.  If it returns ENOENT, then
     * recursively attempt to create the parent dir(s) until
     * they are all created.
     */
    if ((mkdir(path, mode) == -1) && (errno == ENOENT)) {
        int ec;

        /* Some implementations may modify the path argument,
         * so make a copy first. */
        if ((cpath = strdup(path)) == NULL) {
            IB_FTRACE_RET_STATUS(IB_EALLOC);
        }

        /* Some implementation returns a pointer to internal storage which
         * may change in recursive calls.  So another copy. */
        if ((ppath = strdup(dirname(cpath))) == NULL) {
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
            ib_util_log_error("Failed to create path \"%s\": %s (%d)",
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
    if (ppath != NULL) {
        free(ppath);
    }

    IB_FTRACE_RET_STATUS(rc);
}

char *ib_util_path_join(ib_mpool_t *mp,
                        const char *parent,
                        const char *file_path)
{
    IB_FTRACE_INIT();
    size_t len;
    size_t plen;  /* Length of parent */
    size_t flen;  /* Length of file_path */
    char *out;

    /* Strip off extraneous trailing slash chars */
    plen = strlen(parent);
    while( (plen >= 2) && (*(parent+(plen-1)) == '/') ) {
        --plen;
    }

    /* Ignore leading and trailing slash chars in file_path */
    flen = strlen(file_path);
    while ( (flen > 1) && (*file_path == '/') ) {
        ++file_path;
        --flen;
    }
    while ( (flen > 1) && (*(file_path+(flen-1)) == '/') ) {
        --flen;
    }

    /* Allocate & generate the include file name */
    len = plen;                /* Parent directory */
    if ( (plen > 1) || ((plen == 1) && (*parent == '.')) ) {
        len += 1;              /* slash */
    }
    len += flen;               /* file name */
    len += 1;                  /* NUL */

    out = (char *)ib_mpool_calloc(mp, len, 1);
    if (out == NULL) {
        IB_FTRACE_RET_STR(NULL);
    }

    strncpy(out, parent, plen);
    if ( (plen > 1) || ((plen == 1) && (*parent == '.')) ) {
        strcat(out, "/");
    }
    strncat(out, file_path, flen);

    IB_FTRACE_RET_STR(out);
}

char *ib_util_relative_file(ib_mpool_t *mp,
                            const char *ref_file,
                            const char *file_path)
{
    IB_FTRACE_INIT();
    char *refcopy;       /* Copy of reference file */
    const char *ref_dir; /* Reference directory */
    char *tmp;

    /* If file_path is absolute, just use it */
    if (*file_path == '/') {
        tmp = ib_mpool_strdup(mp, file_path);
        IB_FTRACE_RET_STR(tmp);
    }

    /* Make a copy of cur_file because dirname() modifies it's input */
    refcopy = (char *)ib_mpool_strdup(mp, ref_file);
    if (refcopy == NULL) {
        IB_FTRACE_RET_STR(NULL);
    }

    /* Finally, extract the directory portion of the copy, use it to
     * build the final path. */
    ref_dir = dirname(refcopy);
    tmp = ib_util_path_join(mp, ref_dir, file_path);
    IB_FTRACE_RET_STR(tmp);
}

ib_status_t ib_util_normalize_path(char *data,
                                   bool win,
                                   ib_flags_t *result)
{
    IB_FTRACE_INIT();
    ib_status_t rc;
    size_t len;
    rc = ib_util_normalize_path_ex((uint8_t *)data,
                                   strlen(data),
                                   win,
                                   &len,
                                   result);
    if (rc == IB_OK) {
        *(data+len) = '\0';
    }
    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_util_normalize_path_cow(
    ib_mpool_t *mp,
    const char *data_in,
    bool win,
    char **data_out,
    ib_flags_t *result)
{
   IB_FTRACE_INIT();
    assert(mp != NULL);
    assert(data_in != NULL);
    assert(data_out != NULL);
    assert(result != NULL);

    ib_status_t rc;
    size_t len;
    char *buf;

    /* Make a copy of the original */
    buf = ib_mpool_strdup(mp, data_in);
    if (buf == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    /* Let normalize_path_ex() do the real work. */
    rc = ib_util_normalize_path_ex((uint8_t *)buf,
                                   strlen(data_in),
                                   win,
                                   &len,
                                   result);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* If it's modified, point at the newly allocated buffer. */
    if (*result & IB_STRFLAG_MODIFIED) {
        *result = (IB_STRFLAG_NEWBUF | IB_STRFLAG_MODIFIED );
        *(buf + len) = '\0';
        *data_out = buf;
    }
    else {
        *data_out = (char *)data_in;
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t ib_util_normalize_path_cow_ex(ib_mpool_t *mp,
                                          const uint8_t *data_in,
                                          size_t dlen_in,
                                          bool win,
                                          uint8_t **data_out,
                                          size_t *dlen_out,
                                          ib_flags_t *result)
{
    IB_FTRACE_INIT();
    assert(mp != NULL);
    assert(data_in != NULL);
    assert(data_out != NULL);
    assert(dlen_out != NULL);
    assert(result != NULL);

    ib_status_t rc;
    uint8_t *buf;

    /* Make a copy of the original */
    buf = ib_mpool_alloc(mp, dlen_in);
    if (buf == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }
    memcpy(buf, data_in, dlen_in);

    /* Let normalize_path_ex() do the real work. */
    rc = ib_util_normalize_path_ex(buf, dlen_in, win, dlen_out, result);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* If it's modified, point at the newly allocated buffer. */
    if (*result & IB_STRFLAG_MODIFIED) {
        *result = (IB_STRFLAG_NEWBUF | IB_STRFLAG_MODIFIED);
        *data_out = buf;
    }
    else {
        *data_out = (uint8_t *)data_in;
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}
