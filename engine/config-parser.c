
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
 * @brief IronBee - Configuration File Parser
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

    /* At this point the buffer i pvallen+1 in size, but we cannot shrink it.  */
    /* This is not considered a problem for configuration parsing and it is
       deallocated after parsing and configuration is complete. */
    return pval;
}


#line 261 "config-parser.rl"



#line 107 "config-parser.c"
static const char _ironbee_config_actions[] = {
	0, 1, 0, 1, 1, 1, 12, 1,
	21, 1, 23, 1, 27, 1, 31, 1,
	37, 1, 38, 1, 39, 1, 41, 1,
	42, 1, 43, 2, 1, 25, 2, 1,
	29, 2, 1, 33, 2, 2, 5, 2,
	2, 16, 2, 3, 20, 2, 4, 38,
	2, 4, 40, 2, 4, 43, 2, 5,
	15, 2, 6, 23, 2, 6, 24, 2,
	7, 19, 2, 8, 28, 2, 9, 32,
	2, 10, 11, 2, 13, 14, 2, 13,
	18, 2, 13, 22, 2, 13, 26, 2,
	13, 30, 2, 13, 34, 2, 13, 35,
	2, 13, 36, 2, 31, 9, 3, 2,
	5, 15, 3, 2, 5, 16, 3, 2,
	5, 17, 3, 13, 0, 14, 3, 13,
	0, 18, 3, 13, 0, 22, 3, 13,
	0, 26, 3, 13, 0, 30, 3, 13,
	0, 35
};

static const unsigned char _ironbee_config_key_offsets[] = {
	0, 0, 0, 0, 9, 11, 11, 11,
	19, 21, 21, 21, 30, 30, 38, 42,
	43, 43, 52, 54, 54, 65, 72, 81,
	88, 89, 90, 98, 106, 114, 122, 130,
	138, 147, 154, 161, 161, 169, 176, 176,
	185, 192, 199, 207, 214, 223, 230, 237
};

static const char _ironbee_config_trans_keys[] = {
	9, 10, 13, 32, 34, 35, 60, 62,
	92, 34, 92, 9, 10, 32, 34, 35,
	60, 62, 92, 34, 92, 9, 10, 13,
	32, 60, 62, 92, 34, 35, 9, 10,
	32, 60, 62, 92, 34, 35, 9, 10,
	13, 32, 10, 9, 10, 13, 32, 34,
	35, 60, 62, 92, 34, 92, 9, 10,
	13, 32, 34, 35, 60, 62, 73, 92,
	105, 32, 34, 60, 62, 92, 9, 10,
	9, 10, 13, 32, 34, 35, 60, 62,
	92, 9, 10, 32, 34, 60, 62, 92,
	10, 47, 32, 34, 60, 62, 92, 110,
	9, 10, 32, 34, 60, 62, 92, 99,
	9, 10, 32, 34, 60, 62, 92, 108,
	9, 10, 32, 34, 60, 62, 92, 117,
	9, 10, 32, 34, 60, 62, 92, 100,
	9, 10, 32, 34, 60, 62, 92, 101,
	9, 10, 9, 10, 13, 32, 34, 35,
	60, 62, 92, 32, 34, 60, 62, 92,
	9, 10, 9, 10, 32, 34, 60, 62,
	92, 9, 10, 32, 34, 35, 60, 62,
	92, 32, 34, 60, 62, 92, 9, 10,
	9, 10, 13, 32, 60, 62, 92, 34,
	35, 32, 34, 60, 62, 92, 9, 10,
	9, 10, 32, 34, 60, 62, 92, 9,
	10, 32, 60, 62, 92, 34, 35, 32,
	34, 60, 62, 92, 9, 10, 9, 10,
	13, 32, 34, 35, 60, 62, 92, 32,
	34, 60, 62, 92, 9, 10, 9, 10,
	32, 34, 60, 62, 92, 0
};

static const char _ironbee_config_single_lengths[] = {
	0, 0, 0, 9, 2, 0, 0, 8,
	2, 0, 0, 7, 0, 6, 4, 1,
	0, 9, 2, 0, 11, 5, 9, 7,
	1, 1, 6, 6, 6, 6, 6, 6,
	9, 5, 7, 0, 8, 5, 0, 7,
	5, 7, 6, 5, 9, 5, 7, 0
};

static const char _ironbee_config_range_lengths[] = {
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 1, 0, 1, 0, 0,
	0, 0, 0, 0, 0, 1, 0, 0,
	0, 0, 1, 1, 1, 1, 1, 1,
	0, 1, 0, 0, 0, 1, 0, 1,
	1, 0, 1, 1, 0, 1, 0, 0
};

static const short _ironbee_config_index_offsets[] = {
	0, 0, 1, 2, 12, 15, 16, 17,
	26, 29, 30, 31, 40, 41, 49, 54,
	56, 57, 67, 70, 71, 83, 90, 100,
	108, 110, 112, 120, 128, 136, 144, 152,
	160, 170, 177, 185, 186, 195, 202, 203,
	212, 219, 227, 235, 242, 252, 259, 267
};

static const char _ironbee_config_indicies[] = {
	1, 3, 5, 6, 7, 5, 8, 9,
	9, 9, 10, 4, 12, 13, 11, 11,
	15, 17, 9, 17, 18, 9, 9, 19,
	20, 16, 22, 23, 21, 21, 25, 27,
	28, 29, 27, 30, 30, 31, 30, 26,
	33, 35, 30, 35, 30, 36, 37, 30,
	34, 36, 38, 39, 36, 30, 38, 30,
	41, 43, 44, 45, 43, 46, 30, 30,
	30, 47, 42, 49, 50, 48, 48, 52,
	53, 54, 52, 9, 55, 56, 9, 57,
	58, 57, 51, 59, 59, 59, 59, 60,
	59, 1, 52, 53, 54, 52, 61, 55,
	61, 61, 58, 51, 62, 63, 62, 62,
	62, 62, 60, 1, 64, 55, 66, 65,
	62, 62, 62, 62, 60, 67, 62, 1,
	62, 62, 62, 62, 60, 68, 62, 1,
	62, 62, 62, 62, 60, 69, 62, 1,
	62, 62, 62, 62, 60, 70, 62, 1,
	62, 62, 62, 62, 60, 71, 62, 1,
	62, 62, 62, 62, 60, 72, 62, 1,
	5, 6, 7, 5, 8, 9, 9, 9,
	10, 4, 74, 74, 74, 74, 75, 74,
	3, 74, 76, 74, 74, 74, 74, 75,
	3, 74, 17, 9, 17, 18, 9, 9,
	19, 20, 16, 77, 77, 77, 77, 78,
	77, 15, 77, 27, 28, 29, 27, 30,
	30, 31, 30, 26, 79, 79, 79, 79,
	80, 79, 25, 79, 81, 79, 79, 79,
	79, 80, 25, 35, 30, 35, 30, 36,
	37, 30, 34, 82, 82, 82, 82, 83,
	82, 33, 43, 44, 45, 43, 46, 30,
	30, 30, 47, 42, 84, 84, 84, 84,
	85, 84, 41, 84, 86, 84, 84, 84,
	84, 85, 41, 84, 0
};

static const char _ironbee_config_trans_targs[] = {
	20, 21, 32, 33, 33, 3, 32, 34,
	4, 0, 2, 4, 35, 5, 36, 37,
	37, 7, 8, 36, 6, 8, 38, 9,
	39, 40, 40, 11, 39, 41, 0, 10,
	42, 43, 43, 13, 14, 12, 42, 15,
	44, 45, 45, 17, 44, 46, 18, 16,
	18, 47, 19, 21, 22, 20, 23, 24,
	25, 26, 1, 20, 1, 20, 20, 20,
	20, 20, 20, 27, 28, 29, 30, 31,
	21, 32, 32, 2, 32, 36, 6, 39,
	10, 39, 42, 12, 44, 16, 44
};

static const unsigned char _ironbee_config_trans_actions[] = {
	25, 93, 110, 75, 114, 0, 54, 114,
	1, 0, 1, 0, 0, 0, 7, 78,
	118, 0, 1, 63, 1, 0, 0, 0,
	27, 81, 122, 0, 9, 122, 3, 1,
	30, 84, 126, 0, 0, 1, 11, 0,
	33, 87, 130, 0, 13, 130, 1, 1,
	0, 0, 0, 134, 96, 17, 134, 0,
	0, 134, 1, 51, 0, 23, 48, 45,
	19, 21, 15, 93, 93, 93, 93, 93,
	90, 106, 39, 0, 102, 42, 0, 60,
	0, 57, 66, 0, 69, 0, 99
};

static const unsigned char _ironbee_config_to_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 72, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	72, 0, 0, 0, 72, 0, 0, 72,
	0, 0, 72, 0, 72, 0, 0, 0
};

static const unsigned char _ironbee_config_from_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 5, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	5, 0, 0, 0, 5, 0, 0, 5,
	0, 0, 5, 0, 5, 0, 0, 0
};

static const unsigned char _ironbee_config_eof_actions[] = {
	0, 0, 0, 36, 36, 36, 0, 0,
	0, 0, 0, 3, 0, 3, 3, 3,
	0, 3, 3, 3, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	36, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0
};

static const short _ironbee_config_eof_trans[] = {
	0, 1, 3, 0, 0, 0, 15, 0,
	0, 0, 25, 0, 33, 0, 0, 0,
	41, 0, 0, 0, 0, 60, 62, 63,
	65, 66, 63, 63, 63, 63, 63, 63,
	0, 74, 74, 74, 0, 78, 78, 0,
	80, 80, 0, 83, 0, 85, 85, 85
};

static const int ironbee_config_start = 20;
static const int ironbee_config_first_final = 20;
static const int ironbee_config_error = 0;

static const int ironbee_config_en_parameters = 32;
static const int ironbee_config_en_block_parameters = 36;
static const int ironbee_config_en_newblock = 39;
static const int ironbee_config_en_endblock = 42;
static const int ironbee_config_en_includeconfig = 44;
static const int ironbee_config_en_main = 20;


#line 264 "config-parser.rl"

ib_status_t ib_cfgparser_ragel_parse_chunk(ib_cfgparser_t *cp,
                                           const char *buf,
                                           const size_t blen,
                                           const char *file,
                                           const ib_num_t lineno,
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
    char *dirname = NULL;

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

#line 317 "config-parser.rl"

#line 318 "config-parser.rl"

#line 319 "config-parser.rl"

#line 320 "config-parser.rl"


#line 374 "config-parser.c"
	{
	 fsm.cs = ironbee_config_start;
	 fsm.top = 0;
	 fsm.ts = 0;
	 fsm.te = 0;
	 fsm.act = 0;
	}

#line 322 "config-parser.rl"

#line 385 "config-parser.c"
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
#line 406 "config-parser.c"
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
#line 103 "config-parser.rl"
	{ mark = ( fsm.p); }
	break;
	case 1:
#line 104 "config-parser.rl"
	{
        rc = IB_EOTHER;
        ib_log_debug(ib_engine,
                     "ERROR: parser error before \"%.*s\"",
                     (int)(( fsm.p) - mark), mark);
    }
	break;
	case 2:
#line 112 "config-parser.rl"
	{
        pval = alloc_cpy_marked_string(mark, ( fsm.p), mpcfg);
        ib_list_push(plist, pval);
    }
	break;
	case 3:
#line 116 "config-parser.rl"
	{
        pval = alloc_cpy_marked_string(mark, ( fsm.p), mpcfg);
        ib_list_push(plist, pval);
    }
	break;
	case 4:
#line 122 "config-parser.rl"
	{
        size_t namelen = (size_t)(( fsm.p) - mark);
        dirname = (char *)calloc(namelen + 1, sizeof(*dirname));
        memcpy(dirname, mark, namelen);
        ib_list_clear(plist);
    }
	break;
	case 5:
#line 128 "config-parser.rl"
	{
        rc = ib_config_directive_process(cp, file, lineno, dirname, plist);
        if (rc != IB_OK) {
            ib_log_error(ib_engine,
                         "Failed to process directive \"%s\" on line %d of %s: %s",
                         dirname, lineno, file, ib_status_to_string(rc));
        }
        if (dirname != NULL) {
            free(dirname);
        }
    }
	break;
	case 6:
#line 141 "config-parser.rl"
	{
        size_t namelen = (size_t)(( fsm.p) - mark);
        blkname = (char *)calloc(namelen + 1, sizeof(*blkname));
        memcpy(blkname, mark, namelen);
        ib_list_clear(plist);
    }
	break;
	case 7:
#line 147 "config-parser.rl"
	{
        rc = ib_config_block_start(cp, file, lineno, blkname, plist);
        if (rc != IB_OK) {
            ib_log_error(ib_engine,
                         "Failed to start block \"%s\" on line %d of %s: %s",
                         blkname, file, lineno, ib_status_to_string(rc));
        }
    }
	break;
	case 8:
#line 155 "config-parser.rl"
	{
        blkname = (char *)cp->cur_blkname;
        rc = ib_config_block_process(cp, file, lineno, blkname);
        if (rc != IB_OK) {
            ib_log_error(ib_engine,
                         "Failed to process block \"%s\" on line %d of %s: %s",
                         blkname, lineno, file, ib_status_to_string(rc));
        }
        if (blkname != NULL) {
            free(blkname);
        }
        blkname = (char *)cp->cur_blkname;
    }
	break;
	case 9:
#line 170 "config-parser.rl"
	{
        struct stat statbuf;
    	int statval;
        int error = 0;

        pval = alloc_cpy_marked_string(mark, ( fsm.p), mpcfg);

	if (access(pval, R_OK) != 0) {
            ib_log_error(ib_engine, "Can't access included file \"%s\": %s",
                         pval, strerror(errno));
            error = 1;
            goto include_error;
	}

        statval = stat(pval, &statbuf);
        if (statval != 0) {
             ib_log_error(ib_engine,
                          "Failed to stat include file \"%s\": %s",
                          pval, strerror(errno));
            error = 1;
            goto include_error;
        }

        if (S_ISREG(statbuf.st_mode) == 0) {
            ib_log_error(ib_engine, "Included file \"%s\" isn't a file", pval);
            error = 1;
            goto include_error;
        }

        ib_log_debug(ib_engine, "Include configuration '%s'\n", pval);
        rc = ib_cfgparser_parse(cp, pval);
        if (rc != IB_OK) {
            ib_log_error(ib_engine, "Error parsing included file \"%s\": %s",
                         pval, ib_status_to_string(rc));
            error = 1;
            goto include_error;
        }
        include_error:
            if (error == 0) {
                ib_log_debug(ib_engine, "Done processing include file \"%s\"", pval);
            }
    }
	break;
	case 13:
#line 1 "NONE"
	{ fsm.te = ( fsm.p)+1;}
	break;
	case 14:
#line 228 "config-parser.rl"
	{ fsm.act = 1;}
	break;
	case 15:
#line 229 "config-parser.rl"
	{ fsm.te = ( fsm.p)+1;{ { fsm.cs =  fsm.stack[-- fsm.top]; goto _again;} }}
	break;
	case 16:
#line 228 "config-parser.rl"
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
#line 233 "config-parser.rl"
	{ fsm.act = 3;}
	break;
	case 19:
#line 234 "config-parser.rl"
	{ fsm.te = ( fsm.p)+1;{ { fsm.cs =  fsm.stack[-- fsm.top]; goto _again;} }}
	break;
	case 20:
#line 233 "config-parser.rl"
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
#line 238 "config-parser.rl"
	{ fsm.act = 5;}
	break;
	case 23:
#line 239 "config-parser.rl"
	{ fsm.te = ( fsm.p)+1;{ { fsm.cs =  fsm.stack[-- fsm.top]; goto _again;} }}
	break;
	case 24:
#line 238 "config-parser.rl"
	{ fsm.te = ( fsm.p);( fsm.p)--;{ { fsm.stack[ fsm.top++] =  fsm.cs;  fsm.cs = 36; goto _again;} }}
	break;
	case 25:
#line 1 "NONE"
	{	switch(  fsm.act ) {
	case 0:
	{{ fsm.cs = 0; goto _again;}}
	break;
	case 5:
	{{( fsm.p) = (( fsm.te))-1;} { fsm.stack[ fsm.top++] =  fsm.cs;  fsm.cs = 36; goto _again;} }
	break;
	}
	}
	break;
	case 26:
#line 243 "config-parser.rl"
	{ fsm.act = 7;}
	break;
	case 27:
#line 244 "config-parser.rl"
	{ fsm.te = ( fsm.p)+1;{ { fsm.cs =  fsm.stack[-- fsm.top]; goto _again;} }}
	break;
	case 28:
#line 243 "config-parser.rl"
	{ fsm.te = ( fsm.p);( fsm.p)--;}
	break;
	case 29:
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
	case 30:
#line 249 "config-parser.rl"
	{ fsm.act = 10;}
	break;
	case 31:
#line 248 "config-parser.rl"
	{ fsm.te = ( fsm.p)+1;}
	break;
	case 32:
#line 249 "config-parser.rl"
	{ fsm.te = ( fsm.p);( fsm.p)--;}
	break;
	case 33:
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
	case 34:
#line 254 "config-parser.rl"
	{ fsm.act = 12;}
	break;
	case 35:
#line 255 "config-parser.rl"
	{ fsm.act = 13;}
	break;
	case 36:
#line 258 "config-parser.rl"
	{ fsm.act = 16;}
	break;
	case 37:
#line 257 "config-parser.rl"
	{ fsm.te = ( fsm.p)+1;{ { fsm.stack[ fsm.top++] =  fsm.cs;  fsm.cs = 42; goto _again;} }}
	break;
	case 38:
#line 259 "config-parser.rl"
	{ fsm.te = ( fsm.p)+1;}
	break;
	case 39:
#line 253 "config-parser.rl"
	{ fsm.te = ( fsm.p);( fsm.p)--;}
	break;
	case 40:
#line 255 "config-parser.rl"
	{ fsm.te = ( fsm.p);( fsm.p)--;{ { fsm.stack[ fsm.top++] =  fsm.cs;  fsm.cs = 32; goto _again;} }}
	break;
	case 41:
#line 256 "config-parser.rl"
	{ fsm.te = ( fsm.p);( fsm.p)--;{ { fsm.stack[ fsm.top++] =  fsm.cs;  fsm.cs = 39; goto _again;} }}
	break;
	case 42:
#line 258 "config-parser.rl"
	{ fsm.te = ( fsm.p);( fsm.p)--;}
	break;
	case 43:
#line 1 "NONE"
	{	switch(  fsm.act ) {
	case 0:
	{{ fsm.cs = 0; goto _again;}}
	break;
	case 12:
	{{( fsm.p) = (( fsm.te))-1;} { fsm.stack[ fsm.top++] =  fsm.cs;  fsm.cs = 44; goto _again;} }
	break;
	case 13:
	{{( fsm.p) = (( fsm.te))-1;} { fsm.stack[ fsm.top++] =  fsm.cs;  fsm.cs = 32; goto _again;} }
	break;
	default:
	{{( fsm.p) = (( fsm.te))-1;}}
	break;
	}
	}
	break;
#line 781 "config-parser.c"
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
#line 798 "config-parser.c"
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
#line 104 "config-parser.rl"
	{
        rc = IB_EOTHER;
        ib_log_debug(ib_engine,
                     "ERROR: parser error before \"%.*s\"",
                     (int)(( fsm.p) - mark), mark);
    }
	break;
	case 2:
#line 112 "config-parser.rl"
	{
        pval = alloc_cpy_marked_string(mark, ( fsm.p), mpcfg);
        ib_list_push(plist, pval);
    }
	break;
	case 5:
#line 128 "config-parser.rl"
	{
        rc = ib_config_directive_process(cp, file, lineno, dirname, plist);
        if (rc != IB_OK) {
            ib_log_error(ib_engine,
                         "Failed to process directive \"%s\" on line %d of %s: %s",
                         dirname, lineno, file, ib_status_to_string(rc));
        }
        if (dirname != NULL) {
            free(dirname);
        }
    }
	break;
#line 847 "config-parser.c"
		}
	}
	}

	_out: {}
	}

#line 323 "config-parser.rl"

    /* Ensure that our block is always empty on last chunk. */
    if ( is_last_chunk && blkname != NULL ) {
        return IB_EINVAL;
    }

    return rc;
}
