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

#include "htp.h"

int htp_php_parameter_processor(table_t *params, bstr *name, bstr *value) {
    // TODO Examine the PHP source code to determine the exact
    //      algorithm it uses to transform parameter names

    // TODO Support parameter value transformation

    // Ignore whitespace at the beginning
    char *data = bstr_ptr(name);
    size_t len = bstr_len(name);
    size_t pos = 0;

    while ((pos < len) && (isspace((int)data[pos]))) pos++;

    bstr * new_name = bstr_memdup(data + pos, len - pos);

    // Convert the remaining whitespace underscores
    data = bstr_ptr(new_name);
    len = bstr_len(new_name);
    pos = 0;

    while (pos < len) {
        if (isspace((int)data[pos])) data[pos] = '_';
        pos++;
    }

    // Add parameter to table
    table_add(params, new_name, value);

    return HTP_OK;
}
