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

#ifndef __ENGINE_MANAGER_CONTROL_CHANNEL_H_
#define __ENGINE_MANAGER_CONTROL_CHANNEL_H_

#include <ironbee/engine.h>
#include <ironbee/engine_manager.h>
#include <ironbee/mm.h>
#include <ironbee/string.h>
#include <ironbee/util.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * The largest message that can be sent to the channel.
 */
#define IB_ENGINE_MANAGER_CONTROL_CHANNEL_MAX_MSG_SZ 1024

 /**
 * @defgroup IronBeeEngMgrCtrlChan IronBee Engine Manager Control Channel
 * @ingroup IronBee
 *
 * Manage opening a communication channel by which a client can send
 * commands to an @ref ib_manager_t.
 *
 * An @ref ib_engine_manager_control_channel_t must be started to begin
 * receving commands, and should be stopped to clean up all allocated
 * resources.
 *
 * Cleanup is automatic when the @ref ib_mm_t that a controller is allocated
 * out of is destroyed.
 *
 * @{
 */

/**
 * Dispatch commands from other processes to an @ref ib_manager_t.
 *
 * The @ref ib_manager_t is stored here along with a registry of
 * available commands to execute. Commands do not need to effect
 * the engine manager, but that is the primary concern of this
 * control channel.
 *
 * @sa ib_engine_manager_control_cmd_register().
 */
typedef struct ib_engine_manager_control_channel_t
    ib_engine_manager_control_channel_t;

/**
 * Callback function type.
 *
 * This function may provide resluts back to the client in two ways.
 * The first, is the reesult status code. It will be converted by
 * ib_status_to_string() and returned to the user.
 *
 * The second method is to set the @a result parameter to a non-NULL value.
 * If @a result is set then the return value is ignored and the contents
 * of @a result is sent to the client.
 *
 * The return status code has no other use than to report
 * status when the user chooses not to provide a more precise message
 * by @a result.
 *
 * @param[in] mm Memory manager. All allocations, particularly, @a result,
 *            should be made from this.
 * @param[in] name The name this function is called as.
 * @parma[in] args The arguments as a single null-terminated string.
 * @param[out] result The result to send back to the client. This is initially
               set to NULL and, if it remains unchanged, then the
 *            return code of this function is converted using
 *            ib_status_to_string() and assigned.
 * @param[in] cbdata Callback data.
 *
 * @returns The status code to return to the user if no alternate message
 *          is provided by @a result. If a messae is provided through
 *          the out parameter, @a result, this value is ignored.
 */
typedef ib_status_t(*ib_engine_manager_control_channel_cmd_fn_t)(
    ib_mm_t      mm,
    const char  *name,
    const char  *args,
    const char **result,
    void        *cbdata
);

/**
 * Create a stopped @ref ib_engine_manager_control_channel_t.
 *
 * The user must call ib_engine_manager_control_channel_start() to open the server
 * and start it processing events.
 *
 * @param[out] channel The created struct.
 * @param[in] mm The memory manager to allocate out of. In practice, this
 *            should be the same memory manager used by the @a manager,
 *            but it does not need to be.
 * @param[in] manager The manager we will be controlling.
 *
 * @returns
 * - IB_OK On success.
 * - IB_EALLOC On allocation errors.
 */
ib_status_t DLL_PUBLIC ib_engine_manager_control_channel_create(
    ib_engine_manager_control_channel_t **channel,
    ib_mm_t                               mm,
    ib_manager_t                         *manager
)
NONNULL_ATTRIBUTE(1, 3);

/**
 * Open a domain socket at named ironbee_channel.sock in `${sharedstatedir}`.
 *
 * @param[in] channel The channel to open a socket for.
 *
 * @sa ib_engine_manager_control_channel_stop().
 *
 * @returns
 * - IB_OK On success.
 * - Other on failure.
 */
ib_status_t DLL_PUBLIC ib_engine_manager_control_channel_start(
    ib_engine_manager_control_channel_t *channel
)
NONNULL_ATTRIBUTE(1);

/**
 * Close and remove the communication socket for @a channel.
 *
 * @param[in] channel The channel to open a socket for.
 *
 * @sa ib_engine_manager_control_channel_start().
 *
 * @returns
 * - IB_OK On success.
 * - Other on failure.
 */
ib_status_t DLL_PUBLIC ib_engine_manager_control_channel_stop(
    ib_engine_manager_control_channel_t *channel
)
NONNULL_ATTRIBUTE(1);

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
ib_status_t DLL_PUBLIC ib_engine_manager_control_ready(
    ib_engine_manager_control_channel_t *channel
)
NONNULL_ATTRIBUTE(1);

/**
 * Recieve a command and process it.
 *
 * @param[in] channel The channel to receive from.
 *
 * @returns
 * - IB_OK If the channel received and successfuly dispatched a message.
 * - IB_ENOENT If an unknown command was received.
 * - IB_OTHER Another error has occured.
 */
ib_status_t DLL_PUBLIC ib_engine_manager_control_recv(
    ib_engine_manager_control_channel_t *channel
)
NONNULL_ATTRIBUTE(1);

/**
 * Register @a fn with the channel as an available command.
 *
 * @param[in] channel The channel to register the command with.
 * @param[in] name The name of the command.
 * @param[in] fn The implementation of command @a name.
 * @param[in] cbdata Callback data provided to @a fn
 *
 * @returns
 * - IB_OK On success.
 * - IB_EALLOC On allocation error.
 * - Other failures.
 */
ib_status_t DLL_PUBLIC ib_engine_manager_control_cmd_register(
    ib_engine_manager_control_channel_t        *channel,
    const char                                 *name,
    ib_engine_manager_control_channel_cmd_fn_t  fn,
    void                                       *cbdata
)
NONNULL_ATTRIBUTE(1,2);

/**
 * Register the @c echo command with this channel.
 *
 * This is a useful command used for debugging or pings.
 * It will echo the the arguments submitted to it.
 *
 * @param[in] channel The channel to register this command with.
 *
 * @returns
 * - IB_OK On success.
 * - Other on registration failure.
 */
ib_status_t DLL_PUBLIC ib_engine_manager_control_echo_register(
    ib_engine_manager_control_channel_t *channel
)
NONNULL_ATTRIBUTE(1);

/**
 * Register the default manager control commands.
 *
 * The commands registered are:
 * - enable - enable IronBee in the manager.
 * - disable - disable IronBee in the manager.
 * - cleanup - cleanup old IronBee engines in the manager.
 * - engine_create <config file> - Create a new engine.
 *   IronBee must not be disabled for this to succeed.
 *
 * @param[in] channel The channel to register this command with.
 *
 * @returns
 * - IB_OK On success.
 * - Other on registration failure.
 */
ib_status_t DLL_PUBLIC ib_engine_manager_control_manager_ctrl_register(
    ib_engine_manager_control_channel_t *channel
)
NONNULL_ATTRIBUTE(1);

/**
 * Return the path to the socket being used by this channel.
 *
 * @param[in] channel The channel to access the socket path of.
 *
 * @returns The socket path. This file may or may not exist
 * if the channel has been stopped.
 */
const char DLL_PUBLIC *ib_engine_manager_control_channel_socket_path_get(
    const ib_engine_manager_control_channel_t *channel
)
NONNULL_ATTRIBUTE(1);

/**
 * Copy @a path as the socket path for this channel to use when it is opened.
 *
 * Users of this function should not call this after
 * ib_engine_manager_control_channel_start() has been called. Rather, stop the
 * channel, set this value, then start the channel.
 *
 * @note Channels are initialized in a stopped state, and so this may be
 * called after ib_engine_manager_control_channel_create().
 *
 * @param[in] channel The channel to set the socket for.
 * @param[in] path The path to the file that the socket should be created at.
 *            This string is copied using the channel's memory manager.
 *            This string may never be NULL.
 *
 * @returns
 * - IB_OK On success.
 * - IB_EALLOC On allocation failure.
 */
ib_status_t DLL_PUBLIC ib_engine_manager_control_channel_socket_path_set(
    ib_engine_manager_control_channel_t *channel,
    const char                          *path
)
NONNULL_ATTRIBUTE(1,2);

/**
 * Client function to communicate with a started channel.
 *
 * This code is intended to handle the client-side of the messaging
 * protocol to an @ref ib_engine_manager_control_channel_t. As such,
 * it is not expected that the client can easily construct
 * an @ref ib_engine_manager_control_channel_t and so only the path to the
 * socket is required.
 *
 * @param[in] sock_path Path to the unix domain socket the channel is at.
 * @param[in] message The C-string message to send to the server.
 * @param[in] mm The memory manager used to allocate @a response from.
 * @param[out] response The response from the server is stored here.
 *
 * @returns
 * - IB_OK On succesfully interacting with the server. If the server
 *   returns an error, it is encoded in the @a response, not as
 *   the return code from ib_engine_manager_control_send().
 * - IB_EALLOC If allocations could not be made out of @a mm.
 * - IB_EOTHER On an unexpected system problem. Check `errno`.
 * - IB_EINVAL If @A message is too long to send to the server or the path
 *   is too long for a unix domain socket (107 characters + \0).
 */
ib_status_t ib_engine_manager_control_send(
    const char  *sock_path,
    const char  *message,
    ib_mm_t      mm,
    const char **response
)
NONNULL_ATTRIBUTE(1,2,4);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* __ENGINE_MANAGER_CONTROL_CHANNEL_H_ */
