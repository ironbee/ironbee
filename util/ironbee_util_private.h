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

#ifndef _IB_UTIL_PRIVATE_H_
#define _IB_UTIL_PRIVATE_H_

/**
 * @file
 * @brief IronBee &mdash; Private Utility Declarations
 *
 * @author Brian Rectanus <brectanus@qualys.com>
 * @author Pablo Rincon <pablo.rincon.crespo@gmail.com>
 */

#include <ironbee/types.h>
#include <ironbee/mpool.h>
#include <ironbee/list.h>
#include <ironbee/hash.h>
#include <ironbee/field.h>
#include <ironbee/radix.h>
#include <ironbee/ahocorasick.h>
#include <ironbee/lock.h>

#include <sys/types.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Field value structure.
 *
 * This allows for multiple types of values to be stored within a field.
 */
struct ib_field_val_t {
    ib_field_get_fn_t  fn_get;        /**< Function to get a value. */
    ib_field_set_fn_t  fn_set;        /**< Function to set a value. */
    void              *cbdata_get;    /**< Data passed to fn_get. */
    void              *cbdata_set;    /**< Data passed to fn_get. */
    void              *pval;          /**< Address where value is stored */
    union {
        ib_num_t       num;           /**< Generic numeric value */
        ib_unum_t      unum;          /**< Generic unsigned numeric value */
        ib_bytestr_t  *bytestr;       /**< Byte string value */
        char          *nulstr;        /**< NUL string value */
        ib_list_t     *list;          /**< List of fields */
        ib_stream_t   *stream;        /**< Stream buffer */
        void          *ptr;           /**< Pointer value */
    } u;
};

/**
 * Dynamic array structure.
 */
struct ib_array_t {
    ib_mpool_t       *mp;
    size_t            ninit;
    size_t            nextents;
    size_t            nelts;
    size_t            size;
    void             *extents;
};

/**
 * Calculate the extent index from the array index for an array.
 *
 * @param arr Array
 * @param idx Array index
 *
 * @returns Extent index where data resides
 */
#define IB_ARRAY_EXTENT_INDEX(arr,idx) \
    ((idx) / (arr)->ninit)

/**
 * Calculate the data index from the array and extent indexes for an array.
 *
 * @param arr Array
 * @param idx Array index
 * @param extent_idx Extent index (via @ref IB_ARRAY_EXTENT_INDEX)
 *
 * @returns Data index where data resides within the given extent
 */
#define IB_ARRAY_DATA_INDEX(arr,idx,extent_idx) \
    ((idx) - ((extent_idx) * (arr)->ninit))

/**
 * Set to 1 the specified bit index of a byte array
 * Warning: The bit offset/index starts from the HSB
 *
 * @param byte Array of byte (uint8_t*)
 * @param bit index of bit
 */
#define IB_SET_BIT_ARRAY(byte, bit) \
    (byte[bit / 8] |= (0x01 << (7 - (bit % 8))));

/**
 * Read a bit from the specified byte
 * Warning: The bit offset/index starts from the HSB
 *
 * @param byte Byte to look at (uint8_t)
 * @param bit index of bit
 * @returns 0 or 1
 */
#define IB_READ_BIT(byte, bit) \
    ((byte >> (7 - ((bit) % 8)) ) & 0x01)

/**
 * Calculate the size in bytes to hold a prefix of length bits
 *
 * @param bits The number of bits we want to store
 * @returns size in bytes needed for that bits
 */
#define IB_BITS_TO_BYTES(bits) \
    (((bits) % 8 == 0) ? ((bits) / 8) : ((bits) / 8) + 1)

/**
 * Set to 1 the specified bit index of a byte
 * Warning: The bit offset/index starts from the HSB
 *
 * @param byte Byte to look at (uint8_t)
 * @param bit index of bit
 */
#define IB_SET_BIT(byte, bit) \
    (byte |= (0x01 << (7 - (bit % 8))));

/**
 * Read the HSB of a byte
 *
 * @param byte Byte to look at (uint8_t)
 * @returns 0 or 1
 */
#define IB_GET_DIR(byte) \
    (((byte) >> 7) & 0x01)

/**
 * Prefix for radix nodes
 */
struct ib_radix_prefix_t {
    uint8_t *rawbits;
    uint8_t prefixlen;
};

/**
 * Radix node structure
 */
struct ib_radix_node_t {
    ib_radix_prefix_t *prefix;

    struct ib_radix_node_t *zero;
    struct ib_radix_node_t *one;

    void *data;
};

/**
 * Radix tree structure
 */
struct ib_radix_t {
    ib_radix_node_t *start;

    ib_radix_update_fn_t update_data;
    ib_radix_print_fn_t  print_data;
    ib_radix_free_fn_t   free_data;

    size_t data_cnt;

    ib_mpool_t *mp;
};

/**
 * Matching functions type helper
 */
enum {
    IB_RADIX_PREFIX,
    IB_RADIX_CLOSEST,
};

/**
 * Return if the given prefix is IPV4
 *
 * @param[in] cidr const char * with format ip/mask where mask is optional
 * @returns 1 if true, 0 if false
 */
#define IB_RADIX_IS_IPV4(cidr) ((strchr(cidr, ':') == NULL) ? 1 : 0)

/**
 * Return if the given prefix is IPV6
 *
 * @param cidr const char * with format ip/mask where mask is optional
 * @returns 1 if true, 0 if false
 */
#define IB_RADIX_IS_IPV6(cidr) ((strchr(cidr, ':') != NULL) ? 1 : 0)

/**
 * Determine if the given prefix is IPV4
 *
 * @param[in] cidr const char * with format ip/mask where mask is optional
 * @param[in] len length of the str
 * @param[out] result Result: true / false
 *
 * @returns Status code
 */
#define IB_RADIX_IS_IPV4_EX(cidr,len,result)                          \
    ib_radix_is_ipv4_ex((cidr), (len), &result)

/**
 * Determine if the given prefix is IPV6
 *
 * @param[in] cidr const char * with format ip/mask where mask is optional
 * @param[in] len length of the str
 * @param[out] result Result: true / false
 *
 * @returns Status code
 */
#define IB_RADIX_IS_IPV6_EX(cidr,len,result)                          \
    ib_radix_is_ipv6_ex((cidr), (len), &result)

/**
 * Determine if the given prefix is IPV4
 *
 * @param[in] str const char * with format ip/mask where mask is optional
 * @param[in] len length of the str
 * @param[out] result Result: true / false
 *
 * @returns Status code
 */
ib_status_t ib_radix_is_ipv4_ex(const char *str,
                                size_t len,
                                bool *result);

/**
 * Determine if the given prefix is IPv6
 *
 * @param[in] str const char * with format ip/mask where mask is optional
 * @param[in] len length of the str
 * @param[out] result Result: true / false
 *
 * @returns Status code
 */
ib_status_t ib_radix_is_ipv6_ex(const char *str,
                                size_t len,
                                bool *result);

/**
 * Look for a character in a string that can have embedded NUL characters
 * in it.  This version will ignore NUL characters.
 *
 * @param[in] str String to search
 * @param[in] len length of the str
 * @param[in] c The character to search for
 * @param[out] offset Offset of the character; -1 if not found
 *
 * @return Status code
 */
ib_status_t ib_radix_strchr_nul_ignore(const char *str,
                                       size_t len,
                                       int c,
                                       ssize_t *offset);

/**
 * Look for a character in a string that can have embedded NUL characters
 * in it.  This version returns an error if a NUL character is encountered
 * before len chars.
 *
 * @param[in] str String to search
 * @param[in] len length of the str
 * @param[in] c The character to search for
 * @param[out] offset Offset of the character; -1 if not found
 *
 * @return Status code
 */
ib_status_t ib_radix_strchr_nul_error(const char *str,
                                      size_t len,
                                      int c,
                                      ssize_t *offset);

/**
 *@ internal
 * bin tree for fast goto() function of aho corasick
 */
typedef struct ib_ac_bintree_t ib_ac_bintree_t;

/**
 * Aho Corasick state state, used to represent a state
 */
struct ib_ac_state_t {
    ib_ac_char_t       letter;    /**< the char to go to this state */

    uint8_t            flags;     /**< flags for this state */
    size_t             level;     /**< level in the tree (depth/length) */

    ib_ac_state_t     *fail;      /**< state to go to if goto() fail */
    ib_ac_state_t     *outputs;   /**< pointer to other matching states */

    ib_ac_state_t     *child;     /**< next state to goto() of next level */
    ib_ac_state_t     *sibling;   /**< sibling state (linked list) */
    ib_ac_state_t     *parent;    /**< parent state */

    ib_ac_bintree_t   *bintree;   /**< bintree to speed up the goto() search*/

    uint32_t           match_cnt; /**< match count of this state */

    ib_ac_char_t      *pattern;   /**< (sub) pattern path to this state */

    ib_ac_callback_t   callback;  /**< callback function for matches */
    void              *data;      /**< callback (or match entry) extra params */

};

/**
 * binary tree that performs the Aho Corasick goto function for a given
 * state and letter
 */
struct ib_ac_bintree_t {
    ib_ac_char_t       letter;    /**< the current char */
    ib_ac_state_t     *state;     /**< the goto() state for letter */

    ib_ac_bintree_t   *left;      /**< chars lower than current */
    ib_ac_bintree_t   *right;     /**< chars greater than current */
};


#ifdef __cplusplus
}
#endif

#endif /* IB_UTIL_PRIVATE_H_ */

