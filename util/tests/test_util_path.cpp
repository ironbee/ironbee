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
/// @brief IronBee &mdash Path Test Functions
///
/// @author Nick LeRoy <nleroy@qualys.com>
//////////////////////////////////////////////////////////////////////////////

#include "ironbee_config_auto.h"

#include <ironbee/path.h>

#include <ironbee/mm_mpool.h>

#include <ironbeepp/memory_manager.hpp>
#include <ironbeepp/memory_pool_lite.hpp>
#include <ironbeepp/throw.hpp>

#include "gtest/gtest.h"

#include "simple_fixture.hpp"

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdexcept>

using namespace std;
using namespace IronBee;

/* -- mkpath() Tests -- */
const size_t  pathsize_base = 64;
const size_t  pathsize_full = (pathsize_base + 16);
class TestIBUtilMkPath : public SimpleFixture
{
public:
    virtual void SetUp()
    {
        m_basedir[0] = '\0';
        SimpleFixture::SetUp();
    }
    virtual void TearDown()
    {
        SimpleFixture::TearDown();
        if (m_basedir[0] != '\0') {
            char buf[pathsize_full+1];
            snprintf(buf, pathsize_full, "/bin/rm -fr %s", m_basedir);
            if (system(buf) != 0) {
                std::string error = "Failed to cleanup ";
                error += m_basedir;
                throw std::runtime_error(error);
            }
            m_basedir[0] = '\0';
        }
    }

    char  m_basedir[pathsize_base+1];
};

/// @test Test util path functions - ib_util_mkpath()
TEST_F(TestIBUtilMkPath, mkpath)
{
    ib_status_t  rc;
    char        *tmp;
    char         path[pathsize_full+1];
    struct stat  sbuf;

    strcpy(m_basedir, "/tmp/XXXXXX");
    tmp = mkdtemp(m_basedir);
    ASSERT_STRNE(NULL, tmp)
        << "mkdtemp() returned " << strerror(errno) << std::endl;

    snprintf(path, pathsize_full, "%s/a", m_basedir);
    rc = ib_util_mkpath(path, 0700);
    ASSERT_EQ(IB_OK, rc);

    ASSERT_EQ(0, stat(path, &sbuf)) << path << ": " << strerror(errno);
    ASSERT_TRUE(S_ISDIR(sbuf.st_mode));
    ASSERT_EQ((mode_t)0700, (sbuf.st_mode & 0777));

    snprintf(path, pathsize_full, "%s/a/b", m_basedir);
    rc = ib_util_mkpath(path, 0750);
    ASSERT_EQ(IB_OK, rc);

    ASSERT_EQ(0, stat(path, &sbuf));
    ASSERT_TRUE(S_ISDIR(sbuf.st_mode));
    ASSERT_EQ((mode_t)0750, (sbuf.st_mode & 0777));

    snprintf(path, pathsize_full, "%s/b/c/d/e", m_basedir);
    rc = ib_util_mkpath(path, 0755);
    ASSERT_EQ(IB_OK, rc);

    ASSERT_EQ(0, stat(path, &sbuf));
    ASSERT_TRUE(S_ISDIR(sbuf.st_mode));
    ASSERT_EQ((mode_t)0755, (sbuf.st_mode & 0777));
}


/* -- Join / relative path tests -- */
struct test_path_data_t {
    int            lineno;
    const char    *in1;
    const char    *in2;
    const char    *out;
};

static struct test_path_data_t test_path_join[] = {
    { __LINE__, "/",     "a/b",    "/a/b" },
    { __LINE__, "/a",    "b/c",    "/a/b/c" },
    { __LINE__, "/a",    "/b/c/",  "/a/b/c" },
    { __LINE__, "/a/",   "b/c",    "/a/b/c" },
    { __LINE__, "/a///", "b/c",    "/a/b/c" },
    { __LINE__, "/a/",   "///b/c", "/a/b/c" },
    { __LINE__, NULL,    NULL,     NULL },
};

static struct test_path_data_t test_rel_file[] = {
    { __LINE__, "x.conf",        "y.conf",      "./y.conf" },
    { __LINE__, "x.conf",        "y.conf",      "./y.conf" },
    { __LINE__, "./x.conf",      "y.conf",      "./y.conf" },
    { __LINE__, "./x.conf",      "a/y.conf",    "./a/y.conf" },
    { __LINE__, "/x.conf",       "a/y.conf",    "/a/y.conf" },
    { __LINE__, "/a/b/c/x.conf", "d/y.conf",    "/a/b/c/d/y.conf" },
    { __LINE__, "/a/x.conf",     "/b/c/y.conf", "/b/c/y.conf" },
    { __LINE__, "/a/x.conf",     "b/c/y.conf",  "/a/b/c/y.conf" },
    { __LINE__, "/a///x.conf",   "b/c/y.conf",  "/a/b/c/y.conf" },
    { __LINE__, NULL,     NULL,          NULL },
};

/* -- Tests -- */
class TestIBUtilPath : public SimpleFixture
{
};

/// @test Test util path functions - ib_util_relative_path()
TEST_F(TestIBUtilPath, path_join)
{
    struct test_path_data_t *test;

    for (test = &test_path_join[0]; test->in1 != NULL; ++test) {
        const char *out;
        out = ib_util_path_join(MM(), test->in1, test->in2);
        EXPECT_STREQ(test->out, out)
            << "Test: in1 = '" << test->in1 << "'"
            << ", in2 = '" << test->in2 << "'";
    }
}

/// @test Test util path functions - ib_util_relative_path()
TEST_F(TestIBUtilPath, relative_path)
{
    struct test_path_data_t *test;

    for (test = &test_rel_file[0]; test->in1 != NULL; ++test) {
        const char *out;
        out = ib_util_relative_file(MM(), test->in1, test->in2);
        EXPECT_STREQ(test->out, out)
            << "Test: in1 = '" << test->in1 << "'"
            << ", in2 = '" << test->in2 << "'";
    }
}

namespace {

string normalize_path(const string& s, bool win = false)
{
    ScopedMemoryPoolLite mpl;
    char* result;
    size_t result_length;

    throw_if_error(ib_util_normalize_path(
        MemoryManager(mpl).ib(),
        reinterpret_cast<const uint8_t*>(s.data()), s.length(),
        win,
        reinterpret_cast<uint8_t**>(&result), &result_length
    ));

    return string(result, result_length);
}

}

TEST(TestNormalizePath, Basic)
{
    EXPECT_EQ("", normalize_path(""));
    EXPECT_EQ("/", normalize_path("/"));
    EXPECT_EQ("", normalize_path("."));
    EXPECT_EQ("..", normalize_path(".."));
    EXPECT_EQ("../", normalize_path("../"));
    EXPECT_EQ("x", normalize_path("x"));
    EXPECT_EQ("..", normalize_path("./.."));
    EXPECT_EQ("../", normalize_path("./../"));
    EXPECT_EQ("..", normalize_path(".."));
    EXPECT_EQ("..", normalize_path("../."));
    EXPECT_EQ("../", normalize_path(".././"));
    EXPECT_EQ("../..", normalize_path("../.."));
    EXPECT_EQ("../../", normalize_path("../../"));
    EXPECT_EQ("/foo", normalize_path("/foo"));
    EXPECT_EQ("/foo", normalize_path("/foo/."));
    EXPECT_EQ("/", normalize_path("/foo/.."));
    EXPECT_EQ("/", normalize_path("/foo/../"));
    EXPECT_EQ("/bar", normalize_path("/foo/../bar"));
    EXPECT_EQ("/foo/bar", normalize_path("/foo/bar"));
    EXPECT_EQ("/foo", normalize_path("/foo/bar/.."));
    EXPECT_EQ("/foo/", normalize_path("/foo/bar/../"));
    EXPECT_EQ("/foo/bar/baz", normalize_path("/foo/bar/baz"));
}

TEST(TestNormalizePath, Nul)
{
    EXPECT_EQ(string("/foo/bar\0/baz", 13), normalize_path(string("/foo/bar\0/baz", 13)));
}

TEST(TestNormalizePath, Complex)
{
    EXPECT_EQ("/dir/foo/bar", normalize_path("/dir/foo//bar"));
    EXPECT_EQ("dir/foo/bar/", normalize_path("dir/foo//bar/"));
    EXPECT_EQ("foo", normalize_path("dir/../foo"));
    EXPECT_EQ("../foo", normalize_path("dir/../../foo"));
    EXPECT_EQ("../../foo/bar", normalize_path("dir/./.././../../foo/bar"));
    EXPECT_EQ("../../foo/bar", normalize_path("dir/./.././../../foo/bar/."));
    EXPECT_EQ("../../foo/bar/", normalize_path("dir/./.././../../foo/bar/./"));
    EXPECT_EQ("../../foo", normalize_path("dir/./.././../../foo/bar/.."));
    EXPECT_EQ("../../foo/", normalize_path("dir/./.././../../foo/bar/../"));
    EXPECT_EQ("../../foo/bar/", normalize_path("dir/./.././../../foo/bar/"));
    EXPECT_EQ("../../foo/bar", normalize_path("dir//.//..//.//..//..//foo//bar"));
    EXPECT_EQ("../../foo/bar/", normalize_path("dir//.//..//.//..//..//foo//bar//"));
    EXPECT_EQ("dir", normalize_path("dir/subdir/subsubdir/subsubsubdir/../../.."));
    EXPECT_EQ("dir", normalize_path("dir/./subdir/./subsubdir/./subsubsubdir/../../.."));
    EXPECT_EQ("dir", normalize_path("dir/./subdir/../subsubdir/../subsubsubdir/.."));
    EXPECT_EQ("/dir/", normalize_path("/dir/./subdir/../subsubdir/../subsubsubdir/../"));
    EXPECT_EQ("/etc/passwd", normalize_path("/./.././../../../../../../..//../etc/./passwd"));
}

TEST(TestNormalizePath, ComplexNul)
{
    EXPECT_EQ("/etc/passwd", normalize_path(string("/./.././../../../../../../../\0/../etc/./passwd", 46)));
}

TEST(TestNormalizePathWin, Basic)
{
    EXPECT_EQ("", normalize_path("", true));
    EXPECT_EQ("x", normalize_path("x", true));
    EXPECT_EQ("", normalize_path(".", true));
    EXPECT_EQ("", normalize_path(".\\", true));
    EXPECT_EQ("..", normalize_path(".\\..", true));
    EXPECT_EQ("../", normalize_path(".\\..\\", true));
    EXPECT_EQ("..", normalize_path("..", true));
    EXPECT_EQ("../", normalize_path("..\\", true));
    EXPECT_EQ("..", normalize_path("..\\.", true));
    EXPECT_EQ("../", normalize_path("..\\.\\", true));
    EXPECT_EQ("../..", normalize_path("..\\..", true));
    EXPECT_EQ("../../", normalize_path("..\\..\\", true));
}

TEST(TestNormalizePathWin, Slashes)
{
    EXPECT_EQ("/foo/bar/baz", normalize_path("\\foo\\bar\\baz", true));
}

TEST(TestNormalizePathWin, Complex)
{
    EXPECT_EQ("/dir/foo/bar", normalize_path("\\dir\\foo\\\\bar", true));
    EXPECT_EQ("dir/foo/bar/", normalize_path("dir\\foo\\\\bar\\", true));
    EXPECT_EQ("foo", normalize_path("dir\\..\\foo", true));
    EXPECT_EQ("../foo", normalize_path("dir\\..\\..\\foo", true));
    EXPECT_EQ("../../foo/bar", normalize_path("dir\\.\\..\\.\\..\\..\\foo\\bar", true));
    EXPECT_EQ("../../foo/bar", normalize_path("dir\\.\\..\\.\\..\\..\\foo\\bar\\.", true));
    EXPECT_EQ("../../foo/bar/", normalize_path("dir\\.\\..\\.\\..\\..\\foo\\bar\\.\\", true));
    EXPECT_EQ("../../foo", normalize_path("dir\\.\\..\\.\\..\\..\\foo\\bar\\..", true));
    EXPECT_EQ("../../foo/", normalize_path("dir\\.\\..\\.\\..\\..\\foo\\bar\\..\\", true));
    EXPECT_EQ("../../foo/bar/", normalize_path("dir\\.\\..\\.\\..\\..\\foo\\bar\\", true));
    EXPECT_EQ("../../foo/bar", normalize_path("dir\\\\.\\\\..\\\\.\\\\..\\\\..\\\\foo\\\\bar", true));
    EXPECT_EQ("../../foo/bar/", normalize_path("dir\\\\.\\\\..\\\\.\\\\..\\\\..\\\\foo\\\\bar\\\\", true));
    EXPECT_EQ("dir", normalize_path("dir\\subdir\\subsubdir\\subsubsubdir\\..\\..\\..", true));
    EXPECT_EQ("dir", normalize_path("dir\\.\\subdir\\.\\subsubdir\\.\\subsubsubdir\\..\\..\\..", true));
    EXPECT_EQ("dir", normalize_path("dir\\.\\subdir\\..\\subsubdir\\..\\subsubsubdir\\..", true));
    EXPECT_EQ("/dir/", normalize_path("\\dir\\.\\subdir\\..\\subsubdir\\..\\subsubsubdir\\..\\", true));
    EXPECT_EQ("/etc/passwd", normalize_path("\\.\\..\\.\\..\\..\\..\\..\\..\\..\\..\\\\..\\etc\\.\\passwd", true));
}

TEST(TestNormalizePathWin, SlashesNull)
{
    EXPECT_EQ(string("/foo/bar\0/baz", 13), normalize_path(string("\\foo\\bar\0\\baz", 13), true));
}

TEST(TestNormalizePathWin, Nul)
{
    EXPECT_EQ("/etc/passwd", normalize_path(string("\\.\\..\\.\\..\\..\\..\\..\\..\\..\\..\\\0\\..\\etc\\.\\passwd", 46), true));
}
