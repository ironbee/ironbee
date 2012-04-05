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
 * @brief IronBee interface for handling parsed content.
 *
 * @author Sam Baskinger <sbaskinger@qualys.com>
 */

#include "ironbee_config_auto.h"

/* System includes. */
#include <assert.h>

/* Include engine structs, private content structs, etc. */
#include <ironbee_private.h>
#include <ironbee_parsed_content_private.h>

/* Public IronBee includes. */
#include <ironbee/debug.h>
#include <ironbee/parsed_content.h>
#include <ironbee/mpool.h>

DLL_PUBLIC ib_status_t ib_parsed_tx_notify_req_begin(
    ib_tx_t *transaction,
    ib_parsed_req_line_t *req_line)
{
    IB_FTRACE_INIT();
    assert(transaction != NULL);
    assert(transaction->ib != NULL);
    assert(transaction->mp != NULL);
    assert(req_line != NULL);

    IB_FTRACE_RET_STATUS(IB_ENOTIMPL); /* @todo - implement. */
}

DLL_PUBLIC ib_status_t ib_parsed_tx_notify_req_header(
    ib_tx_t *transaction,
    ib_parsed_header_t *headers)
{
    IB_FTRACE_INIT();
    assert(transaction != NULL);
    assert(transaction->ib != NULL);
    assert(transaction->mp != NULL);
    assert(headers != NULL);
    IB_FTRACE_RET_STATUS(IB_ENOTIMPL); /* @todo - implement. */
}

DLL_PUBLIC ib_status_t ib_parsed_tx_notify_resp_header(
    ib_tx_t *transaction,
    ib_parsed_header_t *headers)
{
    IB_FTRACE_INIT();
    assert(transaction != NULL);
    assert(transaction->ib != NULL);
    assert(transaction->mp != NULL);
    assert(headers != NULL);
    IB_FTRACE_RET_STATUS(IB_ENOTIMPL); /* @todo - implement. */
}

DLL_PUBLIC ib_status_t ib_parsed_tx_notify_req_end(
    ib_tx_t *transaction)
{
    IB_FTRACE_INIT();
    assert(transaction != NULL);
    assert(transaction->ib != NULL);
    assert(transaction->mp != NULL);
    IB_FTRACE_RET_STATUS(IB_ENOTIMPL); /* @todo - implement. */
}

DLL_PUBLIC ib_status_t ib_parsed_tx_notify_resp_begin(
    ib_tx_t *transaction,
    ib_parsed_resp_line_t *line)
{
    IB_FTRACE_INIT();
    assert(transaction != NULL);
    assert(transaction->ib != NULL);
    assert(transaction->mp != NULL);
    assert(line != NULL);
    IB_FTRACE_RET_STATUS(IB_ENOTIMPL); /* @todo - implement. */
}

DLL_PUBLIC ib_status_t ib_parsed_tx_notify_resp_body(
    ib_tx_t *transaction,
    ib_parsed_data_t *data)
{
    IB_FTRACE_INIT();
    assert(transaction != NULL);
    assert(transaction->ib != NULL);
    assert(transaction->mp != NULL);
    assert(data != NULL);
    IB_FTRACE_RET_STATUS(IB_ENOTIMPL); /* @todo - implement. */
}

DLL_PUBLIC ib_status_t ib_parsed_tx_notify_req_body(
    ib_tx_t *transaction,
    ib_parsed_data_t *data)
{
    IB_FTRACE_INIT();
    assert(transaction != NULL);
    assert(transaction->ib != NULL);
    assert(transaction->mp != NULL);
    assert(data != NULL);
    IB_FTRACE_RET_STATUS(IB_ENOTIMPL); /* @todo - implement. */
}

DLL_PUBLIC ib_status_t ib_parsed_tx_notify_resp_end(
    ib_tx_t *transaction)
{
    IB_FTRACE_INIT();
    assert(transaction != NULL);
    assert(transaction->ib != NULL);
    assert(transaction->mp != NULL);
    IB_FTRACE_RET_STATUS(IB_ENOTIMPL); /* @todo - implement. */
}

DLL_PUBLIC ib_status_t ib_parsed_tx_notify_req_trailer(
    ib_tx_t *transaction,
    ib_parsed_trailer_t *trailers)
{
    IB_FTRACE_INIT();
    assert(transaction != NULL);
    assert(transaction->ib != NULL);
    assert(transaction->mp != NULL);
    assert(trailers != NULL);
    IB_FTRACE_RET_STATUS(IB_ENOTIMPL); /* @todo - implement. */
}

DLL_PUBLIC ib_status_t ib_parsed_tx_notify_resp_trailer(
    ib_tx_t *transaction,
    ib_parsed_trailer_t *trailers)
{
    IB_FTRACE_INIT();
    assert(transaction != NULL);
    assert(transaction->ib != NULL);
    assert(transaction->mp != NULL);
    assert(trailers != NULL);
    IB_FTRACE_RET_STATUS(IB_ENOTIMPL); /* @todo - implement. */
}

DLL_PUBLIC ib_status_t ib_parsed_header_create(ib_parsed_header_t **headers,
                                               ib_mpool_t *mp)
{
    IB_FTRACE_INIT();

    assert(headers != NULL);
    assert(mp != NULL);

    ib_parsed_header_t *headers_tmp =
        ib_mpool_calloc(mp, 1, sizeof(*headers_tmp));

    if ( headers_tmp == NULL ) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    headers_tmp->mp = mp;
    /* headers_tmp->head = initialized by calloc */
    /* headers_tmp->tail = initialized by calloc */
    /* headers_tmp->size = initialized by calloc */

    /* Commit back successful object. */
    *headers = headers_tmp;

    IB_FTRACE_RET_STATUS(IB_OK);
}

DLL_PUBLIC ib_status_t ib_parsed_header_add(ib_parsed_header_t *headers,
                                            const char *name,
                                            size_t name_len,
                                            const char *value,
                                            size_t value_len)
{
    IB_FTRACE_INIT();

    assert(headers != NULL);
    assert(headers->mp != NULL);
    assert(name != NULL);
    assert(value != NULL);

    ib_parsed_name_value_pair_list_element_t *ele;

    ele = ib_mpool_alloc(headers->mp, sizeof(*ele));

    if ( ele == NULL ) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    ele->name = name;
    ele->name_len = name_len;
    ele->value = value;
    ele->value_len = value_len;
    ele->next = NULL;

    /* List is empty. Add first element. */
    if ( headers->head == NULL ) {
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

DLL_PUBLIC size_t ib_parsed_header_list_size(const ib_parsed_header_t *headers)
{
    IB_FTRACE_INIT();
    assert(headers != NULL);
    IB_FTRACE_RET_SIZET(headers->size);
}

DLL_PUBLIC ib_status_t ib_parsed_tx_each_header(
    ib_parsed_header_t *headers,
    ib_parsed_tx_each_header_callback callback,
    void* user_data)
{
    IB_FTRACE_INIT();

    assert(headers!=NULL);

    ib_status_t rc = IB_OK;

    /* Loop over headers elements until the end of the list is reached or
     * IB_OK is not returned by the callback. */
    for( const ib_parsed_name_value_pair_list_element_t *le = headers->head;
         le != NULL && rc == IB_OK;
         le = le->next)
    {
        rc = callback(le->name,
                      le->name_len,
                      le->value,
                      le->value_len,
                      user_data);
    }

    IB_FTRACE_RET_STATUS(rc);
}

DLL_PUBLIC ib_status_t ib_parsed_data_create(ib_tx_t *tx,
                                             ib_parsed_data_t **data,
                                             const char *buffer,
                                             size_t start,
                                             size_t offset)
{
    IB_FTRACE_INIT();

    assert(tx != NULL);
    assert(tx->ib != NULL);
    assert(tx->mp != NULL);
    assert(data != NULL);
    assert(buffer != NULL);

    ib_parsed_data_t *data_tmp;

    data_tmp = ib_mpool_alloc(tx->mp, sizeof(*data_tmp));

    if ( data_tmp == NULL ) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    data_tmp->buffer = buffer;
    data_tmp->start = start;
    data_tmp->offset = offset;

    /* Commit the successful result. */
    *data = data_tmp;

    IB_FTRACE_RET_STATUS(IB_OK);
}

DLL_PUBLIC ib_status_t ib_parsed_resp_line_create(ib_tx_t *tx,
                                                  ib_parsed_resp_line_t **line,
                                                  const char *code,
                                                  size_t code_len,
                                                  const char *msg,
                                                  size_t msg_len)
{
    IB_FTRACE_INIT();
    assert(tx != NULL);
    assert(tx->ib != NULL);
    assert(tx->mp != NULL);
    assert(code != NULL);
    assert(code_len > 0);
    assert(msg != NULL);
    assert(msg_len > 0);

    ib_parsed_resp_line_t *line_tmp = ib_mpool_alloc(tx->mp,
                                                     sizeof(*line_tmp));

    if ( line_tmp == NULL ) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    line_tmp->code = code;
    line_tmp->code_len = code_len;
    line_tmp->msg = msg;
    line_tmp->msg_len = msg_len;

    /* Commit back successfully created line. */
    *line = line_tmp;

    IB_FTRACE_RET_STATUS(IB_OK);
}

DLL_PUBLIC ib_status_t ib_parsed_req_line_create(ib_tx_t *tx,
                                                 ib_parsed_req_line_t **line,
                                                 const char *method,
                                                 size_t method_len,
                                                 const char *path,
                                                 size_t path_len,
                                                 const char *version,
                                                 size_t version_len)
{
    IB_FTRACE_INIT();
    assert(tx != NULL);
    assert(tx->ib != NULL);
    assert(tx->mp != NULL);
    assert(method != NULL);
    assert(method_len > 0);
    assert(path != NULL);
    assert(path_len > 0);
    assert(version != NULL);
    assert(version_len > 0);

    ib_parsed_req_line_t *line_tmp = ib_mpool_alloc(tx->mp,
                                                    sizeof(*line_tmp));

    if ( line_tmp == NULL ) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    line_tmp->method = method;
    line_tmp->method_len = method_len;
    line_tmp->path = path;
    line_tmp->path_len = path_len;
    line_tmp->version = version;
    line_tmp->version_len = version_len;

    /* Commit back successfully created line. */
    *line = line_tmp;

    IB_FTRACE_RET_STATUS(IB_OK);
}
