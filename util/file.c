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
 */

#include "ironbee_config_auto.h"

#include <ironbee/file.h>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

ib_status_t ib_file_readall(
    ib_mm_t         mm,
    const char     *file,
    const uint8_t **out,
    size_t         *sz
)
{
    assert(file != NULL);
    assert(out != NULL);
    assert(sz != NULL);

    int          fd;
    uint8_t     *buf;
    size_t       bufsz;
    size_t       fill = 0;
    struct stat  stat_data;
    int          sys_rc;

    fd = open(file, O_RDONLY);
    if (fd == -1) {
        return IB_EINVAL;
    }

    /* Stat the file to get its total size. */
    sys_rc = fstat(fd, &stat_data);
    if (sys_rc != 0) {
        close(fd);
        return IB_EINVAL;
    }

    bufsz = stat_data.st_size;
    buf = ib_mm_alloc(mm, bufsz);
    if (buf == NULL) {
        close(fd);
        return IB_EALLOC;
    }

    for (;;) {
        ssize_t r = read(fd, buf + fill, bufsz - fill);

        /* EOF. */
        if (r == 0) {
            assert(fill == bufsz);
            *sz = fill;
            *out = buf;
            close(fd);
            return IB_OK;
        }

        /* Read error. */
        if (r < 0) {
            close(fd);
            return IB_EOTHER;
        }

        /* We read something. */
        fill += r;
        assert(fill <= bufsz);
    }

    /* Technically unreachable code. */
    assert(0 && "Unreachable code.");
}
