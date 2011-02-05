/***************************************************************************
 * Copyright 2009-2010 Open Information Security Foundation
 * Copyright 2010-2011 Qualys, Inc.
 *
 * Licensed to You under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 ***************************************************************************/

/**
 * @file
 * @author Ivan Ristic <ivanr@webkreator.com>
 */

#include "hooks.h"

/**
 * Creates a new hook.
 *
 * @return New htp_hook_t structure on success, NULL on failure
 */
htp_hook_t *hook_create(void) {
    htp_hook_t *hook = calloc(1, sizeof (htp_hook_t));
    if (hook == NULL) return NULL;

    hook->callbacks = list_array_create(4);
    if (hook->callbacks == NULL) {
        free(hook);
        return NULL;
    }   

    return hook;
}

/**
 * Creates a copy of the provided hook. The hook is allowed to be NULL,
 * in which case this function simply returns a NULL.
 *
 * @param hook
 * @return A copy of the hook, or NULL (if the provided hook was NULL
 *         or, if it wasn't, if there was a memory allocation problem while
 *         constructing a copy).
 */
htp_hook_t * hook_copy(htp_hook_t *hook) {
    if (hook == NULL) return NULL;

    htp_hook_t *copy = hook_create();
    if (copy == NULL) return NULL;

    htp_callback_t *callback = NULL;
    list_iterator_reset(hook->callbacks);
    while ((callback = list_iterator_next(hook->callbacks)) != NULL) {
        if (hook_register(&copy, callback->fn) < 0) {
            hook_destroy(copy);
            return NULL;
        }
    }

    return copy;
}

/**
 * Destroys an existing hook. It is all right to send a NULL
 * to this method because it will simply return straight away.
 *
 * @param hook
 */
void hook_destroy(htp_hook_t *hook) {
    if (hook == NULL) return;

    htp_callback_t *callback = NULL;
    list_iterator_reset(hook->callbacks);
    while ((callback = list_iterator_next(hook->callbacks)) != NULL) {
        free(callback);
    }

    list_destroy(&hook->callbacks);
    
    free(hook);
}

/**
 * Registers a new callback with the hook.
 *
 * @param hook
 * @param callback_fn
 * @return 1 on success, -1 on memory allocation error
 */
int hook_register(htp_hook_t **hook, htp_callback_fn_t callback_fn) {
    int hook_created = 0;
    htp_callback_t *callback = calloc(1, sizeof (htp_callback_t));
    if (callback == NULL) return -1;

    callback->fn = callback_fn;

    // Create a new hook if one does not exist
    if (*hook == NULL) {
        *hook = hook_create();
        if (*hook == NULL) {
            free(callback);
            return -1;
        }

        hook_created = 1;
    }

    // Add callback 
    if (list_add((*hook)->callbacks, callback) < 0) {
        if (hook_created) {
            free(*hook);
        }
        
        free(callback);
        return -1;
    }

    return 1;
}

/**
 * Runs all the callbacks associated with a given hook. Only stops if
 * one of the callbacks returns an error (HOOK_ERROR). 
 *
 * @param hook
 * @param data
 * @return HOOK_OK or HOOK_ERROR
 */
int hook_run_all(htp_hook_t *hook, void *data) {
    if (hook == NULL) {
        return HOOK_OK;
    }

    htp_callback_t *callback = NULL;
    list_iterator_reset(hook->callbacks);
    while ((callback = list_iterator_next(hook->callbacks)) != NULL) {
        if (callback->fn(data) == HOOK_ERROR) {
            return HOOK_ERROR;
        }
    }

    return HOOK_OK;
}

/**
 * Run callbacks until one of them accepts to service the hook.
 *
 * @param hook
 * @param data
 * @return HOOK_OK on success, HOOK_DECLINED if no callback wanted to run and HOOK_ERROR on error.
 */
int hook_run_one(htp_hook_t *hook, void *data) {
    if (hook == NULL) {
        return HOOK_DECLINED;
    }

    htp_callback_t *callback = NULL;
    list_iterator_reset(hook->callbacks);
    while ((callback = list_iterator_next(hook->callbacks)) != NULL) {
        int status = callback->fn(data);
        // Both HOOK_OK and HOOK_ERROR will stop hook processing
        if (status != HOOK_DECLINED) {
            return status;
        }
    }

    return HOOK_DECLINED;
}
