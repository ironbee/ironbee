
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

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include <ironbee/engine.h>
#include <ironbee/util.h>
#include <ironbee/config.h>
#include <ironbee/mpool.h>

#include "config-parser.h"
#include "ironbee_private.h"

typedef struct {
    char    *p;
    char    *pe;
    char    *eof;
    char    *ts;
    char    *te;
    int      cs;
    int      top;
    int      act;
    int      stack[1024];
} fsm_t;

static char *dirname = NULL;
static char *blkname = NULL;
static char *pval = NULL;

/* Store the start of a string to act on.
   fpc - mark is the string length when processing after
   a mark action. */
static char *mark = NULL;

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
static char* alloc_cpy_marked_string(char *fpc_mark,
                                     char *fpc,
                                     ib_mpool_t* mp)
{
  char *afpc = fpc;
  size_t pvallen;
  /* Adjust for quoted value. */
  if ((*fpc_mark == '"') && (*(afpc-1) == '"') && (fpc_mark+1 < afpc-2)) {
      fpc_mark++;
      afpc--;
  }
  pvallen = (size_t)(afpc - fpc_mark);
  pval = (char *)ib_mpool_alloc(mp, (pvallen + 1) * sizeof(*pval));

  ib_util_unescape_string(pval, &pvallen, fpc_mark, pvallen);

  /* At this point the buffer i pvallen+1 in size, but we cannot shrink it. */
  /* This is not considered a problem for configuration parsing and it is
     deallocated after parsing and configuration is complete. */
  return pval;
}


#line 196 "config-parser.rl"



#line 101 "config-parser.c"
static const char _ironbee_config_actions[] = {
	0, 1, 0, 1, 10, 1, 19, 1, 
	21, 1, 23, 1, 25, 1, 27, 1, 
	30, 1, 31, 1, 32, 1, 34, 1, 
	35, 1, 36, 2, 1, 4, 2, 1, 
	14, 2, 2, 18, 2, 3, 31, 2, 
	3, 33, 2, 4, 13, 2, 5, 21, 
	2, 5, 22, 2, 6, 17, 2, 7, 
	26, 2, 8, 9, 2, 11, 12, 2, 
	11, 16, 2, 11, 20, 2, 11, 24, 
	2, 11, 28, 2, 11, 29, 3, 1, 
	4, 13, 3, 1, 4, 14, 3, 1, 
	4, 15, 3, 11, 0, 12, 3, 11, 
	0, 16, 3, 11, 0, 20, 3, 11, 
	0, 24, 3, 11, 0, 28
};

static const unsigned char _ironbee_config_key_offsets[] = {
	0, 0, 0, 0, 9, 11, 11, 11, 
	19, 21, 21, 21, 30, 30, 38, 42, 
	43, 52, 59, 68, 75, 76, 77, 86, 
	93, 100, 100, 108, 115, 115, 124, 131, 
	138, 146
};

static const char _ironbee_config_trans_keys[] = {
	9, 10, 13, 32, 34, 35, 60, 62, 
	92, 34, 92, 9, 10, 32, 34, 35, 
	60, 62, 92, 34, 92, 9, 10, 13, 
	32, 60, 62, 92, 34, 35, 9, 10, 
	32, 60, 62, 92, 34, 35, 9, 10, 
	13, 32, 10, 9, 10, 13, 32, 34, 
	35, 60, 62, 92, 32, 34, 60, 62, 
	92, 9, 10, 9, 10, 13, 32, 34, 
	35, 60, 62, 92, 9, 10, 32, 34, 
	60, 62, 92, 10, 47, 9, 10, 13, 
	32, 34, 35, 60, 62, 92, 32, 34, 
	60, 62, 92, 9, 10, 9, 10, 32, 
	34, 60, 62, 92, 9, 10, 32, 34, 
	35, 60, 62, 92, 32, 34, 60, 62, 
	92, 9, 10, 9, 10, 13, 32, 60, 
	62, 92, 34, 35, 32, 34, 60, 62, 
	92, 9, 10, 9, 10, 32, 34, 60, 
	62, 92, 9, 10, 32, 60, 62, 92, 
	34, 35, 32, 34, 60, 62, 92, 9, 
	10, 0
};

static const char _ironbee_config_single_lengths[] = {
	0, 0, 0, 9, 2, 0, 0, 8, 
	2, 0, 0, 7, 0, 6, 4, 1, 
	9, 5, 9, 7, 1, 1, 9, 5, 
	7, 0, 8, 5, 0, 7, 5, 7, 
	6, 5
};

static const char _ironbee_config_range_lengths[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 1, 0, 1, 0, 0, 
	0, 1, 0, 0, 0, 0, 0, 1, 
	0, 0, 0, 1, 0, 1, 1, 0, 
	1, 1
};

static const unsigned char _ironbee_config_index_offsets[] = {
	0, 0, 1, 2, 12, 15, 16, 17, 
	26, 29, 30, 31, 40, 41, 49, 54, 
	56, 66, 73, 83, 91, 93, 95, 105, 
	112, 120, 121, 130, 137, 138, 147, 154, 
	162, 170
};

static const char _ironbee_config_indicies[] = {
	1, 3, 5, 6, 7, 5, 8, 9, 
	9, 9, 10, 4, 12, 13, 11, 11, 
	15, 17, 9, 17, 18, 9, 9, 19, 
	20, 16, 22, 23, 21, 21, 25, 27, 
	28, 29, 27, 9, 9, 30, 9, 26, 
	32, 34, 9, 34, 9, 35, 36, 9, 
	33, 35, 37, 38, 35, 9, 37, 9, 
	40, 41, 42, 40, 9, 43, 44, 9, 
	45, 39, 46, 46, 46, 46, 47, 46, 
	1, 40, 41, 42, 40, 48, 43, 48, 
	48, 45, 39, 46, 49, 46, 46, 46, 
	46, 47, 1, 50, 43, 52, 51, 5, 
	6, 7, 5, 8, 9, 9, 9, 10, 
	4, 54, 54, 54, 54, 55, 54, 3, 
	54, 56, 54, 54, 54, 54, 55, 3, 
	54, 17, 9, 17, 18, 9, 9, 19, 
	20, 16, 57, 57, 57, 57, 58, 57, 
	15, 57, 27, 28, 29, 27, 9, 9, 
	30, 9, 26, 59, 59, 59, 59, 60, 
	59, 25, 59, 61, 59, 59, 59, 59, 
	60, 25, 34, 9, 34, 9, 35, 36, 
	9, 33, 62, 62, 62, 62, 63, 62, 
	32, 0
};

static const char _ironbee_config_trans_targs[] = {
	16, 17, 22, 23, 23, 3, 22, 24, 
	4, 0, 2, 4, 25, 5, 26, 27, 
	27, 7, 8, 26, 6, 8, 28, 9, 
	29, 30, 30, 11, 29, 31, 10, 32, 
	33, 33, 13, 14, 12, 32, 15, 17, 
	18, 16, 19, 20, 21, 1, 16, 1, 
	16, 16, 16, 16, 16, 22, 22, 2, 
	22, 26, 6, 29, 10, 29, 32, 12
};

static const char _ironbee_config_trans_actions[] = {
	25, 72, 86, 60, 90, 0, 42, 90, 
	1, 0, 1, 0, 0, 0, 5, 63, 
	94, 0, 1, 51, 1, 0, 0, 0, 
	9, 66, 98, 0, 7, 98, 1, 13, 
	69, 102, 0, 0, 1, 11, 0, 106, 
	75, 17, 106, 0, 0, 1, 39, 0, 
	23, 36, 19, 21, 15, 82, 30, 0, 
	78, 33, 0, 48, 0, 45, 54, 0
};

static const char _ironbee_config_to_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	57, 0, 0, 0, 0, 0, 57, 0, 
	0, 0, 57, 0, 0, 57, 0, 0, 
	57, 0
};

static const char _ironbee_config_from_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	3, 0, 0, 0, 0, 0, 3, 0, 
	0, 0, 3, 0, 0, 3, 0, 0, 
	3, 0
};

static const char _ironbee_config_eof_actions[] = {
	0, 0, 0, 27, 27, 27, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 27, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0
};

static const unsigned char _ironbee_config_eof_trans[] = {
	0, 1, 3, 0, 0, 0, 15, 0, 
	0, 0, 25, 0, 32, 0, 0, 0, 
	0, 47, 49, 47, 51, 52, 0, 54, 
	54, 54, 0, 58, 58, 0, 60, 60, 
	0, 63
};

static const int ironbee_config_start = 16;
static const int ironbee_config_first_final = 16;
static const int ironbee_config_error = 0;

static const int ironbee_config_en_parameters = 22;
static const int ironbee_config_en_block_parameters = 26;
static const int ironbee_config_en_newblock = 29;
static const int ironbee_config_en_endblock = 32;
static const int ironbee_config_en_main = 16;


#line 199 "config-parser.rl"

ib_status_t ib_cfgparser_ragel_parse_chunk(ib_cfgparser_t *cp,
                                           const char *buf,
                                           size_t blen,
                                           int is_last_block)
{
    ib_engine_t *ib = cp->ib;
    ib_mpool_t *mptmp = ib_engine_pool_temp_get(ib);
    ib_mpool_t *mpcfg = ib_engine_pool_config_get(ib);
    ib_status_t rc;
    ib_list_t *plist;
    /// @todo Which should be in cp???
    char *data = (char *)buf;
    fsm_t fsm;

    fsm.p = data;
    fsm.pe = data + blen;
    fsm.eof = (is_last_block? fsm.pe : NULL);

    /* Init */
    mark = fsm.p;
    memset(fsm.stack, 0, sizeof(fsm.stack));

    /* Create a temporary list for storing parameter values. */
    ib_list_create(&plist, mptmp);
    if (plist == NULL) {
        return IB_EALLOC;
    }

    /* Access all ragel state variables via structure. */
    
#line 230 "config-parser.rl"
    
#line 231 "config-parser.rl"
    
#line 232 "config-parser.rl"
    
#line 233 "config-parser.rl"

    
#line 306 "config-parser.c"
	{
	 fsm.cs = ironbee_config_start;
	 fsm.top = 0;
	 fsm.ts = 0;
	 fsm.te = 0;
	 fsm.act = 0;
	}

#line 235 "config-parser.rl"
    
#line 317 "config-parser.c"
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
	case 10:
#line 1 "NONE"
	{ fsm.ts = ( fsm.p);}
	break;
#line 338 "config-parser.c"
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
#line 97 "config-parser.rl"
	{ mark = ( fsm.p); }
	break;
	case 1:
#line 103 "config-parser.rl"
	{
        pval = alloc_cpy_marked_string(mark, ( fsm.p), mpcfg);
        ib_list_push(plist, pval);
    }
	break;
	case 2:
#line 107 "config-parser.rl"
	{
        pval = alloc_cpy_marked_string(mark, ( fsm.p), mpcfg);
        ib_list_push(plist, pval);
    }
	break;
	case 3:
#line 113 "config-parser.rl"
	{
        size_t namelen = (size_t)(( fsm.p) - mark);
        dirname = (char *)calloc(namelen + 1, sizeof(*dirname));
        memcpy(dirname, mark, namelen);
        ib_list_clear(plist);
    }
	break;
	case 4:
#line 119 "config-parser.rl"
	{
        rc = ib_config_directive_process(cp, dirname, plist);
        if (rc != IB_OK) {
            ib_log_error(ib, 1, "Failed to process directive \"%s\": %d", dirname, rc);
        }
        if (dirname != NULL) {
            free(dirname);
        }
    }
	break;
	case 5:
#line 130 "config-parser.rl"
	{
        size_t namelen = (size_t)(( fsm.p) - mark);
        blkname = (char *)calloc(namelen + 1, sizeof(*blkname));
        memcpy(blkname, mark, namelen);
        ib_list_clear(plist);
    }
	break;
	case 6:
#line 136 "config-parser.rl"
	{
        rc = ib_config_block_start(cp, blkname, plist);
        if (rc != IB_OK) {
            ib_log_error(ib, 1, "Failed to start block \"%s\": %d", blkname, rc);
        }
    }
	break;
	case 7:
#line 142 "config-parser.rl"
	{
        blkname = (char *)cp->cur_blkname;
        rc = ib_config_block_process(cp, blkname);
        if (rc != IB_OK) {
            ib_log_error(ib, 1, "Failed to process block \"%s\": %d", blkname, rc);
        }
        if (blkname != NULL) {
            free(blkname);
        }
        blkname = (char *)cp->cur_blkname;
    }
	break;
	case 11:
#line 1 "NONE"
	{ fsm.te = ( fsm.p)+1;}
	break;
	case 12:
#line 169 "config-parser.rl"
	{ fsm.act = 1;}
	break;
	case 13:
#line 170 "config-parser.rl"
	{ fsm.te = ( fsm.p)+1;{ { fsm.cs =  fsm.stack[-- fsm.top]; goto _again;} }}
	break;
	case 14:
#line 169 "config-parser.rl"
	{ fsm.te = ( fsm.p);( fsm.p)--;}
	break;
	case 15:
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
	case 16:
#line 174 "config-parser.rl"
	{ fsm.act = 3;}
	break;
	case 17:
#line 175 "config-parser.rl"
	{ fsm.te = ( fsm.p)+1;{ { fsm.cs =  fsm.stack[-- fsm.top]; goto _again;} }}
	break;
	case 18:
#line 174 "config-parser.rl"
	{ fsm.te = ( fsm.p);( fsm.p)--;}
	break;
	case 19:
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
	case 20:
#line 179 "config-parser.rl"
	{ fsm.act = 5;}
	break;
	case 21:
#line 180 "config-parser.rl"
	{ fsm.te = ( fsm.p)+1;{ { fsm.cs =  fsm.stack[-- fsm.top]; goto _again;} }}
	break;
	case 22:
#line 179 "config-parser.rl"
	{ fsm.te = ( fsm.p);( fsm.p)--;{ { fsm.stack[ fsm.top++] =  fsm.cs;  fsm.cs = 26; goto _again;} }}
	break;
	case 23:
#line 1 "NONE"
	{	switch(  fsm.act ) {
	case 0:
	{{ fsm.cs = 0; goto _again;}}
	break;
	case 5:
	{{( fsm.p) = (( fsm.te))-1;} { fsm.stack[ fsm.top++] =  fsm.cs;  fsm.cs = 26; goto _again;} }
	break;
	}
	}
	break;
	case 24:
#line 184 "config-parser.rl"
	{ fsm.act = 7;}
	break;
	case 25:
#line 185 "config-parser.rl"
	{ fsm.te = ( fsm.p)+1;{ { fsm.cs =  fsm.stack[-- fsm.top]; goto _again;} }}
	break;
	case 26:
#line 184 "config-parser.rl"
	{ fsm.te = ( fsm.p);( fsm.p)--;}
	break;
	case 27:
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
	case 28:
#line 190 "config-parser.rl"
	{ fsm.act = 10;}
	break;
	case 29:
#line 193 "config-parser.rl"
	{ fsm.act = 13;}
	break;
	case 30:
#line 191 "config-parser.rl"
	{ fsm.te = ( fsm.p)+1;{ { fsm.stack[ fsm.top++] =  fsm.cs;  fsm.cs = 32; goto _again;} }}
	break;
	case 31:
#line 194 "config-parser.rl"
	{ fsm.te = ( fsm.p)+1;}
	break;
	case 32:
#line 189 "config-parser.rl"
	{ fsm.te = ( fsm.p);( fsm.p)--;}
	break;
	case 33:
#line 190 "config-parser.rl"
	{ fsm.te = ( fsm.p);( fsm.p)--;{ { fsm.stack[ fsm.top++] =  fsm.cs;  fsm.cs = 22; goto _again;} }}
	break;
	case 34:
#line 192 "config-parser.rl"
	{ fsm.te = ( fsm.p);( fsm.p)--;{ { fsm.stack[ fsm.top++] =  fsm.cs;  fsm.cs = 29; goto _again;} }}
	break;
	case 35:
#line 193 "config-parser.rl"
	{ fsm.te = ( fsm.p);( fsm.p)--;}
	break;
	case 36:
#line 1 "NONE"
	{	switch(  fsm.act ) {
	case 0:
	{{ fsm.cs = 0; goto _again;}}
	break;
	case 10:
	{{( fsm.p) = (( fsm.te))-1;} { fsm.stack[ fsm.top++] =  fsm.cs;  fsm.cs = 22; goto _again;} }
	break;
	default:
	{{( fsm.p) = (( fsm.te))-1;}}
	break;
	}
	}
	break;
#line 622 "config-parser.c"
		}
	}

_again:
	_acts = _ironbee_config_actions + _ironbee_config_to_state_actions[ fsm.cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 8:
#line 1 "NONE"
	{ fsm.ts = 0;}
	break;
	case 9:
#line 1 "NONE"
	{ fsm.act = 0;}
	break;
#line 639 "config-parser.c"
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
#line 103 "config-parser.rl"
	{
        pval = alloc_cpy_marked_string(mark, ( fsm.p), mpcfg);
        ib_list_push(plist, pval);
    }
	break;
	case 4:
#line 119 "config-parser.rl"
	{
        rc = ib_config_directive_process(cp, dirname, plist);
        if (rc != IB_OK) {
            ib_log_error(ib, 1, "Failed to process directive \"%s\": %d", dirname, rc);
        }
        if (dirname != NULL) {
            free(dirname);
        }
    }
	break;
#line 677 "config-parser.c"
		}
	}
	}

	_out: {}
	}

#line 236 "config-parser.rl"

    return IB_OK;
}

