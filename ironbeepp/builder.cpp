#include <ironbeepp/internal/builder.hpp>

#define IBPP_EXPOSE_C
#include <ironbeepp/engine.hpp>
#include <ironbeepp/module.hpp>
#include <ironbeepp/context.hpp>

#include "module_data.hpp"
#include "engine_data.hpp"
#include "context_data.hpp"

#include <boost/make_shared.hpp>

using boost::make_shared;

namespace IronBee {
namespace Internal {

Engine Builder::engine( ib_engine_t* ib_engine )
{
    Engine::data_t data = make_shared<EngineData>();
    data->ib_engine = ib_engine;
    return Engine( data );
}

Module Builder::module( ib_module_t* ib_module )
{
    Module::data_t data = make_shared<ModuleData>();
    data->ib_module = ib_module;
    return Module( data );
}

Context Builder::context( ib_context_t* ib_context )
{
    Context::data_t data = make_shared<ContextData>();
    data->ib_context = ib_context;
    return Context( data );
}

} // Internal
} // IronBee
