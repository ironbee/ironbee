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
 * @brief IronAutomata &mdash; Eudoxus DFA Engine Implementation
 *
 * @warning This code makes significant trade offs in favor of time and space
 *          performance at the expense of code complexity.  If you are looking
 *          for a simple example of DFA execution, look elsewhere.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include "ironbee_config_auto.h"

#ifndef _DARWIN_C_SOURCE
#ifndef __FreeBSD__
// Tell glibc to enable fileno()
#ifndef _POSIX_SOURCE
#define _POSIX_SOURCE
#endif
// Tell glibc to enable vasprintf()
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#endif
#endif

#include <ironautomata/eudoxus.h>
#include <ironautomata/bits.h>

#include <ironautomata/eudoxus_automata.h>
#include <ironautomata/vls.h>

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct ia_eudoxus_t
{
    /**
     * Automata.
     */
    const ia_eudoxus_automata_t *automata;

    /**
     * Most recent error message.
     *
     * This will sometimes hold a buffer to be freed and sometimes a static
     * const string.  Which is determined by @c free_error_message.  As such,
     * we will sometimes need to cast the const away.
     *
     * @sa ia_eudoxus_error()
     * @sa ia_eudoxus_set_error()
     */
    const char *error_message;

    /**
     * Should @c error_message be freed if changed.
     *
     * This is set to false for ia_eudoxus_set_error_cstr() and true
     * otherwise.
     */
    bool free_error_message;
};

struct ia_eudoxus_state_t
{
    /**
     * The associated engine and thus, automata.
     */
    ia_eudoxus_t *eudoxus;

    /**
     * Callback function, if any, to call for outputs.
     */
    ia_eudoxus_callback_t callback;

    /**
     * Callback data to provide to @c callback.
     */
    void *callback_data;

    /**
     * Current node in automata.
     */
    const ia_eudoxus_node_t *node;

    /**
     * Current location in path compression nodes.
     */
    int byte_index;

    /**
     * Current location in current input chunk.
     */
    const uint8_t *input_location;

    /**
     * Remaining bytes in current input chunk.
     */
    size_t remaining_bytes;
};

/**
 * Extended Command.  Return code of next and output functions.
 *
 * This enum is a superset of ia_eudoxus_command_t and provides additional
 * items that make sense for next/output functions but not for callbacks.
 */
enum ia_eudoxus_extended_command_t
{
    /** @name Callback Results
     * Items resulting from callbacks.
     * @{
     */

    /**
     * Default, all is well, code.
     */
    IA_EUDOXUS_EXT_CONTINUE = IA_EUDOXUS_CMD_CONTINUE,

    /**
     * Stop execution immediately and give IA_EUDOXUS_STOP to user.
     */
    IA_EUDOXUS_EXT_STOP     = IA_EUDOXUS_CMD_STOP,

    /**
     * Stop execution immediately and give IA_EUDOXUS_ERROR to user.
     */
    IA_EUDOXUS_EXT_ERROR    = IA_EUDOXUS_CMD_ERROR,

    /**
     * Unable to find a next node.  Give IA_EUDOXUS_END to user.
     *
     * Note this does not necessarily indicate an error.  Some automata must
     * use an accept/not-accept model in which case the absence of a next
     * node is sufficient (though not necessary) for non-acceptance.
     */
    IA_EUDOXUS_EXT_NO_NEXT,

    /**
     * Automata is invalid.  Give IA_EUDOXUS_EINVAL to user.
     *
     * This code is used to indicate that some sort of invalid data was found
     * in the automata.  In theory, if an automata passes
     * ia_eudoxus_validate(), and proper parameters are given to
     * ia_eudoxus_execute(), then this should never occur.
     */
    IA_EUDOXUS_EXT_INVALID,

    /**
     * Insanity error.  Give IA_EUDOXUS_EINSANE to user.
     *
     * This code can be used to report assert failures to the user.  It always
     * indicates a bug.
     */
    IA_EUDOXUS_EXT_INSANITY
};
typedef enum ia_eudoxus_extended_command_t ia_eudoxus_extended_command_t;

ia_eudoxus_result_t ia_eudoxus_create(
    ia_eudoxus_t **out_eudoxus,
    char          *data
)
{
    ia_eudoxus_t        *eudoxus = NULL;
    ia_eudoxus_result_t  rc      = IA_EUDOXUS_OK;

    if (out_eudoxus == NULL || data == NULL) {
        return IA_EUDOXUS_EINVAL;
    }

    eudoxus = (ia_eudoxus_t *)malloc(sizeof(*eudoxus));
    if (eudoxus == NULL) {
        return IA_EUDOXUS_EINVAL;
    }

    eudoxus->automata           = (ia_eudoxus_automata_t *)data;
    eudoxus->error_message      = NULL;
    eudoxus->free_error_message = false;

    if (eudoxus->automata->version != IA_EUDOXUS_VERSION) {
        rc = IA_EUDOXUS_EINCOMPAT;
        goto finish;
    }

    if (eudoxus->automata->is_big_endian != ia_eudoxus_is_big_endian()) {
        rc = IA_EUDOXUS_EINCOMPAT;
        goto finish;
    }

finish:
    if (rc != IA_EUDOXUS_OK) {
        if (eudoxus != NULL) {
            free(eudoxus);
        }
    }
    else {
        *out_eudoxus = eudoxus;
    }

    return rc;
}


ia_eudoxus_result_t ia_eudoxus_create_from_file(
    ia_eudoxus_t **out_eudoxus,
    FILE          *fp
)
{
    char *buffer = NULL;
    size_t did_read = 0;

    off_t file_size = lseek(fileno(fp), 0, SEEK_END);
    lseek(fileno(fp), 0, SEEK_SET);
    if (file_size < 0) {
        return IA_EUDOXUS_EINVAL;
    }

    buffer = (char *)malloc(file_size);
    if (! buffer) {
        return IA_EUDOXUS_EALLOC;
    }

    did_read = fread(buffer, file_size, 1, fp);
    if (did_read != 1) {
        free(buffer);
        return IA_EUDOXUS_EINVAL;
    }

    return ia_eudoxus_create(out_eudoxus, buffer);
}

ia_eudoxus_result_t ia_eudoxus_create_from_path(
    ia_eudoxus_t **out_eudoxus,
    const char    *path
)
{
    FILE *fp = fopen(path, "r");
    if (! fp) {
        return IA_EUDOXUS_EINVAL;
    }

    ia_eudoxus_result_t result = ia_eudoxus_create_from_file(out_eudoxus, fp);

    fclose(fp);

    return result;
}

void ia_eudoxus_destroy(
    ia_eudoxus_t *eudoxus
)
{
    if (eudoxus == NULL) {
        return;
    }

    /* Better to cast away const here than to not have const checks for
     * all uses. */
    if (eudoxus->automata) {
        free((void *)eudoxus->automata);
    }
    if (eudoxus->error_message != NULL && eudoxus->free_error_message) {
        free((void *)eudoxus->error_message);
    }
    free(eudoxus);
}

const char *ia_eudoxus_error(
    const ia_eudoxus_t *eudoxus
)
{
    return eudoxus->error_message;
}

ia_eudoxus_result_t ia_eudoxus_create_state(
    ia_eudoxus_state_t    **out_state,
    ia_eudoxus_t           *eudoxus,
    ia_eudoxus_callback_t   callback,
    void                   *callback_data
)
{
    if (out_state == NULL || eudoxus == NULL) {
        return IA_EUDOXUS_EINVAL;
    }

    if (eudoxus->automata == NULL) {
        ia_eudoxus_set_error_cstr(eudoxus, "Invalid Automata.");
        return IA_EUDOXUS_EINVAL;
    }

    ia_eudoxus_state_t *state = (ia_eudoxus_state_t *)malloc(sizeof(*state));
    if (state == NULL) {
        ia_eudoxus_set_error(eudoxus, NULL);
        return IA_EUDOXUS_EINVAL;
    }

    state->eudoxus        = eudoxus;
    state->callback       = callback;
    state->callback_data  = callback_data;
    state->input_location = NULL;
    state->node           = (ia_eudoxus_node_t *)(
        (char *)eudoxus->automata + eudoxus->automata->start_index
    );
    state->byte_index     = 0;

    *out_state = state;

    /* Process outputs for start node. */
    return ia_eudoxus_execute(state, NULL, 0);
}

void ia_eudoxus_destroy_state(
    ia_eudoxus_state_t *state
)
{
    if (state != NULL) {
        free(state);
    }
}

void ia_eudoxus_set_error(
    ia_eudoxus_t *eudoxus,
    char         *message
)
{
    if (eudoxus == NULL) {
        return;
    }

    if (
        eudoxus->error_message != NULL &&
        eudoxus->free_error_message
    ) {
        free((void *)eudoxus->error_message);
    }

    eudoxus->error_message = message;
    eudoxus->free_error_message = true;
}

void ia_eudoxus_set_error_cstr(
    ia_eudoxus_t *eudoxus,
    const char   *message
)
{
    if (eudoxus == NULL) {
        return;
    }

    if (
        eudoxus->error_message != NULL &&
        eudoxus->free_error_message
    ) {
        free((void *)eudoxus->error_message);
    }

    eudoxus->error_message = message;
    eudoxus->free_error_message = false;
}

void ia_eudoxus_set_error_printf(
    ia_eudoxus_t *eudoxus,
    const char   *format,
    ...
)
{
    if (eudoxus == NULL) {
        return;
    }

    va_list ap;
    va_start(ap, format);

    if (
        eudoxus->error_message != NULL &&
        eudoxus->free_error_message
    ) {
        free((void *)eudoxus->error_message);
    }

    int result = vasprintf((char **)&eudoxus->error_message, format, ap);
    if (result == -1) {
        ia_eudoxus_set_error_cstr(
            eudoxus,
            "Allocation error printing error message."
        );
        assert(eudoxus->error_message == NULL);
    }
    else {
        eudoxus->free_error_message = true;
    }

    va_end(ap);
}

/* Specific Subengine Code */

#define IA_EUDOXUS(a) ia_eudoxus8_ ## a
#define IA_EUDOXUS_ID_T   uint64_t
#include "eudoxus_subengine.c"
#undef IA_EUDOXUS
#undef IA_EUDOXUS_ID_T

#define IA_EUDOXUS(a) ia_eudoxus4_ ## a
#define IA_EUDOXUS_ID_T   uint32_t
#include "eudoxus_subengine.c"
#undef IA_EUDOXUS
#undef IA_EUDOXUS_ID_T

#define IA_EUDOXUS(a) ia_eudoxus2_ ## a
#define IA_EUDOXUS_ID_T   uint16_t
#include "eudoxus_subengine.c"
#undef IA_EUDOXUS
#undef IA_EUDOXUS_ID_T

#define IA_EUDOXUS(a) ia_eudoxus1_ ## a
#define IA_EUDOXUS_ID_T   uint8_t
#include "eudoxus_subengine.c"
#undef IA_EUDOXUS
#undef IA_EUDOXUS_ID_T

/* End Specific Subengine Code */

static
ia_eudoxus_result_t ia_eudoxus_execute_impl(
    ia_eudoxus_state_t *state,
    const uint8_t      *input,
    size_t              input_length,
    bool                with_output
)
{
    switch (state->eudoxus->automata->id_width) {
    case 8:
        return ia_eudoxus8_execute(state, input, input_length, with_output);
    case 4:
        return ia_eudoxus4_execute(state, input, input_length, with_output);
    case 2:
        return ia_eudoxus2_execute(state, input, input_length, with_output);
    case 1:
        return ia_eudoxus1_execute(state, input, input_length, with_output);
    default:
        return IA_EUDOXUS_EINCOMPAT;
    }
}

ia_eudoxus_result_t ia_eudoxus_execute(
    ia_eudoxus_state_t *state,
    const uint8_t      *input,
    size_t              input_length
)
{
    return ia_eudoxus_execute_impl(state, input, input_length, true);
}

ia_eudoxus_result_t ia_eudoxus_execute_without_output(
    ia_eudoxus_state_t *state,
    const uint8_t      *input,
    size_t              input_length
)
{
    return ia_eudoxus_execute_impl(state, input, input_length, false);
}

ia_eudoxus_result_t ia_eudoxus_metadata(
    ia_eudoxus_t                   *eudoxus,
    ia_eudoxus_metadata_callback_t  callback,
    void                           *callback_data
)
{
    if (eudoxus == NULL || callback == NULL || eudoxus->automata == NULL) {
        return IA_EUDOXUS_EINVAL;
    }

    if (eudoxus->automata->metadata_index == 0) {
        return IA_EUDOXUS_END;
    }

    if (eudoxus->automata->num_metadata == 0) {
        return IA_EUDOXUS_EINVAL;
    }

    const char *automata_limit = (const char *)(eudoxus->automata) +
        eudoxus->automata->data_length;
    const ia_eudoxus_output_t *key = (const ia_eudoxus_output_t *)(
        (const char *)(eudoxus->automata) + eudoxus->automata->metadata_index
    );
    while ((const char *)key < automata_limit) {
        const ia_eudoxus_output_t *value = (const ia_eudoxus_output_t *)(
            (const char *)(key) + sizeof(*key) + key->length
        );
        if ((const char *)value > automata_limit) {
            return IA_EUDOXUS_EINVAL;
        }

        bool result = callback(
            (const uint8_t *)key->data,   key->length,
            (const uint8_t *)value->data, value->length,
            callback_data
        );

        if (! result) {
            return IA_EUDOXUS_STOP;
        }

        key = (const ia_eudoxus_output_t *)(
            (const char *)(value) + sizeof(*value) + value->length
        );
    }

    return IA_EUDOXUS_END;
}

typedef struct {
    const uint8_t  *search_key;
    size_t          search_key_length;
    const uint8_t **out_value;
    size_t         *out_value_length;
} ia_eudoxus_metadata_with_key_data_t;

static bool ia_eudoxus_metadata_with_key_function(
    const uint8_t *key,
    size_t         key_length,
    const uint8_t *value,
    size_t         value_length,
    void          *data
)
{
    assert(key != NULL);
    assert(value != NULL);
    assert(data != NULL);

    const ia_eudoxus_metadata_with_key_data_t *params =
        (const ia_eudoxus_metadata_with_key_data_t *)data;
    if (
        params->search_key_length == key_length &&
        memcmp(params->search_key, key, key_length) == 0
    ) {
        *params->out_value = value;
        *params->out_value_length = value_length;

        return false;
    }

    return true;
}

ia_eudoxus_result_t ia_eudoxus_metadata_with_key(
    ia_eudoxus_t   *eudoxus,
    const uint8_t  *key,
    size_t          key_length,
    const uint8_t **value,
    size_t         *value_length
)
{
    if (eudoxus == NULL || key == NULL) {
        return IA_EUDOXUS_EINVAL;
    }

    ia_eudoxus_metadata_with_key_data_t data = {
        key, key_length,
        value, value_length
    };

    ia_eudoxus_result_t rc = ia_eudoxus_metadata(
        eudoxus,
        &ia_eudoxus_metadata_with_key_function,
        &data
    );
    if (rc == IA_EUDOXUS_STOP) {
        return IA_EUDOXUS_OK;
    }
    return rc;
}

ia_eudoxus_result_t ia_eudoxus_all_outputs(
    ia_eudoxus_t          *eudoxus,
    ia_eudoxus_callback_t  callback,
    void                  *callback_data
)
{
    if (eudoxus == NULL || callback == NULL) {
        return IA_EUDOXUS_EINVAL;
    }

    uint64_t index = eudoxus->automata->first_output;
    while (
        index < eudoxus->automata->first_output_list &&
        index < eudoxus->automata->data_length
    ) {
        const ia_eudoxus_output_t *output = (const ia_eudoxus_output_t *)(
            (const char *)(eudoxus->automata) + index
        );
        ia_eudoxus_command_t command = callback(
            eudoxus,
            output->data,
            output->length,
            0,
            callback_data
        );
        if (command != IA_EUDOXUS_CMD_CONTINUE) {
            return (ia_eudoxus_result_t)command;
        }
        index += sizeof(ia_eudoxus_output_t) + output->length;
    }

    return IA_EUDOXUS_OK;
}
