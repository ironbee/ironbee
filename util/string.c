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
 * @brief IronBee - String related functions
 * @author Nick LeRoy <nleroy@qualys.com>
 */

#include "ironbee_config_auto.h"

#include <stdlib.h>
#include <limits.h>
#include <assert.h>
#include <errno.h>

#include <ironbee/types.h>
#include <ironbee/debug.h>
#include <ironbee/types.h>
#include <ironbee/string.h>

/**
 * Length of the string buffer for converting strings.
 */
#define NUM_BUF_LEN 64

/**
 * Convert a string (with length) to a number.
 */
ib_status_t string_to_num_ex(const char *s,
                             size_t slen,
                             ib_bool_t allow_hex,
                             ib_num_t *result)
{
    IB_FTRACE_INIT();
    char buf[NUM_BUF_LEN+1];
    ib_status_t rc;

    assert(slen <= NUM_BUF_LEN);

    /* Check for zero length string */
    if (slen == 0) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    /* Copy the string to a buffer, let string_to_num() do the real work */
    memcpy(buf, buf, slen);
    buf[slen] = '\0';
    rc = string_to_num(buf, allow_hex, result);
    IB_FTRACE_RET_STATUS(rc);
}

/**
 * Convert a string (with length) to a number.
 */
ib_status_t string_to_num(const char *s, ib_bool_t allow_hex, ib_num_t *result)
{
    IB_FTRACE_INIT();
    size_t slen = strlen(s);
    char *end;
    long int value;
    size_t vlen;

    /* Check for zero length string */
    if (*s == '\0') {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    /* Do the conversion, check for errors */
    value = strtol(s, &end, (allow_hex == IB_FALSE ? 10 : 0) );
    vlen = (end - s);
    if (vlen != slen) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }
    else if ( ((value == LONG_MIN) || (value == LONG_MAX)) &&
              (errno == ERANGE) )
    {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }
    else {
        *result = value;
        IB_FTRACE_RET_STATUS(IB_OK);
    }
}
