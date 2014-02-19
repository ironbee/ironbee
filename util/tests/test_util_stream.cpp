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
/// @brief IronBee --- Stream utility tests
///
/// @author Nick LeRoy <nleroy@qualys.com>
//////////////////////////////////////////////////////////////////////////////

#include <ironbee/hash.h>

#include "ironbee_config_auto.h"

#include "gtest/gtest.h"
#include "simple_fixture.hpp"

#include <ironbee/types.h>
#include <ironbee/stream.h>

#include <stdexcept>

TEST_F(SimpleFixture, test_create)
{
    ib_status_t  rv;
    ib_stream_t *stream;

    rv = ib_stream_create(&stream, MM());
    ASSERT_EQ(IB_OK, rv);
}

class TestStream : public SimpleFixture
{
public :
    void SetUp( void )
    {
        SimpleFixture::SetUp( );

        ib_status_t rc = ib_stream_create(&m_stream, MM());
        if (rc != IB_OK) {
            throw std::runtime_error("Could not initialize stream.");
        }
    }
    void TearDown( void )
    {
        SimpleFixture::TearDown( );
    }

    ib_status_t Push(ib_sdata_type_t type, const char *str, size_t len)
    {
        return ib_stream_push(m_stream, type, ib_mm_strdup(MM(), str), len);
    }

    ib_status_t PushSdata(ib_sdata_type_t type, const char *str, size_t len)
    {
        ib_sdata_t  *sdata;

        sdata = (ib_sdata_t *)ib_mm_alloc(MM(), sizeof(*sdata));
        sdata->type = type;
        sdata->dlen = len;
        sdata->data = ib_mm_strdup(MM(), str);

        return ib_stream_push_sdata(m_stream, sdata);
    }

    ib_status_t Pull(ib_sdata_t **sdata)
    {
        return ib_stream_pull(m_stream, sdata);
    }

protected:
    ib_stream_t *m_stream;
};

TEST_F(TestStream, test_simple)
{
    ib_sdata_t  *sdata;
    ib_status_t  rc;

    rc = ib_stream_pull(m_stream, &sdata);
    ASSERT_EQ(IB_ENOENT, rc);
}

TEST_F(TestStream, test_push)
{
    const char  *str = "Test Data";
    size_t       len = strlen(str) + 1;
    ib_sdata_t  *sdata;
    ib_status_t  rc;

    rc = Push(IB_STREAM_DATA, str, len);
    ASSERT_EQ(IB_OK, rc);

    rc = ib_stream_peek(m_stream, &sdata);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_EQ(IB_STREAM_DATA, sdata->type);
    ASSERT_EQ(len, sdata->dlen);
    ASSERT_STREQ(str, (char *)sdata->data);

    rc = ib_stream_pull(m_stream, &sdata);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_EQ(IB_STREAM_DATA, sdata->type);
    ASSERT_EQ(len, sdata->dlen);
    ASSERT_STREQ(str, (char *)sdata->data);
    ASSERT_EQ((size_t)0, m_stream->slen);
}

TEST_F(TestStream, test_push_sdata)
{
    const char  *str = "Test Data";
    size_t       len = strlen(str) + 1;
    ib_sdata_t  *sdata;
    ib_status_t  rc;

    rc = PushSdata(IB_STREAM_DATA, str, len);
    ASSERT_EQ(IB_OK, rc);

    rc = ib_stream_pull(m_stream, &sdata);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_EQ(IB_STREAM_DATA, sdata->type);
    ASSERT_EQ(len, sdata->dlen);
    ASSERT_STREQ(str, (char *)sdata->data);
}

TEST_F(TestStream, test_multiple)
{
    const char  *hdrbuf =
        "GET / HTTP/1.1\r\n"
        "Host: UnitTest\r\n"
        "X-MyHeader: header1\r\n"
        "X-MyHeader: header2\r\n"
        "\r\n";
    size_t       hdrlen = strlen(hdrbuf) + 1;

    const char  *bodybuf = "line 1\nline2\n";
    size_t       bodylen = strlen(bodybuf) + 1;

    ib_sdata_t  *sdata;
    ib_status_t  rc;

    rc = Push(IB_STREAM_EOH, hdrbuf, hdrlen);
    ASSERT_EQ(IB_OK, rc);

    rc = Push(IB_STREAM_EOB, bodybuf, bodylen);
    ASSERT_EQ(IB_OK, rc);

    rc = Pull(&sdata);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_EQ(IB_STREAM_EOH, sdata->type);
    ASSERT_EQ(hdrlen, sdata->dlen);
    ASSERT_STREQ(hdrbuf, (char *)sdata->data);

    rc = Pull(&sdata);
    ASSERT_EQ(IB_OK, rc);
    ASSERT_EQ(IB_STREAM_EOB, sdata->type);
    ASSERT_EQ(bodylen, sdata->dlen);
    ASSERT_STREQ(bodybuf, (char *)sdata->data);
}
