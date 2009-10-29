
#include "hooks.h"

/**
 *
 */
htp_hook_t *hook_create() {
   htp_hook_t *hook = calloc(1, sizeof(htp_hook_t));
   
   hook->callbacks = list_array_create(4);
   
   return hook;
}

/**
 *
 */
void hook_register(htp_hook_t **hook, int (*callback_fn)(), int priority) {
    htp_callback_t *callback = calloc(1, sizeof(htp_callback_t));
    
    callback->fn = callback_fn;    
    callback->priority = priority;

    if (*hook == NULL) {
        *hook = hook_create();
    }
    
    // TODO Use priority to place the callback into
    //      the correct position
    list_add((*hook)->callbacks, callback);
}

/**
 *
 */
int hook_run_all(htp_hook_t *hook, void *data) {
    if (hook == NULL) {
        return HOOK_OK;
    }

    htp_callback_t *callback = NULL;
    list_iterator_reset(hook->callbacks);
    while ((callback = list_iterator_next(hook->callbacks)) != NULL) {
        // TODO Do we want to stop on HOOK_ERROR?
        callback->fn(data);
    }
}

/**
 *
 */
int hook_run_one(htp_hook_t *hook, void *data) {
    if (hook == NULL) {
        return HOOK_OK;
    }

    htp_callback_t *callback = NULL;
    list_iterator_reset(hook->callbacks);
    while ((callback = list_iterator_next(hook->callbacks)) != NULL) {
        int status = callback->fn(data);
        if (status == HOOK_OK) {
            // TODO Do we want to stop on HOOK_ERROR?
            return HOOK_OK;
        }
    }
}
