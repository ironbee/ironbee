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
 * @brief IronBee &mdash; Utility Functions
 *
 * @author Brian Rectanus <brectanus@qualys.com>
 * @author Pablo Rincon <pablo.rincon.crespo@gmail.com>
 */

#include "ironbee_config_auto.h"

#include <ironbee/mpool.h>

#include <ironbee/debug.h>

#include "ironbee_util_private.h"

#include <stdlib.h>
#include <assert.h>

ib_status_t ib_mpool_create(ib_mpool_t **pmp,
                            const char *name,
                            ib_mpool_t *parent)
{
    IB_FTRACE_INIT();
    ib_status_t rc;

    if (pmp == NULL) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    rc = ib_mpool_create_ex(pmp, name, parent,
                            IB_MPOOL_DEFAULT_PAGE_SIZE);

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_mpool_create_ex(ib_mpool_t **pmp,
                               const char *name,
                               ib_mpool_t *parent,
                               size_t size)
{
    IB_FTRACE_INIT();
    ib_mpool_buffer_t *buf = NULL;
    size_t slot = 0;
    ib_status_t rc;

    /* Make sure that we have an acceptable page_size */
    if (size < IB_MPOOL_MIN_PAGE_SIZE) {
        size = IB_MPOOL_MIN_PAGE_SIZE;
    }

    /* Always allocate on the heap (as apposed to in the parent pool)
     * so that subpools can be completely freed without having to free
     * the parent pool.
     */
    *pmp = (ib_mpool_t *)calloc(1, sizeof(ib_mpool_t));
    if (*pmp == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    /* Store the name */
    if (name == NULL) {
        (*pmp)->name = NULL;
    }
    else {
        (*pmp)->name = strdup(name);
    }

    /* Add just one page */
    /// @todo Create a list of pages
    IB_MPOOL_CREATE_BUFFER(buf, size);
    if (buf == NULL) {
        /* If an allocation fails, ensure to free the mpool */
        free(*pmp);
        *pmp = NULL;
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    /* Link it in the correct slot. */
    IB_MPOOL_SET_INDEX(size, slot);
    IB_MPOOL_ADD_BUFFER((*pmp), buf, slot);

    /* Set the default page_size to use */
    (*pmp)->page_size = size;

    /* Link parent/child. */
    if (parent != NULL) {
        ib_lock_lock(&(parent->lock));
        (*pmp)->parent = parent;
        if (parent->child_last == NULL) {
            parent->child_last = parent->child = *pmp;
        }
        else {
            parent->child_last = parent->child_last->next = *pmp;
        }
        ib_lock_unlock(&(parent->lock));
    }

    /* Initialize lock */
    rc = ib_lock_init(&((*pmp)->lock));
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

void ib_mpool_setname(ib_mpool_t *mp, const char *name)
{
    ib_lock_lock(&(mp->lock));

    /* Free the current name */
    if (mp->name != NULL) {
        free((void *)(mp->name));
        mp->name = NULL;
    }
    /* Store the new name */
    if (name != NULL) {
        mp->name = strdup(name);
    }

    ib_lock_unlock(&(mp->lock));
}

const char *ib_mpool_name(const ib_mpool_t* mp)
{
    const char* result = NULL;
    if ( mp != NULL ) {
        result = mp->name;
    }

    return result;
}

void *ib_mpool_alloc(ib_mpool_t *mp, size_t size)
{
    IB_FTRACE_INIT();

    ib_lock_lock(&(mp->lock));

    void *ptr = NULL;
    ib_mpool_buffer_t *buf = NULL;
    ib_mpool_buffer_t *iter = NULL;
    size_t cur_slot = 0;
    size_t slot = 0;

    /* First search the index. */
    IB_MPOOL_GET_REQ_INDEX(size, cur_slot);
    for (; cur_slot < IB_MPOOL_NUM_SLOTS; ++cur_slot) {
        iter = mp->indexed[cur_slot];
        /* Should not be necessary to check if mem is available, since buf
         * should have at least 2^(IB_MPOOL_MIN_SIZE_BITS+cur_slot).
         */
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
            /* Look for a buffer bigger than the max guaranteed at indexed[]. */
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

    /* If there is a buffer/page with enough room, alloc. Otherwise create a
     * new page and index it.
     */
    if (buf != NULL) {
        int slot_size;

        /* Alloc the buffer */
        IB_MPOOL_BUFFER_ALLOC(buf,size,ptr);

        /* Only reindex if required. */
        slot_size = 1 << (IB_MPOOL_MIN_SIZE_BITS + cur_slot);
        if (slot_size > IB_MPOOL_BUFFER_AVAILABLE(buf)) {
            /* Extract it from the index and reindex it with the remaining
             * buffer available.
             */
            if (buf->prev == NULL) {
                /* We are at the start. Pop it */
                mp->indexed[cur_slot] = buf->next;
                if (mp->indexed[cur_slot] != NULL) {
                    mp->indexed[cur_slot]->prev = NULL;
                }
                buf->next = NULL;
            }
            else {
                /* In the middle or at the end */
                buf->prev->next = buf->next;
                if (buf->next != NULL) {
                    buf->next->prev = buf->prev;
                }
                buf->prev = NULL;
                buf->next = NULL;
            }

            /* Now if the buffer is full
             * (less than IB_MPOOL_REMAINING_LIMIT bytes available), move
             * it to the list of busy_buffers. Otherwise, reindex.
             */
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
        }
    }
    else {
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
         * IB_MPOOL_REMAINING_LIMIT bytes available), move it to the
         * list of busy_buffers Otherwise, reindex it at indexed[].
         */
        if (IB_MPOOL_BUFFER_AVAILABLE(buf) < IB_MPOOL_REMAINING_LIMIT) {
            buf->next = mp->busy_buffers;
            buf->prev = NULL;
            if (buf->next != NULL) {
                buf->next->prev = buf;
            }
            mp->busy_buffers = buf;
        }
        else {
            slot = 0;
            /* Index the remaining buffer of the page */
            IB_MPOOL_SET_INDEX(IB_MPOOL_BUFFER_AVAILABLE(buf), slot);
            IB_MPOOL_ADD_BUFFER(mp, buf, slot);
        }
    }

    /* Update mem in use */
    mp->inuse += size;

    ib_lock_unlock(&(mp->lock));

    IB_FTRACE_RET_PTR(void, ptr);
}

void ib_mpool_clear(ib_mpool_t *mp)
{
    IB_FTRACE_INIT();


    ib_mpool_buffer_t *buf;
    ib_mpool_buffer_t *next;
    ib_mpool_t *child;
    ib_mpool_t *child_next;
    size_t free_space;
    int i;

    if (mp == NULL) {
        IB_FTRACE_RET_VOID();
    }

    ib_lock_lock(&(mp->lock));

    /* Destroy child pools. */
    for (child = mp->child; child != NULL; child = child_next) {
        child_next = child->next;
        ib_mpool_destroy(child);
    }
    mp->child = mp->child_last = NULL;

    /* Move all indexed buffers to busy_buffers. */
    for (i = 0; i < IB_MPOOL_MAX_INDEX; ++i) {
        buf = mp->indexed[i];
        if (buf != NULL) {
            for (; buf != NULL; buf = next) {
                next = buf->next;
                buf->prev = NULL;
                buf->next = mp->busy_buffers;
                mp->busy_buffers = buf;
            }
            mp->indexed[i] = NULL;
        }
    }

    /* Reset all buffers and index them. */
    for (free_space = 0, buf = mp->busy_buffers; buf != NULL; buf = next) {
        next = buf->next;
        /* Return some mem to the system if we have enough space already. */
        if (free_space > IB_MPOOL_INCREASE_FACTOR * IB_MPOOL_DEFAULT_PAGE_SIZE)
        {
            free(buf);
        }
        else {
            size_t slot = 0;

            IB_MPOOL_BUFFER_RESET(buf);

            /* Index the remaining buffer of the page */
            IB_MPOOL_SET_INDEX(IB_MPOOL_BUFFER_AVAILABLE(buf), slot);
            IB_MPOOL_ADD_BUFFER(mp, buf, slot);
            free_space += buf->size;
        }
    }

    mp->busy_buffers = NULL;
    mp->size = free_space;
    mp->inuse = 0;

    ib_lock_unlock(&(mp->lock));

    IB_FTRACE_RET_VOID();
}

static
void ib_mpool_destroy_helper(ib_mpool_t* mp, bool lock_parents)
{
    IB_FTRACE_INIT();

    ib_mpool_buffer_t *buf;
    ib_mpool_buffer_t *next;
    ib_mpool_t *child;
    ib_mpool_t *child_next;
    int i;

    if (mp == NULL) {
        IB_FTRACE_RET_VOID();
    }

    ib_lock_lock(&(mp->lock));

    /* Free the name */
    if (mp->name != NULL) {
        free((void *)(mp->name));
        mp->name = NULL;
    }

    /* Destroy child pools. */
    for (child = mp->child; child != NULL; child = child_next) {
        child_next = child->next;
        ib_mpool_destroy_helper(child, false);
    }
    mp->child = mp->child_last = NULL;

    /* Run all of the cleanup functions.
     * This must happen before freeing the pool buffers.*/
    if (mp->cleanup != NULL) {
        ib_mpool_cleanup_t *mpc;

        for (mpc = mp->cleanup; mpc != NULL; mpc = mpc->next) {
            mpc->free(mpc->free_data);
        }
    }

    /* Free the indexed buffers. */
    for (i = 0; i <= IB_MPOOL_MAX_INDEX; ++i) {
        buf = mp->indexed[i];
        if (buf != NULL) {
            for (; buf != NULL; buf = next) {
                next = buf->next;
                free(buf->buffer);
                free(buf);
            }
            mp->indexed[i] = NULL;
        }
    }

    /* Free all busy buffers. */
    for (buf = mp->busy_buffers; buf != NULL; buf = next) {
        next = buf->next;
        free(buf->buffer);
        free(buf);
    }

    mp->busy_buffers = NULL;
    mp->current = NULL;
    mp->size = 0;
    mp->inuse = 0;

    /* Unlink parent/child. */
    if (mp->parent != NULL) {
        ib_mpool_t *parent = mp->parent;
        ib_mpool_t *mp_prev = NULL;

        if (lock_parents) {
            ib_lock_lock(&(parent->lock));
        }

        /* Remove this mp from the child list. */
        if (parent->child == mp) {
            /* First in list. */
            parent->child = parent->child->next;
        }
        else {
            mp_prev = parent->child;
            ib_mpool_t *mp_child = mp_prev ? mp_prev->next : NULL;

            /* Search the list. */
            while (mp_child != NULL) {
                if (mp_child == mp) {
                    mp_prev->next = mp_child->next;
                    break;
                }
                mp_prev = mp_child;
                mp_child = mp_child->next;
            }
        }

        /* Update last child reference if required. */
        if (parent->child_last == mp) {
            parent->child_last = mp_prev;
        }

        if (lock_parents) {
            ib_lock_unlock(&(parent->lock));
        }
    }

    ib_lock_unlock(&(mp->lock));
    ib_lock_destroy(&(mp->lock));

    free(mp);

    IB_FTRACE_RET_VOID();
}

void ib_mpool_destroy(ib_mpool_t *mp)
{
    IB_FTRACE_INIT();

    ib_mpool_destroy_helper(mp, true);

    IB_FTRACE_RET_VOID();
}

ib_status_t ib_mpool_cleanup_register(ib_mpool_t *mp,
                                      ib_mpool_cleanup_fn_t cleanup,
                                      void *data)
{
    IB_FTRACE_INIT();

    ib_mpool_cleanup_t *mpc;

    mpc = (ib_mpool_cleanup_t *)ib_mpool_alloc(mp, sizeof(*mpc));

    if (mpc == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    ib_lock_lock(&(mp->lock));

    mpc->next = NULL;
    mpc->free = cleanup;
    mpc->free_data = data;

    /* Add the cleanup to the end of the list. */
    if (mp->cleanup_last == NULL) {
        mp->cleanup_last = mp->cleanup = mpc;
    }
    else {
        mp->cleanup_last = mp->cleanup_last->next = mpc;
    }

    ib_lock_unlock(&(mp->lock));

    IB_FTRACE_RET_STATUS(IB_OK);
}

/* The following routines do not need locking as the heavy lifting is handled
 * by the routines above.
 */

void *ib_mpool_calloc(ib_mpool_t *mp, size_t nelem, size_t size)
{
    IB_FTRACE_INIT();

    void *ptr = ib_mpool_alloc(mp, nelem * size);

    if (ptr == NULL) {
        IB_FTRACE_RET_PTR(void, NULL);
    }

    memset(ptr, 0, nelem * size);

    IB_FTRACE_RET_PTR(void, ptr);
}

char *ib_mpool_strdup(ib_mpool_t *mp, const char *src)
{
    IB_FTRACE_INIT();
    size_t size = strlen(src)+1;
    char *ptr = (char *)ib_mpool_alloc(mp, size);

    if (ptr != NULL) {
        memcpy(ptr, src, size);
    }

    IB_FTRACE_RET_PTR(char, ptr);
}

char DLL_PUBLIC *ib_mpool_memdup_to_str(ib_mpool_t *mp,
                                        const void *src,
                                        size_t size)
{
    IB_FTRACE_INIT();
    char *str = (char *)ib_mpool_alloc(mp, size + 1);

    if (str != NULL) {
        memcpy(str, src, size);
        *(str + size) = '\0';
    }

    IB_FTRACE_RET_PTR(char, str);
}

void *ib_mpool_memdup(ib_mpool_t *mp, const void *src, size_t size)
{
    IB_FTRACE_INIT();
    void *ptr = ib_mpool_alloc(mp, size);

    if (ptr != NULL) {
        memcpy(ptr, src, size);
    }

    IB_FTRACE_RET_PTR(void, ptr);
}
