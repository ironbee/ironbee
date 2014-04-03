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
 * @brief IronBee --- Persist to riak.
 */

#include "ironbee_config_auto.h"

#include <ironbee/kvstore_riak.h>
#include "kvstore_private.h"

#include <ironbee/mm.h>
#include <ironbee/mm_mpool.h>

#include <ctype.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define EXPIRATION "X-Riak-Meta-Expiration"
#define CREATION "X-Riak-Meta-Creation"
#define VCLOCK "X-Riak-Vclock"
#define ETAG "ETag"
#define CONTENT_TYPE "Content-Type"

/**
 * Convenience function.
 */
static inline void * kvmalloc(ib_kvstore_t *kvstore, size_t size) {
    return kvstore->malloc(kvstore, size, kvstore->malloc_cbdata);
}

/**
 * Convenience function.
 */
static inline void kvfree(ib_kvstore_t *kvstore, void *ptr) {
    kvstore->free(kvstore, ptr, kvstore->free_cbdata);
}
/**
 * Memory buffer.
 */
struct membuffer_t {
    ib_kvstore_t *kvstore; /**< KV store. */
    char *buffer;          /**< Pointer to the buffer to read. */
    size_t size;           /**< The size of buffer. */
    size_t read;           /**< The length that has already been ready. */
};
typedef struct membuffer_t membuffer_t;

/**
 * This does not free @a membuffer but frees all not-null fields.
 *
 * Also numeric fields are zero'ed.
 *
 * The kvstore is not free'ed allowing this membuffer to be reused.
 */
static void cleanup_membuffer(membuffer_t *membuffer)
{
    membuffer->size = 0;
    membuffer->read = 0;

    if (membuffer->buffer) {
        kvfree(membuffer->kvstore, membuffer->buffer);
    }
}

/**
 * Null all member values and set kvstore = @a kvstore.
 * @param[in] kvstore The Key Value store.
 * @param[out] buf The initialized object.
 */
static void membuffer_init(ib_kvstore_t *kvstore, membuffer_t *buf)
{
    buf->kvstore = kvstore;
    buf->read = 0;
    buf->size = 0;
    buf->buffer = NULL;
}

/**
 * Riak headers.
 */
struct riak_headers_t {
    ib_kvstore_t *kvstore;    /**< KV store. */
    int status;               /**< HTTP Status. */
    char *x_riak_vclock;      /**< X-Riak-Vclock */
    char *content_type;       /**< Content-Type */
    char *etag;               /**< ETag. */
    ib_time_t expiration;     /**< Expiration. */
    ib_time_t creation;       /**< Creation time */
};
typedef struct riak_headers_t riak_headers_t;

/**
 * Null all member values and set kvstore = @a kvstore.
 * @param[in] kvstore The Key Value store.
 * @param[out] headers The initialized object.
 */
static void riak_headers_init(ib_kvstore_t *kvstore, riak_headers_t *headers)
{
    headers->kvstore = kvstore;
    headers->status = 0;
    headers->expiration = 0;
    headers->creation = 0;
    headers->x_riak_vclock = NULL;
    headers->content_type = NULL;
    headers->etag = NULL;
}

/**
 * This does not free @a riak_headers but frees all not-null fields.
 *
 * Also numeric fields are zero'ed.
 *
 * The kvstore is not free'ed allowing this riak_headers to be reused.
 */
static void cleanup_riak_headers(riak_headers_t *riak_headers)
{
    riak_headers->status = 0;
    riak_headers->expiration = 0;
    riak_headers->creation = 0;

    if (riak_headers->content_type) {
        kvfree(riak_headers->kvstore, riak_headers->content_type);
    }
    if (riak_headers->etag) {
        kvfree(riak_headers->kvstore, riak_headers->etag);
    }
    if (riak_headers->x_riak_vclock) {
        kvfree(riak_headers->kvstore, riak_headers->x_riak_vclock);
    }
}

/**
 * Convert @a response and @a headers into a @a value.
 *
 * This copies the response buffer and related headers
 * into the given @a value.
 *
 * @param[in] kvstore Key-value store.
 * @param[in] riak kvstore->server object, extracted.
 * @param[in] response The response from the server.
 * @param[in] headers Riak headers captured during the get.
 * @param[in] mm Memory manager @a pvalue is allocated out of.
 * @param[out] pvalue The value to be populated.
 *
 * @returns
 *   - IB_OK success.
 *   - IB_EALLOC memory allocation.
 */
static ib_status_t http_to_kvstore_value(
    ib_kvstore_t              *kvstore,
    ib_kvstore_riak_server_t  *riak,
    membuffer_t               *response,
    riak_headers_t            *headers,
    ib_mm_t                    mm,
    ib_kvstore_value_t       **pvalue)
{
    assert(kvstore != NULL);
    assert(riak != NULL);
    assert(response != NULL);
    assert(headers != NULL);
    assert(pvalue != NULL);

    ib_kvstore_value_t *value;
    ib_status_t         rc;

    rc = ib_kvstore_value_create(&value, mm);
    if (rc != IB_OK) {
        return rc;
    }

    /* Set value. */
    {
        const uint8_t *data =
            ib_mm_memdup(
                mm,
                response->buffer,
                response->read);
        if (value == NULL) {
            return IB_EALLOC;
        }

        ib_kvstore_value_value_set(value, data, response->read);
    }

    /* Set type. */
    {
        size_t      type_length = strlen(headers->content_type);
        const char *type =
            ib_mm_memdup(
                mm,
                headers->content_type,
                type_length);
        if (type == NULL) {
            return IB_EALLOC;
        }

        ib_kvstore_value_type_set(value, type, type_length);
    }

    ib_kvstore_value_expiration_set(value, headers->expiration);
    ib_kvstore_value_creation_set(value, headers->creation);

    *pvalue = value;

    return IB_OK;
}

/**
 * Conditional copy.
 *
 * @param[in] kvstore Key-value store.
 * @param[in] header The header to check for.
 * @param[in] ptr The header line.
 * @param[in] ptr_len The pointer length.
 * @param[out] dest A copy of the header value is written here.
 * @returns
 *   - IB_OK on success.
 *   - IB_EINVAL if the header is not found in ptr.
 *   - IB_EALLOC Allocation error.
 */
static ib_status_t cond_copy_header(
    ib_kvstore_t *kvstore,
    const char *header,
    const char *ptr,
    size_t ptr_len,
    char **dest)
{

    size_t header_len = strlen(header);
    size_t value_sz;

    /* If header cannot prefix ptr (because header is too long)... */
    if (ptr_len <= header_len) {
        return IB_EINVAL;
    }

    /* If header is not prefixed by ptr... */
    if (strncmp(ptr, header, header_len)) {
        return IB_EINVAL;
    }

    value_sz = ptr_len - header_len + 1;

    *dest = kvmalloc(kvstore, value_sz);
    if (*dest == NULL) {
        return IB_EALLOC;
    }

    strncpy(*dest, ptr + header_len, value_sz);

    /* If we are given a header with \r\n at the end of it, terminate the
     * string early. This is always the case with Curl. */
    if (value_sz >= 3 && (*dest)[value_sz-3] == '\r') {
        (*dest)[value_sz-3] = '\0';
    }

    return IB_OK;
}

/**
 * Using fields in @a riak this will build a linked list of headers.
 *
 * The result of this must be freed using curl_slist_free_all.
 *
 * @param[in] kvstore The Key-Value store.
 * @param[in] riak The extracted kvstore->server object.
 * @param[in] value Optional value to specify the Content-Type. May be NULL.
 * @returns
 *   - A normally allocated struct curl_slist using curl_slist_append.
 *   - NULL if no headers are set.
 */
static struct curl_slist* build_custom_headers(
    ib_kvstore_t *kvstore,
    ib_kvstore_riak_server_t *riak,
    ib_kvstore_value_t *value)
{
    assert(kvstore != NULL);
    assert(riak != NULL);

    const size_t buffer_len = 1024;
    char *header = malloc(buffer_len);
    struct curl_slist *slist = NULL;

    if (header == NULL) {
        return NULL;
    }

    if (riak->vclock != NULL) {
        snprintf(header, buffer_len, VCLOCK ": %s", riak->vclock);
        slist = curl_slist_append(slist, header);
    }

    if (riak->etag != NULL) {
        snprintf(header, buffer_len, ETAG ": %s", riak->etag);
        slist = curl_slist_append(slist, header);
    }

    if (riak->client_id != NULL) {
        snprintf(header, buffer_len, "X-Riak-ClientId: %s", riak->client_id);
        slist = curl_slist_append(slist, header);
    }

    if (value != NULL) {
        const char *type;
        size_t      type_length;

        ib_kvstore_value_type_get(value, &type, &type_length);

        if (type_length > 0) {
            snprintf(
                header,
                buffer_len,
                "Content-Type: %.*s",
                (int)type_length,
                type);
            slist = curl_slist_append(slist, header);
        }

        snprintf(
            header,
            buffer_len,
            EXPIRATION ": %"PRIu64,
            ib_kvstore_value_expiration_get(value));
        slist = curl_slist_append(slist, header);

        snprintf(
            header,
            buffer_len,
            CREATION ": %"PRIu64,
            ib_kvstore_value_creation_get(value));
        slist = curl_slist_append(slist, header);
    }

    free(header);

    return slist;
}

/**
 * Capture some specific riak headers.
 */
static size_t riak_header_capture(
    void *ptr,
    size_t size,
    size_t nmemb,
    void *userdata)
{

    assert(userdata != NULL);
    assert(ptr != NULL);

    char *cptr = ptr;
    ib_status_t rc;
    size_t total = size * nmemb;
    riak_headers_t *riak_headers = (riak_headers_t *)userdata;

    assert(riak_headers->kvstore != NULL);

    if (total > 12) {
        char status[4] = { cptr[9], cptr[10], cptr[11], '\0' };
        int tmpstat = atoi(status);

        /* If we found a valid status, return. */
        if (tmpstat >= 100 && tmpstat < 600) {
            riak_headers->status = tmpstat;
            goto exit;
        }
    }

    rc = cond_copy_header(
        riak_headers->kvstore,
        VCLOCK ": ",
        ptr,
        size * nmemb,
        &riak_headers->x_riak_vclock);
    if (rc == IB_OK) {
        goto exit;
    }

    rc = cond_copy_header(
        riak_headers->kvstore,
        ETAG ": ",
        ptr,
        size * nmemb,
        &riak_headers->etag);
    if (rc == IB_OK) {
        goto exit;
    }

    rc = cond_copy_header(
        riak_headers->kvstore,
        CONTENT_TYPE ": ",
        ptr,
        size * nmemb,
        &riak_headers->content_type);
    if (rc == IB_OK) {
        goto exit;
    }

exit:
    return size * nmemb;
}

/**
 * This function manages writing returned data from curl into a buffer.
 *
 * @param[in] ptr The buffer.
 * @param[in] size The size of each memory buffer.
 * @param[in] nmemb The number of memory buffers.
 * @param[out] userdata A membuffer_t pointer.
 * @returns The length of bytes written.
 *
 * @see http://curl.haxx.se/libcurl/c/curl_easy_setopt.html#CURLOPTWRITEFUNCTION
 */
static size_t membuffer_writefunction(
    char *ptr,
    size_t size,
    size_t nmemb,
    void *userdata)
{
    membuffer_t  *mb = (membuffer_t *)userdata;
    ib_kvstore_t *kvstore;

    assert(mb != NULL);
    assert(mb->kvstore != NULL);

    kvstore = mb->kvstore;

    /* Resize mb. */
    if (size * nmemb > mb->size - mb->read) {
        size_t new_size = size * nmemb + mb->size + 4096;

        char *buffer_tmp = kvmalloc(kvstore, new_size);
        if (!buffer_tmp) {
            return 0;
        }

        /* Avoid situations where buffer is null. */
        if (mb->buffer) {
            if (mb->read > 0) {
                memcpy(buffer_tmp, mb->buffer, mb->read);
            }

            kvfree(kvstore, mb->buffer);
        }

        mb->buffer = buffer_tmp;
        mb->size = new_size;
    }

    memcpy(mb->buffer + mb->read, ptr, size * nmemb);
    mb->read += size * nmemb;

    return size * nmemb;
}

/**
 * This function manages reading a membuffer_t from the @a userdata.
 *
 * @param[out] ptr Data read is written into this pointer.
 * @param[in] size This * @a nmemb is the maximum size that may be written to
 *            @a ptr.
 * @param[in] nmemb This * @a size is the maximum size that may be written to
 *            @a ptr.
 * @param userdata This is the user callback data set. This must be a
 *        membuffer_t *.
 *
 * @returns
 *   - 0 Signals that there is no more data to send.
 *   - CURL_READFUNC_PAUSE - This is not used.
 *   - CURL_READFUNC_ABORT - This is not used.
 * @see http://curl.haxx.se/libcurl/c/curl_easy_setopt.html#CURLOPTREADFUNCTION
 */
static size_t membuffer_readfunction(
    void *ptr,
    size_t size,
    size_t nmemb,
    void *userdata)
{

    assert(ptr != NULL);
    assert(userdata != NULL);

    size_t len;
    membuffer_t *mb = (membuffer_t *)userdata;

    if (mb->size - mb->read < size*nmemb) {
        len = mb->size - mb->read;
    }
    else {
        len = size * nmemb;
    }

    if (len > 0) {
        memcpy(ptr, mb->buffer + mb->read, len);

        mb->read += len;
    }

    return len;
}

/**
 * Allocates new string using kvstore->malloc representing a riak key url.
 *
 * Caller should free this with kvstore->free.
 *
 * @param[in] kvstore Key-value store.
 * @param[in] riak The riak server data already pulled out of kvstore.
 * @param[in] key The key to be converted into a URL.
 *
 * @returns NULL on failure.
 */
static char * build_key_url(
    ib_kvstore_t *kvstore,
    ib_kvstore_riak_server_t *riak,
    const ib_kvstore_key_t *key)
{

    assert(kvstore != NULL);
    assert(riak != NULL);
    assert(key != NULL);

    char *url;
    size_t url_len;

    size_t         key_len;
    const uint8_t *key_data;

    ib_kvstore_key_get(key, &key_data, &key_len);

    /* bucket + /keys/ + key */
    url_len = riak->bucket_url_len + 6 + key_len;
    url = kvmalloc(kvstore, url_len + 1);
    if (!url) {
        return NULL;
    }

    snprintf(url, url_len, "%s/keys/%s", riak->bucket_url, (char *)key_data);

    return url;
}

static void * mm_malloc(ib_kvstore_t *kvstore,
                        size_t size,
                        ib_kvstore_cbdata_t *cbdata)
{

    assert(kvstore != NULL);

    ib_kvstore_riak_server_t *riak =
        (ib_kvstore_riak_server_t *)kvstore->server;

    return ib_mm_alloc(riak->mm, size);
}

static void mm_free(ib_kvstore_t *kvstore,
                    void *ptr,
                    ib_kvstore_cbdata_t *cbdata)
{

    /* Nop - the memory pool is released by the user. */
}

/**
 * Does a simple get of a Riak object.
 *
 * This will call riak_headers_init and membuffer_init on
 * @a riak_headers and @a response.
 *
 */
static ib_status_t riak_get(
    ib_kvstore_t *kvstore,
    ib_kvstore_riak_server_t *riak,
    const char *url,
    membuffer_t *response,
    riak_headers_t *riak_headers)
{
    assert(riak != NULL);
    assert(riak->curl != NULL);
    assert(kvstore != NULL);
    assert(url != NULL);
    assert(response != NULL);
    assert(riak_headers != NULL);

    CURLcode curl_rc;

    struct curl_slist *header_list = NULL;

    /* Callback data for reading in the body. */
    membuffer_init(kvstore, response);

    /* Callback data for storing the CURL headers. */
    riak_headers_init(kvstore, riak_headers);

    /* Set url. */
    curl_rc = curl_easy_setopt(riak->curl, CURLOPT_URL, url);
    if (curl_rc) {
        return IB_EOTHER;
    }

    /* Use HTTP GET. */
    curl_rc = curl_easy_setopt(riak->curl, CURLOPT_HTTPGET, 1);
    if (curl_rc) {
        return IB_EOTHER;
    }

    curl_rc = curl_easy_setopt(
        riak->curl,
        CURLOPT_WRITEFUNCTION,
        membuffer_writefunction);
    if (curl_rc) {
        return IB_EOTHER;
    }

    curl_rc = curl_easy_setopt(riak->curl, CURLOPT_WRITEDATA, response);
    if (curl_rc) {
        return IB_EOTHER;
    }

    curl_rc = curl_easy_setopt(
        riak->curl,
        CURLOPT_HEADERFUNCTION,
        &riak_header_capture);
    if (curl_rc) {
        return IB_EOTHER;
    }

    curl_rc = curl_easy_setopt(riak->curl, CURLOPT_WRITEHEADER, riak_headers);
    if (curl_rc) {
        return IB_EOTHER;
    }

    header_list = build_custom_headers(kvstore, riak, NULL);
    if (header_list) {
        curl_rc = curl_easy_setopt(
            riak->curl,
            CURLOPT_HTTPHEADER,
            header_list);
        if (curl_rc) {
            return IB_EOTHER;
        }
    }

    /* Perform the transaction. */
    curl_rc = curl_easy_perform(riak->curl);

    if (riak_headers->etag) {
        ib_kvstore_riak_set_etag(kvstore, riak_headers->etag);
    }

    if (riak_headers->x_riak_vclock) {
        ib_kvstore_riak_set_vclock(kvstore, riak_headers->x_riak_vclock);
    }

    /* Free the header list before checking curl_rc. */
    if (header_list) {
        curl_slist_free_all(header_list);
    }

    if (curl_rc) {
        return IB_EOTHER;
    }

    return IB_OK;
}

static ib_status_t kvget(
    ib_kvstore_t             *kvstore,
    ib_mm_t                   mm,
    const ib_kvstore_key_t   *key,
    ib_kvstore_value_t     ***values,
    size_t                   *values_length,
    ib_kvstore_cbdata_t      *cbdata
)
{
    assert(kvstore != NULL);
    assert(key != NULL);
    assert(values != NULL);

    ib_status_t               rc;
    ib_kvstore_riak_server_t *riak;
    char                     *url;
    membuffer_t               response;
    riak_headers_t            riak_headers;

    membuffer_init(kvstore, &response);
    riak_headers_init(kvstore, &riak_headers);
    riak = (ib_kvstore_riak_server_t *)kvstore->server;

    url = build_key_url(kvstore, riak, key);

    rc = riak_get(kvstore, riak, url, &response, &riak_headers);
    if (rc != IB_OK) {
        goto exit;
    }

    if (riak_headers.status == 200) {
        *values_length = 1;

        /* Build 1-element array. */
        *values = ib_mm_alloc(mm, sizeof(**values));
        if (*values == NULL) {
            rc = IB_EALLOC;
            goto exit;
        }

        rc = http_to_kvstore_value(
            kvstore,
            riak,
            &response,
            &riak_headers,
            mm,
            &((*values)[0]));
        goto exit;
    }

    /* Multiple choices. */
    else if (riak_headers.status == 300) {
        /* Current line. */
        char *cur;

        *values_length = 0;

        /* Count the siblings returned in the buffer. */
        for (size_t i = 0;
             i + 1 < response.read && response.buffer[i] != '\0';
             ++i)
        {
            /* Every sibling etag is preceded by a '\n' */
            if (response.buffer[i] == '\n' && isalnum(response.buffer[i+1]))
            {
                ++(*values_length);
            }
        }

        /* Build a *values_length element array. */
        *values = ib_mm_alloc(mm, sizeof(**values) * *values_length);
        if (*values == NULL) {
            rc = IB_EALLOC;
            goto exit;
        }

        /* For each sibling, fetch it to be merged. */
        /* Skip the first line which is always "Siblings:\n". */
        cur = index(response.buffer, '\n') + 1;
        for (size_t i = 0; i < *values_length; ++i) {

            /* URL containing ?vtag=<ETag> from response. */
            char *vtag_url;
            const char *vtag = "?vtag=";
            char *eol = index(cur, '\n');

            *eol = '\0';

            membuffer_t tmp_buf;
            riak_headers_t tmp_headers;

            /* Init and Re-init. */
            membuffer_init(kvstore, &tmp_buf);
            riak_headers_init(kvstore, &tmp_headers);

            vtag_url = ib_mm_alloc(
                mm,
                strlen(url) + strlen(vtag) + strlen(cur) + 1
            );
            if (!vtag_url) {
                rc = IB_EALLOC;
                goto exit;
            }
            sprintf(vtag_url, "%s%s%s", url, vtag, cur);

            rc = riak_get(kvstore, riak, vtag_url, &tmp_buf, &tmp_headers);
            if (rc != IB_OK) {
                /* Nop - just skip this and decrement the results. */
                --(*values_length);
                cleanup_membuffer(&tmp_buf);
                continue;
            }

            /* Convert the retrieved buffer data into a kvstore value. */
            rc = http_to_kvstore_value(
                kvstore,
                riak,
                &tmp_buf,
                &tmp_headers,
                mm,
                &((*values)[i]));
            if (rc != IB_OK) {
                goto exit;
            }

            cleanup_membuffer(&tmp_buf);
            cleanup_riak_headers(&tmp_headers);
            cur = eol+1;
        }
    }
    else if (riak_headers.status == 404) {
        *values_length = 0;
        *values = NULL;
        rc = IB_ENOENT;
        goto exit;
    }

    /* Before cleanly existing, set the riak etag and vclock to that of
     * the representative request, not the individual etag ones. */
    if (riak_headers.etag) {
        ib_kvstore_riak_set_etag(kvstore, riak_headers.etag);
    }

    if (riak_headers.x_riak_vclock) {
        ib_kvstore_riak_set_vclock(kvstore, riak_headers.x_riak_vclock);
    }

exit:
    cleanup_membuffer(&response);
    cleanup_riak_headers(&riak_headers);

    curl_easy_reset(riak->curl);
    kvfree(kvstore, url);
    return rc;
}

static ib_status_t kvset(
    ib_kvstore_t *kvstore,
    ib_kvstore_merge_policy_fn_t merge_policy,
    const ib_kvstore_key_t *key,
    ib_kvstore_value_t *value,
    ib_kvstore_cbdata_t *cbdata)
{
    assert(kvstore != NULL);
    assert(key != NULL);
    assert(value != NULL);

    char *url;
    ib_status_t rc;
    CURLcode curl_rc;
    ib_kvstore_riak_server_t *riak;
    struct curl_slist *header_list = NULL;

    membuffer_t response;
    membuffer_t value_buffer;
    riak_headers_t riak_headers;

    /* Callback data for the value we are setting. */
    membuffer_init(kvstore, &value_buffer);
    ib_kvstore_value_value_get(
        value,
        (const uint8_t **)&(value_buffer.buffer),
        &(value_buffer.size));

    /* Callback data for reading in the body. */
    membuffer_init(kvstore, &response);

    /* Callback data for storing the CURL headers. */
    riak_headers_init(kvstore, &riak_headers);

    riak = (ib_kvstore_riak_server_t *)kvstore->server;
    rc = IB_OK;
    url = build_key_url(kvstore, riak, key);

    /* Set url. */
    curl_rc = curl_easy_setopt(riak->curl, CURLOPT_URL, url);
    if (curl_rc) {
        rc = IB_EOTHER;
        goto exit;
    }

    /* Use PUT action. */
    curl_rc = curl_easy_setopt(riak->curl, CURLOPT_UPLOAD, 1);
    if (curl_rc) {
        rc = IB_EOTHER;
        goto exit;
    }

    curl_rc = curl_easy_setopt(riak->curl, CURLOPT_READDATA, &value_buffer);
    if (curl_rc) {
        rc = IB_EOTHER;
        goto exit;
    }

    curl_rc = curl_easy_setopt(
        riak->curl,
        CURLOPT_INFILESIZE,
        value_buffer.size);
    if (curl_rc) {
        rc = IB_EOTHER;
        goto exit;
    }

    curl_rc = curl_easy_setopt(
        riak->curl,
        CURLOPT_READFUNCTION,
        membuffer_readfunction);
    if (curl_rc) {
        rc = IB_EOTHER;
        goto exit;
    }

    curl_rc = curl_easy_setopt(
        riak->curl,
        CURLOPT_WRITEFUNCTION,
        membuffer_writefunction);
    if (curl_rc) {
        rc = IB_EOTHER;
        goto exit;
    }

    curl_rc = curl_easy_setopt(riak->curl, CURLOPT_WRITEDATA, &response);
    if (curl_rc) {
        rc = IB_EOTHER;
        goto exit;
    }

    curl_rc = curl_easy_setopt(
        riak->curl,
        CURLOPT_HEADERFUNCTION,
        &riak_header_capture);
    if (curl_rc) {
        rc = IB_EOTHER;
        goto exit;
    }

    curl_rc = curl_easy_setopt(riak->curl, CURLOPT_WRITEHEADER, &riak_headers);
    if (curl_rc) {
        rc = IB_EOTHER;
        goto exit;
    }

    header_list = build_custom_headers(kvstore, riak, value);
    if (header_list) {
        curl_rc = curl_easy_setopt(
            riak->curl,
            CURLOPT_HTTPHEADER,
            header_list);
        if (curl_rc) {
            rc = IB_EOTHER;
            goto exit;
        }
    }

    /* Perform the transaction. */
    curl_rc = curl_easy_perform(riak->curl);
    if (curl_rc) {
        rc = IB_EOTHER;
        goto exit;
    }

exit:

    if (header_list) {
        curl_slist_free_all(header_list);
    }

    if (response.buffer) {
        kvfree(kvstore, response.buffer);
    }

    cleanup_riak_headers(&riak_headers);

    curl_easy_reset(riak->curl);
    kvfree(kvstore, url);
    return rc;
}

static ib_status_t kvremove(
    ib_kvstore_t *kvstore,
    const ib_kvstore_key_t *key,
    ib_kvstore_cbdata_t *cbdata)
{

    assert(kvstore != NULL);
    assert(key != NULL);

    char *url;
    ib_status_t rc;
    CURLcode curl_rc;
    ib_kvstore_riak_server_t *riak;

    rc = IB_OK;
    riak = (ib_kvstore_riak_server_t *)kvstore->server;
    url = build_key_url(kvstore, riak, key);

    curl_rc = curl_easy_setopt(riak->curl, CURLOPT_URL, url);
    if (curl_rc) {
        rc =IB_EOTHER;
        goto exit;
    }

    curl_rc = curl_easy_setopt(riak->curl, CURLOPT_CUSTOMREQUEST, "DELETE");
    if (curl_rc) {
        rc =IB_EOTHER;
        goto exit;
    }

    curl_rc = curl_easy_perform(riak->curl);
    if (curl_rc) {
        rc =IB_EOTHER;
        goto exit;
    }

exit:
    curl_easy_reset(riak->curl);
    kvfree(kvstore, url);
    return rc;
}
static ib_status_t kvconnect(
    ib_kvstore_t *kvstore,
    ib_kvstore_cbdata_t *cbdata)
{
    assert(kvstore != NULL);
    assert(kvstore->server != NULL);

    ib_kvstore_riak_server_t *riak =
        (ib_kvstore_riak_server_t *)kvstore->server;
    riak->curl = curl_easy_init();
    if (riak->curl == NULL) {
        return IB_EOTHER;
    }
    return IB_OK;
}
static ib_status_t kvdisconnect(
    ib_kvstore_t *kvstore,
    ib_kvstore_cbdata_t *cbdata)
{
    assert(kvstore != NULL);
    assert(kvstore->server != NULL);

    ib_kvstore_riak_server_t *riak =
        (ib_kvstore_riak_server_t *)kvstore->server;
    curl_easy_cleanup(riak->curl);
    return IB_OK;
}
static void kvdestroy(
    ib_kvstore_t *kvstore,
    ib_kvstore_cbdata_t *cbdata)
{
    assert(kvstore != NULL);

    ib_kvstore_riak_server_t *riak =
        (ib_kvstore_riak_server_t *)kvstore->server;

    kvfree(kvstore, riak->riak_url);
    kvfree(kvstore, riak->bucket_url);
    kvfree(kvstore, riak->bucket);
    if (riak->client_id) {
        kvfree(kvstore, riak->client_id);
    }
    if (riak->etag) {
        kvfree(kvstore, riak->etag);
    }
    if (riak->vclock) {
        kvfree(kvstore, riak->vclock);
    }
    kvfree(kvstore, riak);
}

ib_status_t ib_kvstore_riak_init(
    ib_kvstore_t *kvstore,
    const char *client_id,
    const char *riak_url,
    const char *bucket,
    ib_mm_t mm)
{
    assert(kvstore != NULL);
    assert(riak_url != NULL);
    assert(bucket != NULL);

    ib_status_t rc;
    ib_kvstore_riak_server_t *server;

    rc = ib_kvstore_init(kvstore);
    if (rc != IB_OK) {
        return rc;
    }

    /* If the user gave us a memory manager, use memory pools allocator. */
    if (! ib_mm_is_null(mm)) {
        kvstore->malloc = mm_malloc;
        kvstore->free = mm_free;
    }

    server = kvmalloc(kvstore, sizeof(*server));
    if (!server) {
        return IB_EALLOC;
    }
    server->vclock = NULL;
    server->etag = NULL;
    server->riak_url_len = strlen(riak_url);
    server->bucket_len = strlen(bucket);
    server->mm = mm;

    /* +10 for the intermediate string constant "/buckets/" in the url. */
    server->bucket_url_len = server->riak_url_len + 10 + server->bucket_len;

    server->client_id = kvmalloc(kvstore, strlen(client_id)+1);
    if (server->client_id == NULL) {
        return IB_EALLOC;
    }
    strcpy(server->client_id, client_id);

    server->riak_url = kvmalloc(kvstore, server->riak_url_len + 1);
    if (server->riak_url == NULL) {
        kvfree(kvstore, server->client_id);
        return IB_EALLOC;
    }
    strcpy(server->riak_url, riak_url);

    server->bucket = kvmalloc(kvstore, server->bucket_len + 1);
    if (server->bucket == NULL) {
        kvfree(kvstore, server->client_id);
        kvfree(kvstore, server->riak_url);
        return IB_EALLOC;
    }
    strcpy(server->bucket, bucket);

    server->bucket_url = kvmalloc(
        kvstore,
        server->riak_url_len + server->bucket_len + 10);
    if (server->bucket_url == NULL) {
        kvfree(kvstore, server->client_id);
        kvfree(kvstore, server->riak_url);
        kvfree(kvstore, server->bucket);
        return IB_EALLOC;
    }
    sprintf(
        server->bucket_url,
        "%s/buckets/%s",
        riak_url,
        bucket);

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

    return IB_OK;
}

void ib_kvstore_riak_set_vclock(ib_kvstore_t *kvstore, char *vclock) {
    assert(kvstore != NULL);

    ib_kvstore_riak_server_t *riak;

    riak = (ib_kvstore_riak_server_t *)kvstore->server;

    if (riak->vclock) {
        kvfree(kvstore, riak->vclock);
    }

    if (vclock != NULL) {
        riak->vclock = kvmalloc(kvstore, strlen(vclock)+1);

        if (riak->vclock) {
            strcpy(riak->vclock, vclock);
        }
    }
    else {
        riak->vclock = NULL;
    }
}

void ib_kvstore_riak_set_etag(ib_kvstore_t *kvstore, char *etag) {
    assert(kvstore != NULL);

    ib_kvstore_riak_server_t *riak;

    riak = (ib_kvstore_riak_server_t *)kvstore->server;

    if (riak->etag) {
        kvfree(kvstore, riak->etag);
    }

    if (etag != NULL) {
        riak->etag = kvmalloc(kvstore, strlen(etag)+1);

        if (riak->etag) {
            strcpy(riak->etag, etag);
        }
    }
    else {
        riak->etag = NULL;
    }
}

char * ib_kvstore_riak_get_vclock(ib_kvstore_t *kvstore) {
    assert(kvstore != NULL);
    assert(kvstore->server != NULL);

    char *c = ((ib_kvstore_riak_server_t *)kvstore->server)->vclock;
    return c;
}

char * ib_kvstore_riak_get_etag(ib_kvstore_t *kvstore) {
    assert(kvstore != NULL);
    assert(kvstore->server != NULL);

    char *c = ((ib_kvstore_riak_server_t *)kvstore->server)->etag;
    return c;
}

int ib_kvstore_riak_ping(ib_kvstore_t *kvstore) {
    assert(kvstore != NULL);
    assert(kvstore->server != NULL);

    ib_kvstore_riak_server_t *riak;
    ib_status_t rc;
    membuffer_t resp;
    riak_headers_t headers;
    char *url;
    riak = (ib_kvstore_riak_server_t *)kvstore->server;
    int result;

    url = kvmalloc(kvstore, riak->riak_url_len + 7);
    if (!url) {
        return 0;
    }
    sprintf(url, "%s/ping", riak->riak_url);

    rc = riak_get(kvstore, riak, url, &resp, &headers);
    if (rc != IB_OK) {
        result = 0;
    }
    else {
        result =
            (resp.read == 2 && resp.buffer[0] == 'O' && resp.buffer[1] == 'K');
    }
    kvfree(kvstore, url);
    cleanup_membuffer(&resp);
    cleanup_riak_headers(&headers);

    return result;
}

/**
 * Internal helper function to do the work for
 * ib_kvstore_riak_set_bucket_property_int and
 * ib_kvstore_riak_set_bucket_property_str.
 */
static ib_status_t ib_kvstore_riak_set_bucket_property(
    ib_kvstore_t *kvstore,
    membuffer_t *request)
{
    assert(kvstore != NULL);
    assert(request != NULL);
    assert(request->buffer != NULL);

    /* Constants for this function, alone. */
    const char *props_path = "/props";

    ib_kvstore_riak_server_t *riak;

    CURLcode curl_rc;
    struct curl_slist *header_list = NULL;
    size_t url_length;
    char *url;
    membuffer_t response;
    riak_headers_t headers;
    ib_status_t rc = IB_OK;

    riak_headers_init(kvstore, &headers);

    membuffer_init(kvstore, &response);

    riak = (ib_kvstore_riak_server_t *)kvstore->server;

    url_length = riak->bucket_url_len + strlen(props_path);

    url = kvmalloc(kvstore, url_length + 1);
    if (!url) {
        rc = IB_EALLOC;
        goto exit;
    }

    snprintf(url, url_length+1, "%s%s", riak->bucket_url, props_path);

    /* Set url. */
    curl_rc = curl_easy_setopt(riak->curl, CURLOPT_URL, url);
    if (curl_rc) {
        rc = IB_EOTHER;
        goto exit;
    }

    /* Use PUT action. */
    curl_rc = curl_easy_setopt(riak->curl, CURLOPT_UPLOAD, 1);
    if (curl_rc) {
        rc = IB_EOTHER;
        goto exit;
    }

    /* Set request data. */
    curl_rc = curl_easy_setopt(riak->curl, CURLOPT_READDATA, request);
    if (curl_rc) {
        rc = IB_EOTHER;
        goto exit;
    }

    /* Set request data size. */
    curl_rc = curl_easy_setopt(riak->curl, CURLOPT_INFILESIZE, request->size);
    if (curl_rc) {
        rc = IB_EOTHER;
        goto exit;
    }

    /* Define how to read the request. */
    curl_rc = curl_easy_setopt(
        riak->curl,
        CURLOPT_READFUNCTION,
        membuffer_readfunction);
    if (curl_rc) {
        rc = IB_EOTHER;
        goto exit;
    }

    /* Define how to write the response. */
    curl_rc = curl_easy_setopt(
        riak->curl,
        CURLOPT_WRITEFUNCTION,
        membuffer_writefunction);
    if (curl_rc) {
        rc = IB_EOTHER;
        goto exit;
    }

    /* Set the response buffer. */
    curl_rc = curl_easy_setopt(riak->curl, CURLOPT_WRITEDATA, &response);
    if (curl_rc) {
        rc = IB_EOTHER;
        goto exit;
    }

    /* How are headers captures. */
    curl_rc = curl_easy_setopt(
        riak->curl,
        CURLOPT_HEADERFUNCTION,
        &riak_header_capture);
    if (curl_rc) {
        rc = IB_EOTHER;
        goto exit;
    }

    /* Where are headers captures. */
    curl_rc = curl_easy_setopt(riak->curl, CURLOPT_WRITEHEADER, &headers);
    if (curl_rc) {
        rc = IB_EOTHER;
        goto exit;
    }

    header_list = curl_slist_append(header_list, "Content-Type: application/json");
    if (!header_list) {
        rc = IB_EOTHER;
        goto exit;
    }

    curl_rc = curl_easy_setopt(riak->curl, CURLOPT_HTTPHEADER, header_list);
    if (curl_rc) {
        rc = IB_EOTHER;
        goto exit;
    }

    /* Perform the transaction. */
    curl_rc = curl_easy_perform(riak->curl);
    if (curl_rc) {
        rc = IB_EOTHER;
        goto exit;
    }


exit:

    if (header_list) {
        curl_slist_free_all(header_list);
    }

    cleanup_membuffer(&response);
    cleanup_riak_headers(&headers);

    curl_easy_reset(riak->curl);
    kvfree(kvstore, url);
    return rc;
}

ib_status_t ib_kvstore_riak_set_bucket_property_str(
    ib_kvstore_t *kvstore,
    const char *property,
    const char *value)
{
    assert(kvstore != NULL);
    assert(property != NULL);
    assert(value != NULL);

    const char *post_fmt = "{\"props\":{\"%s\":\"%s\"}}";
    membuffer_t request;
    ib_status_t rc;

    membuffer_init(kvstore, &request);
    request.read = 0;
    request.size =
        strlen(property) +    /* Property length. */
        strlen(value) +       /* Value length. */
        strlen(post_fmt) - 4; /* post_fmt - %s and %s. */

    /* Note: size+1 so we can use sprintf below. */
    request.buffer = kvmalloc(kvstore, request.size+1);
    if (!request.buffer) {
        return IB_EALLOC;
    }
    sprintf(request.buffer, post_fmt, property, value);

    rc = ib_kvstore_riak_set_bucket_property(kvstore, &request);

    cleanup_membuffer(&request);

    return rc;
}

ib_status_t ib_kvstore_riak_set_bucket_property_int(
    ib_kvstore_t *kvstore,
    const char *property,
    int value)
{
    assert(kvstore != NULL);
    assert(property != NULL);

    const char *post_fmt = "{\"props\":{\"%s\":%d}}";
    membuffer_t request;
    const int digits = 6;
    size_t size;
    ib_status_t rc;

    if (value < 0) {
        return IB_EINVAL;
    }

    /* Digits is greater than 6, so we can't represent it in our buffer. */
    if (value > 999999) {
        return IB_EINVAL;
    }

    membuffer_init(kvstore, &request);
    request.read = 0;
    size = strlen(property) + digits + strlen(post_fmt)-4;
    request.buffer = kvmalloc(kvstore, size+1);
    if (!request.buffer) {
        return IB_EALLOC;
    }

    request.size = sprintf(request.buffer, post_fmt, property, value);

    rc = ib_kvstore_riak_set_bucket_property(kvstore, &request);

    cleanup_membuffer(&request);

    return rc;
}
