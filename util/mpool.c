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
#include <ironbee/lock.h>

#include <stdlib.h>
#include <assert.h>
#include <stdbool.h>

/** Default page size. Buffers will be of size N * IB_MPOOL_DEFAULT_PAGE_SIZE */
#define IB_MPOOL_DEFAULT_PAGE_SIZE ((size_t)1024)

/**
 * The page size can be set to other with ib_mpool_create_ex()
 * but it must be greater than IB_MPOOL_MIN_PAGE_SIZE, otherwise it will be
 * overwritten by IB_MPOOL_MIN_PAGE_SIZE
 */
#define IB_MPOOL_MIN_PAGE_SIZE     ((size_t)512)

/**
 * IB_MPOOL_INCREASE_FACTOR -> Min 2, 4, 8, .. when a new var needs to be
 * allocated and size is greater than the default pagesize, it will create a
 * buffer of size page_size * num_pages_needed * IB_MPOOL_INCREASE_FACTOR.
 * Keep in mind that a high value might misuse mem of the system */
#define IB_MPOOL_INCREASE_FACTOR    2

/**
 * IB_MPOOL_MIN_SIZE_BITS defines the minimum size guaranteed by indexed[0]
 * as a exponent of 2, so IB_MPOOL_MIN_SIZE_BITS 4 implies that 31 bytes can
 * be allocated in buffers linked from @a indexed[0] */
#define IB_MPOOL_MIN_SIZE_BITS      4

/**
 * IB_MPOOL_NUM_SLOTS defines the number of slots in @a indexed */
#define IB_MPOOL_NUM_SLOTS          8

/**
 * IB_MPOOL_REMAINING_LIMIT is the limit of available mem of a buffer.
 * Less mem than IB_MPOOL_MIN_SIZE_BITS makes the buffer to be considered as
 * full, so it will be moved to busy_buffers list */
#define IB_MPOOL_REMAINING_LIMIT    (1 << IB_MPOOL_MIN_SIZE_BITS)


/**
 * Memory buffers structure. Size must be n * IB_MPOOL_DEFAULT_PAGE_SIZE
 */
typedef struct ib_mpool_buffer_t ib_mpool_buffer_t;
struct ib_mpool_buffer_t {
    uint8_t                    *buffer;     /**< ptr to the buffer */
    size_t                      used;       /**< amount of mem really used */
    size_t                      size;       /**< size */

    ib_mpool_buffer_t          *prev;       /**< Sibling previous */
    ib_mpool_buffer_t          *next;       /**< Sibling next */
    /// @todo later we might want to be able to free just one page
    ///       so here we will need references to parent and child pools.
    /// @todo Make this a list/node so IB_LIST_* macros will work?
};

#define IB_MPOOL_MAX_INDEX 7

typedef struct ib_mpool_cleanup_t ib_mpool_cleanup_t;
struct ib_mpool_cleanup_t {
    ib_mpool_cleanup_t         *next;       /**< Sibling next */
    ib_mpool_cleanup_fn_t       free;       /**< Function cleanup callback */
    void                       *free_data;  /**< Data to pass to the callback */
};

/**
 * Memory pool structure.
 * The behavior of ib_mpool_t can be changed by tuning sizes and definitions
 * but right now, this is how empty memory (not allocated) of buffers
 * is indexed:
 *
 * indexed[] will hold the buffers in such way that buffers linked:
 *
 * indexed[0] guarantee to have available memory (free) of at
 *            least 16 bytes and less than 32
 * indexed[1] -> at least 32 and less than 64
 * indexed[2] -> at least 64 and less than 128
 * ...           ...
 * indexed[7] -> at least 2048 and no limit here. We should set one.
 *
 * When ib_mpool_(c?alloc|memdup) is called, the ib_mpool_buffer_t
 * will reserve size for the allocation. The remaining buffer available will
 * determine the index at indexed[] where the buffer will be moved.
 * This allow to reutilize buffers in order to reduce fragmentation.
 * The limit is 7 in the current definition, but it should be possible to
 * increase it by just changing the sizes / limits.
 */
struct ib_mpool_t {
    const char             *name;         /**< Memory pool name */
    ib_mpool_buffer_t      *busy_buffers; /**< List of reserved buffers */
    ib_mpool_buffer_t      *current;      /**< Points to the last buffer used */
    size_t                  size;         /**< Sum of all buffer sizes */
    size_t                  buffer_cnt;   /**< Counter of buffers allocated */
    size_t                  inuse;        /**< Number of bytes in real use */
    size_t                  page_size;    /**< default page size */

    ib_mpool_t             *parent;       /**< Pointer to parent pool */
    ib_mpool_t             *next;         /**< Pointer to next pool in list */
    ib_mpool_t             *child;        /**< Pointer to child list */
    ib_mpool_t             *child_last;   /**< Last child */

    ib_mpool_cleanup_t     *cleanup;      /**< List of items to cleanup */
    ib_mpool_cleanup_t     *cleanup_last; /**< Last cleanup */

    ib_mpool_buffer_t      *indexed[IB_MPOOL_MAX_INDEX + 1];/**< Buffer index */
    ib_lock_t               lock;         /**< Pool lock */
    /// @todo Make this a list/node so IB_LIST_* macros will work?
};

/* Helper Macros for ib_mpool_t / ib_mpool_buffer_t */

/**
 * Allocates mem for a var in a buffer. The address is pointed by ptr.
 *
 * @param buf Pointer to the buffer
 * @param rsize Size to allocate
 * @param ptr Pointer to the start of the mem allocated
 */
#define IB_MPOOL_BUFFER_ALLOC(buf,rsize,ptr) \
    do { \
        (ptr) = (buf)->buffer + (buf)->used; \
        (buf)->used += (rsize); \
    } while (0)

/**
 * Reset the allocations of vars inside a buffer
 *
 * @param buf Pointer to the buffer
 */
#define IB_MPOOL_BUFFER_RESET(buf) \
    do { \
        (buf)->used = 0; \
    } while (0)

/**
 * Returns the available mem (free mem) of a buffer
 *
 * @param buf Pointer to the buffer
 */
#define IB_MPOOL_BUFFER_AVAILABLE(buf) \
    ((int)((int)(buf)->size - (int)(buf)->used))

/**
 * Determines if certain size can be allocated in a buffer
 *
 * @param buf Pointer to the buffer
 * @param rsize Size to check
 */
#define IB_MPOOL_BUFFER_IS_AVAILABLE(buf,rsize) \
    (( (int)((int)(buf)->size - (int)(buf)->used) > (int)(rsize)))

/**
 * Creates a new buffer of size rsize
 *
 * @param buf Pointer to the buffer
 * @param rsize Size of the buffer
 */
#define IB_MPOOL_CREATE_BUFFER(buf,rsize) \
    do { \
        (buf) = (ib_mpool_buffer_t *)malloc(sizeof(ib_mpool_buffer_t)); \
        if ((buf) != NULL) { \
            (buf)->prev = NULL; \
            (buf)->next = NULL; \
            (buf)->size = (rsize); \
            (buf)->buffer = (uint8_t *)malloc((rsize)); \
            if ((buf)->buffer == NULL) { \
                free(buf); \
                buf = NULL; \
            } \
            else { \
                (buf)->used = 0; \
            } \
        } \
    } while(0)

/**
 * Adds a buffer to the indexed[] lists
 *
 * @param pool Pointer to the pool
 * @param rbuf Pointer to the buffer
 * @param rindex Index position of indexed[] (calculated by IB_MPOOL_SET_INDEX)
 */
#define IB_MPOOL_ADD_BUFFER(pool,rbuf,rindex) \
    do { \
        (rbuf)->prev = NULL; \
        (rbuf)->next = (pool)->indexed[(rindex)]; \
        if ((rbuf)->next != NULL) { \
            (rbuf)->next->prev = (rbuf); \
        } \
        (pool)->indexed[(rindex)] = (rbuf); \
        (pool)->size += (rbuf)->size; \
        (pool)->buffer_cnt += 1; \
        (pool)->current = (rbuf); \
    } while (0)

/**
 * Get the index that should be used for a buffer with 'size' available
 *
 * @param size Size of empty mem at the buffer to index
 * @param rindex Index to be used
 */
#define IB_MPOOL_SET_INDEX(size,rindex) \
    do { \
        size_t sz = (size) >> IB_MPOOL_MIN_SIZE_BITS; \
        for (; (sz >> (rindex)) !=  0; (rindex)++) { /* nobody */ } \
        if ((rindex) > 0) {\
            (rindex)--; \
        } \
        if ((rindex) > IB_MPOOL_MAX_INDEX) { \
            (rindex) = IB_MPOOL_MAX_INDEX; \
        } \
    } while(0)

/**
 * Get the starting index where a buffer ready to allocate 'size' can be found
 *
 * @param size Size of the var to be allocated
 * @param rindex Index to be used
 */
#define IB_MPOOL_GET_REQ_INDEX(size,rindex) \
    do { \
        size_t sz = 0; \
        for (sz = (size) >> IB_MPOOL_MIN_SIZE_BITS; sz >> (rindex); (rindex)++) { /* nobody */ } \
        if ((rindex) > IB_MPOOL_MAX_INDEX) { \
            (rindex) = IB_MPOOL_MAX_INDEX; \
        } \
    } while(0)

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
    /* Free the current name */
    if (mp->name != NULL) {
        free((void *)(mp->name));
        mp->name = NULL;
    }
    /* Store the new name */
    if (name != NULL) {
        mp->name = strdup(name);
    }
}

const char *ib_mpool_name(const ib_mpool_t* mp)
{
    const char* result = NULL;
    if ( mp != NULL ) {
        result = mp->name;
    }

    return result;
}

size_t ib_mpool_inuse(const ib_mpool_t* mp)
{
    IB_FTRACE_INIT();

    IB_FTRACE_RET_UINT(mp->inuse);
}

void *ib_mpool_alloc(ib_mpool_t *mp, size_t size)
{
    IB_FTRACE_INIT();

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

    IB_FTRACE_RET_PTR(void, ptr);
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
        ib_mpool_destroy_helper(child, false);
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
