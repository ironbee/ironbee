//////////////////////////////////////////////////////////////////////////////
// Licensed to Qualys, Inc. (QUALYS) under one or more
// contributor license agreements.  See the NOTICE file distributed with
// this work for additional information regarding copyright ownership.
// QUALYS licenses this file to You under the Apache License, Version 2.0
// (the "License"); you may not use this file except in compliance with
// the License.  You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
/// @file
/// @brief IronBee --- Configuration Mapping Test
///
/// @author Nick LeRoy <nleroy@qualys.com>
//////////////////////////////////////////////////////////////////////////////

#include <ironbee/hash.h>

#include "ironbee_config_auto.h"
#include "simple_fixture.hpp"

#include "gtest/gtest.h"

#include <ironbee/mm.h>
#include <ironbee/field.h>
#include <ironbee/cfgmap.h>

#include <stdexcept>

struct test_config_t {
    const char  *str1;     /**< String #1 */
    const char  *str2;     /**< String #2 */
    ib_num_t     num1;     /**< Number #1 */
    ib_num_t     num2;     /**< Number #2 */
};

static IB_CFGMAP_INIT_STRUCTURE(config_map) = {
    IB_CFGMAP_INIT_ENTRY(
        "str1",
        IB_FTYPE_NULSTR,
        test_config_t,
        str1),

    IB_CFGMAP_INIT_ENTRY(
        "str2",
        IB_FTYPE_NULSTR,
        test_config_t,
        str2),

    IB_CFGMAP_INIT_ENTRY(
        "num1",
        IB_FTYPE_NUM,
        test_config_t,
        num1),

    IB_CFGMAP_INIT_ENTRY(
        "num2",
        IB_FTYPE_NUM,
        test_config_t,
        num2),

    IB_CFGMAP_INIT_LAST
};

TEST_F(SimpleFixture, test_init)
{
    ib_cfgmap_t   *cfgmap = NULL;
    test_config_t  config;
    ib_status_t    rc;

    rc = ib_cfgmap_create(&cfgmap, MM());
    ASSERT_EQ(IB_OK, rc);
    ASSERT_TRUE(cfgmap);

    rc = ib_cfgmap_init(cfgmap, &config, config_map);
    ASSERT_EQ(IB_OK, rc);
}

class TestIBUtilCfgMap : public SimpleFixture
{
public:
    virtual void SetUp()
    {
        ib_status_t rc;
        SimpleFixture::SetUp();

        m_cfgmap = NULL;
        rc = ib_cfgmap_create(&m_cfgmap, MM());
        if (rc != IB_OK) {
            throw std::runtime_error("Could not create config map.");
        }

        rc = ib_cfgmap_init(m_cfgmap, &m_config, config_map);
        if (rc != IB_OK) {
            throw std::runtime_error("Could not initialize config map.");
        }
    }

    virtual void TearDown()
    {
        SimpleFixture::TearDown();
    }

protected:
    ib_cfgmap_t   *m_cfgmap;
    test_config_t  m_config;
};

TEST_F(TestIBUtilCfgMap, test_get)
{
    ib_status_t rc;
    const char *s;
    ib_num_t n;

    rc = ib_cfgmap_get(m_cfgmap, "xyzzy", ib_ftype_nulstr_out(&s), NULL);
    ASSERT_EQ(IB_ENOENT, rc);

    m_config.str1 = "abc";
    rc = ib_cfgmap_get(m_cfgmap, "str1", ib_ftype_nulstr_out(&s), NULL);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_EQ(s, m_config.str1);

    m_config.num1 = 1234;
    rc = ib_cfgmap_get(m_cfgmap, "num1", ib_ftype_num_out(&n), NULL);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_EQ(n, m_config.num1);
}

TEST_F(TestIBUtilCfgMap, test_set)
{
    ib_status_t rc;
    const char *s1;
    const char *s2;
    ib_num_t n1;
    ib_num_t n2;

    s1 = "xyzzy";
    rc = ib_cfgmap_set(m_cfgmap, "xyzzy", ib_ftype_nulstr_in(s1));
    ASSERT_EQ(IB_ENOENT, rc);

    s1 = "abcdef";
    rc = ib_cfgmap_set(m_cfgmap, "str1", ib_ftype_nulstr_in(s1));
    ASSERT_EQ(IB_OK, rc);
    ASSERT_STREQ(s1, m_config.str1);

    s1 = "xyzzy";
    rc = ib_cfgmap_set(m_cfgmap, "str1", ib_ftype_nulstr_in(s1));
    ASSERT_EQ(IB_OK, rc);
    ASSERT_STREQ(s1, m_config.str1);

    s2 = "lwpi";
    rc = ib_cfgmap_set(m_cfgmap, "str2", ib_ftype_nulstr_in(s2));
    ASSERT_EQ(IB_OK, rc);
    ASSERT_STREQ(s2, m_config.str2);
    ASSERT_STREQ(s1, m_config.str1);

    n1 = 1234;
    rc = ib_cfgmap_set(m_cfgmap, "num1", ib_ftype_num_in(&n1));
    ASSERT_EQ(IB_OK, rc);
    ASSERT_EQ(n1, m_config.num1);

    n1 = 5678;
    rc = ib_cfgmap_set(m_cfgmap, "num1", ib_ftype_num_in(&n1));
    ASSERT_EQ(IB_OK, rc);
    ASSERT_EQ(n1, m_config.num1);

    n2 = 666;
    rc = ib_cfgmap_set(m_cfgmap, "num2", ib_ftype_num_in(&n2));
    ASSERT_EQ(IB_OK, rc);
    ASSERT_EQ(n2, m_config.num2);
    ASSERT_EQ(n1, m_config.num1);
}
