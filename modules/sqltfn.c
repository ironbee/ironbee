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
 * @brief IronBee --- SQL Transformation Module
 *
 * This module utilizes the sqltfn library to implement SQL normalization.
 *
 * Transformations:
 *   - normalizeSqlPg: Normalize Postgres SQL routine from libinjection.
 *
 * @author Brian Rectanus <brectanus@qualys.com>
 */

#include <ironbee/mm.h>
#include <ironbee/module.h>
#include <ironbee/transformation.h>
#include <ironbee/util.h>

#include <sqltfn.h>

#include <assert.h>

/* Define the module name as well as a string version of it. */
#define MODULE_NAME        sqltfn
#define MODULE_NAME_STR    IB_XSTRINGIFY(MODULE_NAME)

/* Declare the public module symbol. */
IB_MODULE_DECLARE();


/*********************************
 * Transformations
 *********************************/

static
ib_status_t sqltfn_normalize_pg_tfn(
    ib_mm_t            mm,
    const ib_field_t  *field_in,
    const ib_field_t **field_out,
    void              *instarg,
    void              *tfn_data
)
{
    assert(field_in != NULL);
    assert(field_out != NULL);

    ib_bytestr_t *bs_in;
    ib_bytestr_t *bs_out;
    const char *buf_in;
    const char *buf_in_start;
    size_t buf_in_len;
    char *buf_out;
    char *buf_out_end;
    size_t buf_out_len;
    size_t lead_len = 0;
    ib_status_t rc;
    int ret;
    ib_field_t *field_new;

    /* Currently only bytestring types are supported.
     * Other types will just get passed through. */
    if (field_in->type != IB_FTYPE_BYTESTR) {
        *field_out = field_in;
        return IB_OK;
    }

    /* Extract the underlying incoming value. */
    rc = ib_field_value(field_in, ib_ftype_bytestr_mutable_out(&bs_in));
    if (rc != IB_OK) {
        return rc;
    }
    if (ib_bytestr_length(bs_in) == 0) {
        *field_out = field_in;
        return IB_OK;
    }

    /* Create a buffer for normalization. */
    buf_out = buf_out_end = (char *)ib_mm_alloc(mm, ib_bytestr_length(bs_in));
    if (buf_out == NULL) {
        return IB_EALLOC;
    }

    /* As SQL can be injected into a string, the normalization
     * needs to start after the first quote character if one
     * exists.
     *
     * First try single quote, then double, then none.
     *
     * TODO: Handle returning multiple transformations:
     *       1) Straight normalization
     *       2) Normalization as if with single quotes (starting point
     *          should be based on straight normalization)
     *       3) Normalization as if with double quotes (starting point
     *          should be based on straight normalization)
     */
    buf_in = (const char *)ib_bytestr_const_ptr(bs_in);
    buf_in_start = memchr(buf_in, '\'', ib_bytestr_length(bs_in));
    if (buf_in_start == NULL) {
        buf_in_start = memchr(buf_in, '"', ib_bytestr_length(bs_in));
    }
    if (buf_in_start == NULL) {
        buf_in_start = buf_in;
        buf_in_len = ib_bytestr_length(bs_in);
    }
    else {
        ++buf_in_start; /* After the quote. */
        buf_in_len = ib_bytestr_length(bs_in) - (buf_in_start - buf_in);
    }

    /* Copy the leading string if one exists. */
    if (buf_in_start != buf_in) {
        lead_len = buf_in_start - buf_in;
        memcpy(buf_out, buf_in, lead_len);
        buf_out_end += lead_len;
    }

    /* Normalize. */
    ret = sqltfn_normalize_pg_ex(buf_in_start, buf_in_len,
                                 &buf_out_end, &buf_out_len);
    if (ret < 0) {
        return IB_EALLOC;
    }

    /* Create the output field wrapping bs_out. */
    buf_out_len += lead_len;
    rc = ib_bytestr_alias_mem(&bs_out, mm, (uint8_t *)buf_out, buf_out_len);
    if (rc != IB_OK) {
        return rc;
    }
    rc =ib_field_create(&field_new, mm,
                        field_in->name, field_in->nlen,
                        IB_FTYPE_BYTESTR,
                        ib_ftype_bytestr_mutable_in(bs_out));
    if (rc == IB_OK) {
        *field_out = field_new;
    }
    return rc;
}


/*********************************
 * Module Functions
 *********************************/

/* Called to initialize a module (on load). */
static ib_status_t sqltfn_init(ib_engine_t *ib, ib_module_t *m, void *cbdata)
{
    ib_status_t rc;

    rc = ib_transformation_create_and_register(
        NULL,
        ib,
        "normalizeSqlPg",
        false,
        NULL, NULL,
        NULL, NULL,
        sqltfn_normalize_pg_tfn, NULL
    );
    if (rc != IB_OK) {
        return rc;
    }

    return IB_OK;
}

/* Initialize the module structure. */
IB_MODULE_INIT(
    IB_MODULE_HEADER_DEFAULTS,           /* Default metadata */
    MODULE_NAME_STR,                     /* Module name */
    IB_MODULE_CONFIG_NULL,               /* Global config data */
    NULL,                                /* Configuration field map */
    NULL,                                /* Config directive map */
    sqltfn_init,                         /* Initialize function */
    NULL,                                /* Callback data */
    NULL,                                /* Finish function */
    NULL,                                /* Callback data */
);
