
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>

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

static void print_tx(htp_connp_t *connp, htp_tx_t *tx) {
    char *request_line = bstr_tocstr(tx->request_line);
    htp_header_t *h_user_agent = table_getc(tx->request_headers, "user-agent");
    htp_header_t *h_referer = table_getc(tx->request_headers, "referer");
    char *referer, *user_agent;
    char buf[256];

    time_t t = time(NULL);
    struct tm *tmp = localtime(&t);

    strftime(buf, 255, "%d/%b/%Y:%T %z", tmp);

    if (h_user_agent == NULL) user_agent = strdup("-");
    else {
        user_agent = bstr_tocstr(h_user_agent->value);
    }

    if (h_referer == NULL) referer = strdup("-");
    else {
        referer = bstr_tocstr(h_referer->value);
    }

    printf("%s - - [%s] \"%s\" %i %i \"%s\" \"%s\"\n", connp->conn->remote_addr, buf,
        request_line, tx->response_status_number, tx->response_body_len_actual,
        referer, user_agent);

    free(referer);
    free(user_agent);
    free(request_line);
}

static int run_directory(char *dirname, htp_cfg_t *cfg) {
    struct dirent *entry;
    char buf[1025];
    DIR *d = opendir(dirname);
    htp_connp_t *connp;

    if (d == NULL) {
        printf("Failed to open directory: %s\n", dirname);
        return -1;
    }

    while ((entry = readdir(d)) != NULL) {
        if (strncmp(entry->d_name, "stream", 6) == 0) {
            //strncpy(buf, dirname, 1024);
            //strncat(buf, "/", 1024 - strlen(buf));
            //strncat(buf, entry->d_name, 1024 - strlen(buf));

            int rc = test_run(dirname, entry->d_name, cfg, &connp);

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
                printf(" -- %i transaction(s)\n", list_size(connp->conn->transactions));

                htp_tx_t *tx = NULL;
                list_iterator_reset(connp->conn->transactions);
                while ((tx = list_iterator_next(connp->conn->transactions)) != NULL) {
                    printf("    ");
                    print_tx(connp, tx);
                }

                printf("\n");

                htp_connp_destroy_all(connp);
            }
        }
    }

    closedir(d);

    return 1;
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
int main2(int argc, char** argv) {
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

int main(int argc, char** argv) {
    htp_cfg_t *cfg = htp_config_create();
    run_directory("c:/http_traces/run1/", cfg);
}

