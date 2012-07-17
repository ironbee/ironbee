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
 * @brief IronBee &mdash; String escape functions
 * @author Nick LeRoy <nleroy@qualys.com>
 */

#include "ironbee_config_auto.h"

#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <ctype.h>
#include <stdbool.h>
#include <inttypes.h>

#include <ironbee/types.h>
#include <ironbee/debug.h>
#include <ironbee/types.h>
#include <ironbee/mpool.h>
#include <ironbee/string.h>
#include <ironbee/escape.h>

/* Convert a bytestring to a json string with escaping, ex version */
ib_status_t ib_string_escape_json_ex(ib_mpool_t *mp,
                                     const uint8_t *data_in,
                                     size_t dlen_in,
                                     bool nul,
                                     char **data_out,
                                     size_t *dlen_out,
                                     ib_flags_t *result)
{
    IB_FTRACE_INIT();
    assert(mp != NULL);
    assert(data_in != NULL);
    assert(data_out != NULL);
    assert(result != NULL);

    const uint8_t *iptr;
    const uint8_t *iend = data_in + dlen_in;
    size_t mult = 2;     /* Size multiplier */
    size_t buflen;       /* Length of data buf can hold */
    size_t bufsize;      /* Size of allocated buffer (may hold trailing nul) */
    char *buf;
    char *optr;
    const char *oend;
    bool modified = false;

allocate:
    buflen = mult * dlen_in;
    bufsize = buflen + (nul ? 1 : 0);
    buf = ib_mpool_alloc(mp, bufsize);
    if (buf == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }
    oend = buf + buflen;
    optr = buf;
    for (iptr = data_in; iptr < iend; ++iptr) {
        size_t size = 1;
        const char *ostr = NULL;
        uint8_t c = *iptr;
        switch (c) {

        case '\"':
            size = 2;
            ostr = "\\\"";
            break;

        case '\\':
            size = 2;
            ostr = "\\\\";
            break;

        case '/':
            size = 2;
            ostr = "\\/";
            break;

        case '\b':
            size = 2;
            ostr = "\\b";
            break;

        case '\f':
            size = 2;
            ostr = "\\f";
            break;

        case '\n':
            size = 2;
            ostr = "\\n";
            break;

        case '\r':
            size = 2;
            ostr = "\\r";
            break;

        case '\t':
            size = 2;
            ostr = "\\t";
            break; 

        case '\0':
            size = 5;
            ostr = "\\0000";
            break;

        default:
            size = 1;
            break;
        }

        if (optr + size > oend) {
            assert (mult == 2);
            mult = 5;
            goto allocate;
        }
        if (size == 1) {
            *optr = (char)*iptr;
            ++optr;
        }
        else {
            memcpy(optr, ostr, size);
            optr += size;
            modified = true;
        }
   }

    /* Add on our nul byte if required */
    if (nul) {
        *optr = '\0';
        ++optr;
    }
    if (modified) {
        *result = IB_STRFLAG_MODIFIED | IB_STRFLAG_NEWBUF;
    }
    else {
        *result = IB_STRFLAG_NEWBUF;
    }
    *data_out = buf;
    if (dlen_out != NULL) {
        *dlen_out = optr - buf;
    }
    IB_FTRACE_RET_STATUS(IB_OK);
}

/* Convert a c-string to a json string with escaping */
ib_status_t ib_string_escape_json(ib_mpool_t *mp,
                                  const char *data_in,
                                  char **data_out,
                                  ib_flags_t *result)
{
    IB_FTRACE_INIT();
    assert(mp != NULL);
    assert(data_in != NULL);
    assert(data_out != NULL);
    assert(result != NULL);

    ib_status_t rc;
    rc = ib_string_escape_json_ex(mp,
                                  (const uint8_t *)data_in, strlen(data_in),
                                  true,
                                  data_out, NULL,
                                  result);
    IB_FTRACE_RET_STATUS(rc);
}
