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
 *****************************************************************************/

#ifndef _HOOKS_H
#define	_HOOKS_H

#include "dslib.h"

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
typedef int (*htp_callback_fn_t) (void *);

#ifdef __cplusplus
extern "C" {
#endif

struct htp_hook_t {
    list_t *callbacks;
};

struct htp_callback_t {
    htp_callback_fn_t fn;
};

 int hook_register(htp_hook_t **hook, htp_callback_fn_t callback_fn);
 int hook_run_one(htp_hook_t *hook, void *data);
 int hook_run_all(htp_hook_t *hook, void *data);

htp_hook_t *hook_create(void);
htp_hook_t *hook_copy(htp_hook_t *hook);
       void hook_destroy(htp_hook_t *hook);


#ifdef __cplusplus
}
#endif

#endif	/* _HOOKS_H */

