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

#ifndef _IB_OPERATOR_H_
#define _IB_OPERATOR_H_

/**
 * @file
 * @brief IronBee operator interface
 *
 * @author Craig Forbes <cforbes@qualys.com>
 */

#include <ironbee/types.h>
#include <ironbee/field.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Operator instance creation callback type */
typedef ib_status_t (* ib_operator_create_fn_t)(void *data);

/** Operator instance destruction callback type */
typedef ib_status_t (* ib_operator_destroy_fn_t)(void);

/** Operator instance execution callback type */
typedef ib_status_t (* ib_operator_execute_fn_t)(void *data,
                                                 ib_field_t *field,
                                                 ib_num_t *result);

/** Operator Structure */
typedef struct ib_operator_t ib_operator_t;

struct ib_operator_t {
    char *name; /**< Name of the operator */
    ib_operator_create_fn_t fn_create; /**< Instance creation function. */
    ib_operator_destroy_fn_t fn_destroy; /**< Instance destroy function. */
    ib_operator_execute_fn_t fn_execute; /**< Instance executtion function. */
};

/** Operator Instance Structure */
typedef struct ib_operator_instance_t ib_operator_instance_t;

struct ib_operator_instance_t {
    struct ib_operator_t *op; /**< Pointer to this instance's operator type */
    void *data; /**< Data passed to the execute function */
};

/**
 * Register a operator.
 * This registers the name and the callbacks used to create, destroy and
 * execute an operator instance.
 *
 * If the name is not unique an error status will be returned.
 *
 * @param[in] name The name of the operator.
 * @param[in] fn_create A pointer to the instance creation function.
 * @param[in] fn_destroy A pointer to the instance destruction function.
 * @param[in] fn_execute A pointer to the operator function.
 *
 * @returns IB_OK on success, IB_EINVAL if the name is not unique.
 */
ib_status_t register_operator(const char *name,
                              ib_operator_create_fn_t fn_create,
                              ib_operator_destroy_fn_t fn_destroy,
                              ib_operator_execute_fn_t fn_execute);

/**
 * Create a operator instance.
 * Looks up the operator by name and executes the operator creation callback.
 *
 * @param[in] name The name of the operator to create.
 * @param[in] unparsed_data Instance data passed to the create function.
 * @param[out] instance The resulting instance.
 *
 * @returns IB_OK on success, IB_EINVAL if the named operator does not exist.
 */
ib_status_t create_operator_instance(const char *name,
                                     const char *unparsed_data,
                                     ib_operator_instance_t *instance);

/**
 * Call the execute function for a operator instance.
 *
 * @param[in] op Operator instance to use.
 * @param[in] field Field to operate on.
 * @param[out] result The result of the operator
 *
 * @returns IB_OK on success
 */
ib_status_t call_operator_instance(const ib_operator_instance_t *op,
                                ib_field_t *field,
                                ib_num_t *result);

#ifdef __cplusplus
}
#endif

#endif /* _IB_OPERATOR_H_ */
