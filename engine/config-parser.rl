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
                                     ib_num_t lineno)
{
    struct stat statbuf;
    ib_status_t rc;
    int statval;
    char *incfile;
    char *pval;

    pval = alloc_cpy_marked_string(mark, fpc, mp);

    if (*pval == '/') {
        incfile = (char *)ib_mpool_strdup(mp, pval);
        if (incfile == NULL) {
            ib_log_error(cp->ib,
                         "Error allocating include file name buffer");
            return IB_EALLOC;
        }
    }
    else { 
        char *ofile;         /* Copy of original file name */
        const char *pdir;    /* Parent directory */
        size_t len;

        ofile = (char *)ib_mpool_strdup(mp, file);
        if (ofile == NULL) {
            ib_log_error(cp->ib,
                         "Cannot allocate original file name buffer");
            return IB_EALLOC;
        }
        pdir = dirname(ofile);

        /* Allocate & generate the include file name */
        len = strlen(pdir);      /* Parent directory */
        len += 1;                /* slash */
        len += strlen(pval);     /* file name */
        len += 1;                /* NUL */
        incfile = (char *)ib_mpool_alloc(mp, len);
        if (incfile == NULL) {
            ib_log_error(cp->ib,
                         "Cannot allocate include file name buffer (%d)",
                         len);
            return IB_EALLOC;
        }
        strcpy(incfile, pdir);
        strcat(incfile, "/");
        strcat(incfile, pval);
    }

    if (access(incfile, R_OK) != 0) {
        ib_log_error(cp->ib, "Can't access included file \"%s\": %s",
                     incfile, strerror(errno));
        return IB_ENOENT;
    }

    statval = stat(incfile, &statbuf);
    if (statval != 0) {
        ib_log_error(cp->ib,
                     "Failed to stat include file \"%s\": %s",
                     incfile, strerror(errno));
        return IB_ENOENT;
    }

    if (S_ISREG(statbuf.st_mode) == 0) {
        ib_log_error(cp->ib,
                     "Included file \"%s\" isn't a file", incfile);
        return IB_ENOENT;
    }

    ib_log_debug(cp->ib, "Including '%s' from line %d of %s",
                 incfile, lineno, file);
    rc = ib_cfgparser_parse(cp, incfile);
    if (rc != IB_OK) {
        ib_log_error(cp->ib, "Error parsing included file \"%s\": %s",
                     incfile, ib_status_to_string(rc));
        return rc;
    }
    
    ib_log_debug(cp->ib, "Done processing include file \"%s\"", incfile);
    return IB_OK;
}

%%{
    machine ironbee_config;

    # Mark the start of a string.
    action mark { mark = fpc; }
    action error_action {
        rc = IB_EOTHER;
        ib_log_debug(ib_engine,
                     "ERROR: parser error before \"%.*s\" on line %d of %s",
                     (int)(fpc - mark), mark, lineno, file);
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
        directive = (char *)calloc(namelen + 1, sizeof(*directive));
        memcpy(directive, mark, namelen);
        ib_list_clear(plist);
    }
    action push_dir {
        rc = ib_config_directive_process(cp, file, lineno, directive, plist);
        if (rc != IB_OK) {
            ib_log_error(ib_engine,
                         "Failed to process directive \"%s\" on line %d of %s: %s",
                         directive, lineno, file, ib_status_to_string(rc));
        }
        if (directive != NULL) {
            free(directive);
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
        rc = ib_config_block_start(cp, file, lineno, blkname, plist);
        if (rc != IB_OK) {
            ib_log_error(ib_engine,
                         "Failed to start block \"%s\" on line %d of %s: %s",
                         blkname, file, lineno, ib_status_to_string(rc));
        }
    }
    action pop_block {
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

    # include file logic
    action include_config {
        rc = include_config_fn(cp, mpcfg, mark, fpc, file, lineno);
        if (rc == IB_OK) {
            ib_log_debug(ib_engine,
                         "Done processing include direction on line %d of %s",
                         lineno, file);
        }
        else {
            ib_log_error(ib_engine,
                         "Failed to process include directive "
                         "on line %d of %s: %s",
                         lineno, file, ib_status_to_string(rc));
        }
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
    iparam = ( '"' (any - (EOL | '"'))+ '"' ) | (any - (WS | EOL) )+;

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
        WS* token >mark %start_block $!error_action { fcall block_parameters; };
        EOL $!error_action { fret; };
    *|;

    endblock := |*
	WS* EOL %error_action { fret; };
        WS* token >mark $!error_action %pop_block;
        WS* ">" EOL $!error_action { fret; };
    *|;

    finclude := |*
        WS* iparam >mark $!error_action %include_config EOL >mark $!error_action { fret; };
    *|;

    main := |*
        WS* comment;
	WS* [Ii] "nclude" { fcall finclude; };
        WS* token >mark %start_dir { fcall parameters; };
        "<" { fcall newblock; };
        "</" { fcall endblock; };
        WS+;
        EOL;
    *|;
}%%

%% write data;

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
    %% access fsm.;
    %% variable p fsm.p;
    %% variable pe fsm.pe;
    %% variable eof fsm.eof;

    %% write init;
    %% write exec;

    /* Ensure that our block is always empty on last chunk. */
    if ( is_last_chunk && blkname != NULL ) {
        return IB_EINVAL;
    }

    return rc;
}
