
#ifndef _TEST_H
#define	_TEST_H

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>

#define UNKNOWN     0
#define CLIENT      1
#define SERVER      2

#ifndef O_BINARY
#define O_BINARY    0
#endif

typedef struct test_t test_t;

struct test_t {
    char *buf;
    size_t pos;
    size_t len;

    char *chunk;
    size_t chunk_offset;
    size_t chunk_len;
    int chunk_direction;
};

int test_run(const char *testsdir, const char *testname, htp_cfg_t *cfg, htp_connp_t **connp);

#endif	/* _TEST_H */

