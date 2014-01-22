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

/**
 * Operator instance creation callback type.
 *
 * This callback is responsible for doing any calculations need to instantiate
 * the operator, and writing a pointer to instance specific data to
 * @a instance_data.
 *
 * @param[in] ctx Current context.
 * @param[in] parameters Unparsed string with the parameters to
 *                       initialize the operator instance.
 * @param[out] instance_data Instance data.
 * @param[in] cbdata Callback data.
 *
 * @return IB_OK if successful.
 */
typedef ib_status_t (* ib_operator_create_fn_t)(
    ib_context_t *ctx,
    const char   *parameters,
    void         *instance_data,
    void         *cbdata
);

/**
 * Operator instance destruction callback type.
 *
 * This callback is responsible for interpreting @a instance_data and freeing
 * any resources the create function acquired.
 *
 * @param[in] instance_data Instance data.
 * @param[in] cbdata Callback data.
 *
 * @return IB_OK if successful.
 */
typedef ib_status_t (* ib_operator_destroy_fn_t)(
    void *instance_data,
    void *cbdata
);

/**
 * Operator instance execution callback type.
 *
 * This callback is responsible for executing an operator given the instance
 * data create by the create callback.
 *
 * @param[in] tx Current transaction.
 * @param[in] instance_data Instance data needed for execution.
 * @param[in] field The field to operate on.
 * @param[in] capture If non-NULL, the collection to capture to.
 * @param[out] result The result of the operator 1=true 0=false.
 * @param[in] cbdata Callback data.
 *
 * @return IB_OK if successful.
 */
typedef ib_status_t (* ib_operator_execute_fn_t)(
    ib_tx_t          *tx,
    void             *instance_data,
    const ib_field_t *field,
    ib_field_t       *capture,
    ib_num_t         *result,
    void             *cbdata
);

/** Operator Structure */
typedef struct ib_operator_t ib_operator_t;

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
 * This creates an operator.
 *
 * @param[out] op             Where to store new operator.
 * @param[in]  mp             Memory pool to use.
 * @param[in]  name           Name of operator.
 * @param[in]  capabilities   Operator capabilities.
 * @param[in]  fn_create      A pointer to the instance creation function.
 *                            NULL means nop.
 * @param[in]  cbdata_create  Callback data passed to @a fn_create.
 * @param[in]  fn_destroy     A pointer to the instance destruction function.
 *                            NULL means nop.
 * @param[in]  cbdata_destroy Callback data passed to @a fn_destroy.
 * @param[in]  fn_execute     A pointer to the operator function.
 *                            NULL means always true.
 * @param[in]  cbdata_execute Callback data passed to @a fn_execute.
 * @return
 * - IB_OK on success.
 * - IB_EALLOC on allocation failure.
 */
ib_status_t DLL_PUBLIC ib_operator_create(
    ib_operator_t            **op,
    ib_mpool_t                *mp,
    const char                *name,
    ib_flags_t                 capabilities,
    ib_operator_create_fn_t    fn_create,
    void                      *cbdata_create,
    ib_operator_destroy_fn_t   fn_destroy,
    void                      *cbdata_destroy,
    ib_operator_execute_fn_t   fn_execute,
    void                      *cbdata_execute
);

/**
 * Register non-stream operator with engine.
 *
 * @param[in] ib IronBee engine.
 * @param[in] op Operator to register.
 * @return
 * - IB_OK on success.
 * - IB_EINVAL if an operator with same name already exists.
 * - IB_EALLOC on allocation failure.
 */
ib_status_t DLL_PUBLIC ib_operator_register(
    ib_engine_t         *ib,
    const ib_operator_t *op
);

/**
 * Register stream operator with engine.
 *
 * @param[in] ib IronBee engine.
 * @param[in] op Operator to register.
 * @return
 * - IB_OK on success.
 * - IB_EINVAL if an operator with same name already exists.
 * - IB_EALLOC on allocation failure.
 */
ib_status_t DLL_PUBLIC ib_operator_stream_register(
    ib_engine_t         *ib,
    const ib_operator_t *op
);

/**
 * Register and create a non-stream operator.
 *
 * @param[out] op             Where to store new operator, may be NULL.
 * @param[in]  ib             Memory pool to use.
 * @param[in]  name           Name of operator.
 * @param[in]  capabilities   Operator capabilities.
 * @param[in]  fn_create      A pointer to the instance creation function.
 *                            NULL means nop.
 * @param[in]  cbdata_create  Callback data passed to @a fn_create.
 * @param[in]  fn_destroy     A pointer to the instance destruction function.
 *                            NULL means nop.
 * @param[in]  cbdata_destroy Callback data passed to @a fn_destroy.
 * @param[in]  fn_execute     A pointer to the operator function.
 *                            NULL means always true.
 * @param[in]  cbdata_execute Callback data passed to @a fn_execute.
 * @return
 * - IB_OK on success.
 * - IB_EINVAL if an operator with same name already exists.
 * - IB_EALLOC on allocation failure.
 */
ib_status_t DLL_PUBLIC ib_operator_create_and_register(
    ib_operator_t            **op,
    ib_engine_t               *ib,
    const char                *name,
    ib_flags_t                 capabilities,
    ib_operator_create_fn_t    fn_create,
    void                      *cbdata_create,
    ib_operator_destroy_fn_t   fn_destroy,
    void                      *cbdata_destroy,
    ib_operator_execute_fn_t   fn_execute,
    void                      *cbdata_execute
);

/**
 * Register and create a stream operator.
 *
 * @param[out] op             Where to store new operator, may be NULL.
 * @param[in]  ib             Memory pool to use.
 * @param[in]  name           Name of operator.
 * @param[in]  capabilities   Operator capabilities.
 * @param[in]  fn_create      A pointer to the instance creation function.
 *                            NULL means nop.
 * @param[in]  cbdata_create  Callback data passed to @a fn_create.
 * @param[in]  fn_destroy     A pointer to the instance destruction function.
 *                            NULL means nop.
 * @param[in]  cbdata_destroy Callback data passed to @a fn_destroy.
 * @param[in]  fn_execute     A pointer to the operator function.
 *                            NULL means always true.
 * @param[in]  cbdata_execute Callback data passed to @a fn_execute.
 * @return
 * - IB_OK on success.
 * - IB_EINVAL if an operator with same name already exists.
 * - IB_EALLOC on allocation failure.
 */
ib_status_t DLL_PUBLIC ib_operator_stream_create_and_register(
    ib_operator_t            **op,
    ib_engine_t               *ib,
    const char                *name,
    ib_flags_t                 capabilities,
    ib_operator_create_fn_t    fn_create,
    void                      *cbdata_create,
    ib_operator_destroy_fn_t   fn_destroy,
    void                      *cbdata_destroy,
    ib_operator_execute_fn_t   fn_execute,
    void                      *cbdata_execute
);

/**
 * Lookup non-stream operator by name.
 *
 * @param[in] ib Ironbee engine.
 * @param[in] name Name of operator.
 * @param[out] op Operator.
 * @return
 * - IB_OK on success.
 * - IB_ENOENT if no such operator.
 */
ib_status_t DLL_PUBLIC ib_operator_lookup(
    ib_engine_t          *ib,
    const char           *name,
    const ib_operator_t **op
);

/**
 * Lookup stream operator by name.
 *
 * @param[in] ib Ironbee engine.
 * @param[in] name Name of operator.
 * @param[out] op Operator.
 * @return
 * - IB_OK on success.
 * - IB_ENOENT if no such operator.
 */
ib_status_t DLL_PUBLIC ib_operator_stream_lookup(
    ib_engine_t          *ib,
    const char           *name,
    const ib_operator_t **op
);

/**
 * Get the name of an operator.
 *
 * @param[in] op Operator.
 * @return Name.
 */
const char DLL_PUBLIC *ib_operator_get_name(
    const ib_operator_t *op
);

/**
 * Get the capabilities of an operator.
 *
 * @param[in] op Operator.
 * @return Capabilities.
 */
ib_flags_t DLL_PUBLIC ib_operator_get_capabilities(
    const ib_operator_t *op
);

/**
 * Create an operator instance.
 *
 * Validates capabilities and invokes the creation function for the given
 * operator.
 *
 * @param[in] op Operator to create.
 * @param[in] ctx Current IronBee context
 * @param[in] required_capabilities Required operator capabilities.
 * @param[in] parameters Parameters used to create the instance.
 * @param[out] instance_data Instance data.  Can be treated as in handle,
 *                           i.e., `T **`.
 *
 * @return
 * - IB_OK on success,
 * - IB_EINVAL if the required capabilities do not match.
 * - Creation callback status if it reports an error.
 */
ib_status_t DLL_PUBLIC ib_operator_inst_create(
    const ib_operator_t *op,
    ib_context_t        *ctx,
    ib_flags_t           required_capabilities,
    const char          *parameters,
    void                *instance_data
);

/**
 * Destroy an operator instance.
 *
 * Calls the destroy function for the given operator.
 *
 * @param[in] op Operator to destroy instance of.
 * @param[in] instance_data Instance data of instance.
 *
 * @return
 * - IB_OK if no destroy callback registered.
 * - Status code of destroy callback.
 */
ib_status_t DLL_PUBLIC ib_operator_inst_destroy(
    const ib_operator_t *op,
    void                *instance_data
);

/**
 * Call the execute callback for an operator instance.
 *
 * @param[in] op Operator to execute.
 * @param[in] instance_data Operator instance data.
 * @param[in] tx Current transaction.
 * @param[in] field Field to operate on.
 * @param[in] capture Collection to capture to or NULL if no capture needed.
 * @param[out] result The result of the operator
 *
 * @return
 * - IB_OK on success.
 * - Execution callback status if it reports an error.
 */
ib_status_t DLL_PUBLIC ib_operator_inst_execute(
    const ib_operator_t *op,
    void                *instance_data,
    ib_tx_t             *tx,
    const ib_field_t    *field,
    ib_field_t          *capture,
    ib_num_t            *result
);

#ifdef __cplusplus
}
#endif

/**
 * @} IronBeeOperators
 */

#endif /* _IB_OPERATOR_H_ */
