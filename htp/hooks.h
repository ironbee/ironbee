#ifndef _HOOKS_H
#define	_HOOKS_H

#include "dslib.h"

#define HOOK_FIRST      1000
#define HOOK_MIDDLE     5000
#define HOOK_LAST       9000

#ifdef _HTP_H
#define HOOK_ERROR      HTP_ERROR
#define HOOK_OK         HTP_OK
#define HOOK_DECLINED   HTP_DECLINED
#else
#define HOOK_ERROR      -1
#define HOOK_OK          0
#define HOOK_DECLINED    1
#endif

typedef struct htp_hook_t htp_hook_t;
typedef struct htp_callback_t htp_callback_t;

struct htp_hook_t {
    list_t *callbacks;
};

struct htp_callback_t {
    int (*fn)();    
    int priority;
};

void hook_register(htp_hook_t **hook, int (*callback_fn)(), int priority);
 int hook_run_one(htp_hook_t *hook, void *data);
 int hook_run_all(htp_hook_t *hook, void *data);

htp_hook_t *hook_create();
       void hook_destroy(htp_hook_t *hook);


#endif	/* _HOOKS_H */

