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

/**
 * @file
 * @brief IronBee --- TestOps module
 *
 * This is a module that defines some rule operators for development purposes.
 *
 * The operators defined here are:
 * - <tt>true</tt>: Always returns True.
 * - <tt>false</tt>: Always returns False.
 * - <tt>exists</tt>: Returns True if the rule target exists.
 * - <tt>is_int</tt>: Returns True if the rule target type is INT.
 * - <tt>is_time</tt>: Returns True if the rule target type is TIME.
 * - <tt>is_float</tt>: Returns True if the rule target type is FLOAT.
 * - <tt>is_string</tt>: Returns True if the rule target type is STRING.
 * - <tt>is_sbuffer</tt>: Returns True if the rule target type is SBUFFER.
 *
 * Examples:
 * - <tt>rule x \@true x id:1 setvar:x=4</tt>
 * - <tt>rule x \@false x id:2 !setvar:x=5</tt>
 * - <tt>rule y \@exists x id:3 abortIf:OpTrue</tt>
 * - <tt>rule z \@is_int x id:4 abortIf:OpFalse</tt>
 * - <tt>rule n \@is_float x id:5 abortIf:OpFalse</tt>
 * - <tt>rule s \@is_string x id:6 abortIf:OpFalse</tt>
 * - <tt>rule s \@is_sbuffer x id:5 abortIf:OpTrue</tt>
 *
 * @author Nick LeRoy <nleroy@qualys.com>
 */

#include <ironbee/action.h>
#include <ironbee/bytestr.h>
#include <ironbee/capture.h>
#include <ironbee/context.h>
#include <ironbee/engine.h>
#include <ironbee/field.h>
#include <ironbee/list.h>
#include <ironbee/module.h>
#include <ironbee/mm.h>
#include <ironbee/operator.h>
#include <ironbee/rule_engine.h>
#include <ironbee/string.h>

#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

/* Define the module name as well as a string version of it. */
#define MODULE_NAME        testops
#define MODULE_NAME_STR    IB_XSTRINGIFY(MODULE_NAME)

/* Declare the public module symbol. */
IB_MODULE_DECLARE();

/** is_type operator data */
struct  istype_op_t {
    const char        *name;      /**< Operator name. */
    ib_ftype_t         type;      /**< The matching IB field type. */
};
typedef struct istype_op_t istype_op_t;

/** IsType operators data */
static const istype_op_t istype_ops[] = {
    { "is_int",     IB_FTYPE_NUM },
    { "is_float",   IB_FTYPE_FLOAT },
    { "is_time",    IB_FTYPE_TIME },
    { "is_string",  IB_FTYPE_BYTESTR },
    { "is_sbuffer", IB_FTYPE_SBUFFER },
    { NULL,         IB_FTYPE_GENERIC },
};

/**
 * Execute function for the "True" operator
 *
 * @param[in] tx Current transaction.
 * @param[in] instance_data Instance data needed for execution.
 * @param[in] field The field to operate on.
 * @param[in] capture If non-NULL, the collection to capture to.
 * @param[out] result The result of the operator 1=true 0=false.
 * @param[in] cbdata Callback data (unused).
 *
 * @returns Status code
 */
static ib_status_t op_true_execute(
    ib_tx_t          *tx,
    void             *instance_data,
    const ib_field_t *field,
    ib_field_t       *capture,
    ib_num_t         *result,
    void             *cbdata
)
{
    assert(tx != NULL);
    assert(result != NULL);

    /* Always return true. */
    *result = 1;

    /* Set the capture. */
    if ((capture != NULL) && *result) {
        ib_capture_clear(capture);
        ib_capture_set_item(capture, 0, tx->mm, field);
    }
    return IB_OK;
}

/**
 * Execute function for the "False" operator
 *
 * @param[in] tx Current transaction.
 * @param[in] instance_data Instance data needed for execution.
 * @param[in] field The field to operate on.
 * @param[in] capture If non-NULL, the collection to capture to.
 * @param[out] result The result of the operator 1=true 0=false.
 * @param[in] cbdata Callback data (unused).
 *
 * @returns Status code
 */
static ib_status_t op_false_execute(
    ib_tx_t          *tx,
    void             *instance_data,
    const ib_field_t *field,
    ib_field_t       *capture,
    ib_num_t         *result,
    void             *cbdata
)
{
    *result = 0;
    /* Don't check for capture, because we always return zero. */

    return IB_OK;
}

/**
 * Execute function for the "Exists" operator
 *
 * @param[in] tx Current transaction.
 * @param[in] instance_data Instance data needed for execution.
 * @param[in] field The field to operate on.
 * @param[in] capture If non-NULL, the collection to capture to.
 * @param[out] result The result of the operator 1=true 0=false.
 * @param[in] cbdata Callback data (unused).
 *
 * @returns Status code
 */
static ib_status_t op_exists_execute(
    ib_tx_t          *tx,
    void             *instance_data,
    const ib_field_t *field,
    ib_field_t       *capture,
    ib_num_t         *result,
    void             *cbdata
)
{
    assert(tx != NULL);
    assert(result != NULL);

    /* Return true of field is not NULL. */
    *result = (field != NULL);

    /* Set the capture. */
    if ((capture != NULL) && *result) {
        ib_capture_clear(capture);
        ib_capture_set_item(capture, 0, tx->mm, field);
    }

    return IB_OK;
}

/**
 * Execute function for the "IsType" operator family
 *
 * @param[in] tx Current transaction.
 * @param[in] instance_data Instance data needed for execution.
 * @param[in] field The field to operate on.
 * @param[in] capture If non-NULL, the collection to capture to.
 * @param[out] result The result of the operator 1=true 0=false.
 * @param[in] cbdata Callback data (@ref istype_op_t).
 *
 * @returns Status code
 */
static ib_status_t op_istype_execute(
    ib_tx_t          *tx,
    void             *instance_data,
    const ib_field_t *field,
    ib_field_t       *capture,
    ib_num_t         *result,
    void             *cbdata
)
{
    assert(tx != NULL);
    assert(cbdata != NULL);

    const istype_op_t *op = cbdata;

    /* Return true if the field type matches the parameter type. */
    *result = ((field != NULL) && (op->type == field->type));

    if (*result == 0) {
        cbdata = NULL;
    }

    /* Set the capture. */
    if ((capture != NULL) && *result) {
        ib_capture_clear(capture);
        ib_capture_set_item(capture, 0, tx->mm, field);
    }

    return IB_OK;
}

/**
 * Initialize the testops module.
 *
 * @param[in] ib IronBee Engine.
 * @param[in] module Module data.
 * @param[in] cbdata Callback data (unused).
 *
 * @returns Status code
 */
static ib_status_t testops_init(
    ib_engine_t *ib,
    ib_module_t *module,
    void        *cbdata
)
{
    assert(ib != NULL);
    assert(module != NULL);

    ib_status_t        rc;
    const istype_op_t *istype_op;

    /*
     * Register the true / false / exists operators.
     */

    /* Register the true operator. */
    rc = ib_operator_create_and_register(
        NULL,
        ib,
        "true",
        ( IB_OP_CAPABILITY_ALLOW_NULL | IB_OP_CAPABILITY_CAPTURE ),
        NULL, NULL, /* No create function */
        NULL, NULL, /* no destroy function */
        op_true_execute, NULL);
    if (rc != IB_OK) {
        return rc;
    }

    /* Register the false operator. */
    rc = ib_operator_create_and_register(
        NULL,
        ib,
        "false",
        ( IB_OP_CAPABILITY_ALLOW_NULL ),
        NULL, NULL, /* No create function */
        NULL, NULL, /* no destroy function */
        op_false_execute, NULL);
    if (rc != IB_OK) {
        return rc;
    }

    /* Register the field exists operator. */
    rc = ib_operator_create_and_register(
        NULL,
        ib,
        "exists",
        ( IB_OP_CAPABILITY_ALLOW_NULL | IB_OP_CAPABILITY_CAPTURE ),
        NULL, NULL, /* No create function */
        NULL, NULL, /* no destroy function */
        op_exists_execute, NULL
    );
    if (rc != IB_OK) {
        return rc;
    }

    /*
     * Register the is_xxx operators.
     */
    for (istype_op = istype_ops;  istype_op->name != NULL; ++istype_op) {
        rc = ib_operator_create_and_register(
            NULL,
            ib,
            istype_op->name,
            ( IB_OP_CAPABILITY_ALLOW_NULL | IB_OP_CAPABILITY_CAPTURE ),
            NULL, NULL, /* no create function */
            NULL, NULL, /* no destroy function */
            op_istype_execute, (void *)istype_op
            );
        if (rc != IB_OK) {
            return rc;
        }
    }

    return IB_OK;
}

/*
 * Module structure.
 *
 * This structure defines some metadata, config data and various functions.
 */
IB_MODULE_INIT(
    IB_MODULE_HEADER_DEFAULTS,               /* Default metadata */
    MODULE_NAME_STR,                         /* Module name */
    IB_MODULE_CONFIG_NULL,                   /* Global config data */
    NULL,                                    /* Module config map */
    NULL,                                    /* Module directive map */
    testops_init,                            /* Initialize function */
    NULL,                                    /* Callback data */
    NULL,                                    /* Finish function */
    NULL,                                    /* Callback data */
);
