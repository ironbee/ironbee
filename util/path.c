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
 * @brief IronBee &mdash; Utility Functions
 * @author Brian Rectanus <brectanus@qualys.com>
 */

#include "ironbee_config_auto.h"

#include <ironbee/util.h>
#include <ironbee/string.h>
#include <ironbee/uuid.h>
#include <ironbee/debug.h>

#include <stdio.h>
#include <sys/stat.h>
#include <errno.h>
#include <libgen.h>
#include <ctype.h>
#include <string.h>
#include <assert.h>

#include "ironbee_util_private.h"

ib_status_t ib_util_mkpath(const char *path, mode_t mode)
{
    IB_FTRACE_INIT();
    char *ppath = NULL;
    char *cpath = NULL;
    ib_status_t rc;

    if (strcmp(path, ".") == 0 || strcmp(path, "/") == 0) {
        return IB_OK;
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

ib_status_t ib_util_normalize_path_ex(uint8_t *data,
                                      size_t dlen_in,
                                      bool win,
                                      size_t *dlen_out,
                                      ib_flags_t *result)
{
    IB_FTRACE_INIT();
    assert(data != NULL);
    assert(dlen_out != NULL);
    assert(result != NULL);

    uint8_t *src = data;
    uint8_t *dst = data;
    const uint8_t *end = data + (dlen_in - 1);
    bool hitroot = false;
    bool done = false;
    bool relative;
    bool trailing;
    bool modified = false;

    /* Some special cases */
    if (dlen_in == 0) {
        goto finish;
    }
    else if ( (dlen_in == 1) && (*src == '/') ) {
        goto finish;
    }
    else if ( (dlen_in == 2) && (*src == '.') && (*(src + 1) == '.') ) {
        goto finish;
    }

    /*
     * ENH: Deal with UNC and drive letters?
     */

    relative = ((*data == '/') || (win && (*data == '\\'))) ? false : true;
    trailing = ((*end == '/') || (win && (*end == '\\'))) ? true : false;

    while ( (done == false) && (src <= end) && (dst <= end) ) {

        /* Convert backslash to forward slash on Windows only. */
        if (win == true) {
            if (*src == '\\') {
                *src = '/';
                modified = true;
            }
            if ((src < end) && (*(src + 1) == '\\')) {
                *(src + 1) = '/';
                modified = true;
            }
        }

        /* Always normalize at the end of the input. */
        if (src == end) {
            done = true;
        }

        /* Skip normalization if this is NOT the end of the path segment. */
        /* else if ( (src + 1 != end) && (*(src + 1) != '/') ) { */
        else if (*(src + 1) != '/') {
            goto copy; /* Skip normalization. */
        }

        /*** Normalize the path segment. ***/

        /* Could it be an empty path segment? */
        if ( (src != end) && (*src == '/') ) {
            /* Ignore */
            modified = true;
            goto copy; /* Copy will take care of this. */
        }

        /* Could it be a back or self reference? */
        else if (*src == '.') {

            /* Back-reference? */
            if ( (dst > data) && (*(dst - 1) == '.') ) {
                /* If a relative path and either our normalization has
                 * already hit the rootdir, or this is a backref with no
                 * previous path segment, then mark that the rootdir was hit
                 * and just copy the backref as no normilization is possible.
                 */
                if (relative && (hitroot || ((dst - 2) <= data))) {
                    hitroot = true;

                    goto copy; /* Skip normalization. */
                }

                /* Remove backreference and the previous path segment. */
                modified = true; /* ? */
                dst -= 3;
                while ( (dst > data) && (*dst != '/') ) {
                    --dst;
                }

                /* But do not allow going above rootdir. */
                if (dst <= data) {
                    hitroot = true;
                    dst = data;

                    /* Need to leave the root slash if this
                     * is not a relative path and the end was reached
                     * on a backreference.
                     */
                    if (!relative && (src == end)) {
                        ++dst;
                    }
                }

                if (done) {
                    continue; /* Skip the copy. */
                }
                ++src;
                modified = true;
            }

            /* Relative Self-reference? */
            else if (dst == data) {
                modified = true;

                /* Ignore. */
                if (done) {
                    continue; /* Skip the copy. */
                }
                ++src;
            }

            /* Self-reference? */
            else if (*(dst - 1) == '/') {
                modified = true;

                /* Ignore. */
                if (done) {
                    continue; /* Skip the copy. */
                }
                --dst;
                ++src;
            }
        }

        /* Found a regular path segment. */
        else if (dst > data) {
            hitroot = false;
        }

  copy:
        /*** Copy the byte if required. ***/

        /* Skip to the last forward slash when multiple are used. */
        if (*src == '/') {
            uint8_t *tmp = src;

            while ( (src < end) &&
                    ( (*(src + 1) == '/') || (win && (*(src + 1) == '\\')) ) )
            {
                ++src;
            }
            if (tmp != src) {
                modified = true;
            }

            /* Do not copy the forward slash to the root
             * if it is not a relative path.  Instead
             * move over the slash to the next segment.
             */
            if (relative && (dst == data)) {
                ++src;
                continue;
            }
        }

        *(dst++) = *(src++);
    }

    /* Make sure that there is not a trailing slash in the
     * normalized form if there was not one in the original form.
     */
    if (!trailing && (dst > data) && *(dst - 1) == '/') {
        --dst;
    }
    if (!relative && (dst == data) ) {
        ++dst;
    }

finish:
    if (modified) {
        *dlen_out = (dst - data);
        *result = (IB_STRFLAG_ALIAS | IB_STRFLAG_MODIFIED );
    }
    else {
        *dlen_out = dlen_in;
        *result = IB_STRFLAG_ALIAS;
    }

    IB_FTRACE_RET_STATUS(IB_OK);
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
