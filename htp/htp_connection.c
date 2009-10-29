
#include "htp.h"

/**
 * Creates a new connection structure.
 *
 * @param connp
 * @return A new htp_connp_t structure on success, NULL on memory allocation failure.
 */
htp_conn_t *htp_conn_create(htp_connp_t *connp) {
    htp_conn_t *conn = calloc(1, sizeof(htp_conn_t));
    if (conn == NULL) return NULL;

    conn->connp = connp;
    conn->transactions = list_array_create(16);
    conn->messages = list_array_create(8);

    return conn;
}

/**
 * Destroys a connection, as well as all the transactions it contains.
 *
 * @param conn
 */
void htp_conn_destroy(htp_conn_t *conn) {
    // TODO Destroy individual transactions
    free(conn->transactions);
    free(conn);
}
