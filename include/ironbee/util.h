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

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

#include <sys/types.h>
#include <unistd.h>

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
    IB_ETIMEDOUT,                   /**< Operation timed out */
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
 * on the parent will propagate to all decendents.
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
 * Deallocate all memory allocated from the pool and any decendent pools.
 *
 * @param mp Memory pool
 */
void DLL_PUBLIC ib_mpool_clear(ib_mpool_t *mp);

/**
 * Destroy pool and any decendent pools.
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
 * Loop through all elements in the list.
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
 * This loop allows deleting elements. If this is not needed, then
 * use the @ref IB_LIST_LOOP loop.
 *
 * @param list List
 * @param node Symbol holding node
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
 * @warning Do not use to delete an element in the list. Instead use
 *          the @ref IB_LIST_LOOP_REVERS_SAFE loop.
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
 * This loop allows deleting elements. If this is not needed, then
 * use the @ref IB_LIST_REVERSE_LOOP loop.
 *
 * @param list List
 * @param node Symbol holding node
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

/**
 * Create a hash table.
 *
 * @param ph Address which new hash table is written
 * @param pool Memory pool to use
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_hash_create(ib_hash_t **ph, ib_mpool_t *pool);

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
 * @param klen Length of key
 * @param pdata Address which data is written
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_hash_get_ex(ib_hash_t *h,
                                      void *key, size_t klen,
                                      void *pdata);

/**
 * Get data from a hash table via key (string).
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
 * Set data in a hash table for key and key length.
 *
 * @param h Hash table
 * @param key Key to lookup
 * @param klen Length of key
 * @param data Data
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_hash_set_ex(ib_hash_t *h,
                                      void *key, size_t klen,
                                      void *data);

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
 * @param klen Length of key
 * @param pdata Address which data is written (or NULL if not required)
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_hash_remove_ex(ib_hash_t *h,
                                         void *key, size_t klen,
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
 * @param type Field type
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
 * @param type Field type
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
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_cfgmap_init(ib_cfgmap_t *cm,
                                      const void *base,
                                      const ib_cfgmap_init_t *init);


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
                                     void *pval, ib_ftype_t *type);

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
 * @} IronBeeUtil
 */

#ifdef __cplusplus
}
#endif

#endif /* IB_UTIL_H_ */
