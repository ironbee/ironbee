#include "ironbee/kvstore.h"
#include "ironbee/kvstore_filesystem.h"

#include "ironbee/debug.h"
#include "ironbee/clock.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

/**
 * Define the width for printing a @c time_t field. 
 * This is related to TIME_T_STR_FMT.
 * Both are 12 to accommodate the typical 10 digits and 2 buffer digits
 * for extreme future-time use.
 */
#define TIME_T_STR_WIDTH 13

/**
 * The sprintf format used for @c time_t types.
 */
#define TIME_T_STR_FMT "%012u"


/**
 * Malloc and populate a filesystem path for a key/value pair.
 *
 * The path will include the key value, the expiration value, and the type
 * of the pattern:
 * @c &lt;base_path&gt;/&lt;key&gt;/&lt;expiration&gt;-&lt;type&gt;.
 *
 * @param[in] kvstore Key-Value store.
 * @param[in] key The key to write to.
 * @param[in] expiration The expiration in seconds. This is ignored
 *            if the argument type is NULL.
 * @param[in] type The type of the file. If null then expiration is
 *            ignored and a shortened path is generated 
 *            representing only the directory.
 * @param[in] type_len The type length of the type_len. This value is ignored
 *            if expiration is < 0.
 * @param[out] path The malloc'ed path. The caller must free this.
 *             The path variable will be set to NULL if a failure
 *             occurs after its memory has been allocated.
 *
 * @return
 *   - IB_OK on success
 *   - IB_EOTHER on system call failure. See @c errno.
 *   - IB_EALLOC if a memory allocation fails.
 */
static ib_status_t build_key_path(
    kvstore_t *kvstore,
    const kvstore_key_t *key,
    uint32_t expiration,
    char *type,
    size_t type_len,
    char **path)
{
    IB_FTRACE_INIT();

    /* System return code. */
    int sys_rc;

    /* Used to compute expiration in absolute time. */
    ib_timeval_t ib_timeval;

    /* Epoch seconds after which this entry should expire. */
    uint32_t seconds;

    /* A stat struct. sb is the name used in the man page example code. */
    struct stat sb;

    kvstore_filesystem_server_t *server =
        (kvstore_filesystem_server_t *)(kvstore->server);

    size_t path_size =
        server->directory_length /* length of path */
        + 1                      /* path separator */
        + key->length            /* key length */
        + 1                      /* path separator */
        + IB_CLOCK_FMT_WIDTH     /* width to format a clock timestamp. */
        + 1                      /* dot. */
        + type_len               /* type. */
        + 1                      /* '\0' */;

    char *path_tmp = (char *) kvstore->malloc(kvstore, path_size+1, kvstore->cbdata);

    if ( ! path_tmp ) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    /* Push allocated path back to user. We now populate it. */
    *path = path_tmp;

    /* Append the path to the directory. */
    path_tmp = strncpy(path_tmp, server->directory, server->directory_length)
               + server->directory_length;

    /* Append the key. */
    path_tmp = strncpy(path_tmp, "/", 1) + 1;
    path_tmp = strncpy(path_tmp, key->key, key->length) + key->length;

    /* Momentarily tag the end of the path for the stat check. */
    *path_tmp = '\0';
    errno = 0;
    /* Check for a key directory. Make one if able.*/
    sys_rc = stat(*path, &sb);
    if (errno == ENOENT) {
        sys_rc = mkdir(*path, 0700);

        if (sys_rc) {
            goto eother_failure;
        }
    }
    else if (sys_rc) {
        goto eother_failure;
    }
    else if (!S_ISDIR(sb.st_mode)) {
        goto eother_failure;
    }

    if ( type ) {
        if ( expiration > 0 ) {
            ib_clock_gettimeofday(&ib_timeval);

            seconds = ib_timeval.tv_sec + expiration;
        }
        else {
            seconds = 0;
        }

        path_tmp = strncpy(path_tmp, "/", 1) + 1;
        path_tmp += snprintf(
            path_tmp,
            TIME_T_STR_WIDTH,
            TIME_T_STR_FMT,
            seconds);
        path_tmp = strncpy(path_tmp, ".", 1) + 1;
        path_tmp = strncpy(path_tmp, type, type_len) + type_len;
    }

    *path_tmp = '\0';

    IB_FTRACE_RET_STATUS(IB_OK);

eother_failure:
    kvstore->free(kvstore, *path, kvstore->cbdata);
    *path = NULL;
    IB_FTRACE_RET_STATUS(IB_EOTHER);
}

static ib_status_t kvconnect(
    kvstore_server_t server,
    ib_kvstore_cbdata_t *cbdata)
{
    IB_FTRACE_INIT();

    /* Nop. */

    IB_FTRACE_RET_STATUS(IB_OK);
}

static ib_status_t kvdisconnect(
    kvstore_server_t server,
    ib_kvstore_cbdata_t *cbdata)
{
    IB_FTRACE_INIT();
    
    /* Nop. */

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * Read a whole file into data.
 *
 * @param[in] kvstore The key-value store.
 * @param[in] path The path to the file.
 * @param[out] data A kvstore->malloc'ed array containing the file data.
 * @param[out] len The length of the data buffer.
 *
 * @returns
 *   - IB_OK on ok.
 *   - IB_EALLOC on memory allocation error.
 *   - IB_EOTHER on system call failure.
 */
static ib_status_t read_whole_file(
    kvstore_t *kvstore,
    const char *path,
    void **data,
    size_t *len)
{
    IB_FTRACE_INIT();

    struct stat sb;
    int sys_rc;
    int fd = -1;
    char *dataptr;
    char *dataend;

    sys_rc = stat(path, &sb);
    if (sys_rc) {
        IB_FTRACE_RET_STATUS(IB_EOTHER);
    }

    dataptr = (char *)kvstore->malloc(kvstore, sb.st_size, kvstore->cbdata);
    if (!dataptr) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }
    *len = sb.st_size;
    *data = dataptr;

    fd = open(path, O_RDONLY);
    if (fd < 0) {
        goto eother_failure;
    }

    for (dataend = dataptr + sb.st_size; dataptr < dataend; )
    {
        ssize_t fr_tmp = read(fd, dataptr, dataend - dataptr);

        if (fr_tmp < 0) {
            goto eother_failure;
        }

        dataptr += fr_tmp;
    }

    close(fd);
    
    IB_FTRACE_RET_STATUS(IB_OK);

eother_failure:
    if (fd >= 0) {
        close(fd);
    }
    kvstore->free(kvstore, *data, kvstore->cbdata);
    *data = NULL;
    *len = 0;
    IB_FTRACE_RET_STATUS(IB_EOTHER);
}

static ib_status_t extract_type(
    kvstore_t *kvstore,
    const char *path,
    char **type,
    size_t *type_length)
{
    IB_FTRACE_INIT();

    const char *start;
    size_t len;

    start = rindex(path, '.')+1;
    if (!start) {
        IB_FTRACE_RET_STATUS(IB_ENOENT);
    }

    len = strlen(start);
    *type = (char *) kvstore->malloc(kvstore, len+1, kvstore->cbdata);
    if (!*type) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }
    strncpy(*type, start, len);
    *type_length = len;
    
    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * @returns
 *   - IB_OK on success.
 *   - IB_EALLOC on memory allocation failures.
 *   - IB_ENOENT if a / and . characters are not found to denote
 *               the location of the expiration decimals.
 */
static ib_status_t extract_expiration(
    kvstore_t *kvstore,
    const char *path,
    uint32_t *expiration)
{
    IB_FTRACE_INIT();

    const char *start;
    const char *stop;
    char *substr;

    start = rindex(path, '/')+1;
    if (!start) {
        IB_FTRACE_RET_STATUS(IB_ENOENT);
    }
    stop = index(start, '.');
    if (!stop) {
        IB_FTRACE_RET_STATUS(IB_ENOENT);
    }
    substr = strndup(start, stop-start);
    if (!substr) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    *expiration = atoll(substr);

    free(substr);
    
    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * @param[in] kvstore Key-value store.
 * @param[in] file The file name.
 * @param[out] value The value to be built.
 * @returns
 *   - IB_OK on success.
 *   - IB_EALLOC on memory failure from @c kstore->malloc
 *   - IB_EOTHER on system call failure from file operations.
 *   - IB_ENOTENT returned when a value is found to be expired.
 */
static ib_status_t load_kv_value(
    kvstore_t *kvstore,
    const char *file,
    kvstore_value_t **value)
{
    IB_FTRACE_INIT();

    ib_status_t rc;
    ib_timeval_t ib_timeval;
    
    ib_clock_gettimeofday(&ib_timeval);

    /* Use kvstore->malloc because of framework contractual requirements. */
    *value = kvstore->malloc(kvstore, sizeof(**value), kvstore->cbdata);
    if (!*value) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    /* Populate expiration. */
    rc = extract_expiration(kvstore, file, &((*value)->expiration));
    if (rc) {
        kvstore->free(kvstore, *value, kvstore->cbdata);
        *value = NULL;
        IB_FTRACE_RET_STATUS(IB_EOTHER);
    }

    /* Remove expired file and signal there is no entry for that file. */
    if ((*value)->expiration < ib_timeval.tv_sec) {

        /* Remove the expired file. */
        unlink(file);

        /* Try to remove the key directory, though it may not be empty.
         * Failure is OK. */
        rmdir(file);
        kvstore->free(kvstore, *value, kvstore->cbdata);
        *value = NULL;
        IB_FTRACE_RET_STATUS(IB_ENOENT);
    }

    /* Populate type and type_length. */
    rc = extract_type(
        kvstore,
        file,
        &((*value)->type),
        &((*value)->type_length));

    if (rc) {
        kvstore->free(kvstore, *value, kvstore->cbdata);
        *value = NULL;
        IB_FTRACE_RET_STATUS(IB_EOTHER);
    }

    /* Populate value and value_length. */
    rc = read_whole_file(
        kvstore,
        file,
        &((*value)->value),
        &((*value)->value_length));

    if (rc) {
        kvstore->free(kvstore, (*value)->type, kvstore->cbdata);
        kvstore->free(kvstore, *value, kvstore->cbdata);
        *value = NULL;
        IB_FTRACE_RET_STATUS(IB_EOTHER);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

typedef ib_status_t(*each_dir_t)(const char *path, const char *dirent, void*);

/**
 * Count the entries that do not begin with a dot.
 *
 * @param[in] path The directory path.
 * @param[in] dirent The directory entry in path.
 * @param[in,out] data An int* representing the current count of directories.
 *
 * @returns IB_OK.
 */
static ib_status_t count_dirent(
    const char *path,
    const char *dirent,
    void *data)
{
    IB_FTRACE_INIT();

    size_t *i = (size_t *)data;

    if (strncmp(".", dirent, 1)) {
        ++(*i);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * Iterate through a directory in a standard way.
 *
 * @returns
 *   - IB_OK on success.
 *   - Other errors based on each_dir_t.
 */
static ib_status_t each_dir(const char *path, each_dir_t f, void* data)
{
    IB_FTRACE_INIT();

    int tmp_errno; /* Holds errno until other system calls finish. */
    int sys_rc;
    ib_status_t rc;

    size_t dirent_sz;      /* Size of a dirent structure. */
    DIR *dir = NULL;
    struct dirent *entry = NULL;
    struct dirent *result = NULL;

    dir = opendir(path);

    if (!dir) {
        rc = IB_EOTHER;
        goto rc_failure;
    }

    /* Man page-specified to portably allocate the dirent buffer for entry. */
    dirent_sz = (offsetof(struct dirent, d_name) +
                pathconf(path, _PC_NAME_MAX) + 1 + sizeof(long))
                & -sizeof(long);

    entry = malloc(dirent_sz);

    if (!entry) {
        rc = IB_EOTHER;
        goto rc_failure;
    }

    while (1) {
        sys_rc = readdir_r(dir, entry, &result);
        if (sys_rc) {
            rc = IB_EOTHER;
            goto rc_failure;
        }
        if (!result) {
            break;
        }
        rc = f(path, result->d_name, data);
        if (rc) {
            goto rc_failure;
        }
    }

    /* Clean exit. */
    closedir(dir);
    free(entry);
    IB_FTRACE_RET_STATUS(IB_OK);

rc_failure:
    tmp_errno = errno;
    if (dir) {
        closedir(dir);
    }
    if (entry) {
        free(entry);
    }
    errno = tmp_errno;
    IB_FTRACE_RET_STATUS(rc);
}

/**
 * Callback user data structure for build_value callback function.
 */
struct build_value_t {
    kvstore_t *kvstore;        /**< Key value store. */
    kvstore_value_t **values;  /**< Values array to be build. */
    size_t values_idx;         /**< Next value to be populated. */
    size_t values_len;         /**< Prevent new file causing array overflow. */
    size_t path_len;           /**< Cached path length value. */
};
typedef struct build_value_t build_value_t;

/**
 * Build an array of values.
 *
 * @param[in] path The directory path.
 * @param[in] file The file to load.
 * @param[out] data The data structure to be populated.
 * @returns 
 *   - IB_OK
 *   - IB_EALLOC On a memory errror.
 */
static ib_status_t build_value(const char *path, const char *file, void *data)
{
    IB_FTRACE_INIT();

    char *full_path;
    ib_status_t rc;
    build_value_t *bv = (build_value_t*)(data);

    /* Return if there is no space left in our array.
     * Partial results are not an error as an asynchronous write may
     * create a new file. */
    if (bv->values_idx >= bv->values_len) {
        IB_FTRACE_RET_STATUS(IB_OK);
    }

    if (strncmp(".", file, 1)) {
        /* Build full path. */
        full_path = malloc(bv->path_len + strlen(file) + 2);
        if (!full_path) {
            IB_FTRACE_RET_STATUS(IB_EALLOC);
        }
        sprintf(full_path, "%s/%s", path, file);

        rc = load_kv_value(
            bv->kvstore,
            full_path,
            bv->values + bv->values_idx);

        /* If IB_ENOENT, a file was expired on get. */
        if (!rc) {
            bv->values_idx++;
        }

        free(full_path);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

static ib_status_t kvget(
    kvstore_t *kvstore,
    const kvstore_key_t *key,
    kvstore_value_t ***values,
    size_t *values_length,
    ib_kvstore_cbdata_t *cbdata)
{
    IB_FTRACE_INIT();

    ib_status_t rc;
    build_value_t build_val;
    char *path = NULL;
    size_t dirent_count = 0;

    /* Build a path with no expiration value on it. */
    rc = build_key_path(kvstore, key, -1, NULL, 0, &path);
    if (rc) {
        goto failure1;
    }

    /* Count entries. */
    rc = each_dir(path, &count_dirent, &dirent_count);
    if (rc) {
        goto failure1;
    }
    if (dirent_count==0){
        rc = IB_ENOENT;
        goto failure1;
    }

    /* Initialize build_values user data. */
    build_val.kvstore = kvstore;
    build_val.path_len = strlen(path);
    build_val.values_idx = 0;
    build_val.values_len = dirent_count;
    build_val.values = (kvstore_value_t**)
        kvstore->malloc(kvstore, sizeof(*build_val.values) * dirent_count, kvstore->cbdata);

    /* Build value array. */
    rc = each_dir(path, &build_value, &build_val);
    if (rc) {
        goto failure2;
    }

    /* Commit back results.
     * NOTE: values_idx does not need a +1 because it points at the
     *       next empty slot in the values array. */
    *values = build_val.values;
    *values_length = build_val.values_idx;

    /* Clean exit. */
    free(path);
    IB_FTRACE_RET_STATUS(IB_OK);

    /**
     * Reverse initialization error labels.
     */
failure2:
    kvstore->free(kvstore, build_val.values, kvstore->cbdata);
failure1:
    if (path) {
        free(path);
    }
    IB_FTRACE_RET_STATUS(rc);
}

/**
 * Set callback.
 *
 * @param[in] kvstore Key-value store.
 * @param[in] merge_policy This implementation does not merge on writes.
 *            Merging is done by the framework on reads.
 * @param[in] key The key to fetch all values for.
 * @param[in] value The value to write. The framework contract says that this 
 *            is also an out-parameters, but in this implementation the
 *            merge_policy is not used so value is never merged
 *            and never written to.
 * @param[in,out] cbdata Callback data for the user.
 */
static ib_status_t kvset(
    kvstore_t *kvstore,
    kvstore_merge_policy_fn_t merge_policy,
    const kvstore_key_t *key,
    kvstore_value_t *value,
    ib_kvstore_cbdata_t *cbdata)
{
    IB_FTRACE_INIT();

    ib_status_t rc;
    char *path = NULL;
    char *tmp_path = NULL;
    int tmp_fd;
    int sys_rc;
    ssize_t written;

    /* Build a path with no expiration value on it. */
    rc = build_key_path(
        kvstore,
        key,
        value->expiration,
        value->type,
        value->type_length,
        &path);
    if (rc) {
        goto error_1;
    }

    rc = build_key_path(
        kvstore,
        key,
        -1,
        NULL,
        0,
        &tmp_path);
    if (rc) {
        goto error_2;
    }

    tmp_path = realloc(tmp_path, strlen(tmp_path) + 12 + 1);
    if (!tmp_path) {
        rc = IB_EALLOC;
        goto error_2;
    }
    strcat(tmp_path, "/.tmpXXXXXX");
    tmp_fd = mkstemp(tmp_path);
    if (tmp_fd < 0) {
        rc = IB_EOTHER;
        goto error_2;
    }

    /* Write to the tmp file. */
    written = write(tmp_fd, value->value, value->value_length);
    if (written < (ssize_t)value->value_length ){
        rc = IB_EOTHER;
        goto error_2;
    }

    /* Atomically rename to the tmp file to the real file name. */
    sys_rc = rename(tmp_path, path);
    if (sys_rc) {
        rc = IB_EOTHER;
        goto error_2;
    }

    rc = IB_OK;
error_2:
    free(tmp_path);
error_1:
    free(path);
    IB_FTRACE_RET_STATUS(rc);
}

/**
 * Remove all entries from key directory.
 *
 * @param[in] path The path to the key directory. @c rmdir is called on this.
 * @param[in] file The file inside of the directory pointed to by path
 *            which will be @c unlink'ed.
 * @param[in,out] cbdata Callback data. This is a size_t pointer containing
 *                the length of the path argument.
 * @returns
 *   - IB_OK
 *   - IB_EALLOC if a memory allocation fails concatinating path and file.
 */
static ib_status_t remove_file(
    const char *path,
    const char *file,
    ib_kvstore_cbdata_t *cbdata)
{
    IB_FTRACE_INIT();

    char *full_path;
    size_t path_len = *(size_t*)(data);

    if (strncmp(".", file, 1)) {
        /* Build full path. */
        full_path = malloc(path_len + strlen(file) + 2);

        if (!full_path) {
            IB_FTRACE_RET_STATUS(IB_EALLOC);
        }

        sprintf(full_path, "%s/%s", path, file);

        unlink(full_path);

        free(full_path);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * Remove a key from the store.
 *
 * @param[in] kvstore
 * @param[in] key
 * @param[in,out] cbdata Callback data.
 * @returns
 *   - IB_OK on success.
 *   - IB_EOTHER on system call failure in build_key_path. See @c errno.
 *   - IB_EALLOC if a memory allocation fails.
 */
static ib_status_t kvremove(
    kvstore_t *kvstore,
    const kvstore_key_t *key,
    ib_kvstore_cbdata_t *cbdata)
{
    IB_FTRACE_INIT();
    
    ib_status_t rc;
    char *path = NULL;
    size_t path_len;

    /* Build a path with no expiration value on it. */
    rc = build_key_path(kvstore, key, -1, NULL, 0, &path);
    if (rc) {
        IB_FTRACE_RET_STATUS(rc);
    }

    path_len = strlen(path);

    /* Remove all files in the key directory. */
    each_dir(path, remove_file, &path_len);

    /* Attempt to remove the key path. This may fail, and that is OK.
     * Another process may write a new key while we were deleting old ones. */
    rmdir(path);

    free(path);

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * Initializes the kvstore to point at the given directory.
 * Directory is copied and free'd on kvstore_filesystem_destroy.
 *
 * @param[out] kvstore Build this kvstore object.
 * @param[in] directory Directory kvstore points at.
 * @param[in,out] cbdata Callback data.
 * @returns
 *   - IB_OK success.
 *   - IB_EALLOC Memory allocation errors.
 */
ib_status_t kvstore_filesystem_init(
    kvstore_t* kvstore,
    const char* directory,
    ib_kvstore_cbdata_t *cbdata)
{
    IB_FTRACE_INIT();

    kvstore_init(kvstore, cbdata);

    kvstore_filesystem_server_t *server = malloc(sizeof(*server));

    if ( server == NULL ) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    server->directory = strdup(directory);
    server->directory_length = strlen(directory);

    if ( server->directory == NULL ) {
        free(server);
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    kvstore->server = (kvstore_server_t) server;
    kvstore->get = kvget;
    kvstore->set = kvset;
    kvstore->remove = kvremove;
    kvstore->connect = kvconnect;
    kvstore->disconnect = kvdisconnect;

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * Destroy any allocated elements of the kvstore structure.
 * @param[out] kvstore to be destroyed. The contents on disk is untouched
 *             and another init of kvstore pointing at that directory
 *             will operate correctly.
 */
void kvstore_filesystem_destroy(kvstore_t* kvstore)
{
    IB_FTRACE_INIT();

    kvstore_filesystem_server_t *server =
        (kvstore_filesystem_server_t*)(kvstore->server);
    free((void *)server->directory);
    free(server);
    kvstore->server = NULL;

    IB_FTRACE_RET_VOID();
}
