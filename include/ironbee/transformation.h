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
 * @author Sam Baskinger <sbaskinger@qualys.com>
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

/**
 * @defgroup IronBeeTransformations Transformations
 * @ingroup IronBee
 *
 * Transformations modify data.
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

/** Transformation */
typedef struct ib_transformation_t ib_transformation_t;
/** Transformation Instance */
typedef struct ib_transformation_inst_t ib_transformation_inst_t;

/**
 * Transformation instance creation callback.
 *
 * This callback is responsible for doing any calculations needed to
 * instantiate the transformation, and writing a pointer to any transformation
 * specific data to @a instance_data.
 *
 * @param[in]  mm            Memory manager.
 * @param[in]  parameters    Parameters.
 * @param[out] instance_data Instance data to pass to execute.  Treat as
 *                           `void **`.
 * @param[in]  cbdata        Callback data.
 *
 * @return
 * - IB_OK On success.
 * - Other on failure.
 */
typedef ib_status_t (* ib_transformation_create_fn_t)(
    ib_mm_t     mm,
    const char *parameters,
    void       *instance_data,
    void       *cbdata
)
NONNULL_ATTRIBUTE(3);

/**
 * Transformation instance destruction callback.
 *
 * This callback is responsible for interpreting @a instance_data and freeing
 * any resources the create function acquired.
 *
 * @param[in] instance_data Instance data produced by create.
 * @param[in] cbdata        Callback data.
 */
typedef void (* ib_transformation_destroy_fn_t)(
    void *instance_data,
    void *cbdata
);

/**
 * Transformation instance creation callback type.
 *
 * This callback is responsible for executing an transformation given the
 * instance data create by the create callback.
 *
 * Implementations of this type should follow some basic rules:
 *
 * -# Do not log, unless absolutely necessary. The caller should log.
 * -# All input types should have well defined behavior, even if that
 *    behavior is to return IB_EINVAL.
 * -# Fields may have null names with the length set to 0. Do
 *    not assume that all fields come from vars.
 * -# @a fout Should not be changed unless you are returning IB_OK.
 * -# @a fout May be assigned @a fin if no transformation is
 *    necessary. Fields are immutable.
 * -# Allocate out of the given @a mm so that if you do assign to @a fout
 *    the lifetime will be appropriate.
 *
 * @param[in]  mm       Memory manager.
 * @param[in]  fin      Input field. This may be assigned to @a fout.
 * @param[out] fout     Output field. This may point to @a fin.
 * @param[in]  instance_data Instance data.
 * @param[in]  cbdata   Callback data.
 *
 * @return
 * - IB_OK on success.
 * - IB_EALLOC on memory allocatio errors.
 * - IB_EINVAL if input field type is incompatible.
 * - IB_EOTHER something unexpected happened.
 */
typedef ib_status_t (* ib_transformation_execute_fn_t)(
    ib_mm_t            mm,
    const ib_field_t  *fin,
    const ib_field_t **fout,
    void              *instance_data,
    void              *cbdata
)
NONNULL_ATTRIBUTE(3, 4);

/**
 * Create a transformation.
 *
 * The create and destroy callbacks may be NULL.
 *
 * @param[out] tf             Created transformation.
 * @param[in]  mm             Memory manager.
 * @param[in]  name           Name.
 * @param[in]  handle_list    If true, list values will be passed in whole.
 *                            If false, list values will be passed in element
 *                            by element.
 * @param[in]  create_fn      Create function.
 * @param[in]  create_cbdata  Create callback data.
 * @param[in]  execute_fn     Execute function.
 * @param[in]  execute_cbdata Execute callback data.
 * @param[in]  destroy_fn     Destroy function.
 * @param[in]  destroy_cbdata Destroy callback data.
 *
 * @return
 * - IB_OK on success.
 * - IB_EALLOC on allocation failure.
 */
ib_status_t DLL_PUBLIC ib_transformation_create(
    ib_transformation_t            **tfn,
    ib_mm_t                          mm,
    const char                      *name,
    bool                             handle_list,
    ib_transformation_create_fn_t    create_fn,
    void                            *create_cbdata,
    ib_transformation_destroy_fn_t   destroy_fn,
    void                            *destroy_cbdata,
    ib_transformation_execute_fn_t   execute_fn,
    void                            *execute_cbdata
)
NONNULL_ATTRIBUTE(1, 3, 9);

/**
 * Register a transformation with engine.
 *
 * @param[in] ib  IronBee engine.
 * @param[in] tfn Transformation to register.
 *
 * @return
 * - IB_OK on success.
 * - IB_EINVAL if a transformation with same name exists.
 * - IB_EALLOC on memory allocation errors.
 */
ib_status_t DLL_PUBLIC ib_transformation_register(
    ib_engine_t               *ib,
    const ib_transformation_t *tfn
)
NONNULL_ATTRIBUTE(1, 2);

/**
 * Create and register a transformation.
 *
 * @sa ib_transformation_create()
 * @sa ib_transformation_register()
 *
 * @param[out] tfn            Created transformation.  May be NULL.
 * @param[in]  ib             IronBee engine.
 * @param[in]  name           Name.
 * @param[in]  handle_list    If true, list values will be passed in whole.
 *                            If false, list values will be passed in element
 *                            by element.
 * @param[in] create_fn       Create function.
 * @param[in] create_cbdata   Create callback data.
 * @param[in] destroy_fn      Destroy function.
 * @param[in] destroy_cbdata  Destroy callback data.
 * @param[in] execute_fn      Execute function.
 * @param[in] execute_cbdata  Execute callback data.
 *
 * @return
 * - IB_OK on success.
 * - IB_EALLOC on memory allocation errors.
 * - IB_EINVAL if a transformation with same name exists.
 */
ib_status_t DLL_PUBLIC ib_transformation_create_and_register(
    const ib_transformation_t      **tfn,
    ib_engine_t                     *ib,
    const char                      *name,
    bool                             handle_list,
    ib_transformation_create_fn_t    create_fn,
    void                            *create_cbdata,
    ib_transformation_destroy_fn_t   destroy_fn,
    void                            *destroy_cbdata,
    ib_transformation_execute_fn_t   execute_fn,
    void                            *execute_cbdata
)
NONNULL_ATTRIBUTE(2, 3, 9);

/**
 * Lookup a transformation by name.
 *
 * @param[in]  ib          IronBee engine.
 * @param[in]  name        Name of transformation.
 * @param[in]  name_length Length of @a name.
 * @param[out] tfn         Transformation if found.
 *
 * @return
 * - IB_OK on success.
 * - IB_ENOENT if no such transformation.
 */
ib_status_t DLL_PUBLIC ib_transformation_lookup(
    ib_engine_t                *ib,
    const char                 *name,
    size_t                      name_length,
    const ib_transformation_t **tfn
)
NONNULL_ATTRIBUTE(1, 2);

/**
 * Name accessor.
 *
 * @param[in] tfn Transformation.
 *
 * @return Name.
 */
const char DLL_PUBLIC *ib_transformation_name(
    const ib_transformation_t *tfn
)
NONNULL_ATTRIBUTE(1);

/**
 * Handle list accessor.
 *
 * @sa ib_transformation_create().
 *
 * @param[in] tfn Transformation.
 *
 * @return
 * - Return true if @a tfn should receive the entire list of elements.
 * - Return false if @a tfn should receive each list element, one at a time.
 */
bool DLL_PUBLIC ib_transformation_handle_list(
    const ib_transformation_t *tfn
)
NONNULL_ATTRIBUTE(1);

/**
 * Create a transformation instance.
 *
 * The destroy function will be register to be called when @a mm is cleaned
 * up.
 *
 * @param[out] tfn_inst   The transformation instance.
 * @param[in]  mm         Memory manager.
 * @param[in]  tfn        Transformation to create an instance of.
 * @param[in]  parameters Parameters used to create the instance.
 *
 * @return
 * - IB_OK On success.
 * - IB_EALLOC On allocation failure.
 * - Other if create callback fails.
 */
ib_status_t DLL_PUBLIC ib_transformation_inst_create(
    ib_transformation_inst_t  **tfn_inst,
    ib_mm_t                     mm,
    const ib_transformation_t  *tfn,
    const char                 *parameters
)
NONNULL_ATTRIBUTE(1, 3);

/**
 * Transformation accessor.
 *
 * @param[in] tfn_inst Transformation instance.
 *
 * @return Transformation.
 **/
const ib_transformation_t DLL_PUBLIC *ib_transformation_inst_transformation(
    const ib_transformation_inst_t *tfn_inst
)
NONNULL_ATTRIBUTE(1);

/**
 * Parameters accessor.
 *
 * @param[in] tfn_inst Transformation instance.
 *
 * @return Parameters.
 */
const char DLL_PUBLIC *ib_transformation_inst_parameters(
    const ib_transformation_inst_t *tfn_inst
)
NONNULL_ATTRIBUTE(1);

/**
 * Instance data accessor.
 *
 * @param[in] tfn_inst Transformation instance.
 *
 * @return Instance data.
 */
void DLL_PUBLIC *ib_transformation_inst_data(
    const ib_transformation_inst_t *tfn_inst
);

/**
 * Execute transformation.
 *
 * @param[in]  tfn_inst Transformation instance.
 * @param[in]  mm       Memory manager.
 * @param[in]  fin      Input data field.
 * @param[out] fout     Output data field; may be set to @a fin.
 *
 * @return
 * - IB_OK on success.
 * - IB_EALLOC on allocation failure.
 * - Other on other failure.
 */
ib_status_t DLL_PUBLIC ib_transformation_inst_execute(
    const ib_transformation_inst_t  *tfn_inst,
    ib_mm_t                          mm,
    const ib_field_t                *fin,
    const ib_field_t               **fout
)
NONNULL_ATTRIBUTE(1, 3, 4);

#ifdef __cplusplus
}
#endif

/**
 * @} IronBeeTransformations
 */

#endif /* _IB_TRANSFORMATION_H_ */
