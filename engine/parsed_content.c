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
 * @brief IronBee --- Interface for handling parsed content.
 *
 * @author Sam Baskinger <sbaskinger@qualys.com>
 */

#include "ironbee_config_auto.h"

#include <ironbee/parsed_content.h>

#include <ironbee/log.h>
#include <ironbee/mpool.h>

#include <assert.h>

ib_status_t ib_parsed_name_value_pair_list_wrapper_create(
    ib_parsed_name_value_pair_list_wrapper_t **headers,
    ib_mpool_t *mp)
{
    assert(headers != NULL);
    assert(mp != NULL);

    ib_parsed_name_value_pair_list_wrapper_t *headers_tmp =
        ib_mpool_calloc(mp, 1, sizeof(*headers_tmp));

    if ( headers_tmp == NULL ) {
        *headers = NULL;
        return IB_EALLOC;
    }

    headers_tmp->mpool = mp;
    /* headers_tmp->head = initialized by calloc */
    /* headers_tmp->tail = initialized by calloc */
    /* headers_tmp->size = initialized by calloc */

    /* Commit back successful object. */
    *headers = headers_tmp;

    return IB_OK;
}

ib_status_t ib_parsed_name_value_pair_list_add(
    ib_parsed_name_value_pair_list_wrapper_t *headers,
    const char *name,
    size_t name_len,
    const char *value,
    size_t value_len)
{
    ib_status_t rc;

    assert(headers != NULL);
    assert(headers->mpool != NULL);
    assert(name != NULL);
    assert(value != NULL);

    ib_parsed_name_value_pair_list_t *ele;

    ele = ib_mpool_alloc(headers->mpool, sizeof(*ele));
    if (ele == NULL) {
        return IB_EALLOC;
    }

    rc = ib_bytestr_dup_mem(&ele->name,
                            headers->mpool,
                            (const uint8_t *)name,
                            name_len);
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_bytestr_dup_mem(&ele->value,
                            headers->mpool,
                            (const uint8_t *)value,
                            value_len);
    if (rc != IB_OK) {
        return rc;
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

    return IB_OK;
}

ib_status_t ib_parsed_tx_each_header(
    ib_parsed_name_value_pair_list_wrapper_t *headers,
    ib_parsed_tx_each_header_callback callback,
    void* user_data)
{
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

    return rc;
}

ib_status_t ib_parsed_resp_line_create(
    ib_parsed_resp_line_t **line,
    ib_mpool_t             *mp,
    const char             *raw,
    size_t                  raw_len,
    const char             *protocol,
    size_t                  protocol_len,
    const char             *status,
    size_t                  status_len,
    const char             *msg,
    size_t                  msg_len
)
{
    ib_status_t rc = IB_OK;

    assert(mp != NULL);

    ib_parsed_resp_line_t *line_tmp = ib_mpool_alloc(mp,
                                                     sizeof(*line_tmp));

    if (line_tmp == NULL) {
        *line = NULL;
        return IB_EALLOC;
    }

    if (protocol != NULL) {
        rc = ib_bytestr_dup_mem(&line_tmp->protocol,
                                mp,
                                (const uint8_t *)protocol,
                                protocol_len);
        if (rc != IB_OK) {
            return rc;
        }
    }
    else {
        rc = ib_bytestr_dup_mem(&line_tmp->protocol,
                                mp,
                                (const uint8_t *)"",
                                0);
        if (rc != IB_OK) {
            return rc;
        }
    }


    if (status != NULL) {
        rc = ib_bytestr_dup_mem(&line_tmp->status,
                                mp,
                                (const uint8_t *)status,
                                status_len);
        if (rc != IB_OK) {
            return rc;
        }
    }
    else {
        rc = ib_bytestr_dup_mem(&line_tmp->status,
                                mp,
                                (const uint8_t *)"",
                                0);
        if (rc != IB_OK) {
            return rc;
        }
    }

    if (msg != NULL) {
        rc = ib_bytestr_dup_mem(&line_tmp->msg,
                                mp,
                                (const uint8_t *)msg,
                                msg_len);
        if (rc != IB_OK) {
            return rc;
        }
    }
    else {
        rc = ib_bytestr_dup_mem(&line_tmp->msg,
                                mp,
                                (const uint8_t *)"",
                                0);
        if (rc != IB_OK) {
            return rc;
        }
    }

    /* If no raw line is available, then create one. */
    if (raw == NULL) {
        assert(protocol != NULL);
        assert(status != NULL);

        if (protocol_len + status_len + msg_len == 0) {
            rc = ib_bytestr_dup_mem(&line_tmp->raw,
                                    mp,
                                    (const uint8_t *)"",
                                    0);
            if (rc != IB_OK) {
                return rc;
            }
        }
        else {
            /* Create a correctly sized bytestr and manually copy
             * the data into it.
             */
            rc = ib_bytestr_create(&line_tmp->raw,
                                   mp,
                                   protocol_len + 1 + status_len +
                                   (msg == NULL ? 0 : 1 + msg_len));
            if (rc != IB_OK) {
                return rc;
            }

            ib_bytestr_append_mem(line_tmp->raw,
                                  (const uint8_t *)protocol,
                                  protocol_len);
            ib_bytestr_append_mem(line_tmp->raw,
                                  (const uint8_t *)" ",
                                  1);
            ib_bytestr_append_mem(line_tmp->raw,
                                  (const uint8_t *)status,
                                  status_len);
            if (msg != NULL) {
                ib_bytestr_append_mem(line_tmp->raw,
                                      (const uint8_t *)" ",
                                      1);
                ib_bytestr_append_mem(line_tmp->raw,
                                      (const uint8_t *)msg,
                                      msg_len);
            }
        }
    }
    else {
        rc = ib_bytestr_dup_mem(&line_tmp->raw,
                                mp,
                                (const uint8_t *)raw,
                                raw_len);
        if (rc != IB_OK) {
            return rc;
        }
    }

    /* Commit back successfully created line. */
    *line = line_tmp;

    return IB_OK;
}

ib_status_t ib_parsed_req_line_create(
    ib_parsed_req_line_t **line,
    ib_mpool_t            *mp,
    const char            *raw,
    size_t                 raw_len,
    const char            *method,
    size_t                 method_len,
    const char            *uri,
    size_t                 uri_len,
    const char            *protocol,
    size_t                 protocol_len
)
{
    ib_status_t rc = IB_OK;

    assert(mp != NULL);

    ib_parsed_req_line_t *line_tmp = ib_mpool_alloc(mp,
                                                    sizeof(*line_tmp));

    if ( line_tmp == NULL ) {
        *line = NULL;
        return IB_EALLOC;
    }

    /* Record the components if available. If the components are
     * not available, but the raw line is, then it will be possible
     * to parse the components out later on.  Otherwise, if there
     * is no component and no raw line, then set default values.
     */
    if (method != NULL) {
        rc = ib_bytestr_dup_mem(&line_tmp->method,
                                mp,
                                (const uint8_t *)method,
                                method_len);
        if (rc != IB_OK) {
            return rc;
        }
    }
    else {
        rc = ib_bytestr_dup_mem(&line_tmp->method,
                                mp,
                                (const uint8_t *)"",
                                0);
        if (rc != IB_OK) {
            return rc;
        }
    }

    if (uri != NULL) {
        rc = ib_bytestr_dup_mem(&line_tmp->uri,
                                mp,
                                (const uint8_t *)uri,
                                uri_len);
        if (rc != IB_OK) {
            return rc;
        }
    }
    else {
        rc = ib_bytestr_dup_mem(&line_tmp->uri,
                                mp,
                                (const uint8_t *)"",
                                0);
        if (rc != IB_OK) {
            return rc;
        }
    }

    if (protocol != NULL) {
        rc = ib_bytestr_dup_mem(&line_tmp->protocol,
                                mp,
                                (const uint8_t *)protocol,
                                protocol_len);
        if (rc != IB_OK) {
            return rc;
        }
    }
    else {
        rc = ib_bytestr_dup_mem(&line_tmp->protocol,
                                mp,
                                (const uint8_t *)"",
                                0);
        if (rc != IB_OK) {
            return rc;
        }
    }

    /* If no raw line is available, then create one. */
    if (raw == NULL) {
        if (method_len + uri_len + protocol_len == 0) {
            rc = ib_bytestr_dup_mem(&line_tmp->raw,
                                    mp,
                                    (const uint8_t *)"",
                                    0);
            if (rc != IB_OK) {
                return rc;
            }
        }
        else {
            size_t raw_line_len;

            assert(method != NULL);
            assert(uri != NULL);

            /* Create a correctly sized bytestr and manually copy
             * the data into it.
             */
            raw_line_len = method_len + 1 + uri_len +
                           (protocol == NULL ? 0 : 1 + protocol_len);
            rc = ib_bytestr_create(&line_tmp->raw, mp, raw_line_len);
            if (rc != IB_OK) {
                return rc;
            }

            ib_bytestr_append_mem(line_tmp->raw,
                                  (const uint8_t *)method,
                                  method_len);
            ib_bytestr_append_mem(line_tmp->raw,
                                  (const uint8_t *)" ",
                                  1);
            ib_bytestr_append_mem(line_tmp->raw,
                                  (const uint8_t *)uri,
                                  uri_len);
            if (protocol != NULL) {
                ib_bytestr_append_mem(line_tmp->raw,
                                      (const uint8_t *)" ",
                                      1);
                ib_bytestr_append_mem(line_tmp->raw,
                                      (const uint8_t *)protocol,
                                      protocol_len);
            }
        }
    }
    else {
        rc = ib_bytestr_dup_mem(&line_tmp->raw,
                                mp,
                                (const uint8_t *)raw,
                                raw_len);
        if (rc != IB_OK) {
            return rc;
        }
    }

    /* Commit back successfully created line. */
    *line = line_tmp;

    return IB_OK;
}

ib_status_t ib_parsed_name_value_pair_list_append(
    ib_parsed_name_value_pair_list_wrapper_t *head,
    const ib_parsed_name_value_pair_list_wrapper_t *tail)
{
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

    return IB_OK;
}
