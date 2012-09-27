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

#ifndef _IA_VLS_H_
#define _IA_VLS_H_

/**
 * @file
 * @brief IronAutomata &mdash; Variable Length Structure Support
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup IronAutomataVariableLengthStructure Variable Length Structures
 * @ingroup IronAutomata
 *
 *
 * Macros to support working with variable length structures.
 *
 * A variable length structures, VLS, is a normal C structure that has
 * additional memory allocated past the end of it that holds either optional
 * or variable length members.
 *
 * Three types of variable members are supported by this module:
 *
 * - Optional members (IA_VLS_IF()).
 * - Variable length arrays of known length (IA_VLS_VARRAY()).
 * - Variable length arrays of unknown length (IA_VLS_VARRAY_FINAL()).
 *
 * In addition, optional variable length arrays are directly supported via
 * IA_VLS_VARRAY_IF().
 *
 * The following pseudocode structure illustrates these features along with
 * a possible notation for them.  Note that this is not valid C code.
 *
 * @code
 * struct example_vls_t
 * {
 *     bool has_a;
 *     bool has_b;
 *     size_t length_c;
 *     int a if has_a;
 *     int b if has_b;
 *     char c[length_c];
 *     char d[];
 * };
 * @endcode
 *
 * In the above, @c example_vls_t contains:
 * - Two optional members, @c a and @c b, whose presence is determined by
 *   always present members @c has_a and @c has_b.
 * - A variable length array, @c c, whose length is determined by the always
 *   present member @c length_c.
 * - A variable length array, @c d, whose length is unknown.  Note that such
 *   unknown length arrays may only exist at the end of the VLS.
 *
 * The optional and variable length members can have their offsets calculated
 * based on existing knowledge.  E.g., the offset of @c has_b depends on
 * whether @c has_a is present or not, but that can be determined at access
 * time from the always present member @c has_a.
 *
 * The IA_VLS_ macros work by creating and updating a pointer as members are
 * extracted.  They can be use directly, but if a VLS will be used in more
 * than one place, it may be useful to write a macro extracting the variable
 * members into local variables, e.g.,
 *
 * @code
 * #define EXAMPLE_IA_VLS_GET(base) \
 *   ia_vls_state_t vls_ # base; \
 *   IA_VLS_INIT(vls_ # base ); \
 *   int base # __a = IA_VLS_IF(vls_ # base, int, base->has_a); \
 *   int base # __b = IA_VLS_IF(vls_ # base, int, base->has_b); \
 *   char *base # __c = IA_VLS_VARRAY(vls_ # base, char, base->length_c); \
 *   char *base # __d = IA_VLS_FINAL(vls_ # base);
 *
 * void display_vls(example_vls_t *vls)
 * {
 *     EXAMPLE_IA_VLS_GET(vls);
 *
 *     if (vls->has_a) {
 *       printf("a: %d\n", vls__a);
 *     }
 *     if (vls->has_b) {
 *       printf("b: %d\n", vls__b);
 *     }
 *     printf("c: %*s\n", vls->length_c, vls__c);
 *     printf("d: %s\n", vls__d);
 * }
 * @endcode
 *
 * The above code illustrates how the VLS macros @e copy values into local
 * variables.  Note that pointers to values can be extracted instead via
 * IA_VLS_IF_PTR().
 **/

/**
 * Type for VLS state.
 *
 * @sa IA_VLS_INIT()
 */
typedef char *ia_vls_state_t;

/**
 * Initialize state @a state for the VLS @a vls.
 *
 * This macro must be called on @a state before any other macros using it.
 *
 * @param[out] state State to initialize.  Should be ia_vls_state_t.
 * @param[in] base Pointer to variable length structure to extract values
 *                 from.
 */
#define IA_VLS_INIT(state, base) \
    state = (char *)base + sizeof(*base)

/**
 * Extract optional field from @a base if @a flag.
 *
 * If @a flag is true, reads field of type @a fieldtype from VLS and advances
 * state.  If @a flag is false, state is unchanged and return value is
 * undefined.
 *
 * If you want to write to a VLS, use IA_VLS_IF_PTR() instead.
 *
 * @param[in,out] state     State of VLS.  See IA_VLS_INIT().
 * @param[in]     fieldtype Type of field.
 * @param[in]     default   What to return if not present.
 * @param[in]     flag      Whether field is present.
 * @return Value of field if @a flag and undefined otherwise.
 */
#define IA_VLS_IF(state, fieldtype, default, flag) \
    ({ \
        fieldtype vls_value = default; \
        if (flag) { \
            vls_value = *(fieldtype *)state; \
            state += sizeof(fieldtype); \
        } \
        vls_value; \
    })

/**
 * As above, but does not return value.
 *
 * Useful for ignoring fields without compiler warnings.
 *
 * @param[in,out] state     State of VLS.  See IA_VLS_INIT().
 * @param[in]     fieldtype Type of field.
 * @param[in]     flag      Whether field is present.
 */
#define IA_VLS_ADVANCE_IF(state, fieldtype, flag) \
    state += (flag) ? sizeof(fieldtype) : 0

/**
 * As above, but returns pointer to field.
 *
 * @return Pointer to field if @a flag and NULL otherwise.
 */
#define IA_VLS_IF_PTR(state, fieldtype, flag) \
    ({ \
        fieldtype *vls_value = NULL; \
        if (flag) { \
            vls_value = (fieldtype *)state; \
            state += sizeof(fieldtype); \
        } \
        vls_value; \
    })

/**
 * Extract variable length array from VLS if @a flag.
 *
 * If @a flag is true, reads field of type @c elementtype* from VLS and
 * advances state.  If @a flag is false, state is unchanged and NULL is
 * returned.
 *
 * @param[in,out] state       State of VLS.  See IA_VLS_INIT().
 * @param[in]     elementtype Type of array element.
 * @param[in]     length      Length of array.
 * @param[in]     flag        Whether field is present.
 * @return Pointer to beginning of array if @a flag and NULL otherwise.
 */
#define IA_VLS_VARRAY_IF(state, elementtype, length, flag) \
    ({ \
        elementtype *vls_value = NULL; \
        if (flag) { \
            vls_value = (elementtype *)state; \
            state += sizeof(elementtype) * length; \
        } \
        vls_value; \
    })

/**
 * Extract variable length array from VLS.
 *
 * This is equivalent to IA_VLS_IF(state, elementtype, length, true).
 *
 * @param[in,out] state       State of VLS.  See IA_VLS_INIT().
 * @param[in]     elementtype Type of array element.
 * @param[in]     length      Length of array.
 * @return Pointer to beginning of array.
 */
#define IA_VLS_VARRAY(state, elementtype, length) \
    IA_VLS_VARRAY_IF(state, elementtype, length, true)

/**
 * Extract pointer to remainder of data in VLS.
 *
 * This macro is usually used to extract a variable length array of unknown
 * length from the end of the VLS.  This does not update state and should
 * only be used as the final IA_VLS_ call.
 *
 * @param[in] state       State of VLS.  See IA_VLS_INIT().
 * @param[in] elementtype Type of array element.
 * @return Pointer of type @c elementtype* to remainder of data in VLS.
 */
#define IA_VLS_FINAL(state, elementtype) \
    ((elementtype *)(state))

/**
 * @} IronAutomataVariableLengthStructure
 */

#ifdef __cplusplus
}
#endif

#endif /* _IA_VLS_H_ */
