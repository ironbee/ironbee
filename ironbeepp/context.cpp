#include <ironbeepp/context.hpp>
#include <ironbeepp/site.hpp>
#include <ironbeepp/engine.hpp>

#include <ironbee/context.h>
#include <ironbee/engine.h>
#include <ironbee/core.h>

namespace IronBee {

// ConstContext

ConstContext::ConstContext() :
    m_ib(NULL)
{
    // nop
}

ConstContext::ConstContext(ib_type ib_context) :
    m_ib(ib_context)
{
    // nop
}

const char* ConstContext::type() const
{
    return ib_context_type_get(ib());
}

const char* ConstContext::name() const
{
    return ib_context_name_get(ib());
}

const char* ConstContext::full_name() const
{
    return ib_context_full_get(ib());
}

Context ConstContext::parent() const
{
    return Context(ib_context_parent_get(ib()));
}

Engine ConstContext::engine() const
{
    return Engine(ib_context_get_engine(ib()));
}

ConstSite ConstContext::site() const
{
    const ib_site_t *site = NULL;
    ib_status_t rc;

    rc = ib_context_site_get(ib(), &site);
    if ((rc != IB_OK) || (site == NULL)) {
        // TODO
    }

    return ConstSite(site);
}

// Context

Context Context::remove_const(ConstContext context)
{
    return Context(const_cast<ib_type>(context.ib()));
}

Context::Context() :
    m_ib(NULL)
{
    // nop
}

Context::Context(ib_type ib_context) :
    ConstContext(ib_context),
    m_ib(ib_context)
{
    // nop
}

MemoryManager Context::memory_manager()
{
    return MemoryManager(ib_context_get_mm(ib()));
}


std::ostream& operator<<(std::ostream& o, const ConstContext& context)
{
    if (! context) {
        o << "IronBee::Context[!singular!]";
    } else {
        o << "IronBee::Context["
          << context.full_name()
          << "]";
    }
    return o;
}

} // IronBee
