/*****************************************************************************
 * Licensed to Qualys, Inc. (QUALYS) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * QUALYS licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *****************************************************************************/

#include "htp.h"

int htp_php_parameter_processor(table_t *params, bstr *name, bstr *value) {
    // TODO Examine the PHP source code to determine the exact
    //      algorithm it uses to transform parameter names    

    // Name transformation

    // Ignore whitespace at the beginning
    char *data = bstr_ptr(name);
    size_t len = bstr_len(name);
    size_t pos = 0;

    while ((pos < len) && (isspace((int)data[pos]))) pos++;

    bstr * new_name = bstr_dup_mem(data + pos, len - pos);

    // Convert the remaining whitespace underscores
    data = bstr_ptr(new_name);
    len = bstr_len(new_name);
    pos = 0;

    while (pos < len) {
        if (isspace((int)data[pos])) data[pos] = '_';
        pos++;
    }

    // Value transformation
    // TODO Support parameter value transformation
    bstr *new_value = bstr_dup(value);

    // Add parameter to table
    table_addn(params, new_name, new_value);

    return HTP_OK;
}
