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

#include <assert.h>

#include <ironbee/engine.h>
#include <ironbee/mpool.h>
#include <ironbee/bytestr.h>
#include <ironbee/radix.h>
#include <ironbee/debug.h>
#include <ironbee/rule_engine.h>
#include <ironbee/operator.h>

#include "ironbee_private.h"
#include "ironbee_core_private.h"


/**
 * @internal
 * Create function for the "str" family of operators
 *
 * @param[in] ib The IronBee engine (unused)
 * @param[in] ctx The current IronBee context (unused)
 * @param[in,out] mp Memory pool to use for allocation
 * @param[in] parameters Constant parameters
 * @param[in,out] op_inst Instance operator
 *
 * @returns Status code
 */
static ib_status_t strop_create(ib_engine_t *ib,
                                ib_context_t *ctx,
                                ib_mpool_t *mp,
                                const char *parameters,
                                ib_operator_inst_t *op_inst)
{
    IB_FTRACE_INIT();
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
 * Execute function for the "streq" operator
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
    IB_FTRACE_INIT();

    /**
     * This works on C-style (NUL terminated) and byte strings.  Note
     * that data is assumed to be a NUL terminated string (because our
     * configuration parser can't produce anything else).
     **/
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
 * Execute function for the "contains" operator
 *
 * @param[in] ib Ironbee engine (unused).
 * @param[in] tx The transaction for this operator (unused).
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
    IB_FTRACE_INIT();
    /* 'searchstr' should be const, but the bytestr index fn takes a char* */
    char *searchstr = (char *)data;
    ib_status_t rc = IB_OK;

    /**
     * This works on C-style (NUL terminated) and byte strings.  Note
     * that data is assumed to be a NUL terminated string (because our
     * configuration parser can't produce anything else).
     **/
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
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    IB_FTRACE_RET_STATUS(rc);
}

/**
 * @internal
 * Execute function for the "exists" operator
 *
 * @param[in] ib Ironbee engine (unused).
 * @param[in] tx The transaction for this operator (unused).
 * @param[in] data Operator data (unused)
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
    IB_FTRACE_INIT();

    /* Return true of field is not NULL */
    *result = (field != NULL);
    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * @internal
 * Execute function for the "checkflag" operator
 *
 * @param[in] ib Ironbee engine (unused).
 * @param[in] tx The transaction for this operator.
 * @param[in] data Name of the flag to check.
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
    IB_FTRACE_INIT();

    /* Data will be a C-Style string */
    const char *cstr = (const char *)data;

    /* Handle the suspicious flag */
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
 * Execute function for the "true" operator
 *
 * @param[in] ib Ironbee engine (unused)
 * @param[in] tx The transaction for this operator (unused)
 * @param[in] data Operator data (unused)
 * @param[in] field Field value (unused)
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
    IB_FTRACE_INIT();
    *result = 1;
    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * @internal
 * Execute function for the "false" operator
 *
 * @param[in] ib Ironbee engine (unused)
 * @param[in] tx The transaction for this operator (unused)
 * @param[in] data Operator data (unused)
 * @param[in] field Field value (unused)
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
    IB_FTRACE_INIT();
    *result = 0;
    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * @internal
 * Create function for the "ipmatch" operator
 *
 * @param[in] ib The IronBee engine
 * @param[in] ctx The current IronBee context (unused)
 * @param[in,out] mp Memory pool to use for allocation
 * @param[in] parameters Constant parameters (ip address strings)
 * @param[in,out] op_inst Instance operator
 *
 * @returns Status code
 */
static ib_status_t op_ipmatch_create(ib_engine_t *ib,
                                     ib_context_t *ctx,
                                     ib_mpool_t *mp,
                                     const char *parameters,
                                     ib_operator_inst_t *op_inst)
{
    IB_FTRACE_INIT();
    ib_status_t rc;
    char *copy;
    char *p;
    ib_radix_t *radix;

    if (parameters == NULL) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    /* Make a copy of the parameters to operate on */
    copy = ib_mpool_strdup(mp, parameters);
    if (copy == NULL) {
        ib_log_error(ib, 4, "Error coping rule parameters '%s'", parameters);
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    /* Create the radix matcher */
    rc = ib_radix_new(&radix, NULL, NULL, NULL, mp);
    if (rc != IB_OK) {
        ib_log_error(ib, 4, "Failed to allocate a radix matcher: %d", rc);
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Split the parameters into the separate pieces */
    for (p = strtok(copy, " ");  p != NULL;  p = strtok(NULL, " ") ) {
        ib_radix_prefix_t *prefix = NULL;

        /* Convert the IP address string to a prefix object */
        rc = ib_radix_ip_to_prefix(p, &prefix, mp);
        if (rc != IB_OK) {
            ib_log_error(ib, 4,
                         "Error created radix prefix for %s: %d", p, rc);
            IB_FTRACE_RET_STATUS(rc);
        }

        /* Insert the prefix into the radix tree */
        rc = ib_radix_insert_data(radix, prefix, NULL);
        if (rc != IB_OK) {
            ib_log_error(ib, 4,
                         "Error loading prefix %s to the radix tree: %d",
                         p, rc);
            IB_FTRACE_RET_STATUS(rc);
        }

        ib_log_debug(ib, 4, "prefix '%s' added to the radix tree", p);
    }

    /* Done */
    op_inst->data = radix;
    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * @internal
 * Execute function for the "ipmatch" operator
 *
 * @param[in] ib Ironbee engine.
 * @param[in] tx The transaction for this operator.
 * @param[in] data C-style string to compare to
 * @param[in] field Field value
 * @param[out] result Pointer to number in which to store the result
 *
 * @returns Status code
 */
static ib_status_t op_ipmatch_execute(ib_engine_t *ib,
                                      ib_tx_t *tx,
                                      void *data,
                                      ib_field_t *field,
                                      ib_num_t *result)
{
    IB_FTRACE_INIT();
    ib_status_t rc;
    ib_radix_t *radix = (ib_radix_t *)data; /* The radix matcher object */
    ib_radix_prefix_t *prefix;              /* The IP address */
    const char *ipstr;                      /* String version of the address */
    ib_num_t iplen;                         /* Length of the address string */
    char *rmatch = NULL;                    /* Radix match */

    /**
     * This works on C-style (NUL terminated) and byte strings.  Note
     * that data is assumed to be a NUL terminated string (because our
     * configuration parser can't produce anything else).
     **/
    if (field->type==IB_FTYPE_NULSTR) {
        ipstr = ib_field_value_nulstr( field );

        /* Verify that we got out a string */
        if (ipstr == NULL) {
            ib_log_error(ib, 4, "Failed to get NULSTR from field");
            IB_FTRACE_RET_STATUS(IB_EUNKNOWN);
        }
        iplen = strlen(ipstr);
    }
    else if (field->type==IB_FTYPE_BYTESTR) {
        ib_bytestr_t *bs = ib_field_value_bytestr(field);

        /* Verify that we got out a bytestr */
        assert(bs != NULL);

        /* Get the bytestr's length and pointer */
        iplen = ib_bytestr_length(bs);
        ipstr = (char *)ib_bytestr_ptr(bs);
    }
    else {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    /* Convert the IP address string to a prefix object */
    rc = ib_radix_ip_to_prefix_ex(ipstr, iplen, &prefix, tx->mp);
    if (rc != IB_OK) {
        ib_log_error(ib, 4,
                     "Error created radix prefix for %*s: %d",
                     iplen, ipstr, rc);
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Do the matching */
    rc = ib_radix_match_closest(radix, prefix, &rmatch);
    if (rc == IB_ENOENT) {
        *result = 0;
    }
    else if (rc == IB_OK) {
        *result = 1;
    }
    else {
        ib_log_error(ib, 4,
                     "Radix matcher failed matching for %*s: %d",
                     iplen, ipstr, rc);
        IB_FTRACE_RET_STATUS(rc);
    }
    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * Initialize the core operators
 **/
ib_status_t ib_core_operators_init(ib_engine_t *ib, ib_module_t *mod)
{
    IB_FTRACE_INIT();
    ib_status_t rc;

    /* Register the string equal operator */
    rc = ib_operator_register(ib,
                              "streq",
                              IB_OP_FLAG_NONE,
                              strop_create,
                              NULL, /* no destroy function */
                              op_streq_execute);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Register the string contains operator */
    rc = ib_operator_register(ib,
                              "contains",
                              IB_OP_FLAG_NONE,
                              strop_create,
                              NULL, /* no destroy function */
                              contains_execute_fn);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Register the checkflag operator */
    rc = ib_operator_register(ib,
                              "checkflag",
                              IB_OP_FLAG_ALLOW_NULL,
                              strop_create,
                              NULL, /* no destroy function */
                              op_checkflag_execute);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Register the field exists operator */
    rc = ib_operator_register(ib,
                              "exists",
                              IB_OP_FLAG_ALLOW_NULL,
                              NULL, /* No create function */
                              NULL, /* no destroy function */
                              op_exists_execute);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Register the true operator */
    rc = ib_operator_register(ib,
                              "true",
                              IB_OP_FLAG_ALLOW_NULL,
                              NULL, /* No create function */
                              NULL, /* no destroy function */
                              op_true_execute);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Register the false operator */
    rc = ib_operator_register(ib,
                              "false",
                              IB_OP_FLAG_ALLOW_NULL,
                              NULL, /* No create function */
                              NULL, /* no destroy function */
                              op_false_execute);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Register the ipmatch operator */
    rc = ib_operator_register(ib,
                              "ipmatch",
                              IB_OP_FLAG_NONE,
                              op_ipmatch_create,
                              NULL, /* no destroy function */
                              op_ipmatch_execute);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}
