#include "ironbee/kvstore.h"
#include "ironbee/kvstore_filesystem.h"

#include "ironbee/debug.h"

#include <string.h>

ib_status_t kvstore_filesystem_init(kvstore_t* kvstore, const char* directory)
{
    IB_FTRACE_INIT();

    kvstore_init(kvstore);

    kvstore_filesystem_server_t *server = malloc(sizeof(*server));

    if ( server == NULL ) {
        return IB_EALLOC;
    }

    server->directory = strdup(directory);

    if ( server->directory == NULL ) {
        free(server);
        return IB_EALLOC;
    }

    kvstore->server = (kvstore_server_t)server;

    IB_FTRACE_RET_STATUS(IB_OK);
}

void kvstore_filesystem_destroy(kvstore_t* kvstore)
{
    IB_FTRACE_INIT();

    free(kvstore->server);
    kvstore->server = NULL;

    IB_FTRACE_RET_VOID();
}
