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

#ifndef _IB_AHOCORASICK_H_
#define _IB_AHOCORASICK_H_

/**
 * @file
 * @brief IronBee - AhoCorasick Utility Functions
 *
 * @author Brian Rectanus <brectanus@qualys.com>
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>

#include <sys/types.h>
#include <unistd.h>

#include <netinet/in.h>
#include <arpa/inet.h>


#include <ironbee/build.h>
#include <ironbee/release.h>
#include <ironbee/types.h>
#include <ironbee/array.h>
#include <ironbee/list.h>
#include <ironbee/field.h>

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @defgroup IronBeeUtilAhoCorasick AhoCorasick Utilities
 * @ingroup IronBeeUtil
 * @{
 */

/* General flags for the parser (and matcher) */
#define IB_AC_FLAG_PARSER_NOCASE    0x01 /**< Case insensitive */
#define IB_AC_FLAG_PARSER_COMPILED  0x02 /**< "Compiled", failure state 
                                              links and output state links 
                                              are built */
#define IB_AC_FLAG_PARSER_READY     0x04 /**< the ac automata is ready */

/* Node specific flags */
#define IB_AC_FLAG_STATE_OUTPUT     0x01 /**< This flag indicates that
                                              the current state produce
                                              an output */

/* Flags for the consume() function (matching function) */
#define IB_AC_FLAG_CONSUME_DEFAULT       0x00 /**< No match list, no
                                                   callback, returns on
                                                   the first match
                                                   (if any) */

#define IB_AC_FLAG_CONSUME_MATCHALL      0x01 /**< Should be used in
                                                   combination to dolist 
                                                   or docallback. Otherwise
                                                   you are wasting cycles*/

#define IB_AC_FLAG_CONSUME_DOLIST        0x02 /**< Enable the storage of a
                                                   list of matching states 
                                                   (match_list located at 
                                                   the matching context */

#define IB_AC_FLAG_CONSUME_DOCALLBACK    0x04 /**< Enable the callback
                                                   (you must also register
                                                   the pattern with the
                                                   callback) */


typedef struct ib_ac_t ib_ac_t;
typedef struct ib_ac_state_t ib_ac_state_t;
typedef struct ib_ac_context_t ib_ac_context_t;
typedef struct ib_ac_match_t ib_ac_match_t;

typedef char ib_ac_char_t;

/**
 * Aho Corasick tree. Used to parse and store the states and transitions
 */
struct ib_ac_t {
    uint8_t flags;          /**< flags of the matcher and parser */
    ib_mpool_t *mp;         /**< mem pool */

    ib_ac_state_t *root;     /**< root of the direct tree */

    uint32_t pattern_cnt;   /**< number of patterns */
};

/**
 * Aho Corasick matching context. Used to consume a buffer in chunks
 * It also store a list of matching states
 */
struct ib_ac_context_t {
    ib_ac_t *ac_tree;           /**< Aho Corasick automata */
    ib_ac_state_t *current;     /**< Current state of match */

    size_t processed;           /**> number of bytes processed over
                                     multiple consume calls */
    size_t current_offset;      /**> number of bytes processed in
                                     the last (or current) call */

    ib_list_t *match_list;      /**< result list of matches */
    size_t match_cnt;           /**< number of matches */
}; 

/**
 * Aho Corasick match result. Holds the pattern, pattern length
 * offset from the beggining of the match, and relative offset.
 * Relative offset is the offset from the end of the given chunk buffer
 * Keep in mind that the start of the match can be at a previous
 * processed chunk
 */
struct ib_ac_match_t {
    const ib_ac_char_t *pattern;     /**< pointer to the original pattern */
    const void *data;                /**< pointer to the data associated */
    size_t pattern_len;              /**< pattern length */

    size_t offset;                   /**< offset over all the segments processed
                                          by this context to the start of
                                          the match */

    size_t relative_offset;          /**< offset of the match from the last
                                          processed buffer within the current
                                          context. Keep in mind that this value
                                          can be negative if a match started
                                          from a previous buffer! */
};

/* Callback definition for functions processing matches */
typedef void (*ib_ac_callback_t)(ib_ac_t *orig,
                                 ib_ac_char_t *pattern,
                                 size_t pattern_len,
                                 void *userdata,
                                 size_t offset,
                                 size_t relative_offset);



/**
 * Init macro for a matching context (needed by ib_ac_consume())
 * @param ac_ctx the ac matching context
 * @param ac_tree the ac tree
 */
#define ib_ac_init_ctx(ac_ctx,ac_t) \
        do { \
            (ac_ctx)->ac_tree = (ac_t); \
            if ((ac_t) != NULL) {\
            (ac_ctx)->current = (ac_t)->root; }\
            (ac_ctx)->processed = 0; \
            (ac_ctx)->current_offset = 0; \
            (ac_ctx)->match_cnt = 0; \
            (ac_ctx)->match_list = NULL; \
        } while(0)

/**
 * Reset macro for a matching context
 * @param ac_ctx the ac matching context
 * @param ac_tree the ac tree
 */
#define ib_ac_reset_ctx(ac_ctx,ac_t) \
        do { \
            (ac_ctx)->ac_tree = (ac_t); \
            if ((ac_t) != NULL) {\
            (ac_ctx)->current = (ac_t)->root; }\
            (ac_ctx)->processed = 0; \
            (ac_ctx)->match_cnt = 0; \
            (ac_ctx)->current_offset = 0; \
            if ((ac_ctx)->match_list != NULL) \
                ib_list_clear((ac_ctx)->match_list); \
        } while(0)



/**
 * creates an aho corasick automata with states in trie form
 *
 * @param ac_tree pointer to store the matcher
 * @param flags options for the matcher
 * @param pool memory pool to use
 *
 * @returns Status code
 */
ib_status_t ib_ac_create(ib_ac_t **ac_tree,
                         uint8_t flags,
                         ib_mpool_t *pool);

/**
 * builds links between states (the AC failure function)
 *
 * @param ac_tree pointer to store the matcher
 *
 * @returns Status code
 */
ib_status_t ib_ac_build_links(ib_ac_t *ac_tree);

/**
 * adds a pattern into the trie
 *
 * @param ac_tree pointer to the matcher
 * @param pattern to add
 * @param callback function pointer to call if pattern is found
 * @param data pointer to pass to the callback if pattern is found
 * @param len the length of the pattern
 *
 * @returns Status code
 */
ib_status_t ib_ac_add_pattern(ib_ac_t *ac_tree,
                              const char *pattern,
                              ib_ac_callback_t callback,
                              void *data,
                              size_t len);

/**
 * Search patterns of the ac_tree matcher in the given buffer using a 
 * matching context. The matching context stores offsets used to process
 * a search over multiple data segments. The function has option flags to
 * specify to return where the first pattern is found, or after all the
 * data is consumed, using a user specified callback and/or building a
 * list of patterns matched
 *
 * @param ac_ctx pointer to the matching context
 * @param data pointer to the buffer to search in
 * @param len the length of the data
 * @param flags options to use while matching
 * @param mp memory pool to use
 *
 * @returns Status code */

ib_status_t ib_ac_consume(ib_ac_context_t *ac_ctx,
                          const char *data,
                          size_t len,
                          uint8_t flags,
                          ib_mpool_t *mp);


/** @} IronBeeUtilAhoCorasick */

#ifdef __cplusplus
}
#endif

#endif /* _IB_AHOCORASICK_H_ */
