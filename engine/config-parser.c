
#line 1 "config-parser.rl"
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
 * @brief IronBee --- Configuration File Parser
 *
 * @author Brian Rectanus <brectanus@qualys.com>
 */

#include "ironbee_config_auto.h"

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <libgen.h>
#include <errno.h>

#include <ironbee/engine.h>
#include <ironbee/util.h>
#include <ironbee/config.h>
#include <ironbee/mpool.h>
#include <ironbee/path.h>

#include "config-parser.h"

/**
 * Finite state machine type.
 *
 * Contains state information for Ragel's parser.
 * Many of these values and names come from the Ragel documentation, section
 * 5.1 Variable Used by Ragel. p35 of The Ragel Guide 6.7 found at
 * http://www.complang.org/ragel/ragel-guide-6.7.pdf
 */
typedef struct {
    const char    *p;     /**< Pointer to the chunk being parsed. */
    const char    *pe;    /**< Pointer past the end of p (p+length(p)). */
    const char    *eof;   /**< eof==p==pe on last chunk. NULL otherwise. */
    const char    *ts;    /**< Pointer to character data for Ragel. */
    const char    *te;    /**< Pointer to character data for Ragel. */
    int      cs;          /**< Current state. */
    int      top;         /**< Top of the stack. */
    int      act;         /**< Used to track the last successful match. */
    int      stack[1024]; /**< Stack of states. */
} fsm_t;


/**
 * @brief Malloc and unescape into that buffer the marked string.
 * @param[in] cp The configuration parser
 * @param[in] fpc_mark The start of the string.
 * @param[in] fpc The current character from ragel.
 * @param[in,out] mp Temporary memory pool passed in by Ragel.
 * @return a buffer allocated from the tmpmp memory pool
 *         available in ib_cfgparser_ragel_parse_chunk. This buffer may be
 *         larger than the string stored in it if the length of the string is
 *         reduced by Javascript unescaping.
 */
static char* alloc_cpy_marked_string(ib_cfgparser_t *cp,
                                     const char *fpc_mark,
                                     const char *fpc,
                                     ib_mpool_t* mp)
{
    const char *afpc = fpc;
    size_t pvallen;
    char* pval;
    /* Adjust for quoted value. */
    if ((*fpc_mark == '"') && (*(afpc-1) == '"') && (fpc_mark+1 < afpc)) {
        fpc_mark++;
        afpc--;
    }
    pvallen = (size_t)(afpc - fpc_mark);
    pval = (char *)ib_mpool_memdup(mp, fpc_mark, (pvallen + 1) * sizeof(*pval));

    pval[pvallen] = '\0';

    /* At this point the buffer i pvallen+1 in size, but we cannot shrink it. */
    /* This is not considered a problem for configuration parsing and it is
       deallocated after parsing and configuration is complete. */
    return pval;
}

static ib_status_t include_config_fn(ib_cfgparser_t *cp,
                                     ib_mpool_t* mp,
                                     const char *mark,
                                     const char *fpc,
                                     const char *file,
                                     unsigned int lineno)
{
    struct stat statbuf;
    ib_status_t rc;
    int statval;
    char *incfile;
    char *pval;
    char *real;
    char *lookup;
    void *freeme = NULL;

    pval = alloc_cpy_marked_string(cp, mark, fpc, mp);
    incfile = ib_util_relative_file(mp, file, pval);
    if (incfile == NULL) {
        ib_cfg_log_error(cp, "Failed to resolve included file \"%s\": %s",
                         file, strerror(errno));
        return IB_ENOENT;
    }

    real = realpath(incfile, NULL);
    if (real == NULL) {
        ib_cfg_log_error(cp,
                         "Failed to find real path of included file "
                         "(using original \"%s\"): %s",
                         incfile, strerror(errno));
        real = incfile;
    }
    else {
        if (strcmp(real, incfile) != 0) {
            ib_cfg_log_notice(cp,
                              "Real path of included file \"%s\" is \"%s\"",
                              incfile, real);
        }
        freeme = real;
    }

    /* Look up the real file path in the hash */
    rc = ib_hash_get(cp->includes, &lookup, real);
    if (rc == IB_OK) {
        ib_cfg_log_warning(cp,
                           "Included file \"%s\" already included: skipping",
                           real);
        return IB_OK;
    }
    else if (rc != IB_ENOENT) {
        ib_cfg_log_error(cp, "Error looking up include file \"%s\": %s",
                         real, strerror(errno));
    }

    /* Put the real name in the hash */
    lookup = ib_mpool_strdup(mp, real);
    if (freeme != NULL) {
        free(freeme);
        freeme = NULL;
    }
    if (lookup != NULL) {
        rc = ib_hash_set(cp->includes, lookup, lookup);
        if (rc != IB_OK) {
            ib_cfg_log_error(cp,
                             "Error adding include file to hash \"%s\": %s",
                             lookup, strerror(errno));
        }
    }

    if (access(incfile, R_OK) != 0) {
        ib_cfg_log_error(cp, "Cannot access included file \"%s\": %s",
                         incfile, strerror(errno));
        return IB_ENOENT;
    }

    statval = stat(incfile, &statbuf);
    if (statval != 0) {
        ib_cfg_log_error(cp,
                         "Failed to stat include file \"%s\": %s",
                         incfile, strerror(errno));
        return IB_ENOENT;
    }

    if (S_ISREG(statbuf.st_mode) == 0) {
        ib_cfg_log_error(cp,
	                 "Included file \"%s\" isn't a file", incfile);
        return IB_ENOENT;
    }

    ib_cfg_log_debug(cp, "Including '%s'", incfile);
    rc = ib_cfgparser_parse(cp, incfile);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp, "Error parsing included file \"%s\": %s",
	                 incfile, ib_status_to_string(rc));
        return rc;
    }

    ib_cfg_log_debug(cp, "Done processing include file \"%s\"", incfile);
    return IB_OK;
}


#line 350 "config-parser.rl"



#line 211 "config-parser.c"
static const char _ironbee_config_actions[] = {
	0, 1, 0, 1, 1, 1, 4, 1,
	8, 1, 10, 1, 11, 1, 13, 1,
	22, 1, 24, 1, 28, 1, 32, 1,
	37, 1, 38, 1, 39, 1, 42, 1,
	43, 1, 44, 1, 45, 2, 1, 26,
	2, 1, 29, 2, 1, 31, 2, 2,
	5, 2, 2, 17, 2, 3, 21, 2,
	4, 38, 2, 4, 41, 2, 4, 45,
	2, 5, 16, 2, 6, 24, 2, 6,
	25, 2, 7, 20, 2, 8, 30, 2,
	9, 0, 2, 10, 40, 2, 11, 12,
	2, 14, 15, 2, 14, 19, 2, 14,
	23, 2, 14, 27, 2, 14, 33, 2,
	14, 34, 2, 14, 35, 2, 14, 36,
	3, 2, 5, 16, 3, 2, 5, 17,
	3, 2, 5, 18, 3, 9, 0, 32,
	3, 10, 4, 40, 3, 14, 0, 15,
	3, 14, 0, 19, 3, 14, 0, 23,
	3, 14, 0, 27, 3, 14, 0, 35,
	3, 14, 10, 35
};

static const short _ironbee_config_key_offsets[] = {
	0, 0, 0, 4, 8, 9, 9, 18,
	20, 20, 20, 28, 30, 30, 30, 39,
	39, 48, 52, 53, 57, 61, 62, 66,
	71, 76, 78, 82, 84, 95, 102, 113,
	120, 121, 130, 139, 148, 157, 166, 175,
	183, 183, 190, 197, 198, 207, 214, 221,
	221, 229, 236, 236, 245, 252, 259, 268,
	275, 275, 282
};

static const char _ironbee_config_trans_keys[] = {
	9, 10, 13, 32, 9, 10, 13, 32,
	10, 9, 10, 13, 32, 34, 35, 60,
	62, 92, 34, 92, 9, 10, 32, 34,
	35, 60, 62, 92, 34, 92, 9, 10,
	13, 32, 60, 62, 92, 34, 35, 9,
	10, 13, 32, 60, 62, 92, 34, 35,
	9, 10, 13, 32, 10, 9, 10, 13,
	32, 9, 10, 13, 32, 10, 9, 10,
	32, 34, 9, 10, 13, 32, 34, 9,
	10, 13, 32, 34, 10, 34, 9, 10,
	13, 32, 10, 34, 9, 10, 13, 32,
	34, 35, 60, 62, 73, 92, 105, 32,
	34, 60, 62, 92, 9, 10, 9, 10,
	13, 32, 34, 35, 60, 62, 73, 92,
	105, 9, 10, 32, 34, 60, 62, 92,
	10, 32, 34, 60, 62, 78, 92, 110,
	9, 10, 32, 34, 60, 62, 67, 92,
	99, 9, 10, 32, 34, 60, 62, 76,
	92, 108, 9, 10, 32, 34, 60, 62,
	85, 92, 117, 9, 10, 32, 34, 60,
	62, 68, 92, 100, 9, 10, 32, 34,
	60, 62, 69, 92, 101, 9, 10, 9,
	10, 13, 32, 34, 60, 62, 92, 9,
	10, 32, 34, 60, 62, 92, 32, 34,
	60, 62, 92, 9, 10, 47, 9, 10,
	13, 32, 34, 35, 60, 62, 92, 32,
	34, 60, 62, 92, 9, 10, 9, 10,
	32, 34, 60, 62, 92, 9, 10, 32,
	34, 35, 60, 62, 92, 32, 34, 60,
	62, 92, 9, 10, 9, 10, 13, 32,
	60, 62, 92, 34, 35, 32, 34, 60,
	62, 92, 9, 10, 9, 10, 32, 34,
	60, 62, 92, 9, 10, 13, 32, 60,
	62, 92, 34, 35, 32, 34, 60, 62,
	92, 9, 10, 9, 10, 32, 34, 60,
	62, 92, 9, 10, 32, 34, 0
};

static const char _ironbee_config_single_lengths[] = {
	0, 0, 4, 4, 1, 0, 9, 2,
	0, 0, 8, 2, 0, 0, 7, 0,
	7, 4, 1, 4, 4, 1, 4, 5,
	5, 2, 4, 2, 11, 5, 11, 7,
	1, 7, 7, 7, 7, 7, 7, 8,
	0, 7, 5, 1, 9, 5, 7, 0,
	8, 5, 0, 7, 5, 7, 7, 5,
	0, 7, 4
};

static const char _ironbee_config_range_lengths[] = {
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 1, 0,
	1, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 1, 0, 0,
	0, 1, 1, 1, 1, 1, 1, 0,
	0, 0, 1, 0, 0, 1, 0, 0,
	0, 1, 0, 1, 1, 0, 1, 1,
	0, 0, 0
};

static const short _ironbee_config_index_offsets[] = {
	0, 0, 1, 6, 11, 13, 14, 24,
	27, 28, 29, 38, 41, 42, 43, 52,
	53, 62, 67, 69, 74, 79, 81, 86,
	92, 98, 101, 106, 109, 121, 128, 140,
	148, 150, 159, 168, 177, 186, 195, 204,
	213, 214, 222, 229, 231, 241, 248, 256,
	257, 266, 273, 274, 283, 290, 298, 307,
	314, 315, 323
};

static const char _ironbee_config_indicies[] = {
	1, 2, 3, 4, 2, 1, 6, 7,
	8, 6, 5, 7, 5, 10, 12, 13,
	14, 12, 15, 16, 16, 16, 17, 11,
	19, 20, 18, 18, 22, 24, 16, 24,
	25, 16, 16, 26, 27, 23, 29, 30,
	28, 28, 32, 34, 35, 36, 34, 37,
	37, 38, 37, 33, 40, 42, 43, 44,
	42, 37, 45, 46, 37, 41, 45, 47,
	48, 45, 37, 47, 37, 50, 51, 52,
	50, 49, 53, 54, 55, 53, 37, 54,
	37, 57, 37, 57, 58, 56, 60, 51,
	61, 60, 49, 59, 63, 54, 64, 63,
	65, 62, 37, 65, 62, 50, 51, 66,
	50, 37, 54, 65, 62, 68, 69, 70,
	68, 16, 71, 72, 16, 73, 74, 73,
	67, 75, 75, 75, 75, 76, 75, 1,
	68, 69, 70, 68, 77, 71, 77, 77,
	73, 74, 73, 67, 78, 79, 78, 78,
	78, 78, 76, 1, 80, 71, 78, 78,
	78, 78, 81, 76, 81, 78, 1, 78,
	78, 78, 78, 82, 76, 82, 78, 1,
	78, 78, 78, 78, 83, 76, 83, 78,
	1, 78, 78, 78, 78, 84, 76, 84,
	78, 1, 78, 78, 78, 78, 85, 76,
	85, 78, 1, 78, 78, 78, 78, 86,
	76, 86, 78, 1, 87, 88, 4, 87,
	78, 78, 78, 76, 1, 89, 78, 88,
	78, 78, 78, 78, 76, 1, 90, 90,
	90, 90, 92, 90, 91, 94, 93, 12,
	13, 14, 12, 15, 16, 16, 16, 17,
	11, 96, 96, 96, 96, 97, 96, 10,
	96, 98, 96, 96, 96, 96, 97, 10,
	96, 24, 16, 24, 25, 16, 16, 26,
	27, 23, 99, 99, 99, 99, 100, 99,
	22, 99, 34, 35, 36, 34, 37, 37,
	38, 37, 33, 101, 101, 101, 101, 102,
	101, 32, 101, 103, 101, 101, 101, 101,
	102, 32, 42, 43, 44, 42, 37, 45,
	46, 37, 41, 104, 104, 104, 104, 105,
	104, 40, 106, 104, 107, 104, 104, 104,
	104, 105, 40, 57, 37, 57, 58, 56,
	0
};

static const char _ironbee_config_trans_targs[] = {
	28, 29, 39, 42, 41, 28, 3, 40,
	4, 44, 45, 45, 6, 44, 46, 7,
	0, 5, 7, 47, 8, 48, 49, 49,
	10, 11, 48, 9, 11, 50, 12, 51,
	52, 52, 14, 51, 53, 0, 13, 54,
	55, 55, 16, 56, 57, 17, 15, 54,
	18, 19, 20, 58, 19, 20, 58, 21,
	19, 22, 23, 23, 24, 23, 25, 24,
	27, 26, 21, 29, 30, 28, 31, 32,
	43, 33, 2, 28, 1, 28, 28, 28,
	28, 34, 35, 36, 37, 38, 29, 3,
	40, 28, 28, 29, 1, 28, 28, 44,
	44, 5, 44, 48, 9, 51, 13, 51,
	54, 15, 54, 56
};

static const unsigned char _ironbee_config_trans_actions[] = {
	35, 106, 106, 100, 106, 33, 0, 0,
	0, 120, 88, 132, 0, 64, 132, 1,
	0, 1, 0, 0, 0, 15, 91, 136,
	0, 1, 73, 1, 0, 0, 0, 37,
	94, 140, 0, 17, 140, 3, 1, 43,
	97, 144, 0, 0, 144, 0, 1, 19,
	0, 0, 79, 124, 79, 0, 21, 0,
	1, 0, 1, 0, 79, 79, 0, 0,
	0, 0, 79, 148, 109, 25, 148, 0,
	0, 148, 1, 61, 0, 31, 58, 55,
	27, 106, 106, 106, 106, 106, 103, 5,
	5, 82, 128, 152, 9, 29, 23, 116,
	49, 0, 112, 52, 0, 70, 0, 67,
	76, 0, 40, 7
};

static const unsigned char _ironbee_config_to_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 85, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 85, 0, 0, 0,
	85, 0, 0, 85, 0, 0, 85, 0,
	0, 0, 11
};

static const unsigned char _ironbee_config_from_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 13, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 13, 0, 0, 0,
	13, 0, 0, 13, 0, 0, 13, 0,
	0, 0, 13
};

static const unsigned char _ironbee_config_eof_actions[] = {
	0, 0, 0, 0, 0, 0, 46, 46,
	46, 0, 0, 0, 0, 0, 3, 0,
	3, 3, 3, 3, 3, 3, 3, 3,
	3, 3, 3, 3, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 46, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0
};

static const short _ironbee_config_eof_trans[] = {
	0, 1, 1, 6, 6, 10, 0, 0,
	0, 22, 0, 0, 0, 32, 0, 40,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 76, 78, 79,
	81, 79, 79, 79, 79, 79, 79, 79,
	90, 79, 91, 94, 0, 96, 96, 96,
	0, 100, 100, 0, 102, 102, 0, 105,
	107, 105, 0
};

static const int ironbee_config_start = 28;
static const int ironbee_config_first_final = 28;
static const int ironbee_config_error = 0;

static const int ironbee_config_en_parameters = 44;
static const int ironbee_config_en_block_parameters = 48;
static const int ironbee_config_en_newblock = 51;
static const int ironbee_config_en_endblock = 54;
static const int ironbee_config_en_finclude = 58;
static const int ironbee_config_en_main = 28;


#line 353 "config-parser.rl"

ib_status_t ib_cfgparser_ragel_parse_chunk(ib_cfgparser_t *cp,
                                           const char *buf,
                                           const size_t blen,
                                           const char *file,
                                           const unsigned lineno,
                                           const int is_last_chunk)
{
    ib_engine_t *ib_engine = cp->ib;

    /* Temporary memory pool. */
    ib_mpool_t *mptmp = ib_engine_pool_temp_get(ib_engine);

    /* Configuration memory pool. */
    ib_mpool_t *mpcfg = ib_engine_pool_config_get(ib_engine);

    /* Error actions will update this. */
    ib_status_t rc = IB_OK;

    /* Directive name being parsed. */
    char *directive = NULL;

    /* Block name being parsed. */
    char *blkname = NULL;

    /* Parameter value being added to the plist. */
    char *pval = NULL;

    /* Store the start of a string to act on.
     * fpc - mark is the string length when processing after
     * a mark action. */
    const char *mark = buf;

    /* Temporary list for storing values before they are committed to the
     * configuration. */
    ib_list_t *plist;

    /* Create a finite state machine type. */
    fsm_t fsm;

    fsm.p = buf;
    fsm.pe = buf + blen;
    fsm.eof = (is_last_chunk ? fsm.pe : NULL);
    memset(fsm.stack, 0, sizeof(fsm.stack));

    /* Create a temporary list for storing parameter values. */
    ib_list_create(&plist, mptmp);
    if (plist == NULL) {
        return IB_EALLOC;
    }

    /* Access all ragel state variables via structure. */

#line 406 "config-parser.rl"

#line 407 "config-parser.rl"

#line 408 "config-parser.rl"

#line 409 "config-parser.rl"


#line 516 "config-parser.c"
	{
	 fsm.cs = ironbee_config_start;
	 fsm.top = 0;
	 fsm.ts = 0;
	 fsm.te = 0;
	 fsm.act = 0;
	}

#line 411 "config-parser.rl"

#line 527 "config-parser.c"
	{
	int _klen;
	unsigned int _trans;
	const char *_acts;
	unsigned int _nacts;
	const char *_keys;

	if ( ( fsm.p) == ( fsm.pe) )
		goto _test_eof;
	if (  fsm.cs == 0 )
		goto _out;
_resume:
	_acts = _ironbee_config_actions + _ironbee_config_from_state_actions[ fsm.cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 13:
#line 1 "NONE"
	{ fsm.ts = ( fsm.p);}
	break;
#line 548 "config-parser.c"
		}
	}

	_keys = _ironbee_config_trans_keys + _ironbee_config_key_offsets[ fsm.cs];
	_trans = _ironbee_config_index_offsets[ fsm.cs];

	_klen = _ironbee_config_single_lengths[ fsm.cs];
	if ( _klen > 0 ) {
		const char *_lower = _keys;
		const char *_mid;
		const char *_upper = _keys + _klen - 1;
		while (1) {
			if ( _upper < _lower )
				break;

			_mid = _lower + ((_upper-_lower) >> 1);
			if ( (*( fsm.p)) < *_mid )
				_upper = _mid - 1;
			else if ( (*( fsm.p)) > *_mid )
				_lower = _mid + 1;
			else {
				_trans += (_mid - _keys);
				goto _match;
			}
		}
		_keys += _klen;
		_trans += _klen;
	}

	_klen = _ironbee_config_range_lengths[ fsm.cs];
	if ( _klen > 0 ) {
		const char *_lower = _keys;
		const char *_mid;
		const char *_upper = _keys + (_klen<<1) - 2;
		while (1) {
			if ( _upper < _lower )
				break;

			_mid = _lower + (((_upper-_lower) >> 1) & ~1);
			if ( (*( fsm.p)) < _mid[0] )
				_upper = _mid - 2;
			else if ( (*( fsm.p)) > _mid[1] )
				_lower = _mid + 2;
			else {
				_trans += ((_mid - _keys)>>1);
				goto _match;
			}
		}
		_trans += _klen;
	}

_match:
	_trans = _ironbee_config_indicies[_trans];
_eof_trans:
	 fsm.cs = _ironbee_config_trans_targs[_trans];

	if ( _ironbee_config_trans_actions[_trans] == 0 )
		goto _again;

	_acts = _ironbee_config_actions + _ironbee_config_trans_actions[_trans];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 )
	{
		switch ( *_acts++ )
		{
	case 0:
#line 207 "config-parser.rl"
	{ mark = ( fsm.p); }
	break;
	case 1:
#line 208 "config-parser.rl"
	{
        rc = IB_EOTHER;
        ib_cfg_log_error(cp,
                         "parser error before \"%.*s\"",
                         (int)(( fsm.p) - mark), mark);
    }
	break;
	case 2:
#line 216 "config-parser.rl"
	{
        pval = alloc_cpy_marked_string(cp, mark, ( fsm.p), mpcfg);
        ib_list_push(plist, pval);
    }
	break;
	case 3:
#line 220 "config-parser.rl"
	{
        pval = alloc_cpy_marked_string(cp, mark, ( fsm.p), mpcfg);
        ib_list_push(plist, pval);
    }
	break;
	case 4:
#line 226 "config-parser.rl"
	{
        size_t namelen = (size_t)(( fsm.p) - mark);
        directive = (char *)calloc(namelen + 1, sizeof(*directive));
        memcpy(directive, mark, namelen);
        ib_list_clear(plist);
    }
	break;
	case 5:
#line 232 "config-parser.rl"
	{
        rc = ib_config_directive_process(cp, directive, plist);
        if (rc != IB_OK) {
            ib_cfg_log_error(cp,
                             "Failed to process directive \"%s\" "
                             ": %s (see preceeding messages for details)",
                             directive, ib_status_to_string(rc));
        }
        if (directive != NULL) {
            free(directive);
            directive = NULL;
        }
    }
	break;
	case 6:
#line 247 "config-parser.rl"
	{
        size_t namelen = (size_t)(( fsm.p) - mark);
        blkname = (char *)calloc(namelen + 1, sizeof(*blkname));
        memcpy(blkname, mark, namelen);
        ib_list_clear(plist);
    }
	break;
	case 7:
#line 253 "config-parser.rl"
	{
        rc = ib_config_block_start(cp, blkname, plist);
        if (rc != IB_OK) {
            ib_cfg_log_error(cp,
	                     "Failed to start block \"%s\": %s",
                             blkname, ib_status_to_string(rc));
        }
    }
	break;
	case 8:
#line 261 "config-parser.rl"
	{
        blkname = (char *)cp->cur_blkname;
        rc = ib_config_block_process(cp, blkname);
        if (rc != IB_OK) {
            ib_cfg_log_error(cp,
                             "Failed to process block \"%s\": %s",
                             blkname, ib_status_to_string(rc));
        }
        if (blkname != NULL) {
            free(blkname);
        }
        blkname = (char *)cp->cur_blkname;
    }
	break;
	case 9:
#line 276 "config-parser.rl"
	{
        if (directive != NULL) {
            free(directive);
            directive = NULL;
        }
        rc = include_config_fn(cp, mpcfg, mark, ( fsm.p), file, lineno);
        if (rc == IB_OK) {
            ib_cfg_log_debug(cp, "Done processing include directive");
        }
        else {
            ib_cfg_log_error(cp,
                             "Failed to process include directive: %s",
                             ib_status_to_string(rc));
        }
    }
	break;
	case 10:
#line 293 "config-parser.rl"
	{
        size_t len = (size_t)(( fsm.p) - mark);
        ib_cfg_log_debug(cp, "contination: \"%.*s\"", (int)len, mark);
        /* blkname = (char *)calloc(namelen + 1, sizeof(*blkname));
           memcpy(blkname, mark, namelen); */
    }
	break;
	case 14:
#line 1 "NONE"
	{ fsm.te = ( fsm.p)+1;}
	break;
	case 15:
#line 316 "config-parser.rl"
	{ fsm.act = 1;}
	break;
	case 16:
#line 317 "config-parser.rl"
	{ fsm.te = ( fsm.p)+1;{ { fsm.cs =  fsm.stack[-- fsm.top]; goto _again;} }}
	break;
	case 17:
#line 316 "config-parser.rl"
	{ fsm.te = ( fsm.p);( fsm.p)--;}
	break;
	case 18:
#line 1 "NONE"
	{	switch(  fsm.act ) {
	case 0:
	{{ fsm.cs = 0; goto _again;}}
	break;
	default:
	{{( fsm.p) = (( fsm.te))-1;}}
	break;
	}
	}
	break;
	case 19:
#line 321 "config-parser.rl"
	{ fsm.act = 3;}
	break;
	case 20:
#line 322 "config-parser.rl"
	{ fsm.te = ( fsm.p)+1;{ { fsm.cs =  fsm.stack[-- fsm.top]; goto _again;} }}
	break;
	case 21:
#line 321 "config-parser.rl"
	{ fsm.te = ( fsm.p);( fsm.p)--;}
	break;
	case 22:
#line 1 "NONE"
	{	switch(  fsm.act ) {
	case 0:
	{{ fsm.cs = 0; goto _again;}}
	break;
	default:
	{{( fsm.p) = (( fsm.te))-1;}}
	break;
	}
	}
	break;
	case 23:
#line 326 "config-parser.rl"
	{ fsm.act = 5;}
	break;
	case 24:
#line 327 "config-parser.rl"
	{ fsm.te = ( fsm.p)+1;{ { fsm.cs =  fsm.stack[-- fsm.top]; goto _again;} }}
	break;
	case 25:
#line 326 "config-parser.rl"
	{ fsm.te = ( fsm.p);( fsm.p)--;{ { fsm.stack[ fsm.top++] =  fsm.cs;  fsm.cs = 48; goto _again;} }}
	break;
	case 26:
#line 1 "NONE"
	{	switch(  fsm.act ) {
	case 0:
	{{ fsm.cs = 0; goto _again;}}
	break;
	case 5:
	{{( fsm.p) = (( fsm.te))-1;} { fsm.stack[ fsm.top++] =  fsm.cs;  fsm.cs = 48; goto _again;} }
	break;
	}
	}
	break;
	case 27:
#line 332 "config-parser.rl"
	{ fsm.act = 8;}
	break;
	case 28:
#line 333 "config-parser.rl"
	{ fsm.te = ( fsm.p)+1;{ { fsm.cs =  fsm.stack[-- fsm.top]; goto _again;} }}
	break;
	case 29:
#line 331 "config-parser.rl"
	{ fsm.te = ( fsm.p);( fsm.p)--;{ { fsm.cs =  fsm.stack[-- fsm.top]; goto _again;} }}
	break;
	case 30:
#line 332 "config-parser.rl"
	{ fsm.te = ( fsm.p);( fsm.p)--;}
	break;
	case 31:
#line 1 "NONE"
	{	switch(  fsm.act ) {
	case 0:
	{{ fsm.cs = 0; goto _again;}}
	break;
	default:
	{{( fsm.p) = (( fsm.te))-1;}}
	break;
	}
	}
	break;
	case 32:
#line 337 "config-parser.rl"
	{ fsm.te = ( fsm.p)+1;{ { fsm.cs =  fsm.stack[-- fsm.top]; goto _again;} }}
	break;
	case 33:
#line 342 "config-parser.rl"
	{ fsm.act = 12;}
	break;
	case 34:
#line 343 "config-parser.rl"
	{ fsm.act = 13;}
	break;
	case 35:
#line 344 "config-parser.rl"
	{ fsm.act = 14;}
	break;
	case 36:
#line 347 "config-parser.rl"
	{ fsm.act = 17;}
	break;
	case 37:
#line 346 "config-parser.rl"
	{ fsm.te = ( fsm.p)+1;{ { fsm.stack[ fsm.top++] =  fsm.cs;  fsm.cs = 54; goto _again;} }}
	break;
	case 38:
#line 348 "config-parser.rl"
	{ fsm.te = ( fsm.p)+1;}
	break;
	case 39:
#line 341 "config-parser.rl"
	{ fsm.te = ( fsm.p);( fsm.p)--;}
	break;
	case 40:
#line 342 "config-parser.rl"
	{ fsm.te = ( fsm.p);( fsm.p)--;}
	break;
	case 41:
#line 344 "config-parser.rl"
	{ fsm.te = ( fsm.p);( fsm.p)--;{ { fsm.stack[ fsm.top++] =  fsm.cs;  fsm.cs = 44; goto _again;} }}
	break;
	case 42:
#line 345 "config-parser.rl"
	{ fsm.te = ( fsm.p);( fsm.p)--;{ { fsm.stack[ fsm.top++] =  fsm.cs;  fsm.cs = 51; goto _again;} }}
	break;
	case 43:
#line 347 "config-parser.rl"
	{ fsm.te = ( fsm.p);( fsm.p)--;}
	break;
	case 44:
#line 344 "config-parser.rl"
	{{( fsm.p) = (( fsm.te))-1;}{ { fsm.stack[ fsm.top++] =  fsm.cs;  fsm.cs = 44; goto _again;} }}
	break;
	case 45:
#line 1 "NONE"
	{	switch(  fsm.act ) {
	case 0:
	{{ fsm.cs = 0; goto _again;}}
	break;
	case 13:
	{{( fsm.p) = (( fsm.te))-1;} { fsm.stack[ fsm.top++] =  fsm.cs;  fsm.cs = 58; goto _again;} }
	break;
	case 14:
	{{( fsm.p) = (( fsm.te))-1;} { fsm.stack[ fsm.top++] =  fsm.cs;  fsm.cs = 44; goto _again;} }
	break;
	default:
	{{( fsm.p) = (( fsm.te))-1;}}
	break;
	}
	}
	break;
#line 903 "config-parser.c"
		}
	}

_again:
	_acts = _ironbee_config_actions + _ironbee_config_to_state_actions[ fsm.cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 11:
#line 1 "NONE"
	{ fsm.ts = 0;}
	break;
	case 12:
#line 1 "NONE"
	{ fsm.act = 0;}
	break;
#line 920 "config-parser.c"
		}
	}

	if (  fsm.cs == 0 )
		goto _out;
	if ( ++( fsm.p) != ( fsm.pe) )
		goto _resume;
	_test_eof: {}
	if ( ( fsm.p) == ( fsm.eof) )
	{
	if ( _ironbee_config_eof_trans[ fsm.cs] > 0 ) {
		_trans = _ironbee_config_eof_trans[ fsm.cs] - 1;
		goto _eof_trans;
	}
	const char *__acts = _ironbee_config_actions + _ironbee_config_eof_actions[ fsm.cs];
	unsigned int __nacts = (unsigned int) *__acts++;
	while ( __nacts-- > 0 ) {
		switch ( *__acts++ ) {
	case 1:
#line 208 "config-parser.rl"
	{
        rc = IB_EOTHER;
        ib_cfg_log_error(cp,
                         "parser error before \"%.*s\"",
                         (int)(( fsm.p) - mark), mark);
    }
	break;
	case 2:
#line 216 "config-parser.rl"
	{
        pval = alloc_cpy_marked_string(cp, mark, ( fsm.p), mpcfg);
        ib_list_push(plist, pval);
    }
	break;
	case 5:
#line 232 "config-parser.rl"
	{
        rc = ib_config_directive_process(cp, directive, plist);
        if (rc != IB_OK) {
            ib_cfg_log_error(cp,
                             "Failed to process directive \"%s\" "
                             ": %s (see preceeding messages for details)",
                             directive, ib_status_to_string(rc));
        }
        if (directive != NULL) {
            free(directive);
            directive = NULL;
        }
    }
	break;
#line 971 "config-parser.c"
		}
	}
	}

	_out: {}
	}

#line 412 "config-parser.rl"

    /* Ensure that our block is always empty on last chunk. */
    if ( is_last_chunk && blkname != NULL ) {
        return IB_EINVAL;
    }

    return rc;
}
