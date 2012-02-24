#ifndef __IBPP_BUILDER__
#define __IBPP_BUILDER__

struct ib_module_t;
struct ib_engine_t;
struct ib_context_t;

namespace IronBee {

class Module;
class Engine;
class Context;

namespace Internal {

class Builder
{
public:
    static Engine engine(   ib_engine_t*  ib_engine  );
    static Module  module(  ib_module_t*  ib_module  );
    static Context context( ib_context_t* ib_context );
};

} // Internal
} // IronBee
#endif
