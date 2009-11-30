
#include <stdlib.h>

#include "htp.h"

// NOTE The parser contains a lot of duplicated code. That is on purpose.
//
//      Within the request parser alone there are several states in which
//      bytes are copied into the line buffer and lines are processed one at a time.
//      This code could be made more elegant by adding a new line-reading state along
//      with a what-fn-to-invoke-to-handle-line pointer.
//
//      Furthermore, the entire request parser is terribly similar to the response parser.
//      It is imaginable that a single parser could handle both.
//
//      After some thought, I decided not to make any changes (at least not for the time
//      being). State-based parsers are sometimes difficult to understand. I remember trying
//      to figure one once and I had a hard time following the logic because each function
//      was small and did exactly one thing. There was jumping all around. You could probably
//      say that it was elegant, but I saw it as difficult to understand, verify and maintain.
//
//      Thus, I am keeping this inelegant but quite straightforward parser with duplicated code,
//      mostly for the sake of maintenance.
//
//      For the time being, anyway. I will review at a later time.


/**
 * Clears an existing parser error, if any.
 *
 * @param connp
 */
void htp_connp_clear_error(htp_connp_t *connp) {
    connp->last_error = NULL;
}

/**
 * Closes the connection associated with the supplied parser.
 *
 * @param connp
 */
void htp_connp_close(htp_connp_t *connp, htp_time_t timestamp) {
    connp->conn->close_timestamp = timestamp;
    // TODO Set status to closed    
}

/**
 * Creates a new connection parser using the provided configuration. Because
 * the configuration structure is used directly, in a multithreaded environment
 * you are not allowed to change the structure, ever. If you have a need to
 * change configuration on per-connection basis, make a copy of the configuration
 * structure to go along with every connection parser.
 *
 * @param cfg
 * @return A pointer to a newly created htp_connp_t instance.
 */
htp_connp_t *htp_connp_create(htp_cfg_t *cfg) {
    htp_connp_t *connp = calloc(1, sizeof (htp_connp_t));
    if (connp == NULL) return NULL;

    // Use the supplied configuration structure
    connp->cfg = cfg;

    // Create a new connection object
    connp->conn = htp_conn_create(connp);
    if (connp->conn == NULL) {
        free(connp);
        return NULL;
    }

    connp->status = HTP_OK;

    // Request parsing

    connp->in_line_size = cfg->field_limit_hard;
    connp->in_line_len = 0;
    connp->in_line = malloc(connp->in_line_size);
    if (connp->in_line == NULL) {
        htp_conn_destroy(connp->conn);
        free(connp);
        return NULL;
    }

    connp->in_header_line_index = -1;
    connp->in_state = htp_connp_REQ_IDLE;

    // Response parsing
    
    connp->out_line_size = cfg->field_limit_hard;
    connp->out_line_len = 0;
    connp->out_line = malloc(connp->out_line_size);
    if (connp->out_line == NULL) {
        free(connp->in_line);
        htp_conn_destroy(connp->conn);
        free(connp);
        return NULL;
    }
    
    connp->out_header_line_index = -1;
    connp->out_state = htp_connp_RES_IDLE;

    return connp;
}

/**
 * Creates a new configuration parser, making a copy of the supplied
 * configuration structure.
 *
 * @param cfg
 * @return A pointer to a newly created htp_connp_t instance.
 */
htp_connp_t *htp_connp_create_copycfg(htp_cfg_t *cfg) {
    htp_connp_t *connp = htp_connp_create(cfg);
    if (connp == NULL) return NULL;

    connp->cfg = htp_config_copy(cfg);
    connp->is_cfg_private = 1;

    return connp;
}

/**
 * Destroys the connection parser and its data structures, leaving
 * the connection data intact.
 *
 * @param connp
 */
void htp_connp_destroy(htp_connp_t *connp) {
    if (connp->in_header_line != NULL) {
        if (connp->in_header_line->line != NULL) {
            free(connp->in_header_line->line);
        }

        free(connp->in_header_line);
    }

    free(connp->in_line);

    // Destroy the configuration structure, but only
    // if it is our private copy
    if (connp->is_cfg_private) {
        htp_config_destroy(connp->cfg);
    }

    free(connp);
}

/**
 * Destroys the connection parser, its data structures, as well
 * as the connection and its transactions.
 *
 * @param connp
 */
void htp_connp_destroy_all(htp_connp_t *connp) {
    // Destroy connection
    htp_conn_destroy(connp->conn);

    // Destroy everything else
    htp_connp_destroy(connp);
}

/**
 * Retrieve the user data associated with this connection parser.
 * 
 * @param connp
 * @return User data, or NULL if there isn't any.
 */
void *htp_connp_get_data(htp_connp_t *connp) {
    return connp->user_data;
}

/**
 * Returns the last error that occured with this connection parser.
 *
 * @param connp
 * @return A pointer to an htp_log_t instance if there is an error, or NULL
 *         if there isn't.
 */
htp_log_t *htp_connp_get_last_error(htp_connp_t *connp) {
    return connp->last_error;
}

/**
 * Opens connection.
 *
 * @param connp
 * @param addr Remote IP address
 * @param port Remote port
 */
void htp_connp_open(htp_connp_t *connp, const char *remote_addr, int remote_port, const char *local_addr, int local_port, htp_time_t timestamp) {    
    connp->conn->remote_addr = remote_addr;
    connp->conn->remote_port = remote_port;
    connp->conn->local_addr = local_addr;
    connp->conn->local_port = local_port;
    connp->conn->open_timestamp = timestamp;
}

/**
 * Associate user data with the supplied parser.
 *
 * @param connp
 * @param user_data
 */
void htp_connp_set_user_data(htp_connp_t *connp, void *user_data) {
    connp->user_data = user_data;
}
