#define IBPP_EXPOSE_C
#include <ironbeepp/engine.hpp>
#include "engine_data.hpp"

#include <ironbee/engine.h>

namespace IronBee {

Engine::Engine( const Engine::data_t& data ) :
    m_data( data )
{
    // nop
}

ib_engine_t* Engine::ib()
{
    return m_data->ib_engine;
}

} // IronBee
