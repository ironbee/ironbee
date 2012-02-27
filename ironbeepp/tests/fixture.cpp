#include "fixture.hpp"

#include <stdexcept>

IBPPTestFixture::IBPPTestFixture()
{
    m_ib_plugin.vernum   = IB_VERNUM;
    m_ib_plugin.abinum   = IB_ABINUM;
    m_ib_plugin.version  = IB_VERSION;
    m_ib_plugin.filename = __FILE__;
    m_ib_plugin.name     = "IBPPTest";

    ib_initialize();

    ib_status_t rc = ib_engine_create(&m_ib_engine, &m_ib_plugin);
    if ( rc != IB_OK ) {
        throw std::runtime_error("ib_engine_create failed.");
    }
    rc = ib_engine_init(m_ib_engine);
    if ( rc != IB_OK ) {
        throw std::runtime_error("ib_engine_init failed.");
    }
}

IBPPTestFixture::~IBPPTestFixture()
{
    ib_shutdown();
}
