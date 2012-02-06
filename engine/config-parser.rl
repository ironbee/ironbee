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
 * @brief Malloc and unescpe into that buffer the marked string.
 * @param[in] fpc_mark the start of the string.
 * @param[in] fpc the current character from ragel.
 * @return a calloc'ed and realloc'ed buffer containing a string.
 */
static char* calloc_cpy_marked_string(char *fpc_mark, char *fpc) {
  char *afpc = fpc;
  size_t pvallen;
  /* Adjust for quoted value. */
  if ((*fpc_mark == '"') && (*(afpc-1) == '"') && (fpc_mark+1 < afpc-2)) {
      fpc_mark++;
      afpc--;
  }
  pvallen = (size_t)(afpc - fpc_mark);
  pval = (char *)malloc(pvallen + 1 * sizeof(*pval));

  ib_util_unescape_string(pval, &pvallen, fpc_mark, pvallen);

  /* Shrink the buffer appropriately. */
  pval = (char*)realloc(pval, pvallen+1);

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
        pval = calloc_cpy_marked_string(mark, fpc);
        ib_list_push(plist, pval);
    }
    action push_blkparam {
        pval = calloc_cpy_marked_string(mark, fpc);
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
            ib_log_error(ib, 1, "Failed to process directive \"%s\": %d", dirname, rc);
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
            ib_log_error(ib, 1, "Failed to start block \"%s\": %d", blkname, rc);
        }
    }
    action pop_block {
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
        WS* param >mark %push_param;
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
                                           uint8_t *buf,
                                           size_t blen)
{
    ib_engine_t *ib = cp->ib;
    ib_mpool_t *mptmp = ib_engine_pool_temp_get(ib);
    ib_status_t rc;
    ib_list_t *plist;
    /// @todo Which should be in cp???
    char *data = (char *)buf;
    fsm_t fsm;

    fsm.p = data;
    fsm.pe = fsm.p + blen;
    fsm.eof = 0;

    /* Init */
    mark = fsm.p;
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

    return IB_OK;
}

