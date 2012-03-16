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
 * @brief IronBee - String related functions
 * @author Nick LeRoy <nleroy@qualys.com>
 */

#include "ironbee_config_auto.h"

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <assert.h>
#include <errno.h>
#include <ctype.h>

#include <ironbee/types.h>
#include <ironbee/debug.h>
#include <ironbee/types.h>
#include <ironbee/string.h>

/**
 * Length of the string buffer for converting strings.
 */
#define NUM_BUF_LEN 64

/**
 * Convert a string (with length) to a number.
 */
ib_status_t ib_string_to_num_ex(const char *s,
                                size_t slen,
                                int base,
                                ib_num_t *result)
{
    IB_FTRACE_INIT();
    assert(result != NULL);

    char buf[NUM_BUF_LEN+1];
    ib_status_t rc;

    /* Check for zero length string */
    if ( (s == NULL) || (slen > NUM_BUF_LEN) || (slen == 0) ) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    /* Copy the string to a buffer, let string_to_num() do the real work */
    memcpy(buf, s, slen);
    buf[slen] = '\0';
    rc = ib_string_to_num(buf, base, result);
    IB_FTRACE_RET_STATUS(rc);
}

/**
 * Convert a string (with length) to a number.
 */
ib_status_t ib_string_to_num(const char *s,
                             int base,
                             ib_num_t *result)
{
    IB_FTRACE_INIT();
    assert(result != NULL);

    size_t slen = strlen(s);
    char *end;
    long int value;
    size_t vlen;

    /* Check for zero length string */
    if ( (s == NULL) || (*s == '\0') ) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    /* Do the conversion, check for errors */
    value = strtol(s, &end, base);
    vlen = (end - s);
    if (vlen != slen) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }
    else if ( ((value == LONG_MIN) || (value == LONG_MAX)) &&
              (errno == ERANGE) )
    {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }
    else {
        *result = value;
        IB_FTRACE_RET_STATUS(IB_OK);
    }
}

/**
 * strstr() clone that works with non-NUL terminated strings
 */
const char *ib_strstr_ex(const char *haystack,
                         size_t      haystack_len,
                         const char *needle,
                         size_t      needle_len)
{
    IB_FTRACE_INIT();
    size_t i = 0;
    size_t imax;

    /* If either pointer is NULL or either length is zero, done */
    if ( (haystack == NULL) || (haystack_len == 0) ||
         (needle == NULL) || (needle_len == 0) )
    {
        IB_FTRACE_RET_CONSTSTR(NULL);
    }

    /* Search for the needle */
    imax = haystack_len - (needle_len-1);
    for (i = 0; i < imax; ++i) {
        const char *hp = haystack + i;
        ib_bool_t found = IB_TRUE;
        size_t j = 0;

        for (j = 0; j < needle_len; ++j) {
            if ( *(hp + j) != *(needle + j) ) {
                found = IB_FALSE;
                break;
            }
        }
        if (found == IB_TRUE) {
            IB_FTRACE_RET_CONSTSTR(hp);
        }
    }

    IB_FTRACE_RET_CONSTSTR(NULL);
}

/*
 * Simple ASCII lowercase function.
 */
ib_status_t ib_strlower_ex(uint8_t *data,
                           size_t dlen,
                           ib_bool_t *modified)
{
    IB_FTRACE_INIT();
    size_t i = 0;
    int modcount = 0;

    assert(data != NULL);
    assert(modified != NULL);

    while(i < dlen) {
        int c = *(data+i);
        *(data+i) = tolower(c);
        if (c != *(data+i)) {
            ++modcount;
        }
        i++;
    }

    /* Note if any modifications were made. */
    *modified = (modcount != 0);

    IB_FTRACE_RET_STATUS(IB_OK);
}

/*
 * Simple ASCII lowercase function.
 */
ib_status_t ib_strlower(char *data,
                        ib_bool_t *modified)
{
    IB_FTRACE_INIT();
    ib_status_t rc;

    assert(data != NULL);
    assert(modified != NULL);

    rc = ib_strlower_ex((uint8_t *)data, strlen(data), modified);
    IB_FTRACE_RET_STATUS(rc);
}

/**
 * Simple ASCII trimLeft function.
 * @internal
 */
ib_status_t ib_strtrim_left_ex(uint8_t *data_in,
                               size_t dlen_in,
                               uint8_t **data_out,
                               size_t *dlen_out,
                               ib_bool_t *modified)
{
    IB_FTRACE_INIT();
    size_t i = 0;
    *modified = IB_FALSE;

    assert(data_in != NULL);
    assert(data_out != NULL);
    assert(dlen_out != NULL);
    assert(modified != NULL);

    if (dlen_in == 0) {
        *dlen_out = 0;
        *data_out = data_in;
        IB_FTRACE_RET_STATUS(IB_OK);
    }

    while(i < dlen_in) {
        if (isspace(data_in[i]) == 0) {
            *data_out = data_in + i;
            *dlen_out = dlen_in - i;
            if (i != 0) {
                *modified = IB_TRUE;
            }
            IB_FTRACE_RET_STATUS(IB_OK);
        }
        i++;
    }

    *modified = IB_TRUE;
    *dlen_out = 0;
    *data_out = data_in;

    IB_FTRACE_RET_STATUS(IB_OK);
}

/*
 * Simple ASCII trimLeft function.
 */
ib_status_t ib_strtrim_left(char *data_in,
                            char **data_out,
                            ib_bool_t *modified)
{
    IB_FTRACE_INIT();
    ib_status_t rc;
    size_t olen;

    assert(data_in != NULL);
    assert(data_out != NULL);
    assert(modified != NULL);

    /* _ex version does the real work */
    rc = ib_strtrim_left_ex((uint8_t *)data_in, strlen(data_in),
                            (uint8_t **)data_out, &olen,
                            modified);
    if ( (rc != IB_OK) || (*modified == IB_FALSE) ) {
        IB_FTRACE_RET_STATUS(rc);
    }

    *((*data_out)+olen) = '\0';
    IB_FTRACE_RET_STATUS(rc);
}

/*
 * Simple ASCII trimRight function.
 */
ib_status_t ib_strtrim_right_ex(uint8_t *data_in,
                                size_t dlen_in,
                                uint8_t **data_out,
                                size_t *dlen_out,
                                ib_bool_t *modified)
{
    IB_FTRACE_INIT();
    uint8_t *cp;      /* Current pointer */
    uint8_t *ep;      /* End pointer */

    assert(data_in != NULL);
    assert(data_out != NULL);
    assert(dlen_out != NULL);
    assert(modified != NULL);

    /* This is an in-place transformation which may change
     * the data length.
     */
    *modified = IB_FALSE;
    *data_out = data_in;
    
    for(cp = ep = data_in + (dlen_in - 1);  cp >= data_in;  --cp) {
        if (isspace(*cp) == 0) {
            break;
        }
    }

    if (cp != ep) {
        *modified = IB_TRUE;
    }
    *dlen_out = (cp - data_in) + 1;
    IB_FTRACE_RET_STATUS(IB_OK);
}

/*
 * Simple ASCII trimRight function.
 */
ib_status_t ib_strtrim_right(char *data_in,
                             char **data_out,
                             ib_bool_t *modified)
{
    IB_FTRACE_INIT();
    ib_status_t rc;
    size_t olen;

    assert(data_in != NULL);
    assert(data_out != NULL);
    assert(modified != NULL);

    /* _ex version does the real work */
    rc = ib_strtrim_right_ex((uint8_t *)data_in, strlen(data_in),
                             (uint8_t **)data_out, &olen,
                             modified);
    if ( (rc != IB_OK) || (*modified == IB_FALSE) ) {
        IB_FTRACE_RET_STATUS(rc);
    }
    if (*modified) {
        *((*data_out)+olen) = '\0';
    }
    IB_FTRACE_RET_STATUS(rc);
}

/*
 * Simple ASCII trim function.
 */
ib_status_t ib_strtrim_lr_ex(uint8_t *data_in,
                             size_t dlen_in,
                             uint8_t **data_out,
                             size_t *dlen_out,
                             ib_bool_t *modified)
{
    IB_FTRACE_INIT();
    ib_bool_t lmod = IB_FALSE;
    ib_bool_t rmod = IB_FALSE;
    ib_status_t rc;

    assert(data_in != NULL);
    assert(data_out != NULL);
    assert(dlen_out != NULL);
    assert(modified != NULL);

    /* Just call the other trim functions. */
    rc = ib_strtrim_left_ex(data_in, dlen_in, data_out, dlen_out, &lmod);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = ib_strtrim_right_ex(*data_out, *dlen_out, data_out, dlen_out, &rmod);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    *modified = ((lmod == IB_TRUE) || (rmod == IB_TRUE)) ? IB_TRUE : IB_FALSE;
    return rc;

}

/*
 * Simple ASCII trim function.
 */
ib_status_t ib_strtrim_lr(char *data_in,
                          char **data_out,
                          ib_bool_t *modified)
{
    IB_FTRACE_INIT();
    size_t len;
    ib_bool_t lmod = IB_FALSE;
    ib_bool_t rmod = IB_FALSE;
    ib_status_t rc;

    assert(data_in != NULL);
    assert(data_out != NULL);
    assert(modified != NULL);

    len = strlen(data_in);
    if (len == 0) {
        *data_out = data_in;
        *modified = IB_FALSE;
        IB_FTRACE_RET_STATUS(IB_OK);
    }

    /* Trim left */
    if (len != 0) {
        rc = ib_strtrim_left_ex((uint8_t *)data_in, len,
                                (uint8_t **)data_out, &len,
                                &lmod);
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }
    }

    /* Trim right */
    if (len != 0) {
        rc = ib_strtrim_right_ex((uint8_t *)*data_out, len,
                                 (uint8_t **)data_out, &len,
                                 &rmod);
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }
    }

    *modified = ((lmod == IB_TRUE) || (rmod == IB_TRUE)) ? IB_TRUE : IB_FALSE;
    if (*modified) {
        *((*data_out)+len) = '\0';
    }
    IB_FTRACE_RET_STATUS(IB_OK);
}
