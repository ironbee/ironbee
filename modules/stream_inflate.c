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
 * @brief IronBee --- Stream decompression.
 *
 * @author Craig Forbes <cforbes@qualys.com>
 */

#include <ironbee/string.h>

//TODO REMOVE
#include <ironbee/log.h>

#include "stream_inflate_private.h"
#include <zlib.h>

//ib_stream_processor_create_fn
ib_status_t create_inflate_processor(
    void    *instance_data,
    ib_tx_t *tx,
    void    *cbdata
)
{
  z_stream *strm;
  int ret;

  strm = (z_stream *) ib_mm_alloc(tx->mm, sizeof(z_stream));
  if (strm == NULL) {
    return IB_EALLOC;
  }
  // Use default memory allocation routines.
  strm->zalloc = Z_NULL;
  strm->zfree = Z_NULL;
  strm->opaque = Z_NULL;
  strm->avail_in = 0;
  strm->next_in = Z_NULL;
  ret = inflateInit(strm);
  if (ret != Z_OK) {
    return IB_EOTHER;
  }
  *(void **)instance_data = (void *)strm;
  return IB_OK;
}

void destroy_inflate_processor(
    void *instance_data,
    void *cbdata
)
{
    //NOOP
}

//ib_stream_processor_execute_fn
ib_status_t execute_inflate_processor(
    void                *instance_data,
    ib_tx_t             *tx,
    ib_mm_t              mm_eval,
    ib_stream_io_tx_t   *io_tx,
    void                *cbdata
)
{
    z_stream *strm = (z_stream *) instance_data;
    int ret;
    ib_status_t rc;
    ib_stream_io_data_t *stream_data;
    uint8_t *buf;
    size_t buf_len;
    ib_stream_io_type_t  data_type;

    do {
        rc = ib_stream_io_data_take(io_tx, &stream_data, &buf, &buf_len, &data_type);
        if (rc == IB_ENOENT) {
            rc = IB_OK;
            break;
        }
        if (data_type == IB_STREAM_IO_FLUSH) {
            ib_stream_io_data_put(io_tx, stream_data);
            continue;
        }
        strm->next_in = buf;
        strm->avail_in = buf_len;
        do {
            ib_stream_io_data_t *out_data;
            uint8_t *out_buf;
            const size_t CHUNK_SIZE = 8096;
            rc = ib_stream_io_data_alloc(io_tx, CHUNK_SIZE, &out_data, &out_buf);
            if (rc != IB_OK) {
                break;
            }
            strm->avail_out = CHUNK_SIZE;
            strm->next_out = out_buf;
            ret = inflate(strm, Z_NO_FLUSH);
            if (ret == Z_DATA_ERROR || ret == Z_NEED_DICT) {
                ib_stream_io_data_error(io_tx, IB_S2SL("Invalid compressed data"));
                (void)inflateEnd(strm);
                return IB_EOTHER;
            }
            else if (ret < 0) {
                ib_stream_io_data_error(io_tx, IB_S2SL("Internal error inflating stream"));
                (void)inflateEnd(strm);
                return IB_EOTHER;
            }

            if (strm->avail_out == 0) {
                rc = ib_stream_io_data_put(io_tx, out_data);
                if (rc != IB_OK) {
                    break;
                }
            }
            else {
                ib_stream_io_data_t *sliced_data;
                rc = ib_stream_io_data_slice(io_tx,
                                             out_data,
                                             0, CHUNK_SIZE - strm->avail_out,
                                             &sliced_data, NULL);
                if (rc != IB_OK) {
                    break;
                }
                rc = ib_stream_io_data_put(io_tx, sliced_data);
                if (rc != IB_OK) {
                    break;
                }
                ib_stream_io_data_unref(io_tx, out_data);
            }
        } while (strm->avail_out == 0);

        ib_stream_io_data_unref(io_tx, stream_data);
    } while (ib_stream_io_data_depth(io_tx) > 0);

    if (rc != IB_OK) {
        (void)inflateEnd(strm);
    }

    return rc;
}
