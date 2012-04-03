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

#ifndef _IB_APACHE_HTTPD_H_
#define _IB_APACHE_HTTPD_H_

/**
 * @file
 * @brief IronBee - Apache Httpd Functions
 *
 * @author Brian Rectanus <brectanus@qualys.com>
 */

#include <ap_config.h>
#include <httpd.h>
#include <http_config.h>
#include <http_log.h>
#include <http_protocol.h>

#include <ironbee/server.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ironbee_config_t ironbee_config_t;

/** Server configuration */
struct ironbee_config_t {
    int            enabled;           /**< Is plugin enabled? */
    const char    *config;            /**< Config file */
    size_t         buf_size;          /**< Buffer size. */
    size_t         flush_size;        /**< Bytes in buffer to trigger flush. */
};

#ifdef __cplusplus
}
#endif

#endif /* _IB_APACHE_HTTPD_H_ */
