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

#include "gtest/gtest.h"

#include "ibtest_textbuf.hpp"
#include "ibtest_strbase.hpp"
#include "simple_fixture.hpp"

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdexcept>


/* -- mkpath() Tests -- */
const size_t  pathsize_base = 64;
const size_t  pathsize_full = (pathsize_base + 16);
class TestIBUtilMkPath : public SimpleFixture
{
public:
    virtual void SetUp(void)
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

/**
 * Input parameter for TestNormalizePath tests.
 */
struct TestNormalizePath_t {
    const char *input;
    const char *expected;
    TestNormalizePath_t(const char *_input, const char *_expected):
        input(_input),
        expected(_expected)
    {
    }
};
class TestNormalizePath : public TestSimpleStringManipulation,
                          public ::testing::WithParamInterface<TestNormalizePath_t>
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

TEST_P(TestNormalizePath, RunTestInplaceNul)
{
    TestNormalizePath_t p = GetParam();
    TextBuf input(p.input);
    TextBuf expected(p.expected);

    RunTestInplaceNul(input, expected);
}

TEST_P(TestNormalizePath, RunTestInplaceEx)
{
    TestNormalizePath_t p = GetParam();
    TextBuf input(p.input);
    TextBuf expected(p.expected);

    RunTestInplaceEx(input, expected);
}

TEST_P(TestNormalizePath, RunTestCowNul)
{
    TestNormalizePath_t p = GetParam();
    TextBuf input(p.input);
    TextBuf expected(p.expected);

    RunTestCowNul(input, expected);
}

TEST_P(TestNormalizePath, RunTestCowEx)
{
    TestNormalizePath_t p = GetParam();
    TextBuf input(p.input);
    TextBuf expected(p.expected);

    RunTestCowEx(input, expected);
}

TEST_P(TestNormalizePath, RunTestCopyNul)
{
    TestNormalizePath_t p = GetParam();
    TextBuf input(p.input);
    TextBuf expected(p.expected);

    RunTestCopyNul(input, expected);
}

TEST_P(TestNormalizePath, RunTestCopyEx)
{
    TestNormalizePath_t p = GetParam();
    TextBuf input(p.input);
    TextBuf expected(p.expected);

    RunTestCopyEx(input, expected);
}

TEST_P(TestNormalizePath, RunTestBuf)
{
    TestNormalizePath_t p = GetParam();
    TextBuf input(p.input);
    TextBuf expected(p.expected);

    RunTestBuf(p.input, p.expected, strlen(p.expected)+1, IB_OK);
}

INSTANTIATE_TEST_CASE_P(Basic, TestNormalizePath, ::testing::Values(
        TestNormalizePath_t("", ""),
        TestNormalizePath_t("/", "/"),
        TestNormalizePath_t(".", ""),
        TestNormalizePath_t("..", ".."),
        TestNormalizePath_t("../", "../"),
        TestNormalizePath_t("x", "x"),
        TestNormalizePath_t("./..", ".."),
        TestNormalizePath_t("./../", "../"),
        TestNormalizePath_t("..", ".."),
        TestNormalizePath_t("../.", ".."),
        TestNormalizePath_t(".././", "../"),
        TestNormalizePath_t("../..", "../.."),
        TestNormalizePath_t("../../", "../../"),
        TestNormalizePath_t("/foo", "/foo"),
        TestNormalizePath_t("/foo/.", "/foo"),
        TestNormalizePath_t("/foo/..", "/"),
        TestNormalizePath_t("/foo/../", "/"),
        TestNormalizePath_t("/foo/../bar", "/bar"),
        TestNormalizePath_t("/foo/bar", "/foo/bar"),
        TestNormalizePath_t("/foo/bar/..", "/foo"),
        TestNormalizePath_t("/foo/bar/../", "/foo/"),
        TestNormalizePath_t("/foo/bar/baz", "/foo/bar/baz")
    ));

TEST_F(TestNormalizePath, NulByte)
{
    const uint8_t in[] = "/foo/bar\0/baz";
    const uint8_t out[] = "/foo/bar\0/baz";
    RunTest(in, sizeof(in)-1, out, sizeof(out)-1);
}

INSTANTIATE_TEST_CASE_P(Complex, TestNormalizePath, ::testing::Values(
        TestNormalizePath_t("/dir/foo//bar", "/dir/foo/bar"),
        TestNormalizePath_t("dir/foo//bar/", "dir/foo/bar/"),
        TestNormalizePath_t("dir/../foo", "foo"),
        TestNormalizePath_t("dir/../../foo", "../foo"),
        TestNormalizePath_t("dir/./.././../../foo/bar", "../../foo/bar"),
        TestNormalizePath_t("dir/./.././../../foo/bar/.", "../../foo/bar"),
        TestNormalizePath_t("dir/./.././../../foo/bar/./", "../../foo/bar/"),
        TestNormalizePath_t("dir/./.././../../foo/bar/..", "../../foo"),
        TestNormalizePath_t("dir/./.././../../foo/bar/../", "../../foo/"),
        TestNormalizePath_t("dir/./.././../../foo/bar/", "../../foo/bar/"),
        TestNormalizePath_t("dir//.//..//.//..//..//foo//bar", "../../foo/bar"),
        TestNormalizePath_t("dir//.//..//.//..//..//foo//bar//", "../../foo/bar/"),
        TestNormalizePath_t("dir/subdir/subsubdir/subsubsubdir/../../..", "dir"),
        TestNormalizePath_t("dir/./subdir/./subsubdir/./subsubsubdir/../../..", "dir"),
        TestNormalizePath_t("dir/./subdir/../subsubdir/../subsubsubdir/..", "dir"),
        TestNormalizePath_t("/dir/./subdir/../subsubdir/../subsubsubdir/../", "/dir/"),
        TestNormalizePath_t("/./.././../../../../../../..//../etc/./passwd", "/etc/passwd")
    ));

TEST_F(TestNormalizePath, Complex) {
    uint8_t in[] = "/./.././../../../../../../../\0/../etc/./passwd";
    uint8_t out[] = "/etc/passwd";
    RunTest(in, sizeof(in)-1, out, sizeof(out)-1);
}

class TestNormalizePathWin : public TestSimpleStringManipulation,
                             public ::testing::WithParamInterface<TestNormalizePath_t>
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

TEST_P(TestNormalizePathWin, RunTestInplaceNul)
{
    TestNormalizePath_t p = GetParam();
    TextBuf input(p.input);
    TextBuf expected(p.expected);

    RunTestInplaceNul(input, expected);
}

TEST_P(TestNormalizePathWin, RunTestInplaceEx)
{
    TestNormalizePath_t p = GetParam();
    TextBuf input(p.input);
    TextBuf expected(p.expected);

    RunTestInplaceEx(input, expected);
}

TEST_P(TestNormalizePathWin, RunTestCowNul)
{
    TestNormalizePath_t p = GetParam();
    TextBuf input(p.input);
    TextBuf expected(p.expected);

    RunTestCowNul(input, expected);
}

TEST_P(TestNormalizePathWin, RunTestCowEx)
{
    TestNormalizePath_t p = GetParam();
    TextBuf input(p.input);
    TextBuf expected(p.expected);

    RunTestCowEx(input, expected);
}

TEST_P(TestNormalizePathWin, RunTestCopyNul)
{
    TestNormalizePath_t p = GetParam();
    TextBuf input(p.input);
    TextBuf expected(p.expected);

    RunTestCopyNul(input, expected);
}

TEST_P(TestNormalizePathWin, RunTestCopyEx)
{
    TestNormalizePath_t p = GetParam();
    TextBuf input(p.input);
    TextBuf expected(p.expected);

    RunTestCopyEx(input, expected);
}

TEST_P(TestNormalizePathWin, RunTestBuf)
{
    TestNormalizePath_t p = GetParam();
    TextBuf input(p.input);
    TextBuf expected(p.expected);

    RunTestBuf(p.input, p.expected, strlen(p.expected)+1, IB_OK);
}

INSTANTIATE_TEST_CASE_P(Basic, TestNormalizePathWin, ::testing::Values(
        TestNormalizePath_t("", ""),
        TestNormalizePath_t("x", "x"),
        TestNormalizePath_t(".", ""),
        TestNormalizePath_t(".\\", ""),
        TestNormalizePath_t(".\\..", ".."),
        TestNormalizePath_t(".\\..\\", "../"),
        TestNormalizePath_t("..", ".."),
        TestNormalizePath_t("..\\", "../"),
        TestNormalizePath_t("..\\.", ".."),
        TestNormalizePath_t("..\\.\\", "../"),
        TestNormalizePath_t("..\\..", "../.."),
        TestNormalizePath_t("..\\..\\", "../../")
    ));
INSTANTIATE_TEST_CASE_P(Slashes, TestNormalizePathWin, ::testing::Values(
        TestNormalizePath_t("\\foo\\bar\\baz", "/foo/bar/baz")
    ));
INSTANTIATE_TEST_CASE_P(Complex, TestNormalizePathWin, ::testing::Values(
        TestNormalizePath_t("\\dir\\foo\\\\bar", "/dir/foo/bar"),
        TestNormalizePath_t("dir\\foo\\\\bar\\", "dir/foo/bar/"),
        TestNormalizePath_t("dir\\..\\foo", "foo"),
        TestNormalizePath_t("dir\\..\\..\\foo", "../foo"),
        TestNormalizePath_t("dir\\.\\..\\.\\..\\..\\foo\\bar", "../../foo/bar"),
        TestNormalizePath_t("dir\\.\\..\\.\\..\\..\\foo\\bar\\.", "../../foo/bar"),
        TestNormalizePath_t("dir\\.\\..\\.\\..\\..\\foo\\bar\\.\\", "../../foo/bar/"),
        TestNormalizePath_t("dir\\.\\..\\.\\..\\..\\foo\\bar\\..", "../../foo"),
        TestNormalizePath_t("dir\\.\\..\\.\\..\\..\\foo\\bar\\..\\", "../../foo/"),
        TestNormalizePath_t("dir\\.\\..\\.\\..\\..\\foo\\bar\\", "../../foo/bar/"),
        TestNormalizePath_t("dir\\\\.\\\\..\\\\.\\\\..\\\\..\\\\foo\\\\bar", "../../foo/bar"),
        TestNormalizePath_t("dir\\\\.\\\\..\\\\.\\\\..\\\\..\\\\foo\\\\bar\\\\",
                "../../foo/bar/"),
        TestNormalizePath_t("dir\\subdir\\subsubdir\\subsubsubdir\\..\\..\\..", "dir"),
        TestNormalizePath_t("dir\\.\\subdir\\.\\subsubdir\\.\\subsubsubdir\\..\\..\\..",
                "dir"),
        TestNormalizePath_t("dir\\.\\subdir\\..\\subsubdir\\..\\subsubsubdir\\..", "dir"),
        TestNormalizePath_t("\\dir\\.\\subdir\\..\\subsubdir\\..\\subsubsubdir\\..\\",
                "/dir/"),
        TestNormalizePath_t("\\.\\..\\.\\..\\..\\..\\..\\..\\..\\..\\\\..\\etc\\.\\passwd",
                "/etc/passwd")
    ));

TEST_F(TestNormalizePathWin, Slashes)
{
    {
        const uint8_t in[]  = "\\foo\\bar\0\\baz";
        const uint8_t out[] =  "/foo/bar\0/baz";
        SCOPED_TRACE("\\foo\\bar\\0\\baz");
        RunTest(in, sizeof(in)-1, out, sizeof(out)-1);
    }
}

TEST_F(TestNormalizePathWin, Nul)
{
    const uint8_t in[]  =
        "\\.\\..\\.\\..\\..\\..\\..\\..\\..\\..\\\0\\..\\etc\\.\\passwd";
    const uint8_t out[] =
        "/etc/passwd";
    RunTest(in, sizeof(in)-1, out, sizeof(out)-1);
}
