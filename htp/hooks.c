/***************************************************************************
 * Copyright (c) 2009-2010, Open Information Security Foundation
 * Copyright (c) 2009-2012, Qualys, Inc.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 * * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * * Neither the name of the Qualys, Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 ***************************************************************************/

/**
 * @file
 * @author Ivan Ristic <ivanr@webkreator.com>
 */

#include "hooks.h"

htp_hook_t * hook_copy(const htp_hook_t *hook) {
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

int hook_register(htp_hook_t **hook, const htp_callback_fn_t callback_fn) {
    int hook_created = 0;
    htp_callback_t *callback = calloc(1, sizeof (htp_callback_t));
    if (callback == NULL) return HOOK_ERROR;

    callback->fn = callback_fn;

    // Create a new hook if one does not exist
    if (*hook == NULL) {
        *hook = hook_create();
        if (*hook == NULL) {
            free(callback);
            return HOOK_ERROR;
        }

        hook_created = 1;
    }

    // Add callback 
    if (list_add((*hook)->callbacks, callback) < 0) {
        if (hook_created) {
            free(*hook);
        }
        
        free(callback);
        return HOOK_ERROR;
    }

    return HOOK_OK;
}

int hook_run_all(htp_hook_t *hook, void *user_data) {
    if (hook == NULL) return HOOK_OK;

    // Loop through registered callbacks,
    // giving each a chance to run.
    htp_callback_t *callback = NULL;
    list_iterator_reset(hook->callbacks);
    while ((callback = list_iterator_next(hook->callbacks)) != NULL) {
        int rc = callback->fn(user_data);
        if ((rc != HOOK_OK)&&(rc != HOOK_DECLINED)) {
            // Return HOOK_STOP or error.
            return rc;
        }
    }

    return HOOK_OK;
}

int hook_run_one(htp_hook_t *hook, void *user_data) {
    if (hook == NULL) return HOOK_DECLINED;

    // Look through registered callbacks
    // until one accepts to process the hook.
    htp_callback_t *callback = NULL;
    list_iterator_reset(hook->callbacks);
    while ((callback = list_iterator_next(hook->callbacks)) != NULL) {
        int rc = callback->fn(user_data);
        if (rc != HOOK_DECLINED) {
            // Return HOOK_OK, HOOK_STOP, or error.
            return rc;
        }
    }

    // No hook wanted to process the callback.
    return HOOK_DECLINED;
}
