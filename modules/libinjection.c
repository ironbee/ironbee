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
 * @brief IronBee --- SQLi Module based on libinjection.
 *
 * This module utilizes libinjection to implement SQLi detection. The
 * libinjection library is the work of Nick Galbreath.
 *
 * http://www.client9.com/projects/libinjection/
 *
 * Transformations:
 *   - normalizeSqli: Normalize SQL routine from libinjection.
 *
 * Operators:
 *   - sqli: Returns true if the data contains SQL injection.
 *
 * @author Brian Rectanus <brectanus@qualys.com>
 */

#include <ironbee/module.h>
#include <ironbee/mpool.h>
#include <ironbee/rule_engine.h>
#include <ironbee/util.h>
#include <ironbee/transformation.h>

#include "sqlparse.h"
#include "sqli_normalize.h"
#include "sqli_fingerprints.h"

#include <assert.h>

/* Define the module name as well as a string version of it. */
#define MODULE_NAME        sqli
#define MODULE_NAME_STR    IB_XSTRINGIFY(MODULE_NAME)

/* Declare the public module symbol. */
IB_MODULE_DECLARE();


/*********************************
 * Transformations
 *********************************/

static
ib_status_t sqli_normalize_tfn(ib_engine_t *ib,
                               ib_mpool_t *mp,
                               void *tfn_data,
                               const ib_field_t *field_in,
                               ib_field_t **field_out,
                               ib_flags_t *pflags)
{
    ib_bytestr_t *bs;
    size_t new_size;
    ib_status_t rc;

    assert(ib != NULL);
    assert(mp != NULL);
    assert(field_in != NULL);
    assert(field_out != NULL);
    assert(pflags != NULL);

    /* Currently only bytestring types are supported.
     * Other types will just get passed through. */
    if (field_in->type != IB_FTYPE_BYTESTR) {
        return ib_field_copy(field_out, mp, field_in->name, field_in->nlen, field_in);
    }

    /* Make a copy of the field and extract the underlying value. */
    rc = ib_field_copy(field_out, mp, field_in->name, field_in->nlen, field_in);
    if (rc != IB_OK) {
        return rc;
    }
    rc = ib_field_value(*field_out, ib_ftype_bytestr_mutable_out(&bs));
    if (rc != IB_OK) {
        return rc;
    }

    /* Normalize the value with libinjection. */
    new_size = sqli_qs_normalize((char *)ib_bytestr_ptr(bs), ib_bytestr_length(bs));
    ib_bytestr_setv(bs, ib_bytestr_ptr(bs), new_size);

    /* Mark as modified. */
    /* TODO: Need to know if it was actually modified or not. */
    *pflags = IB_TFN_FMODIFIED;

    return IB_OK;
}


/*********************************
 * Operators
 *********************************/

static
ib_status_t sqli_op_create(ib_engine_t *ib,
                           ib_context_t *ctx,
                           const ib_rule_t *rule,
                           ib_mpool_t *mp,
                           const char *parameters,
                           ib_operator_inst_t *op_inst)
{

    if (parameters == NULL) {
        ib_log_error(ib, "Missing parameter for operator %s",
                     op_inst->op->name);
        return IB_EINVAL;
    }

    // TODO: Support loading external patterns.
    if (strcmp("default", parameters) != 0) {
        ib_log_notice(ib,
                      "SQLi external data not yet supported for %s."
                      " Use \"default\" for now.",
                      op_inst->op->name);
    }

    return IB_OK;
}

static
ib_status_t sqli_op_execute(const ib_rule_exec_t *rule_exec,
                            void *data,
                            ib_flags_t flags,
                            ib_field_t *field,
                            ib_num_t *result
)
{
    assert(rule_exec != NULL);
    assert(field     != NULL);
    assert(result    != NULL);

    sfilter sf;
    char *val;
    size_t new_size;
    ib_bytestr_t *bs;
    ib_tx_t *tx = rule_exec->tx;
    ib_status_t rc;

    *result = 0;

    /* Currently only bytestring types are supported.
     * Other types will just get passed through. */
    if (field->type != IB_FTYPE_BYTESTR) {
        return IB_DECLINED;
    }

    rc = ib_field_value(field, ib_ftype_bytestr_mutable_out(&bs));
    if (rc != IB_OK) {
        return rc;
    }

    /* Normalize the value with libinjection. */
    val = (char *)malloc(ib_bytestr_length(bs));
    if (val == NULL) {
        return IB_EALLOC;
    }
    memcpy(val, ib_bytestr_const_ptr(bs), ib_bytestr_length(bs));
    new_size = sqli_qs_normalize(val, ib_bytestr_length(bs));

    /* Run through libinjection. */
    // TODO: Support alternative SQLi pattern lookup
    if (is_sqli(&sf, val, new_size, is_sqli_pattern)) {
        ib_log_debug_tx(tx, "Matched SQLi pattern: %s", sf.pat);
        *result = 1;
    }

    free(val);

    return IB_OK;
}



/*********************************
 * Module Functions
 *********************************/

/* Called to initialize a module (on load). */
static ib_status_t sqli_init(ib_engine_t *ib, ib_module_t *m, void *cbdata)
{
    ib_status_t rc;

    ib_log_debug(ib, "Initializing %s module.", MODULE_NAME_STR);

    rc = ib_tfn_register(ib, "normalizeSqli", sqli_normalize_tfn,
                         IB_TFN_FLAG_NONE, NULL);
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_operator_register(ib,
                              "sqli",
                              IB_OP_FLAG_PHASE,
                              sqli_op_create,
                              NULL,
                              NULL,
                              NULL,
                              sqli_op_execute,
                              NULL);
    if (rc != IB_OK) {
        return rc;
    }

    return IB_OK;
}

/* Called to finish a module (on unload). */
static ib_status_t sqli_fini(ib_engine_t *ib, ib_module_t *m, void *cbdata)
{
    ib_log_debug(ib, "Finish %s module.", MODULE_NAME_STR);

    return IB_OK;
}

/* Initialize the module structure. */
IB_MODULE_INIT(
    IB_MODULE_HEADER_DEFAULTS,           /* Default metadata */
    MODULE_NAME_STR,                     /* Module name */
    IB_MODULE_CONFIG_NULL,               /* Global config data */
    NULL,                                /* Configuration field map */
    NULL,                                /* Config directive map */
    sqli_init,                           /* Initialize function */
    NULL,                                /* Callback data */
    sqli_fini,                           /* Finish function */
    NULL,                                /* Callback data */
    NULL,                                /* Context open function */
    NULL,                                /* Callback data */
    NULL,                                /* Context close function */
    NULL,                                /* Callback data */
    NULL,                                /* Context destroy function */
    NULL                                 /* Callback data */
);
