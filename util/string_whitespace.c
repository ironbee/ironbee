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
 * @brief IronBee --- String Whitespace Funcitons.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include "ironbee_config_auto.h"

#include <ironbee/string_whitespace.h>

#include <assert.h>
#include <ctype.h>

/**
 * Count the total whitespace and number of whitespace regions.
 *
 * @param[in] data String.
 * @param[in] dlen Length of @a s.
 * @param[out] spaces Number of whitespace characters.
 * @param[out] regions Number of whitespace regions.
 **/
void count_ws(
    const uint8_t *data,
    size_t         dlen,
    size_t        *spaces,
    size_t        *regions
)
{
    assert(data != NULL);

    size_t local_spaces = 0;
    size_t local_regions = 0;
    local_spaces = 0;
    local_regions = 0;
    bool last_char_is_space = false;

    for (size_t i = 0; i < dlen; ++i) {
        uint8_t c = data[i];

        if (isspace(c)) {
            ++local_spaces;
            if (! last_char_is_space) {
                ++local_regions;
            }
            last_char_is_space = true;
        }
        else {
            last_char_is_space = false;
        }
    }

    if (spaces != NULL) {
        *spaces = local_spaces;
    }
    if (regions != NULL) {
        *regions = local_regions;
    }
}

ib_status_t ib_str_whitespace_remove(
    ib_mm_t         mm,
    const uint8_t  *data_in,
    size_t          dlen_in,
    uint8_t       **data_out,
    size_t         *dlen_out
)
{
    assert(data_in != NULL);
    assert(data_out != NULL);
    assert(dlen_out != NULL);

    size_t spaces = 0;
    uint8_t *buf;
    uint8_t *cur;

    count_ws(data_in, dlen_in, &spaces, NULL);
    buf = ib_mm_alloc(mm, dlen_in - spaces);
    if (buf == NULL) {
        return IB_EALLOC;
    }

    cur = buf;
    for (size_t i = 0; i < dlen_in; ++i) {
        uint8_t c = data_in[i];
        if (! isspace(c)) {
            *cur = c;
            ++cur;
        }
    }

    *data_out = buf;
    *dlen_out = dlen_in - spaces;

    return IB_OK;
}

ib_status_t ib_str_whitespace_compress(
    ib_mm_t         mm,
    const uint8_t  *data_in,
    size_t          dlen_in,
    uint8_t       **data_out,
    size_t         *dlen_out
)
{
    assert(data_in != NULL);
    assert(data_out != NULL);
    assert(dlen_out != NULL);

    size_t spaces = 0;
    size_t regions = 0;
    uint8_t *buf;
    uint8_t *cur;
    bool last_char_is_space = false;

    count_ws(data_in, dlen_in, &spaces, &regions);
    buf = ib_mm_alloc(mm, dlen_in - spaces + regions);
    if (buf == NULL) {
        return IB_EALLOC;
    }

    cur = buf;
    for (size_t i = 0; i < dlen_in; ++i) {
        uint8_t c = data_in[i];
        if (! isspace(c) || ! last_char_is_space) {
            *cur = c;
            ++cur;
        }

        last_char_is_space = isspace(c);
    }

    *data_out = buf;
    *dlen_out = dlen_in - spaces + regions;

    return IB_OK;
}
