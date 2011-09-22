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

#ifndef _IB_UTIL_H_
#define _IB_UTIL_H_

/**
 * @file
 * @brief IronBee - Utility Functions
 *
 * @author Brian Rectanus <brectanus@qualys.com>
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>

#include <sys/types.h>
#include <unistd.h>

#include <netinet/in.h>
#include <arpa/inet.h>


#include <ironbee/build.h>
#include <ironbee/release.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup IronBeeUtil Utility
 * @ingroup IronBee
 * @{
 */

/* Type definitions. */
typedef struct ib_mpool_t ib_mpool_t;
typedef struct ib_dso_t ib_dso_t;
typedef void ib_dso_sym_t;
typedef struct ib_hash_t ib_hash_t;
typedef uint32_t ib_ftype_t;
typedef uint32_t ib_flags_t;
typedef uint64_t ib_flags64_t;
typedef intmax_t ib_num_t;
typedef uintmax_t ib_unum_t;
typedef struct ib_cfgmap_t ib_cfgmap_t;
typedef struct ib_cfgmap_init_t ib_cfgmap_init_t;
typedef struct ib_field_t ib_field_t;
typedef struct ib_field_val_t ib_field_val_t;
typedef struct ib_bytestr_t ib_bytestr_t;

/** Generic function pointer type. */
typedef void (*ib_void_fn_t)(void);


/** Status code. */
typedef enum ib_status_t {
    IB_OK,                          /**< No error */
    IB_DECLINED,                    /**< Declined execution */
    IB_EUNKNOWN,                    /**< Unknown error */
    IB_ENOTIMPL,                    /**< Not implemented (yet?) */
    IB_EINCOMPAT,                   /**< Incompatible with ABI version */
    IB_EALLOC,                      /**< Could not allocate resources */
    IB_EINVAL,                      /**< Invalid argument */
    IB_ENOENT,                      /**< Entity does not exist */
    IB_ETRUNC,                      /**< Buffer truncated, size limit reached */
    IB_ETIMEDOUT,                   /**< Operation timed out */
    IB_EAGAIN,                      /**< Not ready, try again later */
} ib_status_t;

/**
 * Convert string const to string and length parameters.
 *
 * Allows using a NUL terminated string in place of two parameters
 * (char*, len) by calling strlen().
 *
 * @param s String
 */
#define IB_S2SL(s)  (s), strlen(s)

/**
 * Convert string const to unsigned string and length parameters.
 *
 * Allows using a NUL terminated string in place of two parameters
 * (uint8_t*, len) by calling strlen().
 *
 * @param s String
 */
#define IB_S2USL(s)  ((uint8_t *)(s)), strlen(s)

/**
 * Parameter for string/length formatter.
 *
 * Allow using a string and length for %.*s style formatters.
 *
 * @todo Fix with escaping for at least NULs
 *
 * @param s String
 * @param l Length
 */
#define IB_BYTESTRSL_FMT_PARAM(s,l)  (int)(l), (const char *)(s)

/**
 * Parameter for byte string formatter.
 *
 * Allows using a ib_bytestr_t with %.*s style formatters.
 *
 * @todo Fix for ib_bytestr_t type with escaping for at least NULs
 *
 * @param bs Bytestring
 */
#define IB_BYTESTR_FMT_PARAM(bs)  (int)ib_bytestr_length(bs), (const char *)ib_bytestr_ptr(bs)

/** Printf style format string for bytestr. */
#define IB_BYTESTR_FMT         ".*s"



/**
 * Logger callback.
 *
 * @param cbdata Callback data
 * @param level Log level
 * @param prefix Optional prefix to the log
 * @param file Optional source filename (or NULL)
 * @param line Optional source line number (or 0)
 * @param fmt Formatting string
 */
typedef void (*ib_util_fn_logger_t)(void *cbdata, int level,
                                    const char *prefix,
                                    const char *file, int line,
                                    const char *fmt, va_list ap)
                                    VPRINTF_ATTRIBUTE(6);

/** Normal Logger. */
#define ib_util_log(lvl,...) ib_util_log_ex((lvl),"IronBee: ",NULL,0,__VA_ARGS__)

/** Error Logger. */
#define ib_util_log_error(lvl,...) ib_util_log_ex((lvl),"IronBeeUtil ERROR: ",NULL,0,__VA_ARGS__)

/** Abort Logger. */
#define ib_util_log_abort(...) do { ib_util_log_ex(0,"IronBeeUtil ABORT: ",__FILE__,__LINE__,__VA_ARGS__); abort(); } while(0)

/** Debug Logger. */
#define ib_util_log_debug(lvl,...) ib_util_log_ex((lvl),"IronBeeUtil DBG: ",__FILE__,__LINE__,__VA_ARGS__)

/**
 * Set the logger level.
 */
ib_status_t DLL_PUBLIC ib_util_log_level(int level);

/**
 * Set the logger.
 */
ib_status_t DLL_PUBLIC ib_util_log_logger(ib_util_fn_logger_t callback,
                                          void *cbdata);

/**
 * Write a log entry via the logger callback.
 *
 * @param level Log level (0-9)
 * @param prefix String to prefix log header data (or NULL)
 * @param file Filename (or NULL)
 * @param line Line number (or 0)
 * @param fmt Printf-like format string
 */
void DLL_PUBLIC ib_util_log_ex(int level, const char *prefix,
                               const char *file, int line,
                               const char *fmt, ...);

/**
 * Create a directory path recursivly.
 *
 * @param path Path to create
 * @param mode Mode to create directories with
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_util_mkpath(const char *path, mode_t mode);


/**
 * Initialize the IB lib.
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_initialize(void);

/**
 * Shutdown the IB lib.
 */
void DLL_PUBLIC ib_shutdown(void);


/**
 * @defgroup IronBeeUtilMemPool Memory Pool
 *
 * Memory pool routines.
 *
 * @{
 */

typedef ib_status_t (*ib_mpool_cleanup_fn_t)(void *);

/**
 * Create a new memory pool.
 *
 * @note If a pool has a parent specified, then any call to clear/destroy
 * on the parent will propagate to all descendants.
 *
 * @param pmp Address which new pool is written
 * @param parent Optional parent memory pool (or NULL)
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_mpool_create(ib_mpool_t **pmp, ib_mpool_t *parent);

/**
 * Allocate memory from a memory pool.
 *
 * @param mp Memory pool
 * @param size Size in bytes
 *
 * @returns Address of allocated memory
 */
void DLL_PUBLIC *ib_mpool_alloc(ib_mpool_t *mp, size_t size);

/**
 * Allocate memory from a memory pool and initialize to zero.
 *
 * @param mp Memory pool
 * @param nelem Number of elements to allocate
 * @param size Size of each element in bytes
 *
 * @returns Address of allocated memory
 */
void DLL_PUBLIC *ib_mpool_calloc(ib_mpool_t *mp, size_t nelem, size_t size);

/**
 * Duplicate a memory block.
 *
 * @param mp Memory pool
 * @param src Memory addr
 * @param size Size of memory
 *
 * @returns Address of duplicated memory
 */
void DLL_PUBLIC *ib_mpool_memdup(ib_mpool_t *mp, const void *src, size_t size);

/**
 * Deallocate all memory allocated from the pool and any descendant pools.
 *
 * @param mp Memory pool
 */
void DLL_PUBLIC ib_mpool_clear(ib_mpool_t *mp);

/**
 * Destroy pool and any descendant pools.
 *
 * @param mp Memory pool
 */
void DLL_PUBLIC ib_mpool_destroy(ib_mpool_t *mp);

/**
 * Register a function to be called after a memory pool is destroyed.
 *
 * @param mp Memory pool
 * @param data Data
 * @param cleanup Cleanup function
 */
void DLL_PUBLIC ib_mpool_cleanup_register(ib_mpool_t *mp, void *data,
                                          ib_mpool_cleanup_fn_t cleanup);

/** @} IronBeeUtilMemPool */


/**
 * @defgroup IronBeeUtilList List
 *
 * List routines.
 *
 * This is currently implemented as a doubly linked list.
 *
 * @{
 */

typedef struct ib_list_t ib_list_t;
typedef struct ib_list_node_t ib_list_node_t;

/**
 * Required fields for a list node structure.
 *
 * @todo Perhaps this should be a struct?
 *
 * @param ntype Node type literal
 */
#define IB_LIST_NODE_REQ_FIELDS(ntype) \
    ntype *next; /**< Next list node */ \
    ntype *prev /**< Previous list node */

/**
 * Required fields for a list structure.
 *
 * @todo Perhaps this should be a struct?
 *
 * @param ntype Node type literal
 */
#define IB_LIST_REQ_FIELDS(ntype) \
    size_t nelts; /**< Number of elements in list */ \
    ntype *head; /**< First node in list */ \
    ntype *tail /**< Last node in list */

/**
 * First node of a list.
 *
 * @param list List
 *
 * @returns List node
 */
#define IB_LIST_FIRST(list) ((list)->head)

/**
 * Last node of a list.
 *
 * @param list List
 *
 * @returns List node
 */
#define IB_LIST_LAST(list) ((list)->tail)

/**
 * Next node in a list in relation to another node.
 *
 * @param node Node
 *
 * @returns List node
 */
#define IB_LIST_NODE_NEXT(node) ((node) == NULL ? NULL : (node)->next)

/**
 * Previous node in a list in relation to another node.
 *
 * @param node Node
 *
 * @returns List node
 */
#define IB_LIST_NODE_PREV(node) ((node) == NULL ? NULL : (node)->prev)

/**
 * List node data.
 *
 * @param node Node
 *
 * @returns List node data
 */
#define IB_LIST_NODE_DATA(node) ((node) == NULL ? NULL : (node)->data)

/**
 * Insert a node after another node in a list.
 *
 * @param list List
 * @param at Node to insert after
 * @param node Node to insert
 * @param ntype Node type literal
 */
#define IB_LIST_NODE_INSERT_AFTER(list,at,node,ntype) \
    do { \
        ntype *__ib_list_node_ia_tmp = (at)->next; \
        (at)->next = (node); \
        (node)->prev = (at); \
        (node)->next = __ib_list_node_ia_tmp; \
        (list)->nelts++; \
    } while(0)

/**
 * Insert a node before another node in a list.
 *
 * @param list List
 * @param at Node to insert before
 * @param node Node to insert
 * @param ntype Node type literal
 */
#define IB_LIST_NODE_INSERT_BEFORE(list,at,node,ntype) \
    do { \
        ntype *__ib_list_node_ib_tmp = (at)->prev; \
        (at)->prev = (node); \
        (node)->prev = __ib_list_node_ib_tmp; \
        (node)->next = (at); \
        (list)->nelts++; \
    } while(0)

/**
 * Insert a node at the end of a list.
 *
 * @param list List
 * @param node Node to insert
 * @param ntype Node type literal
 */
#define IB_LIST_NODE_INSERT_LAST(list,node,ntype) \
    do { \
        IB_LIST_NODE_INSERT_AFTER((list), (list)->tail, (node), ntype); \
        (list)->tail = (node); \
    } while(0)

/**
 * Insert a node at the beginning of a list.
 *
 * @param list List
 * @param node Node to insert
 * @param ntype Node type literal
 */
#define IB_LIST_NODE_INSERT_FIRST(list,node,ntype) \
    do { \
        IB_LIST_NODE_INSERT_BEFORE((list), (list)->head, (node), ntype); \
        (list)->head = (node); \
    } while(0)

/**
 * Insert the first node of a list.
 *
 * @param list List
 * @param node Node to insert
 */
#define IB_LIST_NODE_INSERT_INITIAL(list,node) \
    do { \
        (list)->head = (list)->tail =(node); \
        (node)->next = (node)->prev = NULL; \
        (list)->nelts = 1; \
    } while(0)

/**
 * Remove a node from a list.
 *
 * @param list List
 * @param node Node to insert
 */
#define IB_LIST_NODE_REMOVE(list,node) \
    do { \
        if ((node)->prev != NULL) { \
            (node)->prev->next = (node)->next; \
        } \
        if ((node)->next != NULL) { \
            (node)->next->prev = (node)->prev; \
        } \
        (list)->nelts--; \
    } while(0)

/**
 * Remove the last node from a list.
 *
 * @param list List
 */
#define IB_LIST_NODE_REMOVE_LAST(list) \
    do { \
        if ((list)->tail != NULL) { \
            IB_LIST_NODE_REMOVE((list), (list)->tail); \
            (list)->tail = (list)->tail->prev; \
        } \
    } while(0)

/**
 * Remove the first node from a list.
 *
 * @param list List
 */
#define IB_LIST_NODE_REMOVE_FIRST(list) \
    do { \
        if ((list)->head != NULL) { \
            IB_LIST_NODE_REMOVE((list), (list)->head); \
            (list)->head = (list)->head->next; \
        } \
    } while(0)

/**
 * Loop through all elements in the list.
 *
 * @todo Make this generic (non-ib_list__t specific)
 *
 * @warning Do not use to delete an element in the list. Instead use
 *          the @ref IB_LIST_LOOP_SAFE loop.
 *
 * @param list List
 * @param node Symbol holding node
 */
#define IB_LIST_LOOP(list,node) \
    for ((node) = ib_list_first(list); \
         (node) != NULL; \
         (node) = ib_list_node_next(node))

/**
 * Loop through all elements in the list, taking care to allow for deletions.
 *
 * @todo Make this generic (non-ib_list__t specific)
 *
 * This loop allows deleting elements. If this is not needed, then
 * use the @ref IB_LIST_LOOP loop.
 *
 * @param list List
 * @param node Symbol holding node
 * @param node_next Symbol holding next node
 */
#define IB_LIST_LOOP_SAFE(list,node,node_next) \
    for ((node) = ib_list_first(list), \
           (node_next) = ib_list_node_next(node); \
         (node) != NULL; \
         (node) = (node_next), \
           (node_next) = ib_list_node_next(node))

/**
 * Loop through all elements in the list in reverse order.
 *
 * @todo Make this generic (non-ib_list__t specific)
 *
 * @warning Do not use to delete an element in the list. Instead use
 *          the @ref IB_LIST_LOOP_REVERSE_SAFE loop.
 *
 * @param list List
 * @param node Symbol holding node
 */
#define IB_LIST_LOOP_REVERSE(list,node) \
    for ((node) = ib_list_last(list); \
         (node) != NULL; \
         (node) = ib_list_node_prev(node))

/**
 * Loop through all elements in the list in reverse order, taking care
 * to allow for deletions.
 *
 * @todo Make this generic (non-ib_list__t specific)
 *
 * This loop allows deleting elements. If this is not needed, then
 * use the @ref IB_LIST_LOOP_REVERSE loop.
 *
 * @param list List
 * @param node Symbol holding node
 * @param node_next Symbol holding next node
 */
#define IB_LIST_LOOP_REVERSE_SAFE(list,node,node_next) \
    for ((node) = ib_list_last(list), \
           (node_next) = ib_list_node_prev(node); \
         (node) != NULL; \
         (node) = (node_next), \
           (node_next) = ib_list_node_prev(node))

/**
 * Create a list.
 *
 * @param plist Address which new list is written
 * @param pool Memory pool to use
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_list_create(ib_list_t **plist, ib_mpool_t *pool);

/**
 * Insert data at the end of a list.
 *
 * @param list List
 * @param data Data to store
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_list_push(ib_list_t *list, void *data);

/**
 * Fetch and remove data from the end of a list.
 *
 * @param list List
 * @param pdata Address which data is stored (if non-NULL)
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_list_pop(ib_list_t *list, void *pdata);

/**
 * Insert data at the first position of the list (queue behavior)
 *
 * This is currently just an alias for @ref ib_list_unshift().
 *
 * @param list List
 * @param data Address which data is stored (if non-NULL)
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_list_enqueue(ib_list_t *list, void *data);

#define ib_list_enqueue(list, data) \
    ib_list_unshift((list), (data))

/**
 * Fetch and remove data at the end of the list (queue behavior)
 *
 * This is currently just an alias for @ref ib_list_pop().
 *
 * @param list List
 * @param pdata Address which data is stored (if non-NULL)
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_list_dequeue(ib_list_t *list, void *pdata);

#define ib_list_dequeue(list, data) \
    ib_list_pop((list), (data))

/**
 * Insert data at the beginning of a list.
 *
 * @param list List
 * @param data Data to store
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_list_unshift(ib_list_t *list, void *data);

/**
 * Fetch and remove data from the beginning of a list.
 *
 * @param list List
 * @param pdata Address which data is stored (if non-NULL)
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_list_shift(ib_list_t *list, void *pdata);

/**
 * Clear a list.
 *
 * @note This does not destroy any element, but instead disassociates
 *       the elements with the list.
 *
 * @param list List
 */
void DLL_PUBLIC ib_list_clear(ib_list_t *list);

/**
 * Return number of elements stored in the list.
 *
 * @param list List
 *
 * @returns Number of elements stored in the list
 */
size_t DLL_PUBLIC ib_list_elements(ib_list_t *list);

/**
 * Return first node in the list.
 *
 * @param list List
 *
 * @returns First node in the list.
 */
ib_list_node_t DLL_PUBLIC *ib_list_first(ib_list_t *list);

/**
 * Return last node in the list.
 *
 * @param list List
 *
 * @returns Last node in the list.
 */
ib_list_node_t DLL_PUBLIC *ib_list_last(ib_list_t *list);

/**
 * Return next node in the list.
 *
 * @param node Node in a list
 *
 * @returns Next node in the list.
 */
ib_list_node_t DLL_PUBLIC *ib_list_node_next(ib_list_node_t *node);

/**
 * Return previous node in the list.
 *
 * @param node Node in a list
 *
 * @returns Previous node in the list.
 */
ib_list_node_t DLL_PUBLIC *ib_list_node_prev(ib_list_node_t *node);

/**
 * Remove a node from the list.
 *
 * @param list List
 * @param node Node in a list
 */
void DLL_PUBLIC ib_list_node_remove(ib_list_t *list, ib_list_node_t *node);

/**
 * Return data from the given node.
 *
 * @param node Node in a list
 *
 * @returns Data stored in the node
 */
void DLL_PUBLIC *ib_list_node_data(ib_list_node_t *node);

/** @} IronBeeUtilList */

/**
 * @defgroup IronBeeUtilArray Dynamic Array
 *
 * Dynamic array routines.
 *
 * @{
 */

typedef struct ib_array_t ib_array_t;

/**
 * Create an array.
 *
 * The array will be extended by "ninit" elements when more room is required.
 * Up to "nextents" extents will be performed.  If more than this number of
 * extents is required, then "nextents" will be doubled and the array will
 * be reorganized.
 *
 * @param parr Address which new array is written
 * @param pool Memory pool to use
 * @param ninit Initial number of elements
 * @param nextents Initial number of extents
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_array_create(ib_array_t **parr, ib_mpool_t *pool,
                                       size_t ninit, size_t nextents);

/**
 * Get an element from an array at a given index.
 *
 * If the array is not big enough to hold the index, then IB_EINVAL is
 * returned and pval will be NULL.
 *
 * @param arr Array
 * @param idx Index
 * @param pval Address which element is written
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_array_get(ib_array_t *arr, size_t idx, void *pval);

/**
 * Set an element from an array at a given index.
 *
 * If the array is not big enough to hold the index, then it will be extended
 * by ninit until it is at least this value before setting the value.
 *
 * @note The element is added without copying.
 *
 * @param arr Array
 * @param idx Index
 * @param val Value
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_array_setn(ib_array_t *arr, size_t idx, void *val);

/**
 * Append an element to the end of the array.
 *
 * If the array is not big enough to hold the index, then it will be extended
 * by ninit first.
 *
 * @note The element is added without copying.
 *
 * @param arr Array
 * @param val Value
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_array_appendn(ib_array_t *arr, void *val);

/**
 * Number of elements in an array.
 *
 * @param arr Array
 *
 * @returns Number of elements
 */
size_t DLL_PUBLIC ib_array_elements(ib_array_t *arr);

/**
 * Allocated space in the array.
 *
 * @param arr Array
 *
 * @returns Max number of elements before needing to extend.
 */
size_t DLL_PUBLIC ib_array_size(ib_array_t *arr);


/**
 * Dynamic array loop.
 *
 * This just loops over the indexes in a dynamic array.
 *
 * @code
 * // Where data stored in "arr" is "int *", this will print all int values.
 * size_t ne;
 * size_t idx;
 * int *val;
 * IB_ARRAY_LOOP(arr, ne, idx, val) {
 *     printf("%4d: item[%p]=%d\n", i++, val, *val);
 * }
 * @endcode
 *
 * @param arr Array
 * @param ne Symbol holding the number of elements
 * @param idx Symbol holding the index, set for each iteration
 * @param val Symbol holding the value at the index, set for each iteration
 */
#define IB_ARRAY_LOOP(arr,ne,idx,val) \
    for ((ne)=ib_array_elements(arr), (idx)=0; \
         ib_array_get((arr),(idx),(void *)&(val))==IB_OK && (idx)<(ne); \
         (idx)++)

/** @} IronBeeUtilArray */


/**
 * @defgroup IronBeeHash Hashtable
 *
 * @{
 */

typedef unsigned int (*ib_hashfunc_t)(const void *key, size_t len,
                                      uint8_t flags);

#define IB_HASH_INITIAL_SIZE   15

/* Options */
#define IB_HASH_FLAG_NOCASE    0x01 /**< Ignore case lookup */

typedef struct ib_hash_entry_t ib_hash_entry_t;
typedef struct ib_hash_iter_t ib_hash_iter_t;

/**
 * Create a hash table with nocase option by default.
 *
 * If you do not need key case insensitivity, use @ref ib_hash_create_ex()
 *
 * @param ph Address which new hash table is written
 * @param pool Memory pool to use
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_hash_create(ib_hash_t **ph, ib_mpool_t *pool);

#define ib_hash_create(ph,pool) \
    ib_hash_create_ex(ph, pool, IB_HASH_INITIAL_SIZE, IB_HASH_FLAG_NOCASE)

/**
 * DJB2 Hash Function (Dan Bernstein).
 *
 * This is the default hash function.
 *
 * @param key buffer holding the key to hash
 * @param len size of the key to hash in bytes
 * @param flags bit flag options for the key
 *              (currently IB_HASH_FLAG_NOCASE)
 *
 * @returns Status code
 */
unsigned int DLL_PUBLIC ib_hashfunc_djb2(const void *char_key,
                                         size_t len,
                                         uint8_t flags);

/**
 * Create a hash table with nocase option by default.
 * If you dont need it, use ib_hash_create_ex
 *
 * @param ph Address which new hash table is written
 * @param pool Memory pool to use
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_hash_create_ex(ib_hash_t **ht,
                                         ib_mpool_t *pool,
                                         int slots,
                                         uint8_t flags);

/**
 * @internal
 * Seach an entry for the given key and key length
 * The hash used to search the key will be also returned via param
 *
 * @param ib_ht the hash table to search in
 * @param key buffer holding the key
 * @param len number of bytes key length
 * @param hte pointer reference used to store the entry if found
 * @param hash reference to store the calculated hash
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_hash_find_entry(ib_hash_t *ib_ht,
                                          const void *key,
                                          size_t len,
                                          ib_hash_entry_t **hte,
                                          unsigned int *hash,
                                          uint8_t lookup_flags);

/**
 * Clear a hash table.
 *
 * @param h Hash table
 */
void DLL_PUBLIC ib_hash_clear(ib_hash_t *h);

/**
 * Get data from a hash table via key and key length.
 *
 * @param h Hash table
 * @param key Key to lookup
 * @param len Length of key
 * @param pdata Address which data is written
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_hash_get_ex(ib_hash_t *h,
                                      void *key, size_t len,
                                      void *pdata,
                                      uint8_t lookup_flags);

/**
 * Get data from a hash table via NUL terminated key.
 *
 * @param h Hash table
 * @param key Key to lookup
 * @param pdata Address which data is written
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_hash_get(ib_hash_t *h,
                                   const char *key,
                                   void *pdata);

/**
 * Get data from a hash table via NUL terminated key with ignore
 * case option set.
 *
 * @param h Hash table
 * @param key Key to lookup
 * @param pdata Address which data is written
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_hash_get_nocase(ib_hash_t *h,
                                   const char *key,
                                   void *pdata);

/**
 * Get all data from a hash table and push onto the supplied list.
 *
 * @param h Hash table
 * @param list List to push values
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_hash_get_all(ib_hash_t *h, ib_list_t *list);

/**
 * Set data in a hash table for key and key length.
 *
 * @param h Hash table
 * @param key Key to lookup
 * @param len Length of key
 * @param data Data
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_hash_set_ex(ib_hash_t *h,
                                      const void *key,
                                      size_t len,
                                      const void *data);

/**
 * Set data in a hash table via key (string).
 *
 * @param h Hash table
 * @param key Key to lookup
 * @param data Data
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_hash_set(ib_hash_t *h,
                                   const char *key,
                                   void *data);

/**
 * Remove data from a hash table via key and key length.
 *
 * @param h Hash table
 * @param key Key to lookup
 * @param len Length of key
 * @param pdata Address which data is written (or NULL if not required)
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_hash_remove_ex(ib_hash_t *h,
                                         void *key,
                                         size_t len,
                                         void *pdata);

/**
 * Remove data from a hash table via key (string).
 *
 * @param h Hash table
 * @param key Key to lookup
 * @param pdata Address which data is written (or NULL if not required)
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_hash_remove(ib_hash_t *h,
                                      const char *key,
                                      void *pdata);

#define ib_hash_remove(h,key,pdata) \
    ib_hash_remove_ex((h),(void *)(key),strlen(key),(pdata))


/**
 * Creates an initialized iterator for the hash table entries.
 *
 * @param mp Memory pool for the iterator allocation
 * @param ib_ht hash table to iterate
 *
 * @returns Status code
 */
ib_hash_iter_t DLL_PUBLIC *ib_hash_first(ib_mpool_t *p,
                                         ib_hash_t *ib_ht);

/**
 * Move the iterator to the next entry.
 *
 * @param hti hash table iterator
 *
 * @returns Status code
 */
ib_hash_iter_t DLL_PUBLIC *ib_hash_next(ib_hash_iter_t *hti);

/** @} IronBeeUtilHash */


/**
 * @defgroup IronBeeUtilField Field
 *
 * A field is arbitrary data with a given type and length.
 *
 * @{
 */

#define IB_FTYPE_GENERIC      0          /**< Generic pointer value */
#define IB_FTYPE_NUM          1          /**< Numeric value */
#define IB_FTYPE_UNUM         2          /**< Unsigned numeric value */
#define IB_FTYPE_NULSTR       3          /**< NUL terminated string value */
#define IB_FTYPE_BYTESTR      4          /**< Binary data value */
#define IB_FTYPE_LIST         5          /**< List of fields */

/**
 * Dynamic field get function.
 *
 * @param f Field
 * @param data Userdata
 *
 * @returns Value
 */
typedef void *(*ib_field_get_fn_t)(ib_field_t *f,
                                   void *arg, size_t alen,
                                   void *data);

#if 0
/**
 * Dynamic field set function.
 *
 * @param f Field
 * @param val Value to set
 * @param data Userdata
 *
 * @returns Old value
 */
typedef void *(*ib_field_set_fn_t)(ib_field_t *f,
                                   void *arg, size_t alen,
                                   void *val, void *data);

/**
 * Dynamic field relative set function.
 *
 * @param f Field
 * @param val Numeric incrementor value
 * @param data Userdata
 *
 * @returns Old value
 */
typedef void *(*ib_field_rset_fn_t)(ib_field_t *f,
                                    void *arg, size_t alen,
                                    intmax_t val, void *data);
#endif


/** Field Structure */
struct ib_field_t {
    ib_mpool_t               *mp;        /**< Memory pool */
    ib_ftype_t                type;      /**< Field type */
    const char               *name;      /**< Field name */
    size_t                    nlen;      /**< Field name length */
    const char               *tfn;       /**< Transformations performed */
    ib_field_val_t           *val;       /**< Private value store */
};

/**
 * Create a field, copying name/data into the field.
 *
 * @param pf Address which new field is written
 * @param mp Memory pool
 * @param name Field name as byte string
 * @param nlen Field name length
 * @param type Field type
 * @param pval Pointer to value to store in field (based on type)
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_field_create_ex(ib_field_t **pf,
                                          ib_mpool_t *mp,
                                          const char *name,
                                          size_t nlen,
                                          ib_ftype_t type,
                                          void *pval);

/**
 * Create a field and store data without copying.
 *
 * @param pf Address which new field is written
 * @param mp Memory pool
 * @param name Field name as byte string
 * @param nlen Field name length
 * @param type Field type
 * @param pval Pointer to value to store in field (based on type)
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_field_createn_ex(ib_field_t **pf,
                                           ib_mpool_t *mp,
                                           const char *name,
                                           size_t nlen,
                                           ib_ftype_t type,
                                           void *pval);

/**
 * Make a copy of a field.
 *
 * @param pf Address which new field is written
 * @param mp Memory pool
 * @param name Field name as byte string
 * @param nlen Field name length
 * @param src Source field to copy
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_field_copy_ex(ib_field_t **pf,
                                        ib_mpool_t *mp,
                                        const char *name,
                                        size_t nlen,
                                        ib_field_t *src);

/**
 * Create a bytestr field which directly aliases a value in memory.
 *
 * @param pf Address which new field is written
 * @param mp Memory pool
 * @param name Field name as byte string
 * @param nlen Field name length
 * @param val Value
 * @param vlen Value length
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_field_alias_mem_ex(ib_field_t **pf,
                                             ib_mpool_t *mp,
                                             const char *name,
                                             size_t nlen,
                                             uint8_t *val,
                                             size_t vlen);

/**
 * Create a field, copying data into the field.
 *
 * @param pf Address which new field is written
 * @param mp Memory pool
 * @param name Field name as string
 * @param type Field type
 * @param pval Pointer to value to store in field (based on type)
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_field_create(ib_field_t **pf,
                                       ib_mpool_t *mp,
                                       const char *name,
                                       ib_ftype_t type,
                                       void *pval);

#define ib_field_create(pf,mp,name,type,pval) \
    ib_field_create_ex(pf,mp,name,strlen(name),type,pval)

/**
 * Create a field and store data without making a copy.
 *
 * @param pf Address which new field is written
 * @param mp Memory pool
 * @param name Field name as string
 * @param type Field type
 * @param pval Pointer to value to store in field (based on type)
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_field_createn(ib_field_t **pf,
                                        ib_mpool_t *mp,
                                        const char *name,
                                        ib_ftype_t type,
                                        void *pval);

#define ib_field_createn(pf,mp,name,type,pval) \
    ib_field_createn_ex(pf,mp,name,strlen(name),type,pval)

/**
 * Make a copy of a field.
 *
 * @param pf Address which new field is written
 * @param mp Memory pool
 * @param name Field name as byte string
 * @param src Source field to copy
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_field_copy(ib_field_t **pf,
                                     ib_mpool_t *mp,
                                     const char *name,
                                     ib_field_t *src);

#define ib_field_copy(pf,mp,name,type,src) \
    ib_field_copy(pf,mp,name,strlen(name),type,src)

/**
 * Create a bytestr field which directly aliases a value in memory.
 *
 * @param pf Address which new field is written
 * @param mp Memory pool
 * @param name Field name as byte string
 * @param val Value
 * @param vlen Value length
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_field_alias_mem(ib_field_t **pf,
                                          ib_mpool_t *mp,
                                          const char *name,
                                          uint8_t *val,
                                          size_t vlen);

#define ib_field_alias_mem(pf,mp,name,val,vlen) \
    ib_field_alias_mem_ex(pf,mp,name,strlen(name),val,vlen)

/**
 * Add a field to a IB_FTYPE_LIST field.
 * 
 * @param f Field list
 * @param val Field to add to the list
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_field_list_add(ib_field_t *f,
                                         ib_field_t *val);

/**
 * Set a field value.
 * 
 * @param f Field to add
 * @param pval Pointer to value to store in field (based on type)
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_field_setv(ib_field_t *f,
                                     void *pval);


/**
 * Get the value stored in the field.
 *
 * @param f Field
 *
 * @returns Value stored in the field
 */
void DLL_PUBLIC *ib_field_value(ib_field_t *f);

/** Return field value for a field as "ib_num_t *". */
#define ib_field_value_num(f) (ib_num_t *)ib_field_value(f)

/** Return field value for a field as "ib_unum_t *". */
#define ib_field_value_unum(f) (ib_unum_t *)ib_field_value(f)

/** Return field value for a field as "ib_bytestr_t *". */
#define ib_field_value_bytestr(f) (ib_bytestr_t *)ib_field_value(f)

/** Return field value for a field as "void *". */
#define ib_field_value_generic(f) (void *)ib_field_value(f)

/** Return field value for a field as "char *". */
#define ib_field_value_nulstr(f) (char *)ib_field_value(f)

/** Return field value for a field as "ib_list_t *". */
#define ib_field_value_list(f) (ib_list_t *)ib_field_value(f)


/**
 * Set userdata for dynamic field access.
 *
 * @param f Field
 * @param data Userdata
 */
void DLL_PUBLIC ib_field_dyn_set_data(ib_field_t *f,
                                      void *data);

/**
 * Register dynamic get function.
 *
 * @param f Field
 * @param fn_get Userdata
 */
void DLL_PUBLIC ib_field_dyn_register_get(ib_field_t *f,
                                          ib_field_get_fn_t fn_get);

#if 0
/**
 * Register dynamic set function.
 *
 * @param f Field
 * @param fn_set Userdata
 */
void DLL_PUBLIC ib_field_dyn_register_set(ib_field_t *f,
                                          ib_field_set_fn_t fn_set);

/**
 * Register dynamic rset function.
 *
 * @param f Field
 * @param fn_rset Userdata
 */
void DLL_PUBLIC ib_field_dyn_register_rset(ib_field_t *f,
                                           ib_field_rset_fn_t fn_rset);
#endif

/**
 * @} IronBeeUtilField
 */


/**
 * @defgroup IronBeeUtilCfgMap Configuration Map
 *
 * Configuration map routines.
 *
 * This is a wrapper around a hash specifically targeted
 * at configuration data, making it easier to deal with storing specific
 * types of data and accessing the data faster.  CfgMap data can be
 * accessed directly (and simultaneously) via a structure and hash
 * like syntax. It is meant to expose the hash syntax externally while
 * being able to access the structure internally without the overhead
 * of the hash functions.
 *
 * To accomplish this, the creator is responsible for allocating
 * a fixed length base data structure (C struct) and supply a mapping as to
 * the locations (offsets within the struct) of each configuration field.
 *
 * @{
 */


/**
 * Configuration Map
 */
struct ib_cfgmap_t {
    ib_mpool_t         *mp;           /**< Memory pool */
    ib_hash_t          *hash;         /**< The underlying hash */
    ib_cfgmap_init_t   *data;         /**< Initialization mapping */
};

/** Config map initialization structure. */
struct ib_cfgmap_init_t {
    const char          *name;     /**< Field name */
    ib_ftype_t           type;     /**< Field type */
    off_t                offset;   /**< Field data offset within base */
    size_t               dlen;     /**< Field data length (<= uintptr_t) */
    uintptr_t            defval;   /**< Default value */
};

/**
 * Create a configuration map.
 *
 * @param pcm Address which new map is written
 * @param pool Memory pool to use
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_cfgmap_create(ib_cfgmap_t **pcm,
                                        ib_mpool_t *pool);

/**
 * Defines a configuration map initialization structure.
 *
 * @param name Name of structure
 */
#define IB_CFGMAP_INIT_STRUCTURE(name) const ib_cfgmap_init_t name[]

/**
 * Defines a configuration map entry.
 *
 * @param name Configuration entry name
 * @param type Configuration entry data type
 * @param base Base address of the structure holding the values
 * @param field Field name in structure holding values
 * @param defval Default value of entry
 */
#define IB_CFGMAP_INIT_ENTRY(name,type,base,field,defval) \
    { (name), (type), (off_t)(((uintptr_t)&((base)->field)) - ((uintptr_t)(base))), sizeof((base)->field), (uintptr_t)(defval) }

/**
 * Required as the last entry.
 */
#define IB_CFGMAP_INIT_LAST { NULL }


/**
 * Initialize a configuration map with entries.
 *
 * @param cm Configuration map
 * @param base Base address of the structure holding the values
 * @param init Configuration map initialization structure
 * @param usedefaults If true, use the map default values as base
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_cfgmap_init(ib_cfgmap_t *cm,
                                      const void *base,
                                      const ib_cfgmap_init_t *init,
                                      int usedefaults);


/**
 * Clear a configuration map.
 *
 * @param cm Map
 */
void DLL_PUBLIC ib_cfgmap_clear(ib_cfgmap_t *cm);

/**
 * Set a configuration value.
 *
 * @param cm Configuration map
 * @param name Configuration entry name
 * @param data Data to assign to entry
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_cfgmap_set(ib_cfgmap_t *cm,
                                     const char *name,
                                     void *data);

/**
 * Get a configuration value.
 *
 * @param cm Configuration map
 * @param name Configuration entry name
 * @param pval Address which the value is written
 * @param ptype Address which the value type is written if non-NULL
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_cfgmap_get(ib_cfgmap_t *cm,
                                     const char *name,
                                     void *pval, ib_ftype_t *ptype);

/** @} IronBeeUtilCfgMap */

/**
 * @defgroup IronBeeUtilDso Dynamic Shared Object (DSO)
 * @{
 */


/**
 * Open a dynamic shared object (DSO) from a file.
 *
 * @param pdso DSO handle is stored in *dso
 * @param file DSO filename
 * @param pool Memory pool to use
 *
 * @returns Status code.
 */
ib_status_t DLL_PUBLIC ib_dso_open(ib_dso_t **pdso,
                                   const char *file,
                                   ib_mpool_t *pool);


/**
 * Close a dynamic shared object (DSO).
 *
 * @param dso DSO handle is stored in *dso
 *
 * @returns Status code.
 */
ib_status_t DLL_PUBLIC ib_dso_close(ib_dso_t *dso);


/**
 * Find a given symbol in a dynamic shared object (DSO).
 *
 * @param dso DSO handle
 * @param name DSO symbol name
 * @param psym DSO symbol handle is stored in *sym
 *
 * @returns Status code.
 */
ib_status_t DLL_PUBLIC ib_dso_sym_find(ib_dso_t *dso,
                                       const char *name,
                                       ib_dso_sym_t **psym);

/** @} IronBeeUtilDso */

/**
 * @defgroup IronBeeUtilDebug Debugging
 * @{
 */

#ifdef IB_DEBUG
/**
 * Initialize tracing system.
 *
 * @todo This will evolve into something much more useful.
 *
 * @param fn Filename to log tracing to (or NULL for default)
 */
void DLL_PUBLIC ib_trace_init(const char *fn);

/**
 * @internal
 * Log a generic trace message.
 *
 * @param file Source filename (typically __FILE__)
 * @param line Source line (typically __LINE__)
 * @param func Source function name
 * @param msg Message to log
 */
void DLL_PUBLIC ib_trace_msg(const char *file,
                             int line,
                             const char *func,
                             const char *msg);

/**
 * @internal
 * Log a numeric trace message.
 *
 * @param file Source filename (typically __FILE__)
 * @param line Source line (typically __LINE__)
 * @param func Source function name
 * @param msg Message to log
 * @param num Number to log
 */
void DLL_PUBLIC ib_trace_num(const char *file,
                             int line,
                             const char *func,
                             const char *msg,
                             intmax_t num);

/**
 * @internal
 * Log a unsigned numeric trace message.
 *
 * @param file Source filename (typically __FILE__)
 * @param line Source line (typically __LINE__)
 * @param func Source function name
 * @param msg Message to log
 * @param unum Unsigned number to log
 */
void DLL_PUBLIC ib_trace_unum(const char *file,
                              int line,
                              const char *func,
                              const char *msg,
                              uintmax_t unum);

/**
 * @internal
 * Log a pointer address trace message.
 *
 * @param file Source filename (typically __FILE__)
 * @param line Source line (typically __LINE__)
 * @param func Source function name
 * @param msg Message to log
 * @param ptr Address to log
 */
void DLL_PUBLIC ib_trace_ptr(const char *file,
                             int line,
                             const char *func,
                             const char *msg,
                             void *ptr);

/**
 * @internal
 * Log a string trace message.
 *
 * @param file Source filename (typically __FILE__)
 * @param line Source line (typically __LINE__)
 * @param func Source function name
 * @param msg Message to log
 * @param str String to log
 */
void DLL_PUBLIC ib_trace_str(const char *file,
                             int line,
                             const char *func,
                             const char *msg,
                             const char *str);

/**
 * Initialize function tracing for a function.
 *
 * This should be the first line of a function and is required before
 * any other ftrace macro is used.
 *
 * @code
 * ib_status_t my_func_name(int foo)
 * {
 *     IB_FTRACE_INIT(my_func_name);
 *     ...
 *     IB_FTRACE_RET_STATUS(IB_OK);
 * }
 * @endcode
 *
 * @param name Name of function
 */
#define IB_FTRACE_INIT(name) \
    const char *__ib_fname__ = IB_XSTRINGIFY(name); \
    ib_trace_msg(__FILE__, __LINE__, __ib_fname__, "called")
    
/**
 * Logs a string message to the ftrace log.
 *
 * @param msg String message
 */
#define IB_FTRACE_MSG(msg) \
    ib_trace_msg(__FILE__, __LINE__, __ib_fname__, (msg))

/**
 * Return wrapper for functions which do not return a value.
 */
#define IB_FTRACE_RET_VOID() \
    ib_trace_msg(__FILE__, __LINE__, __ib_fname__, "returned"); \
    return

/**
 * Return wrapper for functions which return a @ref ib_status_t value.
 *
 * @param rv Return value
 */
#define IB_FTRACE_RET_STATUS(rv) \
    do { \
        ib_status_t __ib_ft_rv = rv; \
        if (__ib_ft_rv != IB_OK) { \
            ib_trace_num(__FILE__, __LINE__, __ib_fname__, "returned error", (intmax_t)__ib_ft_rv); \
        } \
        else { \
            ib_trace_num(__FILE__, __LINE__, __ib_fname__, "returned success", (intmax_t)__ib_ft_rv); \
        } \
        return __ib_ft_rv; \
    } while(0)

/**
 * Return wrapper for functions which return an int value.
 *
 * @param rv Return value
 */
#define IB_FTRACE_RET_INT(rv) \
    do { \
        int __ib_ft_rv = rv; \
        ib_trace_num(__FILE__, __LINE__, __ib_fname__, "returned", (intmax_t)__ib_ft_rv); \
        return __ib_ft_rv; \
    } while(0)

/**
 * Return wrapper for functions which return a size_t value.
 *
 * @param rv Return value
 */
#define IB_FTRACE_RET_SIZET(rv) \
    do { \
        size_t __ib_ft_rv = rv; \
        ib_trace_num(__FILE__, __LINE__, __ib_fname__, "returned", (intmax_t)__ib_ft_rv); \
        return __ib_ft_rv; \
    } while(0)

/**
 * Return wrapper for functions which return a pointer value.
 *
 * @param type The type of pointer to return
 * @param rv Return value
 */
#define IB_FTRACE_RET_PTR(type,rv) \
    do { \
        type *__ib_ft_rv = rv; \
        ib_trace_ptr(__FILE__, __LINE__, __ib_fname__, "returned", (void *)__ib_ft_rv); \
        return __ib_ft_rv; \
    } while(0)

/**
 * Return wrapper for functions which return a string value.
 * 
 * @param rv Return value
 */
#define IB_FTRACE_RET_STR(rv) \
    do { \
        char *__ib_ft_rv = rv; \
        ib_trace_str(__FILE__, __LINE__, __ib_fname__, "returned", (const char *)__ib_ft_rv); \
        return __ib_ft_rv; \
    } while(0)

/**
 * Return wrapper for functions which return a constant string value.
 * 
 * @param rv Return value
 */
#define IB_FTRACE_RET_CONSTSTR(rv) \
    do { \
        const char *__ib_ft_rv = rv; \
        ib_trace_str(__FILE__, __LINE__, __ib_fname__, "returned", __ib_ft_rv); \
        return __ib_ft_rv; \
    } while(0)

#else
#define ib_trace_init(fn)
#define ib_trace_msg(file,line,func,msg)
#define ib_trace_num(file,line,func,msg,num)
#define ib_trace_unum(file,line,func,msg,unum)
#define ib_trace_ptr(file,line,func,msg,ptr)
#define ib_trace_str(file,line,func,msg,str)

#define IB_FTRACE_INIT(name)
#define IB_FTRACE_MSG(msg)
#define IB_FTRACE_RET_VOID() return
#define IB_FTRACE_RET_STATUS(rv) return (rv)
#define IB_FTRACE_RET_INT(rv) return (rv)
#define IB_FTRACE_RET_UINT(rv) return (rv)
#define IB_FTRACE_RET_SIZET(rv) return (rv)
#define IB_FTRACE_RET_PTR(type,rv) return (rv)
#define IB_FTRACE_RET_STR(rv) return (rv)
#define IB_FTRACE_RET_CONSTSTR(rv) return (rv)
#endif /* IB_DEBUG */

/** @} IronBeeUtilDebug */

/**
 * @defgroup IronBeeUtilByteStr Byte String
 * @{
 */

#define IB_BYTESTR_FREADONLY           (1<<0)

#define IB_BYTESTR_CHECK_FREADONLY(f)  ((f) & IB_BYTESTR_FREADONLY)

/**
 * Create a byte string.
 *
 * @param pdst Address which new bytestring is written
 * @param pool Memory pool
 * @param size Size allocated for byte string
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_bytestr_create(ib_bytestr_t **pdst,
                                         ib_mpool_t *pool,
                                         size_t size);

/**
 * Create a byte string as a copy of another byte string.
 *
 * @param pdst Address which new bytestring is written
 * @param pool Memory pool
 * @param src Byte string to duplicate
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_bytestr_dup(ib_bytestr_t **pdst,
                                      ib_mpool_t *pool,
                                      const ib_bytestr_t *src);

/**
 * Create a byte string as a copy of a memory address and length.
 *
 * @param pdst Address which new bytestring is written
 * @param pool Memory pool
 * @param data Memory address which contains the data
 * @param dlen Length of data
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_bytestr_dup_mem(ib_bytestr_t **pdst,
                                          ib_mpool_t *pool,
                                          const uint8_t *data,
                                          size_t dlen);

/**
 * Create a byte string as a copy of a NUL terminated string.
 *
 * @param pdst Address which new bytestring is written
 * @param pool Memory pool
 * @param data String to duplicate
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_bytestr_dup_nulstr(ib_bytestr_t **pdst,
                                             ib_mpool_t *pool,
                                             const char *data);

/**
 * Create a byte string that is an alias (contains a reference) to the
 * data in another byte string.
 *
 * If either bytestring is modified, both will see the change as they
 * will both reference the same address.
 *
 * @param pdst Address which new bytestring is written
 * @param pool Memory pool
 * @param src Byte string to alias
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_bytestr_alias(ib_bytestr_t **pdst,
                                        ib_mpool_t *pool,
                                        const ib_bytestr_t *src);

/**
 * Create a byte string that is an alias (contains a reference) to the
 * data at a given memory location with a given length.
 *
 * If either the bytestring or memory is modified, both will see the change
 * as they will both reference the same address.
 *
 * @param pdst Address which new bytestring is written
 * @param pool Memory pool
 * @param data Memory address which contains the data
 * @param dlen Length of data
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_bytestr_alias_mem(ib_bytestr_t **pdst,
                                            ib_mpool_t *pool,
                                            const uint8_t *data,
                                            size_t dlen);

/**
 * Create a byte string that is an alias (contains a reference) to the
 * data in a NUL terminated string.
 *
 * If either the bytestring or string is modified, both will see the change
 * as they will both reference the same address.
 *
 * @param pdst Address which new bytestring is written
 * @param pool Memory pool
 * @param data String to alias
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_bytestr_alias_nulstr(ib_bytestr_t **pdst,
                                               ib_mpool_t *pool,
                                               const char *data);

/**
 * Extend a bytestring by appending the data from another bytestring.
 *
 * @param dst Bytestring which will have data appended
 * @param src Byte string which data is copied
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_bytestr_append(ib_bytestr_t *dst,
                                         const ib_bytestr_t *src);

/**
 * Extend a bytestring by appending the data from a memory address with
 * a given length.
 *
 * @param dst Bytestring which will have data appended
 * @param data Memory address containing the data
 * @param dlen Length of data
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_bytestr_append_mem(ib_bytestr_t *dst,
                                             const uint8_t *data,
                                             size_t dlen);

/**
 * Extend a bytestring by appending the data from a NUL terminated string.
 *
 * @note The NUL is not copied.
 *
 * @param dst Bytestring which will have data appended
 * @param data String containing data to be appended
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_bytestr_append_nulstr(ib_bytestr_t *dst,
                                                const char *data);

/**
 * Length of the data in a byte string.
 *
 * @param bs Byte string
 *
 * @returns Length of data in bytestring.
 */
size_t DLL_PUBLIC ib_bytestr_length(ib_bytestr_t *bs);

/**
 * Allocated size of the data in a byte string.
 *
 * @param bs Byte string
 *
 * @returns Allocated size of data in bytestring.
 */
size_t DLL_PUBLIC ib_bytestr_size(ib_bytestr_t *bs);

/**
 * Raw buffer containing data in a byte string.
 *
 * @param bs Byte string
 *
 * @returns Address of byte string buffer
 */
uint8_t DLL_PUBLIC *ib_bytestr_ptr(ib_bytestr_t *bs);

/** @} IronBeeUtilByteStr */

/**
 * @defgroup IronBeeUtilRadix
 * @{
 */

typedef struct ib_radix_t ib_radix_t;
typedef struct ib_radix_prefix_t ib_radix_prefix_t;
typedef struct ib_radix_node_t ib_radix_node_t;

typedef void (*ib_radix_update_fn_t)(ib_radix_node_t*, void*);
typedef void (*ib_radix_print_fn_t)(void*);
typedef void (*ib_radix_free_fn_t)(void*);

/**
 * Creates a new prefix instance
 *
 * @param prefix reference to a pointer that will link to the allocated prefix
 * @param pool memory pool that the allocation should use
 *
 * @returns Status code
 */
ib_status_t ib_radix_prefix_new(ib_radix_prefix_t **prefix,
                                ib_mpool_t *pool);

/**
 * Creates a new prefix instance
 *
 * @param prefix reference to a pointer that will link to the allocated prefix
 * @param rawbits sequence of bytes representing the key prefix
 * @param prefixlen size in bits with the len of the prefix
 * @param pool memory pool that the allocation should use
 *
 * @returns Status code
 */
ib_status_t ib_radix_prefix_create(ib_radix_prefix_t **prefix,
                                   uint8_t *rawbits,
                                   uint8_t prefixlen,
                                   ib_mpool_t *pool);

/**
 * creates a clone of the prefix instance
 *
 * @param orig pointer to the original prefix
 * @param new_prefix reference to a pointer to the allocated prefix
 * @param mp memory pool that the allocation should use
 *
 * @returns Status code
 */
ib_status_t ib_radix_clone_prefix(ib_radix_prefix_t *orig,
                                  ib_radix_prefix_t **new_prefix,
                                  ib_mpool_t *mp);

/**
 * destroy a prefix
 *
 * @param prefix to destroy
 * @param pool memory of the prefix
 *
 * @returns Status code
 */
ib_status_t ib_radix_prefix_destroy(ib_radix_prefix_t **prefix,
                                    ib_mpool_t *pool);


/**
 * Creates a new node instance
 *
 * @param node reference to a pointer that will link to the allocated node
 * @param pool memory pool that the allocation should use
 *
 * @returns Status code
 */
ib_status_t ib_radix_node_new(ib_radix_node_t **node,
                              ib_mpool_t *pool);

/**
 * creates a clone of the node instance
 *
 * @param orig pointer to the original node
 * @param new_node reference to a pointer that will link to the allocated node
 * @param mp memory pool that the allocation should use
 *
 * @returns Status code
 */
ib_status_t ib_radix_clone_node(ib_radix_node_t *orig,
                                ib_radix_node_t **new_node,
                                ib_mpool_t *mp);

/**
 * Destroy a node and its childs (this includes prefix and userdatas)
 *
 * @param radix the radix of the node
 * @param node the node to destroy
 * @param pool memory pool of the node
 *
 * @returns Status code
 */
ib_status_t ib_radix_node_destroy(ib_radix_t *radix,
                                  ib_radix_node_t **node,
                                  ib_mpool_t *pool);

/**
 * Creates a new radix tree registering functions to update, free and print
 * associated to each prefix it also register the memory pool it should use
 * for new allocations
 *
 * @param radix pointer to link the instance of the new radix
 * @param free_data pointer to the function that will be used to
 * free the userdata entries
 * @param update_data pointer to the function that knows how to update a node
 * with new user data
 * @param print_data pointer to a helper function that print_datas a userdata
 * @param pool memory pool that the allocation should use
 *
 * @returns Status code
 */
ib_status_t ib_radix_new(ib_radix_t **radix,
                         ib_radix_free_fn_t free_data,
                         ib_radix_print_fn_t print_data,
                         ib_radix_update_fn_t update_data,
                         ib_mpool_t *pool);

/*
 * Inserts a new user data associated to the prefix passed. The prefix is not
 * used, so developers are responsible to free that prefixs
 * Keys can be of "any size" but this will be probably used for
 * CIDR data prefixes only (from 0 to 32 ~ 128 depending on IPv4
 * or IPv6 respectively)
 *
 * @param radix the radix of the node
 * @param prefix the prefix to use as index
 * @param prefix_data, the data to store under that prefix
 *
 * @returns Status code
 */
ib_status_t ib_radix_insert_data(ib_radix_t *radix,
                                 ib_radix_prefix_t *prefix,
                                 void *prefix_data);

/* 
 * creates a clone of the tree, allocating memory from mp
 *
 * @param orig pointer to the original pool
 * @param new_pool reference to a pointer that will link to the allocated pool
 * @param mp memory pool that the allocation should use
 *
 * @returns Status code
 */
ib_status_t ib_radix_clone_radix(ib_radix_t *orig,
                                 ib_radix_t **new_radix,
                                 ib_mpool_t *mp);

/**
 * return the number of datas linked from the radix
 *
 * @param radix the radix of the node
 *
 * @returns Status code
 */
size_t ib_radix_elements(ib_radix_t *radix);

/*
 * Destroy the memory pool of a radix (warning: this usually includes itself)
 *
 * @param radix the radix to destroy
 *
 * @returns Status code
 */
ib_status_t ib_radix_destroy(ib_radix_t **radix);

/*
 * Function that return the data allocated to an exact prefix
 *
 * @param node the node to check
 * @param prefix the prefix we are searching
 * @param offset, the number of bits already compared +1 (cur possition)
 * @param result reference to the pointer that will be linked to the data if any
 * @param mp pool where we should allocate the list
 *
 * @returns Status code
 */
ib_status_t ib_radix_match_exact(ib_radix_t *radix,
                                 ib_radix_prefix_t *prefix,
                                 void *result);

/*
 * Function that return the data linked to an exact prefix if any. Otherwise
 * it will start falling backwars until it reach a immediate shorter prefix with
 * any data returning it. If no data is found on it's path it will return null.
 *
 * Example: insert data in 192.168.1.0/24
 * search with this function the data of 192.168.1.27
 * it will not have an exact match ending with .27, but walking backwards the
 * recursion, it will find data associated to 192.168.1.0/24
 *
 * @param node the node to check
 * @param prefix the prefix we are searching
 * @param offset, the number of bits already compared +1 (cur possition)
 * @param result reference to the pointer that will be linked to the data if any
 * @param mp pool where we should allocate the list
 *
 * @returns Status code
 */
ib_status_t ib_radix_match_closest(ib_radix_t *radix,
                                   ib_radix_prefix_t *prefix,
                                   void *result);

/*
 * Function that return a list of all the datas with a prefix that start like
 * the provided prefix arg
 *
 * Example: insert data in 192.168.1.27, as well as 192.168.1.28,
 * as well as 192.168.10.0/24 and 10.0.0.0/8 and then search 192.168.0.0/16
 * it should return a list containing all the datas except the associated to
 * 10.0.0.0/8
 *
 * @param node the node to check
 * @param prefix the prefix we are searching
 * @param offset, the number of bits already compared +1 (cur possition)
 * @param rlist reference to the pointer that will be linked to the list, if any
 * @param mp pool where we should allocate the list
 *
 * @returns Status code
 */
ib_status_t ib_radix_match_all_data(ib_radix_t *radix,
                                    ib_radix_prefix_t *prefix,
                                    ib_list_t **rlist,
                                    ib_mpool_t *mp);

/*
 * Create a prefix of type ib_radix_prefix_t given the cidr ascii representation
 * Valid for ipv4 and ipv6.
 * warning:
 *  the criteria to determine if ipv6 or ipv4 is the presence of ':' (ipv6)
 *  so the functions using this API should implement their own checks for valid
 *  formats, with regex, or functions, thought
 *
 * @param cidr ascii representation
 * @param prefix reference to link the new prefix
 * @param mp pool where we should allocate the list
 *
 * @returns struct in6_addr*
 */
ib_status_t ib_radix_ip_to_prefix(const char *cidr,
                                  ib_radix_prefix_t **prefix,
                                  ib_mpool_t *mp);

/** @} IronBeeUtilRadix */

/**
 * @defgroup IronBeeUtilAhoCorasick
 * @{
 */

/* General flags for the parser (and matcher) */
#define IB_AC_FLAG_PARSER_NOCASE    0x01 /**< Case insensitive */
#define IB_AC_FLAG_PARSER_COMPILED  0x02 /**< "Compiled", failure state 
                                              links and output state links 
                                              are built */
#define IB_AC_FLAG_PARSER_READY     0x04 /**< the ac automata is ready */

/* Node specific flags */
#define IB_AC_FLAG_STATE_OUTPUT     0x01 /**< This flag indicates that
                                              the current state produce
                                              an output */

/* Flags for the consume() function (matching function) */
#define IB_AC_FLAG_CONSUME_DEFAULT       0x00 /**< No match list, no
                                                   callback, returns on
                                                   the first match
                                                   (if any) */

#define IB_AC_FLAG_CONSUME_MATCHALL      0x01 /**< Should be used in
                                                   combination to dolist 
                                                   or docallback. Otherwise
                                                   you are wasting cycles*/

#define IB_AC_FLAG_CONSUME_DOLIST        0x02 /**< Enable the storage of a
                                                   list of matching states 
                                                   (match_list located at 
                                                   the matching context */

#define IB_AC_FLAG_CONSUME_DOCALLBACK    0x04 /**< Enable the callback
                                                   (you must also register
                                                   the pattern with the
                                                   callback) */


typedef struct ib_ac_t ib_ac_t;
typedef struct ib_ac_state_t ib_ac_state_t;
typedef struct ib_ac_context_t ib_ac_context_t;
typedef struct ib_ac_match_t ib_ac_match_t;

typedef char ib_ac_char_t;

/**
 * Aho Corasick tree. Used to parse and store the states and transitions
 */
struct ib_ac_t {
    uint8_t flags;          /**< flags of the matcher and parser */
    ib_mpool_t *mp;         /**< mem pool */

    ib_ac_state_t *root;     /**< root of the direct tree */

    uint32_t pattern_cnt;   /**< number of patterns */
};

/**
 * Aho Corasick matching context. Used to consume a buffer in chunks
 * It also store a list of matching states
 */
struct ib_ac_context_t {
    ib_ac_t *ac_tree;           /**< Aho Corasick automata */
    ib_ac_state_t *current;     /**< Current state of match */

    size_t processed;           /**> number of bytes processed over
                                     multiple consume calls */
    size_t current_offset;      /**> number of bytes processed in
                                     the last (or current) call */

    ib_list_t *match_list;      /**< result list of matches */
    size_t match_cnt;           /**< number of matches */
}; 

/**
 * Aho Corasick match result. Holds the pattern, pattern length
 * offset from the beggining of the match, and relative offset.
 * Relative offset is the offset from the end of the given chunk buffer
 * Keep in mind that the start of the match can be at a previous
 * processed chunk
 */
struct ib_ac_match_t {
    const ib_ac_char_t *pattern;     /**< pointer to the original pattern */
    const void *data;                /**< pointer to the data associated */
    size_t pattern_len;              /**< pattern length */

    size_t offset;                   /**< offset over all the segments processed
                                          by this context to the start of
                                          the match */

    size_t relative_offset;          /**< offset of the match from the last
                                          processed buffer within the current
                                          context. Keep in mind that this value
                                          can be negative if a match started
                                          from a previous buffer! */
};

/* Callback definition for functions processing matches */
typedef void (*ib_ac_callback_t)(ib_ac_t *orig,
                                 ib_ac_char_t *pattern,
                                 size_t pattern_len,
                                 void *userdata,
                                 size_t offset,
                                 size_t relative_offset);



/**
 * Init macro for a matching context (needed by ib_ac_consume())
 * @param ac_ctx the ac matching context
 * @param ac_tree the ac tree
 */
#define ib_ac_init_ctx(ac_ctx,ac_t) \
        do { \
            (ac_ctx)->ac_tree = (ac_t); \
            if ((ac_t) != NULL) {\
            (ac_ctx)->current = (ac_t)->root; }\
            (ac_ctx)->processed = 0; \
            (ac_ctx)->current_offset = 0; \
            (ac_ctx)->match_cnt = 0; \
            (ac_ctx)->match_list = NULL; \
        } while(0)

/**
 * Reset macro for a matching context
 * @param ac_ctx the ac matching context
 * @param ac_tree the ac tree
 */
#define ib_ac_reset_ctx(ac_ctx,ac_t) \
        do { \
            (ac_ctx)->ac_tree = (ac_t); \
            if ((ac_t) != NULL) {\
            (ac_ctx)->current = (ac_t)->root; }\
            (ac_ctx)->processed = 0; \
            (ac_ctx)->match_cnt = 0; \
            (ac_ctx)->current_offset = 0; \
            if ((ac_ctx)->match_list != NULL) \
                ib_list_clear((ac_ctx)->match_list); \
        } while(0)



/**
 * creates an aho corasick automata with states in trie form
 *
 * @param ac_tree pointer to store the matcher
 * @param flags options for the matcher
 * @param pool memory pool to use
 *
 * @returns Status code
 */
ib_status_t ib_ac_create(ib_ac_t **ac_tree,
                         uint8_t flags,
                         ib_mpool_t *pool);

/**
 * builds links between states (the AC failure function)
 *
 * @param ac_tree pointer to store the matcher
 *
 * @returns Status code
 */
ib_status_t ib_ac_build_links(ib_ac_t *ac_tree);

/**
 * adds a pattern into the trie
 *
 * @param ac_tree pointer to the matcher
 * @param pattern to add
 * @param callback function pointer to call if pattern is found
 * @param data pointer to pass to the callback if pattern is found
 * @param len the length of the pattern
 *
 * @returns Status code
 */
ib_status_t ib_ac_add_pattern(ib_ac_t *ac_tree,
                              const char *pattern,
                              ib_ac_callback_t callback,
                              void *data,
                              size_t len);

/**
 * Search patterns of the ac_tree matcher in the given buffer using a 
 * matching context. The matching context stores offsets used to process
 * a search over multiple data segments. The function has option flags to
 * specify to return where the first pattern is found, or after all the
 * data is consumed, using a user specified callback and/or building a
 * list of patterns matched
 *
 * @param ac_ctx pointer to the matching context
 * @param data pointer to the buffer to search in
 * @param len the length of the data
 * @param flags options to use while matching
 * @param mp memory pool to use
 *
 * @returns Status code */

ib_status_t ib_ac_consume(ib_ac_context_t *ac_ctx,
                          const char *data,
                          size_t len,
                          uint8_t flags,
                          ib_mpool_t *mp);


/** @} IronBeeUtilAhoCorasick */

/**
 * @defgroup IronBeeUtilUUID
 * @{
 */

/**
 * @internal
 * Universal Unique ID Structure.
 *
 * This is a modified UUIDv1 (RFC-4122) that uses fields as follows:
 *
 * time_low: 32-bit second accuracy time that tx started
 * time_mid: 16-bit counter
 * time_hi_and_ver: 4-bit version (0100) + 12-bit least sig usec
 * clk_seq_hi_res: 2-bit reserved (10) + 6-bit reserved (111111)
 * clk_seq_low: 8-bit reserved (11111111)
 * node(0-1): 16-bit process ID
 * node(2-5): 32-bit ID (system default IPv4 address by default)
 *
 * This is loosely based of of Apache mod_unique_id, but with
 * future expansion in mind.
 */
typedef struct ib_uuid_t ib_uuid_t;
struct ib_uuid_t {
    uint32_t  time_low;
    uint16_t  time_mid;
    uint16_t  time_hi_and_ver;
    uint8_t   clk_seq_hi_res;
    uint8_t   clk_seq_low;
    uint8_t   node[6];
};

/**
 * Parses an ASCII UUID (with the format xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
 * where x are hexa chars) into a ib_uuid_t
 *
 * @param ibuuid pointer to an already allocated ib_uuid_t buffer
 * @param uuid pointer to the ascii string of the uuid (no blank spaces allowed)
 * @param ib pointer to the engine (to log format errors)
 *
 * @returns Status code
 */
ib_status_t ib_uuid_ascii_to_bin(ib_uuid_t *ibuuid,
                                const char *uuid);
/** @} IronBeeUtilUUID */

/**
 * @defgroup IronBeeUtilLogformat
 * @{
 */
#define IB_LOGFORMAT_MAXFIELDS 128
#define IB_LOGFORMAT_MAXLINELEN 8192
#define IB_LOGFORMAT_DEFAULT ((char*)"%T %h %a %S %s %t %f")

typedef struct ib_logformat_t ib_logformat_t;

/* fields */
#define IB_LOG_FIELD_REMOTE_ADDR        'a'
#define IB_LOG_FIELD_LOCAL_ADDR         'A'
#define IB_LOG_FIELD_HOSTNAME           'h'
#define IB_LOG_FIELD_SITE_ID            's'
#define IB_LOG_FIELD_SENSOR_ID          'S'
#define IB_LOG_FIELD_TRANSACTION_ID     't'
#define IB_LOG_FIELD_TIMESTAMP          'T'
#define IB_LOG_FIELD_LOG_FILE           'f'

struct ib_logformat_t {
    ib_mpool_t *mp;
    char *orig_format;
    uint8_t literal_starts;

    /* We could use here an ib_list, but this will is faster */
    char fields[IB_LOGFORMAT_MAXFIELDS];     /**< Used to hold the field list */
    char *literals[IB_LOGFORMAT_MAXFIELDS + 2]; /**< Used to hold the list of
                                                   literal strings at the start,
                                                 end, and between fields */
    int literals_len[IB_LOGFORMAT_MAXFIELDS + 2]; /**< Used to hold the sizes of
                                                       literal strings */
    uint8_t field_cnt;   /**< Fields count */
    uint8_t literal_cnt; /**< Literals count */
};

/**
 * Creates a logformat helper
 *
 * @param mp memory pool to use
 * @param lf reference pointer where the new instance will be stored
 *
 * @returns Status code
 */
ib_status_t ib_logformat_create(ib_mpool_t *mp, ib_logformat_t **lf);

/**
 * Used to parse and store the specified format
 *
 * @param mp memory pool to use
 * @param lf pointer to the logformat helper
 * @param format string with the format to process
 *
 * @returns Status code
 */
ib_status_t ib_logformat_set(ib_logformat_t *lf, char *format);
/** @} IronBeeUtilLogformat */


/**
 * @} IronBeeUtil
 */

#ifdef __cplusplus
}
#endif

#endif /* _IB_UTIL_H_ */
