
#include "htp.h"

/**
 * Determines protocol number from a textual representation (i.e., "HTTP/1.1"). This
 * function will only understand a properly formatted protocol information. It does
 * not try to be flexible.
 * 
 * @param protocol
 * @return Protocol version or PROTOCOL_UKNOWN.
 */
int htp_parse_protocol(bstr *protocol) {
    if (bstr_len(protocol) == 8) {
        char *ptr = bstr_ptr(protocol);
        if ((ptr[0] == 'H') && (ptr[1] == 'T') && (ptr[2] == 'T') && (ptr[3] == 'P')
            && (ptr[4] == '/') && (ptr[6] == '.')) {
            // Check the version numbers
            if (ptr[5] == '0') {
                if (ptr[7] == '9') {
                    return HTTP_0_9;
                }
            } else if (ptr[5] == '1') {
                if (ptr[7] == '0') {
                    return HTTP_1_0;
                } else if (ptr[7] == '1') {
                    return HTTP_1_1;
                }
            }
        }
    }

    return PROTOCOL_UNKNOWN;
}

/**
 * Determines the numerical value of a response status given as a string.
 *
 * @param status
 * @return Status code on success, or -1 on error.
 */
int htp_parse_status(bstr *status) {
    return htp_parse_positive_integer_whitespace(bstr_ptr(status), bstr_len(status), 10);
}
