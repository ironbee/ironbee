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
 * @brief IronBee --- SQL Remove Comments
 *
 * @author Sam Baskinger <sbaskinger@qualys.com>
 *
 * Provide transformation logic to remove comments from various
 * database types.
 */

#ifndef _SQL_REMOVE_COMMENTS__HPP_
#define _SQL_REMOVE_COMMENTS__HPP_

#include <ironbeepp/module.hpp>

namespace remove_comments {

void register_trasformations(IronBee::Module m);

} // namespace ibmod_sql

#endif // _SQL_REMOVE_COMMENTS__HPP_
