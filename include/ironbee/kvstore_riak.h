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

#include <ironbee/kvstore.h>
#include <ironbee/mm.h>
#include <ironbee/types.h>

#include <curl/curl.h>

#include <assert.h>

/**
 * @file
 * @brief IronBee --- Key-Value Filesystem Store Interface
 *
 * @author Sam Baskinger <sbaskinger@qualys.com>
 */

/**
 * @addtogroup IronBeeKeyValueStore
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
    ib_mm_t mm;            /**< Memory manager. */
    CURL *curl;            /**< Curl context for web requests. */
    char *client_id;       /**< The Riak client id. */
    char *vclock;          /**< NULL or vector clock for queries to riak. */
    char *etag;            /**< NULL or etag for queries to riak. */
};
typedef struct ib_kvstore_riak_server_t ib_kvstore_riak_server_t;

/**
 * @param[out] kvstore The key-value store object to initialize.
 * @param[in] client_id A unique identifier of this client.
 * @param[in] base_url The base URL where the Riak HTTP interface is rooted.
 * @param[in] bucket The riak bucket that keys are stored in.
 * @param[in] mm The memory manager allocations will be made out of.
 *               If this is IB_MM_NULL then the normal malloc/free
 *               implementation will be used.
 * @returns
 *   - IB_OK on success
 *   - IB_EALLOC on memory allocation failure using malloc.
 */
ib_status_t ib_kvstore_riak_init(
    ib_kvstore_t *kvstore,
    const char *client_id,
    const char *base_url,
    const char *bucket,
    ib_mm_t mm);

/**
 * Set (not copy) vclock in @a kvstore.
 *
 * This field is not freed when the riak server is destroyed. The
 * user should free and null them when they are done with the transaction.
 *
 * @param[in] kvstore Key-value store.
 * @param[in] vclock The vector clock.
 */
void ib_kvstore_riak_set_vclock(ib_kvstore_t *kvstore, char *vclock);

/**
 * Set (not copy) vclock in @a kvstore.
 *
 * This field is not freed when the riak server is destroyed. The
 * user should free and null them when they are done with the transaction.
 *
 * @param[in] kvstore Key-value store.
 * @param[in] etag The etag.
 */
void ib_kvstore_riak_set_etag(ib_kvstore_t *kvstore, char *etag);

/**
 * Get vclock from @a kvstore.
 *
 * @returns The current value of kvstore->vclock.
 */
char * ib_kvstore_riak_get_vclock(ib_kvstore_t *kvstore);

/**
 * Get vclock from @a kvstore.
 *
 * @returns The current value of kvstore->etag.
 */
char * ib_kvstore_riak_get_etag(ib_kvstore_t *kvstore);

/**
 * Check if the server is reachable.
 * @param[in] kvstore The Key-Value store.
 * @returns True if the server is reachable, False if it is not for any reason.
 */
int ib_kvstore_riak_ping(ib_kvstore_t *kvstore);

/**
 * Allows for setting a bucket property on an existing bucket.
 *
 * @param[in] kvstore Key-value store.
 * @param[in] property The name of the property to set.
 * @param[in] value The value of the property. Must be greater than 0 and
 *            less than 999999.
 *
 * @returns
 *   - IB_OK On Success.
 *   - IB_EALLOC On a memory allocation error.
 */
ib_status_t ib_kvstore_riak_set_bucket_property_int(
    ib_kvstore_t *kvstore,
    const char *property,
    int value);

/**
 * Allows for setting a bucket property on an existing bucket.
 *
 * @param[in] kvstore Key-value store.
 * @param[in] property The name of the property to set.
 * @param[in] value The value of the property
 *
 * @returns
 *   - IB_OK On Success.
 *   - IB_EALLOC On a memory allocation error.
 */
ib_status_t ib_kvstore_riak_set_bucket_property_str(
    ib_kvstore_t *kvstore,
    const char *property,
    const char *value);
/**
 * @}
 */
#endif // __KVSTORE_RIAK_H
