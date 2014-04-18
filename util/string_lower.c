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
 * @brief IronBee - String related functions
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include "ironbee_config_auto.h"

#include <ironbee/string_lower.h>

#include <assert.h>
#include <ctype.h>

ib_status_t ib_strlower(
    ib_mm_t         mm,
    const uint8_t  *in,
    size_t          in_len,
    uint8_t       **out
)
{
    assert(in != NULL);
    assert(out != NULL);

    *out = ib_mm_memdup(mm, in, in_len);
    if (*out == NULL) {
        return IB_EALLOC;
    }
    for (size_t i = 0; i < in_len; ++i) {
        int c = *(in+i);
        *(*out+i) = tolower(c);
    }

    return IB_OK;
}
