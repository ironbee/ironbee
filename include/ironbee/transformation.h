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

#ifndef _IB_TRANSFORMATION_H_
#define _IB_TRANSFORMATION_H_

/**
 * @file
 * @brief IronBee --- Transformation interface
 *
 * @author Nick LeRoy <nleroy@qualys.com>
 */

/**
 * @defgroup IronBeeTransformations Transformations
 * @ingroup IronBee
 *
 * Transformations modify input.
 *
 * @{
 */

#include <ironbee/build.h>
#include <ironbee/engine.h>
#include <ironbee/types.h>

#ifdef __cplusplus
extern "C" {
#endif


/**
 * The definition of a transformation function.
 *
 * Implementations of this type should follow some basic rules:
 *
 *  -# Do not log, unless absolutely necessary. The caller should log.
 *  -# All input types should have well defined behavior, even if that
 *     behavior is to return IB_EINVAL.
 *  -# Fields may have null names with the length set to 0. Do
 *     not assume that all fields come from the DPI.
 *  -# @a fout Should not be changed unless you are returning IB_OK.
 *  -# @a fout May be assigned @a fin if no transformation is
 *     necessary. Fields are immutable.
 *  -# Allocate out of the given @a mp so that if you do assign @a fin
 *     to @a fout their lifetimes will be the same.
 *
 * @param[in] fndata Transformation function data (config).
 * @param[in] pool Memory pool to use for allocations.
 * @param[in] fin Input field. This may be assigned to @a fout.
 * @param[out] data_out Output field. This may point to @a fin.
 * @param[in,out] pflags Address of flags set by transformation.
 *
 * @returns
 *   - IB_OK On success.
 *   - IB_EALLOC On memory allocation errors.
 *   - IB_EINVAL If input field type is incompatible with this.
 *   - IB_EOTHER Something very unexpected happened.
 */
typedef ib_status_t (*ib_tfn_fn_t)(ib_engine_t *ib,
                                   ib_mpool_t *pool,
                                   void *fndata,
                                   const ib_field_t *fin,
                                   const ib_field_t **data_out,
                                   ib_flags_t *pflags);

/** @cond Internal */

/* Transformation flags */
#define IB_TFN_FLAG_NONE        (0x0)      /**< No flags */
#define IB_TFN_FLAG_HANDLE_LIST (1 << 0)   /**< Tfn can handle lists */

/**
 * Transformation.
 */
struct ib_tfn_t {
    const char         *name;              /**< Tfn name */
    ib_tfn_fn_t         fn_execute;        /**< Tfn execute function */
    ib_flags_t          tfn_flags;         /**< Tfn flags */
    void               *fndata;            /**< Tfn function data */
};
/** @endcond **/

/** Set if transformation modified the value. */
#define IB_TFN_NONE                (0x0)
#define IB_TFN_FMODIFIED          (1<<0)

/**
 * Check if FMODIFIED flag is set.
 *
 * @param f Transformation flags
 *
 * @returns True if FMODIFIED flag is set
 */
#define IB_TFN_CHECK_FMODIFIED(f) ((f) & IB_TFN_FMODIFIED)


/**
 * Create and register a new transformation.
 *
 * @param ib Engine handle
 * @param name Transformation name
 * @param fn_execute Transformation execute function
 * @param flags Transformation flags
 * @param fndata Transformation function data
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_tfn_register(ib_engine_t *ib,
                                       const char *name,
                                       ib_tfn_fn_t fn_execute,
                                       ib_flags_t flags,
                                       void *fndata);

/**
 * Lookup a transformation by name (extended version).
 *
 * @param ib Engine
 * @param name Transformation name
 * @param nlen Transformation name length
 * @param ptfn Address where new tfn will be written
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_tfn_lookup_ex(ib_engine_t *ib,
                                        const char *name,
                                        size_t nlen,
                                        ib_tfn_t **ptfn);

/**
 * Lookup a transformation by name.
 *
 * @param ib Engine
 * @param name Transformation name
 * @param ptfn Address where new tfn will be written
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_tfn_lookup(ib_engine_t *ib,
                                     const char *name,
                                     ib_tfn_t **ptfn);

#define ib_tfn_lookup(ib, name, ptfn) \
    ib_tfn_lookup_ex(ib, name, strlen(name), ptfn)

/**
 * Transform data.
 *
 * @note Some transformations may destroy/overwrite the original data.
 *
 * @param ib IronBee Engine object
 * @param mp Pool to use if memory needs to be allocated
 * @param tfn Transformation
 * @param fin Input data field
 * @param fout Address of output data field
 * @param pflags Address of flags set by transformation
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_tfn_transform(ib_engine_t *ib,
                                        ib_mpool_t *mp,
                                        const ib_tfn_t *tfn,
                                        const ib_field_t *fin,
                                        const ib_field_t **fout,
                                        ib_flags_t *pflags);

/**
 * Get a data field with a transformation (extended version).
 *
 * @param ib IronBee engine.
 * @param data Data.
 * @param name Name as byte string
 * @param nlen Name length
 * @param pf Pointer where new field is written if non-NULL
 * @param tfn Transformations (comma separated names)
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_tfn_data_get_ex(
    ib_engine_t *ib,
    ib_data_t   *data,
    const char  *name,
    size_t       nlen,
    const ib_field_t **pf,
    const char  *tfn
);

/**
 * Get a data field with a transformation.
 *
 * @param ib IronBee engine.
 * @param data Data.
 * @param name Name as NUL terminated string
 * @param pf Pointer where new field is written if non-NULL
 * @param tfn Transformations (comma separated names)
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_tfn_data_get(
    ib_engine_t *ib,
    ib_data_t   *data,
    const char  *name,
    const ib_field_t **pf,
    const char  *tfn
);

#ifdef __cplusplus
}
#endif

/**
 * @} IronBeeTransformations
 */

#endif /* _IB_TRANSFORMATION_H_ */
