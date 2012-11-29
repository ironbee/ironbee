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

#ifndef __KVSTORE_RIAK_H
#define __KVSTORE_RIAK_H

#include "ironbee_config_auto.h"

#include <ironbee/debug.h>
#include <ironbee/kvstore.h>
#include <ironbee/mpool.h>
#include <ironbee/types.h>

#include <assert.h>
#include <curl/curl.h>

/**
 * @file
 * @brief IronBee --- Key-Value Filesystem Store Interface
 *
 * @author Sam Baskinger <sbaskinger@qualys.com>
 */

/**
 * @ingroup IronBeeKeyValueStore Key-Value Filesystem Store
 * @ingroup IronBeeUtil
 * @{
 */

/**
 * The riak server object.
 */
struct ib_kvstore_riak_server_t {
    char *riak_url;        /**< Riak URL. */
    size_t riak_url_len;   /**< Length of riak URL. */
    char *bucket;          /**< The name of the bucket. */
    size_t bucket_len;     /**< Length of bucket. */
    char *bucket_url;      /**< riak_url with the bucket appended. */
    size_t bucket_url_len; /**< Length of bucket_url. */
    ib_mpool_t *mp;        /**< Memory pool. */
    CURL *curl;            /**< Curl context for web requests. */
    const char *vclock;    /**< NULL or vector clock for queries to riak. */
    const char *etag;      /**< NULL or etag for queries to riak. */
};
typedef struct ib_kvstore_riak_server_t ib_kvstore_riak_server_t;

/**
 * @param[out] kvstore The key-value store object to initialize.
 * @param[in] base_url The base URL where the Riak HTTP interface is rooted.
 * @param[in] bucket The riak bucket that keys are stored in.
 * @param[in,out] mp The memory pool allocations will be made out of.
 *                   If this is NULL then the normal malloc/free
 *                   implementation will be used.
 * @returns
 *   - IB_OK on success
 *   - IB_EALLOC on memory allocation failure using malloc.
 */
ib_status_t ib_kvstore_riak_init(
    ib_kvstore_t *kvstore,
    const char *base_url,
    const char *bucket,
    ib_mpool_t *mp);

/**
 * Set (not copy) vclock in @a kvstore.
 *
 * This field is not freed when the riak server is destroyed. The
 * user should free and null them when they are done with the transaction.
 *
 * @param[in] kvstore Key-value store.
 * @param[in] vclock The vector clock.
 */
void ib_kvstore_riak_set_vlcock(ib_kvstore_t *kvstore, const char *vclock);

/**
 * Set (not copy) vclock in @a kvstore.
 *
 * This field is not freed when the riak server is destroyed. The
 * user should free and null them when they are done with the transaction.
 *
 * @param[in] kvstore Key-value store.
 * @param[in] etag The etag.
 */
void ib_kvstore_riak_set_etag(ib_kvstore_t *kvstore, const char *etag);

/**
 * Get vclock from @a kvstore.
 *
 * @returns The current value of kvstore->vclock.
 */
const char * ib_kvstore_riak_get_vlcock(ib_kvstore_t *kvstore);

/**
 * Get vclock from @a kvstore.
 *
 * @returns The current value of kvstore->etag.
 */
const char * ib_kvstore_riak_get_etag(ib_kvstore_t *kvstore);

 /**
  * @}
  */
#endif // __KVSTORE_RIAK_H
