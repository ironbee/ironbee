
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
 *****************************************************************************/

/**
 * @file
 * @brief IronBee &mdash; Configuration File Parser
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

#include "config-parser.h"
#include "ironbee_private.h"

/**
 * Finite state machine type.
 * @internal
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
 * @param[in] fpc_mark The start of the string.
 * @param[in] fpc The current character from ragel.
 * @param[in,out] mp Temporary memory pool passed in by Ragel.
 * @return a buffer allocated from the tmpmp memory pool
 *         available in ib_cfgparser_ragel_parse_chunk. This buffer may be
 *         larger than the string stored in it if the length of the string is
 *         reduced by Javascript unescaping.
 */
static char* alloc_cpy_marked_string(const char *fpc_mark,
                                     const char *fpc,
                                     ib_mpool_t* mp)
{
    const char *afpc = fpc;
    size_t pvallen;
    char* pval;
    /* Adjust for quoted value. */
    if ((*fpc_mark == '"') && (*(afpc-1) == '"') && (fpc_mark+1 < afpc-2)) {
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

    pval = alloc_cpy_marked_string(mark, fpc, mp);
    incfile = ib_util_relative_file(mp, file, pval);

    if (access(incfile, R_OK) != 0) {
        ib_log_error_cfg(cp, "Cannot access included file \"%s\": %s",
                         incfile, strerror(errno));
        return IB_ENOENT;
    }

    statval = stat(incfile, &statbuf);
    if (statval != 0) {
        ib_log_error_cfg(cp,
                         "Failed to stat include file \"%s\": %s",
                         incfile, strerror(errno));
        return IB_ENOENT;
    }

    if (S_ISREG(statbuf.st_mode) == 0) {
        ib_log_error_cfg(cp,
	                 "Included file \"%s\" isn't a file", incfile);
        return IB_ENOENT;
    }

    ib_log_debug_cfg(cp, "Including '%s'", incfile);
    rc = ib_cfgparser_parse(cp, incfile);
    if (rc != IB_OK) {
        ib_log_error(cp->ib, "Error parsing included file \"%s\": %s",
                     incfile, ib_status_to_string(rc));
        return rc;
    }

    ib_log_debug(cp->ib, "Done processing include file \"%s\"", incfile);
    return IB_OK;
}


#line 280 "config-parser.rl"



#line 156 "config-parser.c"
static const char _ironbee_config_actions[] = {
	0, 1, 0, 1, 1, 1, 8, 1,
	10, 1, 12, 1, 21, 1, 23, 1,
	27, 1, 31, 1, 35, 1, 36, 1,
	37, 1, 39, 1, 40, 1, 41, 2,
	1, 25, 2, 1, 28, 2, 1, 30,
	2, 2, 5, 2, 2, 16, 2, 3,
	20, 2, 4, 36, 2, 4, 38, 2,
	4, 41, 2, 5, 15, 2, 6, 23,
	2, 6, 24, 2, 7, 19, 2, 8,
	29, 2, 9, 0, 2, 10, 11, 2,
	13, 14, 2, 13, 18, 2, 13, 22,
	2, 13, 26, 2, 13, 32, 2, 13,
	33, 2, 13, 34, 3, 2, 5, 15,
	3, 2, 5, 16, 3, 2, 5, 17,
	3, 9, 0, 31, 3, 13, 0, 14,
	3, 13, 0, 18, 3, 13, 0, 22,
	3, 13, 0, 26, 3, 13, 0, 33

};

static const unsigned char _ironbee_config_key_offsets[] = {
	0, 0, 0, 0, 9, 11, 11, 11,
	19, 21, 21, 21, 30, 30, 39, 43,
	44, 48, 52, 53, 57, 62, 67, 69,
	73, 75, 86, 93, 104, 111, 112, 121,
	130, 139, 148, 157, 166, 167, 176, 183,
	190, 190, 198, 205, 205, 214, 221, 228,
	237, 244, 244, 251
};

static const char _ironbee_config_trans_keys[] = {
	9, 10, 13, 32, 34, 35, 60, 62,
	92, 34, 92, 9, 10, 32, 34, 35,
	60, 62, 92, 34, 92, 9, 10, 13,
	32, 60, 62, 92, 34, 35, 9, 10,
	13, 32, 60, 62, 92, 34, 35, 9,
	10, 13, 32, 10, 9, 10, 13, 32,
	9, 10, 13, 32, 10, 9, 10, 32,
	34, 9, 10, 13, 32, 34, 9, 10,
	13, 32, 34, 10, 34, 9, 10, 13,
	32, 10, 34, 9, 10, 13, 32, 34,
	35, 60, 62, 73, 92, 105, 32, 34,
	60, 62, 92, 9, 10, 9, 10, 13,
	32, 34, 35, 60, 62, 73, 92, 105,
	9, 10, 32, 34, 60, 62, 92, 10,
	32, 34, 60, 62, 78, 92, 110, 9,
	10, 32, 34, 60, 62, 67, 92, 99,
	9, 10, 32, 34, 60, 62, 76, 92,
	108, 9, 10, 32, 34, 60, 62, 85,
	92, 117, 9, 10, 32, 34, 60, 62,
	68, 92, 100, 9, 10, 32, 34, 60,
	62, 69, 92, 101, 9, 10, 47, 9,
	10, 13, 32, 34, 35, 60, 62, 92,
	32, 34, 60, 62, 92, 9, 10, 9,
	10, 32, 34, 60, 62, 92, 9, 10,
	32, 34, 35, 60, 62, 92, 32, 34,
	60, 62, 92, 9, 10, 9, 10, 13,
	32, 60, 62, 92, 34, 35, 32, 34,
	60, 62, 92, 9, 10, 9, 10, 32,
	34, 60, 62, 92, 9, 10, 13, 32,
	60, 62, 92, 34, 35, 32, 34, 60,
	62, 92, 9, 10, 9, 10, 32, 34,
	60, 62, 92, 9, 10, 32, 34, 0
};

static const char _ironbee_config_single_lengths[] = {
	0, 0, 0, 9, 2, 0, 0, 8,
	2, 0, 0, 7, 0, 7, 4, 1,
	4, 4, 1, 4, 5, 5, 2, 4,
	2, 11, 5, 11, 7, 1, 7, 7,
	7, 7, 7, 7, 1, 9, 5, 7,
	0, 8, 5, 0, 7, 5, 7, 7,
	5, 0, 7, 4
};

static const char _ironbee_config_range_lengths[] = {
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 1, 0, 1, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 1, 0, 0, 0, 1, 1,
	1, 1, 1, 1, 0, 0, 1, 0,
	0, 0, 1, 0, 1, 1, 0, 1,
	1, 0, 0, 0
};

static const short _ironbee_config_index_offsets[] = {
	0, 0, 1, 2, 12, 15, 16, 17,
	26, 29, 30, 31, 40, 41, 50, 55,
	57, 62, 67, 69, 74, 80, 86, 89,
	94, 97, 109, 116, 128, 136, 138, 147,
	156, 165, 174, 183, 192, 194, 204, 211,
	219, 220, 229, 236, 237, 246, 253, 261,
	270, 277, 278, 286
};

static const char _ironbee_config_indicies[] = {
	1, 3, 5, 6, 7, 5, 8, 9,
	9, 9, 10, 4, 12, 13, 11, 11,
	15, 17, 9, 17, 18, 9, 9, 19,
	20, 16, 22, 23, 21, 21, 25, 27,
	28, 29, 27, 30, 30, 31, 30, 26,
	33, 35, 36, 37, 35, 30, 38, 39,
	30, 34, 38, 40, 41, 38, 30, 40,
	30, 43, 44, 45, 43, 42, 46, 47,
	48, 46, 30, 47, 30, 50, 30, 50,
	51, 49, 53, 44, 54, 53, 42, 52,
	56, 47, 57, 56, 58, 55, 30, 58,
	55, 43, 44, 59, 43, 30, 47, 58,
	55, 61, 62, 63, 61, 9, 64, 65,
	9, 66, 67, 66, 60, 68, 68, 68,
	68, 69, 68, 1, 61, 62, 63, 61,
	70, 64, 70, 70, 66, 67, 66, 60,
	71, 72, 71, 71, 71, 71, 69, 1,
	73, 64, 71, 71, 71, 71, 74, 69,
	74, 71, 1, 71, 71, 71, 71, 75,
	69, 75, 71, 1, 71, 71, 71, 71,
	76, 69, 76, 71, 1, 71, 71, 71,
	71, 77, 69, 77, 71, 1, 71, 71,
	71, 71, 78, 69, 78, 71, 1, 71,
	71, 71, 71, 79, 69, 79, 71, 1,
	81, 80, 5, 6, 7, 5, 8, 9,
	9, 9, 10, 4, 83, 83, 83, 83,
	84, 83, 3, 83, 85, 83, 83, 83,
	83, 84, 3, 83, 17, 9, 17, 18,
	9, 9, 19, 20, 16, 86, 86, 86,
	86, 87, 86, 15, 86, 27, 28, 29,
	27, 30, 30, 31, 30, 26, 88, 88,
	88, 88, 89, 88, 25, 88, 90, 88,
	88, 88, 88, 89, 25, 35, 36, 37,
	35, 30, 38, 39, 30, 34, 91, 91,
	91, 91, 92, 91, 33, 93, 91, 94,
	91, 91, 91, 91, 92, 33, 50, 30,
	50, 51, 49, 0
};

static const char _ironbee_config_trans_targs[] = {
	25, 26, 37, 38, 38, 3, 37, 39,
	4, 0, 2, 4, 40, 5, 41, 42,
	42, 7, 8, 41, 6, 8, 43, 9,
	44, 45, 45, 11, 44, 46, 0, 10,
	47, 48, 48, 13, 49, 50, 14, 12,
	47, 15, 16, 17, 51, 16, 17, 51,
	18, 16, 19, 20, 20, 21, 20, 22,
	21, 24, 23, 18, 26, 27, 25, 28,
	29, 36, 30, 1, 25, 1, 25, 25,
	25, 25, 31, 32, 33, 34, 35, 26,
	25, 25, 37, 37, 2, 37, 41, 6,
	44, 10, 44, 47, 12, 47, 49
};

static const unsigned char _ironbee_config_trans_actions[] = {
	29, 94, 108, 79, 116, 0, 58, 116,
	1, 0, 1, 0, 0, 0, 11, 82,
	120, 0, 1, 67, 1, 0, 0, 0,
	31, 85, 124, 0, 13, 124, 3, 1,
	37, 88, 128, 0, 0, 128, 0, 1,
	15, 0, 0, 73, 112, 73, 0, 17,
	0, 1, 0, 1, 0, 73, 73, 0,
	0, 0, 0, 73, 132, 97, 21, 132,
	0, 0, 132, 1, 55, 0, 27, 52,
	49, 23, 94, 94, 94, 94, 94, 91,
	25, 19, 104, 43, 0, 100, 46, 0,
	64, 0, 61, 70, 0, 34, 5
};

static const unsigned char _ironbee_config_to_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 76, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 76, 0, 0,
	0, 76, 0, 0, 76, 0, 0, 76,
	0, 0, 0, 7
};

static const unsigned char _ironbee_config_from_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 9, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 9, 0, 0,
	0, 9, 0, 0, 9, 0, 0, 9,
	0, 0, 0, 9
};

static const unsigned char _ironbee_config_eof_actions[] = {
	0, 0, 0, 40, 40, 40, 0, 0,
	0, 0, 0, 3, 0, 3, 3, 3,
	3, 3, 3, 3, 3, 3, 3, 3,
	3, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 40, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0
};

static const short _ironbee_config_eof_trans[] = {
	0, 1, 3, 0, 0, 0, 15, 0,
	0, 0, 25, 0, 33, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 69, 71, 72, 74, 72, 72,
	72, 72, 72, 72, 81, 0, 83, 83,
	83, 0, 87, 87, 0, 89, 89, 0,
	92, 94, 92, 0
};

static const int ironbee_config_start = 25;
static const int ironbee_config_first_final = 25;
static const int ironbee_config_error = 0;

static const int ironbee_config_en_parameters = 37;
static const int ironbee_config_en_block_parameters = 41;
static const int ironbee_config_en_newblock = 44;
static const int ironbee_config_en_endblock = 47;
static const int ironbee_config_en_finclude = 51;
static const int ironbee_config_en_main = 25;


#line 283 "config-parser.rl"

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

#line 336 "config-parser.rl"

#line 337 "config-parser.rl"

#line 338 "config-parser.rl"

#line 339 "config-parser.rl"


#line 438 "config-parser.c"
	{
	 fsm.cs = ironbee_config_start;
	 fsm.top = 0;
	 fsm.ts = 0;
	 fsm.te = 0;
	 fsm.act = 0;
	}

#line 341 "config-parser.rl"

#line 449 "config-parser.c"
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
	case 12:
#line 1 "NONE"
	{ fsm.ts = ( fsm.p);}
	break;
#line 470 "config-parser.c"
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
				_trans += (unsigned int)(_mid - _keys);
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
				_trans += (unsigned int)((_mid - _keys)>>1);
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
#line 152 "config-parser.rl"
	{ mark = ( fsm.p); }
	break;
	case 1:
#line 153 "config-parser.rl"
	{
        rc = IB_EOTHER;
        ib_log_error_cfg(cp,
                         "parser error before \"%.*s\"",
                         (int)(( fsm.p) - mark), mark);
    }
	break;
	case 2:
#line 161 "config-parser.rl"
	{
        pval = alloc_cpy_marked_string(mark, ( fsm.p), mpcfg);
        ib_list_push(plist, pval);
    }
	break;
	case 3:
#line 165 "config-parser.rl"
	{
        pval = alloc_cpy_marked_string(mark, ( fsm.p), mpcfg);
        ib_list_push(plist, pval);
    }
	break;
	case 4:
#line 171 "config-parser.rl"
	{
        size_t namelen = (size_t)(( fsm.p) - mark);
        directive = (char *)calloc(namelen + 1, sizeof(*directive));
        memcpy(directive, mark, namelen);
        ib_list_clear(plist);
    }
	break;
	case 5:
#line 177 "config-parser.rl"
	{
        rc = ib_config_directive_process(cp, directive, plist);
        if (rc != IB_OK) {
            ib_log_error_cfg(cp,
                             "Failed to process directive \"%s\": %s",
                             directive, ib_status_to_string(rc));
        }
        if (directive != NULL) {
            free(directive);
        }
    }
	break;
	case 6:
#line 190 "config-parser.rl"
	{
        size_t namelen = (size_t)(( fsm.p) - mark);
        blkname = (char *)calloc(namelen + 1, sizeof(*blkname));
        memcpy(blkname, mark, namelen);
        ib_list_clear(plist);
    }
	break;
	case 7:
#line 196 "config-parser.rl"
	{
        rc = ib_config_block_start(cp, blkname, plist);
        if (rc != IB_OK) {
            ib_log_error_cfg(cp,
	                     "Failed to start block \"%s\": %s",
                             blkname, ib_status_to_string(rc));
        }
    }
	break;
	case 8:
#line 204 "config-parser.rl"
	{
        blkname = (char *)cp->cur_blkname;
        rc = ib_config_block_process(cp, blkname);
        if (rc != IB_OK) {
            ib_log_error_cfg(cp,
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
#line 219 "config-parser.rl"
	{
        rc = include_config_fn(cp, mpcfg, mark, ( fsm.p), file, lineno);
        if (rc == IB_OK) {
            ib_log_debug_cfg(cp, "Done processing include direction");
        }
        else {
            ib_log_error_cfg(cp,
                             "Failed to process include directive: %s",
                             ib_status_to_string(rc));
        }
    }
	break;
	case 13:
#line 1 "NONE"
	{ fsm.te = ( fsm.p)+1;}
	break;
	case 14:
#line 247 "config-parser.rl"
	{ fsm.act = 1;}
	break;
	case 15:
#line 248 "config-parser.rl"
	{ fsm.te = ( fsm.p)+1;{ { fsm.cs =  fsm.stack[-- fsm.top]; goto _again;} }}
	break;
	case 16:
#line 247 "config-parser.rl"
	{ fsm.te = ( fsm.p);( fsm.p)--;}
	break;
	case 17:
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
	case 18:
#line 252 "config-parser.rl"
	{ fsm.act = 3;}
	break;
	case 19:
#line 253 "config-parser.rl"
	{ fsm.te = ( fsm.p)+1;{ { fsm.cs =  fsm.stack[-- fsm.top]; goto _again;} }}
	break;
	case 20:
#line 252 "config-parser.rl"
	{ fsm.te = ( fsm.p);( fsm.p)--;}
	break;
	case 21:
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
	case 22:
#line 257 "config-parser.rl"
	{ fsm.act = 5;}
	break;
	case 23:
#line 258 "config-parser.rl"
	{ fsm.te = ( fsm.p)+1;{ { fsm.cs =  fsm.stack[-- fsm.top]; goto _again;} }}
	break;
	case 24:
#line 257 "config-parser.rl"
	{ fsm.te = ( fsm.p);( fsm.p)--;{ { fsm.stack[ fsm.top++] =  fsm.cs;  fsm.cs = 41; goto _again;} }}
	break;
	case 25:
#line 1 "NONE"
	{	switch(  fsm.act ) {
	case 0:
	{{ fsm.cs = 0; goto _again;}}
	break;
	case 5:
	{{( fsm.p) = (( fsm.te))-1;} { fsm.stack[ fsm.top++] =  fsm.cs;  fsm.cs = 41; goto _again;} }
	break;
	}
	}
	break;
	case 26:
#line 263 "config-parser.rl"
	{ fsm.act = 8;}
	break;
	case 27:
#line 264 "config-parser.rl"
	{ fsm.te = ( fsm.p)+1;{ { fsm.cs =  fsm.stack[-- fsm.top]; goto _again;} }}
	break;
	case 28:
#line 262 "config-parser.rl"
	{ fsm.te = ( fsm.p);( fsm.p)--;{ { fsm.cs =  fsm.stack[-- fsm.top]; goto _again;} }}
	break;
	case 29:
#line 263 "config-parser.rl"
	{ fsm.te = ( fsm.p);( fsm.p)--;}
	break;
	case 30:
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
	case 31:
#line 268 "config-parser.rl"
	{ fsm.te = ( fsm.p)+1;{ { fsm.cs =  fsm.stack[-- fsm.top]; goto _again;} }}
	break;
	case 32:
#line 273 "config-parser.rl"
	{ fsm.act = 12;}
	break;
	case 33:
#line 274 "config-parser.rl"
	{ fsm.act = 13;}
	break;
	case 34:
#line 277 "config-parser.rl"
	{ fsm.act = 16;}
	break;
	case 35:
#line 276 "config-parser.rl"
	{ fsm.te = ( fsm.p)+1;{ { fsm.stack[ fsm.top++] =  fsm.cs;  fsm.cs = 47; goto _again;} }}
	break;
	case 36:
#line 278 "config-parser.rl"
	{ fsm.te = ( fsm.p)+1;}
	break;
	case 37:
#line 272 "config-parser.rl"
	{ fsm.te = ( fsm.p);( fsm.p)--;}
	break;
	case 38:
#line 274 "config-parser.rl"
	{ fsm.te = ( fsm.p);( fsm.p)--;{ { fsm.stack[ fsm.top++] =  fsm.cs;  fsm.cs = 37; goto _again;} }}
	break;
	case 39:
#line 275 "config-parser.rl"
	{ fsm.te = ( fsm.p);( fsm.p)--;{ { fsm.stack[ fsm.top++] =  fsm.cs;  fsm.cs = 44; goto _again;} }}
	break;
	case 40:
#line 277 "config-parser.rl"
	{ fsm.te = ( fsm.p);( fsm.p)--;}
	break;
	case 41:
#line 1 "NONE"
	{	switch(  fsm.act ) {
	case 0:
	{{ fsm.cs = 0; goto _again;}}
	break;
	case 12:
	{{( fsm.p) = (( fsm.te))-1;} { fsm.stack[ fsm.top++] =  fsm.cs;  fsm.cs = 51; goto _again;} }
	break;
	case 13:
	{{( fsm.p) = (( fsm.te))-1;} { fsm.stack[ fsm.top++] =  fsm.cs;  fsm.cs = 37; goto _again;} }
	break;
	default:
	{{( fsm.p) = (( fsm.te))-1;}}
	break;
	}
	}
	break;
#line 798 "config-parser.c"
		}
	}

_again:
	_acts = _ironbee_config_actions + _ironbee_config_to_state_actions[ fsm.cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 10:
#line 1 "NONE"
	{ fsm.ts = 0;}
	break;
	case 11:
#line 1 "NONE"
	{ fsm.act = 0;}
	break;
#line 815 "config-parser.c"
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
#line 153 "config-parser.rl"
	{
        rc = IB_EOTHER;
        ib_log_error_cfg(cp,
                         "parser error before \"%.*s\"",
                         (int)(( fsm.p) - mark), mark);
    }
	break;
	case 2:
#line 161 "config-parser.rl"
	{
        pval = alloc_cpy_marked_string(mark, ( fsm.p), mpcfg);
        ib_list_push(plist, pval);
    }
	break;
	case 5:
#line 177 "config-parser.rl"
	{
        rc = ib_config_directive_process(cp, directive, plist);
        if (rc != IB_OK) {
            ib_log_error_cfg(cp,
                             "Failed to process directive \"%s\": %s",
                             directive, ib_status_to_string(rc));
        }
        if (directive != NULL) {
            free(directive);
        }
    }
	break;
#line 864 "config-parser.c"
		}
	}
	}

	_out: {}
	}

#line 342 "config-parser.rl"

    /* Ensure that our block is always empty on last chunk. */
    if ( is_last_chunk && blkname != NULL ) {
        return IB_EINVAL;
    }

    return rc;
}
