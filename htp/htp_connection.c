
#include "htp.h"

/**
 * Creates a new connection structure.
 *
 * @param connp
 * @return A new htp_connp_t structure on success, NULL on memory allocation failure.
 */
htp_conn_t *htp_conn_create(htp_connp_t *connp) {
    htp_conn_t *conn = calloc(1, sizeof (htp_conn_t));
    if (conn == NULL) return NULL;

    conn->connp = connp;

    conn->transactions = list_array_create(16);
    if (conn->transactions == NULL) {
        free(conn);
        return NULL;
    }

    conn->messages = list_array_create(8);
    if (conn->messages == NULL) {
        list_destroy(conn->transactions);
        free(conn);
        return NULL;
    }

    return conn;
}

/**
 * Destroys a connection, as well as all the transactions it contains. It is
 * not possible to destroy a connection structure yet leave any of its
 * transactions intact. This is because transactions need its connection and
 * connection structures hold little data anyway. The opposite is true, though
 * it is possible to delete a transaction but leave its connection alive.
 *
 * @param conn
 */
void htp_conn_destroy(htp_conn_t *conn) {
    // Destroy individual transactions
    htp_tx_t *tx = NULL;
    list_iterator_reset(conn->transactions);
    while ((tx = list_iterator_next(conn->transactions)) != NULL) {
        htp_tx_destroy(tx);
    }

    // Destroy the list...
    list_destroy(conn->transactions);

    // ...and the connection structure itself.
    free(conn);
}
