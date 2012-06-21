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
 * String modification transformation core
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
static ib_status_t tfn_strmod(ib_engine_t *ib,
                              ib_mpool_t *mp,
                              ib_strmod_fn_t str_fn,
                              ib_strmod_ex_fn_t ex_fn,
                              const ib_field_t *fin,
                              ib_field_t **fout,
                              ib_flags_t *pflags)
{
    IB_FTRACE_INIT();
    ib_status_t rc;
    ib_flags_t result;

    assert(ib != NULL);
    assert(mp != NULL);
    assert(str_fn != NULL);
    assert(ex_fn != NULL);
    assert(fin != NULL);
    assert(fout != NULL);
    assert(pflags != NULL);

    /* Initialize the output field pointer */
    *fout = NULL;

    switch(fin->type) {
    case IB_FTYPE_NULSTR :
    {
        const char *in;
        char *out;
        rc = ib_field_value(fin, ib_ftype_nulstr_out(&in));
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }
        if (in == NULL) {
            IB_FTRACE_RET_STATUS(IB_EINVAL);
        }
        rc = str_fn(IB_STROP_COW, mp, (char *)in, &out, &result);
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }
        rc = ib_field_create(fout, mp,
                             fin->name, fin->nlen,
                             IB_FTYPE_NULSTR,
                             ib_ftype_nulstr_in(out));
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }
        break;
    }

    case IB_FTYPE_BYTESTR:
    {
        const ib_bytestr_t *bs;
        const uint8_t *din;
        uint8_t *dout;
        size_t dlen;
        rc = ib_field_value(fin, ib_ftype_bytestr_out(&bs));
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }
        if (bs == NULL) {
            IB_FTRACE_RET_STATUS(IB_EINVAL);
        }
        din = ib_bytestr_const_ptr(bs);
        if (din == NULL) {
            IB_FTRACE_RET_STATUS(IB_EINVAL);
        }
        dlen = ib_bytestr_length(bs);
        rc = ex_fn(IB_STROP_COW, mp,
                   (uint8_t *)din, dlen,
                   &dout, &dlen,
                   &result);
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }
        rc = ib_field_create_bytestr_alias(fout, mp,
                                           fin->name, fin->nlen,
                                           dout, dlen);
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }
        break;
    }
    default:
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    } /* switch(fin->type) */

    /* Check the flags */
    if (ib_flags_all(result, IB_STRFLAG_MODIFIED) == true) {
        *pflags = IB_TFN_FMODIFIED;
    }
    else {
        *pflags = IB_TFN_NONE;
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * Simple ASCII lowercase function.
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
                                 const ib_field_t *fin,
                                 ib_field_t **fout,
                                 ib_flags_t *pflags)
{
    IB_FTRACE_INIT();

    ib_status_t rc = tfn_strmod(ib, mp,
                                ib_strlower, ib_strlower_ex,
                                fin, fout, pflags);

    IB_FTRACE_RET_STATUS(rc);
}

/**
 * Simple ASCII trim (left) transformation.
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
                                 const ib_field_t *fin,
                                 ib_field_t **fout,
                                 ib_flags_t *pflags)
{
    IB_FTRACE_INIT();

    ib_status_t rc = tfn_strmod(ib, mp,
                                ib_strtrim_left, ib_strtrim_left_ex,
                                fin, fout, pflags);

    IB_FTRACE_RET_STATUS(rc);
}

/**
 * Simple ASCII trim (right) transformation.
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
                                  const ib_field_t *fin,
                                  ib_field_t **fout,
                                  ib_flags_t *pflags)
{
    IB_FTRACE_INIT();

    ib_status_t rc = tfn_strmod(ib, mp,
                                ib_strtrim_right, ib_strtrim_right_ex,
                                fin, fout, pflags);

    IB_FTRACE_RET_STATUS(rc);
}

/**
 * Simple ASCII trim transformation.
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
                            const ib_field_t *fin,
                            ib_field_t **fout,
                            ib_flags_t *pflags)
{
    IB_FTRACE_INIT();

    ib_status_t rc = tfn_strmod(ib, mp,
                                ib_strtrim_lr, ib_strtrim_lr_ex,
                                fin, fout, pflags);

    IB_FTRACE_RET_STATUS(rc);
}

/**
 * Remove all whitespace from a string
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
                                   const ib_field_t *fin,
                                   ib_field_t **fout,
                                   ib_flags_t *pflags)
{
    IB_FTRACE_INIT();

    ib_status_t rc = tfn_strmod(ib, mp,
                                ib_str_wspc_remove, ib_str_wspc_remove_ex,
                                fin, fout, pflags);

    IB_FTRACE_RET_STATUS(rc);
}

/**
 * Compress whitespace in a string
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
                                     const ib_field_t *fin,
                                     ib_field_t **fout,
                                     ib_flags_t *pflags)
{
    IB_FTRACE_INIT();

    ib_status_t rc = tfn_strmod(ib, mp,
                                ib_str_wspc_compress, ib_str_wspc_compress_ex,
                                fin, fout, pflags);

    IB_FTRACE_RET_STATUS(rc);
}

/**
 * Length transformation
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
                              const ib_field_t *fin,
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
        const char *fval;
        rc = ib_field_value(fin, ib_ftype_nulstr_out(&fval));
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }

        const ib_unum_t len = strlen(fval);
        rc = ib_field_create(
            fout, mp,
            IB_FIELD_NAME("Length"),
            IB_FTYPE_UNUM,
            ib_ftype_unum_in(&len)
        );
    }
    else if (fin->type == IB_FTYPE_BYTESTR) {
        const ib_bytestr_t *value;
        rc = ib_field_value(fin, ib_ftype_bytestr_out(&value));
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }

        const ib_unum_t len = ib_bytestr_length(value);
        rc = ib_field_create(
            fout, mp,
            IB_FIELD_NAME("Length"),
            IB_FTYPE_UNUM,
            ib_ftype_unum_in(&len)
        );
    }
    else if (fin->type == IB_FTYPE_LIST) {
        const ib_list_node_t  *node = NULL;
        const ib_list_t       *ilist;        /** Incoming list */

        /* Get the incoming list */
        // @todo Remove mutable once list is const correct.
        rc = ib_field_value(fin, ib_ftype_list_out(&ilist));
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }

        if (ilist == NULL) {
            IB_FTRACE_RET_STATUS(IB_EUNKNOWN);
        }

        /* Create the outgoing list field */
        rc = ib_field_create(
            fout, mp,
            IB_FIELD_NAME("Length"),
            IB_FTYPE_LIST, NULL
        );
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }

        /* Walk through the incoming fields */
        IB_LIST_LOOP_CONST(ilist, node) {
            const ib_field_t *ifield = (ib_field_t *)node->data;
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
        const ib_unum_t len = 1;
        rc = ib_field_create(
            fout, mp, fin->name, fin->nlen, IB_FTYPE_UNUM,
            ib_ftype_unum_in(&len)
        );
    }

    (*pflags) |= IB_TFN_FMODIFIED;
    IB_FTRACE_RET_STATUS(rc);
}

/**
 * Count transformation
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
                             const ib_field_t *fin,
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
        // @todo Remove mutable once list is const correct.
        const ib_list_t *lst;
        rc = ib_field_value(fin, ib_ftype_list_out(&lst));
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }

        value = IB_LIST_ELEMENTS(lst);
    }
    else {
        value = 1;
    }

    /* Create the output field */
    rc = ib_field_create(
        fout, mp, fin->name, fin->nlen, IB_FTYPE_NUM,
        ib_ftype_num_in(&value)
    );

    (*pflags) |= IB_TFN_FMODIFIED;
    IB_FTRACE_RET_STATUS(rc);
}

/**
 * Get maximum / minimum of a list of values
 *
 * @param[in] is_max true for max, false for min
 * @param[in] mp Memory pool to use for allocations.
 * @param[in] fin Input field.
 * @param[out] fout Output field.
 *
 * @returns IB_OK if successful.
 */
static ib_status_t list_minmax(bool is_max,
                               ib_mpool_t *mp,
                               const ib_field_t *fin,
                               ib_field_t **fout)
{
    IB_FTRACE_INIT();
    ib_status_t           rc = IB_OK;
    const ib_list_t      *lst;
    const ib_list_node_t *node = NULL;
    bool             first = true;
    ib_num_t              mmvalue = 0;     /* Current Min / max value */

    assert(mp != NULL);
    assert(fin != NULL);
    assert(fout != NULL);

    /* Get the incoming list */
    // @todo Remove mutable once list is const correct.
    rc = ib_field_value(fin, ib_ftype_list_out(&lst));
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    if (lst == NULL) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    /* Walk through the incoming fields */
    IB_LIST_LOOP_CONST(lst, node) {
        const ib_field_t *ifield = (ib_field_t *)node->data;
        ib_num_t value;

        switch (ifield->type) {
        case IB_FTYPE_NUM:
        {
            ib_num_t fval;
            rc = ib_field_value(ifield, ib_ftype_num_out(&fval));
            if (rc != IB_OK) {
                IB_FTRACE_RET_STATUS(rc);
            }

            value = fval;
            break;
        }

        case IB_FTYPE_UNUM:
        {
            ib_unum_t fval;
            rc = ib_field_value(ifield, ib_ftype_unum_out(&fval));
            if (rc != IB_OK) {
                IB_FTRACE_RET_STATUS(rc);
            }

            if (fval > INT64_MAX) {
                IB_FTRACE_RET_STATUS(IB_EOTHER);
            }
            value = (ib_num_t)fval;
            break;
        }

        case IB_FTYPE_NULSTR:
        {
            const char *fval;
            rc = ib_field_value(ifield, ib_ftype_nulstr_out(&fval));
            if (rc != IB_OK) {
                IB_FTRACE_RET_STATUS(rc);
            }

            value = (ib_num_t)strlen(fval);
            break;
        }

        case IB_FTYPE_BYTESTR:
        {
            const ib_bytestr_t *fval;
            rc = ib_field_value(ifield, ib_ftype_bytestr_out(&fval));
            if (rc != IB_OK) {
                IB_FTRACE_RET_STATUS(rc);
            }

            value = (ib_num_t)ib_bytestr_length(fval);
            break;
        }

        case IB_FTYPE_LIST:
        {
            ib_field_t *tmp = NULL;
            ib_num_t v;

            rc = list_minmax(is_max, mp, fin, &tmp);
            if (rc != IB_OK) {
                IB_FTRACE_RET_STATUS(rc);
            }
            rc = ib_field_value(tmp, ib_ftype_num_out(&v));
            if (rc != IB_OK) {
                IB_FTRACE_RET_STATUS(rc);
            }
            value = v;
            break;
        }

        default:
            IB_FTRACE_RET_STATUS(IB_EINVAL);
        }

        if ( (first == true) ||
             ( ((is_max == true) && (value > mmvalue)) ||
               ((is_max == false) && (value < mmvalue)) ) )
        {
            first = false;
            mmvalue = value;
        }
    }

    /* Create the output field */
    rc = ib_field_create(fout, mp,
                         fin->name, fin->nlen,
                         IB_FTYPE_NUM, ib_ftype_num_in(&mmvalue));

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
                           const ib_field_t *fin,
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
            *fout = (ib_field_t *)fin;
            break;

        case IB_FTYPE_LIST:
            rc = list_minmax(true, mp, fin, fout);
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
                           const ib_field_t *fin,
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
            *fout = (ib_field_t *)fin;
            rc = IB_OK;
            break;

        case IB_FTYPE_LIST:
            rc = list_minmax(true, mp, fin, fout);
            (*pflags) |= IB_TFN_FMODIFIED;
            break;

        default:
            rc = IB_EINVAL;
    }

    IB_FTRACE_RET_STATUS(rc);
}

/**
 * URL Decode transformation
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
static ib_status_t tfn_url_decode(ib_engine_t *ib,
                                  ib_mpool_t *mp,
                                  void *fndata,
                                  const ib_field_t *fin,
                                  ib_field_t **fout,
                                  ib_flags_t *pflags)
{
    IB_FTRACE_INIT();
    ib_status_t rc;
    ib_flags_t result;

    assert(ib != NULL);
    assert(mp != NULL);
    assert(fin != NULL);
    assert(fout != NULL);
    assert(pflags != NULL);

    /* Initialize the output field pointer */
    *fout = NULL;

    switch(fin->type) {
    case IB_FTYPE_NULSTR :
    {
        const char *in;
        char *out;
        rc = ib_field_value(fin, ib_ftype_nulstr_out(&in));
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }
        if (in == NULL) {
            IB_FTRACE_RET_STATUS(IB_EINVAL);
        }
        rc = ib_util_decode_url_cow(mp, (char *)in, &out, &result);
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }
        rc = ib_field_create(fout, mp,
                             fin->name, fin->nlen,
                             IB_FTYPE_NULSTR,
                             ib_ftype_nulstr_in(out));
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }
        break;
    }

    case IB_FTYPE_BYTESTR:
    {
        const ib_bytestr_t *bs;
        const uint8_t *din;
        uint8_t *dout;
        size_t dlen;
        rc = ib_field_value(fin, ib_ftype_bytestr_out(&bs));
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }
        if (bs == NULL) {
            IB_FTRACE_RET_STATUS(IB_EINVAL);
        }
        din = ib_bytestr_const_ptr(bs);
        if (din == NULL) {
            IB_FTRACE_RET_STATUS(IB_EINVAL);
        }
        dlen = ib_bytestr_length(bs);
        rc = ib_util_decode_url_cow_ex(mp,
                                       din, dlen,
                                       &dout, &dlen,
                                       &result);
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }
        rc = ib_field_create_bytestr_alias(fout, mp,
                                           fin->name, fin->nlen,
                                           dout, dlen);
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }
        break;
    }
    default:
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    } /* switch(fin->type) */

    /* Check the flags */
    if (ib_flags_all(result, IB_STRFLAG_MODIFIED) == true) {
        *pflags = IB_TFN_FMODIFIED;
    }
    else {
        *pflags = IB_TFN_NONE;
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * HTML entity decode transformation
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
static ib_status_t tfn_html_entity_decode(ib_engine_t *ib,
                                          ib_mpool_t *mp,
                                          void *fndata,
                                          const ib_field_t *fin,
                                          ib_field_t **fout,
                                          ib_flags_t *pflags)
{
    IB_FTRACE_INIT();
    ib_status_t rc;
    ib_flags_t result;

    assert(ib != NULL);
    assert(mp != NULL);
    assert(fin != NULL);
    assert(fout != NULL);
    assert(pflags != NULL);

    /* Initialize the output field pointer */
    *fout = NULL;

    switch(fin->type) {
    case IB_FTYPE_NULSTR :
    {
        const char *in;
        char *out;
        rc = ib_field_value(fin, ib_ftype_nulstr_out(&in));
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }
        if (in == NULL) {
            IB_FTRACE_RET_STATUS(IB_EINVAL);
        }
        rc = ib_util_decode_html_entity_cow(mp, (char *)in, &out, &result);
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }
        rc = ib_field_create(fout, mp,
                             fin->name, fin->nlen,
                             IB_FTYPE_NULSTR,
                             ib_ftype_nulstr_in(out));
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }
        break;
    }

    case IB_FTYPE_BYTESTR:
    {
        const ib_bytestr_t *bs;
        const uint8_t *din;
        uint8_t *dout;
        size_t dlen;
        rc = ib_field_value(fin, ib_ftype_bytestr_out(&bs));
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }
        if (bs == NULL) {
            IB_FTRACE_RET_STATUS(IB_EINVAL);
        }
        din = ib_bytestr_const_ptr(bs);
        if (din == NULL) {
            IB_FTRACE_RET_STATUS(IB_EINVAL);
        }
        dlen = ib_bytestr_length(bs);
        rc = ib_util_decode_html_entity_cow_ex(mp,
                                               din, dlen,
                                               &dout, &dlen,
                                               &result);
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }
        rc = ib_field_create_bytestr_alias(fout, mp,
                                           fin->name, fin->nlen,
                                           dout, dlen);
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }
        break;
    }
    default:
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    } /* switch(fin->type) */

    /* Check the flags */
    if (ib_flags_all(result, IB_STRFLAG_MODIFIED) == true) {
        *pflags = IB_TFN_FMODIFIED;
    }
    else {
        *pflags = IB_TFN_NONE;
    }

    IB_FTRACE_RET_STATUS(IB_OK);
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

    rc = ib_tfn_register(ib, "compressWhitespace", tfn_wspc_compress, NULL);
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

    rc = ib_tfn_register(ib, "urlDecode", tfn_url_decode, NULL);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = ib_tfn_register(ib, "htmlEntityDecode", tfn_html_entity_decode, NULL);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}
