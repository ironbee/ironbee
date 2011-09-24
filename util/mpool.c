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
 * @brief IronBee - Utility Functions
 *
 * @author Brian Rectanus <brectanus@qualys.com>
 * @author Pablo Rincon <pablo.rincon.crespo@gmail.com>
 *   - Custom memory pool implementation (removing APR pools dependency)
 */

#include "ironbee_config_auto.h"

#include <string.h>

#include <ironbee/util.h>

#include "ironbee_util_private.h"


/* #define ASSERT_MPOOL */
#ifdef ASSERT_MPOOL
#include <assert.h>
#endif


ib_status_t ib_mpool_create(ib_mpool_t **pmp, ib_mpool_t *parent)
{
    IB_FTRACE_INIT(ib_mpool_create);
    ib_status_t rc;

    if (pmp == NULL) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    rc = ib_mpool_create_ex(pmp, parent,
                            IB_MPOOL_DEFAULT_PAGE_SIZE);

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_mpool_create_ex(ib_mpool_t **pmp,
                               ib_mpool_t *parent,
                               size_t size)
{
    IB_FTRACE_INIT(ib_mpool_create_ex);

    /* Make sure that we have an acceptable page_size */
    if (size < IB_MPOOL_MIN_PAGE_SIZE) {
        size = IB_MPOOL_MIN_PAGE_SIZE;
    }

    if (pmp == NULL) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }
    
    if (parent == NULL) {
        *pmp = (ib_mpool_t *)malloc(sizeof(ib_mpool_t));
        if (*pmp == NULL) {
            IB_FTRACE_RET_STATUS(IB_EALLOC);
        }
    }
    else {
        *pmp = (ib_mpool_t *)ib_mpool_calloc(parent, 1, sizeof(ib_mpool_t));
        if (*pmp == NULL) {
            IB_FTRACE_RET_STATUS(IB_EALLOC);
        }
    }
    memset(*pmp, 0, sizeof(ib_mpool_t));

    /* Add just one page */
    /* @todo Create a list of pages */
    ib_mpool_buffer_t *buf = NULL;
    IB_MPOOL_CREATE_BUFFER(buf, size);
    if (buf == NULL) {
        /* If an allocation fails, ensure to free the mpool */
        if (parent != NULL) {
            parent->current->used -= sizeof(ib_mpool_t);
        }
        else {
            free(*pmp);
            *pmp = NULL;
        }
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    /* Link it in the correct slot */
    size_t slot = 0;
    IB_MPOOL_SET_INDEX(size, slot);
    IB_MPOOL_ADD_BUFFER((*pmp), buf, slot);

    /* Set the default page_size to use */
    (*pmp)->page_size = size;

    /* Link with parent and siblings */
    if (parent != NULL) {
        (*pmp)->parent = parent;
        if (parent->child != NULL) {
            parent->child->prev = *pmp;
        }
        (*pmp)->next = parent->child;
        parent->child = *pmp;
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

void *ib_mpool_alloc(ib_mpool_t *mp, size_t size)
{
    IB_FTRACE_INIT(ib_mpool_alloc);
    void *ptr = NULL;

    ib_mpool_buffer_t *buf = NULL;
    ib_mpool_buffer_t *iter = NULL;
    size_t slot = 0;

    /* First search at indexed */
    size_t cur_slot = 0;
    IB_MPOOL_GET_REQ_INDEX(size, cur_slot);
    for (; cur_slot < 8; cur_slot++) {
        iter = mp->indexed[cur_slot];
        /* Should not be neccesary to check if mem is available, since buf
           should have at least 2^(IB_MPOOL_MIN_SIZE_BITS+cur_slot) */
        if (iter != NULL &&
            (cur_slot < IB_MPOOL_MAX_INDEX ||
             size < (1 << (IB_MPOOL_MAX_INDEX + IB_MPOOL_MIN_SIZE_BITS))))
        {
#ifdef ASSERT_MPOOL
            assert(IB_MPOOL_BUFFER_IS_AVAILABLE(iter, size));
#endif
            buf = iter;
            break;
        }
        else {
            /* Look if we do have a buffer bigger than the max
               guaranteed at indexed[] */
            for (; iter != NULL; iter = iter->next) {
                if (IB_MPOOL_BUFFER_IS_AVAILABLE(iter, size)) {
                    buf = iter;
                    break;
                }
            }

            if (buf != NULL) {
                break;
            }
        }
    }

    /* If we found a buffer/page with enough room, alloc. Otherwise create a
       new page and index it */
    if (buf != NULL) {
        /* Alloc the var */
        IB_MPOOL_BUFFER_ALLOC(buf,size,ptr);
        /* Extract it from the index and reindex it with the remaining
           buffer available */
        if (buf->prev == NULL) {
            /* We are at the start. Pop it */
            mp->indexed[cur_slot] = buf->next;
            if (mp->indexed[cur_slot] != NULL) {
                mp->indexed[cur_slot]->prev = NULL;
            }
            buf->next = NULL;
        } else {
            /* In the middle or at the end */
            buf->prev->next = buf->next;
            if (buf->next != NULL) {
                buf->next->prev = buf->prev;
            }
            buf->prev = NULL;
            buf->next = NULL;
        }

        /* We have extracted it. Now if the buffer is full
           (less than IB_MPOOL_REMAINING_LIMIT bytes available), move it to the
           list of busy_buffers Otherwise, reindex it at indexed[] */
        if (IB_MPOOL_BUFFER_AVAILABLE(buf) < IB_MPOOL_REMAINING_LIMIT) {
            buf->prev = NULL;
            buf->next = mp->busy_buffers;
            if (buf->next != NULL) {
                buf->next->prev = buf;
            }
            mp->busy_buffers = buf;
        }
        else {
            slot = 0;
            /* Get the new index */
            IB_MPOOL_SET_INDEX(IB_MPOOL_BUFFER_AVAILABLE(buf), slot);
            IB_MPOOL_ADD_BUFFER(mp, buf, slot);
        }
    } else {
        size_t nump = 0;
        size_t bufsize = 0;

        /* Calculate size/pages for the new buffer */
        if (size < mp->page_size) {
            nump = 1;
        }
        else {
            nump = size / mp->page_size;
            if (size % mp->page_size) {
                nump++;
            }
        }

        bufsize = mp->page_size * nump * IB_MPOOL_INCREASE_FACTOR;
        IB_MPOOL_CREATE_BUFFER(buf,bufsize);
        if (buf == NULL) {
            IB_FTRACE_RET_PTR(void, NULL);
        }

        /* Alloc the var */
        IB_MPOOL_BUFFER_ALLOC(buf,size,ptr);

        /* Now if the buffer is full (less than
           IB_MPOOL_REMAINING_LIMIT bytes available), move it to the
           list of busy_buffers Otherwise, reindex it at indexed[] */
        if (IB_MPOOL_BUFFER_AVAILABLE(buf) < IB_MPOOL_REMAINING_LIMIT) {
            buf->next = mp->busy_buffers;
            buf->prev = NULL;
            if (buf->next != NULL) {
                buf->next->prev = buf;
            }
            mp->busy_buffers = buf;
        } else {
            slot = 0;
            /* Index the remaining buffer of the page */
            IB_MPOOL_SET_INDEX(IB_MPOOL_BUFFER_AVAILABLE(buf), slot);
            IB_MPOOL_ADD_BUFFER(mp, buf, slot);
        }
    }

    /* Update mem in use */
    mp->inuse += size;

    IB_FTRACE_RET_PTR(void, ptr);
}

void *ib_mpool_calloc(ib_mpool_t *mp, size_t nelem, size_t size)
{
    IB_FTRACE_INIT(ib_mpool_calloc);
    void *ptr = ib_mpool_alloc(mp, nelem * size);
    if (ptr == NULL) {
        IB_FTRACE_RET_PTR(void, NULL);
    }
    memset(ptr, 0, nelem * size);
    IB_FTRACE_RET_PTR(void, ptr);
}

void *ib_mpool_memdup(ib_mpool_t *mp, const void *src, size_t size)
{
    IB_FTRACE_INIT(ib_mpool_memdup);
    void *ptr = ib_mpool_alloc(mp, size);
    if (ptr != NULL) {
        memcpy(ptr, src, size);
    }
    IB_FTRACE_RET_PTR(void, ptr);
}

void ib_mpool_clear(ib_mpool_t *mp)
{
    IB_FTRACE_INIT(ib_mpool_clear);
    ib_mpool_buffer_t *bufs = NULL;
    ib_mpool_buffer_t *next = NULL;

    if (mp == NULL) {
        IB_FTRACE_RET_VOID();
    }

    /* @todo: destroy child pools */
    int i = 0;
    ib_mpool_t *childs = NULL;
    ib_mpool_t *child_next = NULL;
    for (; childs; childs = child_next) {
        child_next = childs->next;
        ib_mpool_destroy(childs);
    }

    /* Move all out of the array of indexed, into busy_buffers */
    for (; i < IB_MPOOL_MAX_INDEX; i++) {
        bufs = mp->indexed[i];
        if (bufs != NULL) {
            for (; bufs != NULL; bufs = next) {
                next = bufs->next;
                bufs->prev = NULL;
                bufs->next = mp->busy_buffers;
                mp->busy_buffers = bufs;
            }
            mp->indexed[i] = NULL;
        }
    }

    /* Reset all buffers and index them */
    size_t free_space = 0;
    bufs = mp->busy_buffers;
    for (; bufs != NULL; bufs = next) {
        next = bufs->next;
        /* Return some mem to the system if we have enough space already */
        if (free_space > IB_MPOOL_INCREASE_FACTOR * IB_MPOOL_DEFAULT_PAGE_SIZE)
        {
            free(bufs);
        }
        else {
            IB_MPOOL_BUFFER_RESET(bufs);
            size_t slot = 0;
            /* Index the remaining buffer of the page */
            IB_MPOOL_SET_INDEX(IB_MPOOL_BUFFER_AVAILABLE(bufs), slot);
            IB_MPOOL_ADD_BUFFER(mp, bufs, slot);
            free_space += bufs->size;
        }
    }
    mp->busy_buffers = NULL;
    mp->size = free_space;
    mp->inuse = 0;

    IB_FTRACE_RET_VOID();
}

void ib_mpool_destroy(ib_mpool_t *mp)
{
    IB_FTRACE_INIT(ib_mpool_destroy);
    ib_mpool_buffer_t *bufs = NULL;
    ib_mpool_buffer_t *next = NULL;

    if (mp == NULL) {
        IB_FTRACE_RET_VOID();
    }

    /* @todo: destroy child pools */
    int i = 0;
    ib_mpool_t *childs = NULL;
    ib_mpool_t *child_next = NULL;
    for (; childs; childs = child_next) {
        child_next = childs->next;
        ib_mpool_destroy(childs);
    }

    /* Move all out of the array of indexed, into busy_buffers */
    for (; i < IB_MPOOL_MAX_INDEX; i++) {
        bufs = mp->indexed[i];
        if (bufs != NULL) {
            for (; bufs != NULL; bufs = next) {
                next = bufs->next;
                bufs->prev = NULL;
                bufs->next = mp->busy_buffers;
                mp->busy_buffers = bufs;
            }
            mp->indexed[i] = NULL;
        }
    }

    /* Free all buffers */
    bufs = mp->busy_buffers;
    for (; bufs != NULL; bufs = next) {
        next = bufs->next;
        free(bufs);
    }
    mp->busy_buffers = NULL;
    mp->size = 0;
    mp->inuse = 0;

    /* If there's a registered clean up function, use it */
    if (mp->free != NULL) {
        mp->free(mp->free_data);
    }

    /* Check if mp is alloced inside another pool or not */
    if (mp->parent == NULL) {
        free(mp);
    }

    IB_FTRACE_RET_VOID();
}

void ib_mpool_cleanup_register(ib_mpool_t *mp,
                               void *data,
                               ib_mpool_cleanup_fn_t cleanup)
{
    IB_FTRACE_INIT(ib_mpool_cleanup_register);
    /* @todo We should create here a list of callbacks,
       instead of just one, to allow multiple cleanups of different things */
    mp->free = cleanup;
    mp->free_data = data;
    IB_FTRACE_RET_VOID();
}


