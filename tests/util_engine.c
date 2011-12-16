/*****************************************************************************
 * Licensed to Qualys, Inc. (QUALYS) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * QUALYS licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *****************************************************************************/

/**
 * @file
 *
 * IronBee - Utility functions for testing the engine.
 *
 * @author Brian Rectanus <brectanus@qualys.com>
 */

#include "gtest/gtest.h"
#include "gtest/gtest-spi.h"

static ib_plugin_t ibt_ibplugin = {
    IB_PLUGIN_HEADER_DEFAULTS,
    "unit_tests"
};

/**
 * Create a new engine, asserting correctness.
 *
 * @param pe Address which engine handle is written
 */
#define ibtest_engine_create(pe) \
    do { \
        ib_engine_t **ibt_pib = (pe); \
        atexit(ib_shutdown); \
        ASSERT_EQ(IB_OK, ib_initialize()); \
        ASSERT_EQ(IB_OK, ib_engine_create(ibt_pib, &ibt_ibplugin)); \
        ASSERT_TRUE(*ibt_pib); \
        ASSERT_TRUE((*ibt_pib)->mp); \
    } while (0)

/**
 * Destroy engine, asserting correctness.
 *
 * @param pe Address which engine handle is written
 */
#define ibtest_engine_destroy(e) \
    do { \
        ib_engine_destroy((e)); \
    } while (0)


/**
 * Memory comparison function that works with @ref ASSERT_PRED3.
 *
 * @param[in] v1 First memory location
 * @param[in] v2 Second memory location
 * @param[in] n Size of memory (in bytes) to compare
 *
 * @returns true if memory values are equal
 */
bool memeq(const void *v1, const void *v2, size_t n)
{
    return memcmp(v1, v2, n) ? false : true;
}

/**
 * Assert that values in memory are equal.
 *
 * @param a First memory location
 * @param b Second memory location
 * @param n Length in bytes to compare
 */
#define ASSERT_MEMEQ(a,b,n) \
    ASSERT_PRED3(memeq,(a),(b),(n))
