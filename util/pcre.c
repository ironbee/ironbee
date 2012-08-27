/* Derived from pcreposix.c */

/*************************************************
 *      Perl-Compatible Regular Expressions      *
 *************************************************/

/*
This is a library of functions to support regular expressions whose syntax
and semantics are as close as possible to those of the Perl 5 language. See
the file Tech.Notes for some information on the internals.

This module is a wrapper that provides a POSIX API to the underlying PCRE
functions.

Written by: Philip Hazel <ph10@cam.ac.uk>

           Copyright (c) 1997-2004 University of Cambridge

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

#include <stdio.h>
#include "pcre.h"
#include "ironbee/regex.h"


#ifndef POSIX_MALLOC_THRESHOLD
#define POSIX_MALLOC_THRESHOLD (10)
#endif


size_t ib_regerror(int errcode, const ib_regex_t *preg,
                   char *errbuf, size_t errbuf_size)
{
    const char *message, *addmessage;
    size_t length, addlength;

    /* Table of error strings corresponding to POSIX error codes; must be
     * kept in synch with regex.h's IB_REG_E* definitions.
     */

    static const char *const pstring[] = {
        "",                         /* Dummy for value 0 */
        "internal error",           /* IB_REG_ASSERT */
        "failed to get memory",     /* IB_REG_ESPACE */
        "bad argument",             /* IB_REG_INVARG */
        "match failed"              /* IB_REG_NOMATCH */
    };

    message = (errcode >= (int)(sizeof(pstring) / sizeof(char *))) ?
              "unknown error code" : pstring[errcode];
    length = strlen(message) + 1;

    addmessage = " at offset ";
    addlength = (preg != NULL && (int)preg->re_erroffset != -1) ?
                strlen(addmessage) + 6 : 0;

    if (errbuf_size > 0) {
        if (addlength > 0 && errbuf_size >= length + addlength)
            snprintf(errbuf, errbuf_size, "%s%s%-6d", message, addmessage,
                         (int)preg->re_erroffset);
        else
            strncpy(errbuf, message, errbuf_size-1);
    }

    return length + addlength;
}




/*************************************************
 *           Free store held by a regex          *
 *************************************************/

void ib_regfree(ib_regex_t *preg)
{
    (pcre_free)(preg->re_pcre);
}



/*************************************************
 *            Compile a regular expression       *
 *************************************************/

/*
 * Arguments:
 *  preg        points to a structure for recording the compiled expression
 *  pattern     the pattern to compile
 *  cflags      compilation flags
 *
 * Returns:      0 on success
 *               various non-zero codes on failure
*/
int ib_regcomp(ib_regex_t * preg, const char *pattern, int cflags)
{
    const char *errorptr;
    int erroffset;
    int errcode = 0;
    int options = 0;

    if ((cflags & IB_REG_ICASE) != 0)
        options |= PCRE_CASELESS;
    if ((cflags & IB_REG_NEWLINE) != 0)
        options |= PCRE_MULTILINE;
    if ((cflags & IB_REG_DOTALL) != 0)
        options |= PCRE_DOTALL;

    preg->re_pcre =
        pcre_compile2(pattern, options, &errcode, &errorptr, &erroffset, NULL);
    preg->re_erroffset = erroffset;

    if (preg->re_pcre == NULL) {
        /*
         * There doesn't seem to be constants defined for compile time error
         * codes. 21 is "failed to get memory" according to pcreapi(3).
         */
        if (errcode == 21)
            return IB_REG_ESPACE;
        return IB_REG_INVARG;
    }

    pcre_fullinfo((const pcre *)preg->re_pcre, NULL,
                   PCRE_INFO_CAPTURECOUNT, &(preg->re_nsub));
    return 0;
}




/*************************************************
 *              Match a regular expression       *
 *************************************************/

/* Unfortunately, PCRE requires 3 ints of working space for each captured
 * substring, so we have to get and release working store instead of just using
 * the POSIX structures as was done in earlier releases when PCRE needed only 2
 * ints. However, if the number of possible capturing brackets is small, use a
 * block of store on the stack, to reduce the use of malloc/free. The threshold
 * is in a macro that can be changed at configure time.
 */
int ib_regexec(const ib_regex_t *preg, const char *string, size_t nmatch,
               ib_regmatch_t *pmatch, int eflags)
{
    return ib_regexec_len(preg, string, strlen(string), nmatch, pmatch, eflags);
}

int ib_regexec_len(const ib_regex_t *preg, const char *buff, size_t len,
                   size_t nmatch, ib_regmatch_t *pmatch, int eflags)
{
    int rc;
    int options = 0;
    int *ovector = NULL;
    int small_ovector[POSIX_MALLOC_THRESHOLD * 3];
    int allocated_ovector = 0;

    if ((eflags & IB_REG_NOTBOL) != 0)
        options |= PCRE_NOTBOL;
    if ((eflags & IB_REG_NOTEOL) != 0)
        options |= PCRE_NOTEOL;

    ((ib_regex_t *)preg)->re_erroffset = (size_t)(-1);    /* Only has meaning after compile */

    if (nmatch > 0) {
        if (nmatch <= POSIX_MALLOC_THRESHOLD) {
            ovector = &(small_ovector[0]);
        }
        else {
            ovector = (int *)malloc(sizeof(int) * nmatch * 3);
            if (ovector == NULL)
                return IB_REG_ESPACE;
            allocated_ovector = 1;
        }
    }

    rc = pcre_exec((const pcre *)preg->re_pcre, NULL, buff, (int)len,
                   0, options, ovector, nmatch * 3);

    if (rc == 0)
        rc = nmatch;            /* All captured slots were filled in */

    if (rc >= 0) {
        size_t i;
        for (i = 0; i < (size_t)rc; i++) {
            pmatch[i].rm_so = ovector[i * 2];
            pmatch[i].rm_eo = ovector[i * 2 + 1];
        }
        if (allocated_ovector)
            free(ovector);
        for (; i < nmatch; i++)
            pmatch[i].rm_so = pmatch[i].rm_eo = -1;
        return 0;
    }

    else {
        if (allocated_ovector)
            free(ovector);
        switch (rc) {
        case PCRE_ERROR_NOMATCH:
            return IB_REG_NOMATCH;
        case PCRE_ERROR_NULL:
            return IB_REG_INVARG;
        case PCRE_ERROR_BADOPTION:
            return IB_REG_INVARG;
        case PCRE_ERROR_BADMAGIC:
            return IB_REG_INVARG;
        case PCRE_ERROR_UNKNOWN_NODE:
            return IB_REG_ASSERT;
        case PCRE_ERROR_NOMEMORY:
            return IB_REG_ESPACE;
#ifdef PCRE_ERROR_MATCHLIMIT
        case PCRE_ERROR_MATCHLIMIT:
            return IB_REG_ESPACE;
#endif
#ifdef PCRE_ERROR_BADUTF8
        case PCRE_ERROR_BADUTF8:
            return IB_REG_INVARG;
#endif
#ifdef PCRE_ERROR_BADUTF8_OFFSET
        case PCRE_ERROR_BADUTF8_OFFSET:
            return IB_REG_INVARG;
#endif
        default:
            return IB_REG_ASSERT;
        }
    }
}

/* End of pcreposix.c */
