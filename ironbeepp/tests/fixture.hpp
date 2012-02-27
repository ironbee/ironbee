#ifndef __IBPP__TESTS__FIXTURE__
#define __IBPP__TESTS__FIXTURE__

#include <ironbee/types.h>

#include "ironbee_private.h"

class IBPPTestFixture
{
public:
    IBPPTestFixture();
    ~IBPPTestFixture();

protected:
    ib_engine_t* m_ib_engine;
    ib_plugin_t  m_ib_plugin;
};

#endif