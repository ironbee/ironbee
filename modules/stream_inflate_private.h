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
 * @brief IronBee --- Stream inflate decompression.
 *
 * @author Craig Forbes <cforbes@qualys.com>
 */

#ifndef __MODULES__STREAM_INFLATE_H
#define __MODULES__STREAM_INFLATE_H

#include <ironbee/stream_processor.h>
#include <ironbee/stream_io.h>

#ifdef __cplusplus
extern "C" {
#endif

ib_status_t create_inflate_processor(
    void    *instance_data,
    ib_tx_t *tx,
    void    *cbdata
);

void destroy_inflate_processor(
    void *instance_data,
    void *cbdata
);

ib_status_t execute_inflate_processor(
    void                *instance_data,
    ib_tx_t             *tx,
    ib_mm_t              mm_eval,
    ib_stream_io_tx_t   *io_tx,
    void                *cbdata
);

#ifdef __cplusplus
}
#endif

#endif
