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
#include <math.h>

/**
 * String modification transformation core
 *
 * @param[in] ib IronBee engine
 * @param[in] mp Memory pool to use for allocations.
 * @param[in] str_fn NUL-terminated string transformation function
 * @param[in] ex_fn EX (string/length) transformation function
 * @param[in] fin Input field.
 * @param[out] fout Output field. This is NULL on error.
 * @param[out] pflags Transformation flags.
 *
 * @returns IB_OK if successful.
 */
static ib_status_t tfn_strmod(ib_engine_t *ib,
                              ib_mpool_t *mp,
                              ib_strmod_fn_t str_fn,
                              ib_strmod_ex_fn_t ex_fn,
                              const ib_field_t *fin,
                              const ib_field_t **fout,
                              ib_flags_t *pflags)
{
    ib_status_t rc;
    ib_flags_t result;
    ib_field_t *fnew;

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
        rc = ib_field_create(&fnew, mp,
                             fin->name, fin->nlen,
                             IB_FTYPE_NULSTR,
                             ib_ftype_nulstr_in(out));
        if (rc != IB_OK) {
            return rc;
        }
        *fout = fnew;
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
        rc = ib_field_create_bytestr_alias(&fnew, mp,
                                           fin->name, fin->nlen,
                                           dout, dlen);
        if (rc != IB_OK) {
            return rc;
        }
        *fout = fnew;
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
 * @param[out] fout Output field. This is NULL on error.
 * @param[out] pflags Transformation flags.
 *
 * @returns IB_OK if successful.
 */
static ib_status_t tfn_lowercase(ib_engine_t *ib,
                                 ib_mpool_t *mp,
                                 void *fndata,
                                 const ib_field_t *fin,
                                 const ib_field_t **fout,
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
 * @param[out] fout Output field. This is NULL on error.
 * @param[out] pflags Transformation flags.
 *
 * @returns IB_OK if successful.
 */
static ib_status_t tfn_trim_left(ib_engine_t *ib,
                                 ib_mpool_t *mp,
                                 void *fndata,
                                 const ib_field_t *fin,
                                 const ib_field_t **fout,
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
 * @param[out] fout Output field. This is NULL on error.
 * @param[out] pflags Transformation flags.
 *
 * @returns IB_OK if successful.
 */
static ib_status_t tfn_trim_right(ib_engine_t *ib,
                                  ib_mpool_t *mp,
                                  void *fndata,
                                  const ib_field_t *fin,
                                  const ib_field_t **fout,
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
 * @param[out] fout Output field. This is NULL on error.
 * @param[out] pflags Transformation flags.
 *
 * @returns IB_OK if successful.
 */
static ib_status_t tfn_trim(ib_engine_t *ib,
                            ib_mpool_t *mp,
                            void *fndata,
                            const ib_field_t *fin,
                            const ib_field_t **fout,
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
 * @param[out] fout Output field. This is NULL on error.
 * @param[out] pflags Transformation flags.
 *
 * @returns IB_OK if successful.
 */
static ib_status_t tfn_wspc_remove(ib_engine_t *ib,
                                   ib_mpool_t *mp,
                                   void *fndata,
                                   const ib_field_t *fin,
                                   const ib_field_t **fout,
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
 * @param[out] fout Output field. This is NULL on error.
 * @param[out] pflags Transformation flags.
 *
 * @returns IB_OK if successful.
 */
static ib_status_t tfn_wspc_compress(ib_engine_t *ib,
                                     ib_mpool_t *mp,
                                     void *fndata,
                                     const ib_field_t *fin,
                                     const ib_field_t **fout,
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
 * @param[out] fout Output field. This is NULL on error.
 * @param[out] pflags Transformation flags.
 *
 * @returns IB_OK if successful.
 */
static ib_status_t tfn_length(ib_engine_t *ib,
                              ib_mpool_t *mp,
                              void *fndata,
                              const ib_field_t *fin,
                              const ib_field_t **fout,
                              ib_flags_t *pflags)
{
    assert(mp != NULL);
    assert(fin != NULL);
    assert(fout != NULL);
    assert(pflags != NULL);

    ib_status_t rc = IB_OK;
    ib_field_t *fnew;

    /* Initialize the output field pointer */
    *fout = NULL;

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
            &fnew, mp,
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
            &fnew, mp,
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
            &fnew, mp,
            IB_FIELD_NAME("Length"),
            IB_FTYPE_LIST, NULL
        );
        if (rc != IB_OK) {
            return rc;
        }

        /* Walk through the incoming fields */
        IB_LIST_LOOP_CONST(ilist, node) {
            const ib_field_t *ifield = (ib_field_t *)node->data;
            const ib_field_t *ofield = NULL;
            ib_flags_t flags = 0;

            rc = tfn_length(ib, mp, NULL, ifield, &ofield, &flags);
            if (rc != IB_OK) {
                return rc;
            }
            rc = ib_field_list_add_const(fnew, ofield);
            if (rc != IB_OK) {
                return rc;
            }
        }
    }
    else {
        const ib_num_t len = 1;
        rc = ib_field_create(
            &fnew, mp, fin->name, fin->nlen, IB_FTYPE_NUM,
            ib_ftype_num_in(&len)
        );
    }

    if (rc == IB_OK) {
        (*pflags) |= IB_TFN_FMODIFIED;
        *fout = fnew;
    }
    return rc;
}

/**
 * Count transformation
 *
 * @param[in] ib IronBee engine
 * @param[in] mp Memory pool to use for allocations.
 * @param[in] fndata Function specific data.
 * @param[in] fin Input field.
 * @param[out] fout Output field. This is NULL on error.
 * @param[out] pflags Transformation flags.
 *
 * @returns IB_OK if successful.
 */
static ib_status_t tfn_count(ib_engine_t *ib,
                             ib_mpool_t *mp,
                             void *fndata,
                             const ib_field_t *fin,
                             const ib_field_t **fout,
                             ib_flags_t *pflags)
{
    assert(mp != NULL);
    assert(fin != NULL);
    assert(fout != NULL);
    assert(pflags != NULL);

    ib_status_t rc = IB_OK;
    ib_num_t value = 0;
    ib_field_t *fnew;

    /* Initialize the output field pointer */
    *fout = NULL;

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
        &fnew, mp, fin->name, fin->nlen, IB_FTYPE_NUM,
        ib_ftype_num_in(&value)
    );

    if (rc == IB_OK) {
        (*pflags) |= IB_TFN_FMODIFIED;
        *fout = fnew;
    }

    return rc;
}

/**
 * Get maximum / minimum of a list of values
 *
 * @param[in] is_max true for max, false for min
 * @param[in] mp Memory pool to use for allocations.
 * @param[in] fin Input field.
 * @param[out] fout Output field. This is NULL on error.
 *
 * @returns IB_OK if successful.
 */
static ib_status_t list_minmax(bool is_max,
                               ib_mpool_t *mp,
                               const ib_field_t *fin,
                               const ib_field_t **fout)
{
    assert(mp != NULL);
    assert(fin != NULL);
    assert(fout != NULL);

    ib_status_t           rc = IB_OK;
    ib_field_t           *fnew;
    const ib_list_t      *lst;
    const ib_list_node_t *node = NULL;
    bool                  first = true;
    ib_num_t              mmvalue = 0;     /* Current Min / max value */

    /* Initialize the output field pointer */
    *fout = NULL;

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
            const ib_field_t *tmp = NULL;
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
    rc = ib_field_create(&fnew, mp,
                         fin->name, fin->nlen,
                         IB_FTYPE_NUM, ib_ftype_num_in(&mmvalue));

    if (rc == IB_OK) {
        *fout = fnew;
    }

    return rc;
}

/**
 * Transformation: Get the max of a list of numbers.
 *
 * @param[in] ib IronBee engine
 * @param[in] mp Memory pool to use for allocations.
 * @param[in] fndata Function specific data.
 * @param[in] fin Input field.
 * @param[out] fout Output field. This is NULL on error.
 * @param[out] pflags Transformation flags.
 *
 * @returns IB_OK if successful.
 */
static ib_status_t tfn_max(ib_engine_t *ib,
                           ib_mpool_t *mp,
                           void *fndata,
                           const ib_field_t *fin,
                           const ib_field_t **fout,
                           ib_flags_t *pflags)
{
    assert(mp != NULL);
    assert(fin != NULL);
    assert(fout != NULL);
    assert(pflags != NULL);

    ib_status_t rc = IB_OK;

    /* Initialize the output field pointer */
    *fout = NULL;

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
 * @param[out] fout Output field. This is NULL on error.
 * @param[out] pflags Transformation flags.
 *
 * @returns IB_OK if successful.
 */
static ib_status_t tfn_min(ib_engine_t *ib,
                           ib_mpool_t *mp,
                           void *fndata,
                           const ib_field_t *fin,
                           const ib_field_t **fout,
                           ib_flags_t *pflags)
{
    ib_status_t rc = IB_OK;

    assert(mp != NULL);
    assert(fin != NULL);
    assert(fout != NULL);
    assert(pflags != NULL);

    /* Initialize the output field pointer */
    *fout = NULL;

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
 * @param[out] fout Output field. This is NULL on error.
 * @param[out] pflags Transformation flags.
 *
 * @returns IB_OK if successful.
 */
static ib_status_t tfn_url_decode(ib_engine_t *ib,
                                  ib_mpool_t *mp,
                                  void *fndata,
                                  const ib_field_t *fin,
                                  const ib_field_t **fout,
                                  ib_flags_t *pflags)
{
    assert(ib != NULL);
    assert(mp != NULL);
    assert(fin != NULL);
    assert(fout != NULL);
    assert(pflags != NULL);

    ib_status_t rc;
    ib_flags_t result;
    ib_field_t *fnew;

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
        rc = ib_field_create(&fnew, mp,
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
        rc = ib_field_create_bytestr_alias(&fnew, mp,
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

    /* When we reach this point rc==IB_OK. Commit the output value. */
    *fout = fnew;

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
 * @param[out] fout Output field. This is NULL on error.
 * @param[out] pflags Transformation flags.
 *
 * @returns IB_OK if successful.
 */
static ib_status_t tfn_html_entity_decode(ib_engine_t *ib,
                                          ib_mpool_t *mp,
                                          void *fndata,
                                          const ib_field_t *fin,
                                          const ib_field_t **fout,
                                          ib_flags_t *pflags)
{
    assert(ib != NULL);
    assert(mp != NULL);
    assert(fin != NULL);
    assert(fout != NULL);
    assert(pflags != NULL);

    ib_status_t rc;
    ib_flags_t result;
    ib_field_t *fnew;

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
        rc = ib_field_create(&fnew, mp,
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
        rc = ib_field_create_bytestr_alias(&fnew, mp,
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

    /* When we reach this point, rc==IB_OK. Commit the output values. */
    *fout = fnew;

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
 * @param[out] fout Output field. This is NULL on error.
 * @param[out] pflags Transformation flags.
 *
 * @returns IB_OK if successful.
 */
static ib_status_t normalize_path(ib_engine_t *ib,
                                  ib_mpool_t *mp,
                                  const ib_field_t *fin,
                                  bool win,
                                  const ib_field_t **fout,
                                  ib_flags_t *pflags)
{
    assert(ib != NULL);
    assert(mp != NULL);
    assert(fin != NULL);
    assert(fout != NULL);
    assert(pflags != NULL);

    ib_status_t rc;
    ib_flags_t result;
    ib_field_t *fnew;

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
        rc = ib_field_create(&fnew, mp,
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
        rc = ib_field_create_bytestr_alias(&fnew, mp,
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

    /* When we reach here, rc == IB_OK. Commit the output values. */
    *fout = fnew;

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
 * @param[out] fout Output field. This is NULL on error.
 * @param[out] pflags Transformation flags.
 *
 * @returns IB_OK if successful.
 */
static ib_status_t tfn_normalize_path(ib_engine_t *ib,
                                      ib_mpool_t *mp,
                                      void *fndata,
                                      const ib_field_t *fin,
                                      const ib_field_t **fout,
                                      ib_flags_t *pflags)
{
    assert(ib != NULL);
    assert(mp != NULL);
    assert(fin != NULL);
    assert(fout != NULL);
    assert(pflags != NULL);

    ib_status_t rc;

    rc = normalize_path(ib, mp, fin, false, fout, pflags);

    return rc;
}

/**
 * Convert @a fin to @a type and store the result in @a fout.
 *
 * If @a fin->type == @a type, then @a fout is set to @a fin.
 *
 * @param[in] ib IronBee engine. Used for logging.
 * @param[in] mp Memory pool. All allocations are out of here.
 * @param[in] type The target type.
 * @param[in] fin The input field.
 * @param[out] fout Output field. This is NULL on error.
 * @returns
 *   - IB_OK On success.
 *   - IB_EALLOC Allocation error.
 *   - IB_EINVAL If a conversion cannot be performed.
 */
static ib_status_t tfn_to_type(
    ib_engine_t *ib,
    ib_mpool_t *mp,
    ib_ftype_t type,
    const ib_field_t *fin,
    const ib_field_t **fout)
{
    assert(ib != NULL);
    assert(mp != NULL);
    assert(fin != NULL);
    assert(fout != NULL);

    ib_status_t rc;
    ib_field_t *fnew;

    /* Initialize the output field pointer */
    *fout = NULL;

    rc = ib_field_convert(mp, type, fin, &fnew);
    if (rc != IB_OK) {
        return rc;
    }

    /* Commit output value. If fnew == NULL, then there was no conversion. */
    if (fnew == NULL) {
        *fout = fin;
    }
    else {
        *fout = fnew;
    }

    return IB_OK;
}

/**
 * Use tfn_to_type() to convert @a fin to @a fout.
 *
 * @param[in] ib IronBee engine
 * @param[in] mp Memory pool to use for allocations.
 * @param[in] fndata Function specific data.
 * @param[in] fin Input field.
 * @param[out] fout Output field. This is NULL on error.
 * @param[out] pflags Transformation flags.
 *
 * @returns 
 *   - IB_OK If successful.
 *   - IB_EALLOC On allocation errors.
 */
static ib_status_t tfn_to_float(
    ib_engine_t *ib,
    ib_mpool_t *mp,
    void *fndata,
    const ib_field_t *fin,
    const ib_field_t **fout,
    ib_flags_t *pflags)
{
    return tfn_to_type(ib, mp, IB_FTYPE_FLOAT, fin, fout);
}

/**
 * Use tfn_to_type() to convert @a fin to @a fout.
 *
 * @param[in] ib IronBee engine
 * @param[in] mp Memory pool to use for allocations.
 * @param[in] fndata Function specific data.
 * @param[in] fin Input field.
 * @param[out] fout Output field. This is NULL on error.
 * @param[out] pflags Transformation flags.
 *
 * @returns 
 *   - IB_OK If successful.
 *   - IB_EALLOC On allocation errors.
 */
static ib_status_t tfn_to_integer(
    ib_engine_t *ib,
    ib_mpool_t *mp,
    void *fndata,
    const ib_field_t *fin,
    const ib_field_t **fout,
    ib_flags_t *pflags)
{
    return tfn_to_type(ib, mp, IB_FTYPE_NUM, fin, fout);
}

/**
 * Use tfn_to_type() to convert @a fin to @a fout.
 *
 * @param[in] ib IronBee engine
 * @param[in] mp Memory pool to use for allocations.
 * @param[in] fndata Function specific data.
 * @param[in] fin Input field.
 * @param[out] fout Output field. This is NULL on error.
 * @param[out] pflags Transformation flags.
 *
 * @returns 
 *   - IB_OK If successful.
 *   - IB_EALLOC On allocation errors.
 */
static ib_status_t tfn_to_string(
    ib_engine_t *ib,
    ib_mpool_t *mp,
    void *fndata,
    const ib_field_t *fin,
    const ib_field_t **fout,
    ib_flags_t *pflags)
{
    return tfn_to_type(ib, mp, IB_FTYPE_BYTESTR, fin, fout);
}

//! Convert a float. This matches operations found in math.h intentionally.
typedef ib_float_t (*ib_float_op_t) (ib_float_t);

/**
 * Convert a floating point field to another floating point field.
 *
 * This code wraps field logic around math.h defined calls to
 * functions like ceill(), floorl(), and roundl(). The math.h calls are
 * passed in as the @a op parameter.
 *
 * @param[in] mp Memory pool to use for allocations.
 * @param[in] op Operation to perform. It should take a long double and
 *            return a long double.
 * @param[in] fin Input field.
 * @param[out] fout Output field. This is NULL on error.
 * @param[out] pflags Transformation flags.
 *
 * @returns 
 *   - IB_OK If successful.
 *   - IB_EALLOC On allocation errors.
 *   - IB_EOTHER If any unexpected error is encountered.
 */
static ib_status_t tfn_float_op(
    ib_mpool_t *mp,
    ib_float_op_t op,
    const ib_field_t *fin,
    const ib_field_t **fout,
    ib_flags_t *pflags)
{
    ib_float_t flt;
    ib_field_t *fnew;
    ib_status_t rc = IB_OK;

    const ib_bytestr_t *bstr;
    const char *str;

    /* Initialize the output field pointer */
    *fout = NULL;

    /* Get the float value. */
    switch(fin->type) {
        case IB_FTYPE_NUM:
            *fout = fin;
            return IB_OK;
        case IB_FTYPE_FLOAT:
            rc = ib_field_value(fin, ib_ftype_float_out(&flt));
            if (rc != IB_OK) {
                return IB_EOTHER;
            }
            break;
        case IB_FTYPE_NULSTR:
            rc = ib_field_value(fin, ib_ftype_nulstr_out(&str));
            if (rc != IB_OK) {
                return IB_EOTHER;
            }
            rc = ib_string_to_float(str, &flt);
            if (rc != IB_OK) {
                return IB_EOTHER;
            }
            break;
        case IB_FTYPE_BYTESTR:
            rc = ib_field_value(fin, ib_ftype_bytestr_out(&bstr));
            if (rc != IB_OK) {
                return IB_EOTHER;
            }
            rc = ib_string_to_float_ex(
                (const char *)ib_bytestr_const_ptr(bstr),
                ib_bytestr_length(bstr),
                &flt);
            if (rc != IB_OK) {
                return IB_EOTHER;
            }
            break;
        case IB_FTYPE_LIST:
        case IB_FTYPE_SBUFFER:
        case IB_FTYPE_GENERIC:
            return IB_EINVAL;
    }

    flt = op(flt);

    rc = ib_field_create(
        &fnew,
        mp,
        fin->name,
        fin->nlen,
        IB_FTYPE_FLOAT,
        ib_ftype_float_in(&flt));
    if (rc == IB_OK) {
        *pflags |= IB_TFN_FMODIFIED;
        *fout = fnew;
    }

    return rc;
}

/**
 * Convert a bytestr, nulstr, float, or num field to a float using floorl().
 *
 * @param[in] ib IronBee engine
 * @param[in] mp Memory pool to use for allocations.
 * @param[in] fndata Function specific data.
 * @param[in] fin Input field.
 * @param[out] fout Output field. This is NULL on error.
 * @param[out] pflags Transformation flags.
 *
 * @returns 
 *   - IB_OK If successful.
 *   - IB_EALLOC On allocation errors.
 */
static ib_status_t tfn_floor(
    ib_engine_t *ib,
    ib_mpool_t *mp,
    void *fndata,
    const ib_field_t *fin,
    const ib_field_t **fout,
    ib_flags_t *pflags)
{
    return tfn_float_op(mp, floorl, fin, fout, pflags);
}

/**
 * Convert a bytestr, nulstr, float, or num field to a float using ceill().
 *
 * @param[in] ib IronBee engine
 * @param[in] mp Memory pool to use for allocations.
 * @param[in] fndata Function specific data.
 * @param[in] fin Input field.
 * @param[out] fout Output field. This is NULL on error.
 * @param[out] pflags Transformation flags.
 *
 * @returns 
 *   - IB_OK If successful.
 *   - IB_EALLOC On allocation errors.
 */
static ib_status_t tfn_ceil(
    ib_engine_t *ib,
    ib_mpool_t *mp,
    void *fndata,
    const ib_field_t *fin,
    const ib_field_t **fout,
    ib_flags_t *pflags)
{
    return tfn_float_op(mp, ceill, fin, fout, pflags);
}

/**
 * Convert a bytestr, nulstr, float, or num field to a float using roundl().
 *
 * @param[in] ib IronBee engine
 * @param[in] mp Memory pool to use for allocations.
 * @param[in] fndata Function specific data.
 * @param[in] fin Input field.
 * @param[out] fout Output field. This is NULL on error.
 * @param[out] pflags Transformation flags.
 *
 * @returns 
 *   - IB_OK If successful.
 *   - IB_EALLOC On allocation errors.
 */
static ib_status_t tfn_round(
    ib_engine_t *ib,
    ib_mpool_t *mp,
    void *fndata,
    const ib_field_t *fin,
    const ib_field_t **fout,
    ib_flags_t *pflags)
{
    return tfn_float_op(mp, roundl, fin, fout, pflags);
}

/**
 * Extract the name of the given field and convert it to a new field.
 *
 * This function is the common to tfn_to_names() and tfn_to_name().
 *
 * The new field will be named as the old field, and will contain
 * a bytestr containing the field name.
 *
 * @param[in] ib IronBee engine
 * @param[in] mp Memory pool to use for allocations.
 * @param[in] fin Input field.
 * @param[out] fout Output field. This is NULL on error.
 *
 * @returns 
 *   - IB_OK if successful.
 *   - IB_EALLOC if allocation error.
 */
static ib_status_t tfn_to_name_common(
    ib_engine_t *ib,
    ib_mpool_t *mp,
    const ib_field_t *fin,
    const ib_field_t **fout)
{
    assert(ib != NULL);
    assert(mp != NULL);
    assert(fin != NULL);
    assert(fout != NULL);

    ib_field_t *fnew;
    ib_status_t rc;
    ib_bytestr_t *new_value;

    /* Initialize the output field pointer */
    *fout = NULL;

    rc = ib_bytestr_dup_mem(
        &new_value,
        mp,
        (const uint8_t*)fin->name,
        sizeof(*(fin->name)) * fin->nlen);
    if (rc != IB_OK) {
        return IB_EALLOC;
    }

    rc = ib_field_create(
        &fnew,
        mp,
        fin->name,
        fin->nlen,
        IB_FTYPE_BYTESTR,
        ib_ftype_bytestr_in(new_value));
    if (rc != IB_OK) {
        return IB_EALLOC;
    }

    /* Commit back the value. */
    *fout = fnew;

    return IB_OK;
}

/**
 * Extract the name of the given field and convert it to a new field.
 *
 * The new field will be named as the old field, and will contain
 * a bytestr containing the field name.
 *
 * @param[in] ib IronBee engine
 * @param[in] mp Memory pool to use for allocations.
 * @param[in] fndata Function specific data.
 * @param[in] fin Input field.
 * @param[out] fout Output field. This is NULL on error.
 * @param[out] pflags Transformation flags.
 *
 * @returns 
 *   - IB_OK if successful.
 *   - IB_EALLOC if allocation error.
 */
static ib_status_t tfn_to_name(
    ib_engine_t *ib,
    ib_mpool_t *mp,
    void *fndata,
    const ib_field_t *fin,
    const ib_field_t **fout,
    ib_flags_t *pflags)
{
    return tfn_to_name_common(ib, mp, fin, fout);
}

/**
 * Extract the names of the given collection members.
 *
 * The extracted names are put into a new collection with the same
 * name as @a fin. The extracted names are contained as bytestr fields
 * in which their name and value are the same.
 *
 * By way of example, the collection C with member fields c1, c2, and c3
 * will produce a new collection named C with member fields c1, c2, and c3.
 * However, the *values* of c1, c2, and c3 will be bytestrs that
 * represent the stings c1, c2, and c3.
 *
 * @param[in] ib IronBee engine
 * @param[in] mp Memory pool to use for allocations.
 * @param[in] fndata Function specific data.
 * @param[in] fin Input field.
 * @param[out] fout Output field. This is NULL on error.
 * @param[out] pflags Transformation flags.
 *
 * @returns 
 *   - IB_OK If successful.
 *   - IB_EINVAL If @a fin is not a list.
 *   - IB_EALLOC Failed allocation.
 */
static ib_status_t tfn_to_names(
    ib_engine_t *ib,
    ib_mpool_t *mp,
    void *fndata,
    const ib_field_t *fin,
    const ib_field_t **fout,
    ib_flags_t *pflags)
{
    assert(ib != NULL);
    assert(mp != NULL);
    assert(fin != NULL);
    assert(fout != NULL);

    ib_field_t *fnew;
    ib_status_t rc;
    ib_list_t *new_value;
    const ib_list_t *list;
    const ib_list_node_t *node;

    /* Initialize the output field pointer */
    *fout = NULL;

    /* Fail if the input field is not a list. */
    if (fin->type != IB_FTYPE_LIST) {
        return IB_EINVAL;
    }

    /* Get the input list of fields. */
    rc = ib_field_value(fin, ib_ftype_list_out(&list));
    if (rc != IB_OK) {
        return rc;
    }

    /* Build an output list to hold the values. */
    rc = ib_list_create(&new_value, mp);
    if (rc != IB_OK) {
        return IB_EALLOC;
    }

    /* If we haven't failed yet, make a new output field holding the created
     * list, new_value. */
    rc = ib_field_create(
        &fnew,
        mp,
        fin->name,
        fin->nlen,
        IB_FTYPE_LIST,
        ib_ftype_list_in(new_value));
    if (rc != IB_OK) {
        return IB_EALLOC;
    }

    /* Build the new_value list using the input list. */
    IB_LIST_LOOP_CONST(list, node) {
        const ib_field_t *list_field =
            (const ib_field_t*)ib_list_node_data_const(node);
        const ib_field_t *list_out_field;
        
        rc = tfn_to_name_common(ib, mp, list_field, &list_out_field);
        if (rc != IB_OK) {
            return rc;
        }

        rc = ib_field_list_add_const(fnew, list_out_field);
        if (rc != IB_OK) {
            return rc;
        }
    }

    /* Commit back the new field. */
    *fout = fnew;
    return IB_OK;
}

/**
 * Path normalization transformation with support for Windows path separator
 *
 * @param[in] ib IronBee engine
 * @param[in] mp Memory pool to use for allocations.
 * @param[in] fndata Function specific data.
 * @param[in] fin Input field.
 * @param[out] fout Output field. This is NULL on error.
 * @param[out] pflags Transformation flags.
 *
 * @returns IB_OK if successful.
 */
static ib_status_t tfn_normalize_path_win(ib_engine_t *ib,
                                          ib_mpool_t *mp,
                                          void *fndata,
                                          const ib_field_t *fin,
                                          const ib_field_t **fout,
                                          ib_flags_t *pflags)
{
    assert(ib != NULL);
    assert(mp != NULL);
    assert(fin != NULL);
    assert(fout != NULL);
    assert(pflags != NULL);

    ib_status_t rc;

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

    /* Math transformations. */
    rc = ib_tfn_register(ib, "round", tfn_round, IB_TFN_FLAG_NONE, NULL);
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_tfn_register(ib, "ceil", tfn_ceil, IB_TFN_FLAG_NONE, NULL);
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_tfn_register(ib, "floor", tfn_floor, IB_TFN_FLAG_NONE, NULL);
    if (rc != IB_OK) {
        return rc;
    }

    /* Type conversion transformations. */
    rc = ib_tfn_register(ib, "toString", tfn_to_string, IB_TFN_FLAG_NONE, NULL);
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_tfn_register(ib, "toInteger", tfn_to_integer,
                         IB_TFN_FLAG_NONE, NULL);
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_tfn_register(ib, "toFloat", tfn_to_float, IB_TFN_FLAG_NONE, NULL);
    if (rc != IB_OK) {
        return rc;
    }

    /* Name extraction transformations. */
    rc = ib_tfn_register(ib, "name", tfn_to_name, IB_TFN_FLAG_NONE, NULL);
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_tfn_register(ib, "names", tfn_to_names, IB_TFN_FLAG_NONE, NULL);
    if (rc != IB_OK) {
        return rc;
    }


    return IB_OK;
}
