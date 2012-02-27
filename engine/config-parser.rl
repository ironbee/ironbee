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

/**
 * Finite state machine type.
 * @internal
 * Contains state information for Ragel's parser.
 * Many of these values and names come from the Ragel documentation, section 
 * 5.1 Variable Used by Ragel. p53 of The Ragel Guide 6.7 found at
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
  pval = (char *)ib_mpool_alloc(mp, (pvallen + 1) * sizeof(*pval));

  ib_util_unescape_string(pval, &pvallen, fpc_mark, pvallen);

  /* At this point the buffer i pvallen+1 in size, but we cannot shrink it. */
  /* This is not considered a problem for configuration parsing and it is
     deallocated after parsing and configuration is complete. */
  return pval;
}

%%{
    machine ironbee_config;

    # Mark the start of a string.
    action mark { mark = fpc; }
    action error {
        ib_log_debug(ib, 4, "ERROR: parser error before \"%.*s\"", (int)(fpc - mark), mark);
    }

    # Parameter
    action push_param {
        pval = alloc_cpy_marked_string(mark, fpc, mpcfg);
        ib_list_push(plist, pval);
    }
    action push_blkparam {
        pval = alloc_cpy_marked_string(mark, fpc, mpcfg);
        ib_list_push(plist, pval);
    }

    # Directives
    action start_dir {
        size_t namelen = (size_t)(fpc - mark);
        dirname = (char *)calloc(namelen + 1, sizeof(*dirname));
        memcpy(dirname, mark, namelen);
        ib_list_clear(plist);
    }
    action push_dir {
        rc = ib_config_directive_process(cp, dirname, plist);
        if (rc != IB_OK) {
            ib_log_error(ib_engine, 1, "Failed to process directive \"%s\": %d", dirname, rc);
        }
        if (dirname != NULL) {
            free(dirname);
        }
    }

    # Blocks
    action start_block {
        size_t namelen = (size_t)(fpc - mark);
        blkname = (char *)calloc(namelen + 1, sizeof(*blkname));
        memcpy(blkname, mark, namelen);
        ib_list_clear(plist);
    }
    action push_block {
        rc = ib_config_block_start(cp, blkname, plist);
        if (rc != IB_OK) {
            ib_log_error(ib_engine, 1, "Failed to start block \"%s\": %d", blkname, rc);
        }
    }
    action pop_block {
        blkname = (char *)cp->cur_blkname;
        rc = ib_config_block_process(cp, blkname);
        if (rc != IB_OK) {
            ib_log_error(ib_engine, 1, "Failed to process block \"%s\": %d", blkname, rc);
        }
        if (blkname != NULL) {
            free(blkname);
        }
        blkname = (char *)cp->cur_blkname;
    }

    WS = [ \t];
    EOLSEQ = '\r'? '\n';
    EOL = WS* EOLSEQ;
    CONT = '\\' EOL;

    sep = WS+;
    qchar = '\\' any;
    qtoken = '"' ( qchar | ( any - ["\\] ) )* '"';
    token = (qchar | (any - (WS | EOL | [<>#"\\]))) (qchar | (any - ( WS | EOL | [<>"\\])))*;
    param = qtoken | token;
    keyval = token '=' param;

    comment = '#' (any -- EOLSEQ)*;

    parameters := |*
        WS* param >mark %push_param $/push_param $/push_dir;
        EOL @push_dir { fret; };
    *|;

    block_parameters := |*
        WS* param >mark %push_blkparam;
        WS* ">" @push_block { fret; };
    *|;

    newblock := |*
        WS* token >mark %start_block { fcall block_parameters; };
        EOL { fret; };
    *|;

    endblock := |*
        WS* token >mark %pop_block;
        WS* ">" EOL { fret; };
    *|;

    main := |*
        WS* comment;
        WS* token >mark %start_dir { fcall parameters; };
        "</" { fcall endblock; };
        "<" { fcall newblock; };
        WS+;
        EOL;
    *|;
}%%

%% write data;

ib_status_t ib_cfgparser_ragel_parse_chunk(ib_cfgparser_t *cp,
                                           const char *buf,
                                           size_t blen,
                                           int is_last_chunk)
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
    %% access fsm.;
    %% variable p fsm.p;
    %% variable pe fsm.pe;
    %% variable eof fsm.eof;

    %% write init;
    %% write exec;

    return rc;
}

