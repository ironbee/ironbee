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
 * @brief IronBee++ Clock Implementation
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 **/

#include <ironbeepp/clock.hpp>

using namespace boost::posix_time;

static const ptime c_epoch = from_time_t(0);

/*
 * This is used to compare against time_duration::ticks_per_second() so we
 * we use that type.
 */
static const time_duration::tick_type c_microseconds_per_second = 1000000;

namespace IronBee {

// time_duration behaves strangely with large microsecond values hence the use
// of both seconds() and microseconds().

boost::posix_time::ptime ib_to_ptime(ib_timeval_t tv)
{
    return c_epoch + seconds(tv.tv_sec)
                   + microseconds(tv.tv_usec);
}

boost::posix_time::ptime ib_to_ptime(ib_timeval_t tv, ib_time_t offset)
{
    return ib_to_ptime(tv)
           + seconds(offset / c_microseconds_per_second)
           + microseconds(offset % c_microseconds_per_second);
}

ib_time_t ptime_to_ib(const boost::posix_time::ptime& t)
{
    time_duration td = t - c_epoch;
    long fractional = td.fractional_seconds();
    uint64_t us;

    if (time_duration::ticks_per_second() >= c_microseconds_per_second) {
        us = fractional *
             (time_duration::ticks_per_second() / c_microseconds_per_second);
    }
    else {
        us = fractional /
            (c_microseconds_per_second / time_duration::ticks_per_second());
    }
    return td.total_seconds() * c_microseconds_per_second + us;
}

namespace {

using namespace boost::posix_time;
time_input_facet* rfc1123_facet = new time_input_facet("%a, %d %b %Y %H:%M:%S GMT");
time_input_facet* rfc850_facet  = new time_input_facet("%A  %d-%b-%y %H:%M:%S GMT");
time_input_facet* asctime_facet = new time_input_facet("%a %b %d %H:%M:%S %Y");

std::locale rfc1123_locale(std::locale(""), rfc1123_facet);
std::locale rfc850_locale(std::locale(""), rfc850_facet);
std::locale asctime_locale(std::locale(""), asctime_facet);

/**
 * Attempt to parse @a str into a ptime object using @a loc.
 *
 * @param[out] p The time is put here.
 * @param[in] str The string to parse.
 * @param[in] loc The local to use. This should have a
 *            boost::posix_time::time_input_facet associated with it.
 *
 * @returns True on success. False on failure.
 */
bool parse_ptime(
    boost::posix_time::ptime& p,
    const std::string& str,
    std::locale& loc
)
{
    std::istringstream is(str);
    is.imbue(loc);
    is >> p;

    if (is.fail()) {
        return false;
    }

    return true;
}
} // end anonymous namespace

ib_time_t parse_ib_time(const std::string& str)
{
    boost::posix_time::ptime p = parse_time(str);

    if (p == boost::posix_time::not_a_date_time) {
        return 0;
    }

    return ptime_to_ib(p);
}

boost::posix_time::ptime parse_time(const std::string& str) {
    boost::posix_time::ptime p;

    if (parse_ptime(p, str, rfc1123_locale)) {
        return p;
    }

    if (parse_ptime(p, str, rfc850_locale)) {
        return p;
    }

    if (parse_ptime(p, str, asctime_locale)) {
        return p;
    }

    return p;
}

} // IronBee
