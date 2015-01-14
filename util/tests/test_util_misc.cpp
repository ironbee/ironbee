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
/// @brief IronBee --- Misc Utility Function Tests
///
/// @author Nick LeRoy <nleroy@qualys.com>
//////////////////////////////////////////////////////////////////////////////

#include "ironbee_config_auto.h"

#include <ironbee/util.h>

#include "gtest/gtest.h"

#include "simple_fixture.hpp"

#include <sys/time.h>
#include <stdlib.h>
#include <errno.h>
#include <stdexcept>

/* uint8_t * NULL for comparisons */
#define UNULL ((uint8_t *)NULL)

/* Class to create & compare to a random buffer */
class RandomBuffer : public SimpleFixture
{
public:
    RandomBuffer() : m_bufsize(0), m_buf(NULL)
    {
        struct timeval tv;
        if (gettimeofday(&tv, NULL) != 0) {
            throw std::runtime_error("Failed gettimeofday");
        }
        srandom((unsigned int)tv.tv_usec);
    };

    ~RandomBuffer()
    {
        FreeBuf( );
    };

    virtual void SetUp()
    {
        SimpleFixture::SetUp();
    }
    virtual void TearDown()
    {
        SimpleFixture::TearDown();
        FreeBuf( );
    }

    void CreateBuf(size_t max_size, size_t *size, uint8_t **buf)
    {
        size_t   n;
        uint8_t *ptr;

        if (*size == 0) {
            *size = (random() % max_size) + 1;
        }
        *buf = (uint8_t *)ib_mm_alloc(MM(), *size);
        if (*buf == NULL) {
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

    void FreeBuf()
    {
        if (m_buf != NULL) {
            m_buf = NULL;
        }
    }

    uint8_t *BufPtr(size_t offset = 0) { return m_buf + offset; };
    size_t BufSize() { return m_bufsize; };

protected:
    size_t       m_bufsize;
    uint8_t     *m_buf;
};

/* -- ib_util_memdup() Tests -- */

class TestIBUtilMemDup : public RandomBuffer
{
};

TEST_F(TestIBUtilMemDup, strings)
{
    char *buf;
    const char *s = "abc123";

    buf = ib_util_memdup_to_string(s, strlen(s));
    ASSERT_STREQ(s, (char *)buf);
    free(buf);
}

/* -- ib_util_copy_on_write() Tests -- */
class TestIBUtilCopyOnWrite : public RandomBuffer
{
};

/// @test Test util copy on write functions - ib_util_copy_on_write()
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
    cur = ib_util_copy_on_write(MM(), BufPtr(), BufPtr(offset), BufSize(),
                                NULL, &out_buf, &out_end);
    ASSERT_NE(UNULL, cur);
    ASSERT_NE(UNULL, out_buf);
    ASSERT_EQ(out_buf + offset, cur);
    ASSERT_EQ(out_buf + BufSize(), out_end);

    // Next case: re-use buffer, start == buf
    out_buf_bak = out_buf;
    offset = 0;
    cur = ib_util_copy_on_write(MM(), BufPtr(), BufPtr(offset), BufSize(),
                                out_buf + offset, &out_buf, &out_end);
    ASSERT_NE(UNULL, cur);
    ASSERT_NE(UNULL, out_buf);
    ASSERT_EQ(out_buf + offset, cur);
    ASSERT_EQ(out_buf_bak, out_buf);
    ASSERT_EQ(out_buf + BufSize(), out_end);

    // Next case: re-use buffer, start != buf
    out_buf_bak = out_buf;
    offset = BufSize() / 2;
    cur = ib_util_copy_on_write(MM(), BufPtr(), BufPtr(offset), BufSize(),
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
    cur = ib_util_copy_on_write(MM(), BufPtr(), BufPtr(offset), BufSize(),
                                NULL, &out_buf, &out_end);
    ASSERT_NE(UNULL, cur);
    ASSERT_NE(UNULL, out_buf);
    ASSERT_EQ(out_buf + offset, cur);
    ASSERT_EQ(out_buf + BufSize(), out_end);
    ASSERT_TRUE(Compare(out_buf, offset, &eoffset, &ecount))
        << "Error offset:" << eoffset << " count:" << ecount <<  std::endl;

    // Next case: re-use buffer, start != end
    out_buf_bak = out_buf;
    cur = ib_util_copy_on_write(MM(), BufPtr(), BufPtr(offset), BufSize(),
                                out_buf + offset, &out_buf, &out_end);
    ASSERT_NE(UNULL, cur);
    ASSERT_NE(UNULL, out_buf);
    ASSERT_EQ(out_buf + offset, cur);
    ASSERT_EQ(out_buf_bak, out_buf);
    ASSERT_EQ(out_buf + BufSize(), out_end);
    ASSERT_TRUE(Compare(out_buf, offset, &eoffset, &ecount))
        << "Error offset:" << eoffset << " count:" << ecount <<  std::endl;
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
    cur = ib_util_copy_on_write(MM(), BufPtr(), BufPtr(offset), BufSize(),
                                NULL, &out_buf, &out_end);
    ASSERT_NE(UNULL, cur);
    ASSERT_NE(UNULL, out_buf);
    ASSERT_EQ(out_buf + offset, cur);
    ASSERT_EQ(out_buf + BufSize(), out_end);
    ASSERT_TRUE(Compare(out_buf, offset, &eoffset, &ecount))
        << "Error offset:" << eoffset << " count:" << ecount <<  std::endl;

    // Next case: re-use buffer, start == end
    out_buf_bak = out_buf;
    cur = ib_util_copy_on_write(MM(), BufPtr(), BufPtr(offset), BufSize(),
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
        cur = ib_util_copy_on_write(MM(),
                                    BufPtr(), BufPtr(offset), BufSize(),
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
        cur = ib_util_copy_on_write(MM(),
                                    BufPtr(), BufPtr(offset), BufSize(),
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

class TestIBUtilFdup : public testing::Test
{
public:
    typedef enum { PRIMARY, DUPLICATE } fd_t;
    enum { NUM_FDS = 2 };

    TestIBUtilFdup()
    {
        memset(m_fds, 0, sizeof(m_fds));
    }
    virtual ~TestIBUtilFdup( )
    {
        CloseFds( );
    }

    virtual void SetUp()
    {
        testing::Test::SetUp();
        m_fds[0] = tmpfile();
        if (m_fds[0] == NULL) {
            throw std::runtime_error("tmpfile() failed.");
        }
    }

    virtual void TearDown()
    {
        testing::Test::TearDown();
        CloseFds( );
    }

    // Implement basic operations
    bool Close(fd_t which) {
        return CloseFd(&m_fds[which]);
    }
    bool Seek(fd_t which, long offset, int whence)
    {
        return fseek(m_fds[which], offset, whence) == 0;
    }
    bool Puts(fd_t which, const char *buf)
    {
        return fputs(buf, m_fds[which]) != EOF;
    }
    bool Gets(fd_t which)
    {
        const size_t  bufsize = 1024;
        char          buf[bufsize+1];
        return fgets(buf, bufsize, m_fds[which]) != NULL;
    }

    FILE *GetFd(fd_t which) { return m_fds[which]; };
    void SetFd(fd_t which, FILE *fd) { m_fds[which] = fd; };

protected:
    // Close all open FDs
    bool CloseFd(FILE **pfd)
    {
        bool ret = true;
        if (*pfd != NULL) {
            if (fclose(*pfd) != 0) {
                ret = false;
            }
            *pfd = NULL;
        }
        return ret;
    }

    void CloseFds()
    {
        for(int n = 0;  n < NUM_FDS;  ++n) {
            Close((fd_t)n);
        }
    }

protected:
    FILE   *m_fds[NUM_FDS];
};

/* -- ib_util_copy_on_write() Tests -- */
TEST_F(TestIBUtilFdup, fdup)
{
    int           ret;
    FILE         *fd;

    ret = fputs("hello\n", GetFd(PRIMARY));
    if (ret == EOF) {
        throw std::runtime_error("fputs() failed.");
    }

    // Duplicate the descriptor
    fd = ib_util_fdup(GetFd(PRIMARY), "a+");
    ASSERT_NE((FILE *)NULL, fd);
    ASSERT_NE(GetFd(PRIMARY), fd);
    SetFd(DUPLICATE, fd);

    // Write to the original descriptor
    ASSERT_TRUE(Seek(PRIMARY, 0L, SEEK_END));
    ASSERT_TRUE(Puts(PRIMARY, "hello\n"));

    // Verify that we can read from the duplicate descriptor
    ASSERT_TRUE(Seek(DUPLICATE, 0L, SEEK_SET));
    ASSERT_TRUE(Gets(DUPLICATE)) << strerror(errno) << std::endl;

    // Verify that we can write to the duplicate descriptor
    ASSERT_TRUE(Seek(DUPLICATE, 0L, SEEK_END));
    ASSERT_TRUE(Puts(DUPLICATE, "hello again\n"));

    // Close the duplicate, should no longer be able to use it
    ASSERT_TRUE(Close(DUPLICATE));

    // Original should still be valid, though
    ASSERT_TRUE(Seek(PRIMARY, 0L, SEEK_SET));
    ASSERT_TRUE(Gets(PRIMARY));
    ASSERT_TRUE(Seek(PRIMARY, 0L, SEEK_END));
    ASSERT_TRUE(Puts(PRIMARY, "hello\n"));

    // Duplicate the descriptor to a read-only
    fd = ib_util_fdup(GetFd(PRIMARY), "r");
    ASSERT_NE((FILE *)NULL, fd);
    ASSERT_NE(GetFd(PRIMARY), fd);
    SetFd(DUPLICATE, fd);

    // Verify that we can still write to the duplicate descriptor
    ASSERT_TRUE(Seek(PRIMARY, 0L, SEEK_END));
    ASSERT_TRUE(Puts(PRIMARY, "hello\n"));

    // We should still be able to read from the duplicate
    ASSERT_TRUE(Seek(DUPLICATE, 0L, SEEK_SET));
    ASSERT_TRUE(Gets(DUPLICATE));

    // Writing to the duplicate should not work now
    ASSERT_TRUE(Seek(DUPLICATE, 0L, SEEK_END));
    ASSERT_FALSE(Puts(DUPLICATE, "hello again\n"));

    // Done; Close both fds
    ASSERT_TRUE(Close(DUPLICATE));
    ASSERT_TRUE(Close(PRIMARY));
}
