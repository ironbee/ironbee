
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
    htp_tx_t *tx = calloc(1, sizeof (htp_tx_t));
    if (tx == NULL) return NULL;

    tx->conn = conn;
    tx->cfg = cfg;
    tx->is_cfg_shared = is_cfg_shared;

    tx->conn = conn;

    tx->request_header_lines = list_array_create(32);
    tx->request_headers = table_create(32);
    tx->request_line_nul_offset = -1;
    tx->parsed_uri = calloc(1, sizeof (htp_uri_t));
    tx->parsed_uri_incomplete = calloc(1, sizeof (htp_uri_t));

    tx->response_header_lines = list_array_create(32);
    tx->response_headers = table_create(32);

    tx->messages = list_array_create(8);
    tx->request_protocol_number = -1;

    return tx;
}

/**
 * Destroys the supplied transaction.
 *
 * @param tx
 */
void htp_tx_destroy(htp_tx_t *tx) {
    bstr_free(tx->request_line);
    bstr_free(tx->request_method);
    bstr_free(tx->request_uri);
    bstr_free(tx->request_protocol);

    if (tx->parsed_uri != NULL) {
        bstr_free(tx->parsed_uri->scheme);
        bstr_free(tx->parsed_uri->username);
        bstr_free(tx->parsed_uri->password);
        bstr_free(tx->parsed_uri->hostname);
        bstr_free(tx->parsed_uri->port);
        bstr_free(tx->parsed_uri->path);
        bstr_free(tx->parsed_uri->query);
        bstr_free(tx->parsed_uri->fragment);

        free(tx->parsed_uri);
    }

    if (tx->parsed_uri_incomplete != NULL) {
        bstr_free(tx->parsed_uri_incomplete->scheme);
        bstr_free(tx->parsed_uri_incomplete->username);
        bstr_free(tx->parsed_uri_incomplete->password);
        bstr_free(tx->parsed_uri_incomplete->hostname);
        bstr_free(tx->parsed_uri_incomplete->port);
        bstr_free(tx->parsed_uri_incomplete->path);
        bstr_free(tx->parsed_uri_incomplete->query);
        bstr_free(tx->parsed_uri_incomplete->fragment);

        free(tx->parsed_uri_incomplete);
    }

    // Destroy request_header_lines
    htp_header_line_t *hl = NULL;
    list_iterator_reset(tx->request_header_lines);
    while ((hl = list_iterator_next(tx->request_header_lines)) != NULL) {
        bstr_free(hl->line);
        // No need to destroy hl->header because
        // htp_header_line_t does not own it.
        free(hl);
    }

    list_destroy(tx->request_header_lines);

    // Destroy request_headers
    bstr *key = NULL;
    htp_header_t *h = NULL;
    table_iterator_reset(tx->request_headers);
    while ((key = table_iterator_next(tx->request_headers, (void **) & h)) != NULL) {
        bstr_free(h->name);
        bstr_free(h->value);
        free(h);
    }

    table_destroy(tx->request_headers);

    bstr_free(tx->response_line);
    bstr_free(tx->response_protocol);
    bstr_free(tx->response_status);
    bstr_free(tx->response_message);

    // Destroy response_header_lines
    hl = NULL;
    list_iterator_reset(tx->response_header_lines);
    while ((hl = list_iterator_next(tx->response_header_lines)) != NULL) {
        bstr_free(hl->line);
        // No need to destroy hl->header because
        // htp_header_line_t does not own it.
        free(hl);
    }
    list_destroy(tx->response_header_lines);

    // Destroy response headers
    key = NULL;
    h = NULL;
    table_iterator_reset(tx->response_headers);
    while ((key = table_iterator_next(tx->response_headers, (void **) & h)) != NULL) {
        bstr_free(h->name);
        bstr_free(h->value);
        free(h);
    }
    table_destroy(tx->response_headers);

    // Destroy messages
    htp_log_t *l = NULL;
    list_iterator_reset(tx->messages);
    while ((l = list_iterator_next(tx->messages)) != NULL) {
        free((void *)l->msg);
        free(l);
    }

    list_destroy(tx->messages);

    // Tell the connection to remove this transaction
    // from the list
    htp_conn_remove_tx(tx->conn, tx);

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
