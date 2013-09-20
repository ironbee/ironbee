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

#ifndef _IB_ENGINE_MANAGER_PRIVATE_H_
#define _IB_ENGINE_MANAGER_PRIVATE_H_

/**
 * @file
 * @brief IronBee --- Engine Manager private
 *
 * @author Nick LeRoy <nleroy@qualys.com>
 */
#include <ironbee/engine_manager.h>

#include <ironbee/engine_types.h>
#include <ironbee/list.h>
#include <ironbee/lock.h>
#include <ironbee/log.h>
#include <ironbee/mpool.h>
#include <ironbee/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup IronBeeEngineManagerPrivate IronBee Engine Manager Private types and API
 * @ingroup IronBeeEngineManager
 *
 * @{
 */

/* The manager's engine wrapper type */
typedef struct ib_manager_engine_t ib_manager_engine_t;

/**
 * The Engine Manager.
 */
struct ib_manager_t {
    const ib_server_t    *server;          /**< Server object */
    ib_mpool_t           *mpool;           /**< Engine Manager's Memory pool */
    size_t                max_engines;     /**< The maximum number of engines */

    /*
     * List of all managed engines, and other related items.  These items are
     * all protected by the engine list lock.  To keep the implementation
     * simple, the latest engine is always stored at the end of the list, and
     * the list is compacted after removing elements from it.
     */
    ib_manager_engine_t **engine_list;     /**< Array of all engines */
    size_t                engine_count;    /**< Count of engines */
    ib_manager_engine_t  *engine_current;  /**< Current IronBee engine */
    volatile size_t       inactive_count;  /**< Count of inactive engines */

    /* The locks themselves */
    ib_lock_t             engines_lock;    /**< The engine list lock */
    ib_lock_t             creation_lock;   /**< Serialize engine creation */

    /* Logging */
    ib_log_level_t            log_level;        /**< Log level for manager */
    ib_manager_log_buf_fn_t   log_buf_fn;       /**< Logger formatted buffer */
    void                     *log_buf_cbdata;   /**< Logger callback data */
    ib_manager_log_flush_fn_t log_flush_fn;     /**< Logger flush function */
    void                     *log_flush_cbdata; /**< Logger callback data */
    void (*callback)(void* cbdata);             /**< Function to callback under mutex */
};

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* _IB_ENGINE_MANAGER_PRIVATE_H_ */
