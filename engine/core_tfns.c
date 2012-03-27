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
#include <ironbee/operator.h>
#include <ironbee/transformation.h>
#include <ironbee/field.h>
#include <ironbee/string.h>

#include "ironbee_private.h"


/**
 * Simple ASCII lowercase function.
 * @internal
 *
 * @param[in] ib IronBee engine
 * @param[in] mp Memory pool to use for allocations.
 * @param[in] fndata Function specific data.
 * @param[in] fin Input field.
 * @param[out] fout Output field.
 * @param[out] pflags Transformation flags.
 *
 * @note For non-ASCII (utf8, etc) you should use case folding.
 *
 * @returns IB_OK if successful.
 */
static ib_status_t tfn_lowercase(ib_engine_t *ib,
                                 ib_mpool_t *mp,
                                 void *fndata,
                                 ib_field_t *fin,
                                 ib_field_t **fout,
                                 ib_flags_t *pflags)
{
    IB_FTRACE_INIT();
    ib_status_t rc;
    ib_bool_t modified = IB_FALSE;

    /* We only handle bytestr and nulstr non-dynamic fields */
    if (ib_field_is_dynamic(fin)) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    if (fin->type == IB_FTYPE_NULSTR) {
        char *p = (char *)ib_field_value_nulstr(fin);
        assert (p != NULL);
        *fout = fin;
        rc = ib_strlower(p, &modified);
    }
    else if (fin->type == IB_FTYPE_BYTESTR) {
        ib_bytestr_t *bs = (ib_bytestr_t *)ib_field_value_bytestr(fin);
        assert (bs != NULL);
        *fout = fin;
        rc = ib_strlower_ex(ib_bytestr_ptr(bs),
                            ib_bytestr_length(bs),
                            &modified);
    }
    else {
        rc = IB_EINVAL;
    }

    if (modified) {
        (*pflags) |= IB_TFN_FMODIFIED;
    }
    if (rc == IB_OK) {
        (*pflags) |= IB_TFN_FINPLACE;
    }
    IB_FTRACE_RET_STATUS(rc);
}

/**
 * Simple ASCII trim (left) transformation.
 * @internal
 *
 * @param[in] ib IronBee engine
 * @param[in] mp Memory pool to use for allocations.
 * @param[in] fndata Function specific data.
 * @param[in] fin Input field.
 * @param[out] fout Output field.
 * @param[out] pflags Transformation flags.
 *
 * @returns IB_OK if successful.
 */
static ib_status_t tfn_trim_left(ib_engine_t *ib,
                                 ib_mpool_t *mp,
                                 void *fndata,
                                 ib_field_t *fin,
                                 ib_field_t **fout,
                                 ib_flags_t *pflags)
{
    IB_FTRACE_INIT();
    ib_status_t rc;
    ib_bool_t modified = IB_FALSE;

    /* We only handle bytestr and nulstr non-dynamic fields */
    if (ib_field_is_dynamic(fin)) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    if (fin->type == IB_FTYPE_NULSTR) {
        char *p = (char *)ib_field_value_nulstr(fin);
        char *out;
        assert (p != NULL);

        rc = ib_strtrim_left(p, &out, &modified);
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }
        if (modified == IB_TRUE) {
            rc = ib_field_create_ex(fout,
                                    mp,
                                    fin->name, fin->nlen,
                                    IB_FTYPE_NULSTR,
                                    &out);
        }
        else {
            *fout = fin;
        }
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }
    }
    else if (fin->type == IB_FTYPE_BYTESTR) {
        ib_bytestr_t *bs = (ib_bytestr_t *)ib_field_value_bytestr(fin);
        uint8_t *out;
        size_t outlen;
        assert (bs != NULL);

        rc = ib_strtrim_left_ex(ib_bytestr_ptr(bs),
                                ib_bytestr_length(bs),
                                &out,
                                &outlen,
                                &modified);
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }
        rc = ib_field_alias_mem_ex(fout, mp, fin->name, fin->nlen, out, outlen);
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }
    }
    else {
        rc = IB_EINVAL;
    }

    if (modified) {
        (*pflags) |= IB_TFN_FMODIFIED;
    }
    if (rc == IB_OK) {
        (*pflags) |= IB_TFN_FINPLACE;
    }
    IB_FTRACE_RET_STATUS(rc);
}

/**
 * Simple ASCII trim (right) transformation.
 * @internal
 *
 * @param[in] ib IronBee engine
 * @param[in] mp Memory pool to use for allocations.
 * @param[in] fndata Function specific data.
 * @param[in] fin Input field.
 * @param[out] fout Output field.
 * @param[out] pflags Transformation flags.
 *
 * @returns IB_OK if successful.
 */
static ib_status_t tfn_trim_right(ib_engine_t *ib,
                                  ib_mpool_t *mp,
                                  void *fndata,
                                  ib_field_t *fin,
                                  ib_field_t **fout,
                                  ib_flags_t *pflags)
{
    IB_FTRACE_INIT();
    ib_status_t rc;
    ib_bool_t modified = IB_FALSE;

    /* We only handle bytestr and nulstr non-dynamic fields */
    if (ib_field_is_dynamic(fin)) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    if (fin->type == IB_FTYPE_NULSTR) {
        char *p = (char *)ib_field_value_nulstr(fin);
        char *out;
        assert (p != NULL);

        rc = ib_strtrim_right(p, &out, &modified);
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }
        rc = ib_field_create_ex(fout,
                                mp,
                                fin->name, fin->nlen,
                                IB_FTYPE_NULSTR,
                                &out);
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }
    }
    else if (fin->type == IB_FTYPE_BYTESTR) {
        ib_bytestr_t *bs = (ib_bytestr_t *)ib_field_value_bytestr(fin);
        uint8_t *out;
        size_t outlen;
        assert (bs != NULL);

        rc = ib_strtrim_right_ex(ib_bytestr_ptr(bs),
                                 ib_bytestr_length(bs),
                                 &out,
                                 &outlen,
                                 &modified);
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }
        rc = ib_field_alias_mem_ex(fout, mp, fin->name, fin->nlen, out, outlen);
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }
    }
    else {
        rc = IB_EINVAL;
    }

    if (modified) {
        (*pflags) |= IB_TFN_FMODIFIED;
    }
    if (rc == IB_OK) {
        (*pflags) |= IB_TFN_FINPLACE;
    }

    IB_FTRACE_RET_STATUS(rc);
}

/**
 * Simple ASCII trim transformation.
 * @internal
 *
 * @param[in] ib IronBee engine
 * @param[in] mp Memory pool to use for allocations.
 * @param[in] fndata Function specific data.
 * @param[in] fin Input field.
 * @param[out] fout Output field.
 * @param[out] pflags Transformation flags.
 *
 * @returns IB_OK if successful.
 */
static ib_status_t tfn_trim(ib_engine_t *ib,
                            ib_mpool_t *mp,
                            void *fndata,
                            ib_field_t *fin,
                            ib_field_t **fout,
                            ib_flags_t *pflags)
{
    IB_FTRACE_INIT();
    ib_status_t rc;
    ib_bool_t modified = IB_FALSE;

    /* We only handle bytestr and nulstr non-dynamic fields */
    if (ib_field_is_dynamic(fin)) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    if (fin->type == IB_FTYPE_NULSTR) {
        char *p = (char *)ib_field_value_nulstr(fin);
        char *out;
        assert (p != NULL);

        rc = ib_strtrim_lr(p, &out, &modified);
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }
        rc = ib_field_create_ex(
            fout, mp, fin->name, fin->nlen, IB_FTYPE_NULSTR, &out);
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }
    }
    else if (fin->type == IB_FTYPE_BYTESTR) {
        ib_bytestr_t *bs = (ib_bytestr_t *)ib_field_value_bytestr(fin);
        uint8_t *out;
        size_t outlen;
        assert (bs != NULL);

        rc = ib_strtrim_lr_ex(ib_bytestr_ptr(bs),
                              ib_bytestr_length(bs),
                              &out,
                              &outlen,
                              &modified);
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }
        rc = ib_field_alias_mem_ex(fout, mp, fin->name, fin->nlen, out, outlen);
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }
    }
    else {
        rc = IB_EINVAL;
    }

    if (modified) {
        (*pflags) |= IB_TFN_FMODIFIED;
    }
    if (rc == IB_OK) {
        (*pflags) |= IB_TFN_FINPLACE;
    }
    IB_FTRACE_RET_STATUS(rc);
}

/**
 * Remove all whitespace from a string
 * @internal
 *
 * @param[in] ib IronBee engine
 * @param[in] mp Memory pool to use for allocations.
 * @param[in] fndata Function specific data.
 * @param[in] fin Input field.
 * @param[out] fout Output field.
 * @param[out] pflags Transformation flags.
 *
 * @returns IB_OK if successful.
 */
static ib_status_t tfn_wspc_remove(ib_engine_t *ib,
                                   ib_mpool_t *mp,
                                   void *fndata,
                                   ib_field_t *fin,
                                   ib_field_t **fout,
                                   ib_flags_t *pflags)
{
    IB_FTRACE_INIT();
    ib_status_t rc;
    ib_bool_t modified = IB_FALSE;

    /* We only handle bytestr and nulstr non-dynamic fields */
    if (ib_field_is_dynamic(fin)) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    if (fin->type == IB_FTYPE_NULSTR) {
        char *p = (char *)ib_field_value_nulstr(fin);
        char *out;
        assert (p != NULL);

        rc = ib_str_wspc_remove(mp, p, &out, &modified);
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }
        rc = ib_field_create_ex(
            fout, mp, fin->name, fin->nlen, IB_FTYPE_NULSTR, &out);
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }
    }
    else if (fin->type == IB_FTYPE_BYTESTR) {
        ib_bytestr_t *bs = (ib_bytestr_t *)ib_field_value_bytestr(fin);
        uint8_t *out;
        size_t outlen;
        assert (bs != NULL);

        rc = ib_str_wspc_remove_ex(mp,
                                   ib_bytestr_ptr(bs),
                                   ib_bytestr_length(bs),
                                   &out,
                                   &outlen,
                                   &modified);
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }
        rc = ib_field_alias_mem_ex(fout, mp, fin->name, fin->nlen, out, outlen);
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }
    }
    else {
        rc = IB_EINVAL;
    }

    if (modified) {
        (*pflags) |= IB_TFN_FMODIFIED;
    }
    if (rc == IB_OK) {
        (*pflags) |= IB_TFN_FINPLACE;
    }
    IB_FTRACE_RET_STATUS(rc);
}

/**
 * Compress whitespace in a string
 * @internal
 *
 * @param[in] ib IronBee engine
 * @param[in] mp Memory pool to use for allocations.
 * @param[in] fndata Function specific data.
 * @param[in] fin Input field.
 * @param[out] fout Output field.
 * @param[out] pflags Transformation flags.
 *
 * @returns IB_OK if successful.
 */
static ib_status_t tfn_wspc_compress(ib_engine_t *ib,
                                     ib_mpool_t *mp,
                                     void *fndata,
                                     ib_field_t *fin,
                                     ib_field_t **fout,
                                     ib_flags_t *pflags)
{
    IB_FTRACE_INIT();
    ib_status_t rc;
    ib_bool_t modified = IB_FALSE;

    /* We only handle bytestr and nulstr non-dynamic fields */
    if (ib_field_is_dynamic(fin)) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    if (fin->type == IB_FTYPE_NULSTR) {
        char *p = (char *)ib_field_value_nulstr(fin);
        char *out;
        assert (p != NULL);

        rc = ib_str_wspc_compress(mp, p, &out, &modified);
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }
        rc = ib_field_create_ex(
            fout, mp, fin->name, fin->nlen, IB_FTYPE_NULSTR, &out);
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }
    }
    else if (fin->type == IB_FTYPE_BYTESTR) {
        ib_bytestr_t *bs = (ib_bytestr_t *)ib_field_value_bytestr(fin);
        uint8_t *out;
        size_t outlen;
        assert (bs != NULL);

        rc = ib_str_wspc_compress_ex(mp,
                                     ib_bytestr_ptr(bs),
                                     ib_bytestr_length(bs),
                                     &out,
                                     &outlen,
                                     &modified);
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }
        rc = ib_field_alias_mem_ex(fout, mp, fin->name, fin->nlen, out, outlen);
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }
    }
    else {
        rc = IB_EINVAL;
    }

    if (modified) {
        (*pflags) |= IB_TFN_FMODIFIED;
    }
    if (rc == IB_OK) {
        (*pflags) |= IB_TFN_FINPLACE;
    }
    IB_FTRACE_RET_STATUS(rc);
}

/**
 * Length transformation
 * @internal
 *
 * @param[in] ib IronBee engine
 * @param[in] mp Memory pool to use for allocations.
 * @param[in] fndata Function specific data.
 * @param[in] fin Input field.
 * @param[out] fout Output field.
 * @param[out] pflags Transformation flags.
 *
 * @returns IB_OK if successful.
 */
static ib_status_t tfn_length(ib_engine_t *ib,
                              ib_mpool_t *mp,
                              void *fndata,
                              ib_field_t *fin,
                              ib_field_t **fout,
                              ib_flags_t *pflags)
{
    IB_FTRACE_INIT();
    ib_status_t rc = IB_OK;

    assert(mp != NULL);
    assert(fin != NULL);
    assert(fout != NULL);
    assert(pflags != NULL);

    /**
     * This works on C-style (NUL terminated) and byte strings.  Note
     * that data is assumed to be a NUL terminated string (because our
     * configuration parser can't produce anything else).
     **/
    if (fin->type == IB_FTYPE_NULSTR) {
        const char *fval = ib_field_value_nulstr(fin);
        size_t      len = strlen(fval);
        rc = ib_field_create(fout, mp, "Length", IB_FTYPE_NUM, &len);
    }
    else if (fin->type == IB_FTYPE_BYTESTR) {
        const ib_bytestr_t *value = ib_field_value_bytestr(fin);
        size_t len = ib_bytestr_length(value);
        rc = ib_field_create(fout, mp, "Length", IB_FTYPE_NUM, &len);
    }
    else if (fin->type == IB_FTYPE_LIST) {
        ib_list_node_t *node = NULL;
        ib_list_t      *ilist;           /** Incoming list */

        /* Get the incoming list */
        // @todo Remove const casting once list is const correct.
        ilist = (ib_list_t *)ib_field_value_list(fin);
        if (ilist == NULL) {
            IB_FTRACE_RET_STATUS(IB_EUNKNOWN);
        }

        /* Create the outgoing list field */
        rc = ib_field_create(fout, mp, "Length", IB_FTYPE_LIST, NULL);
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }

        /* Walk through the incoming fields */
        IB_LIST_LOOP(ilist, node) {
            ib_field_t *ifield = (ib_field_t*)node->data;
            ib_field_t *ofield = NULL;
            ib_flags_t flags = 0;

            rc = tfn_length(ib, mp, NULL, ifield, &ofield, &flags);
            if (rc != IB_OK) {
                IB_FTRACE_RET_STATUS(rc);
            }
            rc = ib_field_list_add(*fout, ofield);
            if (rc != IB_OK) {
                IB_FTRACE_RET_STATUS(rc);
            }
        }
    }
    else {
        size_t len = 1;
        rc = ib_field_create_ex(
            fout, mp, fin->name, fin->nlen, IB_FTYPE_NUM, &len);
    }

    (*pflags) |= IB_TFN_FMODIFIED;
    IB_FTRACE_RET_STATUS(rc);
}

/**
 * Count transformation
 * @internal
 *
 * @param[in] ib IronBee engine
 * @param[in] mp Memory pool to use for allocations.
 * @param[in] fndata Function specific data.
 * @param[in] fin Input field.
 * @param[out] fout Output field.
 * @param[out] pflags Transformation flags.
 *
 * @returns IB_OK if successful.
 */
static ib_status_t tfn_count(ib_engine_t *ib,
                             ib_mpool_t *mp,
                             void *fndata,
                             ib_field_t *fin,
                             ib_field_t **fout,
                             ib_flags_t *pflags)
{
    IB_FTRACE_INIT();
    ib_status_t rc = IB_OK;
    ib_num_t value = 0;

    assert(mp != NULL);
    assert(fin != NULL);
    assert(fout != NULL);
    assert(pflags != NULL);

    /* If this is a list, return it's count */
    if (fin->type == IB_FTYPE_LIST) {
        // @todo Remove const casting once list is const correct.
        ib_list_t *lst = (ib_list_t *)ib_field_value_list(fin);
        value = IB_LIST_ELEMENTS(lst);
    }
    else {
        value = 1;
    }

    /* Create the output field */
    rc = ib_field_create_ex(
        fout, mp, fin->name, fin->nlen, IB_FTYPE_NUM, &value);

    (*pflags) |= IB_TFN_FMODIFIED;
    IB_FTRACE_RET_STATUS(rc);
}

/**
 * Get maximum of a list of values
 * @internal
 *
 * @param[in] mp Memory pool to use for allocations.
 * @param[in] fin Input field.
 * @param[out] fout Output field.
 *
 * @returns IB_OK if successful.
 */
static ib_status_t max_list(ib_mpool_t *mp,
                            ib_field_t *fin,
                            ib_field_t **fout)
{
    IB_FTRACE_INIT();
    ib_status_t     rc = IB_OK;
    ib_list_node_t *node = NULL;
    ib_num_t        maxvalue = 0;
    ib_num_t        n = 0;
    ib_list_t      *lst;

    assert(mp != NULL);
    assert(fin != NULL);
    assert(fout != NULL);

    /* Get the incoming list */
    // @todo Remove const casting once list is const correct.
    lst = (ib_list_t *)ib_field_value_list(fin);
    if (lst == NULL) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    /* Walk through the incoming fields */
    IB_LIST_LOOP(lst, node) {
        ib_field_t *ifield = (ib_field_t*)node->data;
        ib_num_t value;

        switch (ifield->type) {
            case IB_FTYPE_NUM:
                {
                    const ib_num_t *fval = ib_field_value_num(ifield);
                    value = *fval;
                }
                break;

            case IB_FTYPE_UNUM:
                {
                    const ib_unum_t *fval = ib_field_value_unum(ifield);
                    value = (ib_num_t)(*fval);
                }
                break;

            case IB_FTYPE_NULSTR:
                {
                    const char *fval = ib_field_value_nulstr(ifield);
                    value = (ib_num_t)strlen(fval);
                }
                break;

            case IB_FTYPE_BYTESTR:
                {
                    const ib_bytestr_t *fval = ib_field_value_bytestr(ifield);
                    value = (ib_num_t)ib_bytestr_length(fval);
                }
                break;

            case IB_FTYPE_LIST:
                {
                    ib_field_t *tmp = NULL;
                    const ib_num_t *nptr = NULL;

                    rc = max_list(mp, fin, &tmp);
                    if (rc != IB_OK) {
                        IB_FTRACE_RET_STATUS(rc);
                    }
                    nptr = ib_field_value_num(tmp);
                    if (nptr == NULL) {
                        IB_FTRACE_RET_STATUS(IB_EINVAL);
                    }
                    value = *nptr;
                }
                break;

            default:
                IB_FTRACE_RET_STATUS(IB_EINVAL);
        }
        if ( (n == 0) || (value > maxvalue) ) {
            maxvalue = value;
        }
    }

    /* Create the output field */
    rc = ib_field_create_ex(
        fout, mp, fin->name, fin->nlen, IB_FTYPE_NUM, &maxvalue);

    IB_FTRACE_RET_STATUS(rc);
}

/**
 * Get the minimum of a list of items
 * @internal
 *
 * @param[in] mp Memory pool to use for allocations.
 * @param[in] fin Input field.
 * @param[out] fout Output field.
 *
 * @returns IB_OK if successful.
 */
static ib_status_t min_list(ib_mpool_t *mp,
                            ib_field_t *fin,
                            ib_field_t **fout)
{
    IB_FTRACE_INIT();
    ib_status_t     rc = IB_OK;
    ib_list_node_t *node = NULL;
    ib_num_t        minvalue = 0;
    ib_num_t        n = 0;
    ib_list_t      *lst;

    assert(mp != NULL);
    assert(fin != NULL);
    assert(fout != NULL);

    /* Get the incoming list */
    // @todo Remove const casting once list is const correct.
    lst = (ib_list_t *)ib_field_value_list(fin);
    if (lst == NULL) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    /* Walk through the incoming fields */
    IB_LIST_LOOP(lst, node) {
        ib_field_t *ifield = (ib_field_t*)node->data;
        ib_num_t value;

        switch (fin->type) {
            case IB_FTYPE_NUM:
                {
                    const ib_num_t *fval = ib_field_value_num(ifield);
                    value = *fval;
                }
                break;
            case IB_FTYPE_UNUM:
                {
                    const ib_unum_t *fval = ib_field_value_unum(ifield);
                    value = (ib_num_t)(*fval);
                }
                break;

            case IB_FTYPE_NULSTR:
                {
                    const char *fval = ib_field_value_nulstr(ifield);
                    value = (ib_num_t)strlen(fval);
                }
                break;

            case IB_FTYPE_BYTESTR:
                {
                    const ib_bytestr_t *fval = ib_field_value_bytestr(ifield);
                    value = (ib_num_t)ib_bytestr_length(fval);
                }
                break;

            case IB_FTYPE_LIST:
                {
                    ib_field_t *tmp = NULL;
                    const ib_num_t *nptr = NULL;

                    rc = min_list(mp, fin, &tmp);
                    if (rc != IB_OK) {
                        IB_FTRACE_RET_STATUS(rc);
                    }
                    nptr = ib_field_value_num(tmp);
                    if (nptr == NULL) {
                        IB_FTRACE_RET_STATUS(IB_EINVAL);
                    }
                    value = *nptr;
                }
                break;

            default:
                IB_FTRACE_RET_STATUS(IB_EINVAL);
        }
        if ( (n == 0) || (value < minvalue) ) {
            minvalue = value;
        }
    }

    /* Create the output field */
    rc = ib_field_create_ex(
        fout, mp, fin->name, fin->nlen, IB_FTYPE_NUM, &minvalue);

    IB_FTRACE_RET_STATUS(rc);
}

/**
 * Transformation: Get the max of a list of numbers.
 *
 * @param[in] ib IronBee engine
 * @param[in] mp Memory pool to use for allocations.
 * @param[in] fndata Function specific data.
 * @param[in] fin Input field.
 * @param[out] fout Output field.
 * @param[out] pflags Transformation flags.
 *
 * @returns IB_OK if successful.
 */
static ib_status_t tfn_max(ib_engine_t *ib,
                           ib_mpool_t *mp,
                           void *fndata,
                           ib_field_t *fin,
                           ib_field_t **fout,
                           ib_flags_t *pflags)
{
    IB_FTRACE_INIT();
    ib_status_t rc = IB_OK;

    assert(mp != NULL);
    assert(fin != NULL);
    assert(fout != NULL);
    assert(pflags != NULL);

    switch (fin->type) {
        case IB_FTYPE_NUM:
        case IB_FTYPE_UNUM:
            *fout = fin;
            break;

        case IB_FTYPE_LIST:
            rc = max_list(mp, fin, fout);
            (*pflags) |= IB_TFN_FMODIFIED;
            break;

        default:
            rc = IB_EINVAL;
    }

    IB_FTRACE_RET_STATUS(rc);
}

/**
 * Transformation: Get the min of a list of numbers.
 *
 * @param[in] ib IronBee engine
 * @param[in] mp Memory pool to use for allocations.
 * @param[in] fndata Function specific data.
 * @param[in] fin Input field.
 * @param[out] fout Output field.
 * @param[out] pflags Transformation flags.
 *
 * @returns IB_OK if successful.
 */
static ib_status_t tfn_min(ib_engine_t *ib,
                           ib_mpool_t *mp,
                           void *fndata,
                           ib_field_t *fin,
                           ib_field_t **fout,
                           ib_flags_t *pflags)
{
    IB_FTRACE_INIT();
    ib_status_t rc = IB_OK;

    assert(mp != NULL);
    assert(fin != NULL);
    assert(fout != NULL);
    assert(pflags != NULL);

    switch (fin->type) {
        case IB_FTYPE_NUM:
        case IB_FTYPE_UNUM:
            *fout = fin;
            rc = IB_OK;
            break;

        case IB_FTYPE_LIST:
            rc = min_list(mp, fin, fout);
            (*pflags) |= IB_TFN_FMODIFIED;
            break;

        default:
            rc = IB_EINVAL;
    }

    IB_FTRACE_RET_STATUS(rc);
}

/**
 * Initialize the core transformations
 **/
ib_status_t ib_core_transformations_init(ib_engine_t *ib, ib_module_t *mod)
{
    IB_FTRACE_INIT();
    ib_status_t rc;

    /* Define transformations. */
    rc = ib_tfn_register(ib, "lowercase", tfn_lowercase, NULL);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }
    rc = ib_tfn_register(ib, "lc", tfn_lowercase, NULL);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = ib_tfn_register(ib, "trimLeft", tfn_trim_left, NULL);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = ib_tfn_register(ib, "trimRight", tfn_trim_right, NULL);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = ib_tfn_register(ib, "trim", tfn_trim, NULL);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = ib_tfn_register(ib, "removeWhitespace", tfn_wspc_remove, NULL);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = ib_tfn_register(ib, "wspc_rm", tfn_wspc_remove, NULL);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = ib_tfn_register(ib, "compressWhitespace", tfn_wspc_compress, NULL);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = ib_tfn_register(ib, "wspc_comp", tfn_wspc_compress, NULL);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = ib_tfn_register(ib, "length", tfn_length, NULL);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = ib_tfn_register(ib, "count", tfn_count, NULL);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = ib_tfn_register(ib, "max", tfn_max, NULL);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = ib_tfn_register(ib, "min", tfn_min, NULL);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}
