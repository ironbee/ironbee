/*****************************************************************************
 * Licensed to Qualys, Inc. (QUALYS) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * QUALYS licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 ****************************************************************************/

/**
 * @file
 * @brief IronBee++ &mdash; Site
 *
 * This file defines (Const)Site, a wrapper for ib_site_t and (Const)Location,
 * a wrapper for ib_loc_t..
 *
 * @remark Developers should be familiar with @ref ironbeepp to understand
 * aspects of this code, e.g., the public/non-virtual inheritance.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#ifndef __IBPP__SITE__
#define __IBPP__SITE__

#include <ironbeepp/common_semantics.hpp>
#include <ironbeepp/list.hpp>
#include <ironbeepp/engine.hpp>
#include <ironbeepp/memory_pool.hpp>

#include <boost/uuid/uuid.hpp>

#include <ostream>

// IronBee C
typedef struct ib_site_t ib_site_t;
typedef struct ib_loc_t ib_loc_t;

namespace IronBee {

class Site;

/**
 * Const Location; equivalent to a const pointer to ib_loc_t.
 *
 * Provides operators ==, !=, <, >, <=, >= and evaluation as a boolean for
 * singularity via CommonSemantics.
 *
 * See Location for discussion of locations.
 *
 * @tparam T Value type for location.
 *
 * @sa Location
 * @sa ironbeepp
 * @sa ib_loc_t
 * @nosubgrouping
 **/
class ConstLocation :
    public CommonSemantics<ConstLocation>
{
public:
    //! C Type.
    typedef const ib_loc_t* ib_type;

    /**
     * Construct singular ConstLocation.
     *
     * All behavior of a singular ConstLocation is undefined except for
     * assignment, copying, comparison, and evaluate-as-bool.
     **/
    ConstLocation();

    /**
     * @name C Interoperability
     * Methods to access underlying C types.
     **/
    ///@{

    //! const ib_loc_t accessor.
    // Intentionally inlined.
    ib_type ib() const
    {
        return m_ib;
    }

    //! Construct Location from ib_loc_t.
    explicit
    ConstLocation(ib_type ib_location);

    ///@}

    //! Site accessor.
    Site site() const;

    //! Path accessor.
    const char* path() const;

private:
    ib_type m_ib;
};

/**
 * Location; equivalent to a pointer to ib_loc_t.
 *
 * Location can be treated as ConstLocation.  See @ref ironbeepp for
 * details on IronBee++ object semantics.
 *
 * Every Site has a default location and one or more additional locations.
 * Locations store the Site they belong to and a path.
 *
 * @sa ConstLocation
 * @sa ironbeepp
 * @sa ib_loc_t
 * @nosubgrouping
 **/
class Location :
    public ConstLocation
{
public:
    //! C Type.
    typedef ib_loc_t* ib_type;

    /**
     * Remove the constness of a ConstLocation.
     *
     * @warning This is as dangerous as a @c const_cast, use carefully.
     *
     * @param[in] location ConstLocation to remove const from.
     * @returns Location pointing to same underlying location as @a location.
     **/
    static Location remove_const(ConstLocation location);

    /**
     * Construct singular Location.
     *
     * All behavior of a singular Location is undefined except for
     * assignment, copying, comparison, and evaluate-as-bool.
     **/
    Location();

    /**
     * @name C Interoperability
     * Methods to access underlying C types.
     **/
    ///@{

    //! ib_loc_t accessor.
    ib_type ib() const
    {
        return m_ib;
    }

    //! Construct Location from ib_loc_t.
    explicit
    Location(ib_type ib_location);

    ///@}

    //! Set path to @a new_path.
    void set_path(const char* new_path) const;

private:
    ib_type m_ib;
};

/**
 * Output operator for Location.
 *
 * Outputs Location[@e value] to @a o where @e value is replaced with
 * the path of the location.
 *
 * @param[in] o        Ostream to output to.
 * @param[in] location Location to output.
 * @return @a o
 **/
std::ostream& operator<<(std::ostream& o, const ConstLocation& location);

/**
 * Const Site; equivalent to a const pointer to ib_site_t.
 *
 * Provides operators ==, !=, <, >, <=, >= and evaluation as a boolean for
 * singularity via CommonSemantics.
 *
 * See Site for discussion of sites.
 *
 * @tparam T Value type for site.
 *
 * @sa Site
 * @sa ironbeepp
 * @sa ib_site_t
 * @nosubgrouping
 **/
class ConstSite :
    public CommonSemantics<ConstSite>
{
public:
    //! C Type.
    typedef const ib_site_t* ib_type;

    /**
     * Construct singular ConstSite.
     *
     * All behavior of a singular ConstSite is undefined except for
     * assignment, copying, comparison, and evaluate-as-bool.
     **/
    ConstSite();

    /**
     * @name C Interoperability
     * Methods to access underlying C types.
     **/
    ///@{

    //! const ib_site_t accessor.
    // Intentionally inlined.
    ib_type ib() const
    {
        return m_ib;
    }

    //! Construct Site from ib_site_t.
    explicit
    ConstSite(ib_type ib_site);

    ///@}

    //! ID as UUID.
    const boost::uuids::uuid& id() const;

    //! ID as string.
    const char* id_as_s() const;

    //! Associated engine.
    Engine engine() const;

    //! Associated memory pool.
    MemoryPool memory_pool() const;

    //! Name.
    const char* name() const;

    //! IPs of site.
    List<const char*> ips() const;

    //! Hosts of site.
    List<const char*> hosts() const;

    //! Locations of site.
    List<Location> locations() const;

    //! Default location.
    Location default_location() const;

private:
    ib_type m_ib;
};

/**
 * Site; equivalent to a pointer to ib_site_t.
 *
 * Site can be treated as ConstSite.  See @ref ironbeepp for
 * details on IronBee++ object semantics.
 *
 * Sites are a fundamental unit of configuration.  They contain a variety of
 * information used to identify them and one or more locations.
 *
 * @sa ConstSite
 * @sa ironbeepp
 * @sa ib_site_t
 * @nosubgrouping
 **/
class Site :
    public ConstSite
{
public:
    //! C Type.
    typedef ib_site_t* ib_type;

    /**
     * Remove the constness of a ConstSite.
     *
     * @warning This is as dangerous as a @c const_cast, use carefully.
     *
     * @param[in] site ConstSite to remove const from.
     * @returns Site pointing to same underlying site as @a site.
     **/
    static Site remove_const(ConstSite site);

    /**
     * Construct singular Site.
     *
     * All behavior of a singular Site is undefined except for
     * assignment, copying, comparison, and evaluate-as-bool.
     **/
    Site();

    /**
     * @name C Interoperability
     * Methods to access underlying C types.
     **/
    ///@{

    //! ib_site_t accessor.
    ib_type ib() const
    {
        return m_ib;
    }

    //! Construct Site from ib_site_t.
    explicit
    Site(ib_type ib_site);

    ///@}

    /**
     * Create a new site with name @a name.
     *
     * @param[in] engine Engine of site.
     * @param[in] name   Name of site.
     * @return Site.
     * @throw IronBee++ exception on failure.
     **/
    static Site create(
        Engine      engine,
        const char* name
    );

    //! Add @a ip as address.
    void add_ip(const char* ip) const;
    //! Add @a hostname as hostname.
    void add_host(const char* hostname) const;

    //! Create location @a path for site.
    Location create_location(const char* path) const;

    //! Create default location for site.
    Location create_default_location() const;

private:
    ib_type m_ib;
};

/**
 * Output operator for Site.
 *
 * Outputs Site[@e value] to @a o where @e value is replaced with
 * the name of the site.
 *
 * @param[in] o    Ostream to output to.
 * @param[in] site Site to output.
 * @return @a o
 **/
std::ostream& operator<<(std::ostream& o, const ConstSite& site);

} // IronBee

#endif
