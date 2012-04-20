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
 *****************************************************************************/

/**
 * @file
 * @brief IronBee &mdash; Interface for handling parsed content.
 *
 * @author Sam Baskinger <sbaskinger@qualys.com>
 */

#include "ironbee_config_auto.h"

/* System includes. */
#include <assert.h>

/* Include engine structs, private content structs, etc. */
#include <ironbee_private.h>

/* Public IronBee includes. */
#include <ironbee/debug.h>
#include <ironbee/parsed_content.h>
#include <ironbee/mpool.h>

ib_status_t ib_parsed_name_value_pair_list_wrapper_create(
    ib_parsed_name_value_pair_list_wrapper_t **headers,
    ib_tx_t *tx)
{
    IB_FTRACE_INIT();

    assert(headers != NULL);
    assert(tx != NULL);

    ib_parsed_name_value_pair_list_wrapper_t *headers_tmp =
        ib_mpool_calloc(tx->mp, 1, sizeof(*headers_tmp));

    if ( headers_tmp == NULL ) {
        *headers = NULL;
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    headers_tmp->mpool = tx->mp;
    /* headers_tmp->head = initialized by calloc */
    /* headers_tmp->tail = initialized by calloc */
    /* headers_tmp->size = initialized by calloc */

    /* Commit back successful object. */
    *headers = headers_tmp;

    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t ib_parsed_name_value_pair_list_add(
    ib_parsed_name_value_pair_list_wrapper_t *headers,
    const char *name,
    size_t name_len,
    const char *value,
    size_t value_len)
{
    IB_FTRACE_INIT();
    ib_status_t rc;

    assert(headers != NULL);
    assert(headers->mpool != NULL);
    assert(name != NULL);
    assert(value != NULL);

    ib_parsed_name_value_pair_list_t *ele;

    ele = ib_mpool_alloc(headers->mpool, sizeof(*ele));

    if (ele == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    rc = ib_bytestr_alias_mem(&ele->name,
                              headers->mpool,
                              (const uint8_t *)name,
                              name_len);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = ib_bytestr_alias_mem(&ele->value,
                              headers->mpool,
                              (const uint8_t *)value,
                              value_len);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    ele->next = NULL;

    /* List is empty. Add first element. */
    if (headers->head == NULL) {
        headers->head = ele;
        headers->tail = ele;
        headers->size = 1;
    }

    /* Normal append to a list with values in it already. */
    else {
        headers->tail->next = ele;
        headers->tail = ele;
        ++(headers->size);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t ib_parsed_tx_each_header(
    ib_parsed_name_value_pair_list_wrapper_t *headers,
    ib_parsed_tx_each_header_callback callback,
    void* user_data)
{
    IB_FTRACE_INIT();

    assert(headers!=NULL);

    ib_status_t rc = IB_OK;

    /* Loop over headers elements until the end of the list is reached or
     * IB_OK is not returned by the callback. */
    for(const ib_parsed_name_value_pair_list_t *le = headers->head;
        le != NULL && rc == IB_OK;
        le = le->next)
    {
        rc = callback((const char *)ib_bytestr_const_ptr(le->name),
                      ib_bytestr_size(le->name),
                      (const char *)ib_bytestr_const_ptr(le->value),
                      ib_bytestr_size(le->value),
                      user_data);
    }

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_parsed_resp_line_create(ib_tx_t *tx,
                                       ib_parsed_resp_line_t **line,
                                       const char *raw,
                                       size_t raw_len,
                                       const char *protocol,
                                       size_t protocol_len,
                                       const char *status,
                                       size_t status_len,
                                       const char *msg,
                                       size_t msg_len)
{
    IB_FTRACE_INIT();
    ib_status_t rc;

    assert(tx != NULL);
    assert(tx->ib != NULL);
    assert(tx->mp != NULL);
    assert(protocol != NULL);
    assert(protocol_len > 0);
    assert(status != NULL);
    assert(status_len > 0);
    assert(msg != NULL);

    ib_parsed_resp_line_t *line_tmp = ib_mpool_alloc(tx->mp,
                                                     sizeof(*line_tmp));

    if (line_tmp == NULL) {
        *line = NULL;
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    rc = ib_bytestr_alias_mem(&line_tmp->status,
                              tx->mp,
                              (const uint8_t *)status,
                              status_len);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = ib_bytestr_alias_mem(&line_tmp->msg,
                              tx->mp,
                              (const uint8_t *)msg,
                              msg_len);

    /* Commit back successfully created line. */
    *line = line_tmp;

    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t ib_parsed_req_line_create(ib_tx_t *tx,
                                      ib_parsed_req_line_t **line,
                                      const char *raw,
                                      size_t raw_len,
                                      const char *method,
                                      size_t method_len,
                                      const char *uri,
                                      size_t uri_len,
                                      const char *protocol,
                                      size_t protocol_len)
{
    IB_FTRACE_INIT();
    ib_status_t rc;

    assert(tx != NULL);
    assert(tx->ib != NULL);
    assert(tx->mp != NULL);
    assert(method != NULL);
    assert(method_len > 0);
    assert(uri != NULL);
    assert(uri_len > 0);

    ib_parsed_req_line_t *line_tmp = ib_mpool_alloc(tx->mp,
                                                    sizeof(*line_tmp));

    if ( line_tmp == NULL ) {
        *line = NULL;
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    rc = ib_bytestr_alias_mem(&line_tmp->method,
                              tx->mp,
                              (const uint8_t *)method,
                              method_len);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = ib_bytestr_alias_mem(&line_tmp->uri,
                              tx->mp,
                              (const uint8_t *)uri,
                              uri_len);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }
    
    /* HTTP/0.9 will have a NULL protocol. */
    if (protocol == NULL) {
        rc = ib_bytestr_dup_mem(&line_tmp->protocol,
                                tx->mp,
                                (const uint8_t *)"HTTP/0.9",
                                protocol_len);
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }
    }
    else {
        rc = ib_bytestr_alias_mem(&line_tmp->protocol,
                                  tx->mp,
                                  (const uint8_t *)protocol,
                                  protocol_len);
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }
    }

    /* If no raw line is available, then create one. */
    if (raw == NULL) {
        uint8_t *ptr;

        /* Create a correctly sized bytestr and manually copy
         * the data into it.
         */
        rc = ib_bytestr_create(&line_tmp->raw,
                               tx->mp,
                               method_len + 1 + uri_len +
                               (protocol == NULL ? 0 : 1 + protocol_len));
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }

        ptr = ib_bytestr_ptr(line_tmp->raw);
        memcpy(ptr, method, method_len);
        ptr += method_len;
        *ptr = ' ';
        ptr += 1;
        memcpy(ptr, uri, uri_len);
        ptr += uri_len;
        if (protocol != NULL) {
            *ptr = ' ';
            ptr += 1;
            memcpy(ptr, protocol, protocol_len);
        }
    }
    else {
        ib_bytestr_alias_mem(&line_tmp->raw,
                             tx->mp,
                             (const uint8_t *)raw,
                             raw_len);
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }
    }

    /* Commit back successfully created line. */
    *line = line_tmp;

    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t ib_parsed_name_value_pair_list_append(
    ib_parsed_name_value_pair_list_wrapper_t *head,
    const ib_parsed_name_value_pair_list_wrapper_t *tail)
{
    IB_FTRACE_INIT();

    assert(head != NULL);
    assert(tail != NULL);

    /* If the head list is empty, it assumes the contents of tail. */
    if ( head->head == NULL ) {
        /* Shallow copy. */
        *head = *tail;
    }

    else {
        head->tail = tail->head;
        head->size += tail->size;
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}
