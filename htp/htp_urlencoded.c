/*
 * LibHTP (http://www.libhtp.org)
 * Copyright 2009,2010 Ivan Ristic <ivanr@webkreator.com>
 *
 * LibHTP is an open source product, released under terms of the General Public Licence
 * version 2 (GPLv2). Please refer to the file LICENSE, which contains the complete text
 * of the license.
 *
 * In addition, there is a special exception that allows LibHTP to be freely
 * used with any OSI-approved open source licence. Please refer to the file
 * LIBHTP_LICENSING_EXCEPTION for the full text of the exception.
 *
 */

#include "stdlib.h"

#include "htp_urlencoded.h"

/**
 *
 */
htp_urlenp_t *htp_urlenp_create() {
    htp_urlenp_t *urlenp = calloc(1, sizeof (htp_urlenp_t));
    if (urlenp == NULL) return NULL;

    urlenp->params = table_create(HTP_URLENP_DEFAULT_PARAMS_SIZE);
    if (urlenp->params == NULL) {
        free(urlenp);
        return NULL;
    }

    urlenp->_bb = bstr_builder_create();
    if (urlenp->_bb == NULL) {
        table_destroy(urlenp->params);
        free(urlenp);
        return NULL;
    }

    urlenp->argument_separator = '&';
    urlenp->decode_url_encoding = 1;
    urlenp->_state = HTP_URLENP_STATE_KEY;

    return urlenp;
}

/**
 *
 * @param urlenp
 */
void htp_urlenp_destroy(htp_urlenp_t *urlenp) {
    if (urlenp == NULL) return;

    // XXX Deallocate params

    table_destroy(urlenp->params);

    free(urlenp);
}

/**
 *
 */
static void htp_urlenp_add_field_piece(htp_urlenp_t *urlenp, unsigned char *data, size_t startpos, size_t endpos, int c) {
    //fprint_raw_data_ex(stderr, "FIELD", data, startpos, endpos - startpos);

    // Add field if we know it ended or if we know that
    // we've used all of the input data
    if ((c != -1) || (urlenp->_complete)) {
        // Add field
        bstr *field = NULL;
        
        if (bstr_builder_size(urlenp->_bb) > 0) {
            // Add current piece to string builder
            if (endpos - startpos > 0) {
                bstr_builder_append_mem(urlenp->_bb, (char *)data + startpos, endpos - startpos);
            }

            field = bstr_builder_to_str(urlenp->_bb);

            // XXX Clear builder
        } else {
            // We only have the current piece to work with, so
            // no need to involve the string builder
            field = bstr_memdup((char *)data + startpos, endpos - startpos);
        }

        // Process the field differently, depending on the current state
        if (urlenp->_state == HTP_URLENP_STATE_KEY) {
            urlenp->_name = field;
        } else {
            htp_urlen_param_t *param = calloc(1, sizeof(htp_urlen_param_t));
            param->name = urlenp->_name;
            param->value = field;

            fprint_raw_data(stderr, "NAME", (unsigned char *)bstr_ptr(param->name), bstr_len(param->name));
            fprint_raw_data(stderr, "VALUE", (unsigned char *)bstr_ptr(param->value), bstr_len(param->value));
        }
    } else {
        // Make a copy of the data and store it in an array for later
        if (endpos - startpos > 0) {
            bstr_builder_append_mem(urlenp->_bb, (char *)data + startpos, endpos - startpos);
        }
    }
}

/**
 *
 */
int htp_urlenp_parse_complete(htp_urlenp_t *urlenp, unsigned char *data, size_t len) {
    // TODO urlenp->complete must not be 1
    urlenp->_complete = 1;
    return htp_urlenp_parse_partial(urlenp, data, len);
}

/**
 *
 */
int htp_urlenp_parse_partial(htp_urlenp_t *urlenp, unsigned char *data, size_t len) {
    size_t startpos = 0;
    size_t pos = 0;
    int c;

    for (;;) {
        // Get the next character, or -1
        if (pos < len) c = data[pos];
        else c = -1;

        // printf("Pos %d C %c state %d\n", pos, c, urlenp->state);

        switch (urlenp->_state) {
                // Process key
            case HTP_URLENP_STATE_KEY:
                // Look for =, argument separator, or end of input
                if ((c == '=') || (c == urlenp->argument_separator) || (c == -1)) {
                    // Data from startpos to pos                    
                    htp_urlenp_add_field_piece(urlenp, data, startpos, pos, c);

                    // Next state                    
                    startpos = pos + 1;
                    urlenp->_state = HTP_URLENP_STATE_VALUE;
                }
                break;

                // Process value
            case HTP_URLENP_STATE_VALUE:
                // Look for argument separator or end of input
                if ((c == urlenp->argument_separator) || (c == -1)) {
                    // Data from startpos to pos                    
                    htp_urlenp_add_field_piece(urlenp, data, startpos, pos, c);

                    // Next state                    
                    startpos = pos + 1;
                    urlenp->_state = HTP_URLENP_STATE_KEY;
                }
                break;
        }

        // Have we reached the end of input?
        if (c == -1) break;

        pos++;
    }

    return HTP_OK;
}