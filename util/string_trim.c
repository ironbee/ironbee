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
 * @brief IronBee -- String Trimming
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 * @author Nick LeRoy <nleroy@qualys.com>
 */

#include "ironbee_config_auto.h"

#include <ironbee/string_trim.h>

#include <assert.h>
#include <ctype.h>
#include <stddef.h>

/**
 * Search start from the left for the first non-whitespace.
 *
 * @param[in] str String to search
 * @param[in] len Length of @a str.
 *
 * @returns Offset of first non-whitespace, @a len if none found.
 */
static
int find_nonws_left(
    const uint8_t *str,
    size_t         len
)
{
    assert (str != NULL);
    int i;

    for (i = 0; i < (int)len; ++i) {
        if (isspace(str[i]) == 0) {
            return i;
        }
    }

    return len;
}

/**
 * Search start from the right for the first non-whitespace.
 *
 * @param[in] str String to search.
 * @param[in] len Length of @a str.
 *
 * @returns Offset of first non-whitespace, -1 if none found.
 */
static
int find_nonws_right(
    const uint8_t *str,
    size_t         len
)
{
    assert (str != NULL);
    int i;

    for (i = len - 1; i >= 0; --i) {
        if (isspace(str[i]) == 0) {
            return i;
        }
    }

    return i;
}

ib_status_t ib_strtrim_left(
    const uint8_t  *data_in,
    size_t          dlen_in,
    const uint8_t **data_out,
    size_t         *dlen_out
)
{
    assert(data_in != NULL);
    assert(data_out != NULL);
    assert(dlen_out != NULL);

    int offset;

    offset = find_nonws_left(data_in, dlen_in);
    assert(offset <= (int)dlen_in);
    *data_out = data_in + offset;
    *dlen_out = dlen_in - offset;

    return IB_OK;
}

ib_status_t ib_strtrim_right(
    const uint8_t  *data_in,
    size_t          dlen_in,
    const uint8_t **data_out,
    size_t         *dlen_out
)
{
    assert(data_in != NULL);
    assert(data_out != NULL);
    assert(dlen_out != NULL);

    int offset;

    offset = find_nonws_right(data_in, dlen_in);
    *data_out = data_in;
    *dlen_out = offset + 1;

    return IB_OK;
}

ib_status_t ib_strtrim_lr(
    const uint8_t  *data_in,
    size_t          dlen_in,
    const uint8_t **data_out,
    size_t         *dlen_out
)
{
    assert(data_in != NULL);
    assert(data_out != NULL);
    assert(dlen_out != NULL);

    int left_offset;
    int right_offset;

    left_offset = find_nonws_left(data_in, dlen_in);
    right_offset = find_nonws_right(data_in, dlen_in);
    *data_out = data_in + left_offset;
    *dlen_out = right_offset - left_offset + 1;

    return IB_OK;
}
