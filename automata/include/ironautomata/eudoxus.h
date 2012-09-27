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

#ifndef _IA_EUDOXUS_H_
#define _IA_EUDOXUS_H_

/**
 * @file
 * @brief IronAutomata &mdash; Eudoxus DFA Engine
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup IronAutomataEudoxus Eudoxus
 * @ingroup IronAutomata
 *
 * Automata execution engine.
 *
 * Eudoxus is an automata execution engine that focuses on high performance
 * in both time and space and tunable tradeoffs between time and space.
 *
 * This code only implements the execution of an automata.  The building of
 * automata (compilation) from intermediate format and the generation of
 * intermediate format (generation) are separate.
 *
 * To use, create a Eudoxus Engine with one of the @c ia_eudoxus_create_
 * functions, create an execution state with ia_eudoxus_create_state(),
 * feed input to it with ia_eudoxus_execute(), and then clean up with
 * ia_eudoxus_destroy_state() and ia_eudoxus_destroy().  Output will be
 * passed back to you via a callback function which can also abort automata
 * execution if appropriate.
 *
 * Most error conditions (except allocation, engine creation, and some
 * insanity errors) will set an error message in the engine which can be
 * accessed via ia_eudoxus_error().  Callbacks can set an error message with
 * the various @c ia_eudoxus_set_error_ methods.
 */

/**
 * Eudoxus Result Codes
 */
enum ia_eudoxus_result_t
{
    IA_EUDOXUS_OK,        /**< All is normal. */
    IA_EUDOXUS_STOP,      /**< Callback indicated execution should stop. */
    IA_EUDOXUS_ERROR,     /**< Callback indicated error. */
    IA_EUDOXUS_END,       /**< End of automata reached. */
    IA_EUDOXUS_EINVAL,    /**< Invalid input. */
    IA_EUDOXUS_EALLOC,    /**< Allocation failure. */
    IA_EUDOXUS_EINCOMPAT, /**< Not compatible with engine. */
    IA_EUDOXUS_EINSANE    /**< Insanity error; please report as bug. */
};
typedef enum ia_eudoxus_result_t ia_eudoxus_result_t;

/**
 * A Eudoxus automata engine.
 *
 * An opaque data structure representing a Eudoxus engine.  It can be created
 * from file system (ia_eudoxus_create_from_path()), a FILE
 * (ia_eudoxus_create_from_file()), or a chunk of memory
 * (ia_eudoxus_create()).  When finished, it should be destroyed with
 * ia_eudoxus_destroy().  It can be used via ia_eudoxus_create_state().
 */
typedef struct ia_eudoxus_t ia_eudoxus_t;

/**
 * Create a Eudoxus engine from a block of memory.
 *
 * @attention This method will claim ownership of @a data and free that
 *            memory when ia_eudoxus_destroy() is called.  If you do not want
 *            this behavior, pass in a copy instead.
 *
 * The method will check that the automata has a valid header and is
 * compatible with the current version and Endianness, but does no other
 * validation.
 *
 * @param[out] out_eudoxus Variable to hold pointer to created engine.
 * @param[in]  data        Data holding automata.
 * @return
 * - IA_EUDOXUS_OK on success.
 * - IA_EUDOXUS_EINVAL if @a out_eudoxus or @a data is NULL or @a data_length
 *   is 0.
 * - IA_EUDOXUS_EALLOC on allocation failure.
 * - IA_EINCOMPAT if automata is not compatible with engine.
 *
 * @sa ia_eudoxus_t
 * @sa ia_eudoxus_create_from_path()
 * @sa ia_eudoxus_create_from_file()
 */
ia_eudoxus_result_t ia_eudoxus_create(
    ia_eudoxus_t **out_eudoxus,
    const char    *data
);

/**
 * As above, but load from FILE.
 *
 * @param[out] out_eudoxus Variable to hold pointer to created engine.
 * @param[in]  fp          @c FILE* to load from.
 * @return
 * - IA_EUDOXUS_EINVAL if @a out_eudoxus or @a fp is NULL or @a fp is not seekable.
 * - IA_EUDOXUS_EINSANE on unexpected behavior of standard functions.
 * - Other codes as described in ia_eudoxus_create().
 *
 * @sa ia_eudoxus_t
 */
ia_eudoxus_result_t ia_eudoxus_create_from_file(
    ia_eudoxus_t **out_eudoxus,
    FILE          *fp
);

/**
 * As above, but load automata from a file in the file system.
 *
 * This method will attempt to read the file at @a path and load the entire
 * file in @a path as a Eudoxus automata.  See ia_eudoxus_create() for
 * further discussion.
 *
 * @param[out] out_eudoxus Variable to hold pointer to created engine.
 * @param[in]  path        Path to file on disk holding automata.
 * @return
 * - IA_EUDOXUS_END on failure to open file for reading.
 * - IA_EUDOXUS_EINVAL if @a out_eudoxus or @a path is NULL.
 * - Other codes as described in ia_eudoxus_create_from_file().
 *
 * @sa ia_eudoxus_t
 */
ia_eudoxus_result_t ia_eudoxus_create_from_path(
    ia_eudoxus_t **out_eudoxus,
    const char    *path
);

/**
 * Destroy engine @a eudoxus, releasing associated memory.
 *
 * Behavior of any method that takes @a eudoxus as a parameter is undefined
 * after calling this.
 *
 * Does nothing if @a eudoxus is NULL.
 *
 * @param[in] eudoxus Engine to destroy.
 */
void ia_eudoxus_destroy(
    ia_eudoxus_t *eudoxus
);

/**
 * Access most recent error message of @a eudoxus.
 *
 * @attention Not reliable in all cases.  Consider resetting to NULL via
 * @c ia_eudoxus_set_error(NULL) after handling any error.  Cases where an
 * error may not be set
 * include:
 * - Allocation errors.
 * - Insanity errors.
 * - Callback errors (up to callback to set error message).
 * - Errors where engine does not exist or is invalid, e.g., from
 * ia_eudoxus_create().
 * - IA_EUDOXUS_END which is not actually an error.
 *
 * Return value should not me modified or freed.  Lifetime is only guaranteed
 * until next call to a eudoxus method.
 *
 * @param[in] eudoxus Engine to access error of.
 * @return Error message; possibly NULL.
 */
const char *ia_eudoxus_error(
    const ia_eudoxus_t *eudoxus
);

/**
 * Callback commands to engine.
 *
 * This enum is the return value of user callbacks and allows that callback
 * to tell the engine how to proceed.
 *
 * @sa ia_eudoxus_callback_t.
 *
 * @sa ia_eudoxus_set_error_cstr()
 * @sa ia_eudoxus_execute()
 */
enum ia_eudoxus_command_t
{
    /**
     * Continue executing automata.
     */
    IA_EUDOXUS_CMD_CONTINUE,

     /**
      * Stop executing automata.
      *
      * This causes ia_eudoxus_execute() to return immediately without
      * processing additional input.  Processing can be resumed by calling
      * ia_eudoxus_execute() with a NULL input, however this will recall the
      * callback as well.
      *
      * The return code of ia_eudoxus_execute() will be IA_EUDOXUS_STOP.
      */
    IA_EUDOXUS_CMD_STOP,

    /**
     * Stop executing automata and indicate an error.
     *
     * This is identical to IA_EUDOXUS_CMD_STOP except that it changes the
     * return code of ia_eudoxus_execute() from IA_EUDOXUS_STOP to
     * IA_EUDOXUS_ERROR.
     */
    IA_EUDOXUS_CMD_ERROR
};
typedef enum ia_eudoxus_command_t ia_eudoxus_command_t;

/**
 * Callback function for processing input.
 *
 * This user provided function will be called for every output of every
 * entered node.
 *
 * @param[in] output         Output defined by automata.
 * @param[in] output_length  Length of @a output.
 * @param[in] input_location Location in input.
 * @param[in] callback_data  Callback data as passed to
 *                           ia_eudoxus_create_state()
 * @return ia_eudoxus_command_t
 */
typedef ia_eudoxus_command_t (*ia_eudoxus_callback_t)(
    const char    *output,
    size_t         output_length,
    const uint8_t *input_location,
    void          *callback_data
);

/**
 * State of automata execution.
 *
 * This opaque data structure represents the current state of automata
 * execution.  It can be used to stream input to the engine by reusing the
 * same state object between calls to ia_eudoxus_execute().
 *
 * State objects should be created with ia_eudoxus_create_state() and
 * destroyed when no longer need with ia_eudoxus_destroy_state().
 */
typedef struct ia_eudoxus_state_t ia_eudoxus_state_t;

/**
 * Create a new state.
 *
 * This creates a new state for use in future calls to ia_eudoxus_execute().
 * The state is initialized at the start state of the automata and
 * @a callback is called with any outputs of the start state.
 *
 * If an error is reported, a message may be available via ia_eudoxus_error().
 *
 * @param[out] out_state    Where to store pointer to newly initialized
 *                          state.
 * @param[in]  eudoxus      Engine to initialize for.
 * @param[in]  callback     Callback to be called for each output of each
 *                          entered state.  May be NULL.
 * @param[in] callback_data Data to pass to @a callback.
 * @return
 * - IA_EUDOXUS_OK on success.
 * - IA_EUDOXUS_EINVAL if @a out_state or @a eudoxus is NULL or if @a eudoxus
 *   is detected to be corrupt.
 * - IA_EUDOXUS_EALLOC on allocation error.
 * - IA_EUDOXUS_STOP if callback called and returned IA_EUDOXUS_CMD_STOP.
 * - IA_EUDOXUS_ERROR if callback called and returned IA_EUDOXUS_CMD_ERROR.
 * - IA_EUDOXUS_EINSANE on insanity error; please report as bug along with
 *   message.
 */
ia_eudoxus_result_t ia_eudoxus_create_state(
    ia_eudoxus_state_t     **out_state,
    ia_eudoxus_t            *eudoxus,
    ia_eudoxus_callback_t    callback,
    void                    *callback_data
);

/**
 * Destroy @a state and release associated memory.
 *
 * Behavior of any method that takes @a state as a parameter is undefined
 * after calling this.
 *
 * Does nothing if @a state is NULL.
 *
 * @param[in] state State to destroy.
 */
void ia_eudoxus_destroy_state(
    ia_eudoxus_state_t *state
);

/**
 * Execute automata on a @a input.
 *
 * This method will execute the automata starting according to the input until
 * one of the following occurs:
 * - A callback returns IA_EUDOXUS_CMD_STOP.
 * - A callback returns IA_EUDOXUS_CMD_ERROR.
 * - There is no next state.
 * - There is no more input.
 *
 * When each node of the automata is entered, the callback will be called for
 * each output of that node.
 *
 * If execution stops due to a callback return value, it may be resumed by
 * calling this method with a NULL value for @a input.  However, this will
 * cause the callback to be called again with the outputs of the current
 * node.  Thus, arrangements must be made for the callback to now return
 * IA_EUDOXUS_CMD_CONTINUE for execution to continue.
 *
 * If execution stops due to a lack of input (or no input has been given),
 * the outputs of the current state can be recalled by calling with a NULL
 * value for @a input.
 *
 * If an error is reported, a message may be available via ia_eudoxus_error().
 *
 * @param[in, out] state        State of automata.
 * @param[in]      input        Input to execute on.
 * @param[in]      input_length Length of input.
 * @return
 * - IA_EUDOXUS_OK if out of input.
 * - IA_EUDOXUS_END if no next state can be reached.
 * - IA_EUDOXUS_EINVAL if @a state is NULL or a corrupt engine or automata is
 *   detected
 * - IA_EUDOXUS_EALLOC on allocation error.
 * - IA_EUDOXUS_STOP if callback called and returned IA_EUDOXUS_CMD_STOP.
 * - IA_EUDOXUS_ERROR if callback called and returned IA_EUDOXUS_CMD_ERROR.
 * - IA_EUDOXUS_EINSANE on insanity error; please report as bug along with
 *   message.
 */
ia_eudoxus_result_t ia_eudoxus_execute(
    ia_eudoxus_state_t *state,
    const uint8_t      *input,
    size_t              input_length
);

/**
 * Set error for @a eudoxus to @a message (claim ownership version).
 *
 * This sets the error message to @a message and indicates that that message
 * should be freed and engine destruction or when a new message is set.  For
 * messages that should not be freed, e.g., string literals, use
 * ia_eudoxus_set_error_cstr().
 *
 * The only useful for place to call this, as a Eudoxus user, is in a callback
 * function.  See ia_eudoxus_callback_t.
 *
 * This function will do nothing if @a eudoxus is NULL.  It otherwise can not
 * fail.
 *
 * @param[in] eudoxus Engine to set error message for.
 * @param[in] message Message to set.
 *
 * @sa ia_eudoxus_set_error_cstr()
 * @sa ia_eudoxus_set_error_printf()
 */
void ia_eudoxus_set_error(
    ia_eudoxus_t *eudoxus,
    char         *message
);

/**
 * Set error for @a eudoxus to @a message (NO ownership version).
 *
 * This acts as ia_eudoxus_set_error() above except that @a message will not
 * be freed.  This property makes this function appropriate for string
 * literals.
 *
 * See ia_eudoxus_set_error() for further discussion.
 *
 * @param[in] eudoxus Engine to set error message for.
 * @param[in] message Message to set.
 *
 * @sa ia_eudoxus_set_error()
 * @sa ia_eudoxus_set_error_printf()
 */
void ia_eudoxus_set_error_cstr(
    ia_eudoxus_t *eudoxus,
    const char   *message
);

/**
 * Set error for @a eudoxus to @a message (printf version).
 *
 * This function acts as ia_eudoxus_set_error() above except that it accepts
 * a printf style format string and arguments.  Unlike the previous two
 * functions, it can fail, e.g., due to allocation errors.  If this happens,
 * the error message will be set to NULL.
 *
 * @param[in] eudoxus Engine to set error message for.
 * @param[in] format  Printf style format string.
 *
 * @sa ia_eudoxus_set_error()
 * @sa ia_eudoxus_set_error_cstr()
 */
void ia_eudoxus_set_error_printf(
    ia_eudoxus_t *eudoxus,
    const char   *format,
    ...
) __attribute__((__format__ (printf, 2, 3)));

/**
 * @} IronAutomataEudoxus
 */

#ifdef __cplusplus
}
#endif

#endif /* _IA_EUDOXUS_H_ */
