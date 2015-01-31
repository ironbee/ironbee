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
 * @brief IronBee --- Filter Implementation
 *
 * @author Sam Baskinger <sbaskinger@qualys.com>
 */

#include <ironbee/build.h>
#include <ironbee/filter.h>
#include <ironbee/mm.h>
#include <ironbee/types.h>

#include <assert.h>

struct ib_filter_t {
    const char *name; /**< The name of this filter. */
    const char *type; /**< The type this filter handles. */

    ib_filter_create_fn    create_fn;      /**< Create callback. */
    void                  *create_cbdata;  /**< Create cbdata. */
    ib_filter_execute_fn   execute_fn;     /**< Execute callback. */
    void                  *execute_cbdata; /**< Execute cbdata. */
    ib_filter_destroy_fn   destroy_fn;     /**< Destroy callback. */
    void                  *destroy_cbdata; /**< Destroy cbdata. */
};

struct ib_filter_inst_t {
    ///! Reference back to the filter that defines this filter instance.
    ib_filter_t *filter;

    ///! Data particular to this instance.
    void *instance_data;

    ///! List of the next filter instances to call.
    ib_list_t *next;
};

struct ib_filter_data_t
{
    ib_mpool_freeable_segment_t *segment; /**< Memory backing. */
    void                        *ptr;     /**< Pointer into segment. */
    size_t                       len;     /**< The length in bytes. */
    ib_filter_data_type_t        type;
};

/* Filter */
ib_status_t ib_filter_create(
    ib_filter_t          **filter,
    ib_mm_t                mm,
    const char            *name,
    const char            *type,
    ib_filter_create_fn    create_fn,
    void                  *create_cbdata,
    ib_filter_execute_fn   execute_fn,
    void                  *execute_cbdata,
    ib_filter_destroy_fn   destroy_fn,
    void                  *destroy_cbdata
)
{
    ib_filter_t *fltr;

    fltr = ib_mm_alloc(mm, sizeof(*fltr));
    if (fltr == NULL) {
        return IB_EALLOC;
    }

    fltr->name = ib_mm_strdup(mm, name);
    if (fltr->name == NULL) {
        return IB_EALLOC;
    }

    fltr->type = ib_mm_strdup(mm, type);
    if (fltr->type == NULL) {
        return IB_EALLOC;
    }

    fltr->create_fn      = create_fn;
    fltr->create_cbdata  = create_cbdata;
    fltr->execute_fn     = execute_fn;
    fltr->execute_cbdata = execute_cbdata;
    fltr->destroy_fn     = destroy_fn;
    fltr->destroy_cbdata = destroy_cbdata;

    *filter = fltr;
    return IB_OK;
}

const char *ib_filter_name(
    const ib_filter_t *filter
)
{
    assert(filter != NULL);
    assert(filter->name != NULL);

    return filter->name;
}

const char *ib_filter_type(
    const ib_filter_t *filter
)
{
    assert(filter != NULL);
    assert(filter->type != NULL);

    return filter->type;
}

/* End Filter */

/* Filter Instance */

static void filter_inst_destroy(void *cbdata) {

    ib_filter_inst_t *inst = (ib_filter_inst_t *)cbdata;

    assert(inst != NULL);
    assert(inst->filter != NULL);

    if (inst->filter->destroy_fn != NULL) {
        inst->filter->destroy_fn(
            inst->instance_data,
            inst->filter->destroy_cbdata
        );
    }
}
ib_status_t ib_filter_inst_create(
    ib_filter_inst_t **filter_inst,
    ib_mm_t            mm,
    ib_filter_t       *filter,
    void              *arg
)
{
    assert(filter_inst != NULL);
    assert(filter != NULL);

    ib_status_t       rc;
    ib_filter_inst_t *inst;

    inst = ib_mm_alloc(mm, sizeof(*inst));
    if (inst == NULL) {
        return IB_EALLOC;
    }

    rc = ib_list_create(&inst->next, mm);
    if (rc != IB_OK) {
        return rc;
    }

    inst->filter = filter;

    /* All the constructor. */
    rc = filter->create_fn(
        &inst->instance_data,
        mm,
        filter,
        arg,
        filter->create_cbdata);
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_mm_register_cleanup(mm, filter_inst_destroy, inst);
    if (rc != IB_OK) {
        return rc;
    }

    *filter_inst = inst;
    return IB_OK;
}

ib_filter_t *ib_filter_inst_filter(
    ib_filter_inst_t *filter_inst
)
{
    assert(filter_inst != NULL);
    assert(filter_inst->filter != NULL);

    return filter_inst->filter;
}

ib_status_t ib_filter_inst_add(
    ib_filter_inst_t *filter,
    ib_filter_inst_t *next
)
{
    assert(filter != NULL);
    assert(filter->next != NULL);
    assert(next != NULL);

    return ib_list_push(filter->next, next);
}

/* End Filter Instance */

/* Filter Data */

ib_status_t ib_filter_data_create(
    ib_filter_data_t    **data,
    ib_mpool_freeable_t  *mp,
    size_t                sz
)
{
    assert(data != NULL);
    assert(mp != NULL);

    ib_filter_data_t            *d;
    ib_mpool_freeable_segment_t *seg;

    /* Allocate a segment that can hold *d and sz bytes. */
    seg = ib_mpool_freeable_segment_alloc(mp, sizeof(*d) + sz);
    if (seg == NULL) {
        return IB_EALLOC;
    }

    d          = ib_mpool_freeable_segment_ptr(seg);
    d->ptr     = (void *)(((char *)d) + sizeof(*d));
    d->len     = sz;
    d->segment = seg;
    d->type    = IB_FILTER_DATA;

    *data = d;

    return IB_OK;
}

ib_status_t ib_filter_data_flush_create(
    ib_filter_data_t    **data,
    ib_mpool_freeable_t  *mp
)
{
    assert(data != NULL);
    assert(mp != NULL);

    ib_filter_data_t            *d;
    ib_mpool_freeable_segment_t *seg;

    /* Allocate a segment that can hold *d and sz bytes. */
    seg = ib_mpool_freeable_segment_alloc(mp, sizeof(*d));
    if (seg == NULL) {
        return IB_EALLOC;
    }

    d          = ib_mpool_freeable_segment_ptr(seg);
    d->ptr     = NULL;
    d->len     = 0;
    d->segment = seg;
    d->type    = IB_FILTER_FLUSH;

    *data = d;

    return IB_OK;
}

ib_status_t ib_filter_data_cpy(
    ib_filter_data_t    **data,
    ib_mpool_freeable_t  *mp,
    const uint8_t        *src,
    size_t                sz
)
{
    assert(data != NULL);
    assert(mp != NULL);
    assert(src != NULL);

    ib_status_t rc;

    rc = ib_filter_data_create(data, mp, sz);
    if (rc != IB_OK) {
        return rc;
    }

    /* Populate the stream pump data. */
    memcpy(ib_filter_data_ptr(*data), src, sz);

    return IB_OK;
}

static ib_status_t filter_data_slice(
    ib_filter_data_t      **dst,
    ib_mpool_freeable_t    *mp,
    const ib_filter_data_t *src,
    size_t                  start,
    size_t                  length
)
{
    assert(dst != NULL);
    assert(mp != NULL);
    assert(src != NULL);
    assert(src->segment != NULL);
    assert(src->type == IB_FILTER_DATA);

    ib_filter_data_t *d;
    ib_status_t       rc;

    /* If the user askes us to copy a segment that lands outside of src. */
    if (start + length > src->len) {
        return IB_EINVAL;
    }

    d = malloc(sizeof(*d));
    if (d == NULL) {
        return IB_EALLOC;
    }

    /* Ask the segment to clean up d when it is destroyed. */
    rc = ib_mpool_freeable_segment_register_cleanup(
        mp,
        src->segment,
        free,
        d);
    if (rc != IB_OK) {
        free(d);
        return rc;
    }

    /* Increase the references to the segment. */
    rc = ib_mpool_freeable_segment_ref(mp, src->segment);
    if (rc != IB_OK) {
        /* No free(d). It is registered with the segment for cleanup. */
        return rc;
    }

    d->segment = src->segment;
    d->ptr     = (void *)((((char *)src->ptr)) + start);
    d->len     = length;
    d->type    = src->type;

    *dst = d;
    return IB_OK;
}

ib_status_t ib_filter_data_slice(
    ib_filter_data_t      **dst,
    ib_mpool_freeable_t    *mp,
    const ib_filter_data_t *src,
    size_t                  start,
    size_t                  length
)
{
    assert(dst != NULL);
    assert(mp != NULL);
    assert(src != NULL);
    assert(src->segment != NULL);

    switch(src->type) {
    case IB_FILTER_DATA:
        return filter_data_slice(dst, mp, src, start, length);
    case IB_FILTER_FLUSH:
        /* We can't slice meta types, like flush. Just make another. */
        return ib_filter_data_flush_create(dst, mp);
    }

    return IB_EINVAL;
}

void * ib_filter_data_ptr(
    const ib_filter_data_t *data
)
{
    assert(data != NULL);

    return data->ptr;
}

size_t ib_filter_data_len(
    const ib_filter_data_t *data
)
{
    assert(data != NULL);

    return data->len;
}

void ib_filter_data_destroy(
    ib_filter_data_t    *data,
    ib_mpool_freeable_t *mp
)
{
    assert(mp != NULL);
    assert(data != NULL);
    assert(data->segment != NULL);

    ib_mpool_freeable_segment_free(mp, data->segment);
}

ib_filter_data_type_t ib_filter_data_type(
    const ib_filter_data_t *data
)
{
    assert(data != NULL);

    return data->type;
}

/* End Filter Data */

/* Evaluation code. */

ib_status_t ib_filter_inst_process(
    ib_filter_inst_t   *filter_inst,
    ib_mpool_freeable_t *mp,
    ib_mm_t              mm_eval,
    const ib_list_t     *data
)
{
    assert(filter_inst != NULL);
    assert(mp != NULL);
    assert(filter_inst->filter != NULL);

    if (filter_inst->filter->execute_fn) {
        ib_list_t   *out_data;
        ib_status_t  rc;

        rc = ib_list_create(&out_data, mm_eval);
        if (rc != IB_OK) {
            // FIXME - Log this.
            return rc;
        }

        rc = filter_inst->filter->execute_fn(
            filter_inst,
            filter_inst->instance_data,
            mp,
            mm_eval,
            data,
            out_data,
            filter_inst->filter->execute_cbdata
        );

        /* If a filter declines, do not pass data down stream. */
        if (rc == IB_DECLINED) {
            return rc;
        }
        /* On error, do not pass data down stream. */
        else if (rc != IB_OK) {
            // FIXME - Log this.
            return rc;
        }

        /* Return down stream errors back. */
        rc = ib_filter_insts_process(
            filter_inst->next,
            mp,
            mm_eval,
            out_data
        );
        if (rc == IB_DECLINED) {
            return rc;
        }
        else if (rc != IB_OK) {
            // FIXME - Log this. Need engine?
            return rc;
        }
    }

    return IB_OK;
}

ib_status_t ib_filter_insts_process(
    ib_list_t           *filter_insts,
    ib_mpool_freeable_t *mp,
    ib_mm_t              mm_eval,
    const ib_list_t     *data
)
{
    assert(filter_insts != NULL);
    assert(mp != NULL);
    assert(data != NULL);

    ib_status_t rc;
    ib_list_node_t *node;

    IB_LIST_LOOP(filter_insts, node) {

        assert(node != NULL);

        ib_filter_inst_t *filter_inst =
            (ib_filter_inst_t *)ib_list_node_data(node);

        rc = ib_filter_inst_process(filter_inst, mp, mm_eval, data);
        if (rc != IB_OK && rc != IB_DECLINED) {
            // FIXME - real error. Destroy / remove filter?
        }
    }

    return IB_OK;
}

/* End Evaluation Code. */