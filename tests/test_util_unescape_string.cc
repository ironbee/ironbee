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
/// @brief IronBee - UUID Test Functions
/// 
/// @author Brian Rectanus <brectanus@qualys.com>
//////////////////////////////////////////////////////////////////////////////

#include "ironbee_config_auto.h"
#include "ironbee/util.h"

#include "gtest/gtest.h"
#include "gtest/gtest-spi.h"

TEST(TestIBUtilUnescapeString, singleCharacter) {
  char str[7] = "\\r\\n\\t";
  char str2[4];
  size_t len;
  ASSERT_EQ(IB_OK, ib_util_unescape_string(str2,
                                           &len,
                                           str,
                                           strlen(str),
                                           IB_UTIL_UNESCAPE_NULTERMINATE));
  
  EXPECT_EQ('\r', str2[0]);
  EXPECT_EQ('\n', str2[1]);
  EXPECT_EQ('\t', str2[2]);
  EXPECT_EQ('\0', str2[3]);
  ASSERT_EQ(3UL, len);
}

TEST(TestIBUtilUnescapeString, singleBytes) {
  const char *str = "\\x01\\x02";
  char str2[3];
  char chk[2] = { 1, 2 };
  size_t len;
  ASSERT_EQ(IB_OK, ib_util_unescape_string(str2,
                                           &len,
                                           str,
                                           strlen(str),
                                           IB_UTIL_UNESCAPE_NULTERMINATE));
  
  EXPECT_EQ(chk[0], str2[0]);
  EXPECT_EQ(chk[1], str2[1]);
  EXPECT_EQ('\0', str2[2]);
  EXPECT_EQ(2UL, len);
}

TEST(TestIBUtilUnescapeString, longBytes) {
  const char *str = "\\u0001\\u4321";
  char str2[5];
  char chk[4] = { 0, 1, 67, 33 };
  size_t len;
  ASSERT_EQ(IB_OK, ib_util_unescape_string(str2,
                                           &len,
                                           str,
                                           strlen(str),
                                           IB_UTIL_UNESCAPE_NULTERMINATE));
  
  EXPECT_EQ(chk[0], str2[0]);
  EXPECT_EQ(chk[1], str2[1]);
  EXPECT_EQ(chk[2], str2[2]);
  EXPECT_EQ(chk[3], str2[3]);
  EXPECT_EQ('\0', str2[4]);
  EXPECT_EQ(4UL, len);
}

TEST(TestIBUtilUnescapeString, shortSingleBytesEndOfLine) {
  const char *str = "\\x01\\x0";
  char str2[3];
  size_t len;
  ASSERT_EQ(IB_EINVAL, ib_util_unescape_string(str2,
                                               &len,
                                               str,
                                               strlen(str),
                                               IB_UTIL_UNESCAPE_NULTERMINATE));
}

TEST(TestIBUtilUnescapeString, shortSingleBytes) {
  const char *str = "\\x0\\x00";
  char str2[3];
  size_t len;
  ASSERT_EQ(IB_EINVAL, ib_util_unescape_string(str2,
                                               &len,
                                               str,
                                               strlen(str),
                                               IB_UTIL_UNESCAPE_NULTERMINATE));
}

TEST(TestIBUtilUnescapeString, shortLongBytes) {
  const char *str = "\\u001\\u4321";
  char str2[5];
  size_t len;
  ASSERT_EQ(IB_EINVAL, ib_util_unescape_string(str2,
                                               &len,
                                               str,
                                               strlen(str),
                                               IB_UTIL_UNESCAPE_NULTERMINATE));
}

TEST(TestIBUtilUnescapeString, shortLongBytesEndOfLine) {
  const char *str = "\\u0001\\u431";
  char str2[5];
  size_t len;
  ASSERT_EQ(IB_EINVAL, ib_util_unescape_string(str2,
                                               &len,
                                               str,
                                               strlen(str),
                                               IB_UTIL_UNESCAPE_NULTERMINATE));
}

TEST(TestIBUtilUnescapeString, nochange01) {
  const char* str = "LoadModule";
  char str2[100];
  size_t len;
  ASSERT_EQ(IB_OK, ib_util_unescape_string(str2,
                                           &len,
                                           str,
                                           strlen(str),
                                           IB_UTIL_UNESCAPE_NULTERMINATE));

  ASSERT_EQ(0, strcmp(str, str2));
}

TEST(TestIBUtilUnescapeString, nullsInString) {
  const char *src1 = "hi\\x00hello";
  const char *src2 = "hi\\u0000hello";
  const char *src3 = "hi\\u0100hello";
  const char *src4 = "hi\\u0001hello";
  char dst[10];
  size_t len;

  /* \u0000 test. */
  ASSERT_EQ(IB_OK, ib_util_unescape_string(dst,
                                           &len,
                                           src1,
                                           strlen(src1),
                                           IB_UTIL_UNESCAPE_NULTERMINATE));
  ASSERT_STREQ("hi", dst);
  ASSERT_STREQ("hello", dst+3);
  ASSERT_EQ(IB_EBADVAL, ib_util_unescape_string(dst,
                                               &len,
                                               src1,
                                               strlen(src1),
                                               IB_UTIL_UNESCAPE_NONULL |
                                               IB_UTIL_UNESCAPE_NULTERMINATE));

  /* \x00 test. */
  ASSERT_EQ(IB_OK, ib_util_unescape_string(dst,
                                           &len,
                                           src2,
                                           strlen(src2),
                                           IB_UTIL_UNESCAPE_NULTERMINATE));
  ASSERT_STREQ("hi", dst);
  ASSERT_STREQ("hello", dst+4);
  ASSERT_EQ(IB_EBADVAL, ib_util_unescape_string(dst,
                                               &len,
                                               src2,
                                               strlen(src2),
                                               IB_UTIL_UNESCAPE_NONULL | 
                                               IB_UTIL_UNESCAPE_NULTERMINATE));

  ASSERT_EQ(IB_EBADVAL, ib_util_unescape_string(dst,
                                               &len,
                                               src3,
                                               strlen(src3),
                                               IB_UTIL_UNESCAPE_NONULL | 
                                               IB_UTIL_UNESCAPE_NULTERMINATE));

  ASSERT_EQ(IB_EBADVAL, ib_util_unescape_string(dst,
                                               &len,
                                               src4,
                                               strlen(src4),
                                               IB_UTIL_UNESCAPE_NONULL | 
                                               IB_UTIL_UNESCAPE_NULTERMINATE));
}

TEST(TestIBUtilUnescapeString, removesQuotes) {
    const char *src = "\\\"hi\\\'";
    char dst[5];
    size_t len;
    ASSERT_EQ(IB_OK, ib_util_unescape_string(dst,
                                             &len,
                                             src,
                                             strlen(src),
                                             IB_UTIL_UNESCAPE_NULTERMINATE));
    ASSERT_STREQ("\"hi\'", dst);
}

