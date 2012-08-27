/* Derived from Apache HTTPD's regexp utility. */

/* Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
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
 */

/* Part derived in turn from PCRE's pcreposix.h. */

/*           Copyright (c) 1997-2004 University of Cambridge

-----------------------------------------------------------------------------
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice,
      this list of conditions and the following disclaimer.

    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.

    * Neither the name of the University of Cambridge nor the names of its
      contributors may be used to endorse or promote products derived from
      this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.
-----------------------------------------------------------------------------
*/

/**
 * @file regex.h
 * @brief Ironbee Regexp utilities
 */

#ifndef IB_REGEX_H
#define IB_REGEX_H

//#include <regex.h>
#include "ironbee/mpool.h"

/* Allow for C++ users */

#ifdef __cplusplus
extern "C" {
#endif

/* Options for ib_regcomp, ib_regexec, and ib_rx versions: */

#define IB_REG_ICASE    0x01 /** use a case-insensitive match */
#define IB_REG_NEWLINE  0x02 /** don't match newlines against '.' etc */
#define IB_REG_NOTBOL   0x04 /** ^ will not match against start-of-string */
#define IB_REG_NOTEOL   0x08 /** $ will not match against end-of-string */

#define IB_REG_EXTENDED (0)  /** unused */
#define IB_REG_NOSUB    (0)  /** unused */

#define IB_REG_MULTI 0x10    /* perl's /g (needs fixing) */
#define IB_REG_NOMEM 0x20    /* nomem in our code */
#define IB_REG_DOTALL 0x40   /* perl's /s flag */

/* Error values: */
enum {
  IB_REG_ASSERT = 1,  /** internal error ? */
  IB_REG_ESPACE,      /** failed to get memory */
  IB_REG_INVARG,      /** invalid argument */
  IB_REG_NOMATCH      /** match failed */
};

/* The structure representing a compiled regular expression. */
typedef struct {
    void *re_pcre;
    int re_nsub;
    size_t re_erroffset;
} ib_regex_t;

/* The structure in which a captured offset is returned. */
typedef struct {
    int rm_so;
    int rm_eo;
} ib_regmatch_t;

/* The functions */

/**
 * Compile a regular expression.
 * @param preg Returned compiled regex
 * @param regex The regular expression string
 * @param cflags Bitwise OR of IB_REG_* flags (ICASE and NEWLINE supported,
 *                                             other flags are ignored)
 * @return Zero on success or non-zero on error
 */
int ib_regcomp(ib_regex_t *preg, const char *regex, int cflags);

/**
 * Match a NUL-terminated string against a pre-compiled regex.
 * @param preg The pre-compiled regex
 * @param string The string to match
 * @param nmatch Provide information regarding the location of any matches
 * @param pmatch Provide information regarding the location of any matches
 * @param eflags Bitwise OR of IB_REG_* flags (NOTBOL and NOTEOL supported,
 *                                             other flags are ignored)
 * @return 0 for successful match, \p IB_REG_NOMATCH otherwise
 */
int ib_regexec(const ib_regex_t *preg, const char *string,
                           size_t nmatch, ib_regmatch_t *pmatch, int eflags);

/**
 * Match a string with given length against a pre-compiled regex. The string
 * does not need to be NUL-terminated.
 * @param preg The pre-compiled regex
 * @param buff The string to match
 * @param len Length of the string to match
 * @param nmatch Provide information regarding the location of any matches
 * @param pmatch Provide information regarding the location of any matches
 * @param eflags Bitwise OR of IB_REG_* flags (NOTBOL and NOTEOL supported,
 *                                             other flags are ignored)
 * @return 0 for successful match, IB_REG_NOMATCH otherwise
 */
int ib_regexec_len(const ib_regex_t *preg, const char *buff, size_t len,
                   size_t nmatch, ib_regmatch_t *pmatch, int eflags);

/**
 * Return the error code returned by regcomp or regexec into error messages
 * @param errcode the error code returned by regexec or regcomp
 * @param preg The precompiled regex
 * @param errbuf A buffer to store the error in
 * @param errbuf_size The size of the buffer
 */
size_t ib_regerror(int errcode, const ib_regex_t *preg,
                   char *errbuf, size_t errbuf_size);

/** Destroy a pre-compiled regex.
 * @param preg The pre-compiled regex to free.
 */
void ib_regfree(ib_regex_t *preg);

#if 0
/**
 * Compile a regular expression to be used later. The regex is freed when
 * the pool is destroyed.
 * @param p The pool to allocate from
 * @param pattern the regular expression to compile
 * @param cflags The bitwise or of one or more of the following:
 *   @li REG_EXTENDED - Use POSIX extended Regular Expressions
 *   @li REG_ICASE    - Ignore case
 *   @li REG_NOSUB    - Support for substring addressing of matches
 *       not required
 *   @li REG_NEWLINE  - Match-any-character operators don't match new-line
 * @return The compiled regular expression
 */
AP_DECLARE(ap_regex_t *) ap_pregcomp(apr_pool_t *p, const char *pattern,
                                     int cflags);
/**
 * Free the memory associated with a compiled regular expression
 * @param p The pool the regex was allocated from
 * @param reg The regular expression to free
 * @note This function is only necessary if the regex should be cleaned
 * up before the pool
 */
AP_DECLARE(void) ap_pregfree(apr_pool_t *p, ap_regex_t *reg);
#endif
#if 0
/**
 * After performing a successful regex match, you may use this function to
 * perform a series of string substitutions based on subexpressions that were
 * matched during the call to ap_regexec. This function is limited to
 * result strings of 64K. Consider using ap_pregsub_ex() instead.
 * @param p The pool to allocate from
 * @param input An arbitrary string containing $1 through $9.  These are
 *              replaced with the corresponding matched sub-expressions
 * @param source The string that was originally matched to the regex
 * @param nmatch the nmatch returned from ap_pregex
 * @param pmatch the pmatch array returned from ap_pregex
 * @return The substituted string, or NULL on error
 */
char *ib_pregsub(ib_mpool_t *p, const char *input, const char *source,
                 size_t nmatch, ib_regmatch_t pmatch[]);
#endif
#if 0
/**
 * After performing a successful regex match, you may use this function to
 * perform a series of string substitutions based on subexpressions that were
 * matched during the call to ap_regexec
 * @param p The pool to allocate from
 * @param result where to store the result, will be set to NULL on error
 * @param input An arbitrary string containing $1 through $9.  These are
 *              replaced with the corresponding matched sub-expressions
 * @param source The string that was originally matched to the regex
 * @param nmatch the nmatch returned from ap_pregex
 * @param pmatch the pmatch array returned from ap_pregex
 * @param maxlen the maximum string length to return, 0 for unlimited
 * @return The substituted string, or NULL on error
 */
AP_DECLARE(apr_status_t) ap_pregsub_ex(apr_pool_t *p, char **result,
                                       const char *input, const char *source,
                                       apr_size_t nmatch,
                                       ap_regmatch_t pmatch[],
                                       apr_size_t maxlen);
#endif


/* ib_rx_t: higher-level regexps, parsed and executed from Perl-like strings */

typedef struct {
    ib_regex_t rx;
    unsigned int flags;
    const char *subs;
    size_t nmatch;
} ib_rx_t;

/* ib_rxmatch_t: memory/backreferences from an ib_rx match */
typedef struct {
    const char *match;
    size_t nmatch;
    ib_regmatch_t *pmatch;
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

