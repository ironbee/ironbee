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
 ****************************************************************************/

/**
 * @file
 * @brief IronBee -- IronBee Private Configuration
 *
 * @author Sam Baskinger <sbaskinger@qualys.com>
 */

#ifndef _CONFIG_PRIVATE_H_
#define _CONFIG_PRIVATE_H_

#include <ironbee/config.h>

/**
 * @param[in] cp Configuration parser.
 * @param[in] file The file to parse.
 * @param[in] eof_mask This is boolean AND'ed with the EOF flag sent to ragel.
 *            If @a eof_mask if false, then ragel will never know that the 
 *            input stream has ended. This is the case when including 
 *            files. Only the top-level file parser will
 *            set this to true so Ragel is told when the 
 *            top-level file is done being parsed.
 * @returns
 * - IB_OK on success.
 * - IB_EALLOC on allocation errors.
 * - Others appropriate to the error. See error log for details.
 */
ib_status_t DLL_LOCAL
ib_cfgparser_parse_private(ib_cfgparser_t *cp, const char *file, bool eof_mask);

#endif /* _CONFIG_PRIVATE_H_ */
