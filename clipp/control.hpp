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
 * @brief IronBee &mdash; CLIPP Control Exceptions.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#ifndef __IRONBEE__CLIPP__CONTROL__
#define __IRONBEE__CLIPP__CONTROL__

namespace IronBee {
namespace CLIPP {

/**
 * Abort current chain.
 *
 * This causes the entire chain to immediately end.  If there are any chains
 * left, clipp will go to the next chain.
 **/
struct clipp_break {};

/**
 * Discard input.
 *
 * This causes clipp to discard the current input and ask the generator for
 * a new input.  This is identical behavior to any non-control exception
 * except that no error message is generated.
 **/
struct clipp_continue {};

} // CLIPP
} // IronBee

#endif
