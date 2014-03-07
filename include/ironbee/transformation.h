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
#include <ironbee/mm.h>
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
 * @param[in]  instdata Instance data.
 * @param[in]  mm       Memory manager to use for allocations.
 * @param[in]  fin      Input field. This may be assigned to @a fout.
 * @param[out] fout     Output field. This may point to @a fin.
 * @param[in]  cbdata   Callback data.
 *
 * @returns
 * - IB_OK on success.
 * - IB_EALLOC on memory allocation errors.
 * - IB_EINVAL if input field type is incompatible with this.
 * - IB_EOTHER something very unexpected happened.
 */
typedef ib_status_t (*ib_tfn_execute_fn_t)(
    void              *instdata,
    ib_mm_t            mm,
    const ib_field_t  *fin,
    const ib_field_t **fout,
    void              *cbdata
)
NONNULL_ATTRIBUTE(1, 3, 4);

/**
 * Create instance data for a transformation.
 *
 * @param[out] instdata Create an instance of a given transformation
 *             for the given parameter. Implementors should
 *             treat this as a void** to return the instance data.
 * @param[in]  mm Memory manager with the lifetime of the instance to be
 *             created.
 * @param[in]  param The single parameter.
 * @param[in]  cbdata Callback data.
 *
 * @returns
 * - IB_OK On success.
 * - Other on failure.
 */
typedef ib_status_t (*ib_tfn_create_fn_t)(
    void       *instdata,
    ib_mm_t     mm,
    const char *param,
    void       *cbdata
)
NONNULL_ATTRIBUTE(3);

/**
 * Destroy an instance data created by an @ref ib_tfn_create_fn_t.
 *
 * @param[out] instdata The pointer to the instance data to destroy.
 * @param[in]  cbdata Callback data.
 */
typedef void (*ib_tfn_destroy_fn_t)(
    void *instdata,
    void *cbdata
);

/**
 * Create a transformation.
 *
 * @param[out] ptfn          Created transformation.
 * @param[in]  mm            Memory manager to use.
 * @param[in]  name          Name.
 * @param[in]  handle_list   If true, list values will be passed in whole.  If
 *                           false, list values will be passed in element by
 *                           element.
 * @param[in] create_fn      Create function.
 * @param[in] create_cbdata  Create callback data.
 * @param[in] execute_fn     Execute function.
 * @param[in] execute_cbdata Execute callback data.
 * @param[in] destroy_fn     Destroy function.
 * @param[in] destroy_cbdata Destroy callback data.
 *
 * @returns
 * - IB_OK on success.
 * - IB_EALLOC on allocation failure.
 **/
ib_status_t DLL_PUBLIC ib_tfn_create(
    const ib_tfn_t      **ptfn,
    ib_mm_t               mm,
    const char           *name,
    bool                  handle_list,
    ib_tfn_create_fn_t    create_fn,
    void                 *create_cbdata,
    ib_tfn_execute_fn_t   execute_fn,
    void                 *execute_cbdata,
    ib_tfn_destroy_fn_t   destroy_fn,
    void                 *destroy_cbdata
)
NONNULL_ATTRIBUTE(1, 3, 7);

/**
 * Create a transformation instance.
 *
 * The destroy function will be registered to be called when
 * @a mm is destroyed.
 *
 * @param[out] ptfn_inst The transformation instance.
 * @param[in]  mm        Memory manager for allocations.
 * @param[in]  tfn       The transformation to make an instance of.
 * @param[in]  param     The user parameter to the transformation.
 *
 * @returns
 * - IB_OK On success.
 * - IB_EALLOC On allocation failure.
 */
ib_status_t DLL_PUBLIC ib_tfn_inst_create(
    const ib_tfn_inst_t **ptfn_inst,
    ib_mm_t               mm,
    const ib_tfn_t       *tfn,
    const char           *param
)
NONNULL_ATTRIBUTE(1, 3, 4);

/**
 * Register a transformation with @a ib.
 *
 * @param[in] ib  Engine to register with.
 * @param[in] tfn Transformation to register.
 *
 * @returns
 * - IB_OK on success.
 * - IB_EALLOC on memory allocation errors.
 * - IB_EINVAL if a transformation with same name exists.
 **/
ib_status_t DLL_PUBLIC ib_tfn_register(
    ib_engine_t    *ib,
    const ib_tfn_t *tfn
)
NONNULL_ATTRIBUTE(1, 2);

/**
 * Create and register a transformation.
 *
 * @sa ib_tfn_create()
 * @sa ib_tfn_register()
 *
 * @param[out] ptfn           Created transformation.  May be NULL.
 * @param[in]  ib             Engine to register with.
 * @param[in]  name           Name.
 * @param[in]  handle_list    If true, list values will be passed in whole.  If
 *                            false, list values will be passed in element by
 *                            element.
 * @param[in]  create_fn      Create function.
 * @param[in]  create_cbdata  Callback data for @a create_fn.
 * @param[in]  execute_fn     Transformation execute function.
 * @param[in]  execute_cbdata Callback data for @a fn_execute.
 * @param[in]  destroy_fn     Destroy function.
 * @param[in]  destroy_cbdata Destroy callback data.
 * @returns
 * - IB_OK on success.
 * - IB_EALLOC on memory allocation errors.
 * - IB_EINVAL if a transformation with same name exists.
 **/
ib_status_t DLL_PUBLIC ib_tfn_create_and_register(
    const ib_tfn_t      **ptfn,
    ib_engine_t          *ib,
    const char           *name,
    bool                  handle_list,
    ib_tfn_create_fn_t    create_fn,
    void                 *create_cbdata,
    ib_tfn_execute_fn_t   execute_fn,
    void                 *execute_cbdata,
    ib_tfn_destroy_fn_t   destroy_fn,
    void                 *destroy_cbdata
)
NONNULL_ATTRIBUTE(1, 2, 3, 7);

/**
 * Name accessor.
 *
 * @param[in] tfn Transformation to access.
 *
 * @return Name of transformation.
 **/
const char DLL_PUBLIC *ib_tfn_name(const ib_tfn_t *tfn)
NONNULL_ATTRIBUTE(1);

/**
 * Name accessor.
 *
 * @param[in] tfn_inst Transformation instance.
 *
 * @return The name of the transformation.
 */
const char DLL_PUBLIC *ib_tfn_inst_name(const ib_tfn_inst_t *tfn_inst)
NONNULL_ATTRIBUTE(1);


 /**
  * Argument accessor.
  *
  * @param[in] tfn_inst Transformation instance.
  *
  * @returns The parameter for this transformation instance.
  */
 const char DLL_PUBLIC *ib_tfn_inst_param(const ib_tfn_inst_t *tfn_inst)
NONNULL_ATTRIBUTE(1);

/**
 * True if @a tfn_inst gets the whole list, false if it gets each list element.
 *
 * @param[in] tfn_inst The transformation instance to check.
 *
 * @return
 * - Return true if @a tfn should receive the entire list of elements.
 * - Return false if @a tfn should receive each list element, one at a time.
 */
bool DLL_PUBLIC ib_tfn_inst_handle_list(const ib_tfn_inst_t *tfn_inst)
NONNULL_ATTRIBUTE(1);

/**
 * True if @a tfn gets the whole list, false if it gets each list element.
 *
 * @param[in] tfn The transformation instance to check.
 *
 * @return
 * - Return true if @a tfn should receive the entire list of elements.
 * - Return false if @a tfn should receive each list element, one at a time.
 */
bool DLL_PUBLIC ib_tfn_handle_list(const ib_tfn_t *tfn)
NONNULL_ATTRIBUTE(1);

/**
 * Lookup a transformation by name (extended version).
 *
 * @param[in]  ib   Engine.
 * @param[in]  name Name.
 * @param[in]  nlen Length of @a name.
 * @param[out] ptfn Transformation if found.
 *
 * @returns
 * - IB_OK on success.
 * - IB_ENOENT if transformation not found.
 */
ib_status_t DLL_PUBLIC ib_tfn_lookup_ex(
    ib_engine_t     *ib,
    const char      *name,
    size_t           nlen,
    const ib_tfn_t **ptfn
)
NONNULL_ATTRIBUTE(1, 2, 4);

/**
 * Lookup a transformation by name.
 *
 * @param[in]  ib   Engine.
 * @param[in]  name Transformation name.
 * @param[out] ptfn Transformation if found.
 *
 * @returns
 * - IB_OK on success.
 * - IB_ENOENT if transformation not found.
 */
ib_status_t DLL_PUBLIC ib_tfn_lookup(
    ib_engine_t     *ib,
    const char      *name,
    const ib_tfn_t **ptfn
)
NONNULL_ATTRIBUTE(1, 2, 3);

/**
 * Transform data.
 *
 * @param[in]  mm       Memory manager to use.
 * @param[in]  tfn_inst Transformation to apply.
 * @param[in]  fin      Input data field.
 * @param[out] fout     Output data field; may be set to @a fin.
 *
 * @returns
 * - IB_OK on success.
 * - IB_EALLOC on allocation failure.
 * - Status code of transformation on other failure.
 */
ib_status_t DLL_PUBLIC ib_tfn_inst_execute(
    const ib_tfn_inst_t  *tfn_inst,
    ib_mm_t               mm,
    const ib_field_t     *fin,
    const ib_field_t    **fout
)
NONNULL_ATTRIBUTE(1, 3, 4);

#ifdef __cplusplus
}
#endif

/**
 * @} IronBeeTransformations
 */

#endif /* _IB_TRANSFORMATION_H_ */
