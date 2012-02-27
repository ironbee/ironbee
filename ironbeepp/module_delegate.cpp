#include <ironbeepp/module_delegate.hpp>

namespace IronBee {

ModuleDelegate::ModuleDelegate( Module module ) :
    m_module( module )
{
    // nop
}

void ModuleDelegate::context_open( Context context )
{
    // nop
}

void ModuleDelegate::context_close( Context context )
{
    // nop
}

void ModuleDelegate::context_destroy( Context context )
{
    // nop
}

} // IronBee
