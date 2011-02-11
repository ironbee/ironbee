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

#ifndef _IB_PLUGINS_H_
#define _IB_PLUGINS_H_

/**
 * @file
 * @brief IronBee - Plugins
 *
 * @author Brian Rectanus <brectanus@qualys.com>
 */

#include <ironbee/ironbee.h>
#include <ironbee/util.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup IronBeePlugins Plugins
 * @ingroup IronBee
 * @{
 */

#define IB_PLUGIN_HEADER_DEFAULTS     IB_VERNUM, \
                                      IB_ABINUM, \
                                      IB_VERSION, \
                                      __FILE__

/** Plugin Structure */
typedef struct ib_plugin_t ib_plugin_t;

struct ib_plugin_t {
    /* Header */
    int                      vernum;   /**< Engine version number */
    int                      abinum;   /**< Engine ABI Number */
    const char              *version;  /**< Engine version string */
    const char              *filename; /**< Plugin code filename */

    const char              *name;     /**< Unique plugin name */
};

/**
 * @} IronBeePlugins
 */

#ifdef __cplusplus
}
#endif

#endif /* IB_PLUGINS_H_ */
