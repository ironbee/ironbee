/***************************************************************************
 * Copyright (c) 2009-2010 Open Information Security Foundation
 * Copyright (c) 2010-2013 Qualys, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * - Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.

 * - Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.

 * - Neither the name of the Qualys, Inc. nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
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

#ifndef HTP_CONFIG_H
#define	HTP_CONFIG_H

#ifdef	__cplusplus
extern "C" {
#endif

#include "htp.h"

/**
 * Enumerates the possible server personalities.
 */
enum htp_server_personality_t {

    /**
     * Minimal personality that performs at little work as possible. All optional
     * features are disabled. This personality is a good starting point for customization.
     */
    HTP_SERVER_MINIMAL = 0,

    /** A generic personality that aims to work reasonably well for all server types. */
    HTP_SERVER_GENERIC = 1,

    /** The IDS personality tries to perform as much decoding as possible. */
    HTP_SERVER_IDS = 2,

    /** Mimics the behavior of IIS 4.0, as shipped with Windows NT 4.0. */
    HTP_SERVER_IIS_4_0 = 3,

    /** Mimics the behavior of IIS 5.0, as shipped with Windows 2000. */
    HTP_SERVER_IIS_5_0 = 4,

    /** Mimics the behavior of IIS 5.1, as shipped with Windows XP Professional. */
    HTP_SERVER_IIS_5_1 = 5,

    /** Mimics the behavior of IIS 6.0, as shipped with Windows 2003. */
    HTP_SERVER_IIS_6_0 = 6,

    /** Mimics the behavior of IIS 7.0, as shipped with Windows 2008. */
    HTP_SERVER_IIS_7_0 = 7,

    /* Mimics the behavior of IIS 7.5, as shipped with Windows 7. */
    HTP_SERVER_IIS_7_5 = 8,

    /* Mimics the behavior of Apache 2.x. */
    HTP_SERVER_APACHE_2 = 9
};

/**
 * Creates a new configuration structure. Configuration structures created at
 * configuration time must not be changed afterwards in order to support lock-less
 * copying.
 *
 * @return New configuration structure.
 */
htp_cfg_t *htp_config_create(void);

/**
 * Creates a copy of the supplied configuration structure. The idea is to create
 * one or more configuration objects at configuration-time, but to use this
 * function to create per-connection copies. That way it will be possible to
 * adjust per-connection configuration as necessary, without affecting the
 * global configuration. Make sure no other thread changes the configuration
 * object while this function is operating.
 *
 * @param[in] cfg
 * @return A copy of the configuration structure.
 */
htp_cfg_t *htp_config_copy(htp_cfg_t *cfg);

/**
 * Destroy a configuration structure.
 *
 * @param[in] cfg
 */
void htp_config_destroy(htp_cfg_t *cfg);

/**
 * Registers a callback that is invoked every time there is a log message with
 * severity equal and higher than the configured log level.
 *
 * @param[in] cfg
 * @param[in] callback_fn
 */
void htp_config_register_log(htp_cfg_t *cfg, int (*callback_fn)(htp_log_t *));

/**
 * Adds the built-in Multipart parser to the configuration. This parser will extract information
 * stored in request bodies, when they are in multipart/form-data format.
 *
 * @param[in] cfg
 */
void htp_config_register_multipart_parser(htp_cfg_t *cfg);

/**
 * Registers a REQUEST_START callback, which is invoked every time a new
 * request begins and before any parsing is done.
 *
 * @param[in] cfg
 * @param[in] callback_fn
 */
void htp_config_register_request_start(htp_cfg_t *cfg, int (*callback_fn)(htp_connp_t *));

/**
 * Registers a REQUEST_BODY_DATA callback.
 *
 * @param[in] cfg
 * @param[in] callback_fn
 */
void htp_config_register_request_body_data(htp_cfg_t *cfg, int (*callback_fn)(htp_tx_data_t *));

/**
 * Registers a REQUEST_COMPLETE callback.
 *
 * @param[in] cfg
 * @param[in] callback_fn
 */
void htp_config_register_request_complete(htp_cfg_t *cfg, int (*callback_fn)(htp_connp_t *));

/**
 * Registers a REQUEST_FILE_DATA callback.
 *
 * @param[in] cfg
 * @param[in] callback_fn
 */
void htp_config_register_request_file_data(htp_cfg_t *cfg, int (*callback_fn)(htp_file_data_t *));

/**
 * Registers a REQUEST_HEADERS callback.
 *
 * @param[in] cfg
 * @param[in] callback_fn
 */
void htp_config_register_request_headers(htp_cfg_t *cfg, int (*callback_fn)(htp_connp_t *));

/**
 * Registers a REQUEST_LINE callback.
 *
 * @param[in] cfg
 * @param[in] callback_fn
 */
void htp_config_register_request_line(htp_cfg_t *cfg, int (*callback_fn)(htp_connp_t *, unsigned char *, size_t));

/**
 * Registers a REQUEST_URI_NORMALIZE callback.
 *
 * @param[in] cfg
 * @param[in] callback_fn
 */
void htp_config_register_request_uri_normalize(htp_cfg_t *cfg, int (*callback_fn)(htp_connp_t *));

/**
 * Registers a HTP_REQUEST_TRAILER callback.
 *
 * @param[in] cfg
 * @param[in] callback_fn
 */
void htp_config_register_request_trailer(htp_cfg_t *cfg, int (*callback_fn)(htp_connp_t *));

/**
 * Registers a RESPONSE_BODY_DATA callback.
 *
 * @param[in] cfg
 * @param[in] callback_fn
 */
void htp_config_register_response_body_data(htp_cfg_t *cfg, int (*callback_fn)(htp_tx_data_t *));

/**
 * Registers a RESPONSE_COMPLETE callback.
 *
 * @param[in] cfg
 * @param[in] callback_fn
 */
void htp_config_register_response_complete(htp_cfg_t *cfg, int (*callback_fn)(htp_connp_t *));

/**
 * Registers a RESPONSE_HEADERS callback.
 *
 * @param[in] cfg
 * @param[in] callback_fn
 */
void htp_config_register_response_headers(htp_cfg_t *cfg, int (*callback_fn)(htp_connp_t *));

/**
 * Registers a RESPONSE_LINE callback.
 *
 * @param[in] cfg
 * @param[in] callback_fn
 */
void htp_config_register_response_line(htp_cfg_t *cfg, int (*callback_fn)(htp_connp_t *));

/**
 * Registers a RESPONSE_START callback.
 *
 * @param[in] cfg
 * @param[in] callback_fn
 */
void htp_config_register_response_start(htp_cfg_t *cfg, int (*callback_fn)(htp_connp_t *));

/**
 * Registers a RESPONSE_TRAILER callback.
 *
 * @param[in] cfg
 * @param[in] callback_fn
 */
void htp_config_register_response_trailer(htp_cfg_t *cfg, int (*callback_fn)(htp_connp_t *));

/**
 * Adds the built-in Urlencoded parser to the configuration. The parser will
 * parse query strings and request bodies with the appropriate MIME type.
 *
 * @param[in] cfg
 */
void htp_config_register_urlencoded_parser(htp_cfg_t *cfg);

/**
 * Configures the best-fit map, which is used to convert UCS-2 characters into
 * single-byte characters. By default a Windows 1252 best-fit map is used. The map
 * is an list of triplets, the first 2 bytes being an UCS-2 character to map from,
 * and the third byte being the single byte to map to. Make sure that your map contains
 * the mappings to cover the full-width and half-width form characters (U+FF00-FFEF).
 *
 * @param[in] cfg
 * @param[in] map
 */
void htp_config_set_bestfit_map(htp_cfg_t *cfg, unsigned char *map);

/**
 * Configures field parsing limits, which are used when processing request and response
 * lines, and request and response headers. A warning is created when a field is longer
 * than the soft limit. A fatal error will be raised if a field is longer than the hard
 * limit. The hard limit controls the amount of per-field buffering that takes place when
 * requests and responses are fragmented.
 * 
 * @param[in] cfg
 * @param[in] soft_limit
 * @param[in] hard_limit
 */
void htp_config_set_field_limits(htp_cfg_t *cfg, size_t soft_limit, size_t hard_limit);

/**
 * Configures the desired log level.
 * 
 * @param[in] cfg
 * @param[in] log_level
 */
void htp_config_set_log_level(htp_cfg_t *cfg, enum htp_log_level_t log_level);

/**
 * Whether to generate the request_uri_normalized field.
 *
 * @param[in] cfg
 * @param[in] generate
 */
void htp_config_set_generate_request_uri_normalized(htp_cfg_t *cfg, int generate);

/**
 * Enable or disable request HTTP Authentication parsing. Enabled by default.
 *
 * @param[in] cfg
 * @param[in] parse_request_auth
 */
void htp_config_set_parse_request_auth(htp_cfg_t *cfg, int parse_request_auth);

/**
 * Enable or disable request cookie parsing. Enabled by default.
 *
 * @param[in] cfg
 * @param[in] parse_request_cookies
 */
void htp_config_set_parse_request_cookies(htp_cfg_t *cfg, int parse_request_cookies);

/**
 * Configures whether backslash characters are treated as path segment separators. They
 * are not on Unix systems, but are on Windows systems. If this setting is enabled, a path
 * such as "/one\two/three" will be converted to "/one/two/three".
 *
 * @param[in] cfg
 * @param[in] backslash_separators
 */
void htp_config_set_path_backslash_separators(htp_cfg_t *cfg, int backslash_separators);

/**
 * Configures filesystem sensitivity. This setting affects
 * how URL paths are normalized. There are no path modifications by default, but
 * on a case-insensitive systems path will be converted to lowercase.
 *
 * @param[in] cfg
 * @param[in] path_case_insensitive
 */
void htp_config_set_path_case_insensitive(htp_cfg_t *cfg, int path_case_insensitive);

/**
 * Configures whether consecutive path segment separators will be compressed. When
 * enabled, a path such as "/one//two" will be normalized to "/one/two". The backslash_separators
 * and decode_separators parameters are used before compression takes place. For example, if
 * backslash_separators and decode_separators are both enabled, the path "/one\\/two\/%5cthree/%2f//four"
 * will be converted to "/one/two/three/four".
 *
 * @param[in] cfg
 * @param[in] compress_separators
 */
void htp_config_set_path_compress_separators(htp_cfg_t *cfg, int compress_separators);

/**
 * This parameter is used to predict how a server will react when control
 * characters are present in a request path, but does not affect path
 * normalization.
 *
 * @param[in] cfg
 * @param[in] control_char_handling Use NONE with servers that ignore control characters in
 *                              request path, and STATUS_400 with servers that respond
 *                              with 400.
 */
void htp_config_set_path_control_char_handling(htp_cfg_t *cfg, int control_char_handling);

/**
 * Controls the UTF-8 treatment of request paths. One option is to only validate
 * path as UTF-8. In this case, the UTF-8 flags will be raised as appropriate, and
 * the path will remain in UTF-8 (if it was UTF-8in the first place). The other option
 * is to convert a UTF-8 path into a single byte stream using best-fit mapping.
 *
 * @param[in] cfg
 * @param[in] convert_utf8
 */
void htp_config_set_path_convert_utf8(htp_cfg_t *cfg, int convert_utf8);

/**
 * Configures whether encoded path segment separators will be decoded. Apache does
 * not do this, but IIS does. If enabled, a path such as "/one%2ftwo" will be normalized
 * to "/one/two". If the backslash_separators option is also enabled, encoded backslash
 * characters will be converted too (and subsequently normalized to forward slashes).
 *
 * @param[in] cfg
 * @param[in] backslash_separators
 */
void htp_config_set_path_decode_separators(htp_cfg_t *cfg, int backslash_separators);

/**
 * Configures whether %u-encoded sequences in path will be decoded. Such sequences
 * will be treated as invalid URL encoding if decoding is not desirable.
 *
 * @param[in] cfg
 * @param[in] decode_u_encoding
 */
void htp_config_set_path_decode_u_encoding(htp_cfg_t *cfg, int decode_u_encoding);

/**
 * Configures how server reacts to invalid encoding in path.
 *
 * @param[in] cfg
 * @param[in] invalid_encoding_handling The available options are: URL_DECODER_PRESERVE_PERCENT,
 *                                  URL_DECODER_REMOVE_PERCENT, URL_DECODER_DECODE_INVALID
 *                                  and URL_DECODER_STATUS_400.
 */
void htp_config_set_path_invalid_encoding_handling(htp_cfg_t *cfg, int invalid_encoding_handling);

/**
 * Configures how server reacts to invalid UTF-8 characters in path. This setting will
 * not affect path normalization; it only controls what response status we expect for
 * a request that contains invalid UTF-8 characters.
 *
 * @param[in] cfg
 * @param[in] invalid_utf8_unwanted
 */
void htp_config_set_path_invalid_utf8_handling(htp_cfg_t *cfg, enum htp_unwanted_t invalid_utf8_unwanted);

/**
 * Configures how server reacts to encoded NUL bytes. Some servers will terminate
 * path at NUL, while some will respond with 400 or 404. When the termination option
 * is not used, the NUL byte will remain in the path.
 *
 * @param[in] cfg
 * @param[in] nul_encoded_terminates Possible values: TERMINATE, STATUS_400, STATUS_404
 */
void htp_config_set_path_nul_encoded_handling(htp_cfg_t *cfg, int nul_encoded_terminates);

/**
 * Configures how server reacts to raw NUL bytes. Some servers will terminate
 * path at NUL, while some will respond with 400 or 404. When the termination option
 * is not used, the NUL byte will remain in the path.
 *
 * @param[in] cfg
 * @param[in] nul_encoded_terminates Possible values: TERMINATE, STATUS_400, STATUS_404
 */
void htp_config_set_path_nul_raw_terminates(htp_cfg_t *cfg, int nul_encoded_terminates);

/**
 * Sets the replacement character that will be used to in the lossy best-fit
 * mapping from Unicode characters into single-byte streams. The question mark
 * is the default replacement character.
 *
 * @param[in] cfg
 * @param[in] replacement_char
 */
void htp_config_set_path_replacement_char(htp_cfg_t *cfg, int replacement_char);

/**
 * Controls whether compressed response bodies will be automatically decompressed.
 *
 * @param[in] cfg
 * @param[in] enabled set to 1 to enable decompression, 0 otherwise
 */
void htp_config_set_response_decompression(htp_cfg_t *cfg, int enabled);

/**
 * Configure desired server personality.
 *
 * @param[in] cfg
 * @param[in] personality
 * @return HTP_OK if the personality is supported, HTP_ERROR if it isn't.
 */
htp_status_t htp_config_set_server_personality(htp_cfg_t *cfg, enum htp_server_personality_t personality);

/**
 * Configures the path where temporary files should be stored. Must be set
 * in order to use the Multipart file extraction functionality.
 * 
 * @param[in] cfg
 * @param[in] tmpdir
 */
void htp_config_set_tmpdir(htp_cfg_t *cfg, char *tmpdir);

/**
 * Configures whether transactions will be automatically destroyed once they
 * are processed and all callbacks invoked. This option is appropriate for
 * programs that process transactions as they are processed.
 *
 * @param[in] cfg
 * @param[in] tx_auto_destroy
 */
void htp_config_set_tx_auto_destroy(htp_cfg_t *cfg, int tx_auto_destroy);

#ifdef	__cplusplus
}
#endif

#endif	/* HTP_CONFIG_H */

