/***************************************************************************
 * Copyright (c) 2009-2010, Open Information Security Foundation
 * Copyright (c) 2009-2012, Qualys, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * * Neither the name of the Qualys, Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 ***************************************************************************/

/**
 * @file
 * @author Ivan Ristic <ivanr@webkreator.com>
 */

#include <stdio.h>
#include <stdlib.h>

#include "htp_hybrid.h"

htp_tx_t *htp_txh_create(htp_connp_t *connp) {
    // Detect pipelining
    if (list_size(connp->conn->transactions) > connp->out_next_tx_index) {
        connp->conn->flags |= PIPELINED_CONNECTION;
    }
    
    htp_tx_t *tx = htp_tx_create(connp->cfg, CFG_SHARED, connp->conn);
    if (tx == NULL) return NULL;
    
    tx->connp = connp;
    
    list_add(connp->conn->transactions, tx);
    
    htp_connp_in_reset(connp);
    
    return tx;
}

int htp_txh_state_transaction_start(htp_tx_t *tx) {
    // Run hook TRANSACTION_START
    int rc = hook_run_all(tx->connp->cfg->hook_transaction_start, tx->connp);
    if (rc != HOOK_OK) return rc;

    // Change state into request line parsing
    tx->connp->in_state = htp_connp_REQ_LINE;
    tx->connp->in_tx->progress = TX_PROGRESS_REQ_LINE;

    return HTP_OK;
}
