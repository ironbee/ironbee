
#line 1 "../../ironbee/engine/config-parser.rl"
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

/* Caused by Ragel */
#ifdef __clang__
#pragma clang diagnostic ignored "-Wunreachable-code"
#endif

/**
 * Variables used by the finite state machine per-call.
 * Values here do not have to persist across calls to
 * ib_cfgparser_ragel_parse_chunk().
 */
typedef struct {
    const char *p;     /**< Pointer to the chunk being parsed. */
    const char *pe;    /**< Pointer past the end of p (p+length(p)). */
    const char *eof;   /**< eof==p==pe on last chunk. NULL otherwise. */
} fsm_vars_t;

/**
 * Append @a c to the internal buffer of @a cp. 
 * @param[in] cp Configuration parser.
 * @param[in] c Char to append.
 * @returns
 * - IB_OK
 * - IB_EALLOC if there is no space left in the buffer.
 */
static ib_status_t cpbuf_append(ib_cfgparser_t *cp, char c)
{
    assert(cp != NULL);
    assert(cp->buffer != NULL);
    assert (cp->buffer_sz >= cp->buffer_len);

    if (cp->buffer_sz == cp->buffer_len) {
        return IB_EALLOC;
    }

    cp->buffer[cp->buffer_len] = c;
    ++(cp->buffer_len);

    return IB_OK;
}

/**
 * Clear the buffer in @a cp.
 * @param[in] cp Configuration parser.
 */
static void cpbuf_clear(ib_cfgparser_t *cp) {
    assert(cp != NULL);
    assert(cp->buffer != NULL);

    cp->buffer_len = 0;
    cp->buffer[0] = '\0';
}

/**
 * Using the given mp, strdup the buffer in @a cp.
 *
 * If the buffer starts and ends with double quotes, remove them.
 *
 * @param[in] cp The configuration parser
 * @param[in,out] mp Pool to copy out of.
 *
 * @return a buffer allocated from the tmpmp memory pool
 *         available in ib_cfgparser_ragel_parse_chunk. This buffer may be
 *         larger than the string stored in it if the length of the string is
 *         reduced by Javascript unescaping.
 */
static char* qstrdup(ib_cfgparser_t *cp, ib_mpool_t* mp)
{
    const char *start = cp->buffer;
    const char *end = cp->buffer + cp->buffer_len - 1;
    size_t len = cp->buffer_len;

    /* Adjust for quoted value. */
    if ((*start == '"') && (*end == '"') && (start < end))
    {
        start++;
        len -= 2;
    }

    return ib_mpool_memdup_to_str(mp, start, len);
}

static ib_status_t include_config_fn(ib_cfgparser_t *cp,
                                     ib_mpool_t* mp,
                                     const char *directive)
{
    struct stat statbuf;
    ib_status_t rc;
    int statval;
    char *incfile;
    char *pval;
    char *real;
    char *lookup;
    const char *file = cp->curr->file;
    void *freeme = NULL;
    bool if_exists = strcasecmp("IncludeIfExists", directive) ? false : true;

    pval = qstrdup(cp, mp);
    if (pval == NULL) {
        return IB_EALLOC;
    }

    incfile = ib_util_relative_file(mp, file, pval);
    if (incfile == NULL) {
        ib_cfg_log_error(cp, "Failed to resolve included file \"%s\": %s",
                         file, strerror(errno));
        return IB_ENOENT;
    }

    real = realpath(incfile, NULL);
    if (real == NULL) {
        if (!if_exists) {
            ib_cfg_log_error(cp,
                             "Failed to find real path of included file "
                             "(using original \"%s\"): %s",
                             incfile, strerror(errno));
        }

        real = incfile;
    }
    else {
        if (strcmp(real, incfile) != 0) {
            ib_cfg_log_info(cp,
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
        if (if_exists) {
            ib_cfg_log(cp, (errno == ENOENT) ? IB_LOG_DEBUG : IB_LOG_NOTICE,
                       "Ignoring include file \"%s\": %s",
                       incfile, strerror(errno));
            return IB_OK;
        }

        ib_cfg_log_error(cp, "Cannot access included file \"%s\": %s",
                         incfile, strerror(errno));
        return IB_ENOENT;
    }

    statval = stat(incfile, &statbuf);
    if (statval != 0) {
        if (if_exists) {
            ib_cfg_log(cp, (errno == ENOENT) ? IB_LOG_DEBUG : IB_LOG_NOTICE,
                       "Ignoring include file \"%s\": %s",
                       incfile, strerror(errno));
            return IB_OK;
        }

        ib_cfg_log_error(cp,
                         "Failed to stat include file \"%s\": %s",
                         incfile, strerror(errno));
        return IB_ENOENT;
    }

    if (S_ISREG(statbuf.st_mode) == 0) {
        if (if_exists) {
            ib_cfg_log_info(cp,
                            "Ignoring include file \"%s\": Not a regular file",
                            incfile);
            return IB_OK;
        }

        ib_cfg_log_error(cp,
	                 "Included file \"%s\" is not a regular file", incfile);
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


#line 482 "../../ironbee/engine/config-parser.rl"



#line 263 "../../ironbee/engine/config-parser.c"
static const char _ironbee_config_actions[] = {
	0, 1, 0, 1, 3, 1, 4, 1, 
	6, 1, 12, 1, 13, 1, 17, 1, 
	24, 1, 28, 1, 31, 1, 39, 1, 
	47, 1, 48, 1, 49, 1, 52, 1, 
	55, 1, 56, 1, 57, 2, 0, 36, 
	2, 0, 43, 2, 0, 44, 2, 0, 
	56, 2, 0, 57, 2, 1, 5, 2, 
	1, 20, 2, 1, 21, 2, 2, 27, 
	2, 2, 28, 2, 3, 33, 2, 3, 
	41, 2, 3, 51, 2, 4, 3, 2, 
	4, 6, 2, 4, 53, 2, 6, 3, 
	2, 6, 7, 2, 7, 6, 2, 8, 
	34, 2, 8, 35, 2, 9, 26, 2, 
	10, 3, 2, 10, 42, 2, 10, 44, 
	2, 11, 54, 2, 14, 6, 3, 1, 
	5, 20, 3, 1, 5, 21, 3, 3, 
	5, 19, 3, 3, 7, 6, 3, 3, 
	18, 1, 3, 3, 25, 2, 3, 3, 
	32, 8, 3, 3, 33, 8, 3, 3, 
	40, 10, 3, 4, 3, 50, 3, 4, 
	3, 51, 3, 4, 7, 6, 3, 4, 
	11, 53, 3, 6, 3, 7, 3, 7, 
	6, 3, 3, 14, 6, 3, 3, 14, 
	6, 11, 3, 14, 6, 16, 3, 14, 
	6, 23, 3, 14, 6, 30, 3, 14, 
	6, 38, 3, 14, 6, 45, 3, 14, 
	7, 6, 4, 3, 5, 19, 1, 4, 
	14, 3, 6, 16, 4, 14, 3, 6, 
	23, 4, 14, 3, 6, 30, 4, 14, 
	3, 6, 38, 4, 14, 3, 6, 46, 
	4, 14, 3, 15, 6, 4, 14, 3, 
	22, 6, 4, 14, 3, 29, 6, 4, 
	14, 3, 37, 6, 4, 14, 4, 7, 
	6, 4, 14, 6, 45, 3, 4, 14, 
	7, 6, 3, 4, 14, 7, 6, 16, 
	4, 14, 7, 6, 23, 4, 14, 7, 
	6, 30, 4, 14, 7, 6, 38, 5, 
	14, 3, 7, 6, 16, 5, 14, 3, 
	7, 6, 30, 5, 14, 3, 7, 6, 
	46, 5, 14, 7, 6, 38, 3
};

static const short _ironbee_config_key_offsets[] = {
	0, 0, 0, 1, 6, 8, 10, 12, 
	12, 14, 14, 16, 16, 18, 18, 20, 
	20, 22, 22, 24, 25, 27, 38, 45, 
	52, 53, 62, 71, 80, 89, 98, 107, 
	116, 119, 123, 123, 127, 130, 139, 148, 
	157, 166, 175, 184, 193, 200, 207, 214, 
	217, 221, 228, 235, 242, 251, 258, 265, 
	265, 272, 280, 287, 287, 294, 303, 310, 
	317, 324, 331, 340, 347, 347, 354
};

static const char _ironbee_config_trans_keys[] = {
	47, 9, 10, 32, 34, 92, 10, 34, 
	10, 13, 10, 13, 34, 92, 10, 13, 
	34, 92, 10, 13, 10, 13, 10, 13, 
	10, 10, 13, 9, 10, 13, 32, 34, 
	35, 60, 62, 73, 92, 105, 32, 34, 
	60, 62, 92, 9, 10, 9, 10, 32, 
	34, 60, 62, 92, 10, 32, 34, 60, 
	62, 78, 92, 110, 9, 10, 32, 34, 
	60, 62, 67, 92, 99, 9, 10, 32, 
	34, 60, 62, 76, 92, 108, 9, 10, 
	32, 34, 60, 62, 85, 92, 117, 9, 
	10, 32, 34, 60, 62, 68, 92, 100, 
	9, 10, 32, 34, 60, 62, 69, 92, 
	101, 9, 10, 9, 10, 32, 34, 60, 
	62, 73, 92, 105, 32, 9, 10, 9, 
	10, 32, 34, 9, 10, 13, 32, 9, 
	10, 32, 32, 34, 60, 62, 70, 92, 
	102, 9, 10, 32, 34, 60, 62, 69, 
	92, 101, 9, 10, 32, 34, 60, 62, 
	88, 92, 120, 9, 10, 32, 34, 60, 
	62, 73, 92, 105, 9, 10, 32, 34, 
	60, 62, 83, 92, 115, 9, 10, 32, 
	34, 60, 62, 84, 92, 116, 9, 10, 
	32, 34, 60, 62, 83, 92, 115, 9, 
	10, 9, 10, 32, 34, 60, 62, 92, 
	9, 10, 32, 34, 60, 62, 92, 32, 
	34, 60, 62, 92, 9, 10, 32, 9, 
	10, 9, 10, 13, 32, 9, 10, 32, 
	34, 60, 62, 92, 9, 10, 32, 34, 
	60, 62, 92, 9, 10, 32, 34, 60, 
	62, 92, 9, 10, 13, 32, 34, 35, 
	60, 62, 92, 32, 34, 60, 62, 92, 
	9, 10, 9, 10, 32, 34, 60, 62, 
	92, 9, 10, 32, 34, 60, 62, 92, 
	9, 10, 32, 34, 35, 60, 62, 92, 
	32, 34, 60, 62, 92, 9, 10, 9, 
	10, 32, 34, 60, 62, 92, 9, 10, 
	13, 32, 60, 62, 92, 34, 35, 32, 
	34, 60, 62, 92, 9, 10, 9, 10, 
	32, 34, 60, 62, 92, 32, 34, 60, 
	62, 92, 9, 10, 9, 10, 32, 34, 
	60, 62, 92, 9, 10, 13, 32, 60, 
	62, 92, 34, 35, 32, 34, 60, 62, 
	92, 9, 10, 9, 10, 32, 34, 60, 
	62, 92, 9, 10, 32, 34, 60, 62, 
	92, 0
};

static const char _ironbee_config_single_lengths[] = {
	0, 0, 1, 5, 2, 2, 2, 0, 
	2, 0, 2, 0, 2, 0, 2, 0, 
	2, 0, 2, 1, 2, 11, 5, 7, 
	1, 7, 7, 7, 7, 7, 7, 9, 
	1, 4, 0, 4, 3, 7, 7, 7, 
	7, 7, 7, 7, 7, 7, 5, 1, 
	4, 7, 7, 7, 9, 5, 7, 0, 
	7, 8, 5, 0, 7, 7, 5, 7, 
	5, 7, 7, 5, 0, 7, 7
};

static const char _ironbee_config_range_lengths[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 1, 0, 
	0, 1, 1, 1, 1, 1, 1, 0, 
	1, 0, 0, 0, 0, 1, 1, 1, 
	1, 1, 1, 1, 0, 0, 1, 1, 
	0, 0, 0, 0, 0, 1, 0, 0, 
	0, 0, 1, 0, 0, 1, 1, 0, 
	1, 0, 1, 1, 0, 0, 0
};

static const short _ironbee_config_index_offsets[] = {
	0, 0, 1, 3, 9, 12, 15, 18, 
	19, 22, 23, 26, 27, 30, 31, 34, 
	35, 38, 39, 42, 44, 47, 59, 66, 
	74, 76, 85, 94, 103, 112, 121, 130, 
	140, 143, 148, 149, 154, 158, 167, 176, 
	185, 194, 203, 212, 221, 229, 237, 244, 
	247, 252, 260, 268, 276, 286, 293, 301, 
	302, 310, 319, 326, 327, 335, 344, 351, 
	359, 366, 374, 383, 390, 391, 399
};

static const unsigned char _ironbee_config_indicies[] = {
	1, 3, 2, 6, 7, 6, 8, 9, 
	5, 12, 13, 11, 14, 15, 1, 16, 
	17, 1, 19, 21, 22, 20, 20, 23, 
	24, 19, 26, 28, 29, 27, 27, 30, 
	31, 26, 33, 34, 35, 33, 37, 39, 
	40, 38, 39, 38, 41, 42, 37, 44, 
	45, 46, 44, 48, 47, 49, 48, 50, 
	51, 50, 43, 52, 52, 52, 52, 53, 
	52, 1, 52, 54, 52, 52, 52, 52, 
	53, 1, 55, 47, 52, 52, 52, 52, 
	56, 53, 56, 52, 1, 52, 52, 52, 
	52, 57, 53, 57, 52, 1, 52, 52, 
	52, 52, 58, 53, 58, 52, 1, 52, 
	52, 52, 52, 59, 53, 59, 52, 1, 
	52, 52, 52, 52, 60, 53, 60, 52, 
	1, 52, 52, 52, 52, 61, 53, 61, 
	52, 1, 62, 52, 62, 52, 52, 52, 
	63, 64, 63, 1, 65, 65, 66, 11, 
	65, 11, 66, 67, 65, 65, 68, 69, 
	65, 66, 65, 68, 65, 66, 52, 52, 
	52, 52, 70, 53, 70, 52, 1, 52, 
	52, 52, 52, 71, 53, 71, 52, 1, 
	52, 52, 52, 52, 72, 53, 72, 52, 
	1, 52, 52, 52, 52, 73, 53, 73, 
	52, 1, 52, 52, 52, 52, 74, 53, 
	74, 52, 1, 52, 52, 52, 52, 75, 
	53, 75, 52, 1, 52, 52, 52, 52, 
	76, 53, 76, 52, 1, 62, 52, 62, 
	52, 52, 52, 64, 1, 62, 52, 62, 
	78, 79, 79, 80, 77, 81, 83, 83, 
	83, 84, 81, 82, 85, 85, 82, 85, 
	14, 86, 85, 82, 81, 87, 81, 83, 
	83, 83, 84, 82, 52, 87, 52, 52, 
	52, 52, 53, 1, 52, 88, 52, 52, 
	52, 52, 53, 1, 90, 91, 92, 90, 
	93, 48, 48, 48, 94, 89, 95, 95, 
	95, 95, 96, 95, 19, 98, 99, 98, 
	98, 98, 98, 96, 19, 98, 98, 100, 
	98, 98, 98, 98, 96, 19, 102, 48, 
	102, 103, 48, 48, 104, 105, 101, 106, 
	106, 106, 106, 107, 106, 26, 108, 108, 
	109, 108, 108, 108, 108, 107, 26, 111, 
	112, 113, 111, 38, 38, 114, 38, 110, 
	115, 115, 115, 115, 116, 115, 33, 115, 
	117, 115, 115, 115, 115, 116, 33, 118, 
	118, 118, 118, 116, 118, 33, 115, 119, 
	115, 115, 115, 115, 116, 33, 121, 122, 
	123, 121, 38, 124, 125, 38, 120, 126, 
	126, 126, 126, 127, 126, 37, 128, 129, 
	130, 129, 129, 129, 129, 127, 37, 129, 
	131, 129, 129, 129, 129, 127, 37, 0
};

static const char _ironbee_config_trans_targs[] = {
	21, 22, 21, 21, 21, 32, 3, 21, 
	33, 35, 21, 4, 21, 34, 45, 50, 
	22, 51, 52, 53, 8, 55, 9, 53, 
	56, 57, 58, 12, 59, 13, 58, 60, 
	61, 62, 64, 65, 66, 67, 0, 66, 
	19, 67, 70, 22, 21, 21, 23, 24, 
	0, 2, 25, 6, 21, 1, 21, 21, 
	26, 27, 28, 29, 30, 31, 3, 37, 
	5, 21, 32, 33, 3, 36, 38, 39, 
	40, 41, 42, 43, 44, 46, 33, 32, 
	48, 21, 46, 32, 47, 22, 49, 3, 
	21, 53, 52, 52, 54, 8, 10, 52, 
	7, 52, 52, 52, 52, 58, 57, 12, 
	57, 14, 57, 11, 57, 57, 62, 61, 
	61, 63, 16, 61, 15, 61, 61, 61, 
	67, 66, 68, 69, 18, 20, 66, 17, 
	66, 66, 68, 66
};

static const short _ironbee_config_trans_actions[] = {
	31, 115, 23, 25, 49, 91, 0, 35, 
	206, 307, 46, 7, 33, 7, 265, 265, 
	178, 178, 122, 186, 7, 7, 7, 240, 
	215, 17, 190, 7, 7, 7, 245, 220, 
	37, 194, 250, 225, 43, 198, 1, 70, 
	3, 255, 230, 206, 27, 73, 270, 0, 
	0, 0, 206, 174, 82, 7, 158, 29, 
	115, 115, 115, 115, 115, 202, 5, 115, 
	85, 112, 7, 115, 3, 235, 115, 115, 
	115, 115, 115, 115, 202, 88, 260, 162, 
	170, 166, 7, 79, 7, 182, 265, 76, 
	154, 275, 13, 126, 295, 91, 130, 58, 
	7, 118, 55, 210, 134, 280, 15, 91, 
	100, 130, 64, 7, 61, 138, 285, 19, 
	67, 301, 130, 97, 7, 146, 94, 142, 
	290, 21, 3, 313, 0, 130, 109, 7, 
	40, 106, 103, 150
};

static const short _ironbee_config_to_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 9, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 9, 0, 0, 0, 
	0, 9, 0, 0, 0, 9, 0, 0, 
	0, 0, 9, 0, 0, 0, 0
};

static const short _ironbee_config_from_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 11, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 11, 0, 0, 0, 
	0, 11, 0, 0, 0, 11, 0, 0, 
	0, 0, 11, 0, 0, 0, 0
};

static const short _ironbee_config_eof_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	52, 52, 52, 0, 0, 0, 0, 0, 
	1, 0, 1, 1, 1, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 52, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0
};

static const short _ironbee_config_eof_trans[] = {
	0, 1, 0, 5, 11, 1, 0, 19, 
	0, 0, 0, 26, 0, 0, 0, 33, 
	0, 37, 0, 0, 0, 0, 53, 53, 
	56, 53, 53, 53, 53, 53, 53, 53, 
	66, 66, 66, 66, 66, 53, 53, 53, 
	53, 53, 53, 53, 53, 53, 82, 66, 
	66, 82, 53, 53, 0, 19, 98, 98, 
	98, 0, 107, 109, 109, 0, 116, 116, 
	119, 116, 0, 127, 129, 130, 130
};

static const int ironbee_config_start = 21;
static const int ironbee_config_first_final = 21;
static const int ironbee_config_error = 0;

static const int ironbee_config_en_parameters = 52;
static const int ironbee_config_en_block_parameters = 57;
static const int ironbee_config_en_newblock = 61;
static const int ironbee_config_en_endblock = 66;
static const int ironbee_config_en_main = 21;


#line 485 "../../ironbee/engine/config-parser.rl"

ib_status_t ib_cfgparser_ragel_parse_chunk(ib_cfgparser_t *cp,
                                           const char *buf,
                                           const size_t blen,
                                           const int is_last_chunk)
{
    assert(cp != NULL);
    assert(cp->ib != NULL);

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

    /* Temporary list for storing values before they are committed to the
     * configuration. */
    ib_list_t *plist;

    /* Create a finite state machine type. */
    fsm_vars_t fsm_vars;

    fsm_vars.p = buf;
    fsm_vars.pe = buf + blen;
    fsm_vars.eof = (is_last_chunk ? fsm_vars.pe : NULL);

    /* Create a temporary list for storing parameter values. */
    ib_list_create(&plist, mptmp);
    if (plist == NULL) {
        return IB_EALLOC;
    }

    /* Access all ragel state variables via structure. */
    
#line 533 "../../ironbee/engine/config-parser.rl"
    
#line 534 "../../ironbee/engine/config-parser.rl"
    
#line 535 "../../ironbee/engine/config-parser.rl"
    
#line 536 "../../ironbee/engine/config-parser.rl"

    
#line 615 "../../ironbee/engine/config-parser.c"
	{
	 cp->fsm.cs = ironbee_config_start;
	 cp->fsm.top = 0;
	 cp->fsm.ts = 0;
	 cp->fsm.te = 0;
	 cp->fsm.act = 0;
	}

#line 538 "../../ironbee/engine/config-parser.rl"
    
#line 626 "../../ironbee/engine/config-parser.c"
	{
	int _klen;
	unsigned int _trans;
	const char *_acts;
	unsigned int _nacts;
	const char *_keys;

	if ( ( fsm_vars.p) == ( fsm_vars.pe) )
		goto _test_eof;
	if (  cp->fsm.cs == 0 )
		goto _out;
_resume:
	_acts = _ironbee_config_actions + _ironbee_config_from_state_actions[ cp->fsm.cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 13:
#line 1 "NONE"
	{ cp->fsm.ts = ( fsm_vars.p);}
	break;
#line 647 "../../ironbee/engine/config-parser.c"
		}
	}

	_keys = _ironbee_config_trans_keys + _ironbee_config_key_offsets[ cp->fsm.cs];
	_trans = _ironbee_config_index_offsets[ cp->fsm.cs];

	_klen = _ironbee_config_single_lengths[ cp->fsm.cs];
	if ( _klen > 0 ) {
		const char *_lower = _keys;
		const char *_mid;
		const char *_upper = _keys + _klen - 1;
		while (1) {
			if ( _upper < _lower )
				break;

			_mid = _lower + ((_upper-_lower) >> 1);
			if ( (*( fsm_vars.p)) < *_mid )
				_upper = _mid - 1;
			else if ( (*( fsm_vars.p)) > *_mid )
				_lower = _mid + 1;
			else {
				_trans += (_mid - _keys);
				goto _match;
			}
		}
		_keys += _klen;
		_trans += _klen;
	}

	_klen = _ironbee_config_range_lengths[ cp->fsm.cs];
	if ( _klen > 0 ) {
		const char *_lower = _keys;
		const char *_mid;
		const char *_upper = _keys + (_klen<<1) - 2;
		while (1) {
			if ( _upper < _lower )
				break;

			_mid = _lower + (((_upper-_lower) >> 1) & ~1);
			if ( (*( fsm_vars.p)) < _mid[0] )
				_upper = _mid - 2;
			else if ( (*( fsm_vars.p)) > _mid[1] )
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
	 cp->fsm.cs = _ironbee_config_trans_targs[_trans];

	if ( _ironbee_config_trans_actions[_trans] == 0 )
		goto _again;

	_acts = _ironbee_config_actions + _ironbee_config_trans_actions[_trans];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 )
	{
		switch ( *_acts++ )
		{
	case 0:
#line 258 "../../ironbee/engine/config-parser.rl"
	{
        rc = IB_EOTHER;
        ib_cfg_log_error(
            cp,
            "parser error near %s:%zu.",
            cp->curr->file,
            cp->curr->line);
    }
	break;
	case 1:
#line 268 "../../ironbee/engine/config-parser.rl"
	{
        pval = qstrdup(cp, mpcfg);
        if (pval == NULL) {
            return IB_EALLOC;
        }
        ib_list_push(plist, pval);
    }
	break;
	case 2:
#line 275 "../../ironbee/engine/config-parser.rl"
	{
        pval = qstrdup(cp, mpcfg);
        if (pval == NULL) {
            return IB_EALLOC;
        }
        ib_list_push(plist, pval);
    }
	break;
	case 3:
#line 283 "../../ironbee/engine/config-parser.rl"
	{
        cp->curr->line += 1;
    }
	break;
	case 4:
#line 288 "../../ironbee/engine/config-parser.rl"
	{
        directive = ib_mpool_memdup_to_str(cp->mp, cp->buffer, cp->buffer_len);
        if (directive == NULL) {
            return IB_EALLOC;
        }
        ib_list_clear(plist);
        cpbuf_clear(cp);
    }
	break;
	case 5:
#line 296 "../../ironbee/engine/config-parser.rl"
	{
        ib_cfgparser_node_t *node = NULL;
        ib_cfgparser_node_create(&node, cp);
        if (rc != IB_OK) {
            ib_cfg_log_error(cp, "Cannot create node.");
            return rc;
        }
        node->directive = directive;
        directive = NULL;
        node->file = ib_mpool_strdup(cp->mp, cp->curr->file);
        if (node->file == NULL) {
            return IB_EALLOC;
        }
        node->line = cp->curr->line;
        node->type = IB_CFGPARSER_NODE_DIRECTIVE;
        ib_list_node_t *lst_node;
        IB_LIST_LOOP(plist, lst_node) {
            ib_list_push(node->params, ib_list_node_data(lst_node));
        }
        ib_list_push(cp->curr->children, node);
        if (rc != IB_OK) {
            ib_cfg_log_error(cp, "Out of memory.");
        }
    }
	break;
	case 6:
#line 321 "../../ironbee/engine/config-parser.rl"
	{
        if (cpbuf_append(cp, *( fsm_vars.p)) != IB_OK) {
            return IB_EALLOC;
        }
    }
	break;
	case 7:
#line 327 "../../ironbee/engine/config-parser.rl"
	{
        cpbuf_clear(cp);
    }
	break;
	case 8:
#line 332 "../../ironbee/engine/config-parser.rl"
	{
        blkname = ib_mpool_memdup_to_str(cp->mp, cp->buffer, cp->buffer_len);
        if (blkname == NULL) {
            return IB_EALLOC;
        }
        ib_list_clear(plist);
        cpbuf_clear(cp);
    }
	break;
	case 9:
#line 340 "../../ironbee/engine/config-parser.rl"
	{
        ib_cfgparser_node_t *node = NULL;
        rc = ib_cfgparser_node_create(&node, cp);
        if (rc != IB_OK) {
            ib_cfg_log_error(cp, "Cannot create node.");
            return rc;
        }
        node->directive = blkname;
        node->file = ib_mpool_strdup(cp->mp, cp->curr->file);
        if (node->file == NULL) {
            return IB_EALLOC;
        }
        node->line = cp->curr->line;
        node->type = IB_CFGPARSER_NODE_BLOCK;
        ib_list_node_t *lst_node;
        IB_LIST_LOOP(plist, lst_node) {
            rc = ib_list_push(node->params, ib_list_node_data(lst_node));
            if (rc != IB_OK) {
                ib_cfg_log_error(cp, "Cannot push directive.");
                return rc;
            }
        }
        rc = ib_cfgparser_push_node(cp, node);
        if (rc != IB_OK) {
            ib_cfg_log_error(cp, "Cannot push node.");
            return rc;
        }
    }
	break;
	case 10:
#line 368 "../../ironbee/engine/config-parser.rl"
	{
        ib_cfgparser_pop_node(cp);
        blkname = NULL;
    }
	break;
	case 11:
#line 374 "../../ironbee/engine/config-parser.rl"
	{
        rc = include_config_fn(cp, mpcfg, directive);

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
	case 14:
#line 1 "NONE"
	{ cp->fsm.te = ( fsm_vars.p)+1;}
	break;
	case 15:
#line 410 "../../ironbee/engine/config-parser.rl"
	{ cp->fsm.act = 2;}
	break;
	case 16:
#line 417 "../../ironbee/engine/config-parser.rl"
	{ cp->fsm.act = 4;}
	break;
	case 17:
#line 409 "../../ironbee/engine/config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;}
	break;
	case 18:
#line 410 "../../ironbee/engine/config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;}
	break;
	case 19:
#line 412 "../../ironbee/engine/config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;{ { cp->fsm.cs =  cp->fsm.stack[-- cp->fsm.top]; goto _again;} }}
	break;
	case 20:
#line 417 "../../ironbee/engine/config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p);( fsm_vars.p)--;}
	break;
	case 21:
#line 1 "NONE"
	{	switch(  cp->fsm.act ) {
	default:
	{{( fsm_vars.p) = (( cp->fsm.te))-1;}}
	break;
	}
	}
	break;
	case 22:
#line 422 "../../ironbee/engine/config-parser.rl"
	{ cp->fsm.act = 6;}
	break;
	case 23:
#line 425 "../../ironbee/engine/config-parser.rl"
	{ cp->fsm.act = 7;}
	break;
	case 24:
#line 421 "../../ironbee/engine/config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;}
	break;
	case 25:
#line 422 "../../ironbee/engine/config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;}
	break;
	case 26:
#line 427 "../../ironbee/engine/config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;{ { cp->fsm.cs =  cp->fsm.stack[-- cp->fsm.top]; goto _again;} }}
	break;
	case 27:
#line 425 "../../ironbee/engine/config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p);( fsm_vars.p)--;}
	break;
	case 28:
#line 1 "NONE"
	{	switch(  cp->fsm.act ) {
	default:
	{{( fsm_vars.p) = (( cp->fsm.te))-1;}}
	break;
	}
	}
	break;
	case 29:
#line 433 "../../ironbee/engine/config-parser.rl"
	{ cp->fsm.act = 10;}
	break;
	case 30:
#line 440 "../../ironbee/engine/config-parser.rl"
	{ cp->fsm.act = 12;}
	break;
	case 31:
#line 431 "../../ironbee/engine/config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;}
	break;
	case 32:
#line 433 "../../ironbee/engine/config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;{ { cp->fsm.cs =  cp->fsm.stack[-- cp->fsm.top]; goto _again;} }}
	break;
	case 33:
#line 435 "../../ironbee/engine/config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;{ { cp->fsm.cs =  cp->fsm.stack[-- cp->fsm.top]; goto _again;} }}
	break;
	case 34:
#line 433 "../../ironbee/engine/config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p);( fsm_vars.p)--;{ { cp->fsm.cs =  cp->fsm.stack[-- cp->fsm.top]; goto _again;} }}
	break;
	case 35:
#line 440 "../../ironbee/engine/config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p);( fsm_vars.p)--;{ { cp->fsm.stack[ cp->fsm.top++] =  cp->fsm.cs;  cp->fsm.cs = 57; goto _again;} }}
	break;
	case 36:
#line 1 "NONE"
	{	switch(  cp->fsm.act ) {
	case 10:
	{{( fsm_vars.p) = (( cp->fsm.te))-1;} { cp->fsm.cs =  cp->fsm.stack[-- cp->fsm.top]; goto _again;} }
	break;
	case 12:
	{{( fsm_vars.p) = (( cp->fsm.te))-1;} { cp->fsm.stack[ cp->fsm.top++] =  cp->fsm.cs;  cp->fsm.cs = 57; goto _again;} }
	break;
	}
	}
	break;
	case 37:
#line 445 "../../ironbee/engine/config-parser.rl"
	{ cp->fsm.act = 14;}
	break;
	case 38:
#line 449 "../../ironbee/engine/config-parser.rl"
	{ cp->fsm.act = 15;}
	break;
	case 39:
#line 444 "../../ironbee/engine/config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;}
	break;
	case 40:
#line 445 "../../ironbee/engine/config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;}
	break;
	case 41:
#line 453 "../../ironbee/engine/config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;{ { cp->fsm.cs =  cp->fsm.stack[-- cp->fsm.top]; goto _again;} }}
	break;
	case 42:
#line 449 "../../ironbee/engine/config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p);( fsm_vars.p)--;}
	break;
	case 43:
#line 451 "../../ironbee/engine/config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p);( fsm_vars.p)--;{ { cp->fsm.cs =  cp->fsm.stack[-- cp->fsm.top]; goto _again;} }}
	break;
	case 44:
#line 1 "NONE"
	{	switch(  cp->fsm.act ) {
	default:
	{{( fsm_vars.p) = (( cp->fsm.te))-1;}}
	break;
	}
	}
	break;
	case 45:
#line 463 "../../ironbee/engine/config-parser.rl"
	{ cp->fsm.act = 19;}
	break;
	case 46:
#line 471 "../../ironbee/engine/config-parser.rl"
	{ cp->fsm.act = 20;}
	break;
	case 47:
#line 474 "../../ironbee/engine/config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;{ ( fsm_vars.p)--; { cp->fsm.stack[ cp->fsm.top++] =  cp->fsm.cs;  cp->fsm.cs = 61; goto _again;}}}
	break;
	case 48:
#line 475 "../../ironbee/engine/config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;{        { cp->fsm.stack[ cp->fsm.top++] =  cp->fsm.cs;  cp->fsm.cs = 66; goto _again;}}}
	break;
	case 49:
#line 478 "../../ironbee/engine/config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;}
	break;
	case 50:
#line 479 "../../ironbee/engine/config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;}
	break;
	case 51:
#line 480 "../../ironbee/engine/config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p)+1;}
	break;
	case 52:
#line 457 "../../ironbee/engine/config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p);( fsm_vars.p)--;}
	break;
	case 53:
#line 463 "../../ironbee/engine/config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p);( fsm_vars.p)--;{ { cp->fsm.stack[ cp->fsm.top++] =  cp->fsm.cs;  cp->fsm.cs = 52; goto _again;} }}
	break;
	case 54:
#line 471 "../../ironbee/engine/config-parser.rl"
	{ cp->fsm.te = ( fsm_vars.p);( fsm_vars.p)--;}
	break;
	case 55:
#line 463 "../../ironbee/engine/config-parser.rl"
	{{( fsm_vars.p) = (( cp->fsm.te))-1;}{ { cp->fsm.stack[ cp->fsm.top++] =  cp->fsm.cs;  cp->fsm.cs = 52; goto _again;} }}
	break;
	case 56:
#line 471 "../../ironbee/engine/config-parser.rl"
	{{( fsm_vars.p) = (( cp->fsm.te))-1;}}
	break;
	case 57:
#line 1 "NONE"
	{	switch(  cp->fsm.act ) {
	case 19:
	{{( fsm_vars.p) = (( cp->fsm.te))-1;} { cp->fsm.stack[ cp->fsm.top++] =  cp->fsm.cs;  cp->fsm.cs = 52; goto _again;} }
	break;
	default:
	{{( fsm_vars.p) = (( cp->fsm.te))-1;}}
	break;
	}
	}
	break;
#line 1073 "../../ironbee/engine/config-parser.c"
		}
	}

_again:
	_acts = _ironbee_config_actions + _ironbee_config_to_state_actions[ cp->fsm.cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 12:
#line 1 "NONE"
	{ cp->fsm.ts = 0;}
	break;
#line 1086 "../../ironbee/engine/config-parser.c"
		}
	}

	if (  cp->fsm.cs == 0 )
		goto _out;
	if ( ++( fsm_vars.p) != ( fsm_vars.pe) )
		goto _resume;
	_test_eof: {}
	if ( ( fsm_vars.p) == ( fsm_vars.eof) )
	{
	if ( _ironbee_config_eof_trans[ cp->fsm.cs] > 0 ) {
		_trans = _ironbee_config_eof_trans[ cp->fsm.cs] - 1;
		goto _eof_trans;
	}
	const char *__acts = _ironbee_config_actions + _ironbee_config_eof_actions[ cp->fsm.cs];
	unsigned int __nacts = (unsigned int) *__acts++;
	while ( __nacts-- > 0 ) {
		switch ( *__acts++ ) {
	case 0:
#line 258 "../../ironbee/engine/config-parser.rl"
	{
        rc = IB_EOTHER;
        ib_cfg_log_error(
            cp,
            "parser error near %s:%zu.",
            cp->curr->file,
            cp->curr->line);
    }
	break;
	case 1:
#line 268 "../../ironbee/engine/config-parser.rl"
	{
        pval = qstrdup(cp, mpcfg);
        if (pval == NULL) {
            return IB_EALLOC;
        }
        ib_list_push(plist, pval);
    }
	break;
	case 5:
#line 296 "../../ironbee/engine/config-parser.rl"
	{
        ib_cfgparser_node_t *node = NULL;
        ib_cfgparser_node_create(&node, cp);
        if (rc != IB_OK) {
            ib_cfg_log_error(cp, "Cannot create node.");
            return rc;
        }
        node->directive = directive;
        directive = NULL;
        node->file = ib_mpool_strdup(cp->mp, cp->curr->file);
        if (node->file == NULL) {
            return IB_EALLOC;
        }
        node->line = cp->curr->line;
        node->type = IB_CFGPARSER_NODE_DIRECTIVE;
        ib_list_node_t *lst_node;
        IB_LIST_LOOP(plist, lst_node) {
            ib_list_push(node->params, ib_list_node_data(lst_node));
        }
        ib_list_push(cp->curr->children, node);
        if (rc != IB_OK) {
            ib_cfg_log_error(cp, "Out of memory.");
        }
    }
	break;
#line 1153 "../../ironbee/engine/config-parser.c"
		}
	}
	}

	_out: {}
	}

#line 539 "../../ironbee/engine/config-parser.rl"

    /* Ensure that our block is always empty on last chunk. */
    if ( is_last_chunk && blkname != NULL ) {
        return IB_EINVAL;
    }

    return rc;
}
