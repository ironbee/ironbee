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
 * @brief IronBee++ &mdash; Clock
 *
 * This file defines ib_to_ptime() and ptime_to_ib() to convert between
 * boost's ptime and IronBee's ib_time_t.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#ifndef __IBPP__CLOCK__
#define __IBPP__CLOCK__

#include <ironbee/clock.h>
#include <boost/date_time/posix_time/posix_time.hpp>

namespace IronBee {

/**
 * Convert ib_time_t (microseconds since epoch) to ptime.
 *
 * @param[in] t Microseconds since epoch.
 * @return @a t as ptime.
 **/
boost::posix_time::ptime ib_to_ptime(ib_time_t t);

/**
 * Convert ptime to ib_time_t (microseconds since epoch).
 *
 * @param[in] t Time.
 * @return @a t as microseconds since epoch.
 **/
ib_time_t ptime_to_ib(const boost::posix_time::ptime& t);

} // IronBee

#endif
