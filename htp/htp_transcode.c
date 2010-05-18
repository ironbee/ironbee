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

#include <errno.h>

#include "htp.h"

/**
 * Transcode all parameters supplied in the table.
 *
 * @param connp
 * @param params
 * @param destroy_old
 */
int htp_transcode_params(htp_connp_t *connp, table_t **params, int destroy_old) {
    table_t *input_params = *params;

    // No transcoding unless necessary
    if (connp->cfg->internal_encoding == NULL) {
        return HTP_OK;
    }

    // Create a new table that will hold transcoded parameters
    table_t *output_params = table_create(table_size(input_params));
    if (output_params == NULL) {
        return HTP_ERROR;
    }
    
    // Initialize iconv
    iconv_t cd = iconv_open(connp->cfg->internal_encoding, connp->cfg->request_encoding);
    if (cd == (iconv_t) -1) {
        // TODO Report iconv initialization error
        table_destroy(&output_params);
        return HTP_ERROR;
    }

    #if (_LIBICONV_VERSION >= 0x0108)
    int iconv_param = 0;
    iconvctl(cd, ICONV_SET_TRANSLITERATE, &iconv_param);
    iconv_param = 1;
    iconvctl(cd, ICONV_SET_DISCARD_ILSEQ, &iconv_param);
    #endif
    
    // Convert the parameters, one by one
    bstr *name = NULL;
    bstr *value = NULL;
    table_iterator_reset(input_params);
    while ((name = table_iterator_next(input_params, (void **) & value)) != NULL) {
        bstr *new_name = NULL, *new_value = NULL;
        
        // Convert name
        htp_transcode_bstr(cd, name, &new_name);
        if (new_name == NULL) {
            iconv_close(cd);
            
            table_iterator_reset(output_params);
            while(table_iterator_next(output_params, (void **) & value) != NULL) {
                bstr_free(&value);
            }
            
            table_destroy(&output_params);
        }
        
        // Convert value        
        htp_transcode_bstr(cd, value, &new_value);
        if (new_value == NULL) {
            bstr_free(&new_name);
            iconv_close(cd);
            
            table_iterator_reset(output_params);
            while(table_iterator_next(output_params, (void **) & value) != NULL) {
                bstr_free(&value);
            }
            
            table_destroy(&output_params);
            return HTP_ERROR;
        }
        
        // Add to new table
        table_addn(output_params, new_name, new_value);
    }
    
    // Replace the old parameter table
    *params = output_params;

    // Destroy the old parameter table if necessary
    if (destroy_old) {
        table_iterator_reset(input_params);
        while(table_iterator_next(input_params, (void **) & value) != NULL) {
            bstr_free(&value);
        }      
    
        table_destroy(&input_params);
    }
    
    iconv_close(cd);

    return HTP_OK;
}

/**
 * Transcode one bstr.
 *
 * @param cd
 * @param input
 * @param output
 */
int htp_transcode_bstr(iconv_t cd, bstr *input, bstr **output) {
    // Reset conversion state for every new string
    iconv(cd, NULL, 0, NULL, 0);

    bstr_builder_t *bb = NULL;

    size_t buflen = 10;
    char *buf = malloc(buflen);
    if (buf == NULL) {
        return HTP_ERROR;
    }

    char *inbuf = bstr_ptr(input);
    size_t inleft = bstr_len(input);
    char *outbuf = buf;
    size_t outleft = buflen;

    int loop = 1;
    while (loop) {
        loop = 0;

        if (iconv(cd, &inbuf, &inleft, &outbuf, &outleft) == (size_t) - 1) {
            if (errno == E2BIG) {
                // Create bstr builder on-demand
                if (bb == NULL) {
                    bb = bstr_builder_create();
                    if (bb == NULL) {
                        free(buf);
                        return HTP_ERROR;
                    }
                }

                // The output buffer is full
                bstr_builder_append_mem(bb, buf, buflen - outleft);

                outbuf = buf;
                outleft = buflen;

                // Continue in the loop, as there's more work to do
                loop = 1;
            } else {
                // Error
                if (bb != NULL) bstr_builder_destroy(bb);
                free(buf);
                return HTP_ERROR;
            }
        }
    }
    
    if (bb != NULL) {
        bstr_builder_append_mem(bb, buf, buflen - outleft);
        *output = bstr_builder_to_str(bb);
        bstr_builder_destroy(bb);
        if (*output == NULL) {
            free(buf);
            return HTP_ERROR;
        }
    } else {
        *output = bstr_memdup(buf, buflen - outleft);
        if (*output == NULL) {
            free(buf);
            return HTP_ERROR;
        }
    }
    
    free(buf);

    return HTP_OK;
}
