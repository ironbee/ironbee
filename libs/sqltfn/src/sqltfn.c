/***************************************************************************
 * Copyright (c) 2012, Qualys, Inc.
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

#include <sqltfn.h>

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* http://www.postgresql.org/docs/9.2/static/sql-syntax-lexical.html */

typedef struct tfn_state_t {
    char *s;
    char *d;
    size_t slen;
    size_t dlen;
    int last_byte;
} tfn_state_t;

#define COPY_BYTE \
    state->last_byte = *state->s++; \
    *state->d++ = state->last_byte; \
    state->slen--; \
    state->dlen++;    

#define SKIP_BYTE state->s++; \
    state->slen--

#define WRITE_BYTE(B) \
    *state->d++ = (B); \
    state->dlen++; \
    state->last_byte = (B)

#define LF 0x0a
#define CR 0x0d
#define SP 0x20

// This macro tests for what the RDBMS sees as whitespace, not what actually is
#define IS_WHITESPACE(X) ( ((X) == 0x09) || ((X) == 0x0a) || ((X) == 0x0c) || ((X) == 0x0d) || ((X) == 0x20) )

#define MATCH_BYTE(X) (*state->s == (X))

#define MATCH_TWO_BYTES(X, Y) (*state->s == (X)) && (state->slen > 1) && (*(state->s + 1) == (Y))

#define MATCH_E_STRING ( ((*state->s == 'e') || (*state->s == 'E')) && (state->slen > 1) && (*(state->s + 1) == '\'') )

#define MATCH_U_STRING ( ((*state->s == 'u') || (*state->s == 'U')) && (state->slen > 2) && (*(state->s + 1) == '&') && ((*(state->s + 2) == '\'')||(*(state->s + 2) == '"')) )

#define TAG_CHAR(X) ( ((X) == '_') || (((X) >= 'a')&&((X) <= 'z')) || (((X) >= 'A')&&((X) <= 'Z')) || (((X) >= '0')&&((X) <= '9')) )

/**
 * Handle a dollar-escaped string (e.g., $$text$$ or $tag$text$tag$).
 * 
 * @param[in, out] state
 * @return -1 in case of memory allocation errorr 
 */
static int sqltfn_normalize_pg_handle_dollar_string(tfn_state_t *state) {
    char *tag = NULL;
    size_t tag_len = 0;

    // copy $
    COPY_BYTE;

    // Have we reached the end of input?
    if (state->slen == 0) {
        return 1;
    }

    // The first character cannot be a digit
    if (isdigit(*state->s)) {
        return 1;
    }

    // Is there a tag?
    if (!MATCH_BYTE('$')) {
        // Extract tag

        char *tag_start = state->s;
        size_t tag_len = 0;
        while (((state->slen - tag_len) > 0) && (*(state->s + tag_len) != '$')) {
            // Check that the byte is a valid tag character
            if (!TAG_CHAR(*(state->s + tag_len))) {
                return 1;
            }

            tag_len++;
        }

        // Have we reached the end of input without
        // finding the second $ character?
        if (state->slen - tag_len == 0) {
            return 1;
        }

        // Now that we know that we have a valid tag, make
        // a copy of it, and also copy it into output       

        // Determine the tag length, and copy it
        // TODO Why copy when the tag is always available in input?        
        tag = malloc(tag_len);
        if (tag == NULL) return -1;
        memcpy(tag, tag_start, tag_len);

        while(tag_len--) {            
            COPY_BYTE;
        }
    }

    // Copy the second $    
    COPY_BYTE;       

    // Loop until the end of the string
    while (state->slen > 0) {
        if (MATCH_BYTE('$')) {
            // Possible end of string

            // Match the tag, if any
            size_t i = 0;
            while (((state->slen - i) > 0) && (i < tag_len) && (state->s[i + 1] == tag[i])) {
                i++;
            }

            // If we've matched the entire tag, and the next character
            // is a $, then we've reached the end of the string

            if ((i == tag_len) && (state->slen - i - 1 > 0) && (*(state->s + i + 1) == '$')) {
                // Copy the first $
                COPY_BYTE;

                // Copy tag characters
                while (i--) {
                    COPY_BYTE;
                }

                // Copy the second $
                COPY_BYTE;

                if (tag != NULL) {
                    free(tag);
                }

                return 1;
            }
        }

        // Copy the current byte
        COPY_BYTE;
    }

    if (tag != NULL) {
        free(tag);
    }

    return 1;
}

/**
 * Handle a Unicode-escaped string (e.g., U&'unicode-text').
 * 
 * @param[in, out] state
 */
static void sqltfn_normalize_pg_handle_string_unicode(tfn_state_t *state, int delimiter) {
    // Process input until the end of string is encountered
    while (state->slen > 0) {
        if (MATCH_BYTE(delimiter)) {
            // Copy the terminating single quote
            COPY_BYTE;

            return;
        } else {
            // Copy the current byte
            COPY_BYTE;
        }
    }

    return;
}

/**
 * Handle a plain string (e.g., 'text').
 * 
 * @param[in, out] state
 */
static void sqltfn_normalize_pg_handle_string(tfn_state_t *state, int delimiter) {
    // Process input until the end of string is encountered
    while (state->slen > 0) {
        if (MATCH_TWO_BYTES('\\', '\\')) {
            // Backslash-escaped backslash
            COPY_BYTE;
            COPY_BYTE;            
        } else if (MATCH_TWO_BYTES('\\', delimiter)) {
            // Backslash-escaped delimiter
            COPY_BYTE;
            COPY_BYTE;
        } else if (MATCH_BYTE(delimiter)) {
            // Delimiter
            COPY_BYTE;

            // End of string
            return;
        } else {
            // Copy the current byte
            COPY_BYTE;
        }
    }

    return;
}

static int _sqltfn_normalize_pg(const char *input, size_t input_len, char **output, size_t *output_len) {
    tfn_state_t _state;
    tfn_state_t *state = &_state;

    // Parameter sanity check
    if (input == NULL) return -1;
    if (output == NULL) return -1;
    if (*output == NULL) return -1;
    if (output_len == NULL) return -1;

    // Setup transformation state
    state->s = (char *) input;
    state->d = *output;
    state->slen = input_len;
    state->dlen = 0;
    state->last_byte = -1;

    int comment_depth = 0;

    // Loop until there is input data to process
    while (state->slen > 0) {
        // Are we in a multi-line comment?
        if (comment_depth > 0) {
            // In a multi-line comment; ignoring bytes
            // until the comments unwrap.

            // Is it a beginning of a sub-comment?
            if (MATCH_TWO_BYTES('/', '*')) {
                // Sub-comment

                comment_depth++;

                // Go over /*
                SKIP_BYTE;
                SKIP_BYTE;
            }
            if (MATCH_TWO_BYTES('*', '/')) {
                // End of an existing comment

                comment_depth--;

                // Go over */
                SKIP_BYTE;
                SKIP_BYTE;

                // If we have unwrapped the entire comment, determine
                // if we need to replace it with a space.
                if (comment_depth == 0) {
                    // If the last byte we sent to output was
                    // not a whitespace, send one whitespace
                    // instead of the entire comment.
                    if (state->last_byte != SP) {
                        WRITE_BYTE(SP);
                    }
                }
            } else {
                // Go over one byte of input data
                SKIP_BYTE;
            }
        } else {
            // Not in a multi-line comment           

            // Determine the next token
            if (MATCH_BYTE('\'')) { // 'text'
                // String
                COPY_BYTE;
                sqltfn_normalize_pg_handle_string(state, '\'');
            } else if (MATCH_BYTE('"')) { // "text"
                // String
                COPY_BYTE;
                sqltfn_normalize_pg_handle_string(state, '"');
            } else
                /* "A dollar sign (cash) followed by digits is used to represent a
                 *  positional parameter in the body of a function definition or a
                 *  prepared statement. In other contexts the dollar sign can be
                 *  part of an identifier or a dollar-quoted string constant."
                 */
                if ((MATCH_BYTE('$')) && (!isalpha(state->last_byte))) { // $$text$$ or $tag$text$tag$, but not a$b$
                // $ string
                if (sqltfn_normalize_pg_handle_dollar_string(state) < 0) {
                    free(*output);
                    return -1;
                }
            } else if (MATCH_E_STRING) { // E'text'
                // E string
                COPY_BYTE;
                COPY_BYTE;
                sqltfn_normalize_pg_handle_string(state, '\'');
            } else if (MATCH_U_STRING) { // U&'text'
                // U string
                COPY_BYTE;
                COPY_BYTE;
                COPY_BYTE;
                sqltfn_normalize_pg_handle_string_unicode(state, state->last_byte);
            } else if (IS_WHITESPACE(*state->s)) {
                // Handle a whitespace character

                // Was the previous character also a whitespace?                
                if (state->last_byte != SP) {
                    // The previous character was not a whitespace

                    // Go over the whitespace character
                    SKIP_BYTE;

                    // Convert whitespace to SP
                    WRITE_BYTE(SP);
                } else {
                    // The previous character was also a whitespace,
                    // so we're going to ignore this one.
                    SKIP_BYTE;
                }
            } else if (MATCH_TWO_BYTES('/', '*')) {
                // Handle the beginning of a multi-line comment
                comment_depth++;

                // Go over /*
                SKIP_BYTE;
                SKIP_BYTE;
            } else if (MATCH_TWO_BYTES('-', '-')) {
                // Handle a dash comment

                // Go over --
                SKIP_BYTE;
                SKIP_BYTE;

                // Find end of line or end of input
                while ((state->slen > 0) && (*state->s != CR) && (*state->s != LF)) {
                    SKIP_BYTE;
                }

                // If we stopped because we encountered a newline, go over it
                if (state->slen > 0) {
                    SKIP_BYTE;

                    // Replace comment with a SP, but only if
                    // the previous character was not a SP
                    if (state->last_byte != SP) {
                        WRITE_BYTE(SP);
                    }
                }
            } else {
                // Handle a non-significant byte               

                // Copy byte
                COPY_BYTE;
            }
        }
    }

    *output_len = state->dlen;

    return 1;
}

int sqltfn_normalize_pg(const char *input, size_t input_len, char **output, size_t *output_len) {
    if (output == NULL) return -1;

    // Allocate a buffer of the same size as the input
    *output = malloc(input_len);
    if (*output == NULL) return -1;

    return _sqltfn_normalize_pg(input, input_len, output, output_len);
}

int sqltfn_normalize_pg_ex(const char *input, size_t input_len, char **output, size_t *output_len) {
    if (output == NULL) return -1;

    // Expect the output buffer to already be available 
    if (*output == NULL) return -1;

    return _sqltfn_normalize_pg(input, input_len, output, output_len);
}

