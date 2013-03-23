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
 * @brief IronBee
 *
 * @author Nick LeRoy <nleroy@qualys.com>
 */

#include "ironbee_config_auto.h"

#include "core_private.h"

#include <ironbee/bytestr.h>
#include <ironbee/decode.h>
#include <ironbee/engine.h>
#include <ironbee/field.h>
#include <ironbee/flags.h>
#include <ironbee/mpool.h>
#include <ironbee/operator.h>
#include <ironbee/path.h>
#include <ironbee/string.h>
#include <ironbee/transformation.h>
#include <ironbee/util.h>

#include <assert.h>
#include <ctype.h>

/**
 * String modification transformation core
 *
 * @param[in] ib IronBee engine
 * @param[in] mp Memory pool to use for allocations.
 * @param[in] str_fn NUL-terminated string transformation function
 * @param[in] ex_fn EX (string/length) transformation function
 * @param[in] fin Input field.
 * @param[out] fout Output field.
 * @param[out] pflags Transformation flags.
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
            return rc;
        }
        if (in == NULL) {
            return IB_EINVAL;
        }
        rc = str_fn(IB_STROP_COW, mp, (char *)in, &out, &result);
        if (rc != IB_OK) {
            return rc;
        }
        rc = ib_field_create(fout, mp,
                             fin->name, fin->nlen,
                             IB_FTYPE_NULSTR,
                             ib_ftype_nulstr_in(out));
        if (rc != IB_OK) {
            return rc;
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
            return rc;
        }
        if (bs == NULL) {
            return IB_EINVAL;
        }
        din = ib_bytestr_const_ptr(bs);
        if (din == NULL) {
            return IB_EINVAL;
        }
        dlen = ib_bytestr_length(bs);
        rc = ex_fn(IB_STROP_COW, mp,
                   (uint8_t *)din, dlen,
                   &dout, &dlen,
                   &result);
        if (rc != IB_OK) {
            return rc;
        }
        rc = ib_field_create_bytestr_alias(fout, mp,
                                           fin->name, fin->nlen,
                                           dout, dlen);
        if (rc != IB_OK) {
            return rc;
        }
        break;
    }

    default:
        return IB_EINVAL;
    } /* switch(fin->type) */

    /* Check the flags */
    if (ib_flags_all(result, IB_STRFLAG_MODIFIED)) {
        *pflags = IB_TFN_FMODIFIED;
    }
    else {
        *pflags = IB_TFN_NONE;
    }

    return IB_OK;
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
 * @returns IB_OK if successful.
 */
static ib_status_t tfn_lowercase(ib_engine_t *ib,
                                 ib_mpool_t *mp,
                                 void *fndata,
                                 const ib_field_t *fin,
                                 ib_field_t **fout,
                                 ib_flags_t *pflags)
{
    ib_status_t rc = tfn_strmod(ib, mp,
                                ib_strlower, ib_strlower_ex,
                                fin, fout, pflags);

    return rc;
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
    ib_status_t rc = tfn_strmod(ib, mp,
                                ib_strtrim_left, ib_strtrim_left_ex,
                                fin, fout, pflags);

    return rc;
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
    ib_status_t rc = tfn_strmod(ib, mp,
                                ib_strtrim_right, ib_strtrim_right_ex,
                                fin, fout, pflags);

    return rc;
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
    ib_status_t rc = tfn_strmod(ib, mp,
                                ib_strtrim_lr, ib_strtrim_lr_ex,
                                fin, fout, pflags);

    return rc;
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
    ib_status_t rc = tfn_strmod(ib, mp,
                                ib_str_wspc_remove, ib_str_wspc_remove_ex,
                                fin, fout, pflags);

    return rc;
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
    ib_status_t rc = tfn_strmod(ib, mp,
                                ib_str_wspc_compress, ib_str_wspc_compress_ex,
                                fin, fout, pflags);

    return rc;
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
            return rc;
        }

        const ib_num_t len = strlen(fval);
        rc = ib_field_create(
            fout, mp,
            IB_FIELD_NAME("Length"),
            IB_FTYPE_NUM,
            ib_ftype_num_in(&len)
        );
    }
    else if (fin->type == IB_FTYPE_BYTESTR) {
        const ib_bytestr_t *value;
        rc = ib_field_value(fin, ib_ftype_bytestr_out(&value));
        if (rc != IB_OK) {
            return rc;
        }

        const ib_num_t len = ib_bytestr_length(value);
        rc = ib_field_create(
            fout, mp,
            IB_FIELD_NAME("Length"),
            IB_FTYPE_NUM,
            ib_ftype_num_in(&len)
        );
    }
    else if (fin->type == IB_FTYPE_LIST) {
        const ib_list_node_t  *node = NULL;
        const ib_list_t       *ilist;        /** Incoming list */

        /* Get the incoming list */
        // @todo Remove mutable once list is const correct.
        rc = ib_field_value(fin, ib_ftype_list_out(&ilist));
        if (rc != IB_OK) {
            return rc;
        }

        if (ilist == NULL) {
            return IB_EUNKNOWN;
        }

        /* Create the outgoing list field */
        rc = ib_field_create(
            fout, mp,
            IB_FIELD_NAME("Length"),
            IB_FTYPE_LIST, NULL
        );
        if (rc != IB_OK) {
            return rc;
        }

        /* Walk through the incoming fields */
        IB_LIST_LOOP_CONST(ilist, node) {
            const ib_field_t *ifield = (ib_field_t *)node->data;
            ib_field_t *ofield = NULL;
            ib_flags_t flags = 0;

            rc = tfn_length(ib, mp, NULL, ifield, &ofield, &flags);
            if (rc != IB_OK) {
                return rc;
            }
            rc = ib_field_list_add(*fout, ofield);
            if (rc != IB_OK) {
                return rc;
            }
        }
    }
    else {
        const ib_num_t len = 1;
        rc = ib_field_create(
            fout, mp, fin->name, fin->nlen, IB_FTYPE_NUM,
            ib_ftype_num_in(&len)
        );
    }

    (*pflags) |= IB_TFN_FMODIFIED;
    return rc;
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
    ib_status_t rc = IB_OK;
    ib_num_t value = 0;

    assert(mp != NULL);
    assert(fin != NULL);
    assert(fout != NULL);
    assert(pflags != NULL);

    /* If this is a list, return it's count */
    if (fin->type == IB_FTYPE_LIST) {
        const ib_list_t *lst;
        rc = ib_field_value(fin, ib_ftype_list_out(&lst));
        if (rc != IB_OK) {
            return rc;
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
    return rc;
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
        return rc;
    }

    if (lst == NULL) {
        return IB_EINVAL;
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
                return rc;
            }

            value = fval;
            break;
        }

        case IB_FTYPE_NULSTR:
        {
            const char *fval;
            rc = ib_field_value(ifield, ib_ftype_nulstr_out(&fval));
            if (rc != IB_OK) {
                return rc;
            }

            value = (ib_num_t)strlen(fval);
            break;
        }

        case IB_FTYPE_BYTESTR:
        {
            const ib_bytestr_t *fval;
            rc = ib_field_value(ifield, ib_ftype_bytestr_out(&fval));
            if (rc != IB_OK) {
                return rc;
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
                return rc;
            }
            rc = ib_field_value(tmp, ib_ftype_num_out(&v));
            if (rc != IB_OK) {
                return rc;
            }
            value = v;
            break;
        }

        default:
            return IB_EINVAL;
        }

        if ( first ||
             ( (is_max && (value > mmvalue)) ||
               ((! is_max) && (value < mmvalue)) ) )
        {
            first = false;
            mmvalue = value;
        }
    }

    /* Create the output field */
    rc = ib_field_create(fout, mp,
                         fin->name, fin->nlen,
                         IB_FTYPE_NUM, ib_ftype_num_in(&mmvalue));

    return rc;
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
    ib_status_t rc = IB_OK;

    assert(mp != NULL);
    assert(fin != NULL);
    assert(fout != NULL);
    assert(pflags != NULL);

    switch (fin->type) {
        case IB_FTYPE_NUM:
            *fout = (ib_field_t *)fin;
            break;

        case IB_FTYPE_LIST:
            rc = list_minmax(true, mp, fin, fout);
            (*pflags) |= IB_TFN_FMODIFIED;
            break;

        default:
            rc = IB_EINVAL;
    }

    return rc;
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
    ib_status_t rc = IB_OK;

    assert(mp != NULL);
    assert(fin != NULL);
    assert(fout != NULL);
    assert(pflags != NULL);

    switch (fin->type) {
        case IB_FTYPE_NUM:
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

    return rc;
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
 * @returns IB_OK if successful.
 */
static ib_status_t tfn_url_decode(ib_engine_t *ib,
                                  ib_mpool_t *mp,
                                  void *fndata,
                                  const ib_field_t *fin,
                                  ib_field_t **fout,
                                  ib_flags_t *pflags)
{
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
            return rc;
        }
        if (in == NULL) {
            return IB_EINVAL;
        }
        rc = ib_util_decode_url_cow(mp, (char *)in, &out, &result);
        if (rc != IB_OK) {
            return rc;
        }
        rc = ib_field_create(fout, mp,
                             fin->name, fin->nlen,
                             IB_FTYPE_NULSTR,
                             ib_ftype_nulstr_in(out));
        if (rc != IB_OK) {
            return rc;
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
            return rc;
        }
        if (bs == NULL) {
            return IB_EINVAL;
        }
        din = ib_bytestr_const_ptr(bs);
        if (din == NULL) {
            return IB_EINVAL;
        }
        dlen = ib_bytestr_length(bs);
        rc = ib_util_decode_url_cow_ex(mp,
                                       din, dlen, false,
                                       &dout, &dlen,
                                       &result);
        if (rc != IB_OK) {
            return rc;
        }
        rc = ib_field_create_bytestr_alias(fout, mp,
                                           fin->name, fin->nlen,
                                           dout, dlen);
        if (rc != IB_OK) {
            return rc;
        }
        break;
    }
    default:
        return IB_EINVAL;
    } /* switch(fin->type) */

    /* Check the flags */
    if (ib_flags_all(result, IB_STRFLAG_MODIFIED)) {
        *pflags = IB_TFN_FMODIFIED;
    }
    else {
        *pflags = IB_TFN_NONE;
    }

    return IB_OK;
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
 * @returns IB_OK if successful.
 */
static ib_status_t tfn_html_entity_decode(ib_engine_t *ib,
                                          ib_mpool_t *mp,
                                          void *fndata,
                                          const ib_field_t *fin,
                                          ib_field_t **fout,
                                          ib_flags_t *pflags)
{
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
            return rc;
        }
        if (in == NULL) {
            return IB_EINVAL;
        }
        rc = ib_util_decode_html_entity_cow(mp, (char *)in, &out, &result);
        if (rc != IB_OK) {
            return rc;
        }
        rc = ib_field_create(fout, mp,
                             fin->name, fin->nlen,
                             IB_FTYPE_NULSTR,
                             ib_ftype_nulstr_in(out));
        if (rc != IB_OK) {
            return rc;
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
            return rc;
        }
        if (bs == NULL) {
            return IB_EINVAL;
        }
        din = ib_bytestr_const_ptr(bs);
        if (din == NULL) {
            return IB_EINVAL;
        }
        dlen = ib_bytestr_length(bs);
        rc = ib_util_decode_html_entity_cow_ex(mp,
                                               din, dlen,
                                               &dout, &dlen,
                                               &result);
        if (rc != IB_OK) {
            return rc;
        }
        rc = ib_field_create_bytestr_alias(fout, mp,
                                           fin->name, fin->nlen,
                                           dout, dlen);
        if (rc != IB_OK) {
            return rc;
        }
        break;
    }
    default:
        return IB_EINVAL;
    } /* switch(fin->type) */

    /* Check the flags */
    if (ib_flags_all(result, IB_STRFLAG_MODIFIED)) {
        *pflags = IB_TFN_FMODIFIED;
    }
    else {
        *pflags = IB_TFN_NONE;
    }

    return IB_OK;
}

/**
 * Path normalization transformation
 *
 * @param[in] ib IronBee engine
 * @param[in] mp Memory pool to use for allocations.
 * @param[in] fin Input field.
 * @param[in] win Handle windows-style '\'?
 * @param[out] fout Output field.
 * @param[out] pflags Transformation flags.
 *
 * @returns IB_OK if successful.
 */
static ib_status_t normalize_path(ib_engine_t *ib,
                                  ib_mpool_t *mp,
                                  const ib_field_t *fin,
                                  bool win,
                                  ib_field_t **fout,
                                  ib_flags_t *pflags)
{
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
            return rc;
        }
        if (in == NULL) {
            return IB_EINVAL;
        }
        rc = ib_util_normalize_path_cow(mp, in, win, &out, &result);
        if (rc != IB_OK) {
            return rc;
        }
        rc = ib_field_create(fout, mp,
                             fin->name, fin->nlen,
                             IB_FTYPE_NULSTR,
                             ib_ftype_nulstr_in(out));
        if (rc != IB_OK) {
            return rc;
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
            return rc;
        }
        if (bs == NULL) {
            return IB_EINVAL;
        }
        din = ib_bytestr_const_ptr(bs);
        if (din == NULL) {
            return IB_EINVAL;
        }
        dlen = ib_bytestr_length(bs);
        rc = ib_util_normalize_path_cow_ex(mp,
                                           din, dlen, win,
                                           &dout, &dlen,
                                           &result);
        if (rc != IB_OK) {
            return rc;
        }
        rc = ib_field_create_bytestr_alias(fout, mp,
                                           fin->name, fin->nlen,
                                           dout, dlen);
        if (rc != IB_OK) {
            return rc;
        }
        break;
    }
    default:
        return IB_EINVAL;
    } /* switch(fin->type) */

    /* Check the flags */
    if (ib_flags_all(result, IB_STRFLAG_MODIFIED)) {
        *pflags = IB_TFN_FMODIFIED;
    }
    else {
        *pflags = IB_TFN_NONE;
    }

    return IB_OK;
}

/**
 * Path normalization transformation
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
static ib_status_t tfn_normalize_path(ib_engine_t *ib,
                                      ib_mpool_t *mp,
                                      void *fndata,
                                      const ib_field_t *fin,
                                      ib_field_t **fout,
                                      ib_flags_t *pflags)
{
    ib_status_t rc;

    assert(ib != NULL);
    assert(mp != NULL);
    assert(fin != NULL);
    assert(fout != NULL);
    assert(pflags != NULL);

    rc = normalize_path(ib, mp, fin, false, fout, pflags);
    return rc;
}

/**
 * Path normalization transformation with support for Windows path separator
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
static ib_status_t tfn_normalize_path_win(ib_engine_t *ib,
                                          ib_mpool_t *mp,
                                          void *fndata,
                                          const ib_field_t *fin,
                                          ib_field_t **fout,
                                          ib_flags_t *pflags)
{
    ib_status_t rc;

    assert(ib != NULL);
    assert(mp != NULL);
    assert(fin != NULL);
    assert(fout != NULL);
    assert(pflags != NULL);

    rc = normalize_path(ib, mp, fin, true, fout, pflags);
    return rc;
}

/**
 * Initialize the core transformations
 **/
ib_status_t ib_core_transformations_init(ib_engine_t *ib, ib_module_t *mod)
{
    ib_status_t rc;

    /* Define transformations. */
    rc = ib_tfn_register(ib, "lowercase", tfn_lowercase,
                         IB_TFN_FLAG_NONE, NULL);
    if (rc != IB_OK) {
        return rc;
    }
    rc = ib_tfn_register(ib, "lc", tfn_lowercase,
                         IB_TFN_FLAG_NONE, NULL);
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_tfn_register(ib, "trimLeft", tfn_trim_left,
                         IB_TFN_FLAG_NONE, NULL);
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_tfn_register(ib, "trimRight", tfn_trim_right,
                         IB_TFN_FLAG_NONE, NULL);
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_tfn_register(ib, "trim", tfn_trim,
                         IB_TFN_FLAG_NONE, NULL);
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_tfn_register(ib, "removeWhitespace", tfn_wspc_remove,
                         IB_TFN_FLAG_NONE, NULL);
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_tfn_register(ib, "compressWhitespace", tfn_wspc_compress,
                         IB_TFN_FLAG_NONE, NULL);
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_tfn_register(ib, "length", tfn_length,
                         IB_TFN_FLAG_HANDLE_LIST, NULL);
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_tfn_register(ib, "count", tfn_count,
                         IB_TFN_FLAG_HANDLE_LIST, NULL);
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_tfn_register(ib, "max", tfn_max,
                         IB_TFN_FLAG_HANDLE_LIST, NULL);
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_tfn_register(ib, "min", tfn_min,
                         IB_TFN_FLAG_HANDLE_LIST, NULL);
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_tfn_register(ib, "urlDecode", tfn_url_decode,
                         IB_TFN_FLAG_NONE, NULL);
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_tfn_register(ib, "htmlEntityDecode", tfn_html_entity_decode,
                         IB_TFN_FLAG_NONE, NULL);
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_tfn_register(ib, "normalizePath", tfn_normalize_path,
                         IB_TFN_FLAG_NONE, NULL);
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_tfn_register(ib, "normalizePathWin", tfn_normalize_path_win,
                         IB_TFN_FLAG_NONE, NULL);
    if (rc != IB_OK) {
        return rc;
    }

    return IB_OK;
}
