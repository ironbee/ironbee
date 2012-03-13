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

#include <stdio.h>
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
                             int base,
                             ib_num_t *result)
{
    IB_FTRACE_INIT();
    assert(result != NULL);

    char buf[NUM_BUF_LEN+1];
    ib_status_t rc;

    /* Check for zero length string */
    if ( (s == NULL) || (slen > NUM_BUF_LEN) || (slen == 0) ) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    /* Copy the string to a buffer, let string_to_num() do the real work */
    memcpy(buf, s, slen);
    buf[slen] = '\0';
    rc = string_to_num(buf, base, result);
    IB_FTRACE_RET_STATUS(rc);
}

/**
 * Convert a string (with length) to a number.
 */
ib_status_t string_to_num(const char *s,
                          int base,
                          ib_num_t *result)
{
    IB_FTRACE_INIT();
    assert(result != NULL);

    size_t slen = strlen(s);
    char *end;
    long int value;
    size_t vlen;

    /* Check for zero length string */
    if ( (s == NULL) || (*s == '\0') ) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    /* Do the conversion, check for errors */
    value = strtol(s, &end, base);
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

/**
 * strstr() clone that works with non-NUL terminated strings
 */
const char *strstr_ex(const char *haystack,
                      size_t      haystack_len,
                      const char *needle,
                      size_t      needle_len)
{
    IB_FTRACE_INIT();
    size_t i = 0;
    size_t imax;

    /* If either pointer is NULL or either length is zero, done */
    if ( (haystack == NULL) || (haystack_len == 0) ||
         (needle == NULL) || (needle_len == 0) )
    {
        IB_FTRACE_RET_CONSTSTR(NULL);
    }

    /* Search for the needle */
    imax = haystack_len - (needle_len-1);
    for (i = 0; i < imax; ++i) {
        const char *hp = haystack + i;
        ib_bool_t found = IB_TRUE;
        size_t j = 0;

        for (j = 0; j < needle_len; ++j) {
            if ( *(hp + j) != *(needle + j) ) {
                found = IB_FALSE;
                break;
            }
        }
        if (found == IB_TRUE) {
            IB_FTRACE_RET_CONSTSTR(hp);
        }
    }

    IB_FTRACE_RET_CONSTSTR(NULL);
}
