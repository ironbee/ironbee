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
 * @brief IronBee --- Memory Pool Freeable Implementation
 *
 * See ib_mpool_freeable_t for details.
 *
 * @author Sam Baskinger <sbaskinger@qualys.com>
 * @nosubgrouping
 */
#include "ironbee_config_auto.h"

#include <ironbee/mpool_freeable.h>

#include <ironbee/lock.h>

#include <assert.h>

#ifdef IB_MPOOL_VALGRIND
#include <valgrind/memcheck.h>
#endif

/**
 * Size of redzones to place between allocations.
 *
 * If this is non-zero, then:
 * - A gap of this size will be placed before and after the allocation.  This
 *   effectively means that a page needs 2*IB_MPOOL_FREEABLE_REDZONE_SIZE more bytes
 *   available than usual to fulfill an allocation.
 * - If IB_MPOOL_VALGRIND is defined then these gaps are marked as red zones
 *   for valgrind.
 *
 * Note that the cost of a single redzone is added to each allocation for the
 * purpose of bytes in use and ib_mpool_analyze().
 */
#ifdef IB_MPOOL_VALGRIND
#define IB_MPOOL_FREEABLE_REDZONE_SIZE 8
#else
#define IB_MPOOL_FREEABLE_REDZONE_SIZE 0
#endif

/**
 * The number of tracks.
 *
 * Defines the number of tracks for tracking small allocations.  The smallest
 * track will handle allocations up to 2^IB_MPOOL_FREEABLE_TRACK_ZERO_SIZE, the next
 * track will be handle allocations up to double that limit that are too
 * large for the smallest track, i.e., 2^NUM_TRACKS+1 to
 * 2^(NUM_TRACKS+1).
 *
 * With IB_MPOOL_FREEABLE_TRACK_ZERO_SIZE, this macro defines what a small allocation
 * is, i.e., up to 2^(IB_MPOOL_FREEABLE_TRACK_ZERO_SIZE+NUM_TRACKS-1).
 *
 * Increasing this number, and hence the small allocation limit, can
 * significantly improve performance if it means many more allocations are
 * now small.  However, it also increases the amount of memory used and
 * wasted.
 *
 * @sa IB_MPOOL_FREEABLE_TRACK_ZERO_SIZE
 **/
#define NUM_TRACKS 4

#define IB_MP_FRBLE_DEFAULT_PAGE_SIZE 4096

/**
 * A single point in memory for us to return when a zero length buffer is
 * returned.
 */
static char s_zero_length_buffer[1];

//! Segment cleanup function signature.
typedef struct segment_cleanup_t segment_cleanup_t;

//! Pool cleanup function signature.
typedef struct pool_cleanup_t pool_cleanup_t;

//! Tiny allocation record.
typedef struct tiny_allocation_t tiny_allocation_t;

/**
 * Description of an large allocation.
 *
 * This header allows any segment to self-describe as well as
 * find the next allocation made from the memory pool.
 *
 * Segments headers are organized into a circularly linked list
 * with `next` pointing to the next allocation.
 *
 * When a segment is actually deallocated and destroyed, it is
 * removed from the circular list. When a new segment is created,
 * it is added to the list.
 *
 * A circularly linked list was chosen so deallocation can
 * remove segment from a list without having to compute the
 * preceeding segment.
 */
struct ib_mpool_freeable_segment_t {
    ib_mpool_freeable_t         *mp;         /**< Memory pool. */
    size_t                       references; /**< References to this. */
    size_t                       size;       /**< Size of allocation. */
    segment_cleanup_t           *cleanup;    /**< Linked lst of cleanup fns. */
    ib_mpool_freeable_segment_t *next;       /**< Ptr to the next alloc. */
    ib_mpool_freeable_segment_t *prev;       /**< Ptr to the prev alloc. */
};

/**
 * A tiny allocation is a segment and some data used to allocate small sizes.
 *
 * This structure is used for optimizing small allocations from this
 * memory pool. Small allocations are shared out of a common
 * segment and freed from that common segment.
 *
 * This prevents excessive waste of header data for small allocations.
 */
struct tiny_allocation_t {
    size_t             references; /**< How many references to this. */
    size_t             size;       /**< Size of allocation. */
    size_t             allocated;  /**< Amount of segment that is allocated. */
    segment_cleanup_t *cleanup;    /**< Linked list of cleanup functions. */
    tiny_allocation_t *next;       /**< Next allocation. */
    char               alloc;      /**< 1st byte of allocation. */
};

/**
 * Linked list node used to cleanup just before a segment is freed.
 */
struct segment_cleanup_t {
    ib_mpool_freeable_segment_cleanup_fn_t  fn;     /**< Cleanup function. */
    void                                   *cbdata; /**< Callback data. */
    segment_cleanup_t                      *next;   /**< Next function. */
};

/**
 * Linked list node used to cleanup just before a pool is destroyed.
 */
struct pool_cleanup_t {
    ib_mpool_freeable_cleanup_fn_t  fn;     /**< Cleanup function. */
    void                           *cbdata; /**< Callback data. */
    pool_cleanup_t                 *next;   /**< Next function. */
};

/**
 * Memory pool.
 */
struct ib_mpool_freeable_t {
    ib_lock_t         *mutex;                   /**< Mutex for threading. */
    pool_cleanup_t    *cleanup;                 /**< Cleanup functions. */
    ib_mpool_freeable_segment_t  *segment_list; /**< List of segments. */
    tiny_allocation_t *tracks[NUM_TRACKS];      /**< Allocaton tracks. */
};


/**
 * The size of track zero; actually log2 of size in bytes.
 *
 * Track zero will hold all allocations up to 2^IB_MPOOL_FREEABLE_TRACK_ZERO_SIZE.
 * Subsequent tracks will each double the limit of the previous track (see
 * NUM_TRACKS for further discussion).
 *
 * If this number is too large, then track zero will cover too wide a range,
 * leading to increased waste.  If it is too small, then the lower tracks
 * will be underutilized, also leading to waste.
 *
 * @sa NUM_TRACKS
 **/
#define IB_MPOOL_FREEABLE_TRACK_ZERO_SIZE 5

/**@}*/

/* Basic Sanity Check -- Otherwise track number calculation fails. */
#if NUM_TRACKS - IB_MPOOL_FREEABLE_TRACK_ZERO_SIZE > 32
#error "NUM_TRACKS - IB_MPOOL_FREEABLE_TRACK_ZERO_SIZE > 32"
#endif

/**
 * The minimum page size.
 *
 * Any page size smaller than this will be changed to this.
 *
 * This macro is calculated from IB_MPOOL_FREEABLE_TRACK_ZERO_SIZE and
 * NUM_TRACKS.  Do not change it.
 **/
#define IB_MPOOL_FREEABLE_TINYALLOC_MAX_PAGESIZE \
     (1 << \
        (IB_MPOOL_FREEABLE_TRACK_ZERO_SIZE + NUM_TRACKS - 1))


/**
 * Calculate the track number for an allocation of size @a size.
 *
 * @param[in] size Size of allocation in bytes.
 * @returns Track number to allocate from or NUM_TRACKS if a large
 *          allocation.
 **/
static
size_t compute_track_number(size_t size)
{
    if (size > IB_MPOOL_FREEABLE_TINYALLOC_MAX_PAGESIZE) {
        return NUM_TRACKS;
    }

    /* Subtract 1 from size so that the most significant bit can tell us the
     * track number.  We want tracks to go up to a power of 2 so that it is
     * possible for a page (also a power of 2) to be completely filled.
     *
     * By the previous check, we know we can store this in a 32 bit word,
     * which will help us find the index of the most significant bit.
     */
    uint32_t v = (size - 1) >> (IB_MPOOL_FREEABLE_TRACK_ZERO_SIZE - 1);
    uint32_t r = 0;

    if (v == 0) {
        return 0;
    }

    /* Special thanks to Sean Anderson for this code (public domain):
     * http://graphics.stanford.edu/~seander/bithacks.html#IntegerLogObvious
     */
    {
        static const char LogTable256[256] =
        {
/**@cond DoNotDocument*/
#define LT(n) n, n, n, n, n, n, n, n, n, n, n, n, n, n, n, n
            -1, 0, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3,
            LT(4), LT(5), LT(5), LT(6), LT(6), LT(6), LT(6),
            LT(7), LT(7), LT(7), LT(7), LT(7), LT(7), LT(7), LT(7)
/**@endcond*/
#undef LT
        };

        unsigned int t, tt; // temporaries

        if ((tt = v >> 16))
        {
          r = (t = tt >> 8) ? 24 + LogTable256[t] : 16 + LogTable256[tt];
        }
        else
        {
          r = (t = v >> 8) ? 8 + LogTable256[t] : LogTable256[v];
        }
    }

    return r;
}

/**
 * Create a segment with a reference count of 1.
 *
 * The segment is also added to @a mp's segment list.
 *
 * @param[out] segment The segment is put here.
 * @param[in] mp The memory pool that this segment will be a member of.
 * @param[in] sz The size of this segment.
 *
 * @return
 * - IB_OK On success.
 * - IB_EALLOC On allocation failures.
 * - Other on lock creation failures.
 */
static ib_status_t segment_create(
    ib_mpool_freeable_segment_t **segment,
    ib_mpool_freeable_t          *mp,
    size_t                        sz
)
{
    assert(segment != NULL);
    assert(mp != NULL);

    ib_status_t       rc;
    ib_mpool_freeable_segment_t *s =
        malloc(sizeof(*s) + sz + IB_MPOOL_FREEABLE_REDZONE_SIZE);

    if (s == NULL) {
        return IB_EALLOC;
    }

#ifdef IB_MPOOL_VALGRIND
    VALGRIND_MEMPOOL_ALLOC(mp, s, sizeof(*s) + sz);
#endif

    rc = ib_lock_lock(mp->mutex);
    if (rc != IB_OK) {
        goto failure;
    }

    s->mp         = mp;
    s->references = 1;
    s->size       = sz;
    s->cleanup    = NULL;

    if (mp->segment_list == NULL) {
        s->next = s;
        s->prev = s;
    }
    else {
        s->next = mp->segment_list;
        s->prev = mp->segment_list->prev;
        s->next->prev = s;
        s->prev->next = s;
    }

    mp->segment_list = s;

    *segment = s;

    rc = ib_lock_unlock(mp->mutex);
    if (rc != IB_OK) {
        /* Don't go to failure. Just report the error. */
        return rc;
    }

    return IB_OK;

failure:

    free(s);
#ifdef IB_MPOOL_VALGRIND
    VALGRIND_MEMPOOL_FREE(mp, s);
#endif
    return rc;
}

/**
 * Internal destroy function.
 *
 * This removes the segment from the memory pool's segment list.
 * If @a seg is the last segment in the list, then the segment list is set
 * to NULL.
 *
 * @param[in] mp Memory pool.
 * @param[in] seg Segment to destroy.
 */
static void segment_destroy(
    ib_mpool_freeable_t         *mp,
    ib_mpool_freeable_segment_t *seg
)
{
    assert(mp != NULL);
    assert(seg != NULL);

    /* Remove this node from the list. */
    seg->prev->next = seg->next;
    seg->next->prev = seg->prev;

    /* Update the segment list if seg is the current head. */
    if (mp->segment_list == seg) {
        /* If seg points to itself, then removing it results in a NULL list. */
        mp->segment_list = (seg != seg->next)? seg->next : NULL;
    }

    /* Execute all cleanup functions. */
    for (
        segment_cleanup_t *cleanup = seg->cleanup;
        cleanup != NULL;
    )
    {
        segment_cleanup_t *tmp = cleanup;
        cleanup->fn(cleanup->cbdata);
        cleanup = cleanup->next;
        free(tmp);
    }

    memset(seg, 0, sizeof(*seg));
    free(seg);

#ifdef IB_MPOOL_VALGRIND
    VALGRIND_MEMPOOL_FREE(mp, seg);
#endif
}

/**
 * Given a pointer to ib_mpool_freeable_segment_t::alloc return the struct.
 *
 * @param[in] alloc The pointer to ib_mpool_freeable_segment_t::alloc.
 *
 * @returns @ref ib_mpool_freeable_segment_t that contains @a alloc.
 */
static ib_mpool_freeable_segment_t * segment_void_to_t(
    void *alloc
)
{
    assert(alloc != NULL);

    char *c = (char *) alloc;

    ib_mpool_freeable_segment_t *t = NULL;

    t = (ib_mpool_freeable_segment_t *)(c - sizeof(*t));

    return t;
}

/**
 * Create a new tiny allocation and inserts it in the proper track.
 *
 * This creates a tiny allocation with a reference count of 1.
 *
 * The memory pool must be locked before this is called.
 *
 */
static ib_status_t tiny_allocation_create(
    tiny_allocation_t   **tiny_allocation,
    ib_mpool_freeable_t  *mp,
    size_t                track_number
)
{
    assert(tiny_allocation != NULL);
    assert(mp != NULL);

    tiny_allocation_t *t;

    t = malloc(sizeof(*t) + IB_MP_FRBLE_DEFAULT_PAGE_SIZE + IB_MPOOL_FREEABLE_REDZONE_SIZE);
    if (t == NULL) {
        return IB_EALLOC;
    }

    /* Insert the tiny allocation. */
    t->references            = 1;
    t->size                  = IB_MP_FRBLE_DEFAULT_PAGE_SIZE;
    t->allocated             = 0;
    t->cleanup               = NULL;
    t->next                  = mp->tracks[track_number];
    mp->tracks[track_number] = t;

    *tiny_allocation = t;

    return IB_OK;
}

/**
 * Destroy a tiny allocation page, running the callback functions.
 *
 * @param[in] tiny_allocation The tiny allocaton to remove.
 * @param[in] mp Memory pool.
 * @param[in] track_number The number of the track this segment is a member of.
 * @param[in] prev_allocation The allocation whose `next` value
 *            points to @a tiny_allocation. NULL if this is the
 *            first value in the list.
 */
static void tiny_allocation_destroy(
    tiny_allocation_t   *tiny_allocation,
    ib_mpool_freeable_t *mp,
    size_t               track_number,
    tiny_allocation_t   *prev_allocation
)
{
    assert(tiny_allocation != NULL);
    assert(mp != NULL);

    if (prev_allocation == NULL) {
        mp->tracks[track_number] = tiny_allocation->next;
    }
    else {
        prev_allocation->next = tiny_allocation->next;
    }

    /* Execute all cleanup functions. */
    for (
        segment_cleanup_t *cleanup = tiny_allocation->cleanup;
        cleanup != NULL;
    )
    {
        segment_cleanup_t *tmp = cleanup;
        cleanup->fn(cleanup->cbdata);
        cleanup = cleanup->next;
        free(tmp);
    }

    free(tiny_allocation);
}

/**
 * Find the allocation in the list that contains the memory pointer.
 *
 * This returns the allocation that contains the pointer in @a found
 * and the preceeding allocation (tiny_allocation_t::next points to
 * @a found). This is to support deleting an allocation.
 *
 * If a segment is not found, IB_ENOENT is returned and @a found and
 * @a prev are unchanged.
 *
 * @param[in] mp The memory pool.
 * @param[in] addr The address.
 * @param[out] found The page found.
 * @param[out] prev The preceeding page. Used for deleting pages from lists.
 * @param[out] track_number The track the page was found in.
 *
 * @returns
 * - IB_OK When the alocation is found.
 * - IB_ENOENT If the memory segment could not be found.
 */
static ib_status_t tiny_allocation_find_mem(
    ib_mpool_freeable_t  *mp,
    void                 *addr,
    tiny_allocation_t   **found,
    tiny_allocation_t   **prev,
    size_t               *track_number
)
{
    assert(mp != NULL);
    assert(addr != NULL);
    assert(found != NULL);
    assert(prev != NULL);
    assert(track_number != NULL);

    for (int tn = 0; tn < NUM_TRACKS; ++tn)
    {
        /* Get the start of a track. */
        tiny_allocation_t *ta  = mp->tracks[tn];
        tiny_allocation_t *pta = NULL;

        /* While there is stuff. */
        while (ta != NULL) {
            /* If addr is in the range of this page, output and return. */
            if (
                addr >= (void *)(&ta->alloc) &&
                addr <  (void *)(&ta->alloc + ta->allocated)
            )
            {
                *found        = ta;
                *prev         = pta;
                *track_number = tn;
                return IB_OK;
            }

            /* Advance... */
            pta = ta;
            ta  = ta->next;
        }
    }

    return IB_ENOENT;
}

ib_status_t ib_mpool_freeable_create(ib_mpool_freeable_t **mp)
{
    assert(mp != NULL);

    ib_status_t          rc;
    ib_mpool_freeable_t *tmp_mp;

    tmp_mp = malloc(sizeof(*tmp_mp));
    if (tmp_mp == NULL) {
        return IB_EALLOC;
    }

    rc = ib_lock_create_malloc(&tmp_mp->mutex);
    if (rc != IB_OK) {
        free(tmp_mp);
        return rc;
    }

    /* Zero all tracks. */
    for (int i = 0; i < NUM_TRACKS; ++i) {
        tmp_mp->tracks[i] = NULL;
    }

    tmp_mp->segment_list = NULL;
    tmp_mp->cleanup = NULL;
    *mp = tmp_mp;

#ifdef IB_MPOOL_VALGRIND
    VALGRIND_CREATE_MEMPOOL(tmp_mp, IB_MPOOL_FREEABLE_REDZONE_SIZE, 0);
#endif

    return IB_OK;
}

static void* tiny_alloc(
    ib_mpool_freeable_t *mp,
    size_t               size,
    size_t               track_number
)
{
    assert(mp != NULL);

    void              *alloc = NULL;
    tiny_allocation_t *tiny_allocation;
    ib_status_t        rc;

    rc = ib_lock_lock(mp->mutex);
    if (rc != IB_OK) {
        return NULL;
    }

    /* If there is no space in the specified track.
     * Note that we ONLY allocate out of the head of the list. When that
     * head is full, we push an new, empty segment of memory onto the
     * front of the stack. */
    if (
        (mp->tracks[track_number] == NULL) ||

        (mp->tracks[track_number]->size - mp->tracks[track_number]->allocated)
            < (size + IB_MPOOL_FREEABLE_REDZONE_SIZE)
    )
    {
        rc = tiny_allocation_create(&tiny_allocation, mp, track_number);
        if (rc != IB_OK) {
            alloc = NULL;
            goto exit_label;
        }
    }
    /* Allocate space. */
    else {
        /* Grab the page with space. */
        tiny_allocation = mp->tracks[track_number];

        /* Increment the reference count. */
        ++(tiny_allocation->references);
    }

    /* Compute the start of the next allocation.
     * This is the value we will return. */
    alloc =
        ((void *)(tiny_allocation)) +
        sizeof(*tiny_allocation) +
        tiny_allocation->allocated;

    /* Having computed the start, allocate the size. */
    tiny_allocation->allocated += (size + IB_MPOOL_FREEABLE_REDZONE_SIZE);

exit_label:
    ib_lock_unlock(mp->mutex);
    return alloc;
}

void* ib_mpool_freeable_alloc(ib_mpool_freeable_t *mp, size_t size)
{
    assert(mp != NULL);

    size_t  track_number;
    void   *alloc;

    if (size == 0) {
        return &s_zero_length_buffer;
    }

    track_number = compute_track_number(size);

    if (track_number < NUM_TRACKS) {
        alloc = tiny_alloc(mp, size, track_number);
    }
    else {
        ib_status_t                  rc;
        ib_mpool_freeable_segment_t *seg;

        rc = segment_create(&seg, mp, size);

        alloc = (rc != IB_OK)? NULL : ib_mpool_freeable_segment_ptr(seg);
    }

    return alloc;
}

ib_status_t ib_mpool_freeable_ref(
    ib_mpool_freeable_t *mp,
    void                *segment
)
{
    assert(mp != NULL);

    ib_status_t        rc;
    tiny_allocation_t *tiny_allocation;
    tiny_allocation_t *tiny_allocation_prev;
    size_t             track_number;

    if (segment == NULL) {
        return IB_OK;
    }

    if (segment == &s_zero_length_buffer) {
        return IB_OK;
    }

    rc = ib_lock_lock(mp->mutex);
    if (rc != IB_OK) {
        return rc;
    }

    /* Is this a small allocation? */
    rc = tiny_allocation_find_mem(
        mp,
        segment,
        &tiny_allocation,
        &tiny_allocation_prev,
        &track_number
    );
    /* We found it! */
    if (rc == IB_OK) {
        ++(tiny_allocation->references);
        goto exit_label;
    }
    else {
        ib_mpool_freeable_segment_t  *seg;

        /* If segment is not a constant known value, assign to seg the pointer
         * that is before the given pointer, ->segment. */
        seg = segment_void_to_t(segment);

        /* We cannot be sure this mp allocated this page. */
        if (seg->mp != mp) {
            rc = IB_EINVAL;
            goto exit_label;
        }

        ++(seg->references);
    }

exit_label:
    ib_lock_unlock(mp->mutex);

    return rc;
}

void ib_mpool_freeable_free(
    ib_mpool_freeable_t *mp,
    void                *segment
)
{
    assert(mp != NULL);

    ib_status_t        rc;
    tiny_allocation_t *tiny_allocation;
    tiny_allocation_t *tiny_allocation_prev;
    size_t             track_number;

    if (segment == NULL) {
        return;
    }

    if (segment == &s_zero_length_buffer) {
        return;
    }

    rc = ib_lock_lock(mp->mutex);
    if (rc != IB_OK) {
        return;
    }

    /* Is this a small allocation? */
    rc = tiny_allocation_find_mem(
        mp,
        segment,
        &tiny_allocation,
        &tiny_allocation_prev,
        &track_number
    );
    /* We found it! */
    if (rc == IB_OK) {

        /* One less reference to this segment. */
        --(tiny_allocation->references);

        /* Free it! */
        if (tiny_allocation->references == 0) {
            tiny_allocation_destroy(
                tiny_allocation,
                mp,
                track_number,
                tiny_allocation_prev
            );
        }

        goto exit_label;
    }
    else if (rc == IB_ENOENT) {
        ib_mpool_freeable_segment_t  *seg;

        /* If we wind up here we are dealing with a large allocation. */
        seg = segment_void_to_t(segment);

        /* We cannot be sure this mp allocated this page. */
        if (seg->mp != mp) {
            goto exit_label;
        }

        --(seg->references);
        if (seg->references == 0) {
            segment_destroy(mp, seg);
        }

        goto exit_label;
    }

exit_label:
    ib_lock_unlock(mp->mutex);
}

ib_status_t ib_mpool_freeable_register_cleanup(
    ib_mpool_freeable_t            *mp,
    ib_mpool_freeable_cleanup_fn_t  fn,
    void                           *cbdata
)
{
    assert(mp != NULL);

    ib_status_t     rc;
    pool_cleanup_t *cleanup;

    rc = ib_lock_lock(mp->mutex);
    if (rc != IB_OK) {
        return IB_EOTHER;
    }

    cleanup = malloc(sizeof(*cleanup));
    if (cleanup == NULL) {
        ib_lock_unlock(mp->mutex);
        return IB_EALLOC;
    }

    cleanup->fn     = fn;
    cleanup->cbdata = cbdata;
    cleanup->next   = mp->cleanup;
    mp->cleanup     = cleanup;

    rc = ib_lock_unlock(mp->mutex);
    if (rc != IB_OK) {
        return IB_EOTHER;
    }

    return IB_OK;
}

ib_status_t ib_mpool_freeable_alloc_register_cleanup(
    ib_mpool_freeable_t                    *mp,
    void                                   *segment,
    ib_mpool_freeable_segment_cleanup_fn_t  fn,
    void                                   *cbdata
)
{
    assert(mp != NULL);

    ib_mpool_freeable_segment_t   *seg;
    ib_status_t         rc;
    segment_cleanup_t  *cleanup;
    tiny_allocation_t  *tiny_allocation;
    tiny_allocation_t  *tiny_allocation_prev;
    size_t              track_number;
    segment_cleanup_t **cleanup_fn;

    if (segment == NULL) {
        return IB_EINVAL;
    }

    if (segment == &s_zero_length_buffer) {
        return IB_EINVAL;
    }

    rc = ib_lock_lock(mp->mutex);
    if (rc != IB_OK) {
        return IB_EOTHER;
    }

    /* Is this a small allocation? */
    rc = tiny_allocation_find_mem(
        mp,
        segment,
        &tiny_allocation,
        &tiny_allocation_prev,
        &track_number
    );

    /* We found it! */
    if (rc == IB_OK) {
        cleanup_fn = &tiny_allocation->cleanup;
    }
    else if (rc == IB_ENOENT) {
        /* If segment is not a constant known value, assign to seg the pointer
         * that is before the given pointer, ->segment. */
        seg = segment_void_to_t(segment);

        /* We cannot be sure this mp allocated this page. */
        if (seg->mp != mp) {
            rc = IB_EINVAL;
            goto failure;
        }

        cleanup_fn = &seg->cleanup;
    }
    else {
        rc = IB_EOTHER;
        goto failure;
    }

    cleanup = malloc(sizeof(*cleanup));
    if (cleanup == NULL) {
        rc = IB_EALLOC;
        goto failure;
    }

    cleanup->fn     =  fn;
    cleanup->cbdata =  cbdata;
    cleanup->next   = *cleanup_fn;
    *cleanup_fn     =  cleanup;

failure:
    ib_lock_unlock(mp->mutex);

    return rc;
}

void ib_mpool_freeable_destroy(ib_mpool_freeable_t *mp)
{
    assert(mp != NULL);

    /* Destroy all remaining segments. */
    while (mp->segment_list != NULL) {
        segment_destroy(mp, mp->segment_list);
    }

    /* Destroy all small allocation tracks. */
    for (int track_number = 0; track_number < NUM_TRACKS; ++track_number) {

        /* While there are members in the track, destroy the head.
         * The enclosing for-loop does this to each track. */
        while (mp->tracks[track_number] != NULL) {
            tiny_allocation_destroy(
                mp->tracks[track_number],
                mp,
                track_number,
                NULL
            );
        }
    }

    /* Execute all memory pool-level cleanup functions. */
    for (
        pool_cleanup_t *cleanup = mp->cleanup;
        cleanup != NULL;
    )
    {
        pool_cleanup_t *tmp = cleanup;
        cleanup->fn(cleanup->cbdata);
        cleanup = cleanup->next;
        free(tmp);
    }

    ib_lock_destroy_malloc(mp->mutex);
    free(mp);

#ifdef IB_MPOOL_VALGRIND
    VALGRIND_DESTROY_MEMPOOL(mp);
#endif
}


ib_mpool_freeable_segment_t * ib_mpool_freeable_segment_alloc(
    ib_mpool_freeable_t *mp,
    size_t size
)
{
    assert(mp != NULL);

    ib_mpool_freeable_segment_t *seg;
    ib_status_t                  rc;

    rc = segment_create(&seg, mp, size);
    if (rc != IB_OK) {
        return NULL;
    }

    return seg;
}

void ib_mpool_freeable_segment_free(
    ib_mpool_freeable_t         *mp,
    ib_mpool_freeable_segment_t *segment
)
{
    assert(mp!= NULL);
    assert(segment!= NULL);

    ib_status_t rc;

    /* Check if we should destroy this segment. */
    rc = ib_lock_lock(mp->mutex);
    if (rc != IB_OK) {
        return;
    }

    --(segment->references);
    if (segment->references == 0) {
        segment_destroy(mp, segment);
    }

    ib_lock_unlock(mp->mutex);
}

ib_status_t ib_mpool_freeable_segment_ref(
    ib_mpool_freeable_t         *mp,
    ib_mpool_freeable_segment_t *segment
)
{
    assert(mp!= NULL);
    assert(segment!= NULL);

    ib_status_t rc;

    /* Check if we should destroy this segment. */
    rc = ib_lock_lock(mp->mutex);
    if (rc != IB_OK) {
        return rc;
    }

    ++(segment->references);

    ib_lock_unlock(mp->mutex);
    return rc;
}

void *ib_mpool_freeable_segment_ptr(
    ib_mpool_freeable_segment_t *segment
)
{
    assert(segment != NULL);

    return ((char *)segment) + sizeof(*segment);
}

ib_status_t ib_mpool_freeable_segment_register_cleanup(
    ib_mpool_freeable_t                    *mp,
    ib_mpool_freeable_segment_t            *segment,
    ib_mpool_freeable_segment_cleanup_fn_t  fn,
    void                                   *cbdata
)
{
    assert(mp != NULL);
    assert(segment!= NULL);

    ib_status_t        rc;
    segment_cleanup_t *cleanup;

    rc = ib_lock_lock(mp->mutex);
    if (rc != IB_OK) {
        return IB_EOTHER;
    }

    if (segment->mp != mp) {
        rc = IB_EINVAL;
        goto failure;
    }

    cleanup = malloc(sizeof(*cleanup));
    if (cleanup == NULL) {
        rc = IB_EALLOC;
        goto failure;
    }

    cleanup->fn      =  fn;
    cleanup->cbdata  =  cbdata;
    cleanup->next    = segment->cleanup;
    segment->cleanup = cleanup;

failure:
    ib_lock_unlock(mp->mutex);

    return rc;}


/**@}*/
