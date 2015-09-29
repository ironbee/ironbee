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
 * @brief IronBee --- Memory Pool Implementation
 *
 * See ib_mpool_t for details.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 * @nosubgrouping
 */

#include "ironbee_config_auto.h"

#include <ironbee/mpool.h>

#include <ironbee/lock.h>

#ifdef IB_MPOOL_VALGRIND
#include <valgrind/memcheck.h>
#endif

#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>


/**
 * A single point in memory for us to return when a zero length buffer is
 * returned.
 */
static char s_zero_length_buffer[1];

/**
 * @name Memory Pool Configuration
 *
 * Adjusting the values of these macros can significantly change the time and
 * space performance of memory pool.  See ib_mpool_analyze().
 */
/**@{*/

/**
 * Size of redzones to place between allocations.
 *
 * If this is non-zero, then:
 * - A gap of this size will be placed before and after the allocation.  This
 *   effectively means that a page needs 2*IB_MPOOL_REDZONE_SIZE more bytes
 *   available than usual to fulfill an allocation.
 * - If IB_MPOOL_VALGRIND is defined then these gaps are marked as red zones
 *   for valgrind.
 *
 * Note that the cost of a single redzone is added to each allocation for the
 * purpose of bytes in use and ib_mpool_analyze().
 */
#ifdef IB_MPOOL_VALGRIND
#define IB_MPOOL_REDZONE_SIZE 8
#else
#define IB_MPOOL_REDZONE_SIZE 0
#endif

/**
 * Default Page Size in bytes.
 *
 * Used by ib_mpool_create().  Large page sizes can mean faster allocation,
 * clearing, and destruction (especially the latter two), but can also mean
 * more memory waste.
 *
 * @sa ib_mpool_create()
 * @sa IB_MPOOL_POINTER_PAGE_SIZE
 **/
#define IB_MPOOL_DEFAULT_PAGE_SIZE 4096

/**
 * Pointer Page Size in pointers.
 *
 * Large allocations are passed directly to malloc and the pointer stored in
 * a special pointer page.  This macro defines the number of pointers per
 * pointer page and has a similar effect as IB_MPOOL_POINTER_PAGE_SIZE.
 *
 * If you have few large allocations, this can be made small to save memory.
 * If you have many large allocations, this can be made large to improve
 * performance.
 *
 * @sa IB_MPOOL_DEFAULT_PAGE_SIZE
 **/
#define IB_MPOOL_POINTER_PAGE_SIZE \
     (IB_MPOOL_DEFAULT_PAGE_SIZE / sizeof(void *))

/**
 * The number of tracks.
 *
 * Defines the number of tracks for tracking small allocations.  The smallest
 * track will handle allocations up to 2^IB_MPOOL_TRACK_ZERO_SIZE, the next
 * track will be handle allocations up to double that limit that are too
 * large for the smallest track, i.e., 2^IB_MPOOL_NUM_TRACKS+1 to
 * 2^(IB_MPOOL_NUM_TRACKS+1).
 *
 * With IB_MPOOL_TRACK_ZERO_SIZE, this macro defines what a small allocation
 * is, i.e., up to 2^(IB_MPOOL_TRACK_ZERO_SIZE+IB_MPOOL_NUM_TRACKS-1).
 *
 * Increasing this number, and hence the small allocation limit, can
 * significantly improve performance if it means many more allocations are
 * now small.  However, it also increases the amount of memory used and
 * wasted.
 *
 * If you have many memory pools, consider lowering this number.  If you have
 * very few, raise it significantly.
 *
 * @sa IB_MPOOL_TRACK_ZERO_SIZE
 **/
#define IB_MPOOL_NUM_TRACKS 10

/**
 * The size of track zero; actually log2 of size in bytes.
 *
 * Track zero will hold all allocations up to 2^IB_MPOOL_TRACK_ZERO_SIZE.
 * Subsequent tracks will each double the limit of the previous track (see
 * IB_MPOOL_NUM_TRACKS for further discussion).
 *
 * If this number is too large, then track zero will cover too wide a range,
 * leading to increased waste.  If it is too small, then the lower tracks
 * will be underutilized, also leading to waste.
 *
 * @sa IB_MPOOL_NUM_TRACKS
 **/
#define IB_MPOOL_TRACK_ZERO_SIZE 8

/**@}*/

/* Basic Sanity Check -- Otherwise track number calculation fails. */
#if IB_MPOOL_NUM_TRACKS - IB_MPOOL_TRACK_ZERO_SIZE > 32
    #error "IB_MPOOL_NUM_TRACKS - IB_MPOOL_TRACK_ZERO_SIZE > 32"
#endif

/* Structures */

/** See struct ib_mpool_page_t */
typedef struct ib_mpool_page_t ib_mpool_page_t;
/** See struct ib_mpool_pointer_page_t */
typedef struct ib_mpool_pointer_page_t ib_mpool_pointer_page_t;
/** See struct ib_mpool_cleanup_t */
typedef struct ib_mpool_cleanup_t ib_mpool_cleanup_t;

/**
 * A page to hold small allocations.
 *
 * Small allocations are taken from pages.  Pages are stored in tracks that
 * determine the range of allocations that can be taken from them.
 *
 * @sa ib_mpool_t
 */
struct ib_mpool_page_t
{
    /** Next page in track. */
    ib_mpool_page_t *next;
    /** Number of bytes used. */
    size_t used;
    /** If this was a sub-allocation, then what was the parent. */
    ib_mpool_page_t *parent_page;
    /**
     * First byte of page.
     *
     * Enough additional memory is allocated so that @c &page is the
     * beginning of an allocated region of memory page size bytes long.
     **/
    char page;
};

/**
 * A page to hold pointers to large allocations.
 *
 * Large allocations are directly allocated via malloc.  Pointers to these
 * allocations are stored in pointer pages.
 *
 * @sa ib_mpool_t
 **/
struct ib_mpool_pointer_page_t
{
    /** Next pointer page. */
    ib_mpool_pointer_page_t *next;
    /** Index in @c pointers of next free pointer. */
    size_t next_pointer;
    /** Pointers */
    void *pointers[IB_MPOOL_POINTER_PAGE_SIZE];
};

/**
 * A cleanup function.
 *
 * Cleanup functions are stored in a singly linked list.
 *
 * @note This implementation is simple but assumes that cleanup functions are
 * relatively rare.  If cleanup functions become frequent, a pointer page
 * style approach may worthwhile.
 **/
struct ib_mpool_cleanup_t {
    /** Next cleanup function. */
    ib_mpool_cleanup_t    *next;
    /** Function to call. */
    ib_mpool_cleanup_fn_t  function;
    /** Data to pass to function. */
    void                  *function_data;
};

/**
 * A memory pool.
 *
 * Allocations are handled in two cases.  Smaller allocations are allocated
 * from larger pages, while larger allocations are directly malloced.  This
 * approach reduces the number of mallocs() and frees() needed for small
 * allocations while maintaining simple data structures and algorithms.  In
 * contrast, handling all sizes via pages and tracks would require a variable
 * number of tracks and a variable pagesize, both of which would greatly
 * complicate the code.
 *
 * Pages for small allocations are stored in tracks.  Track zero handles
 * allocations of sizes 1 up to 2^IB_MPOOL_TRACK_ZERO_SIZE.  Track one handles
 * allocations that are too large for track zero and up to double the limit
 * of track zero.  And so on.  A large allocation is defined as one too large
 * to fit in the last track.
 *
 * For example, if IB_MPOOL_TRACK_ZERO_SIZE is 5 and IB_MPOOL_NUM_TRACKS is
 * 6, then:
 *
 * <table>
 * <tr><th>Track</th><th>Size Range</th></tr>
 * <tr><td>0</td>    <td>0..32</td></tr>
 * <tr><td>1</td>    <td>33..64</td></tr>
 * <tr><td>2</td>    <td>65..128</td></tr>
 * <tr><td>3</td>    <td>129..256</td></tr>
 * <tr><td>4</td>    <td>257..512</td></tr>
 * <tr><td>5</td>    <td>513..1024</td></tr>
 * </table>
 *
 * And large allocations are those of 1025 or more bytes.
 *
 * Each track is a singly linked list of pages.  All pages except the first
 * are guaranteed to have too few bytes remaining to satisfy a request.  E.g.,
 * for track 4 above, all pages except the first have less than 512 bytes
 * remaining.
 *
 * When a small allocation is called for, fulfilled it is as simple as
 * checking the first page (if any) on the track, creating and pushing a new
 * page if necessary, and then allocating from that page.  Allocating from a
 * page involves increasing its @c used counter and returning a pointer to the
 * appropriate location in the page.
 *
 * This approach was chosen because it allows for very fast allocations.  The
 * downside is that it can have significant wastage.  For example, in track 4
 * above, a request for less than 512 bytes might fit on a later page but the
 * implementation will not search for that case.  Wastage is ameliorated by
 * the use of multiple tracks.
 *
 * Large allocations are handled by calling malloc directly for the needed
 * number of bytes and storing the resulting pointer.  The pointer needs to be
 * stored so that it can be freed when the pool is cleared or destroyed.
 * Large allocations may be common, so to avoid two mallocs for every large
 * allocations (one to store the pointer and one for the allocation), pointers
 * are aggregated into pointer pages.
 *
 * When a pool is destroyed all pages, pointer pages, cleanup nodes, and the
 * pool itself are freed.  In contrast, when a pool is cleared, the pages,
 * pointer pages, and cleanup nodes are moved to free lists for future reuse.
 * Release functions similar to destroy except that if there is a parent pool
 * the pool is cleared and added to the free children list of the parent
 * pool.
 *
 * Finally, cleanup functions can be registered with a pool to be called on
 * clear or destroy.  It is assumed that these are relatively rare.  They are
 * thus stored in a simple singly linked list, with a new node being allocated
 * for each cleanup function.
 *
 * The implementation manages all of the memory it uses directly via malloc
 * and free.  It could, instead, use itself for cleanup nodes and (if small
 * enough) pointer pages.  It does not in order to better track client usage
 * vs its own usage.  This can be useful to developers in understanding
 * usage patterns.  See, e.g., ib_mpool_analyze().
 **/
struct ib_mpool_t
{
    /**
     * The name of the pool.
     *
     * This can be changed and accessed via ib_mpool_name() and
     * ib_mpool_setname().  It may also be NULL.  Its has two purposes:
     *
     * - To allow the client to attach a string to the pool.
     * - To be displayed in messages/reports from ib_mpool_validate(),
     *   ib_mpool_analyze(), and ib_mpool_debug_report().
     **/
    char *name;

    /**
     * The size of a page.
     *
     * This member can be set when the pool is created via
     * ib_mpool_create_ex().  It is the only configuration available to
     * clients.  That this, and not other configuration parameters, are at
     * runtime and available to the client is partially historical and
     * partially because page size is in the sweet spot of utility to the
     * client and ease of implementation.
     **/
    size_t pagesize;

    /**
     * Malloc function to use.
     *
     * ib_mpool_create() will use malloc(), ib_mpool_create_ex() allows the
     * client to pass in a pointer.
     **/
    ib_mpool_malloc_fn_t malloc_fn;

    /**
     * Free function to use.
     *
     * ib_mpool_create() will use free(), ib_mpool_create_ex() allows the
     * client to pass in a pointer.
     **/
    ib_mpool_free_fn_t free_fn;

    /**
     * Number of bytes allocated.
     *
     * The client can access this via ib_mpool_inuse().  It is also used by
     * ib_mpool_validate() and in reports.
     *
     * This member is the sum of all sizes allocated.  As such, it includes
     * large allocations but does not include the overhead memory of any
     * memory pool structures.
     **/
    size_t inuse;

    /**
     * Number of bytes allocated by large allocations.
     *
     * This is used by ib_mpool_validate() and in reports.
     **/
    size_t large_allocation_inuse;

    /**
     * The parent memory pool.
     **/
    ib_mpool_t *parent;

    /**
     * The next sibling.
     *
     * This pointer is considered to be part of the parent in terms of
     * locking.  I.e., to modify this, the parent should be locked.
     **/
    ib_mpool_t *next;

    /**
     * Singly linked list of all child pools.
     **/
    ib_mpool_t *children;
    /**
     * End of children list.
     **/
    ib_mpool_t *children_end;

    /**
     * Lock for multithreading support.
     *
     * This lock is only used in creation and destruction to allow
     * simultaneous creation/destruction of memory pools with a common
     * parent.  Both operations must modify the parents children list and
     * this lock protects it.
     **/
    ib_lock_t *lock;

    /**
     * Tracks of pages.
     *
     * @sa ib_mpool_t
     **/
    ib_mpool_page_t         *tracks[IB_MPOOL_NUM_TRACKS];
    /**
     * End of tracks.
     **/
    ib_mpool_page_t        *tracks_end[IB_MPOOL_NUM_TRACKS];
    /**
     * Singly linked list of pointers page for large allocations.
     *
     * @sa ib_mpool_t
     **/
    ib_mpool_pointer_page_t *large_allocations;
    /**
     * End of large allocations list.
     **/
    ib_mpool_pointer_page_t *large_allocations_end;
    /**
     * Singly linked list of cleanup functions.
     *
     * @sa ib_mpool_t
     **/
    ib_mpool_cleanup_t      *cleanups;
    /**
     * End of cleanup list.
     **/
    ib_mpool_cleanup_t      *cleanups_end;
    /**
     * Singly linked list of free pages.
     *
     * @sa ib_mpool_t
     **/
    ib_mpool_page_t         *free_pages;
    /**
     * Singly linked list of free pointer pages.
     *
     * @sa ib_mpool_t
     **/
    ib_mpool_pointer_page_t *free_pointer_pages;
    /**
     * Singly linked list of free cleanups.
     *
     * @sa ib_mpool_t
     **/
    ib_mpool_cleanup_t      *free_cleanups;
    /**
     * Singly linked list of free children.
     *
     * @sa ib_mpool_t
     **/
    ib_mpool_t              *free_children;
};

/**
 * @name Helper functions for many things.
 */
/**@{*/

/**
 * The minimum page size.
 *
 * Any page size smaller than this will be changed to this.
 *
 * This macro is calculated from IB_MPOOL_TRACK_ZERO_SIZE and
 * IB_MPOOL_NUM_TRACKS.  Do not change it.
 **/
#define IB_MPOOL_MINIMUM_PAGESIZE \
     (1 << (IB_MPOOL_TRACK_ZERO_SIZE + IB_MPOOL_NUM_TRACKS - 1))

/**
 * Loop through a singly linked allowing for mutation.
 *
 * This macro loops over a singly linked list defined by its first node,
 * @a list.  It provides a variable @a var of type @a vartype in the body,
 * that is set to each element in the list.
 *
 * An important property of this macro is that the next node is calculated
 * before the body of the loop is run.  This allows the node pointed to by
 * @a var to be safely changed or removed.
 *
 * @code
 * IB_MPOOL_FOREACH(ib_mpool_t, child, mp->children) {
 *   child->parent = NULL;
 *   ib_mpool_destroy(child);
 * }
 * @endcode
 *
 * @param[in] vartype The *base type* of a node of @a list.
 * @param[in] var     The *name* of a variable to a pointer to each node.
 * @param[in] list    The first node of the list to iterate over.
 **/
#define IB_MPOOL_FOREACH(vartype, var, list) \
    for ( \
        vartype *imf_next = NULL, *var = (list); \
        (var) != NULL && (imf_next = (var)->next, true); \
        (var) = imf_next \
    )

/**
 * The maximum size of an allocation for a page of track @a track_num.
 *
 * @param[in] track_num Track number.
 * @return Maximum size of allocation for track @a track_num.
 */
#define IB_MPOOL_TRACK_SIZE(track_num) \
    (1 << (IB_MPOOL_TRACK_ZERO_SIZE + (track_num)))

/**
 * Calculate the track number for an allocation of size @a size.
 *
 * @param[in] size Size of allocation in bytes.
 * @returns Track number to allocate from or IB_MPOOL_NUM_TRACKS if a large
 *          allocation.
 **/
static
size_t ib_mpool_track_number(size_t size)
{
    if (size > IB_MPOOL_MINIMUM_PAGESIZE) {
        return IB_MPOOL_NUM_TRACKS;
    }

    /* Subtract 1 from size so that the most significant bit can tell us the
     * track number.  We want tracks to go up to a power of 2 so that it is
     * possible for a page (also a power of 2) to be completely filled.
     *
     * By the previous check, we know we can store this in a 32 bit word,
     * which will help us find the index of the most significant bit.
     */
    uint32_t v = (size - 1) >> (IB_MPOOL_TRACK_ZERO_SIZE - 1);
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
 * Remove a child pool from a parent pools child list.
 *
 * The parent should usually be locked before calling this.
 *
 * @param[in] child Child to remove from parent pool.
 */
static
void ib_mpool_remove_child_from_parent(const ib_mpool_t *child)
{
    assert(child         != NULL);
    assert(child->parent != NULL);

    if (child->parent->children == child) {
        child->parent->children = child->next;
        if (child->next == NULL) {
            child->parent->children_end = NULL;
        }
    }
    else {
        /* Find node whose next node is child. */
        ib_mpool_t *before_child = child->parent->children;
        while (before_child->next != child) {
            before_child = before_child->next;
        }
        before_child->next = child->next;
        if (child->next == NULL) {
            child->parent->children_end = before_child;
        }
    }

    return;
}

/**@}*/

/**
 * @name Helper functions for managing internal memory.
 */
/**@{*/

/**
 * Allocate pages in a contiguous block.
 *
 * @param[in] mp Memory pool to allocate pages for.
 * @param[in] pages Number of pages to allocate (must be >0).
 * @return Uninitialized linked pages or NULL on allocation error.
 **/
static
ib_mpool_page_t *ib_mpool_alloc_pages(
    ib_mpool_t *mp,
    int pages
)
{
    assert(mp != NULL);
    assert(pages > 0);

    /* Allocate a block of memory to hold all pages.
     *
     * NOTE: Since the ib_mpool_page_t structure size is not
     *       the allocated size of the page (it is just the header)
     *       the actual allocated size is tracked and memory is
     *       tracked as a byte array so that the pointer math
     *       is based on bytes and not structure size.
     */
    size_t alloc_pagesize = sizeof(ib_mpool_page_t) + mp->pagesize - 1;
    uint8_t *block = mp->malloc_fn(alloc_pagesize * pages);
    if (block == NULL) {
        return NULL;
    }

    /* Define the parent page. */
    ib_mpool_page_t *parent_mpage = (ib_mpool_page_t *)block;
    parent_mpage->next = NULL;
    parent_mpage->parent_page = NULL;

    /* Iterate over each remaining page to link them together. */
    for (int i = 1; i < pages; ++i) {
        ib_mpool_page_t *mpage = (ib_mpool_page_t *)(block + (i * alloc_pagesize));
        ib_mpool_page_t *prev_mpage = (ib_mpool_page_t *)(block + ((i - 1) * alloc_pagesize));

#ifdef IB_MPOOL_VALGRIND
        int rc = VALGRIND_MAKE_MEM_NOACCESS(&(mpage->page), mp->pagesize);
        assert(rc < 2);
#endif

        /* Update links and track parent page. */
        mpage->next = NULL;
        mpage->parent_page = parent_mpage;
        prev_mpage->next = mpage;
    }

    return parent_mpage;
}

/**
 * Acquire a new page.
 *
 * Pops a page from the free list if available or allocates a new page if
 * not.  The page returned should be considered uninitialized.
 *
 * @param[in] mp Memory pool to acquire page for.
 * @return Uninitialized page or NULL on allocation error.
 **/
static
ib_mpool_page_t *ib_mpool_acquire_page(
    ib_mpool_t *mp
)
{
    assert(mp != NULL);

    ib_mpool_page_t *mpage = NULL;

    if (mp->free_pages != NULL) {
        mpage = mp->free_pages;
        mp->free_pages = mp->free_pages->next;
    }
    else {
        mpage = ib_mpool_alloc_pages(mp, 1);
    }

    return mpage;
}

/**
 * Acquire a new pointer page.
 *
 * Pops a pointer page from the free list if available or allocates a new
 * pointer page if not.  The pointer page returned should be considered
 * uninitialized.
 *
 * @param[in] mp Memory pool to acquire pointer page for.
 * @return Uninitialized pointer page or NULL on allocation error.
 **/
static
ib_mpool_pointer_page_t *ib_mpool_acquire_pointer_page(
    ib_mpool_t *mp
)
{
    assert(mp != NULL);

    ib_mpool_pointer_page_t *ppage = NULL;

    if (mp->free_pointer_pages != NULL) {
        ppage = mp->free_pointer_pages;
        mp->free_pointer_pages = mp->free_pointer_pages->next;
    }
    else {
        ppage = mp->malloc_fn(sizeof(*ppage));
    }

    return ppage;
}

/**
 * Acquire a new cleanup node.
 *
 * Pops a cleanup node from the free list if available or allocates a new
 * cleanup node if not.  The cleanup node returned should be considered
 * uninitialized.
 *
 * @param[in] mp Memory pool to acquire cleanup node for.
 * @return Uninitialized cleanup node or NULL on allocation error.
 **/
static
ib_mpool_cleanup_t *ib_mpool_acquire_cleanup(
    ib_mpool_t *mp
)
{
    assert(mp != NULL);

    ib_mpool_cleanup_t *cleanup = NULL;

    if (mp->free_cleanups != NULL) {
        cleanup = mp->free_cleanups;
        mp->free_cleanups = mp->free_cleanups->next;
    }
    else {
        cleanup = mp->malloc_fn(sizeof(*cleanup));
    }

    return cleanup;
}

/**@}*/

/**
 * @name Helper functions for ib_mpool_clear() and ib_mpool_destroy().
 */
/**@{*/

/**
 * Call free on every large allocation of @a mp.
 *
 * @param[in] mp Memory pool to free large allocations for.
 **/
static
void ib_mpool_free_large_allocations(ib_mpool_t *mp)
{
    assert(mp != NULL);

    IB_MPOOL_FOREACH(ib_mpool_pointer_page_t, ppage, mp->large_allocations) {
        for (
            size_t i = 0;
            i < IB_MPOOL_POINTER_PAGE_SIZE && ppage->pointers[i] != NULL;
            ++i
        ) {
            mp->free_fn(ppage->pointers[i]);
        }
    }

    return;
}

/**
 * Call every cleanup function for @a mp.
 *
 * @param[in] mp Memory pool to call cleanups for.
 **/
static
void ib_mpool_call_cleanups(const ib_mpool_t *mp)
{
    assert(mp != NULL);

    IB_MPOOL_FOREACH(ib_mpool_cleanup_t, cleanup, mp->cleanups)
    {
        cleanup->function(cleanup->function_data);
    }

    return;
}

/**@}*/

/**
 * @name Report Support
 *
 * These facilitate report generation for ib_mpool_analyze() and
 * ib_mpool_debug_report().
 */
/**@{*/

/** Maximum length of one line of a report. */
#define IB_MPOOL_REPORT_MAX_LINE 1024

/** See struct ib_mpool_report_line_t */
typedef struct ib_mpool_report_line_t ib_mpool_report_line_t;
/** See struct ib_mpool_report_t */
typedef struct ib_mpool_report_t ib_mpool_report_t;

/**
 * A line of a report.
 */
struct ib_mpool_report_line_t
{
    /** The line. */
    char *line;
    /** Pointer to next report line. */
    ib_mpool_report_line_t *next;
};

/**
 * A linked list of lines.
 *
 * This structure holds a linked list of lines.  It facilitates building up
 * reports and can be converted into a single buffer via
 * ib_mpool_report_convert().
 */
struct ib_mpool_report_t
{
    /** First line. */
    ib_mpool_report_line_t *first;
    /** Last line. */
    ib_mpool_report_line_t *last;
    /** Total size of report. */
    size_t total_size;
};

/**
 * Initialize a report.
 *
 * @param[in] report Report to initialize.
 */
static
void ib_mpool_report_init(
     ib_mpool_report_t *report
)
{
    assert(report != NULL);

    report->first      = NULL;
    report->last       = NULL;
    report->total_size = 0;

    return;
}

/**
 * Print to a report.
 *
 * @param[in] report Report to print to.
 * @param[in] fmt    Format string.
 * @returns True iff success.
 */
static
bool ib_mpool_report_printf(
    ib_mpool_report_t *report,
    const char*        fmt,
    ...
)
PRINTF_ATTRIBUTE(2, 3);

static
bool ib_mpool_report_printf(
    ib_mpool_report_t *report,
    const char        *fmt,
    ...
)
{
    assert(report != NULL);
    assert(fmt    != NULL);

    va_list ap;

    char *page = (char *)malloc(IB_MPOOL_REPORT_MAX_LINE);
    if (page == NULL) {
        return false;
    }

    ib_mpool_report_line_t *new_line =
        (ib_mpool_report_line_t *)malloc(sizeof(*new_line));
    if (new_line == NULL) {
        free(page);
        return false;
    }
    new_line->next = NULL;
    new_line->line = page;

    if (report->first == NULL) {
        report->first = report->last = new_line;
    }
    else {
        report->last->next = new_line;
        report->last = new_line;
    }

    va_start(ap, fmt);

    int n = vsnprintf(page, IB_MPOOL_REPORT_MAX_LINE, fmt, ap);

    va_end(ap);

    report->total_size += n;

    return true;
}

/**
 * Convert report to a single string.
 *
 * @param[in] report Report to convert.
 * @return Concatenation of all lines in @a report or NULL on allocation
 *         failure.
 */
static
char *ib_mpool_report_convert(
    ib_mpool_report_t *report
)
{
    assert(report != NULL);

    if (report->total_size == 0) {
        return NULL;
    }

    char *page = (char *)malloc(report->total_size + 1);
    if (page == NULL) {
        return NULL;
    }
    *page = '\0';

    IB_MPOOL_FOREACH(ib_mpool_report_line_t, line, report->first) {
        strcat(page, line->line);
    }

    return page;
}

/**
 * Destroy report.
 *
 * Frees all memory associated with @a report.
 *
 * @param[in] report Report to destroy.
 */
static
void ib_mpool_report_destroy(
    ib_mpool_report_t *report
)
{
    assert(report != NULL);

    IB_MPOOL_FOREACH(ib_mpool_report_line_t, line, report->first) {
        free(line->line);
        free(line);
    }

    return;
}

/**@}*/

/**
 * @name Helper functions for ib_mpool_debug_report() and ib_mpool_analyze().
 **/

/* Highly specific helper for next two functions. */
/**@cond DoNotDocument */
#define IMR_PRINTF(fmt, ...) \
    do { \
        bool imr_result = \
            ib_mpool_report_printf(report, (fmt), __VA_ARGS__); \
        if (! imr_result) { \
            goto failure; \
        } \
    } while (0)
/**@endcond*/

/**
 * Add debug report for @a mp to @a report.
 *
 * @sa ib_mpool_debug_report()
 *
 * @param[in] mp     Memory pool to report on.
 * @param[in] report Report to append to.
 * @return true iff success.
 */
static
bool ib_mpool_debug_report_helper(
    const ib_mpool_t  *mp,
    ib_mpool_report_t *report
)
{
    assert(mp     != NULL);
    assert(report != NULL);

    bool result;
    char *path = ib_mpool_path(mp);

    if (path == NULL) {
        goto failure;
    }

    IMR_PRINTF(
        "Debug Report for %p [%s]\n",
        mp, path
    );

    IMR_PRINTF("%s", "Attributes:\n");
    IMR_PRINTF("  pagesize               = %zd\n", mp->pagesize);
    IMR_PRINTF("  inuse                  = %zd\n", mp->inuse);
    IMR_PRINTF("  large_allocation_inuse = %zd\n",
        mp->large_allocation_inuse);
    IMR_PRINTF("  next                   = %p\n",  mp->next);
    IMR_PRINTF("  children               = %p\n",  mp->children);
    IMR_PRINTF("  children_end           = %p\n",  mp->children_end);
    IMR_PRINTF("  lock                   = %p\n",  mp->lock);
    IMR_PRINTF("  tracks                 = %p\n",  mp->tracks);
    IMR_PRINTF("  large_allocations      = %p\n",  mp->large_allocations);
    IMR_PRINTF("  large_allocations_end  = %p\n",  mp->large_allocations_end);
    IMR_PRINTF("  cleanups               = %p\n",  mp->cleanups);
    IMR_PRINTF("  cleanups_end           = %p\n",  mp->cleanups_end);
    IMR_PRINTF("  free_pages             = %p\n",  mp->free_pages);
    IMR_PRINTF("  free_pointer_pages     = %p\n",  mp->free_pointer_pages);
    IMR_PRINTF("  free_cleanups          = %p\n",  mp->free_cleanups);
    IMR_PRINTF("  free_children          = %p\n",  mp->free_children);

    IMR_PRINTF("%s", "Tracks:\n");
    for (size_t track_num = 0; track_num < IB_MPOOL_NUM_TRACKS; ++track_num) {
        size_t track_size = IB_MPOOL_TRACK_SIZE(track_num);
        IMR_PRINTF("  %2zd (<= %5zd):\n", track_num, track_size);
        IB_MPOOL_FOREACH(
            const ib_mpool_page_t, mpage,
            mp->tracks[track_num]
        ) {
            IMR_PRINTF(
                "    %p: page=%p used=%zd\n",
                mpage, &(mpage->page), mpage->used
            );
        }
    }

    IMR_PRINTF("%s", "Large Allocations:\n");
    IB_MPOOL_FOREACH(
        const ib_mpool_pointer_page_t, ppage,
        mp->large_allocations
    ) {
        IMR_PRINTF("  %p: next_pointer=%zd\n", ppage, ppage->next_pointer);
    }

    IMR_PRINTF("%s", "Cleanups:\n");
    IB_MPOOL_FOREACH(
        const ib_mpool_cleanup_t, cleanup,
        mp->cleanups
    ) {
        IMR_PRINTF(
            "  %p: function=%p data=%p",
            cleanup, cleanup->function, cleanup->function_data
        );
    }

    IMR_PRINTF("%s", "Free Buffers:\n");
    IB_MPOOL_FOREACH(
        const ib_mpool_page_t, mpage,
        mp->free_pages
    ) {
        IMR_PRINTF("  %p\n", mpage);
    }

    IMR_PRINTF("%s", "Free Pointer Buffers:\n");
    IB_MPOOL_FOREACH(
        const ib_mpool_pointer_page_t, ppage,
        mp->free_pointer_pages
    ) {
        IMR_PRINTF("  %p\n", ppage);
    }

    IMR_PRINTF("%s", "Free Cleanups:\n");
    IB_MPOOL_FOREACH(
        const ib_mpool_cleanup_t, cleanup,
        mp->free_cleanups
    ) {
        IMR_PRINTF("  %p\n", cleanup);
    }

    IMR_PRINTF("Done with %p.  Moving on to free children.\n\n", mp);

    IB_MPOOL_FOREACH(
        const ib_mpool_t, free_child,
        mp->free_children
    ) {
        bool result = ib_mpool_debug_report_helper(free_child, report);
        if (! result) {
            goto failure;
        }
    }

    IMR_PRINTF("Done with %p.  Moving on to children.\n\n", mp);

    IB_MPOOL_FOREACH(
        const ib_mpool_t, child,
        mp->children
    ) {
        bool result = ib_mpool_debug_report_helper(child, report);
        if (! result) {
            goto failure;
        }
    }

    result = true;
    goto finished;

failure:
    result = false;
    goto finished;

finished:
    if (path != NULL) {
        free(path);
    }
    return result;
}

/**
 * Add analyze of free child @a free_child to @a report.
 *
 * @sa ib_mpool_analyze_helper()
 *
 * @param[in]  free_child     Free child to analyze.
 * @param[in]  report         Report to append to.
 * @param[out] free_child_use Bytes used by free child.
 * @return true iff success.
 */
static
bool ib_mpool_analyze_free_child(
    const ib_mpool_t  *free_child,
    ib_mpool_report_t *report,
     size_t           *free_child_use
)
{
    assert(free_child != NULL);
    assert(report     != NULL);

    const size_t unit_page_cost =
        free_child->pagesize + sizeof(ib_mpool_page_t) - 1;

    size_t free_page         = 0;
    size_t free_cleanup      = 0;
    size_t free_pointer_page = 0;

    size_t total_used = sizeof(*free_child);

    IB_MPOOL_FOREACH(
        const ib_mpool_page_t, mpage,
        free_child->free_pages
    ) {
        free_page += unit_page_cost;
    }

    IB_MPOOL_FOREACH(
        const ib_mpool_pointer_page_t, ppage,
        free_child->free_pointer_pages
    ) {
        free_pointer_page += sizeof(ib_mpool_pointer_page_t);
    }

    IB_MPOOL_FOREACH(
        const ib_mpool_cleanup_t, cleanup,
        free_child->free_cleanups
    ) {
        free_cleanup += sizeof(ib_mpool_cleanup_t);
    }

    IMR_PRINTF("%zd=%zd+%zd+%zd",
        free_page + free_cleanup + free_pointer_page,
        free_page, free_cleanup, free_pointer_page
    );

    total_used += free_page + free_cleanup + free_pointer_page;

    if (free_child->free_children != NULL) {
        IMR_PRINTF("%s", " + [");
        IB_MPOOL_FOREACH(
            const ib_mpool_t, free_subchild,
            free_child->free_children
        ) {
            size_t child_use = 0;
            ib_mpool_analyze_free_child(free_subchild, report, &child_use);
            total_used += child_use;
            if (free_subchild->next != NULL) {
                IMR_PRINTF("%s", " + ");
            }
        }
        IMR_PRINTF("%s", "]");
    }

    *free_child_use = total_used;
    return true;

failure:
    *free_child_use = 0;
    return false;
}

/**
 * Add analysis report for @a mp to @a report.
 *
 * @sa ib_mpool_analyze()
 *
 * @param[in] mp     Memory pool to report on.
 * @param[in] report Report to append to.
 * @return true iff success.
 */
static
bool ib_mpool_analyze_helper(
    const ib_mpool_t  *mp,
    ib_mpool_report_t *report
)
{
    assert(mp     != NULL);
    assert(report != NULL);

    size_t page_cost         = 0;
    size_t page_use          = 0;
    size_t cleanup_cost      = 0;
    size_t cleanup_use       = 0;
    size_t pointer_page_cost = 0;
    size_t pointer_page_use  = 0;
    size_t free_page         = 0;
    size_t free_cleanup      = 0;
    size_t free_pointer_page = 0;
    size_t num_used_pages    = 0;
    size_t num_free_pages    = 0;

    bool result;

    const size_t unit_page_cost =
        mp->pagesize + sizeof(ib_mpool_page_t) - 1;
    char *path = ib_mpool_path(mp);

    if (path == NULL) {
        goto failure;
    }

    IMR_PRINTF(
        "Analysis of mpool %p [%s]\n",
        mp, path
    );

    IMR_PRINTF("%s", "Tracks:\n");
    for (size_t track_num = 0; track_num < IB_MPOOL_NUM_TRACKS; ++track_num) {
        size_t track_size = IB_MPOOL_TRACK_SIZE(track_num);
        size_t track_cost = 0;
        size_t track_use  = 0;
        IB_MPOOL_FOREACH(
            const ib_mpool_page_t, mpage,
            mp->tracks[track_num]
        ) {
            track_cost += unit_page_cost;
            track_use  += mpage->used;
            ++num_used_pages;
        }
        IMR_PRINTF(
            "  %2zd (<= %-5zd): cost=%12zd use=%12zd waste=%12zd "
            "efficiency=%4.1f%%\n",
            track_num, track_size, track_cost, track_use,
            track_cost - track_use,
            100*(double)track_use / track_cost
        );
        page_use  += track_use;
        page_cost += track_cost;
    }

    IB_MPOOL_FOREACH(
        const ib_mpool_pointer_page_t, ppage,
        mp->large_allocations
    ) {
        pointer_page_use += ppage->next_pointer * sizeof(void *);
        pointer_page_cost += sizeof(ib_mpool_pointer_page_t);
    }

    IB_MPOOL_FOREACH(
        const ib_mpool_cleanup_t, cleanup,
        mp->cleanups
    ) {
        cleanup_use  += sizeof(ib_mpool_cleanup_t);
        cleanup_cost += sizeof(ib_mpool_cleanup_t);
    }

    IB_MPOOL_FOREACH(
        const ib_mpool_page_t, mpage,
        mp->free_pages
    ) {
        free_page += unit_page_cost;
        ++num_free_pages;
    }

    IB_MPOOL_FOREACH(
        const ib_mpool_pointer_page_t, ppage,
        mp->free_pointer_pages
    ) {
        free_pointer_page += sizeof(ib_mpool_pointer_page_t);
    }

    IB_MPOOL_FOREACH(
        const ib_mpool_cleanup_t, cleanup,
        mp->free_cleanups
    ) {
        free_cleanup += sizeof(ib_mpool_cleanup_t);
    }

    IMR_PRINTF(
        "Page Info:\n"
        "   size=%-12zd used=%-12zd free=%-12zd efficency=%4.1f%%\n",
        mp->pagesize, num_used_pages, num_free_pages,
        (num_used_pages ? 100*(double)num_used_pages / (num_used_pages + num_free_pages) : 0)
    );
    IMR_PRINTF(
        "Pages:            use=%12zd cost=%12zd waste=%12zd free=%12zd "
        "efficiency=%4.1f%%\n",
        page_use, page_cost, page_cost - page_use, free_page,
        100*(double)page_use / page_cost
    );
    IMR_PRINTF(
        "PointerPages:     use=%12zd cost=%12zd waste=%12zd free=%12zd "
        "efficiency=%4.1f%%\n",
        pointer_page_use, pointer_page_cost,
        pointer_page_cost - pointer_page_use, free_pointer_page,
        (pointer_page_cost == 0 ?
            100 :
            100*(double)pointer_page_use / pointer_page_cost
        )
    );
    IMR_PRINTF(
        "LargeAllocations: use=%12zd (all others N/A)\n",
        mp->large_allocation_inuse
    );
    IMR_PRINTF(
        "Cleanups:         use=%12zd cost=%12zd waste=%12zd free=%12zd "
        "efficiency=%4.1f%%\n",
        cleanup_use, cleanup_cost,
        cleanup_cost - cleanup_use, free_cleanup,
        (cleanup_cost == 0 ?
            100 :
            100*(double)cleanup_use / cleanup_cost
        )
    );
    {
        size_t total_use = page_use + pointer_page_use + cleanup_use +
            mp->large_allocation_inuse;
        size_t total_cost = page_cost + pointer_page_cost + cleanup_cost +
            mp->large_allocation_inuse;
        size_t total_free_cost = free_page + free_pointer_page +
            free_cleanup;
        IMR_PRINTF(
            "Total:            use=%12zd cost=%12zd waste=%12zd "
            "free=%12zd efficiency=%4.1f%%\n",
            total_use, total_cost, total_cost - total_use, total_free_cost,
            (total_cost == 0 ?
                100 :
                100*(double)total_use / total_cost
            )
        );
    }

    if (mp->free_children != NULL) {
        IMR_PRINTF("%s", "Free children: ");

        size_t total_free_child_use = 0;
        IB_MPOOL_FOREACH(
            const ib_mpool_t, free_child,
            mp->free_children
        ) {
            size_t free_child_use = 0;
            bool result = ib_mpool_analyze_free_child(
                free_child,
                report,
                &free_child_use
            );
            if (! result) {
                goto failure;
            }
            total_free_child_use += free_child_use;
            if (free_child->next != NULL) {
                IMR_PRINTF("%s", " + ");
            }
        }
        IMR_PRINTF("\nTotal Free Child Use=%zd\n", total_free_child_use);
    }

    if (mp->children != NULL) {
        IMR_PRINTF("Done with %p.  Moving on to children.\n\n", mp);

        IB_MPOOL_FOREACH(
            const ib_mpool_t, child,
            mp->children
        ) {
            bool result = ib_mpool_analyze_helper(child, report);
            if (! result) {
                goto failure;
            }
        }
    }


    result = true;
    goto finished;

failure:
    result = false;
    goto finished;

finished:
    if (path != NULL) {
        free(path);
    }

    return result;
}

#undef IMR_PRINTF

/* End Internal */

/**
 * @name Public API
 *
 * Documented in mpool.h
 */
/**@{*/

ib_status_t ib_mpool_create(
    ib_mpool_t **pmp,
    const char  *name,
    ib_mpool_t  *parent
)
{
    ib_status_t rc;

    assert(pmp != NULL);

    rc = ib_mpool_create_ex(
        pmp,
        name,
        parent,
        0,
        NULL,
        NULL
    );

    return rc;
}

ib_status_t ib_mpool_create_ex(
    ib_mpool_t           **pmp,
    const char            *name,
    ib_mpool_t            *parent,
    size_t                 pagesize,
    ib_mpool_malloc_fn_t   malloc_fn,
    ib_mpool_free_fn_t     free_fn
)
{
    ib_status_t rc;
    ib_mpool_t *mp = NULL;

    assert(pmp != NULL);

    if (pagesize == 0) {
        if (parent != NULL) {
            pagesize = parent->pagesize;
        }
        else {
            pagesize = IB_MPOOL_DEFAULT_PAGE_SIZE;
        }
    }

    if (pagesize < IB_MPOOL_MINIMUM_PAGESIZE) {
        pagesize = IB_MPOOL_MINIMUM_PAGESIZE;
    }

    if (malloc_fn == NULL) {
        if (parent != NULL) {
            malloc_fn = parent->malloc_fn;
        }
        else {
            malloc_fn = &malloc;
        }
    }
    if (free_fn == NULL) {
        if (parent != NULL) {
            free_fn = parent->free_fn;
        }
        else {
            free_fn = &free;
        }
    }

    bool reacquired = false;
    if (parent != NULL) {
        rc = ib_lock_lock(parent->lock);
        if (rc != IB_OK) {
            goto failure;
        }
        if (
            parent->free_children != NULL &&
            parent->free_children->pagesize  == pagesize &&
            parent->free_children->malloc_fn == malloc_fn &&
            parent->free_children->free_fn   == free_fn
        ) {
            mp = parent->free_children;
            parent->free_children = mp->next;

            mp->next = NULL;
            reacquired = true;
            assert(mp->inuse                  == 0);
            assert(mp->large_allocation_inuse == 0);
        }
        ib_lock_unlock(parent->lock);
    }
    if (! reacquired) {
        mp = (ib_mpool_t *)malloc_fn(sizeof(**pmp));
        if (mp == NULL) {
            return IB_EALLOC;
        }
        memset(mp, 0, sizeof(**pmp));
        rc = ib_lock_create_malloc(&(mp->lock));
        if (rc != IB_OK) {
            goto failure;
        }
    }
    *pmp = mp;

    mp->pagesize               = pagesize;
    mp->malloc_fn              = malloc_fn;
    mp->free_fn                = free_fn;
    mp->inuse                  = 0;
    mp->large_allocation_inuse = 0;
    mp->parent                 = parent;

    rc = ib_mpool_setname(mp, name);
    if (rc != IB_OK) {
        return rc;
    }

    if (parent != NULL) {
        rc = ib_lock_lock(parent->lock);
        if (rc != IB_OK) {
            goto failure;
        }
        mp->next = parent->children;
        if (parent->children == NULL) {
            parent->children_end = mp;
        }
        parent->children = mp;
        ib_lock_unlock(parent->lock);
    }

#ifdef IB_MPOOL_VALGRIND
    VALGRIND_CREATE_MEMPOOL(mp, IB_MPOOL_REDZONE_SIZE, 0);
#endif

    return IB_OK;

failure:
    if (mp != NULL) {
        if (mp->name != NULL) {
            free_fn(mp->name);
        }
        if (mp->lock != NULL) {
            ib_lock_destroy_malloc(mp->lock);
        }
        free_fn(mp);
    }
    *pmp = NULL;

    return rc;
}

ib_status_t ib_mpool_setname(
    ib_mpool_t *mp,
    const char *name
)
{
    assert(mp != NULL);

    if (mp->name != NULL) {
        mp->free_fn(mp->name);
        mp->name = NULL;
    }

    if (name != NULL) {
        size_t len = strlen(name);
        mp->name = (char *)mp->malloc_fn(len+1);
        if (mp->name == NULL) {
            return IB_EALLOC;
        }
        memcpy(mp->name, name, len);
        mp->name[len] = '\0';
    }

    return IB_OK;
}

const char *ib_mpool_name(
    const ib_mpool_t* mp
)
{
    assert(mp != NULL);

    return mp->name;
}

size_t ib_mpool_inuse(
    const ib_mpool_t* mp
)
{
    if (mp == NULL) {
        return 0;
    }

    return mp->inuse;
}

ib_status_t ib_mpool_prealloc_pages(
    ib_mpool_t *mp,
    int pages
)
{
    assert(mp != NULL);
    assert(pages > 0);

    if (pages <= 0) {
        return IB_EINVAL;
    }

    /* Allocate the pages and store in free_pages. */
    ib_status_t rc = IB_OK;
    if (mp->free_pages == NULL) {
        mp->free_pages = ib_mpool_alloc_pages(mp, pages);
        if (mp->free_pages == NULL) {
            rc = IB_EALLOC;
        }
    }
    else {
        ib_mpool_page_t *mpage = ib_mpool_alloc_pages(mp, pages);
        if (mp->free_pages == NULL) {
            rc = IB_EALLOC;
        }

        mpage->next = mp->free_pages;
        mp->free_pages = mpage;
    }

    return rc;
}

void *ib_mpool_alloc(
    ib_mpool_t *mp,
    size_t      size
)
{
    void *ptr = NULL;

    assert(mp != NULL);

    if (size == 0) {
        return &s_zero_length_buffer;
    }

    /* Actual size: will add redzone if small allocation. */
    size_t actual_size = size;

    size_t track_number = ib_mpool_track_number(actual_size);
    if (track_number < IB_MPOOL_NUM_TRACKS) {
        /* Small allocation */
        /* Need to make sure we leave red zone at end. */
        actual_size += IB_MPOOL_REDZONE_SIZE;
        if (mp->tracks[track_number] == NULL ||
            (mp->pagesize -
             mp->tracks[track_number]->used -
             IB_MPOOL_REDZONE_SIZE
            ) < actual_size
        ) {
            ib_mpool_page_t *mpage = ib_mpool_acquire_page(mp);
            if (mpage == NULL) {
                return NULL;
            }
            mpage->next = mp->tracks[track_number];
            mpage->used = 0;
            if (mp->tracks[track_number] == NULL) {
                mp->tracks_end[track_number] = mpage;
            }
            mp->tracks[track_number] = mpage;
        }

        ib_mpool_page_t *mpage = mp->tracks[track_number];

        assert(
            (mpage->used + actual_size) <=
            mp->pagesize - IB_MPOOL_REDZONE_SIZE
        );
        ptr = &(mpage->page) + mpage->used + IB_MPOOL_REDZONE_SIZE;
        mpage->used += actual_size;

#ifdef IB_MPOOL_VALGRIND
        VALGRIND_MEMPOOL_ALLOC(mp, ptr, size);
#endif
    }
    else {
        /* Large allocation */
        /* Large allocations do not use redzones. */
        if (
            mp->large_allocations == NULL ||
            mp->large_allocations->next_pointer
                == IB_MPOOL_POINTER_PAGE_SIZE
        ) {
            ib_mpool_pointer_page_t *pointers =
                ib_mpool_acquire_pointer_page(mp);
            if (pointers == NULL) {
                return NULL;
            }
            memset(
                pointers->pointers, 0,
                sizeof(pointers->pointers)
            );
            pointers->next = mp->large_allocations;
            pointers->next_pointer = 0;
            if (mp->large_allocations == NULL) {
                mp->large_allocations_end = pointers;
            }
            mp->large_allocations = pointers;
        }

        ptr = mp->malloc_fn(size);
        if (ptr == NULL) {
            return NULL;
        }

        mp->large_allocations->pointers[mp->large_allocations->next_pointer] =
            ptr;
        ++mp->large_allocations->next_pointer;

        mp->large_allocation_inuse += size;
    }

    mp->inuse += actual_size;

    return ptr;
}

void ib_mpool_clear(
    ib_mpool_t *mp
)
{
    if (mp == NULL) {
        return;
    }

    ib_mpool_call_cleanups(mp);
    ib_mpool_free_large_allocations(mp);
    ib_mpool_setname(mp, NULL);

    for (size_t track_num = 0; track_num < IB_MPOOL_NUM_TRACKS; ++track_num) {
        if (mp->tracks[track_num] != NULL) {
            assert(mp->tracks_end[track_num] != NULL);
#ifdef IB_MPOOL_VALGRIND
            IB_MPOOL_FOREACH(
                const ib_mpool_page_t, mpage,
                mp->tracks[track_num]
            ) {
                int rc =
                  VALGRIND_MAKE_MEM_NOACCESS(&(mpage->page), mp->pagesize);
                assert(rc < 2);
            }
#endif
            mp->tracks_end[track_num]->next = mp->free_pages;
            mp->free_pages                  = mp->tracks[track_num];
            mp->tracks[track_num]           = NULL;
            mp->tracks_end[track_num]       = NULL;
        }
    }

    if (mp->large_allocations != NULL) {
        assert(mp->large_allocations_end != NULL);
        mp->large_allocations_end->next = mp->free_pointer_pages;
        mp->free_pointer_pages          = mp->large_allocations;
        mp->large_allocations           = NULL;
        mp->large_allocations_end       = NULL;
    }

    if (mp->cleanups != NULL) {
        assert(mp->cleanups_end != NULL);
        mp->cleanups_end->next = mp->free_cleanups;
        mp->free_cleanups      = mp->cleanups;
        mp->cleanups           = NULL;
        mp->cleanups_end       = NULL;
    }

    mp->inuse                  = 0;
    mp->large_allocation_inuse = 0;

    IB_MPOOL_FOREACH(ib_mpool_t, child, mp->children) {
        ib_mpool_clear(child);
    }

#ifdef IB_MPOOL_VALGRIND
    VALGRIND_DESTROY_MEMPOOL(mp);
    VALGRIND_CREATE_MEMPOOL(mp, IB_MPOOL_REDZONE_SIZE, 0);
#endif

    return;
}

void ib_mpool_destroy(
    ib_mpool_t *mp
)
{
    ib_mpool_call_cleanups(mp);
    ib_mpool_free_large_allocations(mp);

    for (size_t track_num = 0; track_num < IB_MPOOL_NUM_TRACKS; ++track_num) {
        IB_MPOOL_FOREACH(ib_mpool_page_t, mpage, mp->tracks[track_num]) {
            if (mpage->parent_page == NULL) {
                mp->free_fn(mpage);
            }
        }
    }

    IB_MPOOL_FOREACH(ib_mpool_pointer_page_t, ppage, mp->large_allocations) {
        mp->free_fn(ppage);
    }

    IB_MPOOL_FOREACH(ib_mpool_cleanup_t, cleanup, mp->cleanups) {
        mp->free_fn(cleanup);
    }

    IB_MPOOL_FOREACH(ib_mpool_page_t, mpage, mp->free_pages) {
        if (mpage->parent_page == NULL) {
            mp->free_fn(mpage);
        }
    }

    IB_MPOOL_FOREACH(ib_mpool_pointer_page_t, ppage, mp->free_pointer_pages) {
        mp->free_fn(ppage);
    }

    IB_MPOOL_FOREACH(ib_mpool_cleanup_t, cleanup, mp->free_cleanups) {
        mp->free_fn(cleanup);
    }

   /* We remove the child's parent link so that the child does not
    * worry about us as we also face imminent destruction.
    */

    IB_MPOOL_FOREACH(ib_mpool_t, free_child, mp->free_children) {
        free_child->parent = NULL;
        ib_mpool_destroy(free_child);
    }
    IB_MPOOL_FOREACH(ib_mpool_t, child, mp->children) {
        child->parent = NULL;
        ib_mpool_destroy(child);
    }

    if (mp->parent) {
        /* We have no good options if lock or unlock fails, so we hope. */
        ib_lock_lock(mp->parent->lock);

        ib_mpool_remove_child_from_parent(mp);

        ib_lock_unlock(mp->parent->lock);
    }

    if (mp->name) {
        mp->free_fn(mp->name);
    }

    ib_lock_destroy_malloc(mp->lock);

    mp->free_fn(mp);

#ifdef IB_MPOOL_VALGRIND
    /* Check existence so we don't double destroy free children's pools. */
    if (VALGRIND_MEMPOOL_EXISTS(mp)) {
        VALGRIND_DESTROY_MEMPOOL(mp);
    }
#endif

    return;
}

void ib_mpool_release(
    ib_mpool_t *mp
)
{
    if (mp == NULL) {
        return;
    }

    if (mp->parent == NULL) {
        ib_mpool_destroy(mp);
        return;
    }

    /* Clear pool and all subpools. */
    ib_mpool_clear(mp);

    /* Release all subpools. */
    IB_MPOOL_FOREACH(ib_mpool_t, child, mp->children) {
        ib_mpool_release(child);
    }

    ib_lock_lock(mp->parent->lock);

    /* Remove from parent child list. */
    ib_mpool_remove_child_from_parent(mp);

    /* Add to parent free children list. */
    mp->next = mp->parent->free_children;
    mp->parent->free_children = mp;

    ib_lock_unlock(mp->parent->lock);

#ifdef IB_MPOOL_VALGRIND
    VALGRIND_DESTROY_MEMPOOL(mp);
#endif

    return;
}

ib_status_t ib_mpool_cleanup_register(
    ib_mpool_t            *mp,
    ib_mpool_cleanup_fn_t  cleanup_function,
    void                  *function_data
)
{
    assert(mp != NULL);
    assert(cleanup_function != NULL);

    ib_mpool_cleanup_t *cleanup = ib_mpool_acquire_cleanup(mp);

    if (cleanup == NULL) {
        return IB_EALLOC;
    }

    cleanup->next          = mp->cleanups;
    cleanup->function      = cleanup_function;
    cleanup->function_data = function_data;

    if (mp->cleanups == NULL) {
        mp->cleanups_end = cleanup;
    }
    mp->cleanups = cleanup;

    return IB_OK;
}

char DLL_PUBLIC *ib_mpool_path(
    const ib_mpool_t *mp
)
{
    static const char* c_null_name = "null";

/**@cond DoNotDocument*/
#define NAME_OF(mp) ((mp)->name ? (mp)->name : c_null_name)
/**@endcond*/

    size_t  path_length = 0;
    char   *path_buffer = NULL;
    char   *path_i      = NULL;

    /* Pass 1, estimate length. */
    for (
        const ib_mpool_t* current = mp;
        current != NULL;
        current = current->parent
    ) {
        path_length += 1 + strlen(NAME_OF(current));
    }

    assert(path_length > 0);

    path_buffer = (char *)malloc(path_length + 1);
    if (path_buffer == NULL) {
        return NULL;
    }
    path_i = path_buffer + path_length;
    *path_i = '\0';

    /* Pass 2, fill buffer. */
    for (
        const ib_mpool_t* current = mp;
        current != NULL;
        current = current->parent
    ) {
        size_t length = strlen(NAME_OF(current));
        path_i -= length;
        memcpy(path_i, NAME_OF(current), length);
        --path_i;
        *path_i = '/';
    }

    assert(path_i == path_buffer);

    return path_buffer;
#undef NAME_OF
}

ib_status_t ib_mpool_validate(
    const ib_mpool_t  *mp,
    char             **message
)
{
/**@cond DoNotDocument*/
#define VALIDATE_ERROR(fmt, ...) \
    do { \
        error_message = (char *)mp->malloc_fn(c_message_size); \
        if (error_message != NULL) { \
            snprintf(error_message, c_message_size, fmt, __VA_ARGS__); \
        } \
        goto error; \
    } while (0);
/**@endcond*/

    static const size_t c_message_size = 1024;

    assert(mp != NULL);
    assert(message != NULL);

    char *error_message = NULL;

    /* Validate use of each page */
    for (
        size_t track_num = 0;
        track_num < IB_MPOOL_NUM_TRACKS;
        ++track_num
    ) {
        size_t track_size = IB_MPOOL_TRACK_SIZE(track_num);
        IB_MPOOL_FOREACH(ib_mpool_page_t, mpage, mp->tracks[track_num]) {
            /* If the page is not the first in the track then its remaining
             * memory must be less than the appropriate size: i.e., too small
             * to do another allocation.
             */
            size_t remaining = mp->pagesize - mpage->used;
            if (
                mpage     != mp->tracks[track_num] &&
                remaining >= track_size + 2*IB_MPOOL_REDZONE_SIZE
            ) {
                VALIDATE_ERROR(
                    "Available memory: %zd %p %zd",
                    track_num, mpage, remaining
                );
            }
        }
    }

    /* Validate pointer pages */
    IB_MPOOL_FOREACH(ib_mpool_pointer_page_t, ppage, mp->large_allocations) {
        /* If the page is not the first, then its next_pointer must be
         * IB_MPOOL_POINTER_PAGE_SIZE.
         */
        if (
            ppage != mp->large_allocations &&
            ppage->next_pointer != IB_MPOOL_POINTER_PAGE_SIZE
        ) {
            VALIDATE_ERROR(
                "Available pointers: %p %zd",
                ppage, ppage->next_pointer
            );
        }

        /* There can be no null pointers before ppage->next_pointer. */
        for (size_t i = 0; i < ppage->next_pointer; ++i) {
            if (ppage->pointers[i] == NULL) {
                VALIDATE_ERROR(
                    "Early NULL pointer: %p %zd",
                    ppage, i
                );
            }
        }
    }

    /* Validate cleanups */
    IB_MPOOL_FOREACH(ib_mpool_cleanup_t, cleanup, mp->cleanups) {
        /* Every cleanup must have non-null function. */
        if (cleanup->function == NULL) {
            VALIDATE_ERROR(
                "NULL cleanup: %p",
                 cleanup
            );
        }
    }

    /* Validate child of parent */
    if (mp->parent) {
        ib_mpool_t *child = mp->parent->children;
        while (child != NULL && child != mp) {
            child = child->next;
        }
        if (child == NULL) {
            child = mp->parent->free_children;
            while (child != NULL && child != mp) {
                child = child->next;
            }
        }
        if (child == NULL) {
            VALIDATE_ERROR(
                "Not a child or free child of my parent: %p",
                child
            );
        }
    }

    /* Validate inuse */
    {
        size_t inuse = mp->large_allocation_inuse;
        for (
            size_t track_num = 0;
            track_num < IB_MPOOL_NUM_TRACKS;
            ++track_num
        ) {
            IB_MPOOL_FOREACH(ib_mpool_page_t, mpage, mp->tracks[track_num]) {
                inuse += mpage->used;
            }
        }
        if (inuse != mp->inuse) {
            VALIDATE_ERROR(
                "Inconsistent inuse: %zd %zd",
                inuse, mp->inuse
            );
        }
    }


    /* Validate end pointers */
/** @cond DoNotDocument */
#define VALIDATE_END(list_type, begin, end, name) \
    { \
        if ((begin) != NULL) { \
            const list_type *ve_i = (begin); \
            while (ve_i->next != NULL) { \
                ve_i = ve_i->next; \
            } \
            if (ve_i != (end)) { \
                VALIDATE_ERROR( \
                    "List " name " has invalid end: %p %p", \
                    ve_i, (end) \
                ); \
            } \
        } \
        else if ((end) != NULL) { \
            VALIDATE_ERROR( \
                "List " name " has end but no beginning: %p", \
                (end) \
            ); \
        } \
    }
/** @endcond */
    VALIDATE_END(ib_mpool_t, mp->children, mp->children_end, "children");
    for (
        size_t track_num = 0;
        track_num < IB_MPOOL_NUM_TRACKS;
        ++track_num
    ) {
        VALIDATE_END(
            ib_mpool_page_t,
            mp->tracks[track_num], mp->tracks_end[track_num],
            "track"
        );
    }
    VALIDATE_END(
        ib_mpool_pointer_page_t,
        mp->large_allocations, mp->large_allocations_end,
        "large_allocations"
    );
    VALIDATE_END(
        ib_mpool_cleanup_t,
        mp->cleanups, mp->cleanups_end,
        "cleanups"
    );
#undef VALIDATE_END

    /* Validate children */
    IB_MPOOL_FOREACH(ib_mpool_t, child, mp->children) {
        if (child->parent != mp) {
            VALIDATE_ERROR(
                "Child does not consider me its parent: %p %p",
                child, child->parent
            );
        }
        ib_status_t rc = ib_mpool_validate(child, message);
        if (rc == IB_EOTHER) {
            goto child_error;
        }
        if (rc != IB_OK) {
            return rc;
        }
    }

    /* Validate free children */
    IB_MPOOL_FOREACH(ib_mpool_t, free_child, mp->free_children) {
        if (free_child->parent != mp) {
            VALIDATE_ERROR(
                "Free Child does not consider me its parent: %p %p",
                free_child, free_child->parent
            );
        }
        /* Free child specific checks. */
        if (free_child->children != NULL) {
            VALIDATE_ERROR(
                "Free Child has children: %p",
                free_child
            );
        }
        for (
            size_t track_num = 0;
            track_num < IB_MPOOL_NUM_TRACKS;
            ++track_num
        ) {
            if (free_child->tracks[track_num] != NULL) {
                VALIDATE_ERROR(
                    "Free Child has pages: %p %zd",
                    free_child, track_num
                );
            }
        }
        if (free_child->large_allocations != NULL) {
            VALIDATE_ERROR(
                "Free Child has large allocations: %p",
                free_child
            );
        }
        if (free_child->cleanups != NULL) {
            VALIDATE_ERROR(
                "Free Child has cleanups: %p",
                free_child
            );
        }
        if (
            free_child->pagesize  != mp->pagesize ||
            free_child->malloc_fn != mp->malloc_fn ||
            free_child->free_fn   != mp->free_fn
        ) {
            VALIDATE_ERROR(
                "Free Child has different parameters: %p",
                free_child
            );
        }

        /* Normal validation, including validation of free grandchildren. */
        ib_status_t rc = ib_mpool_validate(free_child, message);
        if (rc == IB_EOTHER) {
            goto child_error;
        }
        if (rc != IB_OK) {
            return rc;
        }
    }

    /* Normal exit */
    return IB_OK;

error:
    {
        char *message_page = (char *)mp->malloc_fn(c_message_size);
        if (message_page == NULL) {
            *message = NULL;
        }
        else {
            *message = message_page;
            snprintf(message_page, c_message_size, "%p [%s]: %s",
                mp,
                (mp->name ? mp->name : "NULL"),
                (error_message ? error_message : "No message")
            );
            if (error_message != NULL) {
                mp->free_fn(error_message);
            }
        }
    }
    /* Fall through */

child_error:
    return IB_EOTHER;

#undef VALIDATE_ERROR
}

char *ib_mpool_analyze(
    const ib_mpool_t *mp
)
{
    assert(mp != NULL);

    ib_mpool_report_t report;
    char *report_text = NULL;

    ib_mpool_report_init(&report);

    bool result = ib_mpool_analyze_helper(mp, &report);

    if (result) {
        report_text = ib_mpool_report_convert(&report);
    }

    ib_mpool_report_destroy(&report);
    return report_text;
}

char *ib_mpool_debug_report(
    const ib_mpool_t *mp
)
{
    assert(mp != NULL);

    ib_mpool_report_t report;
    char *report_text = NULL;

    ib_mpool_report_init(&report);

    bool result = ib_mpool_debug_report_helper(mp, &report);

    if (result) {
        report_text = ib_mpool_report_convert(&report);
    }

    ib_mpool_report_destroy(&report);
    return report_text;
}

/* All of the following routines are written in terms of the previous and
 * do not directly touch mp.
 */

ib_mpool_t *ib_mpool_parent(
    ib_mpool_t *mp
)
{
    assert(mp != NULL);

    return mp->parent;
}

/**@}*/
