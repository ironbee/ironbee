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

#ifndef _IB_STREAM_IO_H_
#define _IB_STREAM_IO_H_

/**
 * @file
 * @brief IronBee --- Stream IO
 *
 * @author Sam Baskinger <sbaskinger@qualys.com>
 */

#include <ironbee/mm.h>
#include <ironbee/types.h>
#include <ironbee/mpool_freeable.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup IronBeeStreamIO Stream IO
 * @ingroup IronBeeUtil
 *
 * A stream manager that tracks ownership information about
 * data segments. Ownership information is expressed as a reference count
 * which, when it hits 0, causes the data segment to be destroyed. The
 * user is never shown the reference count, it is tracked through uses of
 * API calls against an ib_stream_io_tx_t.
 *
 * Data is read using.
 *
 * - ib_stream_io_data_depth() - Depth of the input queue.
 * - ib_stream_io_data_peek() - Data at the input head.
 * - ib_stream_io_data_peek_at() - Data at index i of the input stream.
 * - ib_stream_io_data_take() - Own the data at the head.
 * - ib_stream_io_data_slice() - Slice and own part of the data at the head.
 * - ib_stream_io_data_discard() - Throw away the head of the queue.
 *
 * Data is written using.
 *
 * - ib_stream_io_data_put() - Give ownership to the output queue.
 * - ib_stream_io_data_forward() - Take from the input and give to the output.
 * - ib_stream_io_data_flush() - Create a flush data segment in the output.
 * - ib_stream_io_data_close() - Create a close data segment in the output.
 * - ib_stream_io_data_error() - Create an error data segment in the output.
 *
 * Memory is allocated for writing using or released with.
 *
 * - ib_stream_io_data_alloc() - Create new data owned by the caller.
 *
 * Explicitly claiming or releasing ownership.
 *
 * - ib_stream_io_data_ref() - Explicitly claim ownership of data.
 * - ib_stream_io_data_unref() - Explicitly release ownership of data.
 *
 * There are a few functions that modify the transaction, itself.
 * These should not be used during tx processing. Stick to the
 * `ib_stream_io` calls when in your transaction callback function.
 *
 * - ib_stream_io_tx_create() - Create a io_tx.
 * - ib_stream_io_tx_data_add() - Add data to an io_tx.
 * - ib_stream_io_tx_flush_add() - Add flush to an io_tx input.
 * - ib_stream_io_tx_close_add() - Add close to an io_tx input.
 * - ib_stream_io_tx_error_add() - Add error to an io_tx input.
 * - ib_stream_io_tx_reuse() - Swap input with output for reuse.
 * - ib_stream_io_tx_redo() - Clear the output list. Resubmit the input list.
 *
 * @{
 */

enum ib_stream_io_type_t {
    IB_STREAM_IO_DATA,  /**< Data contains a pointer and a length. */
    IB_STREAM_IO_FLUSH, /**< All data should be flushed. */
    IB_STREAM_IO_CLOSE, /**< No more data will arrive. */
    IB_STREAM_IO_ERROR  /**< An error occurred in the previous step. */
};

//! The type of an ib_stream_io_data_t.
typedef enum ib_stream_io_type_t ib_stream_io_type_t;

//! The stream manager object.
typedef struct ib_stream_io_t ib_stream_io_t;

//! Structure to denote the boundaries of IO operations.
typedef struct ib_stream_io_tx_t ib_stream_io_tx_t;

//! Access to the data managed by an @ref ib_stream_io_t.
typedef struct ib_stream_io_data_t ib_stream_io_data_t;

/**
 * Create an io object.
 *
 * @param[out] io The IO manager.
 * @param[in] mm The memory manager that defines the lifetime of
 *            @a io and is used for simple structure allocations.
 *
 * @returns
 * - IB_OK On success.
 * - IB_EALLOC On an allocation error.
 * - Other on an unexpected error.
 */
ib_status_t DLL_PUBLIC ib_stream_io_create(
    ib_stream_io_t      **io,
    ib_mm_t               mm
) NONNULL_ATTRIBUTE(1);

/**
 * Create an empty transaction object.
 *
 * @param[out] io_tx The transaction object.
 * @param[in] io The IO stream to use for this transaction.
 *
 * @returns
 * - IB_OK On success.
 * - Other on error.
 */
ib_status_t DLL_PUBLIC ib_stream_io_tx_create(
    ib_stream_io_tx_t **io_tx,
    ib_stream_io_t     *io
);

/**
 * Add data into the transaction to be processed.
 *
 * Data is copied allowing the memory buffer to freed and the IO
 * processed later.
 *
 * @param[in] io_tx The transaction object.
 * @param[in] data The data to copy into this transaction.
 * @param[in] len The length of @a data.
 *
 * @returns
 * - IB_OK On succes.
 * - IB_EALLOC On allocation error.
 * - Other on another error.
 */
ib_status_t DLL_PUBLIC ib_stream_io_tx_data_add(
    ib_stream_io_tx_t *io_tx,
    const uint8_t     *data,
    size_t             len
) NONNULL_ATTRIBUTE(1, 2);

/**
 * Add a flush into the transaction to be processed.
 *
 * The added message goes into the input queue in anticipation
 * of the data being processed. This is typically used by a
 * controlling entity outside of the pump stream which is submitting
 * data to the stream to be processed.
 *
 * @param[in] io_tx The transaction object.
 *
 * @returns
 * - IB_OK On succes.
 * - IB_EALLOC On allocation error.
 * - Other on another error.
 */
ib_status_t ib_stream_io_tx_flush_add(
    ib_stream_io_tx_t *io_tx
) NONNULL_ATTRIBUTE(1);

/**
 * Add a close into the transaction to be processed.
 *
 * The added message goes into the input queue in anticipation
 * of the data being processed. This is typically used by a
 * controlling entity outside of the pump stream which is submitting
 * data to the stream to be processed.
 *
 * @param[in] io_tx The transaction object.
 *
 * @returns
 * - IB_OK On succes.
 * - IB_EALLOC On allocation error.
 * - Other on another error.
 */
ib_status_t ib_stream_io_tx_close_add(
    ib_stream_io_tx_t *io_tx
) NONNULL_ATTRIBUTE(1);

/**
 * Add an error into the transaction to be processed.
 *
 * The added message goes into the input queue in anticipation
 * of the data being processed. This is typically used by a
 * controlling entity outside of the pump stream which is submitting
 * data to the stream to be processed.
 *
 * @param[in] io_tx The transaction object.
 * @param[in] msg The error message. This will be copied.
 * @param[in] len The message length.
 *
 * @returns
 * - IB_OK On succes.
 * - IB_EALLOC On allocation error.
 * - Other on another error.
 */
ib_status_t ib_stream_io_tx_error_add(
    ib_stream_io_tx_t *io_tx,
    const char        *msg,
    size_t             len
) NONNULL_ATTRIBUTE(1, 2);

/**
 * Reuse @a io_tx by making the output the input and emptying output.
 *
 * This allows for chaining data through a processing pipeline.
 *
 * @param[in] io_tx The transaction to reuse.
 *
 * @returns
 * - IB_OK On success.
 * - Other on error.
 */
ib_status_t DLL_PUBLIC ib_stream_io_tx_reuse(
    ib_stream_io_tx_t *io_tx
) NONNULL_ATTRIBUTE(1);


/**
 * Setup a transaction to replay through anther stage reusing the input.
 *
 * The output queue is cleared.
 *
 * @param[in] io_tx The transaction to reuse.
 *
 * @returns
 * - IB_OK On success.
 * - Other on error.
 */
ib_status_t DLL_PUBLIC ib_stream_io_tx_redo(
    ib_stream_io_tx_t *io_tx
) NONNULL_ATTRIBUTE(1);

/**
 * Clean up a transaction, releasing all resources.
 *
 * A transaction is not usable after this is called.
 * If you would like to reuse a transaction object,
 * using its output stream as the input, see ib_stream_io_tx_reuse().
 *
 * @param[in] io_tx The transaction to cleanup.
 *
 * @sa ib_stream_io_tx_reuse()
 */
void DLL_PUBLIC ib_stream_io_tx_cleanup(
    ib_stream_io_tx_t *io_tx
) NONNULL_ATTRIBUTE(1);

/**
 * Return now many segments of data are available to take or forward.
 *
 * @param[in] io_tx The IO transaction.
 *
 * @returns The depth of the input queue.
 */
size_t DLL_PUBLIC ib_stream_io_data_depth(
    ib_stream_io_tx_t *io_tx
) NONNULL_ATTRIBUTE(1);

/**
 * Peek at the data available to take or forward without changing ownership.
 *
 * @param[in] io_tx The IO transaction.
 * @param[out] ptr The pointer to the start of the data segment if this is a
 *             @ref IB_STREAM_IO_DATA type. NULL otherwise.
 * @param[out] len The length of the data, in bytes, pointed to by @a ptr.
 *             If @a type is @a IB_STREAM_IO_DATA, then this is set to 0.
 * @param[out] type The type of data.
 *             - @ref IB_STREAM_IO_DATA
 *             - @ref ib_stream_io_data_flush
 *
 * @returns
 * - IB_OK On success.
 * - IB_ENOENT If the input queue is empty.
 * - Other on error.
 */
ib_status_t DLL_PUBLIC ib_stream_io_data_peek(
    ib_stream_io_tx_t   *io_tx,
    uint8_t             **ptr,
    size_t               *len,
    ib_stream_io_type_t  *type
) NONNULL_ATTRIBUTE(1);


/**
 * Peek at the data available to take or forward without changing ownership.
 *
 * @param[in] io_tx The IO transaction.
 * @param[in] index The index to examine.
 * @param[out] ptr The pointer to the start of the data segment if this is a
 *             @ref IB_STREAM_IO_DATA type. NULL otherwise.
 * @param[out] len The length of the data, in bytes, pointed to by @a ptr.
 *             If @a type is @a IB_STREAM_IO_DATA, then this is set to 0.
 * @param[out] type The type of data.
 *             - @ref IB_STREAM_IO_DATA
 *             - @ref ib_stream_io_data_flush
 *
 * @returns
 * - IB_OK On success.
 * - IB_EINVAL If the queue does not contain the index.
 * - IB_ENOENT If the input queue is empty.
 * - Other on error.
 */
ib_status_t DLL_PUBLIC ib_stream_io_data_peek_at(
    ib_stream_io_tx_t    *io_tx,
    size_t                index,
    uint8_t             **ptr,
    size_t               *len,
    ib_stream_io_type_t  *type
) NONNULL_ATTRIBUTE(1);

/**
 * Remove data from the input queue, taking ownership of it.
 *
 * If ownership is not passed to the output queue using
 * ib_stream_io_data_put() and this data is not wished to be buffered
 * then ib_stream_io_data_unref() should be called on @a param data to
 * release this processor's claim on the data.
 *
 * @param[in] io_tx The IO transaction.
 * @param[out] data This is the ownership information of the data.
 * @param[out] ptr If not null, set to the address of the data.
 * @param[out] len If not null, set to the length.
 * @param[out] type If not null, set to the type of the data.
 *             If this is not of type IB_STREAM_IO_DATA then
 *             *len and *ptr are set to 0 and NULL.
 *
 * @returns
 * - IB_OK On success.
 * - IB_ENOENT If the input queue is empty.
 * - Other on an unexpected error.
 */
ib_status_t DLL_PUBLIC ib_stream_io_data_take(
    ib_stream_io_tx_t    *io_tx,
    ib_stream_io_data_t **data,
    uint8_t             **ptr,
    size_t               *len,
    ib_stream_io_type_t  *type
) NONNULL_ATTRIBUTE(1);

/**
 * Give ownership of @a data to the output queue.
 *
 * If the user would like to retain ownership of the data
 * she should call ib_stream_io_data_ref() on @a data.
 *
 * @param[in] io_tx The IO transaction.
 * @param[in] data The data whose ownership is changing.
 *
 * @returns
 * - IB_OK On success.
 * - Other on unexpected errors.
 */
ib_status_t DLL_PUBLIC ib_stream_io_data_put(
    ib_stream_io_tx_t  *io_tx,
    ib_stream_io_data_t *data
) NONNULL_ATTRIBUTE(1, 2);

/**
 * Insert a new flush data object in the output queue.
 *
 * @param[in] io_tx The IO transaction.
 *
 * @returns
 * - IB_OK On success.
 * - Other on error.
 */
ib_status_t DLL_PUBLIC ib_stream_io_data_flush(
    ib_stream_io_tx_t *io_tx
) NONNULL_ATTRIBUTE(1);

/**
 * Insert a new close data object in the output queue.
 *
 * @param[in] io_tx The IO transaction.
 *
 * @returns
 * - IB_OK On success.
 * - Other on error.
 */
ib_status_t DLL_PUBLIC ib_stream_io_data_close(
    ib_stream_io_tx_t *io_tx
) NONNULL_ATTRIBUTE(1);

/**
 * Insert a new error data object in the output queue.
 *
 * @param[in] io_tx The IO transaction.
 * @param[in] msg The message for the error data to carry as its data.
 * @param[in] len Length of @a msg.
 *
 * @returns
 * - IB_OK On success.
 * - Other on error.
 */
ib_status_t DLL_PUBLIC ib_stream_io_data_error(
    ib_stream_io_tx_t *io_tx,
    const char        *msg,
    size_t             len
) NONNULL_ATTRIBUTE(1, 2);

/**
 * Allocate a new segment of data owned by the caller.
 *
 * If this data is not to be retained, it should be released by
 * calling ib_stream_io_data_unref() on @a data.
 *
 * @param[in] io_tx The IO transaction.
 * @param[in] len The length allocated.
 * @param[out] data The data segment handle.
 * @param[out] ptr The pointer to the data in @a data.
 *
 * @returns
 * - IB_OK On success.
 * - Other on error.
 */
ib_status_t DLL_PUBLIC ib_stream_io_data_alloc(
    ib_stream_io_tx_t    *io_tx,
    size_t                len,
    ib_stream_io_data_t **data,
    uint8_t             **ptr
) NONNULL_ATTRIBUTE(1);

/**
 * Slice the data at the head of the input queue.
 *
 * This increases an internal reference count to the backing memory
 * causing it to stay around until the sliced data is freed.
 *
 * @param[in] io_tx The IO transaction.
 * @param[in] start The start offset into the data.
 * @param[in] length The length from start to slice.
 * @param[out] dst This is the ownership information of the data.
 * @param[out] ptr If not null, set to the address of the data.
 *
 * @returns
 * - IB_OK On success.
 * - IB_EINVAL If the data type at the head of the input queue is not
 *   a IB_STREAM_IO_DATA.
 */
ib_status_t DLL_PUBLIC ib_stream_io_data_slice(
    ib_stream_io_tx_t    *io_tx,
    size_t                start,
    size_t                length,
    ib_stream_io_data_t **dst,
    uint8_t             **ptr
) NONNULL_ATTRIBUTE(1);

/**
 * Remove the head of the input queue and discard it.
 *
 * @param[in] io_tx The transaction object.
 *
 * @returns
 * - IB_OK On success.
 * - IB_ENOENT If the input queue is empty.
 */
ib_status_t DLL_PUBLIC ib_stream_io_data_discard(
    ib_stream_io_tx_t *io_tx
) NONNULL_ATTRIBUTE(1);

/**
 * Take the head of the input queue and forward it to the output queue.
 *
 * @param[in] io_tx The IO transaction.
 *
 * @returns
 * - IB_OK On success.
 * - IB_ENOENT If the input queue is empty.
 */
ib_status_t DLL_PUBLIC ib_stream_io_data_forward(
    ib_stream_io_tx_t *io_tx
) NONNULL_ATTRIBUTE(1);

/**
 * Explicitly take ownership of a data segment.
 *
 * @param[in] io_tx IO Transaction.
 * @param[in] data The data segment to alter.
 */
void DLL_PUBLIC ib_stream_io_data_ref(
    ib_stream_io_tx_t   *io_tx,
    ib_stream_io_data_t *data
) NONNULL_ATTRIBUTE(1, 2);

/**
 * Explicitly release ownership of a data segment.
 *
 * @param[in] io_tx IO Transaction.
 * @param[in] data The data segment to alter.
 */
void DLL_PUBLIC ib_stream_io_data_unref(
    ib_stream_io_tx_t   *io_tx,
    ib_stream_io_data_t *data
) NONNULL_ATTRIBUTE(1, 2);


/** @} IronBeeStreamIO */

#ifdef __cplusplus
}
#endif
#endif /* _IB_STREAM_IO_H_ */