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
 * IronBee - Unit testing utility functions
 *
 * @author Brian Rectanus <brectanus@qualys.com>
 */

#include "gtest/gtest.h"
#include "gtest/gtest-spi.h"

#include <ironbee/types.h>
#include <ironbee/engine.h>
#include <ironbee/util.h>
#include <ironbee/module.h>
#include <ironbee/plugin.h>

#include "ironbee_private.h"

/**
 * @defgroup IronBeeTests Unit Testing
 * 
 * Various utility macros/functions for unit testing.
 *
 * @ingroup IronBee
 * @{
 */

static ib_plugin_t ibt_ibplugin = {
    IB_PLUGIN_HEADER_DEFAULTS,
    "unit_tests"
};

/**
 * Create and initialize a new engine, asserting correctness.
 *
 * @param pe Address which engine handle is written
 */
#define ibtest_engine_create(pe) \
    do { \
        ib_engine_t **ibt_pib = (pe); \
        atexit(ib_shutdown); \
        ib_trace_init(NULL); \
        ASSERT_EQ(IB_OK, ib_initialize()); \
        ASSERT_EQ(IB_OK, ib_engine_create(ibt_pib, &ibt_ibplugin)); \
        ASSERT_TRUE(*ibt_pib); \
        ASSERT_TRUE((*ibt_pib)->mp); \
        ASSERT_EQ(IB_OK, ib_engine_init(*ibt_pib)); \
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
 * Configure the engine from a config file.
 *
 * @param e Engine handle
 * @param fn Config file name
 */
#define ibtest_engine_config_file(e,fn) \
    do { \
        ib_engine_t *ibt_ib = (e); \
        ib_cfgparser_t *ibt_cp; \
        ASSERT_EQ(IB_OK, ib_state_notify_cfg_started(ibt_ib)); \
        ASSERT_EQ(IB_OK, ib_cfgparser_create(&ibt_cp, ibt_ib)); \
        ASSERT_TRUE(ibt_cp); \
        ASSERT_EQ(IB_OK, ib_cfgparser_parse(ibt_cp, (fn))); \
        ib_cfgparser_destroy(ibt_cp); \
        ASSERT_EQ(IB_OK, ib_state_notify_cfg_finished(ibt_ib)); \
    } while(0)

/**
 * Configure the engine from a string buffer.
 *
 * @param e Engine handle
 * @param buf String buffer containing config data
 * @param len Buffer length
 */
#define ibtest_engine_config_buf(e,buf,len) \
    do { \
        ib_engine_t *ibt_ib = (e); \
        ib_cfgparser_t *ibt_cp; \
        ASSERT_EQ(IB_OK, ib_state_notify_cfg_started(ibt_ib)); \
        ASSERT_EQ(IB_OK, ib_cfgparser_create(&ibt_cp, ibt_ib)); \
        ASSERT_TRUE(ibt_cp); \
        ASSERT_EQ(IB_OK, ib_cfgparser_ragel_parse_chunk(ibt_cp,(buf),(len))); \
        ib_cfgparser_destroy(ibt_cp); \
        ASSERT_EQ(IB_OK, ib_state_notify_cfg_finished(ibt_ib)); \
    } while(0)

/**
 * Load a module into the engine.
 *
 * @param e Engine handle
 * @param fn Module file name
 * @param pm Address which module handle is written
 */
#define ibtest_engine_module_load(e,fn,pm) \
    do { \
        ib_module_t **ibt_pm = (pm); \
        ASSERT_EQ(IB_OK, ib_module_load(ibt_pm,(e),(fn))); \
        ASSERT_TRUE(*ibt_pm); \
    } while(0)

/**
 * Initialize a module which source was already included.
 *
 * @note This assumes that a symbol of the name defined by
 * @ref IB_MODULE_SYM exists.  This is normally accomplished
 * by including your module source prior to this call via
 * preprocessor directive.
 *
 * @param e Engine handle
 * @param pm Address which module handle is written
 */
#define ibtest_engine_module_init(e,pm) \
    do { \
        ib_module_t **ibt_pm = (pm); \
        *ibt_pm = IB_MODULE_SYM(); \
        ASSERT_TRUE(*ibt_pm); \
        ASSERT_EQ(IB_OK, ib_module_init(*ibt_pm,(e))); \
    } while(0)

/**
 * Memory comparison function that works with @ref ASSERT_PRED3.
 *
 * @param[in] v1 First memory location
 * @param[in] v2 Second memory location
 * @param[in] n Size of memory (in bytes) to compare
 *
 * @returns true if memory values are equal
 */
bool ibtest_memeq(const void *v1, const void *v2, size_t n)
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
    ASSERT_PRED3(ibtest_memeq,(a),(b),(n))

/**
 * @} IronBeeTests
 */

