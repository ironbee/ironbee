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
/// @brief IronBee - Misc Utilility Function Tests
///
/// @author Nick LeRoy <nleroy@qualys.com>
//////////////////////////////////////////////////////////////////////////////

#include "ironbee_config_auto.h"

#include <ironbee/util.h>

#include "gtest/gtest.h"
#include "gtest/gtest-spi.h"

#include "simple_fixture.hh"

#include <sys/time.h>
#include <stdlib.h>
#include <errno.h>
#include <stdexcept>


/* -- copy_on_write() Tests -- */
class TestIBUtilCopyOnWrite : public SimpleFixture
{
public:
    TestIBUtilCopyOnWrite() : m_bufsize(0), m_buf(NULL)
    {
        struct timeval tv;
        if (gettimeofday(&tv, NULL) != 0) {
            throw std::runtime_error("Failed gettimeofday");
        }
        srandom((unsigned int)tv.tv_usec);
    };

    ~TestIBUtilCopyOnWrite()
    {
        FreeBuf( );
    };

    virtual void SetUp(void)
    {
        SimpleFixture::SetUp();
    }
    virtual void TearDown()
    {
        FreeBuf( );
    }

    void CreateBuf(size_t max_size, size_t *size, uint8_t **buf)
    {
        size_t   n;
        uint8_t *ptr;

        if (*size == 0) {
            *size = (random() % max_size);
        }
        *buf = (uint8_t *)MemPoolAlloc(*size);
        if (buf == NULL) {
            throw std::runtime_error("Failed to allocate buffer.");
        }

        for (n = 0, ptr = *buf;  n < *size;  ++n, ++ptr) {
            *ptr = random() % 0xff;
        }
    }

    void CreateBuf(size_t max_size = 2048, size_t bufsize = 0)
    {
        FreeBuf( );
        m_bufsize = bufsize;
        CreateBuf(max_size, &m_bufsize, &m_buf);
    }

    bool Compare(const uint8_t *buf,
                 size_t bytes = (size_t)-1,
                 size_t *eoffset = NULL,
                 size_t *ecount = NULL)
    {
        size_t         n;
        const uint8_t *p1;
        const uint8_t *p2;
        size_t         errors = 0;

        if (eoffset != NULL) {
            *eoffset = 0;
        }

        if (bytes == (size_t)-1) {
            bytes = m_bufsize;
        }
        for (n = 0, p1 = m_buf, p2 = buf;  n < bytes;  ++n, ++p1, ++p2) {
            if (*p1 != *p2) {
                if ( (eoffset != NULL) && (*eoffset == 0) ) {
                    *eoffset = n;
                }
                ++errors;
            }
        }
        if (ecount != NULL) {
            *ecount = errors;
        }
        return (errors == 0);
    }
    
    void FreeBuf( void )
    {
        if (m_buf != NULL) {
            // free(m_buf); /* Using mpool */
            m_buf = NULL;
        }
    }

    uint8_t *Buf(void) { return m_buf; };
    size_t BufSize(void) { return m_bufsize; };

protected:
    size_t       m_bufsize;
    uint8_t     *m_buf;
};

/// @test Test util copy on write functions - ib_util_copy_on_write()
#define UNULL ((uint8_t *)NULL)
TEST_F(TestIBUtilCopyOnWrite, basic)
{
    size_t         offset;
    uint8_t       *out_buf  = NULL;
    const uint8_t *out_end  = NULL;
    uint8_t       *out_buf_bak;
    uint8_t       *cur;

    // Setup
    CreateBuf(128, 128);

    // Simple case: new buffer, start == buf
    offset = 0;
    cur = ib_util_copy_on_write(MemPool(), m_buf, m_buf + offset, BufSize(),
                                NULL, &out_buf, &out_end);
    ASSERT_NE(UNULL, cur);
    ASSERT_NE(UNULL, out_buf);
    ASSERT_EQ(out_buf + offset, cur);
    ASSERT_EQ(out_buf + BufSize(), out_end);

    // Next case: re-use buffer, start == buf
    out_buf_bak = out_buf;
    offset = 0;
    cur = ib_util_copy_on_write(MemPool(), m_buf, m_buf + offset, BufSize(),
                                out_buf + offset, &out_buf, &out_end);
    ASSERT_NE(UNULL, cur);
    ASSERT_NE(UNULL, out_buf);
    ASSERT_EQ(out_buf + offset, cur);
    ASSERT_EQ(out_buf_bak, out_buf);
    ASSERT_EQ(out_buf + BufSize(), out_end);

    // Next case: re-use buffer, start != buf
    out_buf_bak = out_buf;
    offset = BufSize() / 2;
    cur = ib_util_copy_on_write(MemPool(), m_buf, m_buf + offset, BufSize(),
                                out_buf + offset, &out_buf, &out_end);
    ASSERT_NE(UNULL, cur);
    ASSERT_NE(UNULL, out_buf);
    ASSERT_EQ(out_buf + offset, cur);
    ASSERT_EQ(out_buf_bak, out_buf);
    ASSERT_EQ(out_buf + BufSize(), out_end);
}

TEST_F(TestIBUtilCopyOnWrite, copy_half)
{
    size_t         offset;
    uint8_t       *out_buf  = NULL;
    const uint8_t *out_end  = NULL;
    uint8_t       *out_buf_bak;
    uint8_t       *cur;
    size_t         eoffset;
    size_t         ecount;

    // Setup
    CreateBuf(128, 128);
    offset = BufSize() / 2;

    // Simple case: new buffer, start != end
    cur = ib_util_copy_on_write(MemPool(), m_buf, m_buf + offset, BufSize(),
                                NULL, &out_buf, &out_end);
    ASSERT_NE(UNULL, cur);
    ASSERT_NE(UNULL, out_buf);
    ASSERT_EQ(out_buf + offset, cur);
    ASSERT_EQ(out_buf + BufSize(), out_end);
    ASSERT_TRUE(Compare(out_buf, offset, &eoffset, &ecount))
        << "Error offset:" << eoffset << " count:" << ecount <<  std::endl;

    // Next case: re-use buffer, start != end
    out_buf_bak = out_buf;
    cur = ib_util_copy_on_write(MemPool(), m_buf, m_buf + offset, BufSize(),
                                out_buf + offset, &out_buf, &out_end);
    ASSERT_NE(UNULL, cur);
    ASSERT_NE(UNULL, out_buf);
    ASSERT_EQ(out_buf + offset, cur);
    ASSERT_EQ(out_buf_bak, out_buf);
    ASSERT_EQ(out_buf + BufSize(), out_end);
    ASSERT_TRUE(Compare(out_buf, offset, &eoffset))
        << "Error offset:" << eoffset << " count:" << ecount <<  std::endl;
}

TEST_F(TestIBUtilCopyOnWrite, random)
{
    int            loop;

    for (loop = 0;  loop < 100;  ++loop) {
        size_t         offset;
        uint8_t       *out_buf  = NULL;
        const uint8_t *out_end  = NULL;
        uint8_t       *out_buf_bak;
        uint8_t       *cur;
        size_t         eoffset;
        size_t         ecount;

        // Setup
        CreateBuf(128 * 1024);
        offset = (random() % BufSize());

        // Simple case: new buffer, random offset
        cur = ib_util_copy_on_write(MemPool(), m_buf, m_buf + offset, BufSize(),
                                    NULL, &out_buf, &out_end);
        ASSERT_NE(UNULL, cur);
        ASSERT_NE(UNULL, out_buf);
        ASSERT_EQ(out_buf + offset, cur);
        ASSERT_EQ(out_buf + BufSize(), out_end);
        ASSERT_TRUE(Compare(out_buf, offset, &eoffset, &ecount))
            << "Error @ Loop #" << loop
            << "  buffer size:" << BufSize()
            << "  copy size:" << offset
            << "  @ offset:" << eoffset << " count:" << ecount <<  std::endl;

        // Next case: re-use buffer, random offset
        out_buf_bak = out_buf;
        cur = ib_util_copy_on_write(MemPool(), m_buf, m_buf + offset, BufSize(),
                                    out_buf + offset, &out_buf, &out_end);
        ASSERT_NE(UNULL, cur);
        ASSERT_NE(UNULL, out_buf);
        ASSERT_EQ(out_buf + offset, cur);
        ASSERT_EQ(out_buf_bak, out_buf);
        ASSERT_EQ(out_buf + BufSize(), out_end);
        ASSERT_TRUE(Compare(out_buf, offset, &eoffset, &ecount))
            << "Error @ Loop #" << loop
            << "  buffer size:" << BufSize()
            << "  copy size:" << offset
            << "  @ offset:" << eoffset << " count:" << ecount <<  std::endl;
    }
}

TEST_F(TestIBUtilCopyOnWrite, copy_whole)
{
    size_t         offset;
    uint8_t       *out_buf  = NULL;
    const uint8_t *out_end  = NULL;
    uint8_t       *out_buf_bak;
    uint8_t       *cur;
    size_t         eoffset;
    size_t         ecount;

    // Setup
    CreateBuf(128, 128);
    offset = BufSize();

    // Simple case: new buffer, start == end
    cur = ib_util_copy_on_write(MemPool(), m_buf, m_buf + offset, BufSize(),
                                NULL, &out_buf, &out_end);
    ASSERT_NE(UNULL, cur);
    ASSERT_NE(UNULL, out_buf);
    ASSERT_EQ(out_buf + offset, cur);
    ASSERT_EQ(out_buf + BufSize(), out_end);
    ASSERT_TRUE(Compare(out_buf, offset, &eoffset, &ecount))
        << "Error offset:" << eoffset << " count:" << ecount <<  std::endl;

    // Next case: re-use buffer, start == end
    out_buf_bak = out_buf;
    cur = ib_util_copy_on_write(MemPool(), m_buf, m_buf + offset, BufSize(),
                                out_buf + offset, &out_buf, &out_end);
    ASSERT_NE(UNULL, cur);
    ASSERT_NE(UNULL, out_buf);
    ASSERT_EQ(out_buf + offset, cur);
    ASSERT_EQ(out_buf_bak, out_buf);
    ASSERT_EQ(out_buf + BufSize(), out_end);
    ASSERT_TRUE(Compare(out_buf, offset, &eoffset))
        << "Error offset:" << eoffset << " count:" << ecount <<  std::endl;
}
