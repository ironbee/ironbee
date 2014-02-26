#include <ironbeepp/site.hpp>
#include <ironbeepp/context.hpp>
#include <ironbeepp/throw.hpp>

#include <ironbee/site.h>

namespace IronBee {

// ConstSiteHost

ConstSiteHost::ConstSiteHost() :
    m_ib(NULL)
{
    // nop
}

ConstSiteHost::ConstSiteHost(ib_type ib_site_host) :
    m_ib(ib_site_host)
{
    // nop
}

ConstSite ConstSiteHost::site() const
{
    return ConstSite(ib()->site);
}

const char* ConstSiteHost::hostname() const
{
    return ib()->hostname;
}

const char* ConstSiteHost::suffix() const
{
    return ib()->suffix;
}

// SiteHost

SiteHost SiteHost::remove_const(ConstSiteHost site_host)
{
    return SiteHost(const_cast<ib_type>(site_host.ib()));
}

SiteHost::SiteHost() :
    m_ib(NULL)
{
    // nop
}

SiteHost::SiteHost(ib_type ib_site_host) :
    ConstSiteHost(ib_site_host),
    m_ib(ib_site_host)
{
    // nop
}

std::ostream& operator<<(std::ostream& o, const ConstSiteHost& site_host)
{
    if (! site_host) {
        o << "IronBee::SiteHost[!singular!]";
    } else {
        o << "IronBee::SiteHost[" << site_host.hostname() << "]";
    }
    return o;
}

// ConstSiteService

ConstSiteService::ConstSiteService() :
    m_ib(NULL)
{
    // nop
}

ConstSiteService::ConstSiteService(ib_type ib_site_service) :
    m_ib(ib_site_service)
{
    // nop
}

ConstSite ConstSiteService::site() const
{
    return ConstSite(ib()->site);
}

const char* ConstSiteService::ip_as_s() const
{
    return ib()->ipstr;
}

int ConstSiteService::port() const
{
    return ib()->port;
}

// SiteService

SiteService SiteService::remove_const(ConstSiteService site_service)
{
    return SiteService(const_cast<ib_type>(site_service.ib()));
}

SiteService::SiteService() :
    m_ib(NULL)
{
    // nop
}

SiteService::SiteService(ib_type ib_site_service) :
    ConstSiteService(ib_site_service),
    m_ib(ib_site_service)
{
    // nop
}

std::ostream& operator<<(std::ostream& o, const ConstSiteService& site_service)
{
    if (! site_service) {
        o << "IronBee::SiteService[!singular!]";
    } else {
        o << "IronBee::SiteService[" << site_service.ip_as_s() << ":"
          << site_service.port() << "]";
    }
    return o;
}

// ConstSiteLocation
ConstSiteLocation::ConstSiteLocation() :
    m_ib(NULL)
{
    // nop
}

ConstSiteLocation::ConstSiteLocation(ib_type ib_location) :
    m_ib(ib_location)
{
    // nop
}

ConstSite ConstSiteLocation::site() const
{
    return ConstSite(ib()->site);
}

const char* ConstSiteLocation::path() const
{
    return ib()->path;
}

Context ConstSiteLocation::context() const
{
    return Context(ib()->context);
}

// SiteLocation
SiteLocation SiteLocation::remove_const(ConstSiteLocation location)
{
    return SiteLocation(const_cast<ib_type>(location.ib()));
}

SiteLocation::SiteLocation() :
    m_ib(NULL)
{
    // nop
}

SiteLocation::SiteLocation(ib_type ib_location) :
    ConstSiteLocation(ib_location),
    m_ib(ib_location)
{
    // nop
}

std::ostream& operator<<(std::ostream& o, const ConstSiteLocation& location)
{
    if (! location) {
        o << "IronBee::SiteLocation[!singular!]";
    }
    else {
        o << "IronBee::SiteLocation[" << location.path() << "]";
    }
    return o;
}

// ConstSite
ConstSite::ConstSite() :
    m_ib(NULL)
{
    // nop
}

ConstSite::ConstSite(ib_type ib_site) :
    m_ib(ib_site)
{
    // nop
}

const char* ConstSite::id() const
{
    return ib()->id;
}

MemoryManager ConstSite::memory_manager() const
{
    return MemoryManager(ib()->mm);
}

const char* ConstSite::name() const
{
    return ib()->name;
}

Context ConstSite::context() const
{
    return Context(ib()->context);
}

// Site

Site Site::remove_const(ConstSite site)
{
    return Site(const_cast<ib_type>(site.ib()));
}

Site::Site() :
    m_ib(NULL)
{
    // nop
}

Site::Site(ib_type ib_site) :
    ConstSite(ib_site),
    m_ib(ib_site)
{
    // nop
}

std::ostream& operator<<(std::ostream& o, const ConstSite& site)
{
    if (! site) {
        o << "IronBee::Site[!singular!]";
    }
    else {
        o << "IronBee::Site[" << site.name() << "]";
    }
    return o;
}

} // IronBee
