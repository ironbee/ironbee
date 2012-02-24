#ifndef __IBPP_ENGINE_DATA__
#define __IBPP_ENGINE_DATA__

struct ib_engine_t;

namespace IronBee {
namespace Internal {

struct EngineData
{
    ib_engine_t* ib_engine;
};

} // Impl
} // IronBee

#endif