#define IBPP_EXPOSE_C
#include <ironbeepp/context.hpp>
#include "context_data.hpp"

#include <ironbee/engine.h>

namespace IronBee {

Context::Context( const Context::data_t& data ) :
    m_data( data )
{
    // nop
}

ib_context_t* Context::ib()
{
    return m_data->ib_context;
}

} // IronBee
