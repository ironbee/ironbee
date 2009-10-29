
#include "htp.h"

/**
 * Creates a new transaction.
 *
 * @param cfg
 * @param is_cfg_shared
 * @param conn
 * @return The newly created transaction, or NULL on memory allocation failure.
 */
htp_tx_t *htp_tx_create(htp_cfg_t *cfg, int is_cfg_shared, htp_conn_t *conn) {
    // TODO Does the transaction care which connection it belongs to?
    htp_tx_t *tx = calloc(1, sizeof (htp_tx_t));
    if (tx == NULL) return NULL;

    tx->cfg = cfg;
    tx->is_cfg_shared = is_cfg_shared;

    tx->conn = conn;

    tx->request_header_lines = list_array_create(32);
    tx->request_headers = table_create(32);
    tx->request_line_nul_offset = -1;
    
    tx->response_header_lines = list_array_create(32);
    tx->response_headers = table_create(32);

    tx->messages = list_array_create(8);
    tx->request_protocol_number = -1;

    return tx;
}

/**
 * Destroys the supplied transaction.
 *
 * @param htp_tx_t
 */
 void htp_tx_destroy(htp_tx_t *tx) {
    if (tx->request_line != 0) {
        free(tx->request_line);
    }

    free(tx->messages);
    free(tx->request_headers);
    free(tx->request_header_lines);

    free(tx);
}

/**
 * Returns the user data associated with this transaction.
 *
 * @param tx
 * @return A pointer to user data or NULL
 */
void *htp_tx_get_user_data(htp_tx_t *tx) {
   return tx->user_data;
}

/**
 * Sets the configuration that is to be used for this transaction.
 *
 * @param tx
 * @param cfg
 * @param is_cfg_shared
 */
void htp_tx_set_config(htp_tx_t *tx, htp_cfg_t *cfg, int is_cfg_shared) {
    tx->cfg = cfg;
    tx->is_cfg_shared = is_cfg_shared;
}

/**
 * Associates user data with this transaction.
 *
 * @param tx
 * @param user_data
 */
void htp_tx_set_user_data(htp_tx_t *tx, void *user_data) {
    tx->user_data = user_data;
}
