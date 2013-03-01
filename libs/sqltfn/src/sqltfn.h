/***************************************************************************
 * Copyright (c) 2012 Qualys, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 *
 * * Neither the name of the Qualys, Inc. nor the names of its
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

#ifndef _SQLTFN_H
#define	_SQLTFN_H

#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Normalize input, assuming it contains an SQL string that will be interpreted
 * by PostgreSQL. Normalization removes comments, converts all whitespace
 * characters to SP, and compresses multiple SP instances into a single character.
 * 
 * @param[in]   input
 * @param[in]   input_len
 * @param[out]  output
 * @param[out]  output_len
 *
 * @return -1 on memory allocation failure
 */
int sqltfn_normalize_pg(const char *input, size_t input_len, char **output, size_t *output_len);

// XXX What if the attack payload is being injected into a string (single-, double-, or string-quoted)?

/**
 * Same as sqltfn_normalize_pg(), but expects a pre-allocated output buffer.
 */
int sqltfn_normalize_pg_ex(const char *input, size_t input_len, char **output, size_t *output_len);

#ifdef __cplusplus
}
#endif

#endif /* _SQLTFN_H */
