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

#ifndef _IB_FIELD_H_
#define _IB_FIELD_H_

/**
 * @file
 * @brief IronBee - Field Utility Functions
 *
 * @author Brian Rectanus <brectanus@qualys.com>
 */

#include <ironbee/build.h>
#include <ironbee/types.h>

#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup IronBeeUtilField Field
 * @ingroup IronBeeUtil
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
#define IB_FTYPE_SBUFFER      6          /**< Stream buffer */

/**
 * Dynamic field get function.
 *
 * @param field Field in question; use to access type.
 * @param arg   Optional argument (e.g., subkey).
 * @param alen  Length of @a arg.
 * @param data  Callback data.
 *
 * @returns Value.
 */
typedef const void *(*ib_field_get_fn_t)(
    const ib_field_t *field,
    const void *arg, size_t alen,
    void *data
);

/**
 * Dynamic field set function.
 *
 * @param field Field in question; use to access type.
 * @param arg   Optional argument (e.g., subkey).
 * @param alen  Length of @a arg.
 * @param val   Value to set.
 * @param data  Callback data.
 *
 * @returns Status code
 */
typedef ib_status_t (*ib_field_set_fn_t)(
    ib_field_t *field,
    const void *arg, size_t alen,
    const void *val,
    void *data
);


/** Field Structure */
struct ib_field_t {
    ib_mpool_t      *mp;        /**< Memory pool */
    ib_ftype_t       type;      /**< Field type */
    const char      *name;      /**< Field name; not '\0' terminated! */
    size_t           nlen;      /**< Field name length */
    const char      *tfn;       /**< Transformations performed */
    ib_field_val_t  *val;       /**< Private value store */
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
                                          const void *pval);

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
                                        const ib_field_t *src);

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
                                       const void *pval);

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
 * Add a buffer to a @ref IB_FTYPE_SBUFFER type field.
 *
 * @param f Field list
 * @param dtype Data type
 * @param buf Buffer
 * @param blen Buffer length
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_field_buf_add(ib_field_t *f,
                                        int dtype,
                                        uint8_t *buf,
                                        size_t blen);

/**
 * Set a field value, skipping dynamic setter.
 *
 * @param f Field to add
 * @param pval Pointer to value to store in field (based on type)
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_field_setv_static(ib_field_t *f,
                                            const void *pval);

/**
 * Set a field value.
 *
 * @param f Field to add
 * @param pval Pointer to value to store in field (based on type)
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_field_setv(ib_field_t *f,
                                     const void *pval);

/**
 * Set a field value, passing the argument on to dynamic fields.
 *
 * @param f Field to add
 * @param pval Pointer to value to store in field (based on type)
 * @param[in] arg Arbitrary argument.  Use NULL for non-dynamic fields.
 * @param[in] alen Argument length
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_field_setv_ex(ib_field_t *f,
                                        const void *pval,
                                        const void* arg,
                                        size_t alen);

/**
 * Get the value stored in the field, passing the argument on to dynamic fields.
 *
 * @param[in] f Field
 * @param[in] arg Arbitrary argument.  Use NULL for non-dynamic fields.
 * @param[in] alen Argument length
 *
 * @returns Value stored in the field
 */
const void DLL_PUBLIC *ib_field_value_ex(const ib_field_t *f,
                                         const void *arg,
                                         size_t alen);

/**
 * Get the value stored in the field, passing the argument on to dynamic
 * fields, with type checking.
 *
 * @param[in] f Field
 * @param[in] t Field type number
 * @param[in] arg Arbitrary argument.  Use NULL for non-dynamic fields.
 * @param[in] alen Argument length
 *
 * @returns Value stored in the field.
 */
const void DLL_PUBLIC *ib_field_value_type_ex(const ib_field_t *f,
                                              ib_ftype_t t,
                                              const void *arg,
                                              size_t alen);

/** Return field value for a field as "ib_num_t *" with argument. */
#define ib_field_value_num_ex(f,arg,alen) \
    (ib_num_t *)ib_field_value_type_ex((f),IB_FTYPE_NUM,(arg),(alen))

/** Return field value for a field as "ib_unum_t * with argument". */
#define ib_field_value_unum_ex(f,arg,alen) \
    (ib_unum_t *)ib_field_value_type_ex((f),IB_FTYPE_UNUM,(arg),(alen))

/** Return field value for a field as "ib_bytestr_t * with argument". */
#define ib_field_value_bytestr_ex(f,arg,alen) \
    (ib_bytestr_t *)ib_field_value_type_ex((f),IB_FTYPE_BYTESTR,(arg),(alen))

/** Return field value for a field as "void * with argument". */
#define ib_field_value_generic_ex(f,arg,alen) \
    (void *)ib_field_value_type_ex((f),IB_FTYPE_GENERIC,(arg),(alen))

/** Return field value for a field as "char * with argument". */
#define ib_field_value_nulstr_ex(f,arg,alen) \
    (char *)ib_field_value_type_ex((f),IB_FTYPE_NULSTR,(arg),(alen))

/** Return field value for a field as "ib_list_t * with argument". */
#define ib_field_value_list_ex(f,arg,alen) \
    (ib_list_t *)ib_field_value_type_ex((f),IB_FTYPE_LIST,(arg),(alen))


/**
 * Get the value stored in the field.
 *
 * @param[in] f Field
 *
 * @returns Value stored in the field
 */
const void DLL_PUBLIC *ib_field_value(const ib_field_t *f);

/**
 * Get the value stored in the field, with type checking.
 *
 * @param[in] f Field
 * @param[in] t Expected type
 *
 * @returns Value stored in the field
 */
const void DLL_PUBLIC *ib_field_value_type(const ib_field_t *f, ib_ftype_t t);

/** Return field value for a field as "ib_num_t *". */
#define ib_field_value_num(f) \
    (ib_num_t *)ib_field_value_type((f), IB_FTYPE_NUM)

/** Return field value for a field as "ib_unum_t *". */
#define ib_field_value_unum(f) \
    (ib_unum_t *)ib_field_value_type((f), IB_FTYPE_UNUM)

/** Return field value for a field as "ib_bytestr_t *". */
#define ib_field_value_bytestr(f) \
    (ib_bytestr_t *)ib_field_value_type((f), IB_FTYPE_BYTESTR)

/** Return field value for a field as "void *". */
#define ib_field_value_generic(f) \
    (void *)ib_field_value_type((f), IB_FTYPE_GENERIC)

/** Return field value for a field as "char *". */
#define ib_field_value_nulstr(f) \
    (char *)ib_field_value_type((f), IB_FTYPE_NULSTR)

/** Return field value for a field as "ib_list_t *". */
#define ib_field_value_list(f) \
    (ib_list_t *)ib_field_value_type((f), IB_FTYPE_LIST)

/** Return field value for a field as "ib_stream_t *". */
#define ib_field_value_stream(f) (ib_stream_t *)ib_field_value(f)

/**
 * Determine if a field is dynamic.
 *
 * @param f Field
 *
 * @return true if field is dynamic
 */
int ib_field_is_dynamic(const ib_field_t *f);

/**
 * Register dynamic get function.
 *
 * @param f          Field.
 * @param fn_get     Get function.
 * @param cbdata_get Callback data for @a fn_get.
 */
void DLL_PUBLIC ib_field_dyn_register_get(ib_field_t *f,
                                          ib_field_get_fn_t fn_get,
                                          void *cbdata_get);

/**
 * Register dynamic set function.
 *
 * @param f          Field.
 * @param fn_set     set function.
 * @param cbdata_set Callback data for @a fn_set.
 */
void DLL_PUBLIC ib_field_dyn_register_set(ib_field_t *f,
                                          ib_field_set_fn_t fn_set,
                                          void *cbdata_set);

/**
 * Helper function for returning numbers.
 *
 * IB_FTYPE_NUM values need to be returned as ib_num_t*, i.e., pointer to
 * values.  This can be problemating for dynamic getters that may have
 * calculated that value on the fly.  This helper function stores the
 * result in the field (without making it not dynamic) and returns a pointer
 * to that value.  E.g.,
 *
 * @code
 * IB_FTRACE_RET_PTR(void, ib_field_dyn_return_num(f, 17));
 * @endcode
 *
 * Note that @a field is passed in as a const.  Caching the value does not
 * semantically change the field (it remains dynamic).
 *
 * @sa ib_field_dyn_return_unum()
 * @param[in] field Field in question.
 * @param[in] value Value to return.
 */
void *ib_field_dyn_return_num(const ib_field_t *f, ib_num_t value);

/**
 * As ib_field_dyn_return_num(), but for unum.
 *
 * @sa ib_field_dyn_return_num()
 * @param[in] field Field in question.
 * @param[in] value Value to return.
 */
void *ib_field_dyn_return_unum(const ib_field_t *f, ib_unum_t value);

/**
 * @} IronBeeUtilField
 */

#ifdef __cplusplus
}
#endif

#endif /* _IB_FIELD_H_ */
