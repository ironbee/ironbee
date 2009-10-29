
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
 * @param cfg;
 * @return A copy of the configuration structure.
 */
htp_cfg_t *htp_config_copy(htp_cfg_t *cfg) {
    htp_cfg_t *copy = calloc(1, sizeof(htp_cfg_t));
    if (copy == NULL) return NULL;

    memcpy(copy, cfg, sizeof(htp_cfg_t));

    // TODO Create copies of the hooks' structures

    return copy;
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
        case HTP_SERVER_STRICT:
            break;
        case HTP_SERVER_PERMISSIVE:
            break;
        case HTP_SERVER_APACHE_2_2:
            cfg->parse_request_line = htp_parse_request_line_apache_2_2;            
            cfg->process_request_header = htp_process_request_header_apache_2_2;
            cfg->parse_response_line = htp_parse_response_line_generic;
            cfg->process_response_header = htp_process_response_header_generic;
            break;
        case HTP_SERVER_IIS_5_1:
            break;
        case HTP_SERVER_IIS_7_5:
            break;
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
void htp_config_register_transaction_start(htp_cfg_t *cfg, int (*callback_fn)(htp_connp_t *), int priority) {
    hook_register(&cfg->hook_transaction_start, callback_fn, priority);
}

/**
 * Registers a request_line callback.
 *
 * @param cfg
 * @param callback_fn
 * @param callback_data
 */
void htp_config_register_request_line(htp_cfg_t *cfg, int (*callback_fn)(htp_connp_t *), int priority) {
    hook_register(&cfg->hook_request_line, callback_fn, priority);
}

/**
 * Registers a request_headers callback.
 *
 * @param cfg
 * @param callback_fn
 * @param callback_data
 */
void htp_config_register_request_headers(htp_cfg_t *cfg, int (*callback_fn)(htp_connp_t *), int priority) {
    hook_register(&cfg->hook_request_headers, callback_fn, priority);
}

/**
 * Registers a request_trailer callback.
 *
 * @param cfg
 * @param callback_fn
 * @param callback_data
 */
void htp_config_register_request_trailer(htp_cfg_t *cfg, int (*callback_fn)(htp_connp_t *), int priority) {
    hook_register(&cfg->hook_request_trailer, callback_fn, priority);
}

/**
 * Registers a request_body_data callback.
 *
 * @param cfg
 * @param callback_fn
 * @param callback_data
 */
void htp_config_register_request_body_data(htp_cfg_t *cfg, int (*callback_fn)(htp_tx_data_t *), int priority) {
    hook_register(&cfg->hook_request_body_data, callback_fn, priority);
}

/**
 * Registers a request callback.
 *
 * @param cfg
 * @param callback_fn
 * @param callback_data
 */
void htp_config_register_request(htp_cfg_t *cfg, int (*callback_fn)(htp_connp_t *), int priority) {
    hook_register(&cfg->hook_request, callback_fn, priority);
}

/**
 * Registers a request_line callback.
 *
 * @param cfg
 * @param callback_fn
 * @param callback_data
 */
void htp_config_register_response_line(htp_cfg_t *cfg, int (*callback_fn)(htp_connp_t *), int priority) {
    hook_register(&cfg->hook_response_line, callback_fn, priority);
}

/**
 * Registers a request_headers callback.
 *
 * @param cfg
 * @param callback_fn
 * @param callback_data
 */
void htp_config_register_response_headers(htp_cfg_t *cfg, int (*callback_fn)(htp_connp_t *), int priority) {
    hook_register(&cfg->hook_response_headers, callback_fn, priority);
}

/**
 * Registers a request_trailer callback.
 *
 * @param cfg
 * @param callback_fn
 * @param callback_data
 */
void htp_config_register_response_trailer(htp_cfg_t *cfg, int (*callback_fn)(htp_connp_t *), int priority) {
    hook_register(&cfg->hook_response_trailer, callback_fn, priority);
}

/**
 * Registers a request_body_data callback.
 *
 * @param cfg
 * @param callback_fn
 * @param callback_data
 */
void htp_config_register_response_body_data(htp_cfg_t *cfg, int (*callback_fn)(htp_tx_data_t *), int priority) {
    hook_register(&cfg->hook_response_body_data, callback_fn, priority);
}

/**
 * Registers a request callback.
 *
 * @param cfg
 * @param callback_fn
 * @param callback_data
 */
void htp_config_register_response(htp_cfg_t *cfg, int (*callback_fn)(htp_connp_t *), int priority) {
    hook_register(&cfg->hook_response, callback_fn, priority);
}
