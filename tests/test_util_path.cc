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
/// @brief IronBee - Path Test Functions
///
/// @author Nick LeRoy <nleroy@qualys.com>
//////////////////////////////////////////////////////////////////////////////

#include "ironbee_config_auto.h"

#include <ironbee/util.h>

#include "gtest/gtest.h"
#include "gtest/gtest-spi.h"

#include "ibtest_textbuf.hh"
#include "ibtest_strbase.hh"
#include "simple_fixture.hh"

#include <string.h>
#include <stdexcept>

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
        out = ib_util_path_join(MemPool(), test->in1, test->in2);
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
        out = ib_util_relative_file(MemPool(), test->in1, test->in2);
        EXPECT_STREQ(test->out, out)
            << "Test: in1 = '" << test->in1 << "'"
            << ", in2 = '" << test->in2 << "'";
    }
}

class TestNormalizePath : public TestSimpleStringManipulation
{
public:
    const char *TestName(ib_strop_t op, test_type_t tt)
    {
        return TestNameImpl("normalize_path", op, tt);
    }

    ib_status_t ExecInplaceNul(char *buf, ib_flags_t &result)
    {
        return ib_util_normalize_path(buf, false, &result);
    }

    ib_status_t ExecInplaceEx(uint8_t *data_in,
                              size_t dlen_in,
                              size_t &dlen_out,
                              ib_flags_t &result)
    {
        return ib_util_normalize_path_ex(data_in, dlen_in, false,
                                         &dlen_out, &result);
    }

    ib_status_t ExecCowNul(const char *data_in,
                           char **data_out,
                           ib_flags_t &result)
    {
        return ib_util_normalize_path_cow(m_mpool, data_in, false,
                                          data_out, &result);
    }
    ib_status_t ExecCowEx(const uint8_t *data_in,
                          size_t dlen_in,
                          uint8_t **data_out,
                          size_t &dlen_out,
                          ib_flags_t &result)
    {
        return ib_util_normalize_path_cow_ex(m_mpool,
                                             data_in, dlen_in, false,
                                             data_out, &dlen_out,
                                             &result);
    }
};

TEST_F(TestNormalizePath, Basic)
{
    {
        SCOPED_TRACE("Empty");
        RunTest("", "");
    }
    RunTest("/");
    RunTest(".", "");
    RunTest("..");
    RunTest("../", "../");
    RunTest("x", "x");
    RunTest("./..", "..");
    RunTest("./../", "../");
    RunTest("..", "..");
    RunTest("../.", "..");
    RunTest(".././", "../");
    RunTest("../..", "../..");
    RunTest("../../", "../../");
    RunTest("/foo", "/foo");
    RunTest("/foo/.", "/foo");
    RunTest("/foo/..", "/");
    RunTest("/foo/../", "/");
    RunTest("/foo/../bar", "/bar");
    RunTest("/foo/bar", "/foo/bar");
    RunTest("/foo/bar/..", "/foo");
    RunTest("/foo/bar/../", "/foo/");
    RunTest("/foo/bar/baz", "/foo/bar/baz");
}

TEST_F(TestNormalizePath, NulByte)
{
    const uint8_t in[] = "/foo/bar\0/baz";
    const uint8_t out[] = "/foo/bar\0/baz";
    RunTest(in, sizeof(in)-1, out, sizeof(out)-1);
}

TEST_F(TestNormalizePath, Complex)
{
    RunTest("/dir/foo//bar", "/dir/foo/bar");
    RunTest("dir/foo//bar/", "dir/foo/bar/");
    RunTest("dir/../foo", "foo");
    RunTest("dir/../../foo", "../foo");
    RunTest("dir/./.././../../foo/bar", "../../foo/bar");
    RunTest("dir/./.././../../foo/bar/.", "../../foo/bar");
    RunTest("dir/./.././../../foo/bar/./", "../../foo/bar/");
    RunTest("dir/./.././../../foo/bar/..", "../../foo");
    RunTest("dir/./.././../../foo/bar/../", "../../foo/");
    RunTest("dir/./.././../../foo/bar/", "../../foo/bar/");
    RunTest("dir//.//..//.//..//..//foo//bar", "../../foo/bar");
    RunTest("dir//.//..//.//..//..//foo//bar//", "../../foo/bar/");
    RunTest("dir/subdir/subsubdir/subsubsubdir/../../..", "dir");
    RunTest("dir/./subdir/./subsubdir/./subsubsubdir/../../..", "dir");
    RunTest("dir/./subdir/../subsubdir/../subsubsubdir/..", "dir");
    RunTest("/dir/./subdir/../subsubdir/../subsubsubdir/../", "/dir/");
    RunTest("/./.././../../../../../../..//../etc/./passwd", "/etc/passwd");

    uint8_t in[] = "/./.././../../../../../../../\0/../etc/./passwd";
    uint8_t out[] = "/etc/passwd";
    RunTest(in, sizeof(in)-1, out, sizeof(out)-1);

}

class TestNormalizePathWin : public TestSimpleStringManipulation
{
public:
    const char *TestName(ib_strop_t op, test_type_t tt)
    {
        return TestNameImpl("normalize_path(win)", op, tt);
    }

    ib_status_t ExecInplaceNul(char *buf, ib_flags_t &result)
    {
        return ib_util_normalize_path(buf, true, &result);
    }

    ib_status_t ExecInplaceEx(uint8_t *data_in,
                              size_t dlen_in,
                              size_t &dlen_out,
                              ib_flags_t &result)
    {
        return ib_util_normalize_path_ex(data_in, dlen_in, true,
                                         &dlen_out, &result);
    }

    ib_status_t ExecCowNul(const char *data_in,
                           char **data_out,
                           ib_flags_t &result)
    {
        return ib_util_normalize_path_cow(m_mpool, data_in, true,
                                          data_out, &result);
    }
    ib_status_t ExecCowEx(const uint8_t *data_in,
                          size_t dlen_in,
                          uint8_t **data_out,
                          size_t &dlen_out,
                          ib_flags_t &result)
    {
        return ib_util_normalize_path_cow_ex(m_mpool,
                                             data_in, dlen_in, true,
                                             data_out, &dlen_out,
                                             &result);
    }
};

TEST_F(TestNormalizePathWin, Empty)
{
    RunTest("", "");
}

TEST_F(TestNormalizePathWin, Slashes)
{
    RunTest("\\foo\\bar\\baz", "/foo/bar/baz");

    {
        const uint8_t in[]  = "\\foo\\bar\0\\baz";
        const uint8_t out[] =  "/foo/bar\0/baz";
        SCOPED_TRACE("\\foo\\bar\\0\\baz");
        RunTest(in, sizeof(in)-1, out, sizeof(out)-1);
    }
}

TEST_F(TestNormalizePathWin, Basics)
{
    RunTest("x", "x");
    RunTest(".", "");
    RunTest(".\\", "");
    RunTest(".\\..", "..");
    RunTest(".\\..\\", "../");
    RunTest("..", "..");
    RunTest("..\\", "../");
    RunTest("..\\.", "..");
    RunTest("..\\.\\", "../");
    RunTest("..\\..", "../..");
    RunTest("..\\..\\", "../../");
}

TEST_F(TestNormalizePathWin, Complex)
{
    RunTest("\\dir\\foo\\\\bar", "/dir/foo/bar");
    RunTest("dir\\foo\\\\bar\\", "dir/foo/bar/");
    RunTest("dir\\..\\foo", "foo");
    RunTest("dir\\..\\..\\foo", "../foo");
    RunTest("dir\\.\\..\\.\\..\\..\\foo\\bar", "../../foo/bar");
    RunTest("dir\\.\\..\\.\\..\\..\\foo\\bar\\.", "../../foo/bar");
    RunTest("dir\\.\\..\\.\\..\\..\\foo\\bar\\.\\", "../../foo/bar/");
    RunTest("dir\\.\\..\\.\\..\\..\\foo\\bar\\..", "../../foo");
    RunTest("dir\\.\\..\\.\\..\\..\\foo\\bar\\..\\", "../../foo/");
    RunTest("dir\\.\\..\\.\\..\\..\\foo\\bar\\", "../../foo/bar/");
    RunTest("dir\\\\.\\\\..\\\\.\\\\..\\\\..\\\\foo\\\\bar", "../../foo/bar");
    RunTest("dir\\\\.\\\\..\\\\.\\\\..\\\\..\\\\foo\\\\bar\\\\",
            "../../foo/bar/");
    RunTest("dir\\subdir\\subsubdir\\subsubsubdir\\..\\..\\..", "dir");
    RunTest("dir\\.\\subdir\\.\\subsubdir\\.\\subsubsubdir\\..\\..\\..",
            "dir");
    RunTest("dir\\.\\subdir\\..\\subsubdir\\..\\subsubsubdir\\..", "dir");
    RunTest("\\dir\\.\\subdir\\..\\subsubdir\\..\\subsubsubdir\\..\\",
            "/dir/");
    RunTest("\\.\\..\\.\\..\\..\\..\\..\\..\\..\\..\\\\..\\etc\\.\\passwd",
            "/etc/passwd");
}

TEST_F(TestNormalizePathWin, Nul)
{
    const uint8_t in[]  =
        "\\.\\..\\.\\..\\..\\..\\..\\..\\..\\..\\\0\\..\\etc\\.\\passwd";
    const uint8_t out[] =
        "/etc/passwd";
    RunTest(in, sizeof(in)-1, out, sizeof(out)-1);
}
