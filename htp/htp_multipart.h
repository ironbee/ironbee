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

#ifndef _HTP_MULTIPART_H
#define	_HTP_MULTIPART_H

typedef struct htp_mpartp_t htp_mpartp_t;
typedef struct htp_mpart_part_t htp_mpart_part_t;

#include "bstr.h"
#include "dslib.h"

#define MULTIPART_PART_UNKNOWN      0
#define MULTIPART_PART_TEXT         1
#define MULTIPART_PART_FILE         2
#define MULTIPART_PART_PREAMBLE     3
#define MULTIPART_PART_EPILOGUE     4

#define MULTIPART_STATE_DATA                1
#define MULTIPART_STATE_BOUNDARY            2
#define MULTIPART_STATE_BOUNDARY_IS_LAST1    3
#define MULTIPART_STATE_BOUNDARY_IS_LAST2    4
#define MULTIPART_STATE_BOUNDARY_EAT_CRLF   5
#define MULTIPART_STATE_BOUNDARY_EAT_LF     6

#ifndef CR
#define CR '\r'
#endif

#ifndef LF
#define LF '\n'
#endif

struct htp_mpart_part_t {
    /** */
    int type;
   
    /** */
    char *name;

    /** */
    table_t *headers;

    /** */
    bstr *body;
};

struct htp_mpartp_t {
    /** */
    char *boundary;
    size_t blen;
    size_t bpos;

    /** */
    int boundary_count;

    /** */
    int seen_last_boundary;

    /** */
    list_t parts;

    // Parsing callbacks
    int (*handle_data)(htp_mpartp_t *mpartp, unsigned char *data, size_t len);
    int (*handle_boundary)(htp_mpartp_t *mpartp);

    // Parsing fields

    int state;
    unsigned char *current_data;
    size_t current_len;
    bstr_builder_t *boundary_pieces;
    int cr_aside;
};

htp_mpartp_t *htp_mpartp_create(char *boundary);
void htp_mpartp_destroy(htp_mpartp_t *mpartp);

 int htp_mpartp_parse(htp_mpartp_t *mpartp, unsigned char *data, size_t len);
void htp_mpartp_finalize(htp_mpartp_t *mpartp);


#endif	/* _HTP_MULTIPART_H */


