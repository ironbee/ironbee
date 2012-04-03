#include <ironbeepp/site.hpp>
#include <ironbeepp/internal/throw.hpp>

#include <ironbee/engine.h>

namespace IronBee {

// ConstLocation
ConstLocation::ConstLocation() :
    m_ib(NULL)
{
    // nop
}

ConstLocation::ConstLocation(ib_type ib_location) :
    m_ib(ib_location)
{
    // nop
}

Site ConstLocation::site() const
{
    return Site(ib()->site);
}

const char* ConstLocation::path() const
{
    return ib()->path;
}

// Location
Location Location::remove_const(ConstLocation location)
{
    return Location(const_cast<ib_type>(location.ib()));
}

Location::Location() :
    m_ib(NULL)
{
    // nop
}

Location::Location(ib_type ib_location) :
    ConstLocation(ib_location),
    m_ib(ib_location)
{
    // nop
}

void Location::set_path(const char* new_path) const
{
    m_ib->path = new_path;
}

std::ostream& operator<<(std::ostream& o, const ConstLocation& location)
{
    if (! location) {
        o << "IronBee::Location[!singular!]";
    }
    else {
        o << "IronBee::Location[" << location.path() << "]";
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

const boost::uuids::uuid& ConstSite::id() const
{
    // boost::uuid::uuid is POD compatible with binary forms.
    return *reinterpret_cast<const boost::uuids::uuid*>(&(ib()->id));
}

const char* ConstSite::id_as_s() const
{
    return ib()->id_str;
}

Engine ConstSite::engine() const
{
    return Engine(ib()->ib);
}

MemoryPool ConstSite::memory_pool() const
{
    return MemoryPool(ib()->mp);
}

const char* ConstSite::name() const
{
    return ib()->name;
}

List<const char*> ConstSite::ips() const
{
    return List<const char*>(m_ib->ips);
}

List<const char*> ConstSite::hosts() const
{
    return List<const char*>(m_ib->hosts);
}

List<Location> ConstSite::locations() const
{
    return List<Location>(m_ib->locations);
}

Location ConstSite::default_location() const
{
    return Location(m_ib->default_loc);
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

Site Site::create(
    Engine      engine,
    const char* name
)
{
    ib_site_t* ib_site = NULL;
    Internal::throw_if_error(ib_site_create(&ib_site, engine.ib(), name));
    return Site(ib_site);
}

void Site::add_ip(const char* ip) const
{
    Internal::throw_if_error(ib_site_address_add(m_ib, ip));
}

void Site::add_host(const char* hostname) const
{
    Internal::throw_if_error(ib_site_hostname_add(m_ib, hostname));
}

Location Site::create_location(const char* path) const
{
    ib_loc_t* ib_loc = NULL;
    Internal::throw_if_error(ib_site_loc_create(m_ib, &ib_loc, path));
    return Location(ib_loc);
}

Location Site::create_default_location() const
{
    ib_loc_t* ib_loc = NULL;
    Internal::throw_if_error(ib_site_loc_create_default(m_ib, &ib_loc));
    return Location(ib_loc);
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
