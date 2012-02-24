#ifndef __IBPP_CONTEXT_DATA__
#define __IBPP_CONTEXT_DATA__

struct ib_context_t;

namespace IronBee {
namespace Internal {

struct ContextData
{
    ib_context_t* ib_context;
};

} // Impl
} // IronBee

#endif