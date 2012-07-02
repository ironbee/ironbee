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
 * @brief IronBee &mdash; Memory Pool Implementation
 *
 * See ib_mpool_t for details.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 * @nosubgrouping
 */

#include "ironbee_config_auto.h"

#include <ironbee/mpool.h>

#include <ironbee/debug.h>
#include <ironbee/lock.h>

#include <stdlib.h>
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdarg.h>

/**
 * @name Memory Pool Configuration
 *
 * Adjusting the values of these macros can significantly change the time and
 * space performance of memory pool.  See ib_mpool_analyze().
 */
/**@{*/

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
#define IB_MPOOL_NUM_TRACKS 6

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
#define IB_MPOOL_TRACK_ZERO_SIZE 5

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
     * Lock for multithreading support.
     *
     * This lock is only used in creation and destruction to allow
     * simultaneous creation/destruction of memory pools with a common
     * parent.  Both operations must modify the parents children list and
     * this lock protects it.
     **/
    ib_lock_t lock;

    /**
     * Tracks of pages.
     *
     * @sa ib_mpool_t
     **/
    ib_mpool_page_t         *tracks[IB_MPOOL_NUM_TRACKS];
    /**
     * Singly linked list of pointers page for large allocations.
     *
     * @sa ib_mpool_t
     **/
    ib_mpool_pointer_page_t *large_allocations;
    /**
     * Singly linked list of cleanup functions.
     *
     * @sa ib_mpool_t
     **/
    ib_mpool_cleanup_t      *cleanups;
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
    IB_FTRACE_INIT();

    if (size > IB_MPOOL_MINIMUM_PAGESIZE) {
        IB_FTRACE_RET_UINT(IB_MPOOL_NUM_TRACKS);
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
        IB_FTRACE_RET_UINT(0);
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

    IB_FTRACE_RET_UINT(r);
}

/**@}*/

/**
 * @name Helper functions for managing internal memory.
 */
/**@{*/

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
    IB_FTRACE_INIT();

    assert(mp != NULL);

    ib_mpool_page_t *mpage = NULL;

    if (mp->free_pages != NULL) {
        mpage = mp->free_pages;
        mp->free_pages = mp->free_pages->next;
    }
    else {
        mpage = mp->malloc_fn(sizeof(ib_mpool_page_t) + mp->pagesize - 1);
    }

    IB_FTRACE_RET_PTR(void, mpage);
}

/**
 * Release a page.
 *
 * Pushes the page onto the free list.
 *
 * @param[in] mp    Memory pool that owns page.
 * @param[in] mpage Page to release.
 **/
static
void ib_mpool_release_page(
    ib_mpool_t      *mp,
    ib_mpool_page_t *mpage
)
{
    IB_FTRACE_INIT();

    assert(mp    != NULL);
    assert(mpage != NULL);

    mpage->next = mp->free_pages;
    mp->free_pages = mpage;

    IB_FTRACE_RET_VOID();
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
    IB_FTRACE_INIT();

    assert(mp != NULL);

    ib_mpool_pointer_page_t *ppage = NULL;

    if (mp->free_pointer_pages != NULL) {
        ppage = mp->free_pointer_pages;
        mp->free_pointer_pages = mp->free_pointer_pages->next;
    }
    else {
        ppage = mp->malloc_fn(sizeof(*ppage));
    }

    IB_FTRACE_RET_PTR(void, ppage);
}

/**
 * Release a pointer page.
 *
 * Pushes the pointer page onto the free list.
 *
 * @param[in] mp    Memory pool that owns pointer page.
 * @param[in] ppage Pointer page to release.
 **/
static
void ib_mpool_release_pointer_page(
    ib_mpool_t              *mp,
    ib_mpool_pointer_page_t *ppage
)
{
    IB_FTRACE_INIT();

    assert(mp    != NULL);
    assert(ppage != NULL);

    ppage->next = mp->free_pointer_pages;
    mp->free_pointer_pages = ppage;

    IB_FTRACE_RET_VOID();
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
    IB_FTRACE_INIT();

    assert(mp != NULL);

    ib_mpool_cleanup_t *cleanup = NULL;

    if (mp->free_cleanups != NULL) {
        cleanup = mp->free_cleanups;
        mp->free_cleanups = mp->free_cleanups->next;
    }
    else {
        cleanup = mp->malloc_fn(sizeof(*cleanup));
    }

    IB_FTRACE_RET_PTR(void, cleanup);
}

/**
 * Release a cleanup node.
 *
 * Pushes the cleanup node onto the free list.
 *
 * @param[in] mp      Memory pool that owns cleanup node.
 * @param[in] cleanup Cleanup node to release.
 **/
static
void ib_mpool_release_cleanup(
    ib_mpool_t         *mp,
    ib_mpool_cleanup_t *cleanup
)
{
    IB_FTRACE_INIT();

    assert(mp      != NULL);
    assert(cleanup != NULL);

    cleanup->next = mp->free_cleanups;
    mp->free_cleanups = cleanup;

    IB_FTRACE_RET_VOID();
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
    IB_FTRACE_INIT();

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

    IB_FTRACE_RET_VOID();
}

/**
 * Call every cleanup function for @a mp.
 *
 * @param[in] mp Memory pool to call cleanups for.
 **/
static
void ib_mpool_call_cleanups(const ib_mpool_t *mp)
{
    IB_FTRACE_INIT();

    assert(mp != NULL);

    IB_MPOOL_FOREACH(ib_mpool_cleanup_t, cleanup, mp->cleanups)
    {
        cleanup->function(cleanup->function_data);
    }

    IB_FTRACE_RET_VOID();
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
    IB_FTRACE_INIT();

    assert(report != NULL);

    report->first      = NULL;
    report->last       = NULL;
    report->total_size = 0;

    IB_FTRACE_RET_VOID();
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
    IB_FTRACE_INIT();

    assert(report != NULL);
    assert(fmt    != NULL);

    va_list ap;

    char *page = (char *)malloc(IB_MPOOL_REPORT_MAX_LINE);
    if (page == NULL) {
        IB_FTRACE_RET_BOOL(false);
    }

    ib_mpool_report_line_t *new_line =
        (ib_mpool_report_line_t *)malloc(sizeof(*new_line));
    if (new_line == NULL) {
        free(page);
        IB_FTRACE_RET_BOOL(false);
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

    IB_FTRACE_RET_BOOL(true);
}

/**
 * Convert report to a single string.
 *
 * @param[in] report Report to convert.
 * @return Concatenation of all lines in @a report or NULL on allocation
 *         failure.
 */
static
const char *ib_mpool_report_convert(
    ib_mpool_report_t *report
)
{
    IB_FTRACE_INIT();

    assert(report != NULL);

    if (report->total_size == 0) {
        IB_FTRACE_RET_STR("");
    }

    char *page = (char *)malloc(report->total_size);
    if (page == NULL) {
        IB_FTRACE_RET_STR(NULL);
    }

    IB_MPOOL_FOREACH(ib_mpool_report_line_t, line, report->first) {
        strcat(page, line->line);
    }

    IB_FTRACE_RET_STR(page);
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
    IB_FTRACE_INIT();

    assert(report != NULL);

    IB_MPOOL_FOREACH(ib_mpool_report_line_t, line, report->first) {
        free(line->line);
        free(line);
    }

    IB_FTRACE_RET_VOID();
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
    IB_FTRACE_INIT();

    assert(mp     != NULL);
    assert(report != NULL);

    IMR_PRINTF(
        "Debug Report for %p [%s]\n",
        mp, (mp->name ? mp->name : "NULL")
    );

    IMR_PRINTF("%s", "Attributes:\n");
    IMR_PRINTF("  pagesize               = %zd\n", mp->pagesize);
    IMR_PRINTF("  inuse                  = %zd\n", mp->inuse);
    IMR_PRINTF("  large_allocation_inuse = %zd\n",
        mp->large_allocation_inuse);
    IMR_PRINTF("  next                   = %p\n",  mp->next);
    IMR_PRINTF("  children               = %p\n",  mp->children);
    IMR_PRINTF("  lock                   = %p\n",  &(mp->lock));
    IMR_PRINTF("  tracks                 = %p\n",  mp->tracks);
    IMR_PRINTF("  large_allocations      = %p\n",  mp->large_allocations);
    IMR_PRINTF("  cleanups               = %p\n",  mp->cleanups);
    IMR_PRINTF("  free_pages             = %p\n",  mp->free_pages);
    IMR_PRINTF("  free_pointer_pages     = %p\n",  mp->free_pointer_pages);
    IMR_PRINTF("  free_cleanups          = %p\n",  mp->free_cleanups);

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

    IB_FTRACE_RET_BOOL(true);

failure:
    IB_FTRACE_RET_BOOL(false);
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
    IB_FTRACE_INIT();

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

    const size_t unit_page_cost =
        mp->pagesize + sizeof(ib_mpool_page_t) - 1;
    IMR_PRINTF(
        "Analysis of mpool %p [%s]\n",
        mp, (mp->name ? mp->name : "NULL")
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
        100*(double)pointer_page_use / pointer_page_cost
    );
    IMR_PRINTF(
        "LargeAllocations: use=%12zd (all others N/A)\n",
        mp->large_allocation_inuse
    );
    IMR_PRINTF(
        "Cleanups:         use=%12zd cost=%12zd waste=%12zd free=%12zd "
        "efficiency=%4.1f%%\n",
        pointer_page_use, pointer_page_cost,
        pointer_page_cost - pointer_page_use, free_pointer_page,
        100*(double)pointer_page_use / pointer_page_cost
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
            100*(double)total_use / total_cost
        );
    }

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

    IB_FTRACE_RET_BOOL(true);

failure:
    IB_FTRACE_RET_BOOL(false);
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
    IB_FTRACE_INIT();

    ib_status_t rc;

    if (pmp == NULL) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    rc = ib_mpool_create_ex(
        pmp,
        name,
        parent,
        0,
        NULL,
        NULL
    );

    IB_FTRACE_RET_STATUS(rc);
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
    IB_FTRACE_INIT();

    ib_status_t rc;
    ib_mpool_t *mp = NULL;

    if (pmp == NULL) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    if (pagesize == 0) {
        pagesize = IB_MPOOL_DEFAULT_PAGE_SIZE;
    }

    if (pagesize < IB_MPOOL_MINIMUM_PAGESIZE) {
        pagesize = IB_MPOOL_MINIMUM_PAGESIZE;
    }

    if (malloc_fn == NULL) {
        malloc_fn = &malloc;
    }
    if (free_fn == NULL) {
        free_fn = &free;
    }

    mp = (ib_mpool_t *)malloc_fn(sizeof(**pmp));
    if (mp == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }
    memset(mp, 0, sizeof(**pmp));
    *pmp = mp;

    rc = ib_lock_init(&(mp->lock));
    if (rc != IB_OK) {
        goto failure;
    }

    mp->pagesize               = pagesize;
    mp->malloc_fn              = malloc_fn;
    mp->free_fn                = free_fn;
    mp->inuse                  = 0;
    mp->large_allocation_inuse = 0;
    mp->parent                 = parent;

    rc = ib_mpool_setname(mp, name);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    if (parent != NULL) {
        rc = ib_lock_lock(&(parent->lock));
        if (rc != IB_OK) {
            goto failure;
        }
        mp->next = parent->children;
        parent->children = mp;
        ib_lock_unlock(&(parent->lock));
    }

    ib_lock_unlock(&(mp->lock));

    IB_FTRACE_RET_STATUS(IB_OK);

failure:
    if (mp != NULL) {
        if (mp->name != NULL) {
            free_fn(mp->name);
        }
        free_fn(mp);
    }
    *pmp = NULL;

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_mpool_setname(
    ib_mpool_t *mp,
    const char *name
)
{
    IB_FTRACE_INIT();

    if (mp == NULL) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    if (mp->name != NULL) {
        mp->free_fn(mp->name);
        mp->name = NULL;
    }

    if (name != NULL) {
        size_t len = strlen(name);
        mp->name = (char *)mp->malloc_fn(len+1);
        if (mp->name == NULL) {
            IB_FTRACE_RET_STATUS(IB_EALLOC);
        }
        memcpy(mp->name, name, len);
        mp->name[len] = '\0';
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

const char *ib_mpool_name(
    const ib_mpool_t* mp
)
{
    IB_FTRACE_INIT();

    if (mp == NULL) {
        IB_FTRACE_RET_STR(NULL);
    }

    IB_FTRACE_RET_STR(mp->name);
}

size_t ib_mpool_inuse(
    const ib_mpool_t* mp
)
{
    IB_FTRACE_INIT();

    if (mp == NULL) {
        IB_FTRACE_RET_UINT(0);
    }

    IB_FTRACE_RET_UINT(mp->inuse);
}

void *ib_mpool_alloc(
    ib_mpool_t *mp,
    size_t      size
)
{
    IB_FTRACE_INIT();

    void *ptr = NULL;

    if (mp == NULL || size == 0) {
        IB_FTRACE_RET_PTR(void, NULL);
    }

    size_t track_number = ib_mpool_track_number(size);
    if (track_number < IB_MPOOL_NUM_TRACKS) {
        /* Small allocation */
        if (mp->tracks[track_number] == NULL ||
            (mp->pagesize - mp->tracks[track_number]->used) < size
        ) {
            ib_mpool_page_t *mpage = ib_mpool_acquire_page(mp);
            mpage->next = mp->tracks[track_number];
            mpage->used = 0;
            mp->tracks[track_number] = mpage;
        }

        ib_mpool_page_t *mpage = mp->tracks[track_number];

        assert((mpage->used + size) <= mp->pagesize);
        ptr = &(mpage->page) + mpage->used;
        mpage->used += size;
    }
    else {
        /* Large allocation */
        if (
            mp->large_allocations == NULL ||
            mp->large_allocations->next_pointer
                == IB_MPOOL_POINTER_PAGE_SIZE
        ) {
            ib_mpool_pointer_page_t *pointers =
                ib_mpool_acquire_pointer_page(mp);
            if (pointers == NULL) {
                IB_FTRACE_RET_PTR(void, NULL);
            }
            memset(
                pointers->pointers, 0,
                sizeof(pointers->pointers)
            );
            pointers->next = mp->large_allocations;
            pointers->next_pointer = 0;
            mp->large_allocations = pointers;
        }

        ptr = mp->malloc_fn(size);
        if (ptr == NULL) {
            IB_FTRACE_RET_PTR(void, NULL);
        }

        mp->large_allocations->pointers[mp->large_allocations->next_pointer] =
            ptr;
        ++mp->large_allocations->next_pointer;

        mp->large_allocation_inuse += size;
    }

    mp->inuse += size;

    IB_FTRACE_RET_PTR(void, ptr);
}

void ib_mpool_clear(
    ib_mpool_t *mp
)
{
    IB_FTRACE_INIT();

    if (mp == NULL) {
        IB_FTRACE_RET_VOID();
    }

    ib_mpool_call_cleanups(mp);
    ib_mpool_free_large_allocations(mp);

    for (size_t track_num = 0; track_num < IB_MPOOL_NUM_TRACKS; ++track_num) {
        IB_MPOOL_FOREACH(ib_mpool_page_t, mpage, mp->tracks[track_num]) {
            ib_mpool_release_page(mp, mpage);
        }
        mp->tracks[track_num] = NULL;
    }

    IB_MPOOL_FOREACH(ib_mpool_pointer_page_t, ppage, mp->large_allocations) {
        ib_mpool_release_pointer_page(mp, ppage);
    }
    mp->large_allocations = NULL;

    IB_MPOOL_FOREACH(ib_mpool_cleanup_t, cleanup, mp->cleanups) {
        ib_mpool_release_cleanup(mp, cleanup);
    }
    mp->cleanups = NULL;

    mp->inuse                  = 0;
    mp->large_allocation_inuse = 0;

    IB_MPOOL_FOREACH(ib_mpool_t, child, mp->children) {
        ib_mpool_clear(child);
    }

    IB_FTRACE_RET_VOID();
}

void ib_mpool_destroy(
    ib_mpool_t *mp
)
{
    IB_FTRACE_INIT();

    ib_mpool_call_cleanups(mp);
    ib_mpool_free_large_allocations(mp);

    for (size_t track_num = 0; track_num < IB_MPOOL_NUM_TRACKS; ++track_num) {
        IB_MPOOL_FOREACH(ib_mpool_page_t, mpage, mp->tracks[track_num]) {
            mp->free_fn(mpage);
        }
    }

    IB_MPOOL_FOREACH(ib_mpool_pointer_page_t, ppage, mp->large_allocations) {
        mp->free_fn(ppage);
    }

    IB_MPOOL_FOREACH(ib_mpool_cleanup_t, cleanup, mp->cleanups) {
        mp->free_fn(cleanup);
    }

    IB_MPOOL_FOREACH(ib_mpool_page_t, mpage, mp->free_pages) {
        mp->free_fn(mpage);
    }

    IB_MPOOL_FOREACH(ib_mpool_pointer_page_t, ppage, mp->free_pointer_pages) {
        mp->free_fn(ppage);
    }

    IB_MPOOL_FOREACH(ib_mpool_cleanup_t, cleanup, mp->free_cleanups) {
        mp->free_fn(cleanup);
    }

    IB_MPOOL_FOREACH(ib_mpool_t, child, mp->children) {
        /* We remove the child's parent link so that the child does not
         * worry about us as we also face imminent destruction.
         */
        child->parent = NULL;
        ib_mpool_destroy(child);
    }

    if (mp->parent) {
        /* We have no good options if lock or unlock fails, so we hope. */
        ib_lock_lock(&(mp->parent->lock));

        ib_mpool_t **my_handle = &(mp->parent->children);

        while(*my_handle != NULL && *my_handle != mp) {
            my_handle = &((*my_handle)->next);
        }

        assert(my_handle != NULL);
        assert(*my_handle != NULL);

        *my_handle = mp->next;

        ib_lock_unlock(&(mp->parent->lock));
    }

    if (mp->name) {
        mp->free_fn(mp->name);
    }

    mp->free_fn(mp);

    IB_FTRACE_RET_VOID();
}

ib_status_t ib_mpool_cleanup_register(
    ib_mpool_t            *mp,
    ib_mpool_cleanup_fn_t  cleanup_function,
    void                  *function_data
)
{
    IB_FTRACE_INIT();

    if (mp == NULL || cleanup_function == NULL) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    ib_mpool_cleanup_t *cleanup = ib_mpool_acquire_cleanup(mp);

    if (cleanup == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    cleanup->next          = mp->cleanups;
    cleanup->function      = cleanup_function;
    cleanup->function_data = function_data;

    mp->cleanups = cleanup;

    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t ib_mpool_validate(
    const ib_mpool_t  *mp,
    const char       **message
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

    IB_FTRACE_INIT();

    static const size_t c_message_size = 1024;

    if (mp == NULL || message == NULL) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

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
             * memory must be less than the appropriate size.
             */
            size_t remaining = mp->pagesize - mpage->used;
            if (
                mpage     != mp->tracks[track_num] &&
                remaining >= track_size
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
            VALIDATE_ERROR(
                "Not a child of my parent: %p",
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
            IB_FTRACE_RET_STATUS(rc);
        }
    }

    /* Normal exit */
    IB_FTRACE_RET_STATUS(IB_OK);

error:
    {
        char *message_page = (char *)mp->malloc_fn(c_message_size);
        if (! message_page) {
            *message = "Could not allocate memory for message.";
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
    IB_FTRACE_RET_STATUS(IB_EOTHER);

#undef VALIDATE_ERROR
}

const char *ib_mpool_analyze(
    const ib_mpool_t *mp
)
{
    IB_FTRACE_INIT();

    if (mp == NULL) {
        IB_FTRACE_RET_STR(NULL);
    }

    ib_mpool_report_t report;
    const char *report_text = NULL;

    ib_mpool_report_init(&report);

    bool result = ib_mpool_analyze_helper(mp, &report);

    if (result) {
        report_text = ib_mpool_report_convert(&report);
    }

    ib_mpool_report_destroy(&report);
    IB_FTRACE_RET_STR(report_text);
}

const char *ib_mpool_debug_report(
    const ib_mpool_t *mp
)
{
    IB_FTRACE_INIT();

    if (mp == NULL) {
        IB_FTRACE_RET_STR(NULL);
    }

    ib_mpool_report_t report;
    const char *report_text = NULL;

    ib_mpool_report_init(&report);

    bool result = ib_mpool_debug_report_helper(mp, &report);

    if (result) {
        report_text = ib_mpool_report_convert(&report);
    }

    ib_mpool_report_destroy(&report);
    IB_FTRACE_RET_STR(report_text);
}

/* All of the following routines are written in terms of the previous and
 * do not directly touch mp.
 */

void *ib_mpool_calloc(
    ib_mpool_t *mp,
    size_t      nelem,
    size_t      size
)
{
    IB_FTRACE_INIT();

    if (mp == NULL || nelem == 0 || size == 0) {
        IB_FTRACE_RET_PTR(void, NULL);
    }

    void *ptr = ib_mpool_alloc(mp, nelem * size);

    if (ptr != NULL)  {
        memset(ptr, 0, nelem * size);
    }

    IB_FTRACE_RET_PTR(void, ptr);
}

char *ib_mpool_strdup(
    ib_mpool_t *mp,
    const char *src
)
{
    IB_FTRACE_INIT();

    if (mp == NULL || src == NULL) {
        IB_FTRACE_RET_STR(NULL);
    }

    size_t size = strlen(src);
    char *ptr = ib_mpool_memdup_to_str(mp, src, size);

    IB_FTRACE_RET_STR(ptr);
}

char *ib_mpool_memdup_to_str(
    ib_mpool_t *mp,
    const void *src,
    size_t      size
)
{
    IB_FTRACE_INIT();

    if (mp == NULL || src == NULL) {
        IB_FTRACE_RET_STR(NULL);
    }

    char *str = (char *)ib_mpool_alloc(mp, size + 1);

    if (str != NULL) {
        if (size != 0) {
            memcpy(str, src, size);
        }
        *(str + size) = '\0';
    }

    IB_FTRACE_RET_PTR(char, str);
}

void *ib_mpool_memdup(
    ib_mpool_t *mp,
    const void *src,
    size_t      size
)
{
    IB_FTRACE_INIT();

    if (mp == NULL || src == NULL || size == 0) {
        IB_FTRACE_RET_PTR(void, NULL);
    }

    void *ptr = ib_mpool_alloc(mp, size);

    if (ptr != NULL) {
        memcpy(ptr, src, size);
    }

    IB_FTRACE_RET_PTR(void, ptr);
}

/**@}*/
