
#include "htp.h"

/**
 * Creates a new configuration structure. Configuration structures created at
 * configuration time must not be changed afterwards in order to support lock-less
 * copying.
 *
 * @return New configuration structure.
 */
htp_cfg_t *htp_config_create() {
    htp_cfg_t *cfg = calloc(1, sizeof(htp_cfg_t));
    if (cfg == NULL) return NULL;

    cfg->field_limit_hard = HTP_HEADER_LIMIT_HARD;
    cfg->field_limit_soft = HTP_HEADER_LIMIT_SOFT;
    cfg->log_level = LOG_NOTICE;

    // No need to create hooks here; they will be created on-demand,
    // during callback registration

    // Set the default personality before we return
    htp_config_server_personality(cfg, HTP_SERVER_APACHE_2_2);

    return cfg;
}

/**
 * Creates a copy of the supplied configuration structure. The idea is to create
 * one or more configuration objects at configuration-time, but to use this
 * function to create per-connection copies. That way it will be possible to
 * adjust per-connection configuration as necessary, without affecting the
 * global configuration. Make sure no other thread changes the configuration
 * object while this function is operating.
 *
 * @param cfg
 * @return A copy of the configuration structure.
 */
htp_cfg_t *htp_config_copy(htp_cfg_t *cfg) {
    htp_cfg_t *copy = calloc(1, sizeof(htp_cfg_t));
    if (copy == NULL) return NULL;

    // Create copies of the hooks' structures
    if (cfg->hook_transaction_start != NULL) {
        copy->hook_transaction_start = hook_copy(cfg->hook_transaction_start);
        if (copy->hook_transaction_start == NULL) {
            free(copy);
            return NULL;
        }
    }

    if (cfg->hook_request_line != NULL) {
        copy->hook_request_line = hook_copy(cfg->hook_request_line);
        if (copy->hook_request_line == NULL) {
            free(copy);
            return NULL;
        }
    }

    if (cfg->hook_request_headers != NULL) {
        copy->hook_request_headers = hook_copy(cfg->hook_request_headers);
        if (copy->hook_request_headers == NULL) {
            free(copy);
            return NULL;
        }
    }

    if (cfg->hook_request_body_data != NULL) {
        copy->hook_request_body_data = hook_copy(cfg->hook_request_body_data);
        if (copy->hook_request_body_data == NULL) {
            free(copy);
            return NULL;
        }
    }

    if (cfg->hook_request_trailer != NULL) {
        copy->hook_request_trailer = hook_copy(cfg->hook_request_trailer);
        if (copy->hook_request_trailer == NULL) {
            free(copy);
            return NULL;
        }
    }

    if (cfg->hook_request != NULL) {
        copy->hook_request = hook_copy(cfg->hook_request);
        if (copy->hook_request == NULL) {
            free(copy);
            return NULL;
        }
    }
    
    if (cfg->hook_response_line != NULL) {
        copy->hook_response_line = hook_copy(cfg->hook_response_line);
        if (copy->hook_response_line == NULL) {
            free(copy);
            return NULL;
        }
    }

    if (cfg->hook_response_headers != NULL) {
        copy->hook_response_headers = hook_copy(cfg->hook_response_headers);
        if (copy->hook_response_headers == NULL) {
            free(copy);
            return NULL;
        }
    }

    if (cfg->hook_response_body_data != NULL) {
        copy->hook_response_body_data = hook_copy(cfg->hook_response_body_data);
        if (copy->hook_response_body_data == NULL) {
            free(copy);
            return NULL;
        }
    }

    if (cfg->hook_response_trailer != NULL) {
        copy->hook_response_trailer = hook_copy(cfg->hook_response_trailer);
        if (copy->hook_response_trailer == NULL) {
            free(copy);
            return NULL;
        }
    }

    if (cfg->hook_response != NULL) {
        copy->hook_response = hook_copy(cfg->hook_response);
        if (copy->hook_response == NULL) {
            free(copy);
            return NULL;
        }
    }

    return copy;
}

/**
 * Destroy a configuration structure.
 * 
 * @param cfg
 */
void htp_config_destroy(htp_cfg_t *cfg) {
    // Destroy the hooks
    hook_destroy(cfg->hook_transaction_start);
    hook_destroy(cfg->hook_request_line);
    hook_destroy(cfg->hook_request_headers);
    hook_destroy(cfg->hook_request_body_data);
    hook_destroy(cfg->hook_request_trailer);
    hook_destroy(cfg->hook_request);
    hook_destroy(cfg->hook_response_line);
    hook_destroy(cfg->hook_response_headers);
    hook_destroy(cfg->hook_response_body_data);
    hook_destroy(cfg->hook_response_trailer);
    hook_destroy(cfg->hook_response);

    // Release the last remaining bit of memory
    free(cfg);
}

/**
 * Configures filesystem sensitivity. This setting affects
 * how URL paths are normalized.
 *
 * @param cfg
 * @param path_case_insensitive
 */
void htp_config_fs_case_insensitive(htp_cfg_t *cfg, int path_case_insensitive) {
    cfg->path_case_insensitive = path_case_insensitive;
}

/**
 * Configure desired server personality.
 *
 * @param cfg
 * @param personality
 * @return HTP_OK if the personality is supported, HTP_ERROR if it isn't.
 */
int htp_config_server_personality(htp_cfg_t *cfg, int personality) {
    switch (personality) {
        //case HTP_SERVER_STRICT:
        //    break;
        //case HTP_SERVER_PERMISSIVE:
        //    break;
        case HTP_SERVER_APACHE_2_2:
            cfg->parse_request_line = htp_parse_request_line_apache_2_2;            
            cfg->process_request_header = htp_process_request_header_apache_2_2;
            cfg->parse_response_line = htp_parse_response_line_generic;
            cfg->process_response_header = htp_process_response_header_generic;
            cfg->path_backslash_separators = 0;
            cfg->path_decode_separators = 0;
            break;
        //case HTP_SERVER_IIS_5_1:
        //    break;
        //case HTP_SERVER_IIS_7_5:
        //    break;
        default:
            return HTP_ERROR;
    }

    cfg->spersonality = personality;

    return HTP_OK;
}

/**
 * Registers a transaction_start callback.
 * 
 * @param cfg
 * @param callback_fn
 * @param callback_data
 */
void htp_config_register_transaction_start(htp_cfg_t *cfg, int (*callback_fn)(htp_connp_t *)) {
    hook_register(&cfg->hook_transaction_start, callback_fn);
}

/**
 * Registers a request_line callback.
 *
 * @param cfg
 * @param callback_fn
 * @param callback_data
 */
void htp_config_register_request_line(htp_cfg_t *cfg, int (*callback_fn)(htp_connp_t *)) {
    hook_register(&cfg->hook_request_line, callback_fn);
}

/**
 * Registers a request_headers callback.
 *
 * @param cfg
 * @param callback_fn
 * @param callback_data
 */
void htp_config_register_request_headers(htp_cfg_t *cfg, int (*callback_fn)(htp_connp_t *)) {
    hook_register(&cfg->hook_request_headers, callback_fn);
}

/**
 * Registers a request_trailer callback.
 *
 * @param cfg
 * @param callback_fn
 * @param callback_data
 */
void htp_config_register_request_trailer(htp_cfg_t *cfg, int (*callback_fn)(htp_connp_t *)) {
    hook_register(&cfg->hook_request_trailer, callback_fn);
}

/**
 * Registers a request_body_data callback.
 *
 * @param cfg
 * @param callback_fn
 * @param callback_data
 */
void htp_config_register_request_body_data(htp_cfg_t *cfg, int (*callback_fn)(htp_tx_data_t *)) {
    hook_register(&cfg->hook_request_body_data, callback_fn);
}

/**
 * Registers a request callback.
 *
 * @param cfg
 * @param callback_fn
 * @param callback_data
 */
void htp_config_register_request(htp_cfg_t *cfg, int (*callback_fn)(htp_connp_t *)) {
    hook_register(&cfg->hook_request, callback_fn);
}

/**
 * Registers a request_line callback.
 *
 * @param cfg
 * @param callback_fn
 * @param callback_data
 */
void htp_config_register_response_line(htp_cfg_t *cfg, int (*callback_fn)(htp_connp_t *)) {
    hook_register(&cfg->hook_response_line, callback_fn);
}

/**
 * Registers a request_headers callback.
 *
 * @param cfg
 * @param callback_fn
 * @param callback_data
 */
void htp_config_register_response_headers(htp_cfg_t *cfg, int (*callback_fn)(htp_connp_t *)) {
    hook_register(&cfg->hook_response_headers, callback_fn);
}

/**
 * Registers a request_trailer callback.
 *
 * @param cfg
 * @param callback_fn
 * @param callback_data
 */
void htp_config_register_response_trailer(htp_cfg_t *cfg, int (*callback_fn)(htp_connp_t *)) {
    hook_register(&cfg->hook_response_trailer, callback_fn);
}

/**
 * Registers a request_body_data callback.
 *
 * @param cfg
 * @param callback_fn
 * @param callback_data
 */
void htp_config_register_response_body_data(htp_cfg_t *cfg, int (*callback_fn)(htp_tx_data_t *)) {
    hook_register(&cfg->hook_response_body_data, callback_fn);
}

/**
 * Registers a request callback.
 *
 * @param cfg
 * @param callback_fn
 * @param callback_data
 */
void htp_config_register_response(htp_cfg_t *cfg, int (*callback_fn)(htp_connp_t *)) {
    hook_register(&cfg->hook_response, callback_fn);
}
