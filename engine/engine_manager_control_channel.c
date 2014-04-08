/*****************************************************************************
 * Licensed to Qualys, Inc. (QUALYS) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * QUALYS licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 ****************************************************************************/

/**
 * @file
 * @brief IronBee --- Engine Manager Control Channel
 *
 * @author Sam Baskinger <sbaskinger@qualys.com>
 */

#include <ironbee/engine_manager_control_channel.h>
#include <ironbee/engine_manager.h>

#include <ironbee/hash.h>
#include <ironbee/mpool_lite.h>
#include <ironbee/mm.h>
#include <ironbee/mm_mpool_lite.h>

#include <assert.h>
#include <errno.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

//! Basename of the socket file.
#define DEFAULT_SOCKET_BASENAME "ironbee_manager_controller.sock"

#ifdef ENGINE_MANAGER_CONTROL_SOCK_PATH
//! The directory that the @ref DEFAULT_SOCKET_BASENAME will be created in.
static const char *DEFAULT_SOCKET_PATH =
    IB_XSTRINGIFY(ENGINE_MANAGER_CONTROL_SOCK_PATH) "/" DEFAULT_SOCKET_BASENAME;
#else
//! The directory that the @ref DEFAULT_SOCKET_BASENAME will be created in.
static const char *DEFAULT_SOCKET_PATH = "/var/run/" DEFAULT_SOCKET_BASENAME;
#endif



/**
 * Structure to hold and manipulate pointers to command implementations.
 */
struct cmd_t {
    ib_engine_manager_control_channel_cmd_fn_t fn; /**< The command. */
    void       *cbdata; /**< Callback data. */
    const char *name;   /**< The command name this is registered under. */
};
typedef struct cmd_t cmd_t;

struct ib_engine_manager_control_channel_t {
    ib_mm_t       mm;        /**< Memory manager. */
    ib_manager_t *manager;   /**< The manager we will be controlling. */
    const char   *sock_path; /**< The path to the socket file. */
    int           sock;      /**< Socket. -1 If not open. */
    /*! Message buffer. */
    uint8_t       msg[IB_ENGINE_MANAGER_CONTROL_CHANNEL_MAX_MSG_SZ+1];
    size_t        msgsz;     /**< How much data is in msg. */
    /**
     * Collection of @ref cmd_t structures with command names as the index.
     */
    ib_hash_t    *cmds;
};

/**
 * Cleanup the manager controller.
 *
 * @param[in] cbdata The @ref ib_engine_manager_control_channel_t created by
 *            ib_engine_manager_control_channel_create().
 */
static void channel_cleanup(void *cbdata) {
    assert(cbdata != NULL);

    ib_engine_manager_control_channel_t *manager_controller =
        (ib_engine_manager_control_channel_t *)cbdata;

    ib_engine_manager_control_channel_stop(manager_controller);
}

/**
 * Echo command.
 *
 * @param[in] mm Memory manager for allocations of @a result and other
 *            allocations that should live until the response is sent.
 * @param[in] name The name this command is called by.
 * @param[in] args The command arguments.
 * @param[out] result This is set to @a args.
 * @param[in] cbdata Callback data. Unused.
 *
 * @returns IB_OK.
 */
static ib_status_t echo_cmd(
    ib_mm_t      mm,
    const char  *name,
    const char  *args,
    const char **result,
    void        *cbdata
)
{
    assert(args != NULL);

    *result = args;

    return IB_OK;
}

/**
 * Disable manager command.
 *
 * Calls ib_manager_disable().
 *
 * @param[in] mm Memory manager for allocations of @a result and other
 *            allocations that should live until the response is sent.
 * @param[in] name The name this command is called by.
 * @param[in] args The command arguments. Ignored.
 * @param[out] result This is unchanged.
 * @param[in] cbdata The @ref ib_manager_t * to act on.
 *
 * @sa ib_manager_disable()
 *
 * @returns The return of ib_manager_disable().
 */
static ib_status_t manager_cmd_disable(
    ib_mm_t      mm,
    const char  *name,
    const char  *args,
    const char **result,
    void        *cbdata
)
{
    assert(cbdata != NULL);

    ib_manager_t *manager = (ib_manager_t *)cbdata;

    return ib_manager_disable(manager);
}

/**
 * Enable manager command.
 *
 * Calls ib_manager_enable().
 *
 * @param[in] mm Memory manager for allocations of @a result and other
 *            allocations that should live until the response is sent.
 * @param[in] name The name this command is called by.
 * @param[in] args The command arguments. Ignored.
 * @param[out] result This is unchanged.
 * @param[in] cbdata The @ref ib_manager_t * to act on.
 *
 * @sa ib_manager_enable()
 *
 * @returns The return of ib_manager_enable().
 */
static ib_status_t manager_cmd_enable(
    ib_mm_t      mm,
    const char  *name,
    const char  *args,
    const char **result,
    void        *cbdata
)
{
    assert(args != NULL);
    assert(cbdata != NULL);

    ib_manager_t *manager = (ib_manager_t *)cbdata;

    return ib_manager_enable(manager);
}

/**
 * Call ib_manager_engine_create(); Args is the path to the config file.
 *
 * @param[in] mm Memory manager for allocations of @a result and other
 *            allocations that should live until the response is sent.
 * @param[in] name The name this command is called by.
 * @param[in] args The path to the configuration file to use.
 * @param[out] result This is unchanged.
 * @param[in] cbdata The @ref ib_manager_t * to act on.
 *
 * @sa ib_manager_engine_create()
 *
 * @returns The return of ib_manager_engine_create().
 */
static ib_status_t manager_cmd_engine_create(
    ib_mm_t      mm,
    const char  *name,
    const char  *args,
    const char **result,
    void        *cbdata
)
{
    assert(args != NULL);
    assert(cbdata != NULL);

    ib_manager_t *manager = (ib_manager_t *)cbdata;

    return ib_manager_engine_create(manager, args);
}

/**
 * Call ib_manager_engine_cleanup().
 *
 * @param[in] mm Memory manager for allocations of @a result and other
 *            allocations that should live until the response is sent.
 * @param[in] name The name this command is called by.
 * @param[in] args Unused.
 * @param[out] result This is unchanged.
 * @param[in] cbdata The @ref ib_manager_t * to act on.
 *
 * @sa ib_manager_engine_cleanup()
 *
 * @returns The return of ib_manager_engine_cleanup().
 */
static ib_status_t manager_cmd_cleanup(
    ib_mm_t      mm,
    const char  *name,
    const char  *args,
    const char **result,
    void        *cbdata
)
{
    assert(cbdata != NULL);

    ib_manager_t *manager = (ib_manager_t *)cbdata;

    return ib_manager_engine_cleanup(manager);
}

/**
 * Log an error message through the current IronBee engine.
 *
 * If there is no active engine available, stderr is used.
 *
 * This function exists to make consistent the slightly complex task
 * of fetching an @ref ib_engine_t and logging to it and returning it to
 * the engine manager.
 *
 * @param[in] channel The channel that we are logging about.
 * @param[in] action The action that failed to be performed.
 * @param[in] msg The message that describes the failure of the @a action.
 */
static void log_socket_error(
    ib_engine_manager_control_channel_t *channel,
    const char *action,
    const char *msg
)
{
    assert(channel != NULL);
    assert(channel->manager != NULL);

    ib_engine_t *ib;
    ib_status_t  rc;

    rc = ib_manager_engine_acquire(channel->manager, &ib);
    if (rc == IB_OK) {
        ib_log_error(
            ib,
            "Failed to %s socket %s: %s",
            action,
            channel->sock_path,
            msg
        );
        ib_manager_engine_release(channel->manager, ib);
    }
}

ib_status_t ib_engine_manager_control_channel_create(
    ib_engine_manager_control_channel_t **channel,
    ib_mm_t                               mm,
    ib_manager_t                         *manager
)
{
    assert(channel != NULL);
    assert(manager != NULL);

    ib_status_t                          rc;
    ib_engine_manager_control_channel_t *mc;

    mc = (ib_engine_manager_control_channel_t *)ib_mm_alloc(mm, sizeof(*mc));
    if (mc == NULL) {
        return IB_EALLOC;
    }

    rc = ib_hash_create(&(mc->cmds), mm);
    if (rc != IB_OK) {
        return rc;
    }

    mc->sock    = -1;
    mc->manager = manager;
    mc->mm      = mm;

    /* Ensure that the last character is always terminated. */
    mc->msg[IB_ENGINE_MANAGER_CONTROL_CHANNEL_MAX_MSG_SZ] = '\0';

    mc->sock_path = DEFAULT_SOCKET_PATH;

    ib_mm_register_cleanup(mm, channel_cleanup, mc);

    *channel = mc;
    return IB_OK;
}


ib_status_t ib_engine_manager_control_channel_stop(
    ib_engine_manager_control_channel_t *channel
)
{
    assert(channel != NULL);

    if (channel->sock >= 0) {
        int sysrc;

        close(channel->sock);
        channel->sock = -1;

        /* Remove the socket file so external programs know it's closed. */
        sysrc = unlink(channel->sock_path);
        if (sysrc == -1 && errno != ENOENT) {
            log_socket_error(channel, "unlink", strerror(errno));
            return IB_EOTHER;
        }
    }

    return IB_OK;
}

ib_status_t ib_engine_manager_control_channel_start(
    ib_engine_manager_control_channel_t *channel
)
{
    assert(channel != NULL);

    int sysrc;
    int sock;
    struct sockaddr_un addr;

    /* Socket path is too long for the path. */
    if (strlen(channel->sock_path) + 1 >= sizeof(addr.sun_path)) {
        return IB_EINVAL;
    }

    sock = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (sock < 0) {
        log_socket_error(channel, "create", strerror(errno));
        return IB_EOTHER;
    }

    addr.sun_family = AF_UNIX;

    strcpy(addr.sun_path, channel->sock_path);

    sysrc = unlink(addr.sun_path);
    if (sysrc == -1 && errno != ENOENT) {
        log_socket_error(channel, "unlink old", strerror(errno));
        return IB_EOTHER;
    }

    sysrc = bind(sock, (const struct sockaddr *)&addr, sizeof(addr));
    if (sysrc == -1) {
        log_socket_error(channel, "bind", strerror(errno));
        return IB_EOTHER;
    }

    channel->sock = sock;

    return IB_OK;
}

/**
 * Check if data is available to receive.
 *
 * @param[in] channel The channel to receive from.
 *
 * @returns
 * - IB_OK If the channel is ready to receive a message.
 * - IB_EAGAIN If the channel has no data available.
 * - IB_OTHER Another error has occured.
 */
ib_status_t ib_engine_manager_control_ready(
    ib_engine_manager_control_channel_t *channel
)
{
    assert(channel != NULL);

    int sysrc;
    const int nfds = channel->sock + 1;
    struct timeval timeout = { 0, 0 };

    fd_set readfds;
    fd_set exceptfds;

    FD_ZERO(&readfds);
    FD_ZERO(&exceptfds);

    sysrc = select(nfds, &readfds, NULL, &exceptfds, &timeout);
    if (sysrc < 0) {
        log_socket_error(channel, "select from", strerror(errno));
        return IB_EOTHER;
    }

    if (sysrc > 0) {
        if (FD_ISSET(channel->sock, &exceptfds)) {
            log_socket_error(channel, "error on", strerror(errno));
            return IB_EOTHER;
        }

        if (FD_ISSET(channel->sock, &readfds)) {
            return IB_OK;
        }
    }

    /* We get here by channel->sock not being in the read set or no sockets
     * appearing in any set (read or exception). */
    return IB_EAGAIN;
}

/**
 * Process the command recieved and send a reply.
 *
 * @param[in] channel The channel.
 * @param[in] cmdline Null-terminated command line.
 * @param[in] cmdlinesz The string length of cmdline (not including \0).
 *            This is provided because it is provided "for free" by
 *            the datagram and does not mean that the string does not
 *            terminate early with an embedded null.
 * @param[in] src_addr The source address as reported by @c recvfrom.
 * @param[in] addrlen The length of @a src_addr set by @c recvfrom.
 *            If this is 0 then @c recvfrom did not get a valid
 *            return address for the client and no reply witll be sent.
 *
 * @returns
 * - IB_OK On success.
 * - IB_EOTHER On failure to send reply.
 */
static ib_status_t handle_command(
    ib_engine_manager_control_channel_t *channel,
    uint8_t                             *cmdline,
    size_t                               cmdlinesz,
    const struct sockaddr_un            *src_addr,
    socklen_t                            addrlen
)
{
    assert(channel != NULL);
    assert(cmdline != NULL);
    assert(src_addr != NULL);

    /* What we consider whitespace. */
    static const char *ws = "\r\n\t ";

    ib_status_t      rc;
    cmd_t           *cmd;
    ib_mpool_lite_t *mp;
    ib_mm_t          mm;
    const char      *result = NULL;
    char            *name;
    char            *args;
    size_t           name_len;
    ssize_t          written;

    rc = ib_mpool_lite_create(&mp);
    if (rc != IB_OK) {
        return rc;
    }

    mm = ib_mm_mpool_lite(mp);

    /* Copy the cmdline so we can modify it freely. */
    name = ib_mm_strdup(mm, (const char *)cmdline);
    if (name == NULL) {
        return IB_EALLOC;
    }

    /* Skip over ws character. */
    name += strspn(name, ws);

    /* A command of all white space? Error. */
    if (*name == '\0') {
        log_socket_error(
            channel,
            "with invalid command on",
            "Command name is entirely whitespace.");
        return IB_EINVAL;
    }

    /* Find the next whitespace character. That's the length of the name. */
    name_len = strcspn(name, ws);

    /* If name[len] is not the end of the command line, parse the args. */
    if (name[name_len] != '\0') {

        /* Terminate the name. */
        name[name_len] = '\0';

        /* Point args past the name. It may point at more leading ws.*/
        args = name+name_len+1;

        /* Skip whitespace, as above with name. */
        args += strspn(args, ws);
    }
    else {
        args = "";
    }

    rc = ib_hash_get(channel->cmds, &cmd, name);
    if (rc == IB_ENOENT) {
        log_socket_error(channel, "find command on", name);
        result = "ENOENT: Command not found.";
    }
    else if (rc != IB_OK) {
        log_socket_error(channel, "retrieve command", name);
        result = NULL;
    }
    else {
        rc = cmd->fn(
            mm,
            name,
            args,
            &result,
            cmd->cbdata);
    }

    if (result == NULL) {
        result = ib_status_to_string(rc);
    }

    /* Only send a reply if we were given a valid reply address. */
    if (addrlen > 0) {
        written = sendto(
            channel->sock,
            (void *)result,
            strlen(result),
            0,
            (const struct sockaddr *)src_addr,
            addrlen);
        if (written == -1) {
            log_socket_error(
                channel,
                "write result response to",
                strerror(errno));
            return IB_EOTHER;
        }
    }

    ib_mpool_lite_destroy(mp);

    return IB_OK;
}

ib_status_t ib_engine_manager_control_recv(
    ib_engine_manager_control_channel_t *channel
)
{
    assert(channel != NULL);

    /* Ensure that the last byte is always null. */
    assert(channel->msg[IB_ENGINE_MANAGER_CONTROL_CHANNEL_MAX_MSG_SZ] == '\0');

    ib_status_t        rc;
    ssize_t            recvsz = 0;
    struct sockaddr_un src_addr;
    socklen_t          addrlen = sizeof(src_addr);
    recvsz = recvfrom(
        channel->sock,
        channel->msg,
        IB_ENGINE_MANAGER_CONTROL_CHANNEL_MAX_MSG_SZ,
        0,
        (struct sockaddr *)&src_addr,
        &addrlen);
    if (recvsz == -1) {
        /* On recv error the message in the channel buffer is not valid.
         * Set the length to 0. */
        channel->msgsz = 0;

        log_socket_error(channel, "receive message on", strerror(errno));

        return IB_EOTHER;
    }

    /* Null terminate the message. NOTE: The message buffer is 1 byte
     * larger than the max message we will receive, so an out-of-bounds
     * check has already been implicitly done by recvfrom(). */
    (channel->msg)[recvsz] = '\0';

    channel->msgsz = (size_t)recvsz;
    rc = handle_command(
        channel,
        channel->msg,
        channel->msgsz,
        &src_addr,
        addrlen);
    if (rc != IB_OK) {
        return rc;
    }

    return IB_OK;
}

ib_status_t ib_engine_manager_control_send(
    const char  *sock_path,
    const char  *message,
    ib_mm_t      mm,
    const char **response
)
{
    assert(sock_path != NULL);
    assert(message != NULL);
    assert(response != NULL);

    ib_status_t        rc = IB_OK;
    size_t             message_len  = strlen(message);
    int                sock;
    struct sockaddr_un dst_addr;
    struct sockaddr_un src_addr;
    int                sysrc;
    ssize_t            ssz;
    char              *resp; /* Our copy of response. */

    /* The message is too long. */
    if (message_len > IB_ENGINE_MANAGER_CONTROL_CHANNEL_MAX_MSG_SZ) {
        return IB_EINVAL;
    }

    /* Path to socket is too long. */
    if (strlen(sock_path) + 1 >= sizeof(dst_addr.sun_path)) {
        return IB_EINVAL;
    }

    sock = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (sock < 0) {
        return IB_EOTHER;
    }

    dst_addr.sun_family = AF_UNIX;
    src_addr.sun_family = AF_UNIX;

    strcpy(dst_addr.sun_path, sock_path);
    sysrc = snprintf(
        src_addr.sun_path,
        sizeof(src_addr.sun_path),
        "/tmp/ibctrl.%d.S",
        getpid());
    if (sysrc < 0) {
        rc = IB_EINVAL;
        goto cleanup_sock;
    }

    unlink(src_addr.sun_path);
    sysrc = bind(sock, (const struct sockaddr *)&src_addr, sizeof(src_addr));
    if (sysrc == -1) {
        rc = IB_EOTHER;
        goto cleanup_sock;
    }

    ssz = sendto(
        sock,
        message,
        message_len,
        0,
        (struct sockaddr *)&dst_addr,
        sizeof(dst_addr));
    if (ssz == EMSGSIZE) {
        /* By API contract, we return IB_EINVAL here.
         * In practice, this should never happen. */
        rc = IB_EINVAL;
        goto cleanup;
    }
    if (ssz < 0) {
        rc = IB_EOTHER;
        goto cleanup;
    }

    /* Since this is a datagram protocol, we should have sent everything. */
    assert(ssz == (ssize_t)message_len);

    /* Allocate after sending the message to the server.
     * It is more likely that the server is down, so we defer allocating mem as
     * that should almost always succeed. */
    resp = ib_mm_alloc(mm, IB_ENGINE_MANAGER_CONTROL_CHANNEL_MAX_MSG_SZ+1);
    if (resp == NULL) {
        rc = IB_EALLOC;
        goto cleanup;
    }

    ssz = recvfrom(
        sock,
        resp,
        IB_ENGINE_MANAGER_CONTROL_CHANNEL_MAX_MSG_SZ,
        0,
        NULL,
        NULL);
    if (ssz == -1) {
        rc = IB_EOTHER;
        goto cleanup;
    }

    /* Ensure this is null-terminated. */
    resp[ssz] = '\0';
    *response = resp;

cleanup:
    unlink(src_addr.sun_path);
cleanup_sock:
    close(sock);

    return rc;
}

ib_status_t ib_engine_manager_control_cmd_register(
    ib_engine_manager_control_channel_t        *channel,
    const char                                 *name,
    ib_engine_manager_control_channel_cmd_fn_t  fn,
    void                                       *cbdata
)
{
    assert(channel != NULL);
    assert(channel->manager != NULL);
    assert(channel->cmds != NULL);
    assert(name != NULL);

    cmd_t       *cmd;
    ib_status_t  rc;

    cmd = ib_mm_alloc(channel->mm, sizeof(*cmd));
    cmd->fn = fn;
    cmd->cbdata = cbdata;
    cmd->name = ib_mm_strdup(channel->mm, name);
    if (name == NULL) {
        return IB_EALLOC;
    }

    rc = ib_hash_set(channel->cmds, name, cmd);
    if (rc != IB_OK) {
        return rc;
    }

    return IB_OK;
}

ib_status_t ib_engine_manager_control_echo_register(
    ib_engine_manager_control_channel_t *channel
)
{
    assert(channel != NULL);

    return ib_engine_manager_control_cmd_register(
        channel,
        "echo",
        echo_cmd, NULL);
}


ib_status_t ib_engine_manager_control_manager_ctrl_register(
    ib_engine_manager_control_channel_t *channel
)
{
    assert(channel != NULL);
    assert(channel->manager != NULL);

    ib_status_t rc;

    /* Local map of commands to register. All commands here
     * take an engine manager as their callback data. */
    struct {
        const char                                 *name;
        ib_engine_manager_control_channel_cmd_fn_t  fn;
    } cmds[] = {
        { "enable",        manager_cmd_enable },
        { "disable",       manager_cmd_disable },
        { "cleanup",       manager_cmd_cleanup },
        { "engine_create", manager_cmd_engine_create },
        { NULL,            NULL }
    };

    for (int i = 0; cmds[i].name != NULL; ++i) {
        rc = ib_engine_manager_control_cmd_register(
            channel,
            cmds[i].name,
            cmds[i].fn,
            channel->manager
        );
        if (rc != IB_OK) {
            return rc;
        }
    }

    return IB_OK;
}

const char *ib_engine_manager_control_channel_socket_path_get(
    const ib_engine_manager_control_channel_t *channel
)
{
    assert(channel != NULL);
    assert(channel->sock_path != NULL);

    return channel->sock_path;
}

ib_status_t ib_engine_manager_control_channel_socket_path_set(
    ib_engine_manager_control_channel_t *channel,
    const char                          *path
)
{
    assert(channel != NULL);
    assert(path != NULL);

    const char *path_cpy = ib_mm_strdup(channel->mm, path);

    if (path_cpy == NULL) {
        return IB_EALLOC;
    }

    channel->sock_path = path_cpy;

    return IB_OK;
}