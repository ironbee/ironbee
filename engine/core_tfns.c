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
#include <ironbee/mm.h>
#include <ironbee/operator.h>
#include <ironbee/path.h>
#include <ironbee/string.h>
#include <ironbee/string_lower.h>
#include <ironbee/string_trim.h>
#include <ironbee/string_whitespace.h>
#include <ironbee/transformation.h>
#include <ironbee/util.h>

#include <assert.h>
#include <ctype.h>
#include <math.h>

/**
 * Function adaptor for many string functions.
 **/
typedef ib_status_t (*ib_strmod_fn_t)(
    ib_mm_t mm,
    const uint8_t *data_in, size_t dlen_in,
    const uint8_t **data_out, size_t *dlen_out
);
    
/**
 * String modification transformation core
 *
 * @param[in] mm Memory manager to use for allocations.
 * @param[in] ex_fn EX (string/length) transformation function
 * @param[in] fin Input field.
 * @param[out] fout Output field. This is NULL on error.
 *
 * @returns IB_OK if successful.
 */
static ib_status_t tfn_strmod(
    ib_mm_t             mm,
    ib_strmod_fn_t      fn,
    const ib_field_t   *fin,
    const ib_field_t  **fout
) 
{
    ib_status_t rc;
    ib_field_t *fnew;

    assert(fn != NULL);
    assert(fin != NULL);
    assert(fout != NULL);

    /* Initialize the output field pointer */
    *fout = NULL;

    switch(fin->type) {
    case IB_FTYPE_BYTESTR:
    {
        const ib_bytestr_t *bs;
        const uint8_t *din;
        const uint8_t *dout;
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
        rc = fn(mm, (const uint8_t *)din, dlen, &dout, &dlen);
        if (rc != IB_OK) {
            return rc;
        }
        rc = ib_field_create_bytestr_alias(&fnew, mm,
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

    return IB_OK;
}

/** Adapt ib_strlower() to ib_strmod_fn_t(). */
static ib_status_t adapt_lower(
    ib_mm_t mm,
    const uint8_t  *data_in,  size_t  dlen_in,
    const uint8_t **data_out, size_t *dlen_out
)
{
    uint8_t *out;
    ib_status_t rc = ib_strlower(mm, data_in, dlen_in, &out);
    if (rc == IB_OK) {
        *dlen_out = strlen((const char *)out);
    }
    
    *data_out = out;
    
    return rc;
}

/** Adapt ib_strtrim_left() to ib_strmod_fn_t(). */
static ib_status_t adapt_trim_left(
    ib_mm_t mm,
    const uint8_t  *data_in,  size_t  dlen_in,
    const uint8_t **data_out, size_t *dlen_out
)
{
    return ib_strtrim_left(data_in, dlen_in, data_out, dlen_out);
}

/** Adapt ib_strtrim_right() to ib_strmod_fn_t(). */
static ib_status_t adapt_trim_right(
    ib_mm_t mm,
    const uint8_t  *data_in,  size_t  dlen_in,
    const uint8_t **data_out, size_t *dlen_out
)
{
    return ib_strtrim_right(data_in, dlen_in, data_out, dlen_out);
}

/** Adapt ib_strtrim_lr() to ib_strmod_fn_t(). */
static ib_status_t adapt_trim_lr(
    ib_mm_t mm,
    const uint8_t  *data_in,  size_t  dlen_in,
    const uint8_t **data_out, size_t *dlen_out
)
{
    return ib_strtrim_lr(data_in, dlen_in, data_out, dlen_out);
}

/** Adapt ib_str_whitespace_remove() to ib_strmod_fn_t(). */
static ib_status_t adapt_whitespace_remove(
    ib_mm_t mm,
    const uint8_t  *data_in,  size_t  dlen_in,
    const uint8_t **data_out, size_t *dlen_out
)
{
    uint8_t *out = NULL;
    ib_status_t rc;
    rc = ib_str_whitespace_remove(mm, data_in, dlen_in, &out, dlen_out);
    *data_out = out;
    
    return rc;
}

/** Adapt ib_str_whitespace_compress() to ib_strmod_fn_t(). */
static ib_status_t adapt_whitespace_compress(
    ib_mm_t mm,
    const uint8_t  *data_in,  size_t  dlen_in,
    const uint8_t **data_out, size_t *dlen_out
)
{
    uint8_t *out = NULL;
    ib_status_t rc;
    rc = ib_str_whitespace_compress(mm, data_in, dlen_in, &out, dlen_out);
    *data_out = out;
    
    return rc;
}

/**
 * Simple ASCII lowercase function.
 *
 * @param[in] instdata Instance data. Unused.
 * @param[in] mm Memory manager to use for allocations.
 * @param[in] fin Input field.
 * @param[out] fout Output field. This is NULL on error.
 * @param[in] fndata Callback data
 *
 * @returns IB_OK if successful.
 */
static ib_status_t tfn_lowercase(
    void              *instdata,
    ib_mm_t            mm,
    const ib_field_t  *fin,
    const ib_field_t **fout,
    void              *fndata
) {
    ib_status_t rc = tfn_strmod(mm,
                                adapt_lower,
                                fin, fout);

    return rc;
}

/**
 * Simple ASCII trim (left) transformation.
 *
 * @param[in] instdata Instance data. Unused.
 * @param[in] mm Memory manager to use for allocations.
 * @param[in] fin Input field.
 * @param[out] fout Output field. This is NULL on error.
 * @param[in] fndata Callback data
 *
 * @returns IB_OK if successful.
 */
static ib_status_t tfn_trim_left(
    void              *instdata,
    ib_mm_t            mm,
    const ib_field_t  *fin,
    const ib_field_t **fout,
    void              *fndata
) {
    ib_status_t rc = tfn_strmod(mm,
                                adapt_trim_left,
                                fin, fout);

    return rc;
}

/**
 * Simple ASCII trim (right) transformation.
 *
 * @param[in] instdata Instance data. Unused.
 * @param[in] mm Memory manager to use for allocations.
 * @param[in] fin Input field.
 * @param[out] fout Output field. This is NULL on error.
 * @param[in] fndata Callback data
 *
 * @returns IB_OK if successful.
 */
static ib_status_t tfn_trim_right(
    void              *instdata,
    ib_mm_t            mm,
    const ib_field_t  *fin,
    const ib_field_t **fout,
    void              *fndata
) {
    ib_status_t rc = tfn_strmod(mm,
                                adapt_trim_right,
                                fin, fout);

    return rc;
}

/**
 * Simple ASCII trim transformation.
 *
 * @param[in] instdata Instance data. Unused.
 * @param[in] mm Memory manager to use for allocations.
 * @param[in] fin Input field.
 * @param[out] fout Output field. This is NULL on error.
 * @param[in] fndata Callback data
 *
 * @returns IB_OK if successful.
 */
static ib_status_t tfn_trim(
    void              *instdata,
    ib_mm_t            mm,
    const ib_field_t  *fin,
    const ib_field_t **fout,
    void              *fndata)
{
    ib_status_t rc = tfn_strmod(mm,
                                adapt_trim_lr,
                                fin, fout);

    return rc;
}

/**
 * Remove all whitespace from a string
 *
 * @param[in] instdata Instance data. Unused.
 * @param[in] mm Memory manager to use for allocations.
 * @param[in] fin Input field.
 * @param[out] fout Output field. This is NULL on error.
 * @param[in] fndata Callback data
 *
 * @returns IB_OK if successful.
 */
static ib_status_t tfn_wspc_remove(
    void              *instdata,
    ib_mm_t            mm,
    const ib_field_t  *fin,
    const ib_field_t **fout,
    void              *fndata
) {
    ib_status_t rc = tfn_strmod(mm,
                                adapt_whitespace_remove,
                                fin, fout);

    return rc;
}

/**
 * Compress whitespace in a string
 *
 * @param[in] instdata Instance data. Unused.
 * @param[in] mm Memory manager to use for allocations.
 * @param[in] fin Input field.
 * @param[out] fout Output field. This is NULL on error.
 * @param[in] fndata Callback data
 *
 * @returns IB_OK if successful.
 */
static ib_status_t tfn_wspc_compress(
    void              *instdata,
    ib_mm_t            mm,
    const ib_field_t  *fin,
    const ib_field_t **fout,
    void              *fndata
) {
    ib_status_t rc = tfn_strmod(mm,
                                adapt_whitespace_compress,
                                fin, fout);

    return rc;
}

/**
 * Length transformation
 *
 * @param[in] instdata Instance data. Unused.
 * @param[in] mm Memory manager to use for allocations.
 * @param[in] fin Input field.
 * @param[out] fout Output field. This is NULL on error.
 * @param[in] fndata Callback data
 *
 * @returns IB_OK if successful.
 */
static ib_status_t tfn_length(
    void              *instdata,
    ib_mm_t            mm,
    const ib_field_t  *fin,
    const ib_field_t **fout,
    void              *fndata
) {
    assert(fin != NULL);
    assert(fout != NULL);

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
            &fnew, mm,
            fin->name, fin->nlen,
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
            &fnew, mm,
            fin->name, fin->nlen,
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
            &fnew, mm,
            fin->name, fin->nlen,
            IB_FTYPE_LIST, NULL
        );
        if (rc != IB_OK) {
            return rc;
        }

        /* Walk through the incoming fields */
        IB_LIST_LOOP_CONST(ilist, node) {
            const ib_field_t *ifield = (ib_field_t *)node->data;
            const ib_field_t *ofield = NULL;

            rc = tfn_length(NULL, mm, ifield, &ofield, NULL);
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
            &fnew, mm, fin->name, fin->nlen, IB_FTYPE_NUM,
            ib_ftype_num_in(&len)
        );
    }

    if (rc == IB_OK) {
        *fout = fnew;
    }
    return rc;
}

/**
 * Count transformation
 *
 * @param[in] instdata Instance data. Unused.
 * @param[in] mm Memory manager to use for allocations.
 * @param[in] fin Input field.
 * @param[out] fout Output field. This is NULL on error.
 * @param[in] fndata Callback data
 *
 * @returns IB_OK if successful.
 */
static ib_status_t tfn_count(
    void              *instdata,
    ib_mm_t            mm,
    const ib_field_t  *fin,
    const ib_field_t **fout,
    void              *fndata
) {
    assert(fin != NULL);
    assert(fout != NULL);

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
        &fnew, mm, fin->name, fin->nlen, IB_FTYPE_NUM,
        ib_ftype_num_in(&value)
    );

    if (rc == IB_OK) {
        *fout = fnew;
    }

    return rc;
}

/**
 * Get maximum / minimum of a list of values
 *
 * @param[in] is_max true for max, false for min
 * @param[in] mm Memory manager to use for allocations.
 * @param[in] fin Input field.
 * @param[out] fout Output field. This is NULL on error.
 *
 * @returns IB_OK if successful.
 */
static ib_status_t list_minmax(
    bool               is_max,
    ib_mm_t            mm,
    const ib_field_t  *fin,
    const ib_field_t **fout
) {
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

            rc = list_minmax(is_max, mm, fin, &tmp);
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
    rc = ib_field_create(&fnew, mm,
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
 * @param[in] instdata Instance data. Unused.
 * @param[in] mm Memory manager to use for allocations.
 * @param[in] fin Input field.
 * @param[out] fout Output field. This is NULL on error.
 * @param[in] fndata Callback data
 *
 * @returns IB_OK if successful.
 */
static ib_status_t tfn_max(
    void              *instdata,
    ib_mm_t            mm,
    const ib_field_t  *fin,
    const ib_field_t **fout,
    void              *fndata
) {
    assert(fin != NULL);
    assert(fout != NULL);

    ib_status_t rc = IB_OK;

    /* Initialize the output field pointer */
    *fout = NULL;

    switch (fin->type) {
        case IB_FTYPE_NUM:
            *fout = (ib_field_t *)fin;
            break;

        case IB_FTYPE_LIST:
            rc = list_minmax(true, mm, fin, fout);
            break;

        default:
            rc = IB_EINVAL;
    }

    return rc;
}

/**
 * Transformation: Get the min of a list of numbers.
 *
 * @param[in] instdata Instance data. Unused.
 * @param[in] mm Memory manager to use for allocations.
 * @param[in] fin Input field.
 * @param[out] fout Output field. This is NULL on error.
 * @param[in] fndata Callback data
 *
 * @returns IB_OK if successful.
 */
static ib_status_t tfn_min(
    void              *instdata,
    ib_mm_t            mm,
    const ib_field_t  *fin,
    const ib_field_t **fout,
    void              *fndata
) {
    ib_status_t rc = IB_OK;

    assert(fin != NULL);
    assert(fout != NULL);

    /* Initialize the output field pointer */
    *fout = NULL;

    switch (fin->type) {
        case IB_FTYPE_NUM:
            *fout = (ib_field_t *)fin;
            rc = IB_OK;
            break;

        case IB_FTYPE_LIST:
            rc = list_minmax(true, mm, fin, fout);
            break;

        default:
            rc = IB_EINVAL;
    }

    return rc;
}

/**
 * URL Decode transformation
 *
 * @param[in] instdata Instance data. Unused.
 * @param[in] mm Memory manager to use for allocations.
 * @param[in] fin Input field.
 * @param[out] fout Output field. This is NULL on error.
 * @param[in] fndata Callback data
 *
 * @returns IB_OK if successful.
 */
static ib_status_t tfn_url_decode(
    void              *instdata,
    ib_mm_t            mm,
    const ib_field_t  *fin,
    const ib_field_t **fout,
    void              *fndata
) {
    assert(fin != NULL);
    assert(fout != NULL);

    ib_status_t rc;
    ib_field_t *fnew;

    /* Initialize the output field pointer */
    *fout = NULL;

    switch(fin->type) {
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
        dout = ib_mm_alloc(mm, dlen + 1);
        if (dout == NULL) {
            return IB_EALLOC;
        }
        rc = ib_util_decode_url(din, dlen, dout, &dlen);
        if (rc != IB_OK) {
            return rc;
        }
        rc = ib_field_create_bytestr_alias(&fnew, mm,
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

    return IB_OK;
}

/**
 * HTML entity decode transformation
 *
 * @param[in] instdata Instance data. Unused.
 * @param[in] mm Memory manager to use for allocations.
 * @param[in] fin Input field.
 * @param[out] fout Output field. This is NULL on error.
 * @param[in] fndata Callback data
 *
 * @returns IB_OK if successful.
 */
static ib_status_t tfn_html_entity_decode(
    void              *instdata,
    ib_mm_t            mm,
    const ib_field_t  *fin,
    const ib_field_t **fout,
    void              *fndata
) {
    assert(fin != NULL);
    assert(fout != NULL);

    ib_status_t rc;
    ib_field_t *fnew;

    /* Initialize the output field pointer */
    *fout = NULL;

    switch(fin->type) {
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
        dout = ib_mm_alloc(mm, dlen);
        if (dout == NULL) {
            return IB_EALLOC;
        }
        rc = ib_util_decode_html_entity(din, dlen, dout, &dlen);
        if (rc != IB_OK) {
            return rc;
        }
        rc = ib_field_create_bytestr_alias(&fnew, mm,
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

    return IB_OK;
}

/**
 * Path normalization transformation
 *
 * @param[in] mm Memory manager to use for allocations.
 * @param[in] fin Input field.
 * @param[in] win Handle windows-style '\'?
 * @param[out] fout Output field. This is NULL on error.
 *
 * @returns IB_OK if successful.
 */
static ib_status_t normalize_path(
    ib_mm_t            mm,
    const ib_field_t  *fin,
    bool               win,
    const ib_field_t **fout
) {
    assert(fin != NULL);
    assert(fout != NULL);

    ib_status_t rc;
    ib_field_t *fnew;

    /* Initialize the output field pointer */
    *fout = NULL;

    switch(fin->type) {

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
        rc = ib_util_normalize_path(mm,
                                    din, dlen, win,
                                    &dout, &dlen);
        if (rc != IB_OK) {
            return rc;
        }
        rc = ib_field_create_bytestr_alias(&fnew, mm,
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

    return IB_OK;
}

/**
 * Path normalization transformation
 *
 * @param[in] instdata Instance data. Unused.
 * @param[in] mm Memory manager to use for allocations.
 * @param[in] fin Input field.
 * @param[out] fout Output field. This is NULL on error.
 * @param[in] fndata Callback data
 *
 * @returns IB_OK if successful.
 */
static ib_status_t tfn_normalize_path(
    void              *instdata,
    ib_mm_t            mm,
    const ib_field_t  *fin,
    const ib_field_t **fout,
    void              *fndata
) {
    assert(fin != NULL);
    assert(fout != NULL);

    ib_status_t rc;

    rc = normalize_path(mm, fin, false, fout);

    return rc;
}

/**
 * Convert @a fin to @a type and store the result in @a fout.
 *
 * If @a fin->type == @a type, then @a fout is set to @a fin.
 *
 * @param[in] mm Memory manager. All allocations are out of here.
 * @param[in] type The target type.
 * @param[in] fin The input field.
 * @param[out] fout Output field. This is NULL on error.
 * @returns
 *   - IB_OK On success.
 *   - IB_EALLOC Allocation error.
 *   - IB_EINVAL If a conversion cannot be performed.
 */
static ib_status_t tfn_to_type(
    ib_mm_t            mm,
    ib_ftype_t         type,
    const ib_field_t  *fin,
    const ib_field_t **fout
) {
    assert(fin != NULL);
    assert(fout != NULL);

    ib_status_t rc;
    ib_field_t *fnew;

    /* Initialize the output field pointer */
    *fout = NULL;

    rc = ib_field_convert(mm, type, fin, &fnew);
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
 * @param[in] instdata Instance data. Unused.
 * @param[in] mm Memory manager to use for allocations.
 * @param[in] fin Input field.
 * @param[out] fout Output field. This is NULL on error.
 * @param[in] fndata Callback data
 *
 * @returns
 *   - IB_OK If successful.
 *   - IB_EALLOC On allocation errors.
 */
static ib_status_t tfn_to_float(
    void              *instdata,
    ib_mm_t            mm,
    const ib_field_t  *fin,
    const ib_field_t **fout,
    void              *fndata
) {
    return tfn_to_type(mm, IB_FTYPE_FLOAT, fin, fout);
}

/**
 * Use tfn_to_type() to convert @a fin to @a fout.
 *
 * @param[in] instdata Instance data. Unused.
 * @param[in] mm Memory manager to use for allocations.
 * @param[in] fin Input field.
 * @param[out] fout Output field. This is NULL on error.
 * @param[in] fndata Callback data
 *
 * @returns
 *   - IB_OK If successful.
 *   - IB_EALLOC On allocation errors.
 */
static ib_status_t tfn_to_integer(
    void              *instdata,
    ib_mm_t            mm,
    const ib_field_t  *fin,
    const ib_field_t **fout,
    void              *fndata
) {
    return tfn_to_type(mm, IB_FTYPE_NUM, fin, fout);
}

/**
 * Use tfn_to_type() to convert @a fin to @a fout.
 *
 * @param[in] instdata Instance data. Unused.
 * @param[in] mm Memory manager to use for allocations.
 * @param[in] fin Input field.
 * @param[out] fout Output field. This is NULL on error.
 * @param[in] fndata Callback data
 *
 * @returns
 *   - IB_OK If successful.
 *   - IB_EALLOC On allocation errors.
 */
static ib_status_t tfn_to_string(
    void              *instdata,
    ib_mm_t            mm,
    const ib_field_t  *fin,
    const ib_field_t **fout,
    void              *fndata
) {
    return tfn_to_type(mm, IB_FTYPE_BYTESTR, fin, fout);
}

//! Convert a float. This matches operations found in math.h intentionally.
typedef ib_float_t (*ib_float_op_t) (ib_float_t);

/**
 * Convert a floating point field to an int field.
 *
 * This code wraps field logic around math.h defined calls to
 * functions like ceill(), floorl(), and roundl() and casts the results
 * to integers. The math.h calls are passed in as the @a op parameter.
 *
 * @param[in] mm Memory manager to use for allocations.
 * @param[in] op Operation to perform. It should take a long double and
 *            return a long double.
 * @param[in] fin Input field.
 * @param[out] fout Output field. This is NULL on error.
 *
 * @returns
 *   - IB_OK If successful.
 *   - IB_EALLOC On allocation errors.
 *   - IB_EOTHER If any unexpected error is encountered.
 */
static ib_status_t tfn_float_to_num_op(
    ib_mm_t            mm,
    ib_float_op_t      op,
    const ib_field_t  *fin,
    const ib_field_t **fout
) {
    ib_float_t flt;
    ib_num_t num;
    ib_field_t *fnew;
    ib_status_t rc = IB_OK;

    const ib_bytestr_t *bstr;
    const char *str;

    /* Initialize the output field pointer */
    *fout = NULL;

    /* Get the float value. */
    switch(fin->type) {
        case IB_FTYPE_NUM:
            rc = ib_field_value(fin, ib_ftype_num_out(&num));
            if (rc != IB_OK) {
                return IB_EOTHER;
            }
            flt = num;
            break;
        case IB_FTYPE_TIME:
            rc = ib_field_value(fin, ib_ftype_num_out(&num));
            if (rc != IB_OK) {
                return IB_EOTHER;
            }
            flt = num;
            break;
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

    num = (ib_num_t)flt;

    rc = ib_field_create(
        &fnew,
        mm,
        fin->name,
        fin->nlen,
        IB_FTYPE_NUM,
        ib_ftype_num_in(&num));
    if (rc == IB_OK) {
        *fout = fnew;
    }

    return rc;
}

/**
 * Convert a bytestr, nulstr, float, or num field to a float using floorl().
 *
 * @param[in] instdata Instance data. Unused.
 * @param[in] mm Memory manager to use for allocations.
 * @param[in] fin Input field.
 * @param[out] fout Output field. This is NULL on error.
 * @param[in] fndata Callback data
 *
 * @returns
 *   - IB_OK If successful.
 *   - IB_EALLOC On allocation errors.
 */
static ib_status_t tfn_ifloor(
    void              *instdata,
    ib_mm_t            mm,
    const ib_field_t  *fin,
    const ib_field_t **fout,
    void              *fndata
) {
    return tfn_float_to_num_op(mm, floorl, fin, fout);
}

/**
 * Convert a bytestr, nulstr, float, or num field to a float using ceill().
 *
 * @param[in] instdata Instance data. Unused.
 * @param[in] mm Memory manager to use for allocations.
 * @param[in] fin Input field.
 * @param[out] fout Output field. This is NULL on error.
 * @param[in] fndata Callback data
 *
 * @returns
 *   - IB_OK If successful.
 *   - IB_EALLOC On allocation errors.
 */
static ib_status_t tfn_iceil(
    void              *instdata,
    ib_mm_t            mm,
    const ib_field_t  *fin,
    const ib_field_t **fout,
    void              *fndata
) {
    return tfn_float_to_num_op(mm, ceill, fin, fout);
}

/**
 * Convert a bytestr, nulstr, float, or num field to a float using roundl().
 *
 * @param[in] instdata Instance data. Unused.
 * @param[in] mm Memory manager to use for allocations.
 * @param[in] fin Input field.
 * @param[out] fout Output field. This is NULL on error.
 * @param[in] fndata Callback data
 *
 * @returns
 *   - IB_OK If successful.
 *   - IB_EALLOC On allocation errors.
 */
static ib_status_t tfn_iround(
    void              *instdata,
    ib_mm_t            mm,
    const ib_field_t  *fin,
    const ib_field_t **fout,
    void              *fndata
) {
    return tfn_float_to_num_op(mm, roundl, fin, fout);
}

/**
 * Extract the name of the given field and convert it to a new field.
 *
 * This function is common to tfn_to_names() and tfn_to_name().
 *
 * The new field will be named as the old field, and will contain
 * a bytestr containing the field name.
 *
 * @param[in] mm Memory manager to use for allocations.
 * @param[in] fin Input field.
 * @param[out] fout Output field. This is NULL on error.
 *
 * @returns
 *   - IB_OK if successful.
 *   - IB_EALLOC if allocation error.
 */
static ib_status_t tfn_to_name_common(
    ib_mm_t            mm,
    const ib_field_t  *fin,
    const ib_field_t **fout
) {
    assert(fin != NULL);
    assert(fout != NULL);

    ib_field_t *fnew;
    ib_status_t rc;
    ib_bytestr_t *new_value;

    /* Initialize the output field pointer */
    *fout = NULL;

    rc = ib_bytestr_dup_mem(
        &new_value,
        mm,
        (const uint8_t*)fin->name,
        sizeof(*(fin->name)) * fin->nlen);
    if (rc != IB_OK) {
        return IB_EALLOC;
    }

    rc = ib_field_create(
        &fnew,
        mm,
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
 * @param[in] instdata Instance data. Unused.
 * @param[in] mm Memory manager to use for allocations.
 * @param[in] fin Input field.
 * @param[out] fout Output field. This is NULL on error.
 * @param[in] fndata Callback data
 *
 * @returns
 *   - IB_OK if successful.
 *   - IB_EALLOC if allocation error.
 */
static ib_status_t tfn_to_name(
    void             *instdata,
    ib_mm_t            mm,
    const ib_field_t  *fin,
    const ib_field_t **fout,
    void              *fndata
) {
    return tfn_to_name_common(mm, fin, fout);
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
 * @param[in] instdata Instance data. Unused.
 * @param[in] mm Memory manager to use for allocations.
 * @param[in] fin Input field.
 * @param[out] fout Output field. This is NULL on error.
 * @param[in] fndata Callback data
 *
 * @returns
 *   - IB_OK If successful.
 *   - IB_EINVAL If @a fin is not a list.
 *   - IB_EALLOC Failed allocation.
 */
static ib_status_t tfn_to_names(
    void             *instdata,
    ib_mm_t            mm,
    const ib_field_t  *fin,
    const ib_field_t **fout,
    void             *fndata
) {
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
    rc = ib_list_create(&new_value, mm);
    if (rc != IB_OK) {
        return IB_EALLOC;
    }

    /* If we haven't failed yet, make a new output field holding the created
     * list, new_value. */
    rc = ib_field_create(
        &fnew,
        mm,
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
            (const ib_field_t *)ib_list_node_data_const(node);
        const ib_field_t *list_out_field;

        rc = tfn_to_name_common(mm, list_field, &list_out_field);
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
 * @param[in] instdata Instance data. Unused.
 * @param[in] mm Memory manager to use for allocations.
 * @param[in] fin Input field.
 * @param[out] fout Output field. This is NULL on error.
 * @param[in] fndata Callback data
 *
 * @returns IB_OK if successful.
 */
static ib_status_t tfn_normalize_path_win(
    void              *instdata,
    ib_mm_t            mm,
    const ib_field_t  *fin,
    const ib_field_t **fout,
    void              *fndata
) {
    assert(fin != NULL);
    assert(fout != NULL);

    ib_status_t rc;

    rc = normalize_path(mm, fin, true, fout);

    return rc;
}

/**
 * Return the first field in a list of fields.
 *
 * If the input field is not a list, then @a fout is set to @a fin.
 *
 * @param[in] instdata Instance data. Unused.
 * @param[in] mm Memory manager for allocations.
 * @param[in] fin Input field. This should be a list.
 * @param[out] fout This will be the first element in @a fin, if fin is a list.
 *             Otherwise, this will be set to @a fin.
 * @param[in] cbdata Callback data.
 *
 * @returns
 * - IB_OK On success.
 * - Other on error.
 */
static ib_status_t tfn_first(
    void              *instdata,
    ib_mm_t            mm,
    const ib_field_t  *fin,
    const ib_field_t **fout,
    void              *cbdata
) {
    assert(fin != NULL);
    assert(fout != NULL);

    const ib_list_t      *list;
    ib_status_t           rc;

    rc = ib_field_value_type(fin, ib_ftype_list_out(&list), IB_FTYPE_LIST);
    if (rc == IB_EINVAL) {
        *fout = fin;
        return IB_OK;
    }
    if (rc == IB_OK) {
        const ib_list_node_t *node = ib_list_first_const(list);
        *fout = (const ib_field_t *)ib_list_node_data_const(node);
        return IB_OK;
    }

    return rc;
}

/**
 * Return the last field in a list of fields.
 *
 * If the input field is not a list, then @a fout is set to @a fin.
 *
 * @param[in] instdata Instance data. Unused.
 * @param[in] mm Memory manager for allocations.
 * @param[in] fin Input field. This should be a list.
 * @param[out] fout This will be the last element in @a fin, if fin is a list.
 *             Otherwise, this will be set to @a fin.
 * @param[in] cbdata Callback data.
 *
 * @returns
 * - IB_OK On success.
 * - Other on error.
 */
 static ib_status_t tfn_last(
    void              *instdata,
    ib_mm_t            mm,
    const ib_field_t  *fin,
    const ib_field_t **fout,
    void              *cbdata
) {
    assert(fin != NULL);
    assert(fout != NULL);

    const ib_list_t      *list;
    ib_status_t           rc;

    rc = ib_field_value_type(fin, ib_ftype_list_out(&list), IB_FTYPE_LIST);
    if (rc == IB_EINVAL) {
        *fout = fin;
        return IB_OK;
    }
    if (rc == IB_OK) {
        const ib_list_node_t *node = ib_list_last_const(list);
        *fout = ib_list_node_data_const(node);
        return IB_OK;
    }

    return rc;
}

/**
 * Initialize the core transformations
 **/
ib_status_t ib_core_transformations_init(ib_engine_t *ib, ib_module_t *mod)
{
    ib_status_t rc;

    /* First and Last list transformations. */
    rc = ib_tfn_create_and_register(
        NULL,
        ib,
        "first",
        true,
        NULL,      NULL,
        tfn_first, NULL,
        NULL,      NULL
    );
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_tfn_create_and_register(
        NULL,
        ib,
        "last",
        true,
        NULL, NULL,
        tfn_last, NULL,
        NULL, NULL
    );
    if (rc != IB_OK) {
        return rc;
    }

    /* Define transformations. */
    rc = ib_tfn_create_and_register(
        NULL,
        ib,
        "lowercase",
        false,
        NULL,          NULL,
        tfn_lowercase, NULL,
        NULL,          NULL
    );
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_tfn_create_and_register(
        NULL,
        ib,
        "trimLeft",
        false,
        NULL, NULL,
        tfn_trim_left, NULL,
        NULL, NULL
    );
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_tfn_create_and_register(
        NULL,
        ib,
        "trimRight",
        false,
        NULL, NULL,
        tfn_trim_right, NULL,
        NULL, NULL
    );
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_tfn_create_and_register(
        NULL,
        ib,
        "trim",
        false,
        NULL, NULL,
        tfn_trim, NULL,
        NULL, NULL
    );
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_tfn_create_and_register(
        NULL,
        ib,
        "removeWhitespace",
        false,
        NULL, NULL,
        tfn_wspc_remove, NULL,
        NULL, NULL
    );
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_tfn_create_and_register(
        NULL,
        ib,
        "compressWhitespace",
        false,
        NULL, NULL,
        tfn_wspc_compress, NULL,
        NULL, NULL
    );
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_tfn_create_and_register(
        NULL,
        ib,
        "length",
        true,
        NULL, NULL,
        tfn_length, NULL,
        NULL, NULL
    );
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_tfn_create_and_register(
        NULL,
        ib,
        "count",
        true,
        NULL, NULL,
        tfn_count, NULL,
        NULL, NULL
    );
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_tfn_create_and_register(
        NULL,
        ib,
        "max",
        true,
        NULL, NULL,
        tfn_max, NULL,
        NULL, NULL
    );
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_tfn_create_and_register(
        NULL,
        ib,
        "min",
        true,
        NULL, NULL,
        tfn_min, NULL,
        NULL, NULL
    );
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_tfn_create_and_register(
        NULL,
        ib,
        "urlDecode",
        false,
        NULL, NULL,
        tfn_url_decode, NULL,
        NULL, NULL
    );
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_tfn_create_and_register(
        NULL,
        ib,
        "htmlEntityDecode",
        false,
        NULL, NULL,
        tfn_html_entity_decode, NULL,
        NULL, NULL
    );
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_tfn_create_and_register(
        NULL,
        ib,
        "normalizePath",
        false,
        NULL, NULL,
        tfn_normalize_path, NULL,
        NULL, NULL
    );
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_tfn_create_and_register(
        NULL,
        ib,
        "normalizePathWin",
        false,
        NULL, NULL,
        tfn_normalize_path_win, NULL,
        NULL, NULL
    );
    if (rc != IB_OK) {
        return rc;
    }

    /* Math transformations. */
    rc = ib_tfn_create_and_register(
        NULL,
        ib,
        "iround",
        false,
        NULL, NULL,
        tfn_iround, NULL,
        NULL, NULL
    );
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_tfn_create_and_register(
        NULL,
        ib,
        "iceil",
        false,
        NULL, NULL,
        tfn_iceil, NULL,
        NULL, NULL
    );
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_tfn_create_and_register(
        NULL,
        ib,
        "ifloor",
        false,
        NULL, NULL,
        tfn_ifloor, NULL,
        NULL, NULL
    );
    if (rc != IB_OK) {
        return rc;
    }

    /* TODO - Backwards compatibility. This should be removed in IronBee 1. */
    rc = ib_tfn_create_and_register(
        NULL,
        ib,
        "round",
        false,
        NULL, NULL,
        tfn_iround, NULL,
        NULL, NULL
    );
    if (rc != IB_OK) {
        return rc;
    }

    /* TODO - Backwards compatibility. This should be removed in IronBee 1. */
    rc = ib_tfn_create_and_register(
        NULL,
        ib,
        "ceil",
        false,
        NULL, NULL,
        tfn_iceil, NULL,
        NULL, NULL
    );
    if (rc != IB_OK) {
        return rc;
    }

    /* TODO - Backwards compatibility. This should be removed in IronBee 1. */
    rc = ib_tfn_create_and_register(
        NULL,
        ib,
        "floor",
        false,
        NULL, NULL,
        tfn_ifloor, NULL,
        NULL, NULL
    );
    if (rc != IB_OK) {
        return rc;
    }

    /* Type conversion transformations. */
    rc = ib_tfn_create_and_register(
        NULL,
        ib,
        "toString",
        false,
        NULL, NULL,
        tfn_to_string, NULL,
        NULL, NULL
    );
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_tfn_create_and_register(
        NULL,
        ib,
        "toInteger",
        false,
        NULL, NULL,
        tfn_to_integer, NULL,
        NULL, NULL
    );
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_tfn_create_and_register(
        NULL,
        ib,
        "toFloat",
        false,
        NULL, NULL,
        tfn_to_float, NULL,
        NULL, NULL
    );
    if (rc != IB_OK) {
        return rc;
    }

    /* Name extraction transformations. */
    rc = ib_tfn_create_and_register(
        NULL,
        ib,
        "name",
        false,
        NULL, NULL,
        tfn_to_name, NULL,
        NULL, NULL
    );
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_tfn_create_and_register(
        NULL,
        ib,
        "names",
        false,
        NULL, NULL,
        tfn_to_names, NULL,
        NULL, NULL
    );
    if (rc != IB_OK) {
        return rc;
    }

    return IB_OK;
}
