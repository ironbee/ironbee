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

#include <ctype.h>
#include <dirent.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>

#include "../htp/htp.h"

#define CLIENT 1
#define SERVER 2

static int parse_filename(const char *filename, char **remote_addr, char **local_addr) {
    char *copy = strdup(filename);
    char *p, *saveptr;

    char *start = copy;
    char *q = strrchr(copy, '/');
    if (q != NULL) start = q;

    q = strrchr(start, '\\');
    if (q != NULL) start = q;

    int count = 0;
    p = strtok_r(start, "_", &saveptr);
    while (p != NULL) {
        count++;
        // printf("%i %s\n", count, p);

        switch (count) {
            case 3:
                *remote_addr = strdup(p);
                break;
            case 4:
                *local_addr = strdup(p);
                break;
        }

        p = strtok_r(NULL, "_", &saveptr);
    }

    free(copy);

    return 0;
}

static int parse_chunk_info(char *buf, size_t *response_offset, size_t *response_len) {
    char *p = buf;
    size_t lastlen;

    while ((*p != ']') && (p != '\0')) p++;
    if (*p == '\0') return -1;
    p++;

    while (isspace(*p)) p++;

    *response_offset = bstr_util_mem_to_pint(p, strlen(p), 10, &lastlen);

    p += lastlen;

    while ((*p != '(') && (p != '\0')) p++;
    if (*p == '\0') return -1;
    p++;

    *response_len = bstr_util_mem_to_pint(p, strlen(p), 10, &lastlen);

    return 1;
}

static int tcpick_run_file(const char *filename, htp_cfg_t *cfg, htp_connp_t **connp) {
    struct timeval tv;
    char buf[1025];
    int first = -1, current = -1;
    char *remote_addr, *local_addr;

    char *request_last_chunk = NULL;
    char *response_last_chunk = NULL;
    size_t request_offset, request_len;
    size_t request_last_offset = 0, request_last_len = 0;
    size_t response_offset, response_len;
    size_t response_last_offset = 0, response_last_len = 0;

    if (parse_filename(filename, &remote_addr, &local_addr) < 0) {
        printf("Failed to parse filename: %s\n", filename);
        return -1;
    }

    FILE *f = fopen(filename, "rb");
    if (f == NULL) {
        printf("Unable to open file: %s\n", filename);
        return -1;
    }

    gettimeofday(&tv, NULL);

    // Create parser
    *connp = htp_connp_create(cfg);

    // Find all chunks and feed them to the parser
    while (fgets(buf, 1024, f) != NULL) {
        // Ignore empty lines
        if (buf[0] == LF) {
            continue;
        }

        if (strncmp(buf, "[server", 7) == 0) {
            current = SERVER;
        } else {
            current = CLIENT;
        }

        if (first == -1) {
            first = current;

            if (first == SERVER) {
                htp_connp_open(*connp, local_addr, 80, remote_addr, 80, &tv);
            } else {
                htp_connp_open(*connp, remote_addr, 80, local_addr, 80, &tv);
            }
        }

        int len = 0;

        if (first == current) {
            if (parse_chunk_info(buf, &request_offset, &request_len) < 0) {
                printf("Invalid line: %s", buf);
                fclose(f);
                htp_connp_destroy_all(*connp);
                *connp = NULL;
                return -1;
            }

            len = request_len;

            // printf("# Request offset %i len %i\n", request_offset, request_len);
        } else {
            if (parse_chunk_info(buf, &response_offset, &response_len) < 0) {
                printf("Invalid line: %s", buf);
                fclose(f);
                htp_connp_destroy_all(*connp);
                *connp = NULL;
                return -1;
            }

            len = response_len;

            // printf("# Response offset %i len %i\n", response_offset, response_len);
        }

        // printf("Len: %i\n", len);

        if (len <= 0) {
            printf("Invalid length: %i\n", len);
            fclose(f);
            htp_connp_destroy_all(*connp);
            *connp = NULL;
            return -1;
        }

        char *data = malloc(len);
        if (data == NULL) {
            printf("Failed to allocate %i bytes\n", len);
            fclose(f);
            htp_connp_destroy_all(*connp);
            *connp = NULL;
            return -1;
        }

        int read = fread(data, 1, len, f);
        if (read != len) {
            // printf("Failed to read %i bytes (got %i)\n", len, read);
            fclose(f);
            htp_connp_destroy_all(*connp);
            *connp = NULL;
            return -1;
        }

        if (first == current) {
            if ((request_last_chunk == NULL) || (request_len != request_last_len) || (memcmp(data, request_last_chunk, request_len) != 0)) {
                // printf("# Parse request data: %i byte(s)\n", len);
                if (htp_connp_req_data(*connp, &tv, data, len) == HTP_ERROR) {
                    fclose(f);
                    return -1;
                }
            }

            request_last_offset = request_offset;
            request_last_len = request_len;
            if (request_last_chunk != NULL) {
                free(request_last_chunk);
            }
            request_last_chunk = data;
        } else {
            if ((response_last_chunk == NULL) || (response_len != response_last_len) || (memcmp(data, response_last_chunk, response_len) != 0)) {
                // printf("# Parse response data: %i byte(s)\n", len);
                if (htp_connp_res_data(*connp, &tv, data, len) == HTP_ERROR) {
                    fclose(f);
                    return -1;
                }
            }

            response_last_offset = response_offset;
            response_last_len = response_len;
            if (response_last_chunk != NULL) {
                free(response_last_chunk);
            }
            response_last_chunk = data;
        }
    }

    fclose(f);

    htp_connp_close(*connp, &tv);

    return 1;
}

static void print_tx(htp_connp_t *connp, htp_tx_t *tx) {
    char *request_line = bstr_util_strdup_to_c(tx->request_line);
    htp_header_t *h_user_agent = htp_table_get_c(tx->request_headers, "user-agent");
    htp_header_t *h_referer = htp_table_get_c(tx->request_headers, "referer");
    char *referer, *user_agent;
    char buf[256];

    time_t t = time(NULL);
    struct tm *tmp = localtime(&t);

    strftime(buf, 255, "%d/%b/%Y:%T %z", tmp);

    if (h_user_agent == NULL) user_agent = strdup("-");
    else {
        user_agent = bstr_util_strdup_to_c(h_user_agent->value);
    }

    if (h_referer == NULL) referer = strdup("-");
    else {
        referer = bstr_util_strdup_to_c(h_referer->value);
    }

    printf("%s - - [%s] \"%s\" %i %zu \"%s\" \"%s\"\n", connp->conn->client_addr, buf,
        request_line, tx->response_status_number, tx->response_message_len,
        referer, user_agent);

    free(referer);
    free(user_agent);
    free(request_line);
}

static int run_file(char *filename, htp_cfg_t *cfg) {
    htp_connp_t *connp;

    fprintf(stdout, "Running file %s", filename);

    int rc = tcpick_run_file(filename, cfg, &connp);
    if (rc < 0) {
        if (connp != NULL) {
            htp_log_t *last_error = htp_connp_get_last_error(connp);
            if (last_error != NULL) {
                printf(" -- failed: %s\n", last_error->msg);
            } else {
                printf(" -- failed: ERROR NOT AVAILABLE\n");
            }

            return 0;
        } else {
            return -1;
        }
    } else {
        printf(" -- %zu transaction(s)\n", htp_list_size(connp->conn->transactions));

        for (int i = 0, n = htp_list_size(connp->conn->transactions); i < n; i++) {
            htp_tx_t *tx = htp_list_get(connp->conn->transactions, i);

            printf("    ");
            print_tx(connp, tx);
        }

        printf("\n");

        htp_connp_destroy_all(connp);

        return 1;
    }
}

static int run_directory(char *dirname, htp_cfg_t *cfg) {
    struct dirent *entry;
    char buf[1025];
    DIR *d = opendir(dirname);

    if (d == NULL) {
        printf("Failed to open directory: %s\n", dirname);
        return -1;
    }

    while ((entry = readdir(d)) != NULL) {
        if (strncmp(entry->d_name, "tcpick", 6) == 0) {
            strncpy(buf, dirname, 1024);
            strncat(buf, "/", 1024 - strlen(buf));
            strncat(buf, entry->d_name, 1024 - strlen(buf));

            // fprintf(stderr, "Filename: %s\n", buf);
            run_file(buf, cfg);
            //if (run_file(buf, cfg) <= 0) {
            //    closedir(d);
            //    return 0;
            //}
        }
    }

    closedir(d);

    return 1;
}
