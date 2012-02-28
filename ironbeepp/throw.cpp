#include "throw.hpp"
#include <ironbeepp/exception.hpp>

namespace IronBee {
namespace Internal {

void throw_if_error( ib_status_t status )
{
    static const char* message = "Error reported from C API.";

    switch ( status ) {
        case IB_OK:
            return;
        case IB_DECLINED:
            BOOST_THROW_EXCEPTION( declined() << errinfo_what( message ) );
        case IB_EUNKNOWN:
            BOOST_THROW_EXCEPTION( eunknown() << errinfo_what( message ) );
        case IB_ENOTIMPL:
            BOOST_THROW_EXCEPTION( enotimpl() << errinfo_what( message ) );
        case IB_EINCOMPAT:
            BOOST_THROW_EXCEPTION( eincompat() << errinfo_what( message ) );
        case IB_EALLOC:
            BOOST_THROW_EXCEPTION( ealloc() << errinfo_what( message ) );
        case IB_EINVAL:
            BOOST_THROW_EXCEPTION( einval() << errinfo_what( message ) );
        case IB_ENOENT:
            BOOST_THROW_EXCEPTION( enoent() << errinfo_what( message ) );
        case IB_ETRUNC:
            BOOST_THROW_EXCEPTION( etrunc() << errinfo_what( message ) );
        case IB_ETIMEDOUT:
            BOOST_THROW_EXCEPTION( etimedout() << errinfo_what( message ) );
        case IB_EAGAIN:
            BOOST_THROW_EXCEPTION( eagain() << errinfo_what( message ) );
        case IB_EOTHER:
            BOOST_THROW_EXCEPTION( eother() << errinfo_what( message ) );
        default:
            BOOST_THROW_EXCEPTION(
              eother() << errinfo_what(
                std::string("Unknown status code: ") +
                ib_status_to_string( status )
              )
            );
    }
}

} // Internal
} // IronBee
