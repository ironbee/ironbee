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

#ifndef _HTP_MULTIPART_PRIVATE_H
#define	_HTP_MULTIPART_PRIVATE_H

#ifdef __cplusplus
extern "C" {
#endif

static htp_status_t htp_mpartp_run_request_file_data_hook(htp_mpart_part_t *part, const unsigned char *data, size_t len);

htp_status_t htp_mpart_part_process_headers(htp_mpart_part_t *part);

htp_status_t htp_mpartp_parse_header(htp_mpart_part_t *part, const unsigned char *data, size_t len);

htp_status_t htp_mpart_part_handle_data(htp_mpart_part_t *part, const unsigned char *data, size_t len, int is_line);

int htp_mpartp_is_boundary_character(int c);

htp_mpart_part_t *htp_mpart_part_create(htp_mpartp_t *mpartp);

htp_status_t htp_mpart_part_finalize_data(htp_mpart_part_t *part);

void htp_mpart_part_destroy(htp_mpart_part_t *part, int gave_up_data);

#ifdef __cplusplus
}
#endif

#endif	/* _HTP_MULTIPART_PRIVATE_H */
