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

#ifndef _IB_FIELD_H_
#define _IB_FIELD_H_

/**
 * @file
 * @brief IronBee --- Field Utility Functions
 *
 * @author Brian Rectanus <brectanus@qualys.com>
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <ironbee/build.h>
#include <ironbee/bytestr.h>
#include <ironbee/clock.h>
#include <ironbee/list.h>
#include <ironbee/mm.h>
#include <ironbee/stream.h>
#include <ironbee/types.h>

#include <stdbool.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup IronBeeUtilField Field
 * @ingroup IronBeeUtil
 *
 * A field is a name and a value.  The values can be one of several types.
 * Values can be stored in the field or they can alias another location in
 * memory.  Fields can also be 'dynamic' where set/get operations are passed
 * to functions.
 *
 * As fields can have various types, the field code constitutes a run-time
 * typing system.  In order to avoid large number of functions, it uses
 * @c void *in many places.  To avoid bugs, it provides a number of helper
 * functions that have no runtime effect but act as a compile time assertions
 * on the type of the inputs.  For example, to extra a null string value from
 * a null string field:
 *
 * @code
 * const char *v;
 * ib_status_t rc;
 *
 * rc = ib_field_value(f, ib_ftype_nulstr_out(&v));
 * @endcode
 *
 * In the above example, ib_field_value() is the general function to access
 * the value of a field.  It takes a @c void *as the second parameter.
 * The ib_ftype_nulstr_out() function takes the appropriate type (@c const
 * @c char @c **) and returns its argument cast to @c void*.  In this way, if
 * you provide the wrong type to ib_ftype_nulstr_out(), the compiler will
 * warn you.
 *
 * There are @c ib_ftype_* functions for every field type and every needed
 * passing convention.  The @c void *arguments are all named by passing
 * convention.  Thus:
 *
 * - @a in_pval --- @c ib_ftype_X_in
 * - @a out_pval --- @c ib_ftype_X_out
 * - @a mutable_in_pval --- @c ib_ftype_X_mutable_in
 * - @a mutable_out_pval --- @c ib_ftype_X_mutable_out
 * - @a storage_pval --- @c ib_ftype_X_storage
 *
 * The storage type is used by ib_field_create_alias() and is the way to
 * pass the location to use for field storage.
 *
 * The following table describes the types for every field type.
 *
 * <table>
 *     <tr>
 *         <th>Field Type</th>
 *         <th>Value</th>
 *         <th>In</th>
 *         <th>Mutable In</th>
 *         <th>Out</th>
 *         <th>Mutable Out</th>
 *         <th>Storage</th>
 *     </tr>
 *
 *     <tr>
 *         <td>@c IB_FTYPE_GENERIC</td>
 *         <td>@c void*</td>
 *         <td>@c void*</td>
 *         <td>@c void*</td>
 *         <td>@c void*</td>
 *         <td>@c void*</td>
 *         <td>@c void*</td>
 *     </tr>
 *
 *     <tr>
 *         <td>@c IB_FTYPE_NUM</td>
 *         <td>@c ib_num_t</td>
 *         <td>@c const @c ib_num_t*</td>
 *         <td>@c ib_num_t*</td>
 *         <td>@c const @c ib_num_t*</td>
 *         <td>@c ib_num_t**</td>
 *         <td>@c ib_num_t*</td>
 *     </tr>
 *
 *     <tr>
 *         <td>@c IB_FTYPE_FLOAT</td>
 *         <td>@c ib_float_t</td>
 *         <td>@c const @c ib_float_t*</td>
 *         <td>@c ib_float_t*</td>
 *         <td>@c const @c ib_float_t*</td>
 *         <td>@c ib_float_t**</td>
 *         <td>@c ib_float_t*</td>
 *     </tr>
 *
 *     <tr>
 *         <td>@c IB_FTYPE_TIME</td>
 *         <td>@c ib_time_t</td>
 *         <td>@c const @c ib_time_t*</td>
 *         <td>@c ib_time_t*</td>
 *         <td>@c const @c ib_time_t*</td>
 *         <td>@c ib_time_t**</td>
 *         <td>@c ib_time_t*</td>
 *     </tr>
 *
 *     <tr>
 *         <td>@c IB_FTYPE_NULSTR</td>
 *         <td>@c char*</td>
 *         <td>@c const @c char*</td>
 *         <td>@c char*</td>
 *         <td>@c const @c char**</td>
 *         <td>@c char**</td>
 *         <td>@c char**</td>
 *     </tr>
 *
 *     <tr>
 *         <td>@c IB_FTYPE_BYTESTR</td>
 *         <td>@c ib_bytestr_t*</td>
 *         <td>@c const @c ib_bytestr_t*</td>
 *         <td>@c ib_bytestr_t*</td>
 *         <td>@c const @c ib_bytestr_t**</td>
 *         <td>@c ib_bytestr_t**</td>
 *         <td>@c ib_bytestr_t**</td>
 *     </tr>
 *
 *     <tr>
 *         <td>@c IB_FTYPE_LIST</td>
 *         <td>@c ib_list_t*</td>
 *         <td>@c const @c ib_list_t*</td>
 *         <td>@c ib_list_t*</td>
 *         <td>@c const @c ib_list_t**</td>
 *         <td>@c ib_list_t**</td>
 *         <td>@c ib_list_t**</td>
 *     </tr>
 *
 *     <tr>
 *         <td>@c IB_FTYPE_SBUFFER</td>
 *         <td>@c ib_stream_t*</td>
 *         <td>@c const @c ib_stream_t*</td>
 *         <td>@c ib_stream_t*</td>
 *         <td>@c const @c ib_stream_t**</td>
 *         <td>@c ib_stream_t**</td>
 *         <td>@c ib_stream_t**</td>
 *     </tr>
 * </table>
 *
 * Notes:
 * - The in type for IB_FTYPE_NUM is @c ib_num_t* instead of ib_num_t because
 *   an ib_num_t will not fit in a @c void *parameter on 32 bit architectures.
 * - The mutable out types for IB_FTYPE_NUM is @c ib_num_t** so that a pointer
 *   to the value can be passed out.  This allows the caller to mutate the
 *   actual number as expected for a mutable value.  The same applies
 *   to IB_FTYPE_FLOAT.
 * - The use of `void *` instead of `void **` for IB_FTYPE_GENERIC may seem
 *   surprising, but using `void *` allows the user to express specific
 *   knowledge of the held type without an awkward cast.
 *
 * @{
 */

/**
 * Field type.
 */
typedef enum {
    IB_FTYPE_GENERIC = 0, /**< Generic pointer value */
    IB_FTYPE_NUM,         /**< Numeric value */
    IB_FTYPE_TIME,        /**< Milliseconds since epoch. */
    IB_FTYPE_FLOAT,       /**< Floating point value. */
    IB_FTYPE_NULSTR,      /**< NUL terminated string value */
    IB_FTYPE_BYTESTR,     /**< Binary data value */
    IB_FTYPE_LIST,        /**< List of fields */
    IB_FTYPE_SBUFFER      /**< Stream buffer */
} ib_ftype_t;

/**
 * Private Implementation Detail.
 */
typedef struct ib_field_val_t ib_field_val_t;

/** Field Structure */
typedef struct ib_field_t ib_field_t;
struct ib_field_t {
    ib_mm_t         mm;        /**< Memory manager */
    ib_ftype_t      type;      /**< Field type */
    const char     *name;      /**< Field name; not '\0' terminated! */
    size_t          nlen;      /**< Field name length */
    const char     *tfn;       /**< Transformations performed */
    ib_field_val_t *val;       /**< Private value store */
};

/**
 * Field numerical signed value type
 */
typedef int64_t ib_num_t;

/**
 * Field float value type
 */
typedef long double ib_float_t;

/**
 * Assert @a v is proper type.
 */
static inline void *ib_ftype_generic_mutable_in(void *v)
{
    return (void *)(v);
}

/**
 * Assert @a v is proper type.
 */
static inline void *ib_ftype_generic_in(void *v)
{
    return (void *)(v);
}

/**
 * Assert @a v is proper type.
 */
static inline void *ib_ftype_generic_out(void *v)
{
    return (void *)(v);
}

/**
 * Assert @a v is proper type.
 */
static inline void *ib_ftype_generic_mutable_out(void *v)
{
    return (void *)(v);
}

/**
 * Assert @a v is proper type.
 */
static inline void *ib_ftype_generic_storage(void *v)
{
    return (void *)(v);
}

/**
 * Assert @a v is proper type.
 */
static inline void *ib_ftype_float_mutable_in(ib_float_t *v)
{
    return (void *)(v);
}
/**
 * Assert @a v is proper type.
 */
static inline void *ib_ftype_float_in(const ib_float_t *v)
{
    return (void *)(v);
}
/**
 * Assert @a v is proper type.
 */
static inline void *ib_ftype_float_mutable_out(ib_float_t **v)
{
    return (void *)(v);
}
/**
 * Assert @a v is proper type.
 */
static inline void *ib_ftype_float_out(ib_float_t *v)
{
    return (void *)(v);
}

/**
 * Assert @a v is proper type.
 */
static inline void *ib_ftype_float_storage(ib_float_t *v)
{
    return (void *)(v);
}

/**
 * Assert @a v is proper type.
 */
static inline void *ib_ftype_num_mutable_in(ib_num_t *v)
{
    return (void *)(v);
}

/**
 * Assert @a v is proper type.
 */
static inline void *ib_ftype_num_in(const ib_num_t *v)
{
    return (void *)(v);
}

/**
 * Assert @a v is proper type.
 */
static inline void *ib_ftype_num_out(ib_num_t *v)
{
    return (void *)(v);
}

/**
 * Assert @a v is proper type.
 */
static inline void *ib_ftype_num_mutable_out(ib_num_t **v)
{
    return (void *)(v);
}

/**
 * Assert @a v is proper type.
 */
static inline void *ib_ftype_num_storage(ib_num_t *v)
{
    return (void *)(v);
}

/**
 * Assert @a v is proper type.
 */
static inline void *ib_ftype_time_mutable_in(ib_time_t *v)
{
    return (void *)(v);
}

/**
 * Assert @a v is proper type.
 */
static inline void *ib_ftype_time_in(const ib_time_t *v)
{
    return (void *)(v);
}

/**
 * Assert @a v is proper type.
 */
static inline void *ib_ftype_time_out(ib_time_t *v)
{
    return (void *)(v);
}

/**
 * Assert @a v is proper type.
 */
static inline void *ib_ftype_time_mutable_out(ib_time_t **v)
{
    return (void *)(v);
}

/**
 * Assert @a v is proper type.
 */
static inline void *ib_ftype_time_storage(ib_time_t *v)
{
    return (void *)(v);
}

/**
 * Assert @a v is proper type.
 */
static inline void *ib_ftype_nulstr_mutable_in(char *v)
{
    return (void *)(v);
}

/**
 * Assert @a v is proper type.
 */
static inline void *ib_ftype_nulstr_in(const char *v)
{
    return (void *)(v);
}

/**
 * Assert @a v is proper type.
 */
static inline void *ib_ftype_nulstr_out(const char **v)
{
    return (void *)(v);
}

/**
 * Assert @a v is proper type.
 */
static inline void *ib_ftype_nulstr_mutable_out(char **v)
{
    return (void *)(v);
}

/**
 * Assert @a v is proper type.
 */
static inline void *ib_ftype_nulstr_storage(char **v)
{
    return (void *)(v);
}

/**
 * Assert @a v is proper type.
 */
static inline void *ib_ftype_bytestr_mutable_in(ib_bytestr_t *v)
{
    return (void *)(v);
}

/**
 * Assert @a v is proper type.
 */
static inline void *ib_ftype_bytestr_in(const ib_bytestr_t *v)
{
    return (void *)(v);
}

/**
 * Assert @a v is proper type.
 */
static inline void *ib_ftype_bytestr_out(const ib_bytestr_t **v)
{
    return (void *)(v);
}

/**
 * Assert @a v is proper type.
 */
static inline void *ib_ftype_bytestr_mutable_out(ib_bytestr_t **v)
{
    return (void *)(v);
}

/**
 * Assert @a v is proper type.
 */
static inline void *ib_ftype_bytestr_storage(ib_bytestr_t **v)
{
    return (void *)(v);
}

/**
 * Assert @a v is proper type.
 */
static inline void *ib_ftype_list_mutable_in(ib_list_t *v)
{
    return (void *)(v);
}

/**
 * Assert @a v is proper type.
 */
static inline void *ib_ftype_list_in(const ib_list_t *v)
{
    return (void *)(v);
}

/**
 * Assert @a v is proper type.
 */
static inline void *ib_ftype_list_out(const ib_list_t **v)
{
    return (void *)(v);
}

/**
 * Assert @a v is proper type.
 */
static inline void *ib_ftype_list_mutable_out(ib_list_t **v)
{
    return (void *)(v);
}

/**
 * Assert @a v is proper type.
 */
static inline void *ib_ftype_list_storage(ib_list_t **v)
{
    return (void *)(v);
}

/**
 * Assert @a v is proper type.
 */
static inline void *ib_ftype_sbuffer_mutable_in(ib_stream_t *v)
{
    return (void *)(v);
}

/**
 * Assert @a v is proper type.
 */
static inline void *ib_ftype_sbuffer_in(const ib_stream_t *v)
{
    return (void *)(v);
}

/**
 * Assert @a v is proper type.
 */
static inline void *ib_ftype_sbuffer_out(const ib_stream_t **v)
{
    return (void *)(v);
}

/**
 * Assert @a v is proper type.
 */
static inline void *ib_ftype_sbuffer_mutable_out(ib_stream_t **v)
{
    return (void *)(v);
}

/**
 * Assert @a v is proper type.
 */
static inline void *ib_ftype_sbuffer_storage(ib_stream_t **v)
{
    return (void *)(v);
}

/**
 * Dynamic field get function type.
 *
 * Note that @a out_pval is an out value not a mutable out value.  Dynamic
 * fields do not support mutable values.
 *
 * The field type is available via @c field->type.
 *
 * @param[in]  field    Field in question.
 * @param[out] out_pval Where to write value.
 * @param[in]  arg      Optional argument.
 * @param[in]  alen     Length of @a arg.
 * @param[in]  data     Callback data.
 *
 * @returns Status code
 */
typedef ib_status_t (*ib_field_get_fn_t)(
    const ib_field_t *field,
    void             *out_pval,
    const void       *arg,
    size_t            alen,
    void             *data
);

/**
 * Dynamic field set function.
 *
 * Note that @a in_pval is an in value not a mutable in value.  Dynamic
 * fields do not support mutable values.
 *
 * The field type is available via @c field->type.
 *
 * @param[in] field   Field in question.
 * @param[in] arg     Optional argument.
 * @param[in] alen    Length of @a arg.
 * @param[in] in_pval Value to set.
 * @param[in] data    Callback data.
 *
 * @returns Status code
 */
typedef ib_status_t (*ib_field_set_fn_t)(
    ib_field_t *field,
    const void *arg,
    size_t      alen,
    void       *in_pval,
    void       *data
);

/**
 * Create a field, copying name/data into the field.
 *
 * @warning At present, this function will create copies of integral types,
 * null strings, and byte strings.  However, for lists and streams it will
 * act the same as ib_field_create_no_copy(), casting away the const of
 * @a in_pval.  This will be fixed in a future version.
 *
 * @param[out] pf      Address to write new field to.
 * @param[in] mm      Memory manager.
 * @param[in]  name    Field name.
 * @param[in]  nlen    Field name length.
 * @param[in]  type    Field type.
 * @param[in]  in_pval Value to store in field.
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_field_create(
    ib_field_t **pf,
    ib_mm_t      mm,
    const char  *name,
    size_t       nlen,
    ib_ftype_t   type,
    void        *in_pval
);

/**
 * Create a field without copying data.
 *
 * This will place @a mutable_in_pval directly into the field value without
 * any copying.  This is different than ib_field_create_alias() which
 * uses a user provided pointer for where to store the field value.
 *
 * @param[out] pf              Address to write new field to.
 * @param[in] mm              Memory manager.
 * @param[in]  name            Field name.
 * @param[in]  nlen            Field name length.
 * @param[in]  type            Field type.
 * @param[in]  mutable_in_pval Value to store in field.
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_field_create_no_copy(
    ib_field_t **pf,
    ib_mm_t      mm,
    const char  *name,
    size_t       nlen,
    ib_ftype_t   type,
    void        *mutable_in_pval
);

/**
 * Create a field but use @a *mutable_out_pval as the storage.
 *
 * When the field is set @a *storage_pval is changed and any get
 * reflects the value of @a *storage_pval.
 *
 * @note For null string and bytestring types, @a storage_pval should be
 * treated as a pointer to a `char *`.  Thus, use the
 * @c ib_ftype_T_mutable_out() functions for this parameter for these types.
 * For numeric and related types, @a storage_pval should be treated as a
 * pointer to the value type, and the developer should use the
 * ib_ftype_T_in() functions for this parameter.
 *
 * @param[out] pf           Address to write new field to.
 * @param[in] mm           Memory manager.
 * @param[in]  name         Field name.
 * @param[in]  nlen         Field name length.
 * @param[in]  type         Field type.
 * @param[in]  storage_pval Where to store field value.
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_field_create_alias(
    ib_field_t **pf,
    ib_mm_t      mm,
    const char  *name,
    size_t       nlen,
    ib_ftype_t   type,
    void        *storage_pval
);

/**
 * Create a dynamic field.
 *
 * Dynamic fields only support non-mutable values.
 *
 * @param[out] pf         Address to write new field to.
 * @param[in] mm         Memory manager.
 * @param[in]  name       Field name..
 * @param[in]  nlen       Field name length.
 * @param[in]  type       Field type.
 * @param[in]  fn_get     Getter.
 * @param[in]  cbdata_get Getter data.
 * @param[in]  fn_set     Setter.
 * @param[in]  cbdata_set Setter data.
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_field_create_dynamic(
    ib_field_t        **pf,
    ib_mm_t             mm,
    const char         *name,
    size_t              nlen,
    ib_ftype_t          type,
    ib_field_get_fn_t   fn_get,
    void               *cbdata_get,
    ib_field_set_fn_t   fn_set,
    void               *cbdata_set
);

/**
 * Make a copy of a field, aliasing data.
 *
 * The new field will use the same value storage as @a src.  Any changes to
 * one will be reflected in the other and in the underlying storage.
 *
 * @param[out] pf   Address to write new field to.
 * @param[in]  mm   Memory manager.
 * @param[in]  name Field name.
 * @param[in]  nlen Field name length.
 * @param[in]  src  Source field.
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_field_alias(
    ib_field_t **pf,
    ib_mm_t      mm,
    const char  *name,
    size_t       nlen,
    const ib_field_t  *src
);

/**
 * Make a copy of a field.
 *
 * This makes a copy of the field.  The new field will have separate
 * storage.
 *
 * @warning For number and string fields, the underlying data will also be
 * duplicated.  For list and stream fields, the data will not be duplicated.
 * This may be fixed in the future.
 *
 * @param[out] pf   Address to write new field to.
 * @param[in]  mm   Memory manager.
 * @param[in]  name Field name.
 * @param[in]  nlen Field name length.
 * @param[in]  src  Source field.
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_field_copy(
    ib_field_t       **pf,
    ib_mm_t            mm,
    const char        *name,
    size_t             nlen,
    const ib_field_t  *src
);

/**
 * Create a bytestr field which directly aliases a value in memory.
 *
 * This is a equivalent to create a byte string alias of @a val and @a vlen
 * and passing it ib_field_create_no_copy().
 *
 * @param[out] pf   Address to write new field to.
 * @param[in]  mm   Memory manager.
 * @param[in]  name Field name.
 * @param[in]  nlen Field name length.
 * @param[in]  val  Value.
 * @param[in]  vlen Value length.
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_field_create_bytestr_alias(
    ib_field_t    **pf,
    ib_mm_t         mm,
    const char     *name,
    size_t          nlen,
    const uint8_t  *val,
    size_t          vlen
);

/**
 * Add a field to a IB_FTYPE_LIST field.
 *
 * @param[in] f   Field.
 * @param[in] val Field to add to the list.
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_field_list_add(
    ib_field_t *f,
    ib_field_t *val
);

/**
 * Add a const field to a IB_FTYPE_LIST field.
 *
 * @param[in] f   Field.
 * @param[in] val Field to add to the list.
 *
 * @returns Status code
 */
ib_status_t ib_field_list_add_const(
    ib_field_t *f,
    const ib_field_t *val
);

/**
 * Add a buffer to a IB_FTYPE_SBUFFER type field.
 *
 * @param[in] f     Field.
 * @param[in] dtype Data type.
 * @param[in] buf   Buffer.
 * @param[in] blen  Length of @a buf.
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_field_buf_add(
    ib_field_t *f,
    int         dtype,
    uint8_t    *buf,
    size_t      blen
);

/**
 * Turn a dynamic field, @a f into a static field.
 *
 * This call should immediately be followed by a @c setv call to set a
 * (static) value.
 *
 * This method removes the setter and getters and sets up internal storage for
 * the field value.  The actual value is undefined, hence the need to follow
 * up with a set.
 *
 * Returns IB_EINVAL if @a f is not dynamic.
 *
 * @param[in] f Field to make static.
 *
 * @returns Status code.
 */
ib_status_t DLL_PUBLIC ib_field_make_static(
    ib_field_t* f
 );

/**
 * Set a field value, copying.
 *
 * @warning This function will not actually copy lists or streams.  It
 * behaves as ib_field_setv_no_copy() for those types.  This may be fixed in
 * the future.
 *
 * @param[in] f       Field to set value of.
 * @param[in] in_pval Pointer to value to store in field (based on type)
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_field_setv(
    ib_field_t *f,
    void       *in_pval
);

/**
 * Set a field directly without copying.
 *
 * Can not be called on dynamic fields.
 * @param[in] f               Field to set value.
 * @param[in] mutable_in_pval Pointer to store in field.
 *
 * @returns Status code.
 */
ib_status_t DLL_PUBLIC ib_field_setv_no_copy(
    ib_field_t *f,
    void *mutable_in_pval
);

/**
 * Set a field value, passing the argument on to dynamic fields.
 *
 * This will result in an error if @a f is not dynamic.
 *
 * @param[in] f       Field.
 * @param[in] in_pval Pointer to value to store in field (based on type).
 * @param[in] arg     Arbitrary argument.
 * @param[in] alen    Argument length.
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_field_setv_ex(
    ib_field_t *f,
    void       *in_pval,
    const void *arg,
    size_t      alen
);

/**
 * Get the value stored in the field, passing the argument on to dynamic
 * fields.
 *
 * This will result in an error if @a f is not dynamic.
 *
 * @param[in]  f        Field.
 * @param[out] out_pval Where to write value.
 * @param[in]  arg      Arbitrary argument.
 * @param[in]  alen     Argument length
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_field_value_ex(
    const ib_field_t *f,
    void             *out_pval,
    const void       *arg,
    size_t            alen
);

/**
 * Get the value stored in the field, passing the argument on to dynamic
 * fields, with type checking.
 *
 * @param[in]  f        Field.
 * @param[out] out_pval Where to write value.
 * @param[in]  t        Field type number.
 * @param[in]  arg      Arbitrary argument.  Use NULL for non-dynamic fields.
 * @param[in]  alen     Argument length.
 *
 * @returns
 * - IB_OK On success.
 * - IB_EINVAL If the type does not match @a t or the field is invalid.
 */
ib_status_t DLL_PUBLIC ib_field_value_type_ex(
    const ib_field_t *f,
    void             *out_pval,
    ib_ftype_t        t,
    const void       *arg,
    size_t            alen
);

/**
 * Get the value stored in the field.
 *
 * @param[in]  f        Field.
 * @param[out] out_pval Where to store value.
 *
 * @returns Status code.
 */
ib_status_t DLL_PUBLIC ib_field_value(
    const ib_field_t *f,
    void             *out_pval
);

/**
 * Get the value stored in the field, with type checking.
 *
 * @param[in]  f        Field.
 * @param[out] out_pval Where to store value.
 * @param[in]  t        Expected type.
 *
 * @returns
 * - IB_OK On success.
 * - IB_EINVAL If the type does not match @a t or the field is invalid.
 */
ib_status_t DLL_PUBLIC ib_field_value_type(
    const ib_field_t *f,
    void             *out_pval,
    ib_ftype_t        t
);

/**
 * Get the value stored in the field.  Non-dynamic only.
 *
 * @param[in]  f                Field.
 * @param[out] mutable_out_pval Where to store value.
 *
 * @returns Status code.
 */
ib_status_t DLL_PUBLIC ib_field_mutable_value(
    ib_field_t *f,
    void       *mutable_out_pval
);

/**
 * Get the value stored in the field, with type checking.  Non-dynamic only.
 *
 * @param[in]  f                Field.
 * @param[out] mutable_out_pval Where to store value.
 * @param[in]  t                Expected type
 *
 * @returns Status code.
 */
ib_status_t DLL_PUBLIC ib_field_mutable_value_type(
    ib_field_t *f,
    void       *mutable_out_pval,
    ib_ftype_t  t
);

/**
 * Determine if a field is dynamic.
 *
 * @param[in] f Field
 *
 * @return true iff field is dynamic
 */
int DLL_PUBLIC ib_field_is_dynamic(
     const ib_field_t *f
);

/**
 * Output debugging information of a field.
 *
 * @param[in] prefix Prefix message.
 * @param[in] f      Field to output.
 */
void DLL_PUBLIC ib_field_util_log_debug(
    const char       *prefix,
    const ib_field_t *f
);

/**
 * Return a string representation of a field type
 *
 * @param[in] ftype Field type
 *
 * @returns String representation of the type
 */
const char DLL_PUBLIC *ib_field_type_name(
    ib_ftype_t ftype
);

/**
 * Attempt to convert a single field.
 *
 * If the desired type matches the in_field type, out_field is set to NULL
 * and IB_OK is returned.
 *
 * @param[in]  mm Memory manager to use.
 * @param[in]  desired_type The type to try to convert this to.
 * @param[in]  in_field The input field.
 * @param[out] out_field The output field to write to.
 *
 * @returns
 *   - IB_OK On success.
 *   - IB_EINVAL If a string cannot be converted to a number type
 *               or some other invalid type conversion is requested.
 *   - IB_EALLOC Memory allocation error.
 */
ib_status_t DLL_PUBLIC ib_field_convert(
    ib_mm_t            mm,
    const ib_ftype_t   desired_type,
    const ib_field_t  *in_field,
    ib_field_t       **out_field
);

/**
 * Convert a string to a field, trying to treat the string as a number if
 * possible.
 *
 * @param[in] mm Memory manager to use for allocations
 * @param[in] name Field name
 * @param[in] nlen Length of @a name
 * @param[in] vstr Value string
 * @param[out] pfield Pointer to newly created field
 *
 * @returns Status code:
 *  - IB_OK All OK
 *  - Errors from @sa ib_field_create().
 */
ib_status_t DLL_PUBLIC ib_field_from_string(
    ib_mm_t      mm,
    const char  *name,
    size_t       nlen,
    const char  *vstr,
    ib_field_t **pfield
);

/**
 * Convert a string to a field, trying to treat the string as a number if
 * possible (Extended version).
 *
 * @param[in] mm Memory manager to use for allocations
 * @param[in] name Field name
 * @param[in] nlen Length of @a name
 * @param[in] vstr Value string
 * @param[in] vlen Length of @a vstr
 * @param[out] pfield Pointer to newly created field
 *
 * @returns Status code:
 *  - IB_OK All OK
 *  - Errors from ib_field_create().
 */
ib_status_t DLL_PUBLIC ib_field_from_string_ex(
    ib_mm_t      mm,
    const char  *name,
    size_t       nlen,
    const char  *vstr,
    size_t       vlen,
    ib_field_t **pfield
);

/**
 * @} IronBeeUtilField
 */

#ifdef __cplusplus
}
#endif

#endif /* _IB_FIELD_H_ */
