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

#ifndef _TEST_H
#define	_TEST_H

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>

#define UNKNOWN     0
#define CLIENT      1
#define SERVER      2

#ifndef O_BINARY
#define O_BINARY    0
#endif

typedef struct test_t test_t;

struct test_t {
    char *buf;
    size_t pos;
    size_t len;

    char *chunk;
    size_t chunk_offset;
    size_t chunk_len;
    int chunk_direction;
};

int test_run(const char *testsdir, const char *testname, htp_cfg_t *cfg, htp_connp_t **connp);

#endif	/* _TEST_H */

