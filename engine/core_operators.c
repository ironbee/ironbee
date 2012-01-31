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

/**
 * @file
 * @brief IronBee
 *
 * @author Nick LeRoy <nleroy@qualys.com>
 */

#include "ironbee_config_auto.h"

#include <ironbee/engine.h>
#include <ironbee/mpool.h>
#include <ironbee/bytestr.h>
#include <ironbee/debug.h>
#include <ironbee/rule_engine.h>
#include <ironbee/operator.h>

#include "ironbee_private.h"
#include "ironbee_core_private.h"


/**
 * @internal
 * Create function for the "@str" operators
 *
 * @param[in,out] mp Memory pool to use for allocation
 * @param[in] parameters Constant parameters
 * @param[in,out] op_inst Instance operator
 *
 * @returns Status code
 */
static ib_status_t strop_create(ib_mpool_t *mp,
                                const char *parameters,
                                ib_operator_inst_t *op_inst)
{
    IB_FTRACE_INIT(strop_create);
    char *str;

    if (parameters == NULL) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    str = ib_mpool_strdup(mp, parameters);
    if (str == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    op_inst->data = str;
    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * @internal
 * Execute function for the "@streq" operator
 *
 * @param[in] ib Ironbee engine.
 * @param[in] tx The transaction for this operator.
 * @param[in] data C-style string to compare to
 * @param[in] field Field value
 * @param[out] result Pointer to number in which to store the result
 *
 * @returns Status code
 */
static ib_status_t op_streq_execute(ib_engine_t *ib,
                                    ib_tx_t *tx,
                                    void *data,
                                    ib_field_t *field,
                                    ib_num_t *result)
{
    IB_FTRACE_INIT(op_streq_execute);

    /* This works on C-style (NUL terminated) and byte strings */
    const char *cstr = (const char *)data;
    if (field->type==IB_FTYPE_NULSTR) {
        const char *fval = ib_field_value_nulstr( field );
        *result = (strcmp(fval,cstr) == 0);
    }
    else if (field->type==IB_FTYPE_BYTESTR) {
        ib_bytestr_t *value = ib_field_value_bytestr(field);
        size_t        len = ib_bytestr_length(value);

        if (len == strlen(cstr)) {
            *result = (memcmp(ib_bytestr_ptr(value), cstr, len) == 0);
        }
        else {
            *result = 0;
        }
    }
    else {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * @internal
 * Execute function for the "@contains" operator
 *
 * @param[in] ib Ironbee engine.
 * @param[in] tx The transaction for this operator.
 * @param[in] data C-style string to compare to
 * @param[in] field Field value
 * @param[out] result Pointer to number in which to store the result
 *
 * @returns Status code
 */
static ib_status_t contains_execute_fn(ib_engine_t *ib,
                                       ib_tx_t *tx,
                                       void *data,
                                       ib_field_t *field,
                                       ib_num_t *result)
{
    IB_FTRACE_INIT(contains_execute_fn);
    char *searchstr = (char *)data;
    ib_status_t rc = IB_OK;

    if (field->type == IB_FTYPE_NULSTR) {
        if (strstr(ib_field_value_nulstr(field), searchstr) == NULL) {
            *result = 0;
        }
        else {
            *result = 1;
        }
    }
    else if (field->type == IB_FTYPE_BYTESTR) {
        ib_bytestr_t *str = ib_field_value_bytestr(field);
        if (ib_bytestr_index_of_c(str, searchstr) == -1) {
            *result = 0;
        }
        else {
            *result = 1;
        }
    }
    else {
        return IB_EINVAL;
    }

    IB_FTRACE_RET_STATUS(rc);
}

/**
 * @internal
 * Execute function for the "@exists" operator
 *
 * @param[in] ib Ironbee engine.
 * @param[in] tx The transaction for this operator.
 * @param[in] data C-style string to compare to (ignored)
 * @param[in] field Field value
 * @param[out] result Pointer to number in which to store the result
 *
 * @returns Status code
 */
static ib_status_t op_exists_execute(ib_engine_t *ib,
                                     ib_tx_t *tx,
                                     void *data,
                                     ib_field_t *field,
                                     ib_num_t *result)
{
    IB_FTRACE_INIT(op_exists_execute);
    /* Return true of field is not NULL */
    *result = (field != NULL);
    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * @internal
 * Execute function for the "@checkflag" operator
 *
 * @param[in] ib Ironbee engine.
 * @param[in] tx The transaction for this operator.
 * @param[in] data C-style string to compare to
 * @param[in] field Field value
 * @param[out] result Pointer to number in which to store the result
 *
 * @returns Status code
 */
static ib_status_t op_checkflag_execute(ib_engine_t *ib,
                                        ib_tx_t *tx,
                                        void *data,
                                        ib_field_t *field,
                                        ib_num_t *result)
{
    IB_FTRACE_INIT(op_checkflag_execute);

    /* Data will be a C-Style string */
    const char *cstr = (const char *)data;

    /* Handle the suspicous flag */
    if (strcasecmp(cstr, "suspicious") == 0) {
        *result = ib_tx_flags_isset(tx, IB_TX_FSUSPICIOUS);
    }
    else {
        ib_log_error(tx->ib, 4, "checkflag operator: invalid flag '%s'", cstr);
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }
    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * @internal
 * Execute function for the "@true" operator
 *
 * @param[in] ib Ironbee engine.
 * @param[in] tx The transaction for this operator.
 * @param[in] data C-style string to compare to
 * @param[in] field Field value
 * @param[out] result Pointer to number in which to store the result
 *
 * @returns Status code
 */
static ib_status_t op_true_execute(ib_engine_t *ib,
                                   ib_tx_t *tx,
                                   void *data,
                                   ib_field_t *field,
                                   ib_num_t *result)
{
    IB_FTRACE_INIT(op_true_execute);
    *result = 1;
    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * @internal
 * Execute function for the "@false" operator
 *
 * @param[in] ib Ironbee engine.
 * @param[in] tx The transaction for this operator.
 * @param[in] data C-style string to compare to
 * @param[in] field Field value
 * @param[out] result Pointer to number in which to store the result
 *
 * @returns Status code
 */
static ib_status_t op_false_execute(ib_engine_t *ib,
                                    ib_tx_t *tx,
                                    void *data,
                                    ib_field_t *field,
                                    ib_num_t *result)
{
    IB_FTRACE_INIT(op_false_execute);
    *result = 0;
    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * Initialize the core operators
 */
ib_status_t ib_core_operators_init(ib_engine_t *ib, ib_module_t *mod)
{
    IB_FTRACE_INIT(ib_core_operators_init);
    ib_status_t rc;

    /* Register the string equal '@streq' operator */
    rc = ib_operator_register(ib,
                              "@streq",
                              IB_OP_FLAG_NONE,
                              strop_create,
                              NULL, /* no destroy function */
                              op_streq_execute);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Register the string contains '@contains' operator */
    rc = ib_operator_register(ib,
                              "@contains",
                              IB_OP_FLAG_NONE,
                              strop_create,
                              NULL, /* no destroy function */
                              contains_execute_fn);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Register the string contains '@checkflag' operator */
    rc = ib_operator_register(ib,
                              "@checkflag",
                              IB_OP_FLAG_ALLOW_NULL,
                              strop_create,
                              NULL, /* no destroy function */
                              op_checkflag_execute);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Register the field exists '@exists' operator */
    rc = ib_operator_register(ib,
                              "@exists",
                              IB_OP_FLAG_ALLOW_NULL,
                              NULL, /* No create function */
                              NULL, /* no destroy function */
                              op_exists_execute);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Register the true '@true' operator */
    rc = ib_operator_register(ib,
                              "@true",
                              IB_OP_FLAG_ALLOW_NULL,
                              NULL, /* No create function */
                              NULL, /* no destroy function */
                              op_true_execute);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Register the false '@false' operator */
    rc = ib_operator_register(ib,
                              "@false",
                              IB_OP_FLAG_ALLOW_NULL,
                              NULL, /* No create function */
                              NULL, /* no destroy function */
                              op_false_execute);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}
