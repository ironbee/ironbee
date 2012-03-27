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

#ifndef _IB_TRANSFORMATION_H_
#define _IB_TRANSFORMATION_H_

/**
 * @file
 * @brief IronBee transformation interface
 *
 * @author Nick LeRoy <nleroy@qualys.com>
 */

/**
 * @defgroup IronBeeTransformations Transformations
 * @ingroup IronBee
 * @{
 */

#include <ironbee/build.h>
#include <ironbee/types.h>
#include <ironbee/engine.h>

#ifdef __cplusplus
extern "C" {
#endif


/**
 * Transformation function.
 *
 * @param[in] fndata Transformation function data (config)
 * @param[in] fin Input field
 * @param[out] fout Output field
 * @param[in,out] flags Address of flags set by transformation
 *
 * @returns Status code
 */
typedef ib_status_t (*ib_tfn_fn_t)(ib_engine_t *ib,
                                   ib_mpool_t *pool,
                                   void *fndata,
                                   ib_field_t *fin,
                                   ib_field_t **data_out,
                                   ib_flags_t *pflags);

/**
 * @internal
 *
 * Transformation.
 */
struct ib_tfn_t {
    const char         *name;              /**< Tfn name */
    ib_tfn_fn_t         fn_execute;        /**< Tfn execute function */
    void               *fndata;            /**< Tfn function data */
};


/**
 * @defgroup IronBeeEngineTfn Transformations
 * @{
 */

/** Set if transformation modified the value. */
#define IB_TFN_FMODIFIED          (1<<0)
/** Set if transformation performed an in-place operation. */
#define IB_TFN_FINPLACE           (1<<1)

/**
 * Check if FMODIFIED flag is set.
 *
 * @param f Transformation flags
 *
 * @returns True if FMODIFIED flag is set
 */
#define IB_TFN_CHECK_FMODIFIED(f) ((f) & IB_TFN_FMODIFIED)

/**
 * Check if FINPLACE flag is set.
 *
 * @param f Transformation flags
 *
 * @returns True if FINPLACE flag is set
 */
#define IB_TFN_CHECK_FINPLACE(f) ((f) & IB_TFN_FINPLACE)


/**
 * Create and register a new transformation.
 *
 * @param ib Engine handle
 * @param name Transformation name
 * @param fn_execute Transformation execute function
 * @param fndata Transformation function data
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_tfn_register(ib_engine_t *ib,
                                       const char *name,
                                       ib_tfn_fn_t fn_execute,
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

#define ib_tfn_lookup(ib,name,ptfn) \
    ib_tfn_lookup_ex(ib,name,strlen(name),ptfn)

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
                                        ib_tfn_t *tfn,
                                        ib_field_t *fin,
                                        ib_field_t **fout,
                                        ib_flags_t *pflags);

/**
 * @} IronBeeEngineTfn
 */


#ifdef __cplusplus
}
#endif

/**
 * @} IronBeeTransformations
 */

#endif /* _IB_TRANSFORMATION_H_ */
