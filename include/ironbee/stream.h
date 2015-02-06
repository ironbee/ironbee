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

#ifndef _IB_STREAM_H_
#define _IB_STREAM_H_

/**
 * @file
 * @brief IronBee --- Stream Routines
 *
 * @author Brian Rectanus <brectanus@qualys.com>
 */

#include <ironbee/build.h>
#include <ironbee/list.h>
#include <ironbee/mm.h>
#include <ironbee/types.h>
#include <ironbee/gen/list_gen.h>

#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup IronBeeUtilStream Stream Buffer
 * @ingroup IronBeeUtil
 *
 * A stream buffer is a list of buffers (data and length) which
 * can also act as a FIFO.
 *
 * @note In addition to the provided methods, the IB_LIST_* macros
 * can also be used as @ref ib_stream_t and @ref ib_sdata_t implement
 * the required list fields (@ref IB_LIST_REQ_FIELDS and
 * @ref IB_LIST_NODE_REQ_FIELDS, respectively).
 *
 * @{
 */

typedef struct ib_stream_t ib_stream_t;
typedef struct ib_sdata_t ib_sdata_t;

typedef enum {
    IB_STREAM_DATA,                      /**< Data is available */
    IB_STREAM_FLUSH,                     /**< Data should be flushed */
    IB_STREAM_EOH,                       /**< End of Header */
    IB_STREAM_EOB,                       /**< End of Body */
    IB_STREAM_EOS,                       /**< End of Stream */
    IB_STREAM_ERROR,                     /**< Error */
} ib_sdata_type_t;

/**
 * IronBee Stream.
 *
 * This is essentially a list of data chunks (@ref ib_sdata_t) with some
 * associated metadata.
 */
struct ib_stream_t {
    /// @todo Need a list of recycled sdata
    ib_mm_t                 mm;         /**< Stream memory manager */
    size_t                  slen;       /**< Stream length */
    IB_LIST_GEN_REQ_FIELDS(ib_sdata_t);     /* Required list fields */
};

/**
 * IronBee Stream Data.
 *
 * This is a type of list node, allowing IB_LIST_* macros to be used.
 */
struct ib_sdata_t {
    ib_sdata_type_t         type;       /**< Stream data type */
    size_t                  dlen;       /**< Data length */
    void                   *data;       /**< Data */
    IB_LIST_GEN_NODE_REQ_FIELDS(ib_sdata_t);/* Required list node fields */
};

/**
 * Create a stream buffer.
 *
 * @param pstream Address which new stream is written
 * @param mm Memory manager to use
 *
 * @returns Status code
 */
ib_status_t ib_stream_create(ib_stream_t **pstream, ib_mm_t mm);

/**
 * Push stream data into a stream.
 *
 * @param s Stream
 * @param sdata Stream data
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_stream_push_sdata(ib_stream_t *s,
                                            ib_sdata_t *sdata);

/**
 * Push a chunk of data (or metadata) into a stream.
 *
 * @param s Stream
 * @param type Stream data type
 * @param data Data
 * @param dlen Data length
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_stream_push(ib_stream_t *s,
                                      ib_sdata_type_t type,
                                      void *data,
                                      size_t dlen);

/**
 * Pull a chunk of data (or metadata) from a stream.
 *
 * @param s Stream
 * @param psdata Address which stream data is written
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_stream_pull(ib_stream_t *s,
                                      ib_sdata_t **psdata);

/**
 * Examine a chunk of data (or metadata) from a stream.
 *
 * @param s Stream
 * @param psdata Address which stream data is written
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_stream_peek(const ib_stream_t *s,
                                      ib_sdata_t **psdata);


/**
 * Get the next data segment.
 *
 * @param[in] sdata Stream data.
 *
 * @returns NULL or the next stream data.
 */
ib_sdata_t *ib_stream_sdata_next(ib_sdata_t *sdata);

/**
 * @} IronBeeUtilStream
 */


#ifdef __cplusplus
}
#endif

#endif /* _IB_STREAM_H_ */
