#ifndef _MOCK_MODULE_H_
#define _MOCK_MODULE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <ironbee/module.h>

struct mock_module_conf_t {
    const char *param1_p1;
    const char *param2_p1;
    const char *param2_p2;
    const ib_list_t *list_params;
    bool blkend_called;
    int onoff_onoff;
    const char *sblk1_p1;
    ib_flags_t opflags_val;
    ib_flags_t opflags_mask;
};
typedef struct mock_module_conf_t mock_module_conf_t;

/**
 * @param[in] ib IronBee to register this module and initalize.
 *
 * @returns Result of ib_module_init.
 */
ib_status_t mock_module_register(ib_engine_t *ib);

/**
 * Return the module name.
 * @returns the module name.
 */
const char *mock_module_name();

#ifdef __cplusplus
}
#endif

#endif // _MOCK_MODULE_H_
