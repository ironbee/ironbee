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

#include "htp.h"

htp_conn_t *htp_conn_create(const htp_connp_t *connp) {
    htp_conn_t *conn = calloc(1, sizeof (htp_conn_t));
    if (conn == NULL) return NULL;

    conn->connp = connp;

    conn->transactions = htp_list_create(16);
    if (conn->transactions == NULL) {
        free(conn);
        return NULL;
    }

    conn->messages = htp_list_create(8);
    if (conn->messages == NULL) {
        htp_list_destroy(&conn->transactions);
        free(conn);
        return NULL;
    }

    return conn;
}

void htp_conn_destroy(htp_conn_t *conn) {
    if (conn == NULL) return;

    if (conn->transactions != NULL) {
        // Destroy individual transactions. Do note that iterating
        // using the iterator does not work here because some of the
        // list element may be NULL (and with the iterator it is impossible
        // to distinguish a NULL element from the end of the list).        
        for (size_t i = 0, n = htp_list_size(conn->transactions); i < n; i++) {
            htp_tx_t *tx = htp_list_get(conn->transactions, i);
            if (tx != NULL) {
                htp_tx_destroy(tx);
            }
        }

        htp_list_destroy(&conn->transactions);
    }

    if (conn->messages != NULL) {
        // Destroy individual messages
        for (size_t i = 0, n = htp_list_size(conn->messages); i < n; i++) {
            htp_log_t *l = htp_list_get(conn->messages, i);
            free((void *) l->msg);
            free(l);
        }

        htp_list_destroy(&conn->messages);
    }

    if (conn->local_addr != NULL) {
        free(conn->local_addr);
    }

    if (conn->remote_addr != NULL) {
        free(conn->remote_addr);
    }

    // Finally, destroy the connection
    // structure itself.
    free(conn);
}

htp_status_t htp_conn_remove_tx(htp_conn_t *conn, const htp_tx_t *tx) {
    if ((tx == NULL) || (conn == NULL)) return HTP_ERROR;
    if (conn->transactions == NULL) return HTP_ERROR;

    for (size_t i = 0, n = htp_list_size(conn->transactions); i < n; i++) {
        htp_tx_t *candidate_tx = htp_list_get(conn->transactions, i);
        if (tx == candidate_tx) {
            htp_list_replace(conn->transactions, i, NULL);
            return HTP_OK;
        }
    }

    return HTP_ERROR;
}
