#include "engine_shutdown.hpp"

#include <ironbeepp/module_bootstrap.hpp>

EngineShutdownModule::EngineShutdownModule(IronBee::Module module) :
    IronBee::ModuleDelegate(module)
{}

IBPP_BOOTSTRAP_MODULE("EngineShutdownModule", EngineShutdownModule);
