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

#include <ironbee/kvstore_riak.h>

#include <stdio.h>

static void * mp_malloc(ib_kvstore_t *kvstore,
                        size_t size,
                        ib_kvstore_cbdata_t *cbdata)
{
    IB_FTRACE_INIT();

    assert(kvstore);

    ib_kvstore_riak_server_t *riak =
        (ib_kvstore_riak_server_t *)kvstore->server; 

    IB_FTRACE_RET_PTR((void *), ib_mpool_alloc(riak->mp, size));
}

static void mp_free(ib_kvstore_t *kvstore,
                    void *ptr,
                    ib_kvstore_cbdata_t *cbdata)
{
    IB_FTRACE_INIT();

    /* Nop - the memory pool is released by the user. */

    IB_FTRACE_RET_VOID();
}

static ib_status_t kvget(
    ib_kvstore_t *kvstore,
    const ib_kvstore_key_t *key,
    ib_kvstore_value_t ***values,
    size_t *values_length,
    ib_kvstore_cbdata_t *cbdata)
{
    IB_FTRACE_INIT();
    //ib_kvstore_riak_server_t *riak =
        //(ib_kvstore_riak_server_t *)kvstore->server; 
    // FIXME
    IB_FTRACE_RET_STATUS(IB_OK);
}

static ib_status_t kvset(
    ib_kvstore_t *kvstore,
    ib_kvstore_merge_policy_fn_t merge_policy,
    const ib_kvstore_key_t *key,
    ib_kvstore_value_t *value,
    ib_kvstore_cbdata_t *cbdata)
{
    IB_FTRACE_INIT();
    //ib_kvstore_riak_server_t *riak =
        //(ib_kvstore_riak_server_t *)kvstore->server; 
    // FIXME
    IB_FTRACE_RET_STATUS(IB_OK);
}


static ib_status_t kvremove(
    ib_kvstore_t *kvstore,
    const ib_kvstore_key_t *key,
    ib_kvstore_cbdata_t *cbdata)
{
    IB_FTRACE_INIT();
    //ib_kvstore_riak_server_t *riak =
        //(ib_kvstore_riak_server_t *)kvstore->server; 
    // FIXME
    IB_FTRACE_RET_STATUS(IB_OK);
}
static ib_status_t kvconnect(
    ib_kvstore_server_t *server,
    ib_kvstore_cbdata_t *cbdata)
{
    IB_FTRACE_INIT();
    //ib_kvstore_riak_server_t *riak =
        //(ib_kvstore_riak_server_t *)kvstore->server; 
    // FIXME
    IB_FTRACE_RET_STATUS(IB_OK);
}
static ib_status_t kvdisconnect(
    ib_kvstore_server_t *server,
    ib_kvstore_cbdata_t *cbdata)
{
    IB_FTRACE_INIT();
    //ib_kvstore_riak_server_t *riak =
        //(ib_kvstore_riak_server_t *)kvstore->server; 
    // FIXME
    IB_FTRACE_RET_STATUS(IB_OK);
}
static void kvdestroy(
    ib_kvstore_t *kvstore,
    ib_kvstore_cbdata_t *cbdata)
{
    IB_FTRACE_INIT();
    //ib_kvstore_riak_server_t *riak =
        //(ib_kvstore_riak_server_t *)kvstore->server; 
    // FIXME
    IB_FTRACE_RET_VOID();
}

ib_status_t ib_kvstore_riak_init(
    ib_kvstore_t *kvstore,
    const char *base_url,
    const char *bucket,
    ib_mpool_t *mp)
{
    IB_FTRACE_INIT();

    assert(kvstore);
    assert(base_url);
    assert(bucket);

    ib_status_t rc;
    ib_kvstore_riak_server_t *server;
    size_t base_url_len;
    size_t bucket_len;
    size_t bucket_url_len;

    rc = ib_kvstore_init(kvstore);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* If the user gave us a memory pool, use memory pools allocator. */
    if (mp) {
        kvstore->malloc = mp_malloc;
        kvstore->free = mp_free;
    }

    server = kvstore->malloc(
        kvstore->server,
        sizeof(*server),
        kvstore->malloc_cbdata);
    base_url_len = strlen(base_url);
    bucket_len = strlen(bucket);
    bucket_url_len = base_url_len + 1 + bucket_len;

    server->riak_url = kvstore->malloc(
       kvstore,
       base_url_len + 1,
       kvstore->malloc_cbdata);
    if (!server->riak_url) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }
    snprintf(server->riak_url, base_url_len, "%s", base_url);

    server->bucket = kvstore->malloc(
       kvstore,
       bucket_len + 1,
       kvstore->malloc_cbdata);
    if (!server->bucket) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
        kvstore->free(kvstore, server->riak_url, kvstore->free_cbdata);
    }
    snprintf(server->bucket, bucket_len, "%s", bucket);

    server->bucket_url = kvstore->malloc(
       kvstore,
       base_url_len + bucket_len + 2,
       kvstore->malloc_cbdata);
    if (!server->bucket_url) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
        kvstore->free(kvstore, server->riak_url, kvstore->free_cbdata);
        kvstore->free(kvstore, server->bucket, kvstore->free_cbdata);
    }
    snprintf(server->bucket_url, bucket_url_len, "%s/%s", base_url, bucket);

    if (!server) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    kvstore->server = (ib_kvstore_server_t *)server;

    kvstore->get = kvget;
    kvstore->set = kvset;
    kvstore->remove = kvremove;
    kvstore->connect = kvconnect;
    kvstore->disconnect = kvdisconnect;
    kvstore->destroy = kvdestroy;

    kvstore->malloc_cbdata = NULL;
    kvstore->free_cbdata = NULL;
    kvstore->connect_cbdata = NULL;
    kvstore->disconnect_cbdata = NULL;
    kvstore->get_cbdata = NULL;
    kvstore->set_cbdata = NULL;
    kvstore->remove_cbdata = NULL;
    kvstore->merge_policy_cbdata = NULL;
    kvstore->destroy_cbdata = NULL;

    IB_FTRACE_RET_STATUS(IB_OK);
}
