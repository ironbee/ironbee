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

#include "ironbee_config_auto.h"

#include <ironbee/kvstore_riak.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

#define VCLOCK "X-Riak-Vclock"
#define ETAG "ETag"

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
        membuffer->kvstore->free(
            membuffer->kvstore,
            membuffer->buffer,
            membuffer->kvstore->free_cbdata);
    }
}

/**
 * Riak headers.
 */
struct riak_headers_t {
    ib_kvstore_t *kvstore; /**< KV store. */
    int status;            /**< HTTP Status. */
    char *x_riak_vclock;   /**< X-Riak-Vclock */
    char *content_type;    /**< Content-Type */
    char *etag;            /**< ETag. */
};
typedef struct riak_headers_t riak_headers_t;

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

    if (riak_headers->content_type) {
        riak_headers->kvstore->free(
            riak_headers->kvstore,
            riak_headers->content_type,
            riak_headers->kvstore->free_cbdata);
    }
    if (riak_headers->etag) {
        riak_headers->kvstore->free(
            riak_headers->kvstore,
            riak_headers->etag,
            riak_headers->kvstore->free_cbdata);
    }
    if (riak_headers->x_riak_vclock) {
        riak_headers->kvstore->free(
            riak_headers->kvstore,
            riak_headers->x_riak_vclock,
            riak_headers->kvstore->free_cbdata);
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
 * @param[out] value The value to be populated.
 *
 * @returns
 *   - IB_OK success.
 *   - IB_EALLOC memory allocation.
 */
static ib_status_t http_to_kvstore_value(
    ib_kvstore_t *kvstore,
    ib_kvstore_riak_server_t *riak,
    membuffer_t *response,
    riak_headers_t *headers,
    ib_kvstore_value_t *value)
{
    assert(kvstore);
    assert(riak);
    assert(response);
    assert(headers);
    assert(value);

    /* Copy value. */
    value->value = kvstore->malloc(
        kvstore,
        response->read,
        kvstore->malloc_cbdata);
    if (value->value == NULL) {
        return IB_EALLOC;
    }
    memcpy(value->value, response->buffer, response->read);
    value->value_length = response->read;

    /* Copy Content-Type */
    value->type = kvstore->malloc(
        kvstore,
        strlen(headers->content_type) + 1,
        kvstore->malloc_cbdata);
    if (value->type == NULL) {
        kvstore->free(
            kvstore,
            value->value,
            kvstore->free_cbdata);
        return IB_EALLOC;
    }
    strcpy(value->type, headers->content_type);

    /* Copy Expiration */
    // TODO - sam

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
    size_t value_len;

    if (ptr_len <= header_len) {
        return IB_EINVAL;
    }

    if (strncmp(ptr, header, header_len)) {
        return IB_EINVAL;
    }

    value_len = ptr_len - header_len;

    *dest = kvstore->malloc(
        kvstore,
        value_len + 1,
        kvstore->malloc_cbdata);
    if (dest == NULL) {
        return IB_EALLOC;
    }

    memcpy(*dest, ptr, value_len);
    dest[value_len] = '\0';

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

    assert(kvstore);
    assert(riak);

    const size_t buffer_len = 1024;
    char *header = malloc(buffer_len);
    struct curl_slist *slist = NULL;

    if (!header) {
        return NULL;
    }

    if (riak->vclock) {
        snprintf(header, buffer_len, VCLOCK ": %s", riak->vclock);
        slist = curl_slist_append(slist, header);
    }

    if (riak->etag) {
        snprintf(header, buffer_len, ETAG ": %s", riak->etag);
        slist = curl_slist_append(slist, header);
    }

    if (value && value->type) {
        snprintf(header, buffer_len, "Content-Type: %s", value->type);
        slist = curl_slist_append(slist, header);
    }

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

    char *cptr = ptr;
    ib_status_t rc;
    size_t total = size * nmemb;
    riak_headers_t *riak_headers = (riak_headers_t *)userdata;

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
        "Content-Type: ",
        ptr,
        size * nmemb,
        &riak_headers->etag);
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

    membuffer_t *mb = (membuffer_t *)userdata;
    ib_kvstore_t *kvstore = mb->kvstore;

    /* Resize mb. */
    if (size * nmemb > mb->size - mb->read) {
        size_t new_size = size * nmemb + mb->size + 4096;

        char *buffer_tmp = kvstore->malloc(
            kvstore,
            new_size,
            kvstore->malloc_cbdata);
        if (!buffer_tmp) {
            return 0;
        }

        /* Avoid situations where buffer is null. */
        if (mb->buffer) {
            if (mb->read > 0) {
                memcpy(buffer_tmp, mb->buffer, mb->read);
            }

            kvstore->free(kvstore, mb->buffer, kvstore->free_cbdata);
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

    assert(ptr);
    assert(userdata);

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

    assert(kvstore);
    assert(riak);
    assert(key);

    char *url;
    size_t url_len;

    /* bucket + /keys/ + key */
    url_len = riak->bucket_url_len + 6 + key->length;
    url = kvstore->malloc(
        kvstore,
        url_len + 1,
        kvstore->malloc_cbdata);
    if (!url) {
        return NULL;
    }

    snprintf(url, url_len, "%s/keys/%s", riak->bucket_url, (char *)key->key);

    return url;
}

static void * mp_malloc(ib_kvstore_t *kvstore,
                        size_t size,
                        ib_kvstore_cbdata_t *cbdata)
{

    assert(kvstore);

    ib_kvstore_riak_server_t *riak =
        (ib_kvstore_riak_server_t *)kvstore->server;

    return ib_mpool_alloc(riak->mp, size);
}

static void mp_free(ib_kvstore_t *kvstore,
                    void *ptr,
                    ib_kvstore_cbdata_t *cbdata)
{

    /* Nop - the memory pool is released by the user. */
}

/**
 * Does a simple get of a Riak object.
 */
static ib_status_t riak_get(
    ib_kvstore_t *kvstore,
    ib_kvstore_riak_server_t *riak,
    const char *url,
    membuffer_t *resp_buffer,
    riak_headers_t *riak_headers)
{

    CURLcode curl_rc;

    struct curl_slist *header_list = NULL;

    /* Callback data for reading in the body. */
    resp_buffer->kvstore = kvstore;
    resp_buffer->read = 0;
    resp_buffer->size = 0;
    resp_buffer->buffer = NULL;

    /* Callback data for storing the CURL headers. */
    riak_headers->kvstore = kvstore;
    riak_headers->status = 0;
    riak_headers->x_riak_vclock = NULL;
    riak_headers->content_type = NULL;
    riak_headers->etag = NULL;

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

    curl_rc = curl_easy_setopt(riak->curl, CURLOPT_WRITEDATA, resp_buffer);
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
    ib_kvstore_t *kvstore,
    const ib_kvstore_key_t *key,
    ib_kvstore_value_t ***values,
    size_t *values_length,
    ib_kvstore_cbdata_t *cbdata)
{
    ib_status_t rc;
    ib_kvstore_riak_server_t *riak;
    char *url;
    membuffer_t resp_buffer;
    riak_headers_t riak_headers;

    riak = (ib_kvstore_riak_server_t *)kvstore->server;

    url = build_key_url(kvstore, riak, key);

    rc = riak_get(kvstore, riak, url, &resp_buffer, &riak_headers);
    if (rc != IB_OK) {
        goto exit;
    }

    if (riak_headers.status == 200) {
        *values_length = 1;

        /* Build 1-element array. */
        *values = kvstore->malloc(
            kvstore,
            sizeof(**values),
            kvstore->malloc_cbdata
        );
        if (*values == NULL) {
            rc = IB_EALLOC;
            goto exit;
        }

        /* Allocate kvstore value. */
        *values[0] = kvstore->malloc(
            kvstore,
            sizeof(*(*values[0])),
            kvstore->malloc_cbdata);
        if (*values[0] == NULL) {
            rc = IB_EALLOC;
            goto exit;
        }

        rc = http_to_kvstore_value(
            kvstore,
            riak,
            &resp_buffer,
            &riak_headers,
            *values[0]);
        goto exit;
    }

    /* Multiple choices. */
    else if (riak_headers.status == 300) {
        *values_length = 0;
        char *cur;

        /* Count the siblings returned in the buffer. */
        for (size_t i = 0; resp_buffer.buffer[i] != '\0'; ++i) {
            /* Every sibling etag is preceded by a '\n' */
            if (resp_buffer.buffer[i] == '\n' &&
                i + 1 < resp_buffer.read &&
                isalpha(resp_buffer.buffer[i+1]))
            {
                ++(*values_length);
            }
        }

        /* Build a *values_length element array. */
        *values = kvstore->malloc(
            kvstore,
            sizeof(**values) * *values_length,
            kvstore->malloc_cbdata
        );
        if (*values == NULL) {
            rc = IB_EALLOC;
            goto exit;
        }

        /* Count the siblings returned in the buffer. */
        cur = resp_buffer.buffer;
        for (size_t i = 0; i < *values_length; ++i) {
            membuffer_t tmp_buf;

            tmp_buf.kvstore = kvstore;
            tmp_buf.read = 0;
            tmp_buf.size = 0;
            tmp_buf.buffer = NULL;

            /* Find the first character after the \n. */
            cur = index(cur, '\n') + 1;

            /* Turn cur into an end-of-line string. */
            *index(cur, '\r') = '\0';

            /* Set the ETag to allow for a specific get. */
            riak->etag = cur;

            rc = riak_get(kvstore, riak, url, &tmp_buf, &riak_headers);
            if (rc != IB_OK) {
                /* Nop - just skip this and decrement the results. */
                --(*values_length);
                cleanup_membuffer(&tmp_buf);
            }

            *values[i] = kvstore->malloc(
                kvstore,
                sizeof(**values[i]),
                kvstore->malloc_cbdata);
            if (*values[i] == NULL) {
                /* On failure, free allocated keys. */
                for(size_t j = 0; j < i; j++) {
                    ib_kvstore_free_value(kvstore, *values[i]);
                }
                rc = IB_EALLOC;
                goto exit;
            }

            /* Convert the retrieved buffer data into a kvstore value. */
            rc = http_to_kvstore_value(
                kvstore,
                riak,
                &tmp_buf,
                &riak_headers,
                *values[i]);

            cleanup_membuffer(&tmp_buf);
        }
    }

exit:

    cleanup_membuffer(&resp_buffer);
    cleanup_riak_headers(&riak_headers);

    curl_easy_reset(riak->curl);
    kvstore->free(kvstore, url, kvstore->free_cbdata);
    return rc;
}

static ib_status_t kvset(
    ib_kvstore_t *kvstore,
    ib_kvstore_merge_policy_fn_t merge_policy,
    const ib_kvstore_key_t *key,
    ib_kvstore_value_t *value,
    ib_kvstore_cbdata_t *cbdata)
{

    char *url;
    ib_status_t rc;
    CURLcode curl_rc;
    ib_kvstore_riak_server_t *riak;
    struct curl_slist *header_list = NULL;

    membuffer_t resp_buffer;
    membuffer_t value_buffer;
    riak_headers_t riak_headers;

    /* Callback data for the value we are setting. */
    value_buffer.read = 0;
    value_buffer.size = value->value_length;
    value_buffer.buffer = value->value;
    value_buffer.kvstore = kvstore;

    /* Callback data for reading in the body. */
    resp_buffer.read = 0;
    resp_buffer.size = 0;
    resp_buffer.buffer = NULL;
    resp_buffer.kvstore = kvstore;

    /* Callback data for storing the CURL headers. */
    riak_headers.kvstore = kvstore;
    riak_headers.status = 0;
    riak_headers.x_riak_vclock = NULL;
    riak_headers.content_type = NULL;
    riak_headers.etag = NULL;

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

    curl_rc = curl_easy_setopt(riak->curl, CURLOPT_WRITEDATA, &resp_buffer);
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

    /* Multiple choices. */
    if (riak_headers.status == 300) {
        // TODO - merge and write values here.
    }

exit:

    if (header_list) {
        curl_slist_free_all(header_list);
    }

    if (resp_buffer.buffer) {
        kvstore->free(kvstore, resp_buffer.buffer, kvstore->free_cbdata);
    }

    cleanup_riak_headers(&riak_headers);

    curl_easy_reset(riak->curl);
    kvstore->free(kvstore, url, kvstore->free_cbdata);
    return rc;
}

static ib_status_t kvremove(
    ib_kvstore_t *kvstore,
    const ib_kvstore_key_t *key,
    ib_kvstore_cbdata_t *cbdata)
{

    assert(kvstore);
    assert(key);

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
    kvstore->free(kvstore, url, kvstore->free_cbdata);
    return rc;
}
static ib_status_t kvconnect(
    ib_kvstore_t *kvstore,
    ib_kvstore_cbdata_t *cbdata)
{
    assert(kvstore);
    assert(kvstore->server);
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
    assert(kvstore);
    assert(kvstore->server);
    ib_kvstore_riak_server_t *riak =
        (ib_kvstore_riak_server_t *)kvstore->server;
    curl_easy_cleanup(riak->curl);
    return IB_OK;
}
static void kvdestroy(
    ib_kvstore_t *kvstore,
    ib_kvstore_cbdata_t *cbdata)
{
    ib_kvstore_riak_server_t *riak =
        (ib_kvstore_riak_server_t *)kvstore->server;

    kvstore->free(kvstore, riak->riak_url, kvstore->free_cbdata);
    kvstore->free(kvstore, riak->bucket_url, kvstore->free_cbdata);
    kvstore->free(kvstore, riak->bucket, kvstore->free_cbdata);
}

ib_status_t ib_kvstore_riak_init(
    ib_kvstore_t *kvstore,
    const char *riak_url,
    const char *bucket,
    ib_mpool_t *mp)
{

    assert(kvstore);
    assert(riak_url);
    assert(bucket);

    ib_status_t rc;
    ib_kvstore_riak_server_t *server;

    rc = ib_kvstore_init(kvstore);
    if (rc != IB_OK) {
        return rc;
    }

    /* If the user gave us a memory pool, use memory pools allocator. */
    if (mp) {
        kvstore->malloc = mp_malloc;
        kvstore->free = mp_free;
    }

    server = kvstore->malloc(
        kvstore,
        sizeof(*server),
        kvstore->malloc_cbdata);
    if (!server) {
        return IB_EALLOC;
    }
    server->vclock = NULL;
    server->etag = NULL;
    server->riak_url_len = strlen(riak_url);
    server->bucket_len = strlen(bucket);

    /* +10 for the intermediate string constant "/buckets/" in the url. */
    server->bucket_url_len = server->riak_url_len + 10 + server->bucket_len;

    server->riak_url = kvstore->malloc(
       kvstore,
       server->riak_url_len + 1,
       kvstore->malloc_cbdata);
    if (server->riak_url == NULL) {
        return IB_EALLOC;
    }
    sprintf(server->riak_url, "%s", riak_url);

    server->bucket = kvstore->malloc(
       kvstore,
       server->bucket_len + 1,
       kvstore->malloc_cbdata);
    if (server->bucket == NULL) {
        kvstore->free(kvstore, server->riak_url, kvstore->free_cbdata);
        return IB_EALLOC;
    }
    sprintf(server->bucket, "%s", bucket);

    server->bucket_url = kvstore->malloc(
       kvstore,
       server->riak_url_len + server->bucket_len + 10,
       kvstore->malloc_cbdata);
    if (server->bucket_url == NULL) {
        kvstore->free(kvstore, server->riak_url, kvstore->free_cbdata);
        kvstore->free(kvstore, server->bucket, kvstore->free_cbdata);
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

void ib_kvstore_riak_set_vlcock(ib_kvstore_t *kvstore, const char *vclock) {
    ((ib_kvstore_riak_server_t *)kvstore->server)->vclock = vclock;
}

void ib_kvstore_riak_set_etag(ib_kvstore_t *kvstore, const char *etag) {
    ((ib_kvstore_riak_server_t *)kvstore->server)->etag = etag;
}

const char * ib_kvstore_riak_get_vlcock(ib_kvstore_t *kvstore) {
    const char *c = ((ib_kvstore_riak_server_t *)kvstore->server)->vclock;
    return c;
}

const char * ib_kvstore_riak_get_etag(ib_kvstore_t *kvstore) {
    const char *c = ((ib_kvstore_riak_server_t *)kvstore->server)->etag;
    return c;
}

int ib_kvstore_riak_ping(ib_kvstore_t *kvstore) {

    assert(kvstore);

    ib_kvstore_riak_server_t *riak;
    ib_status_t rc;
    membuffer_t resp;
    riak_headers_t headers;
    char *url;
    riak = (ib_kvstore_riak_server_t *)kvstore->server;
    int result;

    url = kvstore->malloc(
        kvstore,
        riak->riak_url_len + 7,
        kvstore->malloc_cbdata);
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
    kvstore->free(kvstore, url, kvstore->free_cbdata);
    cleanup_membuffer(&resp);
    cleanup_riak_headers(&headers);

    return result;
}
