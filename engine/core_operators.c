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
#include <ironbee/util.h>

#include "core_private.h"

/* Numeric operator params */
typedef struct {
    const char *str;
    ib_num_t    num;
} numop_params_t;

/* Structure used for ipmatch operator */
typedef struct {
    ib_radix_t  *radix;
    const char  *ascii;
} ipmatch_data_t;

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
        ib_log_debug(ib, "Failed to allocate unescape string buffer.");
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    rc = ib_util_unescape_string(tmp_unesc,
                                 &tmp_unesc_len,
                                 str,
                                 str_len,
                                 IB_UTIL_UNESCAPE_NULTERMINATE);

    if ( rc != IB_OK ) {
        ib_log_debug(ib, "Failed to unescape string: %s", str);
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Commit changes on success. */
    *str_unesc = tmp_unesc;
    *str_unesc_len = tmp_unesc_len;

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
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
                                const ib_rule_t *rule,
                                ib_mpool_t *mp,
                                const char *parameters,
                                ib_operator_inst_t *op_inst)
{
    IB_FTRACE_INIT();
    ib_status_t rc;
    bool expand;
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
                                    const ib_rule_t *rule,
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

    if (ib_rule_should_capture(rule, *result) == true) {
        ib_data_capture_clear(tx);
        ib_data_capture_set_item(tx, 0, field);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * Execute function for the "contains" operator
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
                                       const ib_rule_t *rule,
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

    if ( (tx != NULL) && (ib_rule_should_capture(rule, *result) == true) ) {
        ib_field_t *f;
        const char *name;

        ib_data_capture_clear(tx);

        name = ib_data_capture_name(0);
        rc = ib_field_create_bytestr_alias(&f, tx->mp, name, strlen(name),
                                           (uint8_t *)expanded,
                                           strlen(expanded));
        ib_data_capture_set_item(tx, 0, f);
    }

    IB_FTRACE_RET_STATUS(rc);
}

/**
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
                                     const ib_rule_t *rule,
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
    ipmatch_data_t *ipmatch_data;
    char *ascii;

    if (parameters == NULL) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    /* Make a copy of the parameters to operate on */
    rc = unescape_op_args(ib, mp, &copy, &copy_len, parameters);
    if (rc != IB_OK) {
        ib_log_error(ib,
                     "Error unescaping rule parameters '%s'", parameters);
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    /* Create the radix matcher */
    rc = ib_radix_new(&radix, NULL, NULL, NULL, mp);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to allocate a radix matcher: %s",
                     ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }

    ipmatch_data = ib_mpool_alloc(mp, sizeof(*ipmatch_data) );
    if (ipmatch_data == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }
    ascii = ib_mpool_alloc(mp, copy_len+1);
    if (ascii == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }
    *ascii = '\0';

    ipmatch_data->radix = radix;
    ipmatch_data->ascii = ascii;

    /* Split the parameters into the separate pieces */
    for (p = strtok(copy, " ");  p != NULL;  p = strtok(NULL, " ") ) {
        ib_radix_prefix_t *prefix = NULL;

        /* Convert the IP address string to a prefix object */
        rc = ib_radix_ip_to_prefix(p, &prefix, mp);
        if (rc != IB_OK) {
            ib_log_error(ib,
                         "Error created radix prefix for %s: %s",
                         p, ib_status_to_string(rc));
            IB_FTRACE_RET_STATUS(rc);
        }

        /* Insert the prefix into the radix tree */
        rc = ib_radix_insert_data(radix, prefix, copy);
        if (rc != IB_OK) {
            ib_log_error(ib,
                         "Error loading prefix %s to the radix tree: %s",
                         p, ib_status_to_string(rc));
            IB_FTRACE_RET_STATUS(rc);
        }
        if (*ascii != '\0') {
            strcat(ascii, ",");
        }
        strcat(ascii, p);
        ib_log_debug3(ib, "prefix '%s' added to radix tree %p",
                      p, (void *)radix);
    }

    /* Done */
    op_inst->data = ipmatch_data;
    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * Execute function for the "ipmatch" operator
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
                                      const ib_rule_t *rule,
                                      void *data,
                                      ib_flags_t flags,
                                      ib_field_t *field,
                                      ib_num_t *result)
{
    IB_FTRACE_INIT();
    assert(ib != NULL);
    assert(tx != NULL);
    assert(rule != NULL);
    assert(data != NULL);
    assert(field != NULL);
    assert(result != NULL);

    ib_status_t rc;
    const ipmatch_data_t *ipmatch_data; /* The radix matcher object */
    ib_radix_prefix_t *prefix;          /* The IP address */
    const char *ipstr;                  /* String version of the address */
    ib_num_t iplen;                     /* Length of the address string */
    char *rmatch = NULL;                /* Radix match */

    ipmatch_data = (const ipmatch_data_t *)data;

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
            ib_log_error_tx(tx, "Failed to get NULSTR from field");
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
        ipstr = (const char *)ib_bytestr_const_ptr(bs);
    }
    else {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    /* Convert the IP address string to a prefix object */
    rc = ib_radix_ip_to_prefix_ex(ipstr, iplen, &prefix, tx->mp);
    if (rc != IB_OK) {
        ib_log_error_tx(tx,
                     "Error creating radix prefix for %.*s: %s",
                     (int)iplen, ipstr, ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Do the matching */
    rc = ib_radix_match_closest(ipmatch_data->radix, prefix, &rmatch);
    ib_rule_log_debug(tx, rule, NULL, NULL,
                      "Matching \"%.*s\" against pattern(s) \"%s\": %s",
                      (int)iplen, ipstr, ipmatch_data->ascii,
                      ib_status_to_string(rc));
    if (rc == IB_ENOENT) {
        *result = 0;
    }
    else if (rc == IB_OK) {
        *result = 1;
        if (ib_rule_should_capture(rule, *result) == true) {
            ib_data_capture_clear(tx);
            ib_data_capture_set_item(tx, 0, field);
        }
    }
    else {
        ib_log_error_tx(tx,
                        "Radix matcher failed matching for %.*s: %s",
                        (int)iplen, ipstr, ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }
    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * Create function for the numeric comparison operators
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
                                    const ib_rule_t *rule,
                                    ib_mpool_t *mp,
                                    const char *params,
                                    ib_operator_inst_t *op_inst)
{
    IB_FTRACE_INIT();
    numop_params_t *vptr;
    ib_status_t rc;
    bool expandable;
    ib_num_t value;

    char *params_unesc;
    size_t params_unesc_len;

    rc = unescape_op_args(ib, mp, &params_unesc, &params_unesc_len, params);
    if (rc != IB_OK) {
        ib_log_debug(ib, "Unable to unescape parameter: %s", params);
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
    vptr->str = ib_mpool_strdup(mp, params_unesc);
    if (vptr->str == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }
    if (expandable == false) {
        vptr->num = value;
    }

    op_inst->data = vptr;
    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * Get expanded numeric value of a string
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

static ib_status_t capture_num(ib_tx_t *tx, int num, ib_num_t value)
{
    IB_FTRACE_INIT();
    assert(tx != NULL);

    ib_status_t rc;
    ib_field_t *field;
    const char *name;
    const char *str;

    name = ib_data_capture_name(num);

    str = ib_num_to_string(tx->mp, value);
    if (str == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }
    rc = ib_field_create_bytestr_alias(&field, tx->mp, name, strlen(name),
                                       (uint8_t *)str, strlen(str));
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }
    rc = ib_data_capture_set_item(tx, 0, field);
    IB_FTRACE_RET_STATUS(rc);
}

/**
 * Execute function for the numeric "equal" operator
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
                                 const ib_rule_t *rule,
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

    /* Get the numeric value from the param data (including expansion, etc) */
    rc = get_num_value(tx, pdata, flags, &param_value);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Do the comparison */
    *result = (value == param_value);
    if (ib_rule_should_capture(rule, *result) == true) {
        ib_data_capture_clear(tx);
        capture_num(tx, 0, value);
    }
    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * Execute function for the numeric "not equal" operator
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
                                 const ib_rule_t *rule,
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
    if (ib_rule_should_capture(rule, *result) == true) {
        ib_data_capture_clear(tx);
        rc = capture_num(tx, 0, value);
        if (rc != IB_OK) {
            ib_log_error_tx(tx, "Error storing capture #0: %s",
                            ib_status_to_string(rc));
        }
    }
    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * Execute function for the "gt" operator
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
                                 const ib_rule_t *rule,
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
    if (ib_rule_should_capture(rule, *result) == true) {
        ib_data_capture_clear(tx);
        rc = capture_num(tx, 0, value);
        if (rc != IB_OK) {
            ib_log_error_tx(tx, "Error storing capture #0: %s",
                            ib_status_to_string(rc));
        }
    }
    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * Execute function for the numeric "less-than" operator
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
                                 const ib_rule_t *rule,
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

    if (ib_rule_should_capture(rule, *result) == true) {
        ib_data_capture_clear(tx);
        rc = capture_num(tx, 0, value);
        if (rc != IB_OK) {
            ib_log_error_tx(tx, "Error storing capture #0: %s",
                            ib_status_to_string(rc));
        }
    }
    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * Execute function for the numeric "greater than or equal to" operator
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
                                 const ib_rule_t *rule,
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
    if (ib_rule_should_capture(rule, *result) == true) {
        ib_data_capture_clear(tx);
        rc = capture_num(tx, 0, value);
        if (rc != IB_OK) {
            ib_log_error_tx(tx, "Error storing capture #0: %s",
                            ib_status_to_string(rc));
        }
    }
    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * Execute function for the "less than or equal to" operator
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
                                 const ib_rule_t *rule,
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
    if (ib_rule_should_capture(rule, *result) == true) {
        ib_data_capture_clear(tx);
        rc = capture_num(tx, 0, value);
        if (rc != IB_OK) {
            ib_log_error_tx(tx, "Error storing capture #0: %s",
                            ib_status_to_string(rc));
        }
    }
    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * Execute function for the "nop" operator
 *
 * @param[in] ib Ironbee engine (unused)
 * @param[in] tx The transaction for this operator (unused)
 * @param[in] rule Parent rule to the operator
 * @param[in] data Operator data (unused)
 * @param[in] flags Operator instance flags
 * @param[in] field Field value (unused)
 * @param[out] result Pointer to number in which to store the result
 *
 * @returns Status code (IB_OK)
 */
static ib_status_t op_nop_execute(ib_engine_t *ib,
                                  ib_tx_t *tx,
                                  const ib_rule_t *rule,
                                  void *data,
                                  ib_flags_t flags,
                                  ib_field_t *field,
                                  ib_num_t *result)
{
    IB_FTRACE_INIT();
    ib_log_debug2_tx(tx, "NOP operator returning 1");
    *result = 1;

    if (ib_rule_should_capture(rule, *result) == true) {
        ib_data_capture_clear(tx);
        ib_data_capture_set_item(tx, 0, field);
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


    /**
     * String comparison operators
     */

    /* Register the string equal operator */
    rc = ib_operator_register(ib,
                              "streq",
                              IB_OP_FLAG_PHASE | IB_OP_FLAG_CAPTURE,
                              strop_create,
                              NULL,
                              NULL, /* no destroy function */
                              NULL,
                              op_streq_execute,
                              NULL);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Register the string contains operator */
    rc = ib_operator_register(ib,
                              "contains",
                              IB_OP_FLAG_PHASE | IB_OP_FLAG_CAPTURE,
                              strop_create,
                              NULL,
                              NULL, /* no destroy function */
                              NULL,
                              op_contains_execute,
                              NULL);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Register the ipmatch operator */
    rc = ib_operator_register(ib,
                              "ipmatch",
                              IB_OP_FLAG_PHASE | IB_OP_FLAG_CAPTURE,
                              op_ipmatch_create,
                              NULL,
                              NULL, /* no destroy function */
                              NULL,
                              op_ipmatch_execute,
                              NULL);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /**
     * Numeric comparison operators
     */

    /* Register the numeric equal operator */
    rc = ib_operator_register(ib,
                              "eq",
                              IB_OP_FLAG_PHASE | IB_OP_FLAG_CAPTURE,
                              op_numcmp_create,
                              NULL,
                              NULL, /* no destroy function */
                              NULL,
                              op_eq_execute,
                              NULL);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Register the numeric not-equal operator */
    rc = ib_operator_register(ib,
                              "ne",
                              IB_OP_FLAG_PHASE | IB_OP_FLAG_CAPTURE,
                              op_numcmp_create,
                              NULL,
                              NULL, /* no destroy function */
                              NULL,
                              op_ne_execute,
                              NULL);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Register the numeric greater-than operator */
    rc = ib_operator_register(ib,
                              "gt",
                              IB_OP_FLAG_PHASE | IB_OP_FLAG_CAPTURE,
                              op_numcmp_create,
                              NULL,
                              NULL, /* no destroy function */
                              NULL,
                              op_gt_execute,
                              NULL);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Register the numeric less-than operator */
    rc = ib_operator_register(ib,
                              "lt",
                              IB_OP_FLAG_PHASE | IB_OP_FLAG_CAPTURE,
                              op_numcmp_create,
                              NULL,
                              NULL, /* no destroy function */
                              NULL,
                              op_lt_execute,
                              NULL);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Register the numeric greater-than or equal to operator */
    rc = ib_operator_register(ib,
                              "ge",
                              IB_OP_FLAG_PHASE | IB_OP_FLAG_CAPTURE,
                              op_numcmp_create,
                              NULL,
                              NULL, /* no destroy function */
                              NULL,
                              op_ge_execute,
                              NULL);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Register the numeric less-than or equal to operator */
    rc = ib_operator_register(ib,
                              "le",
                              IB_OP_FLAG_PHASE | IB_OP_FLAG_CAPTURE,
                              op_numcmp_create,
                              NULL,
                              NULL, /* no destroy function */
                              NULL,
                              op_le_execute,
                              NULL);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Register NOP operator */
    rc = ib_operator_register(ib,
                              "nop",
                              ( IB_OP_FLAG_ALLOW_NULL |
                                IB_OP_FLAG_PHASE |
                                IB_OP_FLAG_STREAM |
                                IB_OP_FLAG_CAPTURE ),
                              NULL, NULL, /* No create function */
                              NULL, NULL, /* no destroy function */
                              op_nop_execute, NULL);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }


    IB_FTRACE_RET_STATUS(IB_OK);
}
