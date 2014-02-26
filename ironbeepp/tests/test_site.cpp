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
 * @brief IronBee++ Internals --- Site Tests
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 **/

#include <ironbeepp/site.hpp>
#include <ironbeepp/test_fixture.hpp>

#include "gtest/gtest.h"

#include <ironbee/engine.h>
#include <ironbee/site.h>

#include <ironbeepp/context.hpp>
#include <ironbeepp/throw.hpp>

using namespace std;
using namespace IronBee;

class TestSite : public ::testing::Test, public TestFixture
{
public:
    TestSite()
    {
        ib_status_t rc;

        ib_context_t* ib_context = NULL;

        rc = ib_context_create(
            m_engine.ib(),
            NULL,
            IB_CTYPE_LOCATION,
            "Location",
            "TestFixtureContext",
            &ib_context
        );
        throw_if_error(rc);
        m_location_context = Context(ib_context);

        ib_context = NULL;
        rc = ib_context_create(
            m_engine.ib(),
            NULL,
            IB_CTYPE_SITE,
            "Site",
            "TestFixtureContext",
            &ib_context
        );
        throw_if_error(rc);
        m_site_context = Context(ib_context);

        ib_site_t* ib_site;
        rc = ib_site_create(
            m_site_context.ib(),
            "TestSite",
            NULL,
            NULL,
            &ib_site
        );
        throw_if_error(rc);
        m_site = Site(ib_site);

        ib_site_location_t* ib_site_location;
        rc = ib_site_location_create(
            m_site.ib(),
            m_location_context.ib(),
            "TestSiteLocation",
            NULL,
            NULL,
            &ib_site_location
        );
        throw_if_error(rc);
        m_site_location = SiteLocation(ib_site_location);

        ib_site_service_t* ib_site_service;
        rc = ib_site_service_create(
            m_site.ib(),
            "1.2.3.4:1234",
            NULL,
            NULL,
            &ib_site_service
        );
        throw_if_error(rc);
        m_site_service = SiteService(ib_site_service);

        ib_site_host_t* ib_site_host;
        rc = ib_site_host_create(
            m_site.ib(),
            "TestSiteHost",
            NULL,
            NULL,
            &ib_site_host
        );
        throw_if_error(rc);
        m_site_host = SiteHost(ib_site_host);
    }

protected:
    Context m_site_context;
    Context m_location_context;
    Site m_site;
    SiteLocation m_site_location;
    SiteService m_site_service;
    SiteHost m_site_host;
};

TEST_F(TestSite, Location)
{
    EXPECT_EQ(m_site, m_site_location.site());
    EXPECT_EQ(string("TestSiteLocation"), m_site_location.path());
    EXPECT_EQ(m_location_context, m_site_location.context());
}

TEST_F(TestSite, Site)
{
    EXPECT_EQ(string("TestSite"), m_site.name());
    EXPECT_TRUE(m_site.memory_manager());
    EXPECT_EQ(m_site_context, m_site.context());
}

TEST_F(TestSite, Host)
{
    EXPECT_EQ(m_site, m_site_host.site());
    EXPECT_EQ(string("TestSiteHost"), m_site_host.hostname());
}

TEST_F(TestSite, Service)
{
    EXPECT_EQ(m_site, m_site_service.site());
    EXPECT_EQ(string("1.2.3.4"), m_site_service.ip_as_s());
    EXPECT_EQ(1234, m_site_service.port());
}
