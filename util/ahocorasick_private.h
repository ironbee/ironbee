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

#ifndef _IB_AHOCORASICK_PRIVATE_H_
#define _IB_AHOCORASICK_PRIVATE_H_

/**
 * @file
 * @brief IronBee &mdash; Private Utility Declarations
 *
 * @author Brian Rectanus <brectanus@qualys.com>
 * @author Pablo Rincon <pablo.rincon.crespo@gmail.com>
 */

#include <ironbee/ahocorasick.h>

#ifdef __cplusplus
extern "C" {
#endif

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
    void              *data;   /**< callback (or match entry) extra params */

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

#endif /* IB_AHOCORASICK_PRIVATE_H_ */
