#include "data.hpp"
#include <ironbeepp/internal/catch.hpp>

namespace IronBee {
namespace Internal {

extern "C" {

ib_status_t data_cleanup( void* data )
{
    IB_FTRACE_INIT();

    boost::any* data_any = reinterpret_cast<boost::any*>( data );
    delete data_any;

    IB_FTRACE_RET_STATUS(IB_OK);
}

} // extern "C"

} // IronBee
} // Internal
