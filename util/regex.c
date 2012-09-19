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

/* Derived from regexp utilities in Apache HTTPD */

/**
 * @file
 * @brief IronBee --- Perl-like Regexp utilities Implementation.
 */

#include "ironbee_config_auto.h"

#include <ironbee/regex.h>

#include <ironbee/mpool.h>
#include <ironbee/types.h>

#include <ctype.h>

/* Compiling with DOXYGEN will leave ib_rx_nmatch unimplemented */
#ifdef DOXYGEN
#error "Don`t compile with DOXYGEN!"
#endif

/* If we're using regex.h instead of pcreposix, we need dummy defs
 * for unsupported PCRE extensions.
 */
#ifndef HAVE_PCRE
#define REG_UTF8 0
#define REG_DOTALL 0
#endif

#define HUGE_STRING_LEN 8192
#define IB_MAX_REG_MATCH 10
#define IB_PREGSUB_MAXLEN (HUGE_STRING_LEN * 8)
#define IB_SIZE_MAX (~((size_t)0))

/* Flags are a bitfield combining the regex library with our own bits.
 * To avoid any risk of a future change causing conflict, declare all
 * the bits here, with conversion macro
 */

#define IB_REG_ICASE    0x01 /**< use a case-insensitive match */
#define IB_REG_NEWLINE  0x02 /**< don't match newlines against '.' etc */
#define IB_REG_NOTBOL   0x04 /**< ^ will not match against start-of-string */
#define IB_REG_NOTEOL   0x08 /**< $ will not match against end-of-string */

#define IB_REG_EXTENDED (0)  /**< unused */
#define IB_REG_NOSUB    (0)  /**< unused */

#define IB_REG_MULTI 0x10    /* perl's /g (needs fixing) */
#define IB_REG_NOMEM 0x20    /* nomem in our code */
#define IB_REG_DOTALL 0x40   /* perl's /s flag */
#define IB_REG_UTF8 0x80     /* match utf-8 */

/* Fix on posix extended standard + very-low-hanging PCRE for now */
#define REGCOMP_FLAGS(flags)                           \
    REG_EXTENDED                                     | \
    (((flags)&IB_REG_ICASE) ? REG_ICASE : 0)         | \
    (((flags)&IB_REG_NEWLINE) ? REG_NEWLINE : 0)     | \
    (((flags)&IB_REG_UTF8) ? REG_UTF8 : 0)           | \
    (((flags)&IB_REG_DOTALL) ? REG_DOTALL : 0)

#define REGEXEC_FLAGS(flags)                           \
    (((flags)&IB_REG_NOTBOL) ? REG_NOTBOL : 0)       | \
    (((flags)&IB_REG_NOTEOL) ? REG_NOTEOL : 0)


ib_rx_t *ib_rx_compile(ib_mpool_t *pool, const char *pattern)
{
    /* perl style patterns
     * add support for more as and when wanted
     * substitute: s/rx/subs/
     * match: m/rx/ or just /rx/
     * Flags follow the final delimiter as in Perl
     */

    /* allow any nonalnum delimiter as first or second char.
     * If we ever use this with non-string pattern we'll need an extra check
     */
    const char *endp = 0;
    const char *str = pattern;
    const char *rxstr;
    ib_rx_t *ret = ib_mpool_calloc(pool, 1, sizeof(ib_rx_t));
    char delim = 0;
    enum { SUBSTITUTE = 's', MATCH = 'm'} action = MATCH;
    if (!isalnum(pattern[0])) {
        delim = *str++;
    }
    else if (pattern[0] == 's' && !isalnum(pattern[1])) {
        action = SUBSTITUTE;
        delim = pattern[1];
        str += 2;
    }
    else if (pattern[0] == 'm' && !isalnum(pattern[1])) {
        delim = pattern[1];
        str += 2;
    }
    /* TODO: support perl's after/before */
    /* FIXME: fix these simpleminded delims */

    /* we think there's a delimiter.  Allow for it not to be if unmatched */
    if (delim) {
        endp = strchr(str, delim);
    }
    if (!endp) { /* there's no delim  or flags */
        if (regcomp(&ret->rx, pattern, 0) == 0) {
            ib_mpool_cleanup_register(pool, (ib_mpool_cleanup_fn_t)regfree,
                                      &ret->rx);
            return ret;
        }
        else {
            return NULL;
        }
    }

    /* We have a delimiter.  Use it to extract the regexp */
    rxstr = ib_mpool_memdup_to_str(pool, str, endp-str);

    /* If it's a substitution, we need the replacement string
     * TODO: possible future enhancement - support other parsing
     * in the replacement string.
     */
    if (action == SUBSTITUTE) {
        str = endp+1;
        if (!*str || (endp = strchr(str, delim), !endp)) {
            /* missing replacement string is an error */
            return NULL;
        }
        ret->subs = ib_mpool_memdup_to_str(pool, str, (endp-str));
    }

    /* anything after the current delimiter is flags */
    while (*++endp) {
      switch (*endp) {
        case 'i': ret->flags |= IB_REG_ICASE; break;
        case 'm': ret->flags |= IB_REG_NEWLINE; break;
        case 'n': ret->flags |= IB_REG_NOMEM; break;
        case 'g': ret->flags |= IB_REG_MULTI; break;
        case 's': ret->flags |= IB_REG_DOTALL; break;
        case '^': ret->flags |= IB_REG_NOTBOL; break;
        case '$': ret->flags |= IB_REG_NOTEOL; break;
        case '8': ret->flags |= IB_REG_UTF8; break;
        default: break; /* we should probably be stricter here */
      }
    }
    if (regcomp(&ret->rx, rxstr, REGCOMP_FLAGS(ret->flags)) == 0) {
        ib_mpool_cleanup_register(pool, (ib_mpool_cleanup_fn_t)regfree,
                                  &ret->rx);
    }
    else {
        return NULL;
    }
    if (!(ret->flags & IB_REG_NOMEM)) {
        /* count size of memory required, starting at 1 for the whole-match
         * Simpleminded should be fine 'cos regcomp already checked syntax
         */
        ret->nmatch = 1;
        while (*rxstr) {
            switch (*rxstr++) {
            case '\\':  /* next char is escaped - skip it */
                if (*rxstr != 0) {
                    ++rxstr;
                }
                break;
            case '(':   /* unescaped bracket implies memory */
                ++ret->nmatch;
                break;
            default:
                break;
            }
        }
    }
    return ret;
}

/* This function substitutes for $0-$9, filling in regular expression
 * submatches. Pass it the same nmatch and pmatch arguments that you
 * passed ap_regexec(). pmatch should not be greater than the maximum number
 * of subexpressions - i.e. one more than the re_nsub member of ap_regex_t.
 *
 * nmatch must be <=IB_MAX_REG_MATCH (10).
 *
 * input should be the string with the $-expressions, source should be the
 * string that was matched against.
 *
 * It returns the substituted string, or NULL if a vbuf is used.
 * On errors, returns the orig string.
 *
 * Parts of this code are based on Henry Spencer's regsub(), from his
 * AT&T V8 regexp package.
 */
static ib_status_t regsub_core(ib_mpool_t *p, char **result, const char *input,
                               const char *source, size_t nmatch,
                               regmatch_t pmatch[])
{
    const char *src = input;
    char *dst;
    char c;
    size_t no;
    size_t len = 0;

    if (!source || nmatch>IB_MAX_REG_MATCH)
        return IB_EINVAL;
    if (!nmatch) {
        len = strlen(src);
        if (IB_PREGSUB_MAXLEN > 0 && len >= IB_PREGSUB_MAXLEN)
            return IB_EALLOC;

        *result = ib_mpool_memdup_to_str(p, src, len);
        return IB_OK;
    }

    /* First pass, find the size */
    while ((c = *src++) != '\0') {
        if (c == '$' && isdigit(*src))
            no = *src++ - '0';
        else
            no = IB_MAX_REG_MATCH;

        if (no >= IB_MAX_REG_MATCH) {  /* Ordinary character. */
            if (c == '\\' && *src)
                src++;
            len++;
        }
        else if (no < nmatch && pmatch[no].rm_so < pmatch[no].rm_eo) {
            if (IB_SIZE_MAX - len <=
                (size_t)(pmatch[no].rm_eo - pmatch[no].rm_so)
            ) {
                return IB_EALLOC;
            }
            len += pmatch[no].rm_eo - pmatch[no].rm_so;
        }

    }

    if (len >= IB_PREGSUB_MAXLEN && IB_PREGSUB_MAXLEN > 0)
        return IB_EALLOC;

    *result = dst = ib_mpool_alloc(p, len + 1);

    /* Now actually fill in the string */

    src = input;

    while ((c = *src++) != '\0') {
        if (c == '$' && isdigit(*src))
            no = *src++ - '0';
        else
            no = IB_MAX_REG_MATCH;

        if (no >= IB_MAX_REG_MATCH) {  /* Ordinary character. */
            if (c == '\\' && *src)
                c = *src++;
            *dst++ = c;
        }
        else if (no < nmatch && pmatch[no].rm_so < pmatch[no].rm_eo) {
            len = pmatch[no].rm_eo - pmatch[no].rm_so;
            memcpy(dst, source + pmatch[no].rm_so, len);
            dst += len;
        }

    }
    *dst = '\0';

    return IB_OK;
}

int ib_rx_exec(ib_mpool_t *pool, ib_rx_t *rx, const char *pattern,
               char **newpattern, ib_rxmatch_t *match)
{
    int ret = 1;
    int startl, oldl, newl, diffsz;
    const char *remainder;
    char *subs;
    ib_rxmatch_t tmp_match;
    regmatch_t tmp_pmatch[IB_MAX_REG_MATCH];

    /* If caller asks for a match, we need to give them valid backrefs */
    if (match) {
        match->match = ib_mpool_strdup(pool, pattern);
        match->nmatch = rx->nmatch;
        match->pmatch = ib_mpool_calloc(pool, match->nmatch, sizeof(regmatch_t));
    }
    else {
        /* we need the workspace even if caller doesn't, but we don't
         * need to allocate from pool
         */
        match = &tmp_match;
        match->match = pattern;
        match->nmatch = rx->nmatch;
        match->pmatch = tmp_pmatch;
    }

    if (regexec(&rx->rx, pattern, match->nmatch, match->pmatch,
                REGEXEC_FLAGS(rx->flags)) != 0) {
        match->match = NULL;
        return 0; /* no match, nothing to do */
    }
    if (rx->subs) {
        ib_status_t rc = regsub_core(pool, newpattern, rx->subs, pattern,
                                     match->nmatch, match->pmatch);
        if (rc != IB_OK) {
            return 0; /* FIXME - should we do more to handle error? */
        }
        startl = match->pmatch[0].rm_so;
        oldl = match->pmatch[0].rm_eo - startl;
        newl = strlen(*newpattern);
        diffsz = newl - oldl;
        remainder = pattern + startl + oldl;
        if (rx->flags & IB_REG_MULTI) {
            /* recurse to do any further matches */
            char *rsubs;
            ret += ib_rx_exec(pool, rx, remainder, &rsubs, NULL);
            if (ret > 1) {
                /* a further substitution happened */
                diffsz += strlen(rsubs) - strlen(remainder);
                remainder = rsubs;
            }
        }
        subs  = ib_mpool_alloc(pool, strlen(pattern) + 1 + diffsz);
        memcpy(subs, pattern, startl);
        memcpy(subs+startl, *newpattern, newl);
        strcpy(subs+startl+newl, remainder);
        *newpattern = subs;
    }
    return ret;
}

void ib_rx_match(ib_rxmatch_t *match, int n, int *len, const char **pattern)
{
    if (n >= 0 && (size_t)n < ib_rx_nmatch(match)) {
        *pattern = match->match + match->pmatch[n].rm_so;
        *len = match->pmatch[n].rm_eo - match->pmatch[n].rm_so;
    }
    else {
        *len = -1;
        *pattern = NULL;
    }
}
