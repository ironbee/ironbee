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

#ifndef _IB_CONFIG_PARSER_H_
#define _IB_CONFIG_PARSER_H_

/**
 * @file
 * @brief IronBee &mdash; Configuration Parser
 *
 * @author Brian Rectanus <brectanus@qualys.com>
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

#include <sys/types.h>
#include <unistd.h>

#include <ironbee/build.h>
#include <ironbee/release.h>
#include <ironbee/config.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup IronBeeConfigParser Configuration Parser
 * @ingroup IronBeeConfig
 *
 * Code specific to the RAGEL configuration parser.
 *
 * @{
 */

ib_status_t DLL_PUBLIC ib_cfgparser_ragel_parse_chunk(ib_cfgparser_t *cp,
                                                      const char *buf,
                                                      const size_t blen,
                                                      const char *file,
                                                      unsigned lineno,
                                                      const int is_last_block);

/**
 * @} IronBeeConfigParser
 */

#ifdef __cplusplus
}
#endif

#endif /* IB_CONFIG_PARSER_H_ */
