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
/// @brief IronBee &dso; Dynamic Shared Object Tests
///
/// @author Nick LeRoy <nleroy@qualys.com>
//////////////////////////////////////////////////////////////////////////////

#include <ironbee/hash.h>

#include "ironbee_config_auto.h"

#include "gtest/gtest.h"
#include "gtest/gtest-spi.h"

#include <ironbee/types.h>
#include <ironbee/dso.h>

#include "test_util_dso.h"

#include <stdexcept>

#if defined(__APPLE__) && defined(__MACH__)
#define DSO_SUFFIX ".dylib"
#else
#define DSO_SUFFIX ".so"
#endif

class TestIBUtilDso : public ::testing::Test
{
public:
    TestIBUtilDso( ) : m_dso(NULL) { };

    ~TestIBUtilDso( )
    {
        DsoClose( );
    }

    virtual void SetUp()
    {
        ib_status_t rc = ib_mpool_create(&m_pool, NULL, NULL);
        if (rc != IB_OK) {
            throw std::runtime_error("Could not initialize mpool.");
        }
    }

    virtual void TearDown()
    {
        ib_mpool_destroy(m_pool);
    }

    ib_status_t DsoOpen(const char *file)
    {
        return ib_dso_open(&m_dso, file, m_pool);
    }
    ib_status_t DsoClose( void )
    {
        ib_status_t rc = IB_OK;
        if (m_dso != NULL) {
            rc = ib_dso_close(m_dso);
            m_dso = NULL;
        }
        return rc;
    }
    ib_status_t DsoSymFind(const char *name, ib_dso_sym_t **sym)
    {
        return ib_dso_sym_find(sym, m_dso, name);
    }

protected:
    ib_mpool_t    *m_pool;
    ib_dso_t      *m_dso;
};

TEST_F(TestIBUtilDso, test_open)
{
    {
        SCOPED_TRACE("test_open: normal");
        ib_status_t rc;
        rc = DsoOpen(".libs/libtest_util_dso_lib" DSO_SUFFIX);
        ASSERT_EQ(IB_OK, rc);

        rc = DsoClose( );
        ASSERT_EQ(IB_OK, rc);
    }

    {
        SCOPED_TRACE("test_open: does not exist");
        ib_status_t rc;
        rc = DsoOpen(".libs/libtest_doesnotexist" DSO_SUFFIX);
        ASSERT_EQ(IB_EINVAL, rc);

        rc = DsoClose( );
        ASSERT_EQ(IB_OK, rc);
    }

}

TEST_F(TestIBUtilDso, test_sym_find)
{
    ib_status_t   rc;
    ib_dso_sym_t *sym;

    rc = DsoOpen(".libs/libtest_util_dso_lib" DSO_SUFFIX);
    ASSERT_EQ(IB_OK, rc);

    {
        SCOPED_TRACE("test_sym_find: does not exist");
        rc = DsoSymFind("does_not_exit", &sym);
        ASSERT_EQ(IB_ENOENT, rc);
    }

    {
        SCOPED_TRACE("test_sym_find: normal");
        rc = DsoSymFind("ib_test_util_dso_getfns", &sym);
        ASSERT_EQ(IB_OK, rc);
    }

    rc = DsoClose( );
    ASSERT_EQ(IB_OK, rc);
}

TEST_F(TestIBUtilDso, test_lib)
{
    ib_status_t              rc;
    ib_dso_sym_t            *sym;
    ib_test_dso_getfns_fn_t  getfns;
    ib_test_util_dso_fns_t  *fns;
    ib_test_util_dso_data_t *data;
    int                      num;
    const char              *str;

    rc = DsoOpen(".libs/libtest_util_dso_lib" DSO_SUFFIX);
    ASSERT_EQ(IB_OK, rc);

    rc = DsoSymFind("ib_test_util_dso_getfns", &sym);
    ASSERT_EQ(IB_OK, rc);

    getfns = (ib_test_dso_getfns_fn_t)sym;
    rc = getfns(&fns);
    ASSERT_EQ(IB_OK, rc);

    rc = fns->fn_create(&data, m_pool, 3);
    ASSERT_EQ(IB_OK, rc);

    rc = fns->fn_getnum(data, &num);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_EQ(3, num);

    rc = fns->fn_setnum(data, 666);
    ASSERT_EQ(IB_OK, rc);

    rc = fns->fn_getnum(data, &num);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_EQ(666, num);

    rc = fns->fn_getstr(data, &str);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_STREQ(NULL, str);

    rc = fns->fn_setstr(data, "abc123");
    ASSERT_EQ(IB_OK, rc);

    rc = fns->fn_getstr(data, &str);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_STREQ("abc123", str);

    rc = fns->fn_getnum(data, &num);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_EQ(666, num);

    rc = fns->fn_destroy(data);
    ASSERT_EQ(IB_OK, rc);

    rc = DsoClose( );
    ASSERT_EQ(IB_OK, rc);
}
