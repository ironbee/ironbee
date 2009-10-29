
#include "htp.h"

/**
 * Is character a linear white space character?
 *
 * @param c
 * @return 0 or 1
 */
int htp_is_lws(int c) {
    if ((c == ' ') || (c == '\t')) return 1;
    else return 0;
}

/**
 * Is character a separator character?
 *
 * @param c
 * @return 0 or 1
 */
int htp_is_separator(int c) {
    /* separators = "(" | ")" | "<" | ">" | "@"
                  | "," | ";" | ":" | "\" | <">
                  | "/" | "[" | "]" | "?" | "="
                  | "{" | "}" | SP | HT         */
    switch (c) {
        case '(':
        case ')':
        case '<':
        case '>':
        case '@':
        case ',':
        case ';':
        case ':':
        case '\\':
        case '"':
        case '/':
        case '[':
        case ']':
        case '?':
        case '=':
        case '{':
        case '}':
        case ' ':
        case '\t':
            return 1;
            break;
        default:
            return 0;
    }
}

/**
 * Is character a text character?
 *
 * @param c
 * @return 0 or 1
 */
int htp_is_text(int c) {
    if (c == '\t') return 1;
    if (c < 32) return 0;
    return 1;
}

/**
 * Is character a token character?
 *
 * @param c
 * @return 0 or 1
 */
int htp_is_token(int c) {
    /* token = 1*<any CHAR except CTLs or separators> */
    /* CHAR  = <any US-ASCII character (octets 0 - 127)> */
    if ((c < 32) || (c > 126)) return 0;
    if (htp_is_separator(c)) return 0;
    return 1;
}

/**
 * Remove one or more line terminators (LF or CRLF) from
 * the end of the line provided as input.
 *
 * @return 0 if nothing was removed, 1 if one or more LF characters were removed, or
 *         2 if one or more CR and/or LF characters were removed.
 */
int htp_chomp(unsigned char *data, size_t *len) {
    int r = 0;

    // Loop until there's no more stuff in the buffer
    while (*len > 0) {
        // Try one LF first
        if (data[*len - 1] == LF) {            
            (*len)--;
            r = 1;

            if (*len == 0) return r;

            // A CR is allowed before LF
            if (data[*len - 1] == CR) {                
                (*len)--;
                r = 2;
            }
        } else return r;
    }

    return r;
}

/**
 * Is character a white space character?
 *
 * @param c
 * @return 0 or 1
 */
int htp_is_space(int c) {
    switch (c) {
        case ' ':
        case '\f':
        case '\v':
        case '\t':
        case '\r':
        case '\n':
            return 1;
        default:
            return 0;
    }
}

/**
 * Converts request method, given as a string, into a number.
 *
 * @param method
 * @return Method number of M_UNKNOWN
 */
int htp_convert_method_to_number(bstr *method) {
    // TODO Add the remaining methods and optimize using parallel matching.

    if (bstr_cmpc(method, "GET") == 0) return M_GET;
    if (bstr_cmpc(method, "POST") == 0) return M_POST;
    if (bstr_cmpc(method, "HEAD") == 0) return M_HEAD;
    if (bstr_cmpc(method, "PUT") == 0) return M_PUT;

    return M_UNKNOWN;
}

/**
 * Is the given line empty? This function expects the line to have
 * a terminating LF.
 *
 * @param data
 * @param len
 * @return 0 or 1
 */
int htp_is_line_empty(char *data, int len) {
    if ((len == 1) || ((len == 2) && (data[0] == CR))) {
        return 1;
    }

    return 0;
}

/**
 * Does line consist entirely of whitespace characters?
 * 
 * @param data
 * @param len
 * @return 0 or 1
 */
int htp_is_line_whitespace(char *data, int len) {
    int i;

    for (i = 0; i < len; i++) {
        if (!isspace(data[i])) {
            return 0;
        }
    }

    return 1;
}

/**
 * Parses Content-Length string (positive decimal number).
 * White space is allowed before and after the number.
 *
 * @param b
 * @return Content-Length as a number, or -1 on error.
 */
int htp_parse_content_length(bstr *b) {
    return htp_parse_positive_integer_whitespace(bstr_ptr(b), bstr_len(b), 10);
}

/**
 * Parses chunk length (positive hexadecimal number).
 * White space is allowed before and after the number.
 *
 * @param b
 * @return Chunk length, or -1 on error.
 */
int htp_parse_chunked_length(char *data, size_t len) {
    return htp_parse_positive_integer_whitespace(data, len, 16);
}

/**
 * A forgiving parser for a positive integer in a given base.
 * White space is allowed before and after the number.
 * 
 * @param data
 * @param len
 * @param base
 * @return The parsed number, or -1 on error.
 */
int htp_parse_positive_integer_whitespace(char *data, size_t len, int base) {
    int pos = 0;

    // Ignore LWS before
    while ((pos < len) && (htp_is_lws(data[pos]))) pos++;
    if (pos == len) return -1001;

    int r = bstr_util_memtoip(data + pos, len - pos, base, &pos);
    if (r < 0) return r;

    // Ignore LWS after
    while (pos < len) {
        if (!htp_is_lws(data[pos])) {
            printf("# %i %c\n", data[pos], data[pos]);
            return -1002;
        }
        
        pos++;
    }

    return r;
}

/**
 * Prints one log message to stderr.
 * 
 * @param log
 */
void htp_print_log_stderr(htp_log_t *log) {
    if (log->code != 0) {
        fprintf(stderr, "[%i][code %i][file %s][line %i] %s\n", log->level,
            log->code, log->file, log->line, log->msg);
    } else {
        fprintf(stderr, "[%i][file %s][line %i] %s\n", log->level,
            log->file, log->line, log->msg);
    }
}

/**
 * Records one log message.
 * 
 * @param connp
 * @param file
 * @param line
 * @param level
 * @param code
 * @param fmt
 */
void htp_log(htp_connp_t *connp, const char *file, int line, int level, int code, const char *fmt, ...) {
    char buf[1024];
    va_list args;

#ifndef HTP_DEBUG
    // Ignore messages below our log level
    if (connp->cfg->log_level < level) {
        return;
    }
#endif

    va_start(args, fmt);

    int r = vsnprintf(buf, 1023, fmt, args);

    if (r < 0) {
        // TODO Will vsnprintf ever return an error?
        snprintf(buf, 1024, "[vnsprintf returned error %i]", r);
    }

    // Indicate overflow with a '+' at the end
    if (r > 1023) {
        buf[1022] = '+';
        buf[1023] = '\0';
    }

    // Create a new log entry...
    htp_log_t *log = calloc(1, sizeof (htp_log_t));
    if (log == NULL) return;

    log->file = file;
    log->line = line;
    log->level = level;
    log->code = code;
    log->msg = strdup(buf);

    // ...and add it to the list
    if (connp->in_tx != NULL) {
        list_add(connp->in_tx->messages, log);

        // Keep track of the highest log level encountered
        if ((level < connp->in_tx->highest_log_level)
            || (connp->in_tx->highest_log_level == 0)) {
            connp->in_tx->highest_log_level = level;
        }
    } else {
        list_add(connp->conn->messages, log);
    }

    if (level == LOG_ERROR) {
        connp->last_error = log;
    }

    va_end(args);

#if HTP_DEBUG
    htp_print_log_stderr(log);
#endif
}

/**
 * Determines if the given line is a continuation (of some previous line).
 *
 * @param connp
 * @param data
 * @param len
 * @return 0 or 1
 */
int htp_connp_is_line_folded(htp_connp_t *connp, char *data, size_t len) {
    // Is there a line?
    if (len == 0) {
        return -1;
    }

    if (htp_is_lws(data[0])) return 1;
    else return 0;
}

/**
 * Determines if the given line is a request terminator.
 *
 * @param connp
 * @param data
 * @param len
 * @return 0 or 1
 */
int htp_connp_is_line_terminator(htp_connp_t *connp, char *data, size_t len) {
    // Is this the end of request headers?
    switch (connp->cfg->spersonality) {
        case HTP_SERVER_IIS_5_1:
            // IIS 5 will accept a whitespace line as a terminator
            if (htp_is_line_whitespace(data, len)) {
                return 1;
            }

            // Fall through
        default:
            // Treat an empty line as terminator
            if (htp_is_line_empty(data, len)) {
                return 1;
            }
            break;
    }

    return 0;
}

/**
 * Determines if the given line can be ignored when it appears before a request.
 *
 * @param connp
 * @param data
 * @param len
 * @return 0 or 1
 */
int htp_connp_is_line_ignorable(htp_connp_t *connp, char *data, size_t len) {
    return htp_connp_is_line_terminator(connp, data, len);
}
