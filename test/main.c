
#include <stdio.h>
#include <stdlib.h>

#include "../htp/bstr.h"
#include "../htp/htp.h"
#include "test.h"

char *home = NULL;

/**
 *
 */
int test_get(htp_cfg_t *cfg) {
    htp_connp_t *connp = NULL;
    
    test_run(home, "01-get.t", cfg, &connp);
    if (connp == NULL) return -1;

    return 1;
}

/**
 *
 */
int test_post_urlencoded_chunked(htp_cfg_t *cfg) {
    htp_connp_t *connp = NULL;
    
    test_run(home, "04-post-urlencoded-chunked.t", cfg, &connp);
    if (connp == NULL) return -1;

    htp_tx_t *tx = list_get(connp->conn->transactions, 0);

    bstr *key = NULL;
    htp_header_t *h = NULL;
    table_iterator_reset(tx->request_headers);
    while ((key = table_iterator_next(tx->request_headers, (void **) & h)) != NULL) {
        printf("--   HEADER [%s][%s]\n", bstr_tocstr(h->name), bstr_tocstr(h->value));
    }

    return 1;
}

/**
 *
 */
int test_post_urlencoded(htp_cfg_t *cfg) {
    htp_connp_t *connp = NULL;
    
    test_run(home, "03-post-urlencoded.t", cfg, &connp);
    if (connp == NULL) return -1;

    return 1;
}

/**
 *
 */
int test_apache_header_parsing(htp_cfg_t *cfg) {
    htp_connp_t *connp = NULL;
    
    test_run(home, "02-header-test-apache2.t", cfg, &connp);
    if (connp == NULL) return -1;

    htp_tx_t *tx = list_get(connp->conn->transactions, 0);
    if (tx == NULL) return -1;

    int count = 0;
    bstr *key = NULL;
    htp_header_t *h = NULL;
    table_iterator_reset(tx->request_headers);
    while ((key = table_iterator_next(tx->request_headers, (void **) & h)) != NULL) {
        printf("--   HEADER [%s][%s]\n", bstr_tocstr(h->name), bstr_tocstr(h->value));
    }

    // There must be 9 headers
    if (table_size(tx->request_headers) != 9) {
        printf("Got %i headers, but expected 9\n", table_size(tx->request_headers));
        htp_connp_destroy(connp);
        return -1;
    }

    // Check every header
    count = 0;
    key = NULL;
    h = NULL;
    table_iterator_reset(tx->request_headers);
    while ((key = table_iterator_next(tx->request_headers, (void **) & h)) != NULL) {

        switch (count) {
            case 0:
                if (bstr_cmpc(h->name, " Invalid-Folding") != 0) {
                    printf("Header %i incorrect name\n", count + 1);
                    return -1;
                }
                if (bstr_cmpc(h->value, "1") != 0) {
                    printf("Header %i incorrect value\n", count + 1);
                    return -1;
                }
                break;
            case 1:
                if (bstr_cmpc(h->name, "Valid-Folding") != 0) {
                    printf("Header %i incorrect name\n", count + 1);
                    return -1;
                }
                if (bstr_cmpc(h->value, "2 2") != 0) {
                    printf("Header %i incorrect value\n", count + 1);
                    return -1;
                }
                break;
            case 2:
                if (bstr_cmpc(h->name, "Normal-Header") != 0) {
                    printf("Header %i incorrect name\n", count + 1);
                    return -1;
                }
                if (bstr_cmpc(h->value, "3") != 0) {
                    printf("Header %i incorrect value\n", count + 1);
                    return -1;
                }
                break;
            case 3:
                if (bstr_cmpc(h->name, "Invalid Header Name") != 0) {
                    printf("Header %i incorrect name\n", count + 1);
                    return -1;
                }
                if (bstr_cmpc(h->value, "4") != 0) {
                    printf("Header %i incorrect value\n", count + 1);
                    return -1;
                }
                break;
            case 4:
                if (bstr_cmpc(h->name, "Same-Name-Headers") != 0) {
                    printf("Header %i incorrect name\n", count + 1);
                    return -1;
                }
                if (bstr_cmpc(h->value, "5, 6") != 0) {
                    printf("Header %i incorrect value\n", count + 1);
                    return -1;
                }
                break;
            case 5:
                if (bstr_cmpc(h->name, "Empty-Value-Header") != 0) {
                    printf("Header %i incorrect name\n", count + 1);
                    return -1;
                }
                if (bstr_cmpc(h->value, "") != 0) {
                    printf("Header %i incorrect value\n", count + 1);
                    return -1;
                }
                break;
            case 6:
                if (bstr_cmpc(h->name, "") != 0) {
                    printf("Header %i incorrect name\n", count + 1);
                    return -1;
                }
                if (bstr_cmpc(h->value, "8, ") != 0) {
                    printf("Header %i incorrect value\n", count + 1);
                    return -1;
                }
                break;
            case 7:
                if (bstr_cmpc(h->name, "Header-With-LWS-After") != 0) {
                    printf("Header %i incorrect name\n", count + 1);
                    return -1;
                }
                if (bstr_cmpc(h->value, "9") != 0) {
                    printf("Header %i incorrect value\n", count + 1);
                    return -1;
                }
                break;
            case 8:
            {
                bstr *b = bstr_memdup("BEFORE", 6);
                if (bstr_cmpc(h->name, "Header-With-NUL") != 0) {
                    printf("Header %i incorrect name\n", count + 1);
                    return -1;
                }
                if (bstr_cmp(h->value, b) != 0) {
                    printf("Header %i incorrect value\n", count + 1);
                    return -1;
                }
            }
                break;
        }

        count++;
    }

    htp_connp_destroy(connp);

    return 1;
}

int callback_transaction_start(htp_connp_t *connp) {
    printf("-- Callback: transaction_start\n");
}

int callback_request_line(htp_connp_t *connp) {
    printf("-- Callback: request_line\n");
}

int callback_request_headers(htp_connp_t *connp) {
    printf("-- Callback: request_headers\n");
}

int callback_request_body_data(htp_tx_data_t *d) {
    printf("-- Callback: request_body_data: [%s] %i\n", bstr_memtocstr(d->data, d->len), d->len);
}

int callback_request_trailer(htp_connp_t *connp) {
    printf("-- Callback: request_trailer\n");
}

int callback_request(htp_connp_t *connp) {
    printf("-- Callback: request\n");
}

int callback_response_line(htp_connp_t *connp) {
    printf("-- Callback: response_line\n");
}

int callback_response_headers(htp_connp_t *connp) {
    printf("-- Callback: response_headers\n");
}

int callback_response_body_data(htp_tx_data_t *d) {
    printf("-- Callback: response_body_data: [%s] %i\n", bstr_memtocstr(d->data, d->len), d->len);
}

int callback_response_trailer(htp_connp_t *connp) {
    printf("-- Callback: response_trailer\n");
}

int callback_response(htp_connp_t *connp) {
    printf("-- Callback: response\n");
}

#define RUN_TEST(X, Y) \
    {\
    tests++; \
    printf("---------------------------------\n"); \
    printf("Test: " #X "\n"); \
    int rc = X(Y); \
    if (rc < 0) { \
        printf("    Failed with %i\n", rc); \
        failures++; \
    } \
    printf("\n"); \
    }

/**
 * Entry point; runs a bunch of tests and exits.
 */
int main(int argc, char** argv) {
    char buf[1025];
    int tests = 0, failures = 0;

    home = NULL;

    // Try the current working directory first
    int fd = open("./files/anchor.empty", 0, O_RDONLY);
    if (fd != -1) {
        close(fd);
        home = "./files";
    } else {
        // Try the directory in which the executable resides
        strncpy(buf, argv[0], 1024);
        strncat(buf, "/../files/anchor.empty", 1024 - strlen(buf));
        fd = open(buf, 0, O_RDONLY);
        if (fd != -1) {
            close(fd);
            strncpy(buf, argv[0], 1024);
            strncat(buf, "/../files", 1024 - strlen(buf));
            home = buf;
        } else {
            // Try the directory in which the executable resides
            strncpy(buf, argv[0], 1024);
            strncat(buf, "/../../files/anchor.empty", 1024 - strlen(buf));
            fd = open(buf, 0, O_RDONLY);
            if (fd != -1) {
                close(fd);
                strncpy(buf, argv[0], 1024);
                strncat(buf, "/../../files", 1024 - strlen(buf));
                home = buf;
            }
        }
    }

    if (home == NULL) {
        printf("Failed to find test files.");
        exit(-1);
    }

    htp_cfg_t *cfg = htp_config_create();

    // Register hooks
    htp_config_register_transaction_start(cfg, callback_transaction_start, HOOK_MIDDLE);

    htp_config_register_request_line(cfg, callback_request_line, HOOK_MIDDLE);
    htp_config_register_request_headers(cfg, callback_request_headers, HOOK_MIDDLE);
    htp_config_register_request_body_data(cfg, callback_request_body_data, HOOK_MIDDLE);
    htp_config_register_request_trailer(cfg, callback_request_trailer, HOOK_MIDDLE);
    htp_config_register_request(cfg, callback_request, HOOK_MIDDLE);

    htp_config_register_response_line(cfg, callback_response_line, HOOK_MIDDLE);
    htp_config_register_response_headers(cfg, callback_response_headers, HOOK_MIDDLE);
    htp_config_register_response_body_data(cfg, callback_response_body_data, HOOK_MIDDLE);
    htp_config_register_response_trailer(cfg, callback_response_trailer, HOOK_MIDDLE);
    htp_config_register_response(cfg, callback_response, HOOK_MIDDLE);

    RUN_TEST(test_get, cfg);
    RUN_TEST(test_apache_header_parsing, cfg);
    RUN_TEST(test_post_urlencoded, cfg);
    RUN_TEST(test_post_urlencoded_chunked, cfg);

    printf("Tests: %i\n", tests);
    printf("Failures: %i\n", failures);

    return (EXIT_SUCCESS);
}


