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
 * @brief IronBee --- String / value pair mapping functions
 *
 * @author Brian Rectanus <brectanus@qualys.com>
 * @author Nick LeRoy <nleroy@qualys.com>
 */
#include <ironbee/strval.h>

#include "ironbee_config_auto.h"

#include <ironbee/list.h>

#include <assert.h>
#include <strings.h>

ib_status_t ib_strval_lookup(
    const ib_strval_t *map,
    const char        *str,
    uint64_t          *pval)
{
    if ( (map == NULL) || (str == NULL) || (pval == NULL) ) {
        return IB_EINVAL;
    }

    const ib_strval_t *rec = map;

    while (rec->str != NULL) {
        if (strcasecmp(str, rec->str) == 0) {
            *pval = rec->val;
            return IB_OK;
        }
        ++rec;
    }

    *pval = 0;
    return IB_ENOENT;
}

ib_status_t ib_strval_ptr_lookup(
    const ib_strval_ptr_t  *map,
    const char             *str,
    const void            **pptr)
{
    if ( (map == NULL) || (str == NULL) || (pptr == NULL) ) {
        return IB_EINVAL;
    }

    const ib_strval_ptr_t *rec = map;

    while (rec->str != NULL) {
        if (strcasecmp(str, rec->str) == 0) {
            *pptr = rec->val;
            return IB_OK;
        }
        ++rec;
    }

    *pptr = NULL;
    return IB_ENOENT;
}

ib_status_t ib_strval_data_lookup(
    const ib_strval_data_t  *map,
    size_t                   rec_size,
    const char              *str,
    const void             **pptr)
{
    if ( (map == NULL) || (str == NULL) || (pptr == NULL) ||
         (rec_size <= sizeof(ib_strval_data_t)) ) {
        return IB_EINVAL;
    }

    const uint8_t          *rptr;
    const ib_strval_data_t *rec;

    for (rptr = (const uint8_t *)map, rec = (const ib_strval_data_t *)rptr;
         rec->str != NULL;
         rptr += rec_size, rec = (const ib_strval_data_t *)rptr)
    {
        if (strcasecmp(str, rec->str) == 0) {
            *pptr = &(rec->data);
            return IB_OK;
        }
    }

    *pptr = NULL;
    return IB_ENOENT;
}
