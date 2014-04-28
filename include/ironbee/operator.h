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

#ifndef _IB_OPERATOR_H_
#define _IB_OPERATOR_H_

/**
 * @file
 * @brief IronBee --- Operator interface
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

/**
 * @defgroup IronBeeOperators Operators
 * @ingroup IronBee
 *
 * Operators interpret, modify, or compare data.
 *
 * @{
 */

#include <ironbee/build.h>
#include <ironbee/engine.h>
#include <ironbee/field.h>
#include <ironbee/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Operator */
typedef struct ib_operator_t ib_operator_t;
/** Operator Instance */
typedef struct ib_operator_inst_t ib_operator_inst_t;

/**
 * Operator instance creation callback.
 *
 * This callback is responsible for doing any calculations needed to
 * instantiate the operator, and writing a pointer to any operator specific data
 * to @a instance_data.
 *
 * @param[in]  ctx           Context of operator.
 * @param[in]  mm            Memory manager.
 * @param[in]  parameters    Parameters.
 * @param[out] instance_data Instance data.  Treat as `void **`.
 * @param[in]  cbdata        Callback data.
 *
 * @return IB_OK if successful.
 */
typedef ib_status_t (* ib_operator_create_fn_t)(
    ib_context_t *ctx,
    ib_mm_t       mm,
    const char   *parameters,
    void         *instance_data,
    void         *cbdata
)
NONNULL_ATTRIBUTE(1, 4);

/**
 * Operator instance destruction callback.
 *
 * This callback is responsible for interpreting @a instance_data and freeing
 * any resources the create function acquired.
 *
 * @param[in] instance_data Instance data.
 * @param[in] cbdata Callback data.
 */
typedef void (* ib_operator_destroy_fn_t)(
    void *instance_data,
    void *cbdata
);

/**
 * Operator instance execution callback type.
 *
 * This callback is responsible for executing an operator given the instance
 * data create by the create callback.
 *
 * Implementations of this type should follow some basic rules:
 *
 * -# Do not log, unless absolutely necessary. The caller should log.
 * -# All input types should have well defined behavior, even if that
 *    behavior is to return IB_EINVAL.
 * -# Fields may have null names with the length set to 0. Do
 *    not assume that all fields come from vars.
 * -# Allocate out of the given @a mm so that if you do assign to @a fout
 *    the lifetime will be appropriate.
 *
 * @param[in]  tx            Current transoperator.
 * @param[in]  input         The field to operate on.
 * @param[in]  capture       If non-NULL, the collection to capture to.
 * @param[out] result        The result of the operator: 1=true 0=false.
 * @param[in]  instance_data Instance data.
 * @param[in]  cbdata        Callback data.
 *
 * @return
 * - IB_OK on success.
 * - IB_EALLOC on memory allocatio errors.
 * - IB_EINVAL if input field type is incompatible.
 * - IB_EOTHER something unexpected happened.
 */
typedef ib_status_t (* ib_operator_execute_fn_t)(
    ib_tx_t          *tx,
    const ib_field_t *input,
    ib_field_t       *capture,
    ib_num_t         *result,
    void             *instance_data,
    void             *cbdata
)
NONNULL_ATTRIBUTE(1, 2, 4);

/* Operator capabilities */
/*! No capabilities */
#define IB_OP_CAPABILITY_NONE        (0x0)
/*! Accepts NULL fields */
#define IB_OP_CAPABILITY_ALLOW_NULL  (1 << 0)
/*! Supports capture */
#define IB_OP_CAPABILITY_CAPTURE     (1 << 3)

/**
 * Create an operator.
 *
 * All callbacks may be NULL.  If @a execute_fn is NULL, then it will default
 * to always true.
 *
 * @param[out] op             Created operator.
 * @param[in]  mm             Memory manager.
 * @param[in]  name           Name of operator.
 * @param[in]  capabilities   Operator capabilities.
 * @param[in]  create_fn      Create function.
 * @param[in]  create_cbdata  Create callback data.
 * @param[in]  destroy_fn     Destroy function.
 * @param[in]  destroy_cbdata Destroy callback data.
 * @param[in]  execute_fn     Execute function.
 * @param[in]  execute_cbdata Execute callback data.
 *
 * @return
 * - IB_OK on success.
 * - IB_EALLOC on allocation failure.
 */
ib_status_t DLL_PUBLIC ib_operator_create(
    ib_operator_t            **op,
    ib_mm_t                    mm,
    const char                *name,
    ib_flags_t                 capabilities,
    ib_operator_create_fn_t    create_fn,
    void                      *create_cbdata,
    ib_operator_destroy_fn_t   destroy_fn,
    void                      *destroy_cbdata,
    ib_operator_execute_fn_t   execute_fn,
    void                      *execute_cbdata
)
NONNULL_ATTRIBUTE(1, 3);

/**
 * Register non-stream operator with engine.
 *
 * @param[in] ib IronBee engine.
 * @param[in] op Operator to register.
 *
 * @return
 * - IB_OK on success.
 * - IB_EINVAL if an operator with same name already exists.
 * - IB_EALLOC on allocation failure.
 */
ib_status_t DLL_PUBLIC ib_operator_register(
    ib_engine_t         *ib,
    const ib_operator_t *op
)
NONNULL_ATTRIBUTE(1, 2);

/**
 * Register stream operator with engine.
 *
 * @param[in] ib IronBee engine.
 * @param[in] op Operator to register.
 *
 * @return
 * - IB_OK on success.
 * - IB_EALLOC on allocation failure.
 * - IB_EINVAL if an operator with same name already exists.
 */
ib_status_t DLL_PUBLIC ib_operator_stream_register(
    ib_engine_t         *ib,
    const ib_operator_t *op
)
NONNULL_ATTRIBUTE(1, 2);

/**
 * Register and create a non-stream operator.
 *
 * @sa ib_operator_create()
 * @sa ib_operator_register()
 *
 * @param[out] op             Created operator. May be NULL.
 * @param[in]  ib             IronBee engine.
 * @param[in]  name           Name of operator.
 * @param[in]  capabilities   Operator capabilities.
 * @param[in] create_fn       Create function.
 * @param[in] create_cbdata   Create callback data.
 * @param[in] execute_fn      Execute function.
 * @param[in] execute_cbdata  Execute callback data.
 * @param[in] destroy_fn      Destroy function.
 * @param[in] destroy_cbdata  Destroy callback data.
 *
 * @return
 * - IB_OK on success.
 * - IB_EALLOC on memory allocation errors.
 * - IB_EINVAL if an operator with same name exists.
 */
ib_status_t DLL_PUBLIC ib_operator_create_and_register(
    ib_operator_t            **op,
    ib_engine_t               *ib,
    const char                *name,
    ib_flags_t                 capabilities,
    ib_operator_create_fn_t    create_fn,
    void                      *create_cbdata,
    ib_operator_destroy_fn_t   destroy_fn,
    void                      *destroy_cbdata,
    ib_operator_execute_fn_t   execute_fn,
    void                      *execute_cbdata
)
NONNULL_ATTRIBUTE(2, 3);

/**
 * Register and create a stream operator.
 *
 * @sa ib_operator_create()
 * @sa ib_operator_register()
 *
 * @param[out] op             Created operator. May be NULL.
 * @param[in]  ib             IronBee engine.
 * @param[in]  name           Name of operator.
 * @param[in]  capabilities   Operator capabilities.
 * @param[in] create_fn       Create function.
 * @param[in] create_cbdata   Create callback data.
 * @param[in] execute_fn      Execute function.
 * @param[in] execute_cbdata  Execute callback data.
 * @param[in] destroy_fn      Destroy function.
 * @param[in] destroy_cbdata  Destroy callback data.
 *
 * @return
 * - IB_OK on success.
 * - IB_EALLOC on memory allocation errors.
 * - IB_EINVAL if an operator with same name exists.
 */
ib_status_t DLL_PUBLIC ib_operator_stream_create_and_register(
    ib_operator_t            **op,
    ib_engine_t               *ib,
    const char                *name,
    ib_flags_t                 capabilities,
    ib_operator_create_fn_t    create_fn,
    void                      *create_cbdata,
    ib_operator_destroy_fn_t   destroy_fn,
    void                      *destroy_cbdata,
    ib_operator_execute_fn_t   execute_fn,
    void                      *execute_cbdata
)
NONNULL_ATTRIBUTE(2, 3);

/**
 * Lookup a non-stream operator by name.
 *
 * @param[in]  ib          IronBee engine.
 * @param[in]  name        Name of operator.
 * @param[in]  name_length Length of @a name.
 * @param[out] op          Operator if found.
 *
 * @return
 * - IB_OK on success.
 * - IB_ENOENT if no such operator.
 */
ib_status_t DLL_PUBLIC ib_operator_lookup(
    ib_engine_t          *ib,
    const char           *name,
    size_t                name_length,
    const ib_operator_t **op
)
NONNULL_ATTRIBUTE(1, 2);

/**
 * Lookup a stream operator by name.
 *
 * @param[in]  ib          IronBee engine.
 * @param[in]  name        Name of operator.
 * @param[in]  name_length Length of @a name.
 * @param[out] op          Operator if found.
 *
 * @return
 * - IB_OK on success.
 * - IB_ENOENT if no such operator.
 */
ib_status_t DLL_PUBLIC ib_operator_stream_lookup(
    ib_engine_t          *ib,
    const char           *name,
    size_t                name_length,
    const ib_operator_t **op
)
NONNULL_ATTRIBUTE(1, 2);

/**
 * Name accessor.
 *
 * @param[in] op Operator.
 *
 * @return Name.
 */
const char DLL_PUBLIC *ib_operator_name(
    const ib_operator_t *op
)
NONNULL_ATTRIBUTE(1);

/**
 * Capabilities accessor.
 *
 * @param[in] op Operator.
 *
 * @return Capabilities.
 */
ib_flags_t DLL_PUBLIC ib_operator_capabilities(
    const ib_operator_t *op
)
NONNULL_ATTRIBUTE(1);

/**
 * Create an operator instance.
 *
 * The destroy function will be register to be called when @a mm is cleaned
 * up.
 *
 * @param[out] op_inst               The operator instance.
 * @param[in]  mm                    Memory manager.
 * @param[in]  ctx                   Current IronBee context
 * @param[in]  op                    Operator to create instance of.
 * @param[in]  required_capabilities Required operator capabilities.
 * @param[in]  parameters            Parameters used to create the instance.
 *
 * @return
 * - IB_OK on success,
 * - IB_EALLOC on allocation failure.
 * - IB_EINVAL if the required capabilities do not match.
 * - Other if create callback fails.
 */
ib_status_t DLL_PUBLIC ib_operator_inst_create(
    ib_operator_inst_t  **op_inst,
    ib_mm_t               mm,
    ib_context_t         *ctx,
    const ib_operator_t  *op,
    ib_flags_t            required_capabilities,
    const char           *parameters
)
NONNULL_ATTRIBUTE(1, 3, 4);

/**
 * Get the operator of an operator instance.
 *
 * @param[in] op_inst Operator instance to access.
 *
 * @return Operator of operator instance.
 */
const ib_operator_t DLL_PUBLIC *ib_operator_inst_operator(
    const ib_operator_inst_t *op_inst
)
NONNULL_ATTRIBUTE(1);

/**
 * Get the parameters of an operator instance.
 *
 * @param[in] op_inst Operator instance to access.
 *
 * @return Parameters of operator instance.
 */
const char DLL_PUBLIC *ib_operator_inst_parameters(
    const ib_operator_inst_t *op_inst
)
NONNULL_ATTRIBUTE(1);

/**
 * Get the instance data of an operator instance.
 *
 * @param[in] op_inst Operator instance to access.
 *
 * @return Data of operator instance.
 */
void DLL_PUBLIC *ib_operator_inst_data(
    const ib_operator_inst_t *op_inst
)
NONNULL_ATTRIBUTE(1);

/**
 * Execute operator.
 *
 * @param[in]  op_inst Operator instance.
 * @param[in]  tx      Current transoperator.
 * @param[in]  input   Input.
 * @param[in]  capture Collection to capture to or NULL if no capture needed.
 * @param[out] result  The result of the operator
 *
 * @return
 * - IB_OK on success.
 * - IB_EALLOC on allocation failure.
 * - Other on other failure.
 */
ib_status_t DLL_PUBLIC ib_operator_inst_execute(
    const ib_operator_inst_t *op_inst,
    ib_tx_t                  *tx,
    const ib_field_t         *input,
    ib_field_t               *capture,
    ib_num_t                 *result
)
NONNULL_ATTRIBUTE(1, 5);

#ifdef __cplusplus
}
#endif

/**
 * @} IronBeeOperators
 */

#endif /* _IB_OPERATOR_H_ */
