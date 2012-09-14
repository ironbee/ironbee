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

/* Derived in part from Apache HTTPD's regexp utility. */

/**
 * @file regex.h
 * @brief Ironbee Perl-like Regexp utilities
 *
 *
 * Implements Perl-style expressions.
 * So for a substitution:
 * 
 *   $var =~ s/expression(with)\s+(match(es)?)/exprwith\1backref/gs;
 * 
 * becomes
 * 
 *   ib_rx_t *rx = ib_rx_compile(pool,
 *       "s/expression(with)\s+(match(es)?)/exprwith\1backref/gs");
 *   nsubs = ib_rx_exec(pool, rx, var &newpattern, NULL);
 * 
 * Similarly for a regexp match,
 * 
 *   $var =~ /this(and(.*)that)?/i;
 *   $foo = $1;
 * 
 * becomes
 * 
 *   ib_rx_t *rx = ib_rx_compile(pool, "/this(and(.*)that)?/i");
 *   matched = ib_rx_exec(pool, rx, var, NULL, &backrefs);
 *   ib_rx_match(&backrefs, 1, &foo_len, &foo);
 */

#ifndef IB_REGEX_H
#define IB_REGEX_H

#ifdef HAVE_PCRE
#include <pcreposix.h>
#else
#include <regex.h>
#endif
#include "ironbee/mpool.h"

/* Allow for C++ users */

#ifdef __cplusplus
extern "C" {
#endif

/** ib_rx_t: higher-level regexps, parsed and executed from Perl-like strings */

typedef struct {
    regex_t rx;
    unsigned int flags;
    const char *subs;
    size_t nmatch;
} ib_rx_t;

/* ib_rxmatch_t: memory/backreferences from an ib_rx match */
typedef struct {
    const char *match;
    size_t nmatch;
    regmatch_t *pmatch;
} ib_rxmatch_t;

/**
 * Compile a pattern into a regexp.
 * supports perl-like formats
 *    match-string
 *    /match-string/flags
 *    s/match-string/replacement-string/flags
 *    Intended to support more perl-like stuff as and when round tuits happen
 * match-string is anything supported by ib_regcomp
 * replacement-string is a substitution string and may contain backreferences
 * flags should correspond with perl syntax: treat failure to do so as a bug
 *  
 * @param pool Pool to allocate from
 * @param pattern Pattern to compile
 * @return Compiled regexp, or NULL in case of compile/syntax error
 */
ib_rx_t *ib_rx_compile(ib_mpool_t *pool, const char *pattern);

/**
 * Apply a regexp operation to a string.
 * @param pool Pool to allocate from
 * @param rx The regex match to apply
 * @param pattern The string to apply it to
 * @param newpattern The modified string (ignored if the operation doesn't
 *                                        modify the string)
 * @param match If non-null, will contain regexp memory/backreferences
 *              for the match on return.  Not meaningful for a substitution.
 * @return Number of times a match happens.  Normally 0 (no match) or 1
 *         (match found), but may be greater if a transforming pattern
 *         is applied with the 'g' flag.
 */
int ib_rx_exec(ib_mpool_t *pool, ib_rx_t *rx, const char *pattern,
               char **newpattern, ib_rxmatch_t *match);
#ifdef DOXYGEN
/**
 * Number of matches in the regexp operation's memory
 * This may be 0 if no match is in memory, or up to nmatch from compilation
 * @param match The regexp match
 * @return Number of matches in memory
 */
int ib_rx_nmatch(ib_rxmatch_t *match);
#else
#define ib_rx_nmatch(match) (((match)->match != NULL) ? (match)->nmatch : 0)
#endif
/**
 * Get a pointer to a match from regex memory
 * @param match The regexp match
 * @param n The match number to retrieve (must be between 0 and nmatch)
 * @param len Returns the length of the match pattern.
 * @param pattern Returns the match pattern
 */
void ib_rx_match(ib_rxmatch_t *match, int n, int *len, const char **pattern);


#ifdef __cplusplus
}   /* extern "C" */
#endif

#endif /* IB_REGEX_T */

