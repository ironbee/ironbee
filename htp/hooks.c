
#include "hooks.h"

/**
 * Creates a new hook.
 *
 * @return New htp_hook_t structure on success, NULL on failure
 */
htp_hook_t *hook_create() {
   htp_hook_t *hook = calloc(1, sizeof(htp_hook_t));
   if (hook == NULL) return NULL;
   
   hook->callbacks = list_array_create(4);
   
   return hook;
}

/**
 * Destroys an existing hook.
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

   free(hook->callbacks);
   free(hook);
}

/**
 * Registers a new callback with the hook. The priority parameter is
 * not implemented yet.
 *
 * @param hook
 * @param callback_fn
 * @param priority
 */
void hook_register(htp_hook_t **hook, int (*callback_fn)(), int priority) {
    htp_callback_t *callback = calloc(1, sizeof(htp_callback_t));
    if (callback == NULL) return;
    
    callback->fn = callback_fn;    
    callback->priority = priority;

    // Create a new hook if one does not exist
    if (*hook == NULL) {
        *hook = hook_create();
        if (*hook == NULL) return;
    }
    
    // Add callback
    // TODO Use priority to place the callback into
    //      the correct position
    list_add((*hook)->callbacks, callback);
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
        // but HOOK_ERROR will also stop parsing
        if (status != HOOK_DECLINED) {
            return status;
        }
    }

    return HOOK_DECLINED;
}
