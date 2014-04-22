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
    assert(path != NULL);

    ib_status_t  rc = IB_OK;
    char        *work_path; /* Mutable copy of path. */
    size_t       path_i;    /* Iterator into path. */

    /* Skip leading slashes. */
    for (path_i = 0; path[path_i] == '/'; ++path_i){
        /* Nop. */
    }

    /* Corner case of a zero-length string. */
    if (path[path_i] == '\0') {
        /* Zero-length path is an error. Otherwise, success. */
        return (path_i == 0)? IB_EINVAL : IB_OK;
    }

    work_path = strdup(path);
    if (work_path == NULL) {
        return IB_EALLOC;
    }

    /* path_i points at the first non-slash character.
     * path_i does not point to the end-of-string (yet).
     * Iterate over the remaining path, creating directories as we go.
     * work_path is mutated as required to build arguments to mkdir(). */
    for (; path[path_i] != '\0'; ++path_i) {
        if (path[path_i] == '/') {
            /* Skip over the observed '/'. */
            ++path_i;

            /* Skip over any subsequent '/'s. */
            while (path[path_i] == '/') {
                ++path_i;
            }

            /* If a sequence of '/'s ends the string, success. */
            if (path[path_i] == '\0') {
                rc = IB_OK;
                goto exit;
            }
            /* We've encountered the start of a directory name. */
            else {
                /* Temporary character used to edit work_path with. */
                char   work_char;
                size_t path_j = path_i + 1 + strcspn(path + path_i + 1, "/");
                int    sys_rc;

                /* path[path_j] now points at a / or \0. */
                work_char = work_path[path_j];
                work_path[path_j] = '\0';

                sys_rc = mkdir(work_path, mode);
                if (errno == EEXIST) {
                    struct stat work_path_stat; /* Stat of path. */

                    sys_rc = stat(work_path, &work_path_stat);
                    if (sys_rc == -1) {
                        rc = IB_EOTHER;
                        goto exit;
                    }

                    /* Make sure the path is a directory. */
                    if( !S_ISDIR(work_path_stat.st_mode)) {
                        rc = IB_EOTHER;
                        goto exit;
                    }
                }
                else if (sys_rc == -1) {
                    rc = IB_EOTHER;
                    goto exit;
                }

                /* Put back the previous character. */
                work_path[path_j] = work_char;
            }
        }
    }

exit:
    free(work_path);
    return rc;
}

char *ib_util_path_join(ib_mm_t mm,
                        const char *parent,
                        const char *file_path)
{
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

    out = (char *)ib_mm_calloc(mm, len, 1);
    if (out == NULL) {
        return NULL;
    }

    strncpy(out, parent, plen);
    if ( (plen > 1) || ((plen == 1) && (*parent == '.')) ) {
        strcat(out, "/");
    }
    strncat(out, file_path, flen);

    return out;
}

char *ib_util_relative_file(ib_mm_t mm,
                            const char *ref_file,
                            const char *file_path)
{
    char *refcopy;       /* Copy of reference file */
    const char *ref_dir; /* Reference directory */
    char *tmp;

    /* If file_path is absolute, just use it */
    if (*file_path == '/') {
        tmp = ib_mm_strdup(mm, file_path);
        return tmp;
    }

    /* Make a copy of cur_file because dirname() modifies it's input */
    refcopy = (char *)ib_mm_strdup(mm, ref_file);
    if (refcopy == NULL) {
        return NULL;
    }

    /* Finally, extract the directory portion of the copy, use it to
     * build the final path. */
    ref_dir = dirname(refcopy);
    tmp = ib_util_path_join(mm, ref_dir, file_path);
    return tmp;
}
