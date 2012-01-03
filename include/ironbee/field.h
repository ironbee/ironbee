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

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>

#include <sys/types.h>
#include <unistd.h>

#include <netinet/in.h>
#include <arpa/inet.h>


#include <ironbee/types.h>
#include <ironbee/array.h>
#include <ironbee/list.h>

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
 * @param f Field
 * @param data Userdata
 *
 * @returns Value
 */
typedef void *(*ib_field_get_fn_t)(ib_field_t *f,
                                   const void *arg, size_t alen,
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
 * Compare the field's name to a c-style string.
 *
 * Compare the field's name to a known string.
 *
 * @param[in] field Field structure
 * @param[in] namestr Name string to compare to
 *
 * @returns 0:Name matches; else:Name doesn't match
 */
int DLL_PUBLIC ib_field_namecmp(const ib_field_t *field, const char *namestr);

#define ib_field_namecmp(field,namestr) \
    (  (strlen(namestr) != (field)->nlen) \
       || (strncmp((field)->name, (namestr), (field)->nlen) != 0) )
 

/**
 * Get the value stored in the field, passing the argument on to dynamic fields.
 *
 * @param[in] f Field
 * @param[in] arg Arbitrary argument
 * @param[in] alen Argument length
 *
 * @returns Value stored in the field
 */
void DLL_PUBLIC *ib_field_value_ex(ib_field_t *f,
                                   const void *arg,
                                   size_t alen);

/**
 * Get the value stored in the field, passing the argument on to dynamic
 * fields, with type checking.
 *
 * @param[in] f Field
 * @param[in] t Field type number
 * @param[in] arg Arbitrary argument
 * @param[in] alen Argument length
 *
 * @returns Value stored in the field
 */
void DLL_PUBLIC *ib_field_value_type_ex(ib_field_t *f,
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
void DLL_PUBLIC *ib_field_value(ib_field_t *f);

/**
 * Get the value stored in the field, with type checking.
 *
 * @param[in] f Field
 * @param[in] t Expected type
 *
 * @returns Value stored in the field
 */
void DLL_PUBLIC *ib_field_value_type(ib_field_t *f, ib_ftype_t t);

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
int ib_field_is_dynamic(ib_field_t *f);

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

#ifdef __cplusplus
}
#endif

#endif /* _IB_FIELD_H_ */
