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

#ifndef _IB_DATA_H_
#define _IB_DATA_H_

#include <ironbee/field.h>
#include <ironbee/mpool.h>
#include <ironbee/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file
 * @brief IronBee --- Data Field Support
 *
 * @author Brian Rectanus <brectanus@qualys.com>
 * @author Christopher Alfeld <calfeld@calfeld.net>
 */

/**
 * @defgroup IronBeeEngineData Data Field
 * @ingroup IronBeeEngine
 * @{
 */

/**
 * Data state.
 */
typedef struct ib_data_t ib_data_t;

/**
 * Data configuration.
 *
 * Every @ref ib_data_t belongs to one of these.  It allows keys to be
 * registered as "indexed" allowing them to be rapidly (O(1)) looked up by
 * index but means that memory for a pointer to that will be allocated in
 * every @ref ib_data_t.
 *
 * @sa ib_data_register_indexed_ex()
 **/
typedef struct ib_data_config_t ib_data_config_t;

/**
 * Create new data store configuration.
 * @param[in]  mp     Memory pool to use.
 * @param[out] config The new data store configuration.
 * @returns
 * - IB_OK on success.
 * - IB_EALLOC on allocation failure.
 **/
ib_status_t DLL_PUBLIC ib_data_config_create(
    ib_mpool_t        *mp,
    ib_data_config_t **config
);

/**
 * Create new data store.
 *
 * @param[in]  config Data configuration.
 * @param[in]  mp     Memory pool to use.
 * @param[out] data   The new data store.
 * @returns
 * - IB_OK on success.
 * - IB_EALLOC on allocation failure.
 */
ib_status_t DLL_PUBLIC ib_data_create(
    const ib_data_config_t  *config,
    ib_mpool_t              *mp,
    ib_data_t              **data
);

/**
 * Access data pool of @a data.
 *
 * @param[in] data Data to access pool of.
 * @returns Memory pool of @a data.
 */
ib_mpool_t DLL_PUBLIC *ib_data_pool(
    const ib_data_t *data
);

/**
 * Register an indexed data field.
 *
 * Indexed data fields will have a pointer to them allocated in every
 * @ref ib_data_t which can be looked up in O(1) time by index via
 * ib_data_get_indexed().
 *
 * @param[in]  config     Data configuration to register key with.
 * @param[in]  key        Key.
 * @param[in]  key_length Length of key.
 * @param[out] index      Where to store index; can be NULL.
 * @return
 * - IB_OK on success.
 * - IB_EINVAL if @a key already registered.
 **/
ib_status_t DLL_PUBLIC ib_data_register_indexed_ex(
    ib_data_config_t *config,
    const char       *key,
    size_t            key_length,
    size_t           *index
);

/**
 * Lookup index for an indexed data field.
 *
 * @param[in]  config     Data configuration to lookup key in.
 * @param[in]  key        Key.
 * @param[in]  key_length Length of key.
 * @param[out] index      Where to store index; can be NULL.
 * @return
 * - IB_OK on success.
 * - IB_ENOENT if no such key registered.
 **/
ib_status_t DLL_PUBLIC ib_data_lookup_index_ex(
    const ib_data_config_t *config,
    const char             *key,
    size_t                  key_length,
    size_t                 *index
);

/**
 * Null string version of ib_data_lookup_index_ex().
 *
 * @param[in]  config     Data configuration to lookup key in.
 * @param[in]  key        Key.
 * @param[out] index      Where to store index; can be NULL.
 * @return
 * - IB_OK on success.
 * - IB_ENOENT if no such key registered.
 **/
ib_status_t DLL_PUBLIC ib_data_lookup_index(
    const ib_data_config_t *config,
    const char             *key,
    size_t                 *index
);

/**
 * Null string version of ib_data_register_indexed_ex().
 *
 * @param[in]  config     Data configuration to register key with.
 * @param[in]  key        Key.
 * @return
 * - IB_OK on success.
 * - IB_EINVAL if @a key already registered.
 **/
ib_status_t DLL_PUBLIC ib_data_register_indexed(
    ib_data_config_t *config,
    const char       *key
);

/**
 * Add a data field.
 *
 * @param[in] data Data.
 * @param[in] f Field
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_data_add(
    ib_data_t  *data,
    ib_field_t *f
);

/**
 * Add a data field with a different name than the field name.
 *
 * @param[in] data Data.
 * @param[in] f Field
 * @param[in] name Name
 * @param[in] nlen Name length
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_data_add_named(
    ib_data_t  *data,
    ib_field_t *f,
    const char *name,
    size_t      nlen
);

/**
 * Create and add a numeric data field (extended version).
 *
 * @param[in] data Data.
 * @param[in] name Name as byte string
 * @param[in] nlen Name length
 * @param[in] val Numeric value
 * @param[out] pf Pointer where new field is written if non-NULL
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_data_add_num_ex(
    ib_data_t   *data,
    const char  *name,
    size_t       nlen,
    ib_num_t     val,
    ib_field_t **pf
);

/**
 * Create and add a nulstr data field (extended version).
 *
 * @param[in] data Data.
 * @param[in] name Name as byte string
 * @param[in] nlen Name length
 * @param[in] val String value
 * @param[out] pf Pointer where new field is written if non-NULL
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_data_add_nulstr_ex(
    ib_data_t   *data,
    const char  *name,
    size_t       nlen,
    const char  *val,
    ib_field_t **pf
);

/**
 * Create and add a bytestr data field (extended version).
 *
 * @param[in] data Data.
 * @param[in] name Name
 * @param[in] nlen Name length
 * @param[in] val Byte string value
 * @param[in] vlen Value length
 * @param[out] pf Pointer where new field is written if non-NULL
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_data_add_bytestr_ex(
    ib_data_t   *data,
    const char  *name,
    size_t       nlen,
    uint8_t     *val,
    size_t       vlen,
    ib_field_t **pf
);

/**
 * Create and add a list data field (extended version).
 *
 * @param[in] data Data.
 * @param[in] name Name as byte string
 * @param[in] nlen Name length
 * @param[out] pf Pointer where new field is written if non-NULL
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_data_add_list_ex(
    ib_data_t   *data,
    const char  *name,
    size_t       nlen,
    ib_field_t **pf
);

/**
 * Create and add a stream buffer data field (extended version).
 *
 * @param[in] data Data.
 * @param[in] name Name as byte string
 * @param[in] nlen Name length
 * @param[out] pf Pointer where new field is written if non-NULL
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_data_add_stream_ex(
    ib_data_t   *data,
    const char  *name,
    size_t       nlen,
    ib_field_t **pf
);

/**
 * Get a data field (extended version).
 *
 * @param[in] data Data.
 * @param[in] name Name as byte string
 * @param[in] nlen Name length
 * @param[out] pf Pointer where new field is written. Unlike other calls,
 *           this must not be NULL.
 *
 * @returns IB_OK on success or IB_ENOENT if the element is not found.
 */
ib_status_t DLL_PUBLIC ib_data_get_ex(
    const ib_data_t  *data,
    const char       *name,
    size_t            nlen,
    ib_field_t      **pf
);

/**
 * Get a data field by index.
 *
 * @param[in]  data  Data.
 * @param[in]  index Index of field to get.
 * @param[out] pf    Pointer to where field is written.
 * @return
 * - IB_OK on success.
 * - IB_ENOENT if no field of given index.
 */
ib_status_t DLL_PUBLIC ib_data_get_indexed(
    const ib_data_t  *data,
    size_t            index,
    ib_field_t      **pf
);

/**
 * Get all data fields from a data provider instance.
 *
 * @param[in] data Data.
 * @param[in] list List which data fields will be pushed
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_data_get_all(
    const ib_data_t *data,
    ib_list_t       *list
);

/**
 * Create and add a numeric data field.
 *
 * @param[in] data Data.
 * @param[in] name Name as byte string
 * @param[in] val Numeric value
 * @param[out] pf Pointer where new field is written if non-NULL
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_data_add_num(
    ib_data_t   *data,
    const char  *name,
    ib_num_t     val,
    ib_field_t **pf
);

/**
 * Create and add a nulstr data field.
 *
 * @param[in] data Data.
 * @param[in] name Name as byte string
 * @param[in] val String value
 * @param[out] pf Pointer where new field is written if non-NULL
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_data_add_nulstr(
    ib_data_t   *data,
    const char  *name,
    const char  *val,
    ib_field_t **pf
);

/**
 * Create and add a bytestr data field.
 *
 * @param[in] data Data.

 * @param[in] name Name
 * @param[in] val Byte string value
 * @param[in] vlen Value length
 * @param[out] pf Pointer where new field is written if non-NULL
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_data_add_bytestr(
    ib_data_t   *data,
    const char  *name,
    uint8_t     *val,
    size_t       vlen,
    ib_field_t **pf
);

/**
 * Create and add a list data field.
 *
 * @param[in] data Data.
 * @param[in] name Name as byte string
 * @param[out] pf Pointer where new field is written if non-NULL
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_data_add_list(
    ib_data_t   *data,
    const char  *name,
    ib_field_t **pf
);

/**
 * Create and add a stream buffer data field.
 *
 * @param[in] data Data.
 * @param[in] name Name as byte string
 * @param[out] pf Pointer where new field is written if non-NULL
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_data_add_stream(
    ib_data_t   *data,
    const char  *name,
    ib_field_t **pf
);

/**
 * Get a data field.
 *
 * @param[in] data Data.
 * @param[in] name Name as NUL terminated string
 * @param[out] pf Pointer where new field is written if non-NULL
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_data_get(
    const ib_data_t  *data,
    const char       *name,
    ib_field_t      **pf
);

/**
 * Remove a data field.
 * @param[in] data Data.
 * @param[in] name Name as NUL terminated string
 * @param[in] nlen Length of @a name.
 * @param[out] pf Pointer where old field is written if non-NULL
 */
ib_status_t ib_data_remove_ex(
    ib_data_t   *data,
    const char  *name,
    size_t       nlen,
    ib_field_t **pf
);

/**
 * Remove a data field.
 * @param[in] data Data.
 * @param[in] name Name as NUL terminated string
 * @param[out] pf Pointer where old field is written if non-NULL
 */
ib_status_t ib_data_remove(
    ib_data_t   *data,
    const char  *name,
    ib_field_t **pf
);

/**
 * Set a data field.
 * @param[in] data Data.
 * @param[in] f    Field to set to.
 * @param[in] name Name of data field.
 * @param[in] nlen Length of @a name
 * @returns Status code.
 */
ib_status_t DLL_PUBLIC ib_data_set(
    ib_data_t  *data,
    ib_field_t *f,
    const char *name,
    size_t      nlen
);

/**
 * Set a relative data field value.
 *
 * @param[in] data Data
 * @param[in] name Field name
 * @param[in] nlen Field length
 * @param[in] adjval Value to adjust (add or subtract a numeric value)
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_data_set_relative(
    ib_data_t  *data,
    const char *name,
    size_t      nlen,
    intmax_t    adjval
);

/**
 * Expand a string using fields from the data store.
 *
 * This function looks through @a str for instances of
 * "%{"+_name_+"}" (e.g. "%{FOO}"), then attempts to look up
 * each of such name found in the data @a data.  If _name_ is not
 * found in @a data, the "%{_name_}" sub-string is replaced with an empty
 * string.  If the name is found, the associated field value is used to
 * replace "%{_name_}" sub-string for string and numeric types (numbers are
 * converted to strings); for others, the replacement is an empty string.
 *
 * @param[in] data Data.
 * @param[in] str NUL-terminated string to expand
 * @param[in] recurse Do recursive expansion?
 * @param[out] result Pointer to the expanded string.
 *
 * @returns The code of ib_expand_str_gen.
 *   - IB_OK on success or if the string is not expandable.
 *   - IB_EINVAL if prefix or suffix is zero length.
 *   - IB_EALLOC if a memory allocation failed.
 */
ib_status_t ib_data_expand_str(
    const ib_data_t  *data,
    const char       *str,
    bool              recurse,
    char            **result
);

/**
 * Expand a string using fields from the data store, ex version.
 *
 * @sa ib_data_expand_str()
 *
 * @param[in] data Data.
 * @param[in] str String to expand
 * @param[in] slen Length of string @a str to expand
 * @param[in] nul Append NUL byte to @a result?
 * @param[in] recurse Do recursive expansion?
 * @param[out] result Pointer to the expanded string.
 * @param[out] result_len Length of @a result.
 *
 * @returns The code of ib_expand_str_gen.
 *   - IB_OK on success or if the string is not expandable.
 *   - IB_EINVAL if prefix or suffix is zero length.
 *   - IB_EALLOC if a memory allocation failed.
 */
ib_status_t ib_data_expand_str_ex(
    const ib_data_t  *data,
    const char       *str,
    size_t            slen,
    bool              nul,
    bool              recurse,
    char            **result,
    size_t           *result_len
);

/**
 * Determine if a string would be expanded by ib_data_expand_str().
 *
 * This function looks through @a str for instances of "%{.+}".
 *
 * @param[in] str String to check for expansion
 * @param[out] result true if @a str would be expanded by expand_str().
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_data_expand_test_str(
    const char *str,
    bool       *result
);

/**
 * Determine if a string would be expanded by ib_data_expand_str_ex().
 *
 * @sa ib_data_expand_str(),  ib_data_expand_str_ex(), ib_data_test_str().
 *
 * @param[in] str String to check for expansion
 * @param[in] slen Length of string @a str to expand
 * @param[out] result true if @a str would be expanded by expand_str().
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_data_expand_test_str_ex(
    const char *str,
    size_t      slen,
    bool       *result
);

/**
 * @} IronBeeEngineData
 */

#ifdef __cplusplus
}
#endif

#endif
