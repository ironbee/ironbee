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
 * @brief IronBee++ --- Site
 *
 * This file defines (Const)Site, (Const)SiteHost, (Const)SiteService, and
 * (Const)SiteLocation, wrapper of ib_site_t, ib_site_host_t,
 * ib_site_service_t, and ib_site_location_t, respectively.
 *
 * Provided functionality is currently minimal.  It may be expanded once the
 * C site code matures.
 *
 * @remark Developers should be familiar with @ref ironbeepp to understand
 * aspects of this code, e.g., the public/non-virtual inheritance
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#ifndef __IBPP__SITE__
#define __IBPP__SITE__

#include <ironbeepp/abi_compatibility.hpp>
#include <ironbeepp/common_semantics.hpp>
#include <ironbeepp/engine.hpp>
#include <ironbeepp/memory_manager.hpp>

#include <boost/uuid/uuid.hpp>

#include <ostream>

// IronBee C
typedef struct ib_site_t ib_site_t;
typedef struct ib_site_host_t ib_site_host_t;
typedef struct ib_site_service_t ib_site_service_t;
typedef struct ib_site_location_t ib_site_location_t;

namespace IronBee {

class ConstSite;

/**
 * Const SiteHost; equivalent to a const pointer to ib_site_host_t.
 *
 * Provides operators ==, !=, <, >, <=, >= and evaluation as a boolean for
 * singularity via CommonSemantics.
 *
 * @sa SiteHost
 * @sa ironbeepp
 * @sa ib_site_host_t
 * @nosubgrouping
 **/
class ConstSiteHost :
    public CommonSemantics<ConstSiteHost>
{
public:
    //! C Type.
    typedef const ib_site_host_t* ib_type;

    /**
     * Construct singular ConstSiteHost.
     *
     * All behavior of a singular ConstSiteHost is undefined except for
     * assignment, copying, comparison, and evaluate-as-bool.
     **/
    ConstSiteHost();

    /**
     * @name C Interoperability
     * Methods to access underlying C types.
     **/
    ///@{

    //! const ib_site_host_t accessor.
    // Intentionally inlined.
    ib_type ib() const
    {
        return m_ib;
    }

    //! Construct SiteHost from ib_site_host_t.
    explicit
    ConstSiteHost(ib_type ib_site_host);

    ///@}

    //! Site accessor.
    ConstSite site() const;

    //! Hostname accessor.
    const char* hostname() const;

    //! Suffix accessor.
    const char* suffix() const;

private:
    ib_type m_ib;
};

/**
 * SiteHost; equivalent to a pointer to ib_site_host_t.
 *
 * SiteHost can be treated as ConstSiteHost.  See @ref ironbeepp for
 * details on IronBee++ object semantics.
 *
 * Provides no functionality besides non-const ib() access.
 *
 * @sa ConstSiteHost
 * @sa ironbeepp
 * @sa ib_site_host_t
 * @nosubgrouping
 **/
class SiteHost :
    public ConstSiteHost
{
public:
    //! C Type.
    typedef ib_site_host_t* ib_type;

    /**
     * Remove the constness of a ConstSiteHost.
     *
     * @warning This is as dangerous as a @c const_cast, use carefully.
     *
     * @param[in] site_host ConstSiteHost to remove const from.
     * @returns SiteHost pointing to same underlying sitehost as @a site_host.
     **/
    static SiteHost remove_const(ConstSiteHost site_host);

    /**
     * Construct singular SiteHost.
     *
     * All behavior of a singular SiteHost is undefined except for
     * assignment, copying, comparison, and evaluate-as-bool.
     **/
    SiteHost();

    /**
     * @name C Interoperability
     * Methods to access underlying C types.
     **/
    ///@{

    //! ib_site_host_t accessor.
    ib_type ib() const
    {
        return m_ib;
    }

    //! Construct SiteHost from ib_site_host_t.
    explicit
    SiteHost(ib_type ib_site_host);

    ///@}

private:
    ib_type m_ib;
};

/**
 * Output operator for SiteHost.
 *
 * Output IronBee::SiteHost[@e value] where @e value is the hostname.
 *
 * @param[in] o Ostream to output to.
 * @param[in] site_host SiteHost to output.
 * @return @a o
 **/
std::ostream& operator<<(std::ostream& o, const ConstSiteHost& site_host);

/**
 * Const SiteService; equivalent to a const pointer to ib_site_service_t.
 *
 * Provides operators ==, !=, <, >, <=, >= and evaluation as a boolean for
 * singularity via CommonSemantics.
 *
 * @sa SiteService
 * @sa ironbeepp
 * @sa ib_site_service_t
 * @nosubgrouping
 **/
class ConstSiteService :
    public CommonSemantics<ConstSiteService>
{
public:
    //! C Type.
    typedef const ib_site_service_t* ib_type;

    /**
     * Construct singular ConstSiteService.
     *
     * All behavior of a singular ConstSiteService is undefined except for
     * assignment, copying, comparison, and evaluate-as-bool.
     **/
    ConstSiteService();

    /**
     * @name C Interoperability
     * Methods to access underlying C types.
     **/
    ///@{

    //! const ib_site_service_t accessor.
    // Intentionally inlined.
    ib_type ib() const
    {
        return m_ib;
    }

    //! Construct SiteService from ib_site_service_t.
    explicit
    ConstSiteService(ib_type ib_site_service);

    ///@}

    //! Site accessor.
    ConstSite site() const;

    //! IP address accessor.
    const char* ip_as_s() const;

    //! Port accessor.
    int port() const;

private:
    ib_type m_ib;
};

/**
 * SiteService; equivalent to a pointer to ib_site_service_t.
 *
 * SiteService can be treated as ConstSiteService.  See @ref ironbeepp for
 * details on IronBee++ object semantics.
 *
 * Provides no functionality besides non-const ib() access.
 *
 * @sa ConstSiteService
 * @sa ironbeepp
 * @sa ib_site_service_t
 * @nosubgrouping
 **/
class SiteService :
    public ConstSiteService
{
public:
    //! C Type.
    typedef ib_site_service_t* ib_type;

    /**
     * Remove the constness of a ConstSiteService.
     *
     * @warning This is as dangerous as a @c const_cast, use carefully.
     *
     * @param[in] site_service ConstSiteService to remove const from.
     * @returns SiteService pointing to same underlying site service as @a site_service.
     **/
    static SiteService remove_const(ConstSiteService site_service);

    /**
     * Construct singular SiteService.
     *
     * All behavior of a singular SiteService is undefined except for
     * assignment, copying, comparison, and evaluate-as-bool.
     **/
    SiteService();

    /**
     * @name C Interoperability
     * Methods to access underlying C types.
     **/
    ///@{

    //! ib_site_service_t accessor.
    ib_type ib() const
    {
        return m_ib;
    }

    //! Construct SiteService from ib_site_service_t.
    explicit
    SiteService(ib_type ib_site_service);

    ///@}

private:
    ib_type m_ib;
};

/**
 * Const SiteLocation; equivalent to a const pointer to ib_site_location_t.
 *
 * Provides operators ==, !=, <, >, <=, >= and evaluation as a boolean for
 * singularity via CommonSemantics.
 *
 * @tparam T Value type for location.
 *
 * @sa SiteLocation
 * @sa ironbeepp
 * @sa ib_site_location_t
 * @nosubgrouping
 **/
class ConstSiteLocation :
    public CommonSemantics<ConstSiteLocation>
{
public:
    //! C Type.
    typedef const ib_site_location_t* ib_type;

    /**
     * Construct singular ConstSiteLocation.
     *
     * All behavior of a singular ConstSiteLocation is undefined except for
     * assignment, copying, comparison, and evaluate-as-bool.
     **/
    ConstSiteLocation();

    /**
     * @name C Interoperability
     * Methods to access underlying C types.
     **/
    ///@{

    //! const ib_site_location_t accessor.
    // Intentionally inlined.
    ib_type ib() const
    {
        return m_ib;
    }

    //! Construct SiteLocation from ib_site_location_t.
    explicit
    ConstSiteLocation(ib_type ib_location);

    ///@}

    //! Site accessor.
    ConstSite site() const;

    //! Path accessor.
    const char* path() const;

    //! Context accessor.
    Context context() const;

private:
    ib_type m_ib;
};

/**
 * SiteLocation; equivalent to a pointer to ib_site_location_t.
 *
 * SiteLocation can be treated as ConstSiteLocation.  See @ref ironbeepp for
 * details on IronBee++ object semantics.
 *
 * Provides no functionality besides non-const ib() access.
 *
 * @sa ConstSiteLocation
 * @sa ironbeepp
 * @sa ib_site_location_t
 * @nosubgrouping
 **/
class SiteLocation :
    public ConstSiteLocation
{
public:
    //! C Type.
    typedef ib_site_location_t* ib_type;

    /**
     * Remove the constness of a ConstSiteLocation.
     *
     * @warning This is as dangerous as a @c const_cast, use carefully.
     *
     * @param[in] location ConstSiteLocation to remove const from.
     * @returns SiteLocation pointing to same underlying location as @a location.
     **/
    static SiteLocation remove_const(ConstSiteLocation location);

    /**
     * Construct singular SiteLocation.
     *
     * All behavior of a singular SiteLocation is undefined except for
     * assignment, copying, comparison, and evaluate-as-bool.
     **/
    SiteLocation();

    /**
     * @name C Interoperability
     * Methods to access underlying C types.
     **/
    ///@{

    //! ib_site_location_t accessor.
    ib_type ib() const
    {
        return m_ib;
    }

    //! Construct SiteLocation from ib_site_location_t.
    explicit
    SiteLocation(ib_type ib_location);

    ///@}

private:
    ib_type m_ib;
};

/**
 * Output operator for SiteLocation.
 *
 * Outputs SiteLocation[@e value] to @a o where @e value is replaced with
 * the path of the location.
 *
 * @param[in] o        Ostream to output to.
 * @param[in] location SiteLocation to output.
 * @return @a o
 **/
std::ostream& operator<<(std::ostream& o, const ConstSiteLocation& location);

/**
 * Output operator for SiteService.
 *
 * Output IronBee::SiteService[@e value] where @e value is hostname.
 *
 * @param[in] o Ostream to output to.
 * @param[in] site_service SiteService to output.
 * @return @a o
 **/
std::ostream& operator<<(std::ostream& o, const ConstSiteService& site_service);

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

    //! ID.
    const char* id() const;

    //! Associated memory manager.
    MemoryManager memory_manager() const;

    //! Name.
    const char* name() const;

    //! Context.
    Context context() const;

private:
    ib_type m_ib;
};

/**
 * Site; equivalent to a pointer to ib_site_t.
 *
 * Site can be treated as ConstSite.  See @ref ironbeepp for
 * details on IronBee++ object semantics.
 *
 * Provides no functionality besides non-const ib() access.
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
