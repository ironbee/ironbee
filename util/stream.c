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
 * @brief IronBee - Stream Buffer Routines
 * @author Brian Rectanus <brectanus@qualys.com>
 */

#include "ironbee_config_auto.h"

#include <string.h>

#include <ironbee/mpool.h>
#include <ironbee/stream.h>
#include <ironbee/debug.h>

#include "ironbee_util_private.h"

ib_status_t ib_stream_create(ib_stream_t **pstream, ib_mpool_t *pool)
{
    IB_FTRACE_INIT(ib_stream_create);
    /* Create the structure. */
    *pstream = (ib_stream_t *)ib_mpool_calloc(pool, 1, sizeof(**pstream));
    if (*pstream == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }
    (*pstream)->mp = pool;

    IB_FTRACE_RET_STATUS(IB_OK);
}


ib_status_t ib_stream_push_sdata(ib_stream_t *s,
                                 ib_sdata_t *sdata)
{
    IB_FTRACE_INIT(ib_stream_push_sdata);

    s->slen += sdata->dlen;

    if (IB_LIST_ELEMENTS(s) == 0) {
        IB_LIST_NODE_INSERT_INITIAL(s, sdata);
        IB_FTRACE_RET_STATUS(IB_OK);
    }

    IB_LIST_NODE_INSERT_LAST(s, sdata, ib_sdata_t);

    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t ib_stream_push(ib_stream_t *s,
                           ib_sdata_type_t type,
                           int dtype,
                           void *data,
                           size_t dlen)
{
    IB_FTRACE_INIT(ib_stream_push);
    /// @todo take from a resource pool, if available
    ib_sdata_t *node = (ib_sdata_t *)ib_mpool_calloc(s->mp,
                                                     1, sizeof(*node));
    if (node == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    node->type = type;
    node->dtype = dtype;
    node->dlen = dlen;
    node->data = data;

    ib_stream_push_sdata(s, node);

    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t ib_stream_pull(ib_stream_t *s,
                           ib_sdata_t **psdata)
{
    IB_FTRACE_INIT(ib_stream_pull);

    if (s->nelts == 0) {
        if (psdata != NULL) {
            *psdata = NULL;
        }
        IB_FTRACE_RET_STATUS(IB_ENOENT);
    }

    s->slen -= s->head->dlen;
    if (psdata != NULL) {
        *psdata = s->head;
    }

    IB_LIST_NODE_REMOVE_FIRST(s);

    IB_FTRACE_RET_STATUS(IB_OK);
}

