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
 * @brief IronBee --- Stream Buffer Routines
 * @author Brian Rectanus <brectanus@qualys.com>
 */

#include "ironbee_config_auto.h"

#include <ironbee/stream.h>

ib_status_t ib_stream_create(ib_stream_t **pstream, ib_mm_t mm)
{
    /* Create the structure. */
    *pstream = (ib_stream_t *)ib_mm_calloc(mm, 1, sizeof(**pstream));
    if (*pstream == NULL) {
        return IB_EALLOC;
    }
    (*pstream)->mm = mm;

    return IB_OK;
}


ib_status_t ib_stream_push_sdata(ib_stream_t *s,
                                 ib_sdata_t *sdata)
{
    s->slen += sdata->dlen;

    if (IB_LIST_GEN_ELEMENTS(s) == 0) {
        IB_LIST_GEN_NODE_INSERT_INITIAL(s, sdata);
        return IB_OK;
    }

    IB_LIST_GEN_NODE_INSERT_LAST(s, sdata, ib_sdata_t);

    return IB_OK;
}

ib_status_t ib_stream_push(ib_stream_t *s,
                           ib_sdata_type_t type,
                           void *data,
                           size_t dlen)
{
    /// @todo take from a resource pool, if available
    ib_sdata_t *node = (ib_sdata_t *)ib_mm_calloc(s->mm, 1, sizeof(*node));
    if (node == NULL) {
        return IB_EALLOC;
    }

    node->type = type;
    node->dlen = dlen;
    node->data = data;

    ib_stream_push_sdata(s, node);

    return IB_OK;
}

ib_status_t ib_stream_pull(ib_stream_t *s,
                           ib_sdata_t **psdata)
{
    if (s->nelts == 0) {
        if (psdata != NULL) {
            *psdata = NULL;
        }
        return IB_ENOENT;
    }

    s->slen -= s->head->dlen;
    if (psdata != NULL) {
        *psdata = s->head;
    }

    IB_LIST_GEN_NODE_REMOVE_FIRST(s);

    return IB_OK;
}

ib_status_t ib_stream_peek(const ib_stream_t *s,
                           ib_sdata_t **psdata)
{
    if (s->nelts == 0) {
        if (psdata != NULL) {
            *psdata = NULL;
        }
        return IB_ENOENT;
    }

    if (psdata != NULL) {
        *psdata = s->head;
    }

    return IB_OK;
}

ib_sdata_t *ib_stream_sdata_next(ib_sdata_t *sdata)
{
    return IB_LIST_GEN_NODE_NEXT(sdata);
}
