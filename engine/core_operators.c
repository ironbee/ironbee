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
#include <ctype.h>

#include <ironbee/engine.h>
#include <ironbee/mpool.h>
#include <ironbee/bytestr.h>
#include <ironbee/radix.h>
#include <ironbee/debug.h>
#include <ironbee/rule_engine.h>
#include <ironbee/operator.h>
#include <ironbee/field.h>
#include <ironbee/string.h>

#include "ironbee_private.h"


/* Size of buffer used to convert a bytestr to an int */
#define MAX_FIELD_NUM_BUF    128

/* Numeric operator params */
typedef struct {
    const char *str;
    ib_num_t    num;
} numop_params_t;

/**
 * Allocate a buffer and unescape operator arguments.
 * @param[in] ib IronBee engine used for logging.
 * @param[in] mp Memory pool that @a str_unesc will be allocated out of.
 * @param[in] str The parameter string to be unescaped.
 * @param[out] str_unesc On a successful unescaping, a new buffer allocated
 *             out of @a mp will be assigned to @a *str_unesc with the
 *             unescaped string in it.
 * @param[out] str_unesc_len On success *str_unesc_len is assigned the length
 *             of the unescaped string. Note that this may be different
 *             that strlen(*str_unesc) because \x00 will place a NULL
 *             in the middle of @a *str_unesc.
 *             This string should be wrapped in an ib_bytestr_t.
 * @returns IB_OK on success. IB_EALLOC on error. IB_EINVAL if @a str
 *          was unable to be unescaped.
 */
static ib_status_t unescape_op_args(ib_engine_t *ib,
                                    ib_mpool_t *mp,
                                    char **str_unesc,
                                    size_t *str_unesc_len,
                                    const char *str)
{
    IB_FTRACE_INIT();

    assert(mp!=NULL);
    assert(ib!=NULL);
    assert(str!=NULL);
    assert(str_unesc!=NULL);
    assert(str_unesc_len!=NULL);

    ib_status_t rc;
    const size_t str_len = strlen(str);

    /* Temporary unescaped string holder. */
    char* tmp_unesc = ib_mpool_alloc(mp, str_len+1);
    size_t tmp_unesc_len;

    if ( tmp_unesc == NULL ) {
        ib_log_debug(ib, 3, "Failed to allocate unescape string buffer.");
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    rc = ib_util_unescape_string(tmp_unesc, &tmp_unesc_len, str, str_len, 0);

    if ( rc != IB_OK ) {
        ib_log_debug(ib, 3, "Failed to unescape string: %s", str);
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Commit changes on success. */
    *str_unesc = tmp_unesc;
    *str_unesc_len = tmp_unesc_len;

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * Create function for the "str" family of operators
 * @internal
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
    ib_status_t rc;
    ib_bool_t expand;
    char *str;
    size_t str_len;

    if (parameters == NULL) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    rc = unescape_op_args(ib, mp, &str, &str_len, parameters);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = ib_data_expand_test_str(str, &expand);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }
    if (expand) {
        op_inst->flags |= IB_OPINST_FLAG_EXPAND;
    }

    op_inst->data = str;
    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * Execute function for the "streq" operator
 * @internal
 *
 * @param[in] ib Ironbee engine.
 * @param[in] tx The transaction for this operator.
 * @param[in] data C-style string to compare to
 * @param[in] flags Operator instance flags
 * @param[in] field Field value
 * @param[out] result Pointer to number in which to store the result
 *
 * @returns Status code
 */
static ib_status_t op_streq_execute(ib_engine_t *ib,
                                    ib_tx_t *tx,
                                    void *data,
                                    ib_flags_t flags,
                                    ib_field_t *field,
                                    ib_num_t *result)
{
    IB_FTRACE_INIT();

    /**
     * This works on C-style (NUL terminated) and byte strings.  Note
     * that data is assumed to be a NUL terminated string (because our
     * configuration parser can't produce anything else).
     **/
    ib_status_t  rc;
    const char  *cstr = (const char *)data;
    char        *expanded;

    /* Expand the string */
    if ( (tx != NULL) && ( (flags & IB_OPINST_FLAG_EXPAND) != 0) ) {
        rc = ib_data_expand_str(tx->dpi, cstr, &expanded);
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }
    }
    else {
        expanded = (char *)cstr;
    }

    /* Handle NUL-terminated strings and byte strings */
    if (field->type == IB_FTYPE_NULSTR) {
        const char *fval;
        rc = ib_field_value(field, ib_ftype_nulstr_out(&fval));
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }

        *result = (strcmp(fval,expanded) == 0);
    }
    else if (field->type == IB_FTYPE_BYTESTR) {
        const ib_bytestr_t *value;
        size_t                len;

        rc = ib_field_value(field, ib_ftype_bytestr_out(&value));
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }

        len = ib_bytestr_length(value);

        if (len == strlen(expanded)) {
            *result = (memcmp(ib_bytestr_const_ptr(value), expanded, len) == 0);
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
 * Execute function for the "contains" operator
 * @internal
 *
 * @param[in] ib Ironbee engine (unused).
 * @param[in] tx The transaction for this operator (unused).
 * @param[in] data C-style string to compare to
 * @param[in] flags Operator instance flags
 * @param[in] field Field value
 * @param[out] result Pointer to number in which to store the result
 *
 * @returns Status code
 */
static ib_status_t op_contains_execute(ib_engine_t *ib,
                                       ib_tx_t *tx,
                                       void *data,
                                       ib_flags_t flags,
                                       ib_field_t *field,
                                       ib_num_t *result)
{
    IB_FTRACE_INIT();
    ib_status_t  rc = IB_OK;
    const char  *cstr = (char *)data;
    char        *expanded;

    /* Expand the string */
    if ( (tx != NULL) && ( (flags & IB_OPINST_FLAG_EXPAND) != 0) ) {
        rc = ib_data_expand_str(tx->dpi, cstr, &expanded);
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }
    }
    else {
        expanded = (char *)cstr;
    }

    /**
     * This works on C-style (NUL terminated) and byte strings.  Note
     * that data is assumed to be a NUL terminated string (because our
     * configuration parser can't produce anything else).
     **/
    if (field->type == IB_FTYPE_NULSTR) {
        const char *s;
        rc = ib_field_value(field, ib_ftype_nulstr_out(&s));
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }

        if (strstr(s, expanded) == NULL) {
            *result = 0;
        }
        else {
            *result = 1;
        }
    }
    else if (field->type == IB_FTYPE_BYTESTR) {
        const ib_bytestr_t *str;
        rc = ib_field_value(field, ib_ftype_bytestr_out(&str));
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }

        if (ib_bytestr_index_of_c(str, expanded) == -1) {
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
 * Execute function for the "exists" operator
 * @internal
 *
 * @param[in] ib Ironbee engine (unused).
 * @param[in] tx The transaction for this operator (unused).
 * @param[in] data Operator data (unused)
 * @param[in] flags Operator instance flags
 * @param[in] field Field value
 * @param[out] result Pointer to number in which to store the result
 *
 * @returns Status code
 */
static ib_status_t op_exists_execute(ib_engine_t *ib,
                                     ib_tx_t *tx,
                                     void *data,
                                     ib_flags_t flags,
                                     ib_field_t *field,
                                     ib_num_t *result)
{
    IB_FTRACE_INIT();

    /* Return true of field is not NULL */
    *result = (field != NULL);
    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * Execute function for the "checkflag" operator
 * @internal
 *
 * @param[in] ib Ironbee engine (unused).
 * @param[in] tx The transaction for this operator.
 * @param[in] data Name of the flag to check.
 * @param[in] flags Operator instance flags
 * @param[in] field Field value
 * @param[out] result Pointer to number in which to store the result
 *
 * @returns Status code
 */
static ib_status_t op_checkflag_execute(ib_engine_t *ib,
                                        ib_tx_t *tx,
                                        void *data,
                                        ib_flags_t flags,
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
 * Execute function for the "true" operator
 * @internal
 *
 * @param[in] ib Ironbee engine (unused)
 * @param[in] tx The transaction for this operator (unused)
 * @param[in] data Operator data (unused)
 * @param[in] flags Operator instance flags
 * @param[in] field Field value (unused)
 * @param[out] result Pointer to number in which to store the result
 *
 * @returns Status code
 */
static ib_status_t op_true_execute(ib_engine_t *ib,
                                   ib_tx_t *tx,
                                   void *data,
                                   ib_flags_t flags,
                                   ib_field_t *field,
                                   ib_num_t *result)
{
    IB_FTRACE_INIT();
    *result = 1;
    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * Execute function for the "false" operator
 * @internal
 *
 * @param[in] ib Ironbee engine (unused)
 * @param[in] tx The transaction for this operator (unused)
 * @param[in] data Operator data (unused)
 * @param[in] flags Operator instance flags
 * @param[in] field Field value (unused)
 * @param[out] result Pointer to number in which to store the result
 *
 * @returns Status code
 */
static ib_status_t op_false_execute(ib_engine_t *ib,
                                    ib_tx_t *tx,
                                    void *data,
                                    ib_flags_t flags,
                                    ib_field_t *field,
                                    ib_num_t *result)
{
    IB_FTRACE_INIT();
    *result = 0;
    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * Create function for the "ipmatch" operator
 * @internal
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
    size_t copy_len;
    char *p;
    ib_radix_t *radix;

    if (parameters == NULL) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    /* Make a copy of the parameters to operate on */
    rc = unescape_op_args(ib, mp, &copy, &copy_len, parameters);
    if (rc != IB_OK) {
        ib_log_error(ib, 4,
                     "Error unescaping rule parameters '%s'", parameters);
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

        ib_log_debug(ib, 9, "prefix '%s' added to the radix tree", p);
    }

    /* Done */
    op_inst->data = radix;
    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * Execute function for the "ipmatch" operator
 * @internal
 *
 * @param[in] ib Ironbee engine.
 * @param[in] tx The transaction for this operator.
 * @param[in] data C-style string to compare to
 * @param[in] flags Operator instance flags
 * @param[in] field Field value
 * @param[out] result Pointer to number in which to store the result
 *
 * @returns Status code
 */
static ib_status_t op_ipmatch_execute(ib_engine_t *ib,
                                      ib_tx_t *tx,
                                      void *data,
                                      ib_flags_t flags,
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
        rc = ib_field_value(field, ib_ftype_nulstr_out(&ipstr));
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }

        /* Verify that we got out a string */
        if (ipstr == NULL) {
            ib_log_error(ib, 4, "Failed to get NULSTR from field");
            IB_FTRACE_RET_STATUS(IB_EUNKNOWN);
        }
        iplen = strlen(ipstr);
    }
    else if (field->type==IB_FTYPE_BYTESTR) {
        const ib_bytestr_t *bs;
        rc = ib_field_value(field, ib_ftype_bytestr_out(&bs));
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }

        /* Verify that we got out a bytestr */
        assert(bs != NULL);

        /* Get the bytestr's length and pointer */
        iplen = ib_bytestr_length(bs);
        ipstr = (const char*)ib_bytestr_const_ptr(bs);
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
 * Create function for the numeric comparison operators
 * @internal
 *
 * @param[in] ib The IronBee engine (unused)
 * @param[in] ctx The current IronBee context (unused)
 * @param[in,out] mp Memory pool to use for allocation
 * @param[in] params Constant parameters
 * @param[in,out] op_inst Instance operator
 *
 * @returns Status code
 */
static ib_status_t op_numcmp_create(ib_engine_t *ib,
                                    ib_context_t *ctx,
                                    ib_mpool_t *mp,
                                    const char *params,
                                    ib_operator_inst_t *op_inst)
{
    IB_FTRACE_INIT();
    numop_params_t *vptr;
    ib_status_t rc;
    ib_bool_t expandable;
    ib_num_t value;

    char *params_unesc;
    size_t params_unesc_len;

    rc = unescape_op_args(ib, mp, &params_unesc, &params_unesc_len, params);
    if (rc != IB_OK) {
        ib_log_debug(ib, 3, "Unable to unescape parameter: %s", params);
        IB_FTRACE_RET_STATUS(rc);
    }

    if (params_unesc == NULL) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    /* Is the string expandable? */
    rc = ib_data_expand_test_str(params_unesc, &expandable);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }
    if (expandable) {
        op_inst->flags |= IB_OPINST_FLAG_EXPAND;
    }
    else {
        rc = ib_string_to_num_ex(params_unesc, params_unesc_len, 0, &value);
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }
    }

    /* Allocate storage for the value */
    vptr = (numop_params_t *)ib_mpool_alloc(mp, sizeof(*vptr));
    if (vptr == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    /* Fill in the parameters */
    if (expandable) {
        vptr->str = ib_mpool_strdup(mp, params_unesc);
        if (vptr->str == NULL) {
            IB_FTRACE_RET_STATUS(IB_EALLOC);
        }
    }
    else {
        vptr->num = value;
        vptr->str = NULL;
    }

    op_inst->data = vptr;
    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * Get expanded numeric value of a string
 * @internal
 *
 * @param[in] tx Transaction
 * @param[in] pdata Parameter data
 * @param[in] flags Operator instance flags
 * @param[out] result Pointer to number in which to store the result
 *
 * @returns Status code
 */
static ib_status_t get_num_value(ib_tx_t *tx,
                                 const numop_params_t *pdata,
                                 ib_flags_t flags,
                                 ib_num_t *result)
{
    IB_FTRACE_INIT();
    ib_num_t rc;
    char *expanded;

    /* Easy case: just return the number from the pdata structure */
    if ( (flags & IB_OPINST_FLAG_EXPAND) == 0) {
        *result = pdata->num;
        IB_FTRACE_RET_STATUS(IB_OK);
    }

    /* Expand the string */
    rc = ib_data_expand_str(tx->dpi, pdata->str, &expanded);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Convert string the expanded string to a number */
    IB_FTRACE_RET_STATUS(ib_string_to_num(expanded, 0, result) );
}

/**
 * Get integer representation of a field
 * @internal
 *
 * @param[in] ib Ironbee engine.
 * @param[in] field Field value
 * @param[out] result Pointer to number in which to store the result
 *
 * @returns Status code
 */
static ib_status_t field_to_num(ib_engine_t *ib,
                                ib_field_t *field,
                                ib_num_t *result)
{
    IB_FTRACE_INIT();
    ib_status_t rc;

    switch (field->type) {
        case IB_FTYPE_NUM:
            rc = ib_field_value(field, ib_ftype_num_out(result));
            if (rc != IB_OK) {
                IB_FTRACE_RET_STATUS(rc);
            }
            break;

        case IB_FTYPE_UNUM :
            {
                ib_unum_t n;
                rc = ib_field_value(field, ib_ftype_unum_out(&n));
                if (rc != IB_OK) {
                    IB_FTRACE_RET_STATUS(rc);
                }

                if (n > INT64_MAX) {
                    IB_FTRACE_RET_STATUS(IB_EINVAL);
                }

                *result = (ib_num_t)n;
                break;
            }
        case IB_FTYPE_NULSTR :
            {
                const char *fval;
                rc = ib_field_value(field, ib_ftype_nulstr_out(&fval));
                if (rc != IB_OK) {
                    IB_FTRACE_RET_STATUS(rc);
                }

                rc = ib_string_to_num(fval, 0, result);
                if (rc != IB_OK) {
                    IB_FTRACE_RET_STATUS(IB_EINVAL);
                }
            }
            break;

        case IB_FTYPE_BYTESTR:
            {
                const ib_bytestr_t *bs;
                rc = ib_field_value(field, ib_ftype_bytestr_out(&bs));
                if (rc != IB_OK) {
                    IB_FTRACE_RET_STATUS(rc);
                }

                rc = ib_string_to_num_ex(
                    (const char *)ib_bytestr_const_ptr(bs),
                    ib_bytestr_length(bs),
                    0,
                    result);
                if (rc != IB_OK) {
                    IB_FTRACE_RET_STATUS(IB_EINVAL);
                }
            }
            break;

        default:
            IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * Execute function for the numeric "equal" operator
 * @internal
 *
 * @param[in] ib Ironbee engine.
 * @param[in] tx The transaction for this operator.
 * @param[in] data Pointer to number to compare to
 * @param[in] flags Operator instance flags
 * @param[in] field Field value
 * @param[out] result Pointer to number in which to store the result
 *
 * @returns Status code
 */
static ib_status_t op_eq_execute(ib_engine_t *ib,
                                 ib_tx_t *tx,
                                 void *data,
                                 ib_flags_t flags,
                                 ib_field_t *field,
                                 ib_num_t *result)
{
    IB_FTRACE_INIT();
    const numop_params_t *pdata = (const numop_params_t *)data;
    ib_num_t              param_value;  /* Parameter value */
    ib_num_t              value;
    ib_status_t           rc;

    /* Get integer representation of the field */
    rc = field_to_num(ib, field, &value);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Get the numeric value (including expansion, etc) */
    rc = get_num_value(tx, pdata, flags, &param_value);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Do the comparison */
    *result = (value == param_value);
    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * Execute function for the numeric "not equal" operator
 * @internal
 *
 * @param[in] ib Ironbee engine.
 * @param[in] tx The transaction for this operator.
 * @param[in] data C-style string to compare to
 * @param[in] flags Operator instance flags
 * @param[in] field Field value
 * @param[out] result Pointer to number in which to store the result
 *
 * @returns Status code
 */
static ib_status_t op_ne_execute(ib_engine_t *ib,
                                 ib_tx_t *tx,
                                 void *data,
                                 ib_flags_t flags,
                                 ib_field_t *field,
                                 ib_num_t *result)
{
    IB_FTRACE_INIT();
    const numop_params_t *pdata = (const numop_params_t *)data;
    ib_num_t              param_value;  /* Parameter value */
    ib_num_t              value;
    ib_status_t           rc;

    /* Get integer representation of the field */
    rc = field_to_num(ib, field, &value);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Get the numeric value (including expansion, etc) */
    rc = get_num_value(tx, pdata, flags, &param_value);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Do the comparison */
    *result = (value != param_value);
    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * Execute function for the "gt" operator
 * @internal
 *
 * @param[in] ib Ironbee engine.
 * @param[in] tx The transaction for this operator.
 * @param[in] data Pointer to number to compare to
 * @param[in] flags Operator instance flags
 * @param[in] field Field value
 * @param[out] result Pointer to number in which to store the result
 *
 * @returns Status code
 */
static ib_status_t op_gt_execute(ib_engine_t *ib,
                                 ib_tx_t *tx,
                                 void *data,
                                 ib_flags_t flags,
                                 ib_field_t *field,
                                 ib_num_t *result)
{
    IB_FTRACE_INIT();
    const numop_params_t *pdata = (const numop_params_t *)data;
    ib_num_t              param_value;  /* Parameter value */
    ib_num_t              value;
    ib_status_t           rc;

    /* Get integer representation of the field */
    rc = field_to_num(ib, field, &value);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Get the numeric value (including expansion, etc) */
    rc = get_num_value(tx, pdata, flags, &param_value);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Do the comparison */
    *result = (value > param_value);
    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * Execute function for the numeric "less-than" operator
 * @internal
 *
 * @param[in] ib Ironbee engine.
 * @param[in] tx The transaction for this operator.
 * @param[in] data C-style string to compare to
 * @param[in] flags Operator instance flags
 * @param[in] field Field value
 * @param[out] result Pointer to number in which to store the result
 *
 * @returns Status code
 */
static ib_status_t op_lt_execute(ib_engine_t *ib,
                                 ib_tx_t *tx,
                                 void *data,
                                 ib_flags_t flags,
                                 ib_field_t *field,
                                 ib_num_t *result)
{
    IB_FTRACE_INIT();
    const numop_params_t *pdata = (const numop_params_t *)data;
    ib_num_t              param_value;  /* Parameter value */
    ib_num_t              value;
    ib_status_t           rc;

    /* Get integer representation of the field */
    rc = field_to_num(ib, field, &value);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Get the numeric value (including expansion, etc) */
    rc = get_num_value(tx, pdata, flags, &param_value);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Do the comparison */
    *result = (value < param_value);
    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * Execute function for the numeric "greater than or equal to" operator
 * @internal
 *
 * @param[in] ib Ironbee engine.
 * @param[in] tx The transaction for this operator.
 * @param[in] data Pointer to number to compare to
 * @param[in] flags Operator instance flags
 * @param[in] field Field value
 * @param[out] result Pointer to number in which to store the result
 *
 * @returns Status code
 */
static ib_status_t op_ge_execute(ib_engine_t *ib,
                                 ib_tx_t *tx,
                                 void *data,
                                 ib_flags_t flags,
                                 ib_field_t *field,
                                 ib_num_t *result)
{
    IB_FTRACE_INIT();
    const numop_params_t *pdata = (const numop_params_t *)data;
    ib_num_t              param_value;  /* Parameter value */
    ib_num_t              value;
    ib_status_t           rc;

    /* Get integer representation of the field */
    rc = field_to_num(ib, field, &value);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Expand the data value? */
    rc = get_num_value(tx, pdata, flags, &param_value);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Do the comparison */
    *result = (value >= param_value);
    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * Execute function for the "less than or equal to" operator
 * @internal
 *
 * @param[in] ib Ironbee engine.
 * @param[in] tx The transaction for this operator.
 * @param[in] data Pointer to number to compare to
 * @param[in] flags Operator instance flags
 * @param[in] field Field value
 * @param[out] result Pointer to number in which to store the result
 *
 * @returns Status code
 */
static ib_status_t op_le_execute(ib_engine_t *ib,
                                 ib_tx_t *tx,
                                 void *data,
                                 ib_flags_t flags,
                                 ib_field_t *field,
                                 ib_num_t *result)
{
    IB_FTRACE_INIT();
    const numop_params_t *pdata = (const numop_params_t *)data;
    ib_num_t              param_value;  /* Parameter value */
    ib_num_t              value;
    ib_status_t           rc;

    /* Get integer representation of the field */
    rc = field_to_num(ib, field, &value);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Expand the data value? */
    rc = get_num_value(tx, pdata, flags, &param_value);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Do the comparison */
    *result = (value <= param_value);
    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * Initialize the core operators
 **/
ib_status_t ib_core_operators_init(ib_engine_t *ib, ib_module_t *mod)
{
    IB_FTRACE_INIT();
    ib_status_t rc;


    /**
     * String comparison operators
     */

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
                              op_contains_execute);
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

    /**
     * Numeric comparison operators
     */

    /* Register the numeric equal operator */
    rc = ib_operator_register(ib,
                              "eq",
                              IB_OP_FLAG_NONE,
                              op_numcmp_create,
                              NULL, /* no destroy function */
                              op_eq_execute);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Register the numeric not-equal operator */
    rc = ib_operator_register(ib,
                              "ne",
                              IB_OP_FLAG_NONE,
                              op_numcmp_create,
                              NULL, /* no destroy function */
                              op_ne_execute);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Register the numeric greater-than operator */
    rc = ib_operator_register(ib,
                              "gt",
                              IB_OP_FLAG_NONE,
                              op_numcmp_create,
                              NULL, /* no destroy function */
                              op_gt_execute);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Register the numeric less-than operator */
    rc = ib_operator_register(ib,
                              "lt",
                              IB_OP_FLAG_NONE,
                              op_numcmp_create,
                              NULL, /* no destroy function */
                              op_lt_execute);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Register the numeric greater-than or equal to operator */
    rc = ib_operator_register(ib,
                              "ge",
                              IB_OP_FLAG_NONE,
                              op_numcmp_create,
                              NULL, /* no destroy function */
                              op_ge_execute);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Register the numeric less-than or equal to operator */
    rc = ib_operator_register(ib,
                              "le",
                              IB_OP_FLAG_NONE,
                              op_numcmp_create,
                              NULL, /* no destroy function */
                              op_le_execute);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }


    /**
     * Misc operators
     */

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

    /**
     * Simple True / False operators.
     */

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

    IB_FTRACE_RET_STATUS(IB_OK);
}
