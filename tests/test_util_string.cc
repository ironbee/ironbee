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
/// @brief IronBee - String Util Test Functions
/// 
/// @author Nick LeRoy <nleroy@qualys.com>
//////////////////////////////////////////////////////////////////////////////

#include "ironbee_config_auto.h"

#include <ironbee/types.h>
#include <ironbee/string.h>

#include "ironbee_util_private.h"

#include "gtest/gtest.h"
#include "gtest/gtest-spi.h"

#include <stdexcept>

class TestIBUtilStringToNum : public ::testing::Test
{
public:

    const char *BoolStr(ib_bool_t b)
    {
        return (b == IB_TRUE) ? "True" : "False";
    }

    void RunTest(int line,
                 const char *s,
                 int base,
                 ib_num_t expected)
    {
        RunTest(line, s, base, IB_OK, expected);
    }

    void RunTest(int line,
                 const char *s,
                 int base,
                 ib_status_t estatus)
    {
        ib_status_t rc;
        ib_num_t result;
        rc = ::string_to_num(s, base, &result);
        EXPECT_EQ(rc, estatus)
            << "Line " << line << ": Conversion of '" << s
            << "' base10=" << base
            << " failed; rc=" << rc;
    }

    void RunTest(int line,
                 const char *s,
                 int base,
                 ib_status_t estatus,
                 ib_num_t expected)
    {
        ib_status_t rc;
        ib_num_t result;

        rc = ::string_to_num(s, base, &result);
        if (estatus == IB_OK) {
            EXPECT_EQ(rc, IB_OK)
                << "Line " << line << ": "
                << "Conversion of '" << s << "' base10="<< base
                << " failed; rc=" << rc;
        }
        else {
            EXPECT_EQ(rc, estatus)
                << "Line " << line << ": "
                << "Conversion of '" << s << "' base=" << base
                << " expected status " << estatus << " returned "<< rc;
        }

        if (rc == IB_OK) {
            EXPECT_EQ(expected, result)
                << "Line " << line << ": "
                << "Conversion of '" << s << "' base=" <<base
                << " expected value=" << expected << " result="<< result;
        }
    }
};

/* -- Tests -- */

/// @test Test util string library - convert string to number : error detection
TEST_F(TestIBUtilStringToNum, test_string_to_num_errors)
{
    RunTest(__LINE__, " ",     0,   IB_EINVAL);
    RunTest(__LINE__, " ",     8,   IB_EINVAL);
    RunTest(__LINE__, " ",     10,  IB_EINVAL);
    RunTest(__LINE__, " ",     16,  IB_EINVAL);

    RunTest(__LINE__, "",      0,   IB_EINVAL);
    RunTest(__LINE__, "",      8,   IB_EINVAL);
    RunTest(__LINE__, "",      10,  IB_EINVAL);
    RunTest(__LINE__, "",      16,  IB_EINVAL);

    RunTest(__LINE__, ":",     0,   IB_EINVAL);
    RunTest(__LINE__, ":",     8,   IB_EINVAL);
    RunTest(__LINE__, ":",     10,  IB_EINVAL);
    RunTest(__LINE__, ":",     16,  IB_EINVAL);

    RunTest(__LINE__, "x",     0,   IB_EINVAL);
    RunTest(__LINE__, "x",     8,   IB_EINVAL);
    RunTest(__LINE__, "x",     10,  IB_EINVAL);
    RunTest(__LINE__, "x",     16,  IB_EINVAL);

    RunTest(__LINE__, "-" ,    0,   IB_EINVAL);
    RunTest(__LINE__, "-" ,    8,   IB_EINVAL);
    RunTest(__LINE__, "-",     10,  IB_EINVAL);
    RunTest(__LINE__, "-",     16,  IB_EINVAL);

    RunTest(__LINE__, "+" ,    0,   IB_EINVAL);
    RunTest(__LINE__, "+" ,    8,   IB_EINVAL);
    RunTest(__LINE__, "+",     10,  IB_EINVAL);
    RunTest(__LINE__, "+",     16,  IB_EINVAL);

    RunTest(__LINE__, "0x",    0,   IB_EINVAL);
    RunTest(__LINE__, "0x",    8,   IB_EINVAL);
    RunTest(__LINE__, "0x",    10,  IB_EINVAL);
    RunTest(__LINE__, "0x",    16,  IB_EINVAL);

    RunTest(__LINE__, "0" ,    0,   IB_OK);
    RunTest(__LINE__, "0" ,    8,   IB_OK);
    RunTest(__LINE__, "0",     10,  IB_OK);
    RunTest(__LINE__, "0",     16,  IB_OK);

    RunTest(__LINE__, "8" ,    0,   IB_OK);
    RunTest(__LINE__, "8" ,    8,   IB_EINVAL);
    RunTest(__LINE__, "8",     10,  IB_OK);
    RunTest(__LINE__, "8",     16,  IB_OK);

    RunTest(__LINE__, "0x0",   0,   IB_OK);
    RunTest(__LINE__, "0x0",   8,   IB_EINVAL);
    RunTest(__LINE__, "0x0",   10,  IB_EINVAL);
    RunTest(__LINE__, "0x0",   16,  IB_OK);

    RunTest(__LINE__, "08",    0,   IB_EINVAL);
    RunTest(__LINE__, "08",    8,   IB_EINVAL);
    RunTest(__LINE__, "08",    10,  IB_OK);
    RunTest(__LINE__, "08",    16,  IB_OK);

    RunTest(__LINE__, "-1",    0,   IB_OK);
    RunTest(__LINE__, "-1",    8,   IB_OK);
    RunTest(__LINE__, "-1",    10,  IB_OK);
    RunTest(__LINE__, "-1",    16,  IB_OK);

    RunTest(__LINE__, "+1",    0,   IB_OK);
    RunTest(__LINE__, "+1",    8,   IB_OK);
    RunTest(__LINE__, "+1",    10,  IB_OK);
    RunTest(__LINE__, "+1",    16,  IB_OK);

    RunTest(__LINE__, "01",    0,   IB_OK);
    RunTest(__LINE__, "01",    8,   IB_OK);
    RunTest(__LINE__, "01",    10,  IB_OK);
    RunTest(__LINE__, "01",    16,  IB_OK);

    RunTest(__LINE__, "0x100", 0,   IB_OK);
    RunTest(__LINE__, "0x100", 8,   IB_EINVAL);
    RunTest(__LINE__, "0x100", 10,  IB_EINVAL);
    RunTest(__LINE__, "0x100", 16,  IB_OK);

    RunTest(__LINE__, "-0x1",  0,   IB_OK);
    RunTest(__LINE__, "-0x1",  8,   IB_EINVAL);
    RunTest(__LINE__, "-0x1",  10,  IB_EINVAL);
    RunTest(__LINE__, "-0x1",  16,  IB_OK);

    RunTest(__LINE__, "+0x1",  0,   IB_OK);
    RunTest(__LINE__, "+0x1",  8,   IB_EINVAL);
    RunTest(__LINE__, "+0x1",  10,  IB_EINVAL);
    RunTest(__LINE__, "+0x1",  16,  IB_OK);
}

/// @test Test util string library - convert string to number : overflow
TEST_F(TestIBUtilStringToNum, test_string_to_num_overflow)
{
    // 16-bit
    RunTest(__LINE__, "0x7fff",               0,  IB_OK, 0x7fff);
    RunTest(__LINE__, "32767",                0,  IB_OK, 0x7fff);
    RunTest(__LINE__, "0x8000",               0,  IB_OK, 0x8000);
    RunTest(__LINE__, "32768",                0,  IB_OK, 0x8000);
    RunTest(__LINE__, "0xffff",               0,  IB_OK, 0xffff);
    RunTest(__LINE__, "65535",                0,  IB_OK, 0xffff);
    RunTest(__LINE__, "0x10000",              0,  IB_OK, 0x10000);
    RunTest(__LINE__, "65536",                0,  IB_OK, 0x10000);
    // 32-bit
    RunTest(__LINE__, "0x7fffffff",           0,  IB_OK, 0x7fffffff);
    RunTest(__LINE__, "2147483647",           0,  IB_OK, 0x7fffffff);
    RunTest(__LINE__, "0x80000000",           0,  IB_OK, 0x80000000);
    RunTest(__LINE__, "2147483648",           0,  IB_OK, 0x80000000);
    RunTest(__LINE__, "0xffffffff",           0,  IB_OK, 0xffffffff);
    RunTest(__LINE__, "4294967295",           0,  IB_OK, 0xffffffff);
    RunTest(__LINE__, "0x100000000",          0,  IB_OK, 0x100000000);
    RunTest(__LINE__, "4294967296",           0,  IB_OK, 0x100000000);
    // 64-bit
    RunTest(__LINE__, "0x7fffffffffffffff",   0,  IB_OK, 0x7fffffffffffffffL);
    RunTest(__LINE__, "9223372036854775807",  0,  IB_OK, 0x7fffffffffffffffL);
    RunTest(__LINE__, "0x8000000000000000",   0,  IB_EINVAL);
    RunTest(__LINE__, "9223372036854775808",  0,  IB_EINVAL);
    RunTest(__LINE__, "0xffffffffffffffff",   0,  IB_EINVAL);
    RunTest(__LINE__, "18446744073709551615", 0,  IB_EINVAL);
}

/// @test Test util string library - convert string to number : error detection
TEST_F(TestIBUtilStringToNum, test_string_to_num)
{
    RunTest(__LINE__, "0",       0,   0);
    RunTest(__LINE__, "0",       8,   0);
    RunTest(__LINE__, "0",       10,  0);
    RunTest(__LINE__, "0",       16,  0);

    RunTest(__LINE__, "1",       0,   1);
    RunTest(__LINE__, "1",       8,   1);
    RunTest(__LINE__, "1",       10,  1);
    RunTest(__LINE__, "1",       16,  1);

    RunTest(__LINE__, "10",      0,   10);
    RunTest(__LINE__, "10",      8,   010);
    RunTest(__LINE__, "10",      10,  10);
    RunTest(__LINE__, "10",      16,  0x10);

    RunTest(__LINE__, "100",     0,   100);
    RunTest(__LINE__, "100",     8,   0100);
    RunTest(__LINE__, "100",     10,  100);
    RunTest(__LINE__, "100",     16,  0x100);

    RunTest(__LINE__, "07",      0,   7);
    RunTest(__LINE__, "07",      8,   7);
    RunTest(__LINE__, "07",      10,  7);
    RunTest(__LINE__, "07",      16,  7);

    RunTest(__LINE__, "0377",    0,   255);
    RunTest(__LINE__, "0377",    8,   0377);
    RunTest(__LINE__, "0377",    10,  377);
    RunTest(__LINE__, "0377",    16,  0x377);

    RunTest(__LINE__, "0x100",   0,   0x100);
    RunTest(__LINE__, "0x100",   8,   IB_EINVAL);
    RunTest(__LINE__, "0x100",   10,  IB_EINVAL);
    RunTest(__LINE__, "0x100",   16,  0x100);

    RunTest(__LINE__, "0xf",     0,   0xf);
    RunTest(__LINE__, "0xf",     8,   IB_EINVAL);
    RunTest(__LINE__, "0xf",     10,  IB_EINVAL);
    RunTest(__LINE__, "0xf",     16,  0xf);

    RunTest(__LINE__, "0xff",    0,   0xff);
    RunTest(__LINE__, "0xff",    8,   IB_EINVAL);
    RunTest(__LINE__, "0xff",    10,  IB_EINVAL);
    RunTest(__LINE__, "0xff",    16,  0xff);

    RunTest(__LINE__, "0xffff",  0,   0xffff);
    RunTest(__LINE__, "0xffff",  8,   IB_EINVAL);
    RunTest(__LINE__, "0xffff",  10,  IB_EINVAL);
    RunTest(__LINE__, "0xffff",  16,  0xffff);

    RunTest(__LINE__, "0177777", 0,   0xffff);
    RunTest(__LINE__, "0177777", 8,   0xffff);
    RunTest(__LINE__, "0177777", 10,  177777);
    RunTest(__LINE__, "0177777", 16,  0x177777);

    RunTest(__LINE__, "+1",      0,   1);
    RunTest(__LINE__, "+1",      8,   1);
    RunTest(__LINE__, "+1",      10,  1);
    RunTest(__LINE__, "+1",      16,  1);

    RunTest(__LINE__, "-1",      0,   -1);
    RunTest(__LINE__, "-1",      8,   -1);
    RunTest(__LINE__, "-1",      10,  -1);
    RunTest(__LINE__, "-1",      16,  -1);

    RunTest(__LINE__, "+0",      0,   0);
    RunTest(__LINE__, "+0",      8,   0);
    RunTest(__LINE__, "+0",      10,  0);
    RunTest(__LINE__, "+0",      16,  0);

    RunTest(__LINE__, "-0",      0,   0);
    RunTest(__LINE__, "-0",      8,   0);
    RunTest(__LINE__, "-0",      10,  0);
    RunTest(__LINE__, "-0",      16,  0);

    RunTest(__LINE__, "9",       0,   9);
    RunTest(__LINE__, "9",       8,   IB_EINVAL);
    RunTest(__LINE__, "9",       10,  9);
    RunTest(__LINE__, "9",       16,  9);

    RunTest(__LINE__, "99999",   0,   99999);
    RunTest(__LINE__, "99999",   8,   IB_EINVAL);
    RunTest(__LINE__, "99999",   10,  99999);
    RunTest(__LINE__, "99999",   16,  0x99999);

    RunTest(__LINE__, "-99999",  0,   -99999);
    RunTest(__LINE__, "-99999",  8,   IB_EINVAL);
    RunTest(__LINE__, "-99999",  10,  -99999);
    RunTest(__LINE__, "-99999",  16,  -0x99999);

    RunTest(__LINE__, "+99999",  0,   99999);
    RunTest(__LINE__, "+99999",  8,   IB_EINVAL);
    RunTest(__LINE__, "+99999",  10,  99999);
    RunTest(__LINE__, "+99999",  16,  0x99999);
}

class TestIBUtilStrStrEx : public ::testing::Test
{
public:

    size_t StrLen(const char *s)
    {
        if (s == NULL) {
            return 0;
        }
        else {
            return strlen(s);
        }
    }

    const char *Stringize(const char *haystack,
                          size_t haystack_len,
                          const char *needle,
                          size_t needle_len,
                          char *buf, size_t buflen)
    {
        if ( (haystack == NULL) && (needle == NULL) ){
            snprintf(buf, buflen,
                     "strstr_ex(NULL,%zd,NULL,%zd)",
                     haystack_len, needle_len);
        }
        else if (haystack == NULL) {
            snprintf(buf, buflen,
                     "strstr_ex(NULL,%zd,\"%*s\",%zd)",
                     haystack_len, (int)needle_len, needle, needle_len);
        }
        else if (needle == NULL) {
            snprintf(buf, buflen,
                     "strstr_ex(\"%*s\",%zd,NULL,%zd)",
                     (int)haystack_len, haystack, haystack_len, needle_len);
        }
        else {
            snprintf(buf, buflen,
                     "strstr_ex(\"%*s\",%zd,\"%*s\",%zd)",
                     (int)haystack_len, haystack, haystack_len,
                     (int)needle_len, needle, needle_len);
        }

        return buf;
    }

    const char *Stringize(const char *s, char *buf, size_t buflen)
    {
        if (s == NULL) {
            return "NULL";
        }
        else {
            snprintf(buf, buflen, "\"%s\"", s);
            return buf;
        }
    }

    void RunTest(int line,
                 const char *haystack,
                 size_t haystack_len,
                 const char *needle,
                 size_t needle_len,
                 const char *expected)
    {
        const char *result = strstr_ex(haystack,
                                       haystack_len,
                                       needle,
                                       needle_len);
        const int blen = 256;
        char b1[blen];
        char b2[blen];
        char b3[blen];

        EXPECT_STREQ(expected, result)
            << "Line " << line << ": "
            << Stringize(haystack, haystack_len, needle, needle_len, b1, blen)
            << " expected " << Stringize(expected, b3, blen)
            << " returned " << Stringize(result, b2, blen);
    }

    void RunTest(int line,
                 const char *haystack,
                 const char *needle,
                 const char *expected)
    {
        RunTest(line,
                haystack, StrLen(haystack),
                needle,   StrLen(needle),
                expected);
    }

    void RunTest(int line,
                 const char *haystack,
                 size_t haystack_len,
                 const char *needle,
                 const char *expected)
    {
        RunTest(line,
                haystack, haystack_len,
                needle,   StrLen(needle),
                expected);
    }

    void RunTest(int line,
                 const char *haystack,
                 const char *needle,
                 size_t needle_len,
                 const char *expected)
    {
        RunTest(line,
                haystack, StrLen(haystack),
                needle,   needle_len,
                expected);
    }
};

/// @test Test util string library - strstr_ex()
TEST_F(TestIBUtilStrStrEx, test_strstr_ex_errors)
{
    RunTest(__LINE__, "", "", NULL);
    RunTest(__LINE__, "abc", "", NULL);
    RunTest(__LINE__, "", "abc", NULL);
    RunTest(__LINE__, NULL, "abc", NULL);
    RunTest(__LINE__, "abc", NULL, NULL);
    RunTest(__LINE__, NULL, NULL, NULL);
}

TEST_F(TestIBUtilStrStrEx, test_strstr_ex)
{
    const char *haystack;

    haystack = "a";
    RunTest(__LINE__, haystack, "a",   haystack+0);
    haystack = "a";
    RunTest(__LINE__, haystack, "aa",  NULL);
    haystack = "a";
    RunTest(__LINE__, haystack, "ab",  NULL);

    haystack = "ab";
    RunTest(__LINE__, haystack, "a",   haystack+0);
    haystack = "ab";
    RunTest(__LINE__, haystack, "aa",  NULL);
    haystack = "ab";
    RunTest(__LINE__, haystack, "ab",  haystack+0);
    haystack = "ab";
    RunTest(__LINE__, haystack, "b",   haystack+1);
    haystack = "ab";
    RunTest(__LINE__, haystack, "ba",  NULL);

    haystack = "aa";
    RunTest(__LINE__, haystack, "a",   haystack+0);
    haystack = "aa";
    RunTest(__LINE__, haystack, "aa",  haystack+0);
    haystack = "aa";
    RunTest(__LINE__, haystack, "ab",  NULL);

    haystack = " aa";
    RunTest(__LINE__, haystack, "a",   haystack+1);
    haystack = " aa";
    RunTest(__LINE__, haystack, "aa",  haystack+1);
    haystack = " aa";
    RunTest(__LINE__, haystack, "aaa", NULL);
    haystack = " aa";
    RunTest(__LINE__, haystack, "ab",  NULL);

    haystack = "abc";
    RunTest(__LINE__, haystack, "abc", haystack+0);
    haystack = "abcabc";
    RunTest(__LINE__, haystack, "abc", haystack+0);
    haystack = "aabc";
    RunTest(__LINE__, haystack, "abc", haystack+1);
    haystack = "ababc";
    RunTest(__LINE__, haystack, "abc", haystack+2);
}

TEST_F(TestIBUtilStrStrEx, test_strstr_ex_nul1)
{
    const char *haystack;

    haystack = "a\0";
    RunTest(__LINE__, haystack, 2, "a",   haystack+0);
    haystack = "a\0a";
    RunTest(__LINE__, haystack, 3, "a",   haystack+0);
    haystack = "\0a\0a";
    RunTest(__LINE__, haystack, 4, "a",   haystack+1);
    haystack = "a\0a";
    RunTest(__LINE__, haystack, 3, "aa",  NULL);
    haystack = "a\0a";
    RunTest(__LINE__, haystack, 3, "ab",  NULL);

    haystack = "ab\0";
    RunTest(__LINE__, haystack, 3, "a",   haystack+0);
    haystack = "a\0a";
    RunTest(__LINE__, haystack, 3, "aa",  NULL);
    haystack = "a\0aa";
    RunTest(__LINE__, haystack, 4, "aa",  haystack+2);
    haystack = "\0ab";
    RunTest(__LINE__, haystack, 3, "ab",  haystack+1);
    haystack = "a\0b";
    RunTest(__LINE__, haystack, 3, "b",   haystack+2);

    haystack = "\0aa";
    RunTest(__LINE__, haystack, 3, "a",   haystack+1);
    haystack = "\0aa";
    RunTest(__LINE__, haystack, 3, "aa",  haystack+1);
    haystack = "\0aa";
    RunTest(__LINE__, haystack, 3, "ab",  NULL);

    haystack = "a\0a";
    RunTest(__LINE__, haystack, 3, "a",   haystack+0);
    haystack = "\0a\0a";
    RunTest(__LINE__, haystack, 4, "aa",  NULL);
    haystack = "\0aa\0";
    RunTest(__LINE__, haystack, 4, "aa",  haystack+1);
    haystack = "\0aa\0";
    RunTest(__LINE__, haystack, 4, "ab",  NULL);

    haystack = "\0 aa";
    RunTest(__LINE__, haystack, 4, "a",   haystack+2);
    haystack = "\0 aa";
    RunTest(__LINE__, haystack, 4, "aa",  haystack+2);
    haystack = "\0 aa";
    RunTest(__LINE__, haystack, 4, "ab",  NULL);

    haystack = " a\0a";
    RunTest(__LINE__, haystack, 4, "a",   haystack+1);
    haystack = " a\0a";
    RunTest(__LINE__, haystack, 4, "aa",  NULL);
    haystack = " a\0a";
    RunTest(__LINE__, haystack, 4, "ab",  NULL);

    haystack = "\0abc";
    RunTest(__LINE__, haystack, 4, "abc", haystack+1);
    haystack = "a\0bc";
    RunTest(__LINE__, haystack, 4, "abc", NULL);
    haystack = "ab\0c";
    RunTest(__LINE__, haystack, 4, "abc", NULL);

    haystack = "abc\0abc";
    RunTest(__LINE__, haystack, 7, "abc", haystack+0);
    haystack = "a\0abc";
    RunTest(__LINE__, haystack, 5, "abc", haystack+2);
    haystack = "ab\0abc";
    RunTest(__LINE__, haystack, 6, "abc", haystack+3);

    haystack = "ab\0cabc";
    RunTest(__LINE__, haystack, 7, "abc", haystack+4);
    haystack = "aa\0bc";
    RunTest(__LINE__, haystack, 5, "abc", NULL);
    haystack = "abab\0c";
    RunTest(__LINE__, haystack, 5, "abc", NULL);
}

TEST_F(TestIBUtilStrStrEx, test_strstr_ex_nul2)
{
    const char *haystack;

    haystack = "a\0";
    RunTest(__LINE__, haystack, 2, "a\0",  2,  haystack+0);
    haystack = "a\0a";
    RunTest(__LINE__, haystack, 3, "a\0",  2,  haystack+0);
    haystack = "\0a\0a";
    RunTest(__LINE__, haystack, 4, "a\0",  2,  haystack+1);
    haystack = "a\0a";
    RunTest(__LINE__, haystack, 3, "a\0a", 3,  haystack+0);
    haystack = "a\0a";
    RunTest(__LINE__, haystack, 3, "a\0b", 3,  NULL);

    haystack = "ab\0";
    RunTest(__LINE__, haystack, 3, "\0a",  2,  NULL);
    haystack = "\0ab\0";
    RunTest(__LINE__, haystack, 4, "\0a",  2,  haystack+0);
    haystack = "a\0aa";
    RunTest(__LINE__, haystack, 4, "a\0a", 3,  haystack+0);
    haystack = "a\0aa";
    RunTest(__LINE__, haystack, 4, "\0aa", 3,  haystack+1);
    haystack = "\0ab";
    RunTest(__LINE__, haystack, 3, "\0ab", 3,  haystack+0);
    haystack = "\0ab";
    RunTest(__LINE__, haystack, 3, "a\0b", 3,  NULL);
    haystack = "a\0b";
    RunTest(__LINE__, haystack, 3, "\0b",  2,  haystack+1);
    haystack = "a\0b";
    RunTest(__LINE__, haystack, 3, "b\0",  2,  NULL);

    haystack = "\0aa\0";
    RunTest(__LINE__, haystack, 4, "a\0",  2,  haystack+2);
    haystack = "\0aa";
    RunTest(__LINE__, haystack, 3, "\0aa", 3,  haystack+0);
    haystack = "a\0aa";
    RunTest(__LINE__, haystack, 4, "a\0a", 3,  haystack+0);
    haystack = "aa\0aa";
    RunTest(__LINE__, haystack, 5, "a\0a", 3,  haystack+1);

    haystack = "a\0a";
    RunTest(__LINE__, haystack, 3, "a\0",  2,  haystack+0);
    haystack = "\0a\0a";
    RunTest(__LINE__, haystack, 4, "a\0a", 3,  haystack+1);
    haystack = "\0aa\0";
    RunTest(__LINE__, haystack, 4, "aa\0", 3,  haystack+1);
    haystack = "\0aa\0";
    RunTest(__LINE__, haystack, 4, "\0ab", 3,  NULL);

    haystack = "\0 aa";
    RunTest(__LINE__, haystack, 4, "a\0",  2,  NULL);
    haystack = "\0 aa\0";
    RunTest(__LINE__, haystack, 5, "aa\0", 3,  haystack+2);
    haystack = "\0 aa\0";
    RunTest(__LINE__, haystack, 4, "ab\0", 3,  NULL);

    haystack = " a\0a";
    RunTest(__LINE__, haystack, 4, "a\0",  2,  haystack+1);
    haystack = " a\0a";
    RunTest(__LINE__, haystack, 4, "\0a",  2,  haystack+2);

    haystack = "\0abc";
    RunTest(__LINE__, haystack, 4, "\0abc", 4, haystack+0);
    haystack = "a\0bc";
    RunTest(__LINE__, haystack, 4, "a\0bc", 4, haystack+0);
    haystack = "ab\0c";
    RunTest(__LINE__, haystack, 4, "ab\0c", 4, haystack+0);

    haystack = "abc\0abc";
    RunTest(__LINE__, haystack, 7, "abc\0", 4, haystack+0);
    haystack = "a\0abc";
    RunTest(__LINE__, haystack, 5, "\0abc", 4, haystack+1);
    haystack = "a\0abc";
    RunTest(__LINE__, haystack, 5, "abc\0", 4, NULL);
    haystack = "ab\0abc";
    RunTest(__LINE__, haystack, 6, "\0abc", 4, haystack+2);

    haystack = "ab\0cabc";
    RunTest(__LINE__, haystack, 7, "\0abc", 4, NULL);
    haystack = "aa\0bc";
    RunTest(__LINE__, haystack, 5, "\0abc", 4, NULL);
    haystack = "abab\0c";
    RunTest(__LINE__, haystack, 5, "abc\0", 4, NULL);
}

