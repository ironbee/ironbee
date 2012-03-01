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

#include <string.h>
#include <stdlib.h>

#include <ironbee/types.h>
#include <ironbee/debug.h>
#include <ironbee/types.h>
#include <ironbee/string.h>


ib_status_t string_to_num(const char *s, ib_bool_t allow_hex, ib_num_t *result)
{
    IB_FTRACE_INIT();
    size_t slen = strlen(s);
    ib_status_t rc = string_to_num_ex(s, slen, allow_hex, result);
    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t string_to_num_ex(const char *s,
                             size_t slen,
                             ib_bool_t allow_hex,
                             ib_num_t *result)
{
    IB_FTRACE_INIT();
    const char *pat;
    size_t offset;
    int base;

    /* Look at the string, does it look like a number */
    if (  (allow_hex == IB_TRUE) &&
          ( (*s == '0') && ((*(s+1) == 'x') || (*(s+1) == 'X')) )  ) {
        pat = "0123456789abcdefABCDEF";
        offset = 2;
        base = 16;
    }
    else if ( (*s == '-') || (*s == '+') ) {
        pat = "0123456789";
        offset = 1;
        base = 10;
    }
    else {
        pat = "0123456789";
        offset = 0;
        base = 10;
    }

    if (slen == offset) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }
    if (strspn(s+offset, pat) == (slen - offset) ) {
        *result = (ib_num_t)strtol(s, NULL, base);
    }
    else {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }
    IB_FTRACE_RET_STATUS(IB_OK);
}
