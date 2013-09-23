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

#ifndef _IB_LOGGER_PRIVATE_H_
#define _IB_LOGGER_PRIVATE_H_

/**
 * @file
 * @brief IronBee --- Engine Private Declarations
 *
 * @author Sam Baskinger <sbaskinger@qualys.com>
 */

#include <ironbee/logger.h>

/**
 * A collection of callbacks and function pointer that implement a logger.
 */
struct ib_logger_writer_t {
    ib_logger_open_fn      open_fn;     /**< Open the logger. */
    void                  *open_data;   /**< Callback data. */
    ib_logger_close_fn     close_fn;    /**< Close logs files. */
    void                  *close_data;  /**< Callback data. */
    ib_logger_reopen_fn    reopen_fn;   /**< Close and reopen log files. */
    void                  *reopen_data; /**< Callback data. */
    ib_logger_format_fn_t  format_fn;   /**< Signal that the queue has data. */
    void                  *format_data; /**< Callback data. */
    ib_logger_record_fn_t  record_fn;   /**< Signal a record is ready. */
    void                  *record_data; /**< Callback data. */
    ib_queue_t            *records;     /**< Records for the log writer. */
    ib_lock_t              records_lck; /**< Guard the queue. */
};

/**
 * A logger is what @ref ib_logger_rec_t are submitted to to produce a log.
 */
struct ib_logger_t {
    ib_log_level_t       level;       /**< The log level. */

    /**
     * Memory pool with a lifetime of the logger.
     */
    ib_mpool_t          *mp;

    ib_list_t           *writers;    /**< List of @ref ib_logger_writer_t. */
};

#endif /* _IB_LOGGER_PRIVATE_H_ */

