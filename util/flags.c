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
 * @brief IronBee --- Flag utility functions
 *
 * @author Nick LeRoy <nleroy@qualys.com>
 */

#include "ironbee_config_auto.h"

#include <ironbee/flags.h>

#include <ironbee/list.h>
#include <ironbee/strval.h>

#include <assert.h>
#include <strings.h>

/**
 * Parse a single flag from a name/value pair mapping (internal version)
 *
 * @param[in] map String / value mapping
 * @param[in] str String to lookup in @a map
 * @param[out] poper Pointer to operator
 * @param[out] pflags Pointer to flags
 *
 * @returns Status code:
 *  - IB_OK: All OK,
 *  - IB_ENOENT: @a str not found in @a map
 */
static ib_status_t parse_single(
    const ib_strval_t    *map,
    const char           *str,
    ib_flags_op_t        *poper,
    ib_flags_t           *pflags)
{
    assert(map != NULL);
    assert(str != NULL);
    assert(poper != NULL);
    assert(pflags != NULL);

    ib_flags_op_t oper;
    ib_status_t   rc;

    oper = (*str == '-') ? FLAG_REMOVE : ((*str == '+') ? FLAG_ADD : FLAG_SET);

    /* Remove the operator from the name if required.
     * and determine the numeric value of the option
     * by using the value map.
     */
    if (oper != FLAG_SET) {
        ++str;
    }

    /* Parse the flag */
    rc = ib_strval_lookup(map, str, (uint64_t *)pflags);
    if (rc != IB_OK) {
        return rc;
    }

    /* Store off the operator */
    *poper = oper;

    return IB_OK;
}

/**
 * Apply a single flag operation
 *
 * @param[in] oper Operator to apply
 * @param[in] flags Flags to apply
 * @param[in] num Operator number
 * @param[out] pflags Pointer to flags
 * @param[out] pmask Pointer to flag mask
 *
 * @returns Status code:
 *  - IB_OK: All OK
 */
static ib_status_t apply_operation(
    ib_flags_op_t  oper,
    ib_flags_t     flags,
    int            num,
    ib_flags_t    *pflags,
    ib_flags_t    *pmask)
{
    assert(pflags != NULL);
    assert(pmask != NULL);

    /* If the first option does not use an operator, then
     * this is setting all flags so set all the mask bits.
     */
    if ( (num == 0) && (oper == FLAG_SET) ) {
        *pmask = ~0;
    }

    /* Mark which bit(s) we are setting. */
    *pmask |= flags;

    /* Set/Unset the appropriate bits. */
    if (oper == FLAG_REMOVE) {
        *pflags &= ~flags;
    }
    else {
        *pflags |= flags;
    }

    return IB_OK;
}

ib_flags_t ib_flags_merge(
    ib_flags_t  inflags,
    ib_flags_t  flags,
    ib_flags_t  mask)
{
    return ( (flags & mask) | (inflags & ~mask) );
}

ib_status_t ib_flags_string(
    const ib_strval_t *map,
    const char        *str,
    int                num,
    ib_flags_t        *pflags,
    ib_flags_t        *pmask)
{
    if ( (map == NULL) || (str == NULL) ||
         (pflags == NULL) || (pmask == NULL) )
    {
        return IB_EINVAL;
    }

    ib_flags_op_t oper;
    ib_flags_t    flags;
    ib_status_t   rc;

    rc = parse_single(map, str, &oper, &flags);
    if (rc != IB_OK) {
        return rc;
    }

    rc = apply_operation(oper, flags, num, pflags, pmask);
    if (rc != IB_OK) {
        return rc;
    }

    return IB_OK;
}

ib_status_t ib_flags_strtok(
    const ib_strval_t *map,
    ib_mm_t            mm,
    const char        *str,
    const char        *sep,
    ib_flags_t        *pflags,
    ib_flags_t        *pmask)
{
    if ( (map == NULL) || (str == NULL) || (sep == NULL) ||
         (pflags == NULL) || (pmask == NULL) )
    {
        return IB_EINVAL;
    }

    int         n = 0;
    ib_flags_t  flags = 0;
    ib_flags_t  mask = 0;
    char       *copy;
    const char *tmp;

    /* Make a copy of the string that we can use for strtok */
    copy = ib_mm_strdup(mm, str);
    if (copy == NULL) {
        return IB_EALLOC;
    }

    /* Walk through the separated list */
    tmp = strtok(copy, sep);
    do {
        ib_status_t rc;
        rc = ib_flags_string(map, tmp, n++, &flags, &mask);
        if (rc != IB_OK) {
            return rc;
        }
    } while ( (tmp = strtok(NULL, sep)) != NULL);

    /* Done */
    *pflags = flags;
    *pmask = mask;
    return IB_OK;
}

ib_status_t ib_flags_strlist(
    const ib_strval_t  *map,
    const ib_list_t    *strlist,
    ib_flags_t         *pflags,
    ib_flags_t         *pmask,
    const char        **perror)
{
    if ( (map == NULL) || (strlist == NULL) ||
         (pflags == NULL) || (pmask == NULL) )
    {
        return IB_EINVAL;
    }

    int                   n = 0;
    const ib_list_node_t *node;

    IB_LIST_LOOP_CONST(strlist, node) {
        const char  *s = (const char *)node->data;
        ib_status_t  rc;

        rc = ib_flags_string(map, s, n++, pflags, pmask);
        if (rc != IB_OK) {
            if (perror != NULL) {
                *perror = s;
            }
            return rc;
        }
    }
    if (perror != NULL) {
        *perror = NULL;
    }
    return IB_OK;
}

ib_status_t ib_flags_oplist_parse(
    const ib_strval_t *map,
    ib_mm_t            mm,
    const char        *str,
    const char        *sep,
    ib_list_t         *oplist)
{
    if ( (map == NULL) || (str == NULL) || (sep == NULL) || (oplist == NULL) ) {
        return IB_EINVAL;
    }

    char       *copy;
    const char *tmp;

    /* Make a copy of the string that we can use for strtok */
    copy = ib_mm_strdup(mm, str);
    if (copy == NULL) {
        return IB_EALLOC;
    }

    /* Clear the list */
    ib_list_clear(oplist);

    /* Walk through the separated list, parser each operator, build the list */
    tmp = strtok(copy, sep);
    do {
        ib_status_t           rc;
        ib_flags_op_t         op;
        ib_flags_t            flags;
        ib_flags_operation_t *operation;

        rc = parse_single(map, tmp, &op, &flags);
        if (rc != IB_OK) {
            return rc;
        }
        operation = ib_mm_alloc(mm, sizeof(*operation));
        if (operation == NULL) {
            return IB_EALLOC;
        }
        operation->op = op;
        operation->flags = flags;
        rc = ib_list_push(oplist, operation);
        if (rc != IB_OK) {
            return rc;
        }
    } while ( (tmp = strtok(NULL, sep)) != NULL);

    return IB_OK;
}

ib_status_t ib_flags_oplist_apply(
    const ib_list_t   *oplist,
    ib_flags_t        *pflags,
    ib_flags_t        *pmask)
{
    if ( (oplist == NULL) || (pflags == NULL) || (pmask == NULL) ) {
        return IB_EINVAL;
    }

    int                   n = 0;
    const ib_list_node_t *node;

    IB_LIST_LOOP_CONST(oplist, node) {
        const ib_flags_operation_t *operation =
            (const ib_flags_operation_t *)node->data;
        ib_status_t  rc;

        rc = apply_operation(operation->op, operation->flags, n++,
                             pflags, pmask);
        if (rc != IB_OK) {
            return rc;
        }
    }

    return IB_OK;
}
