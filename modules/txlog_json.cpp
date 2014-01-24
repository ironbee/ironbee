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
 * @brief IronBee Modules --- Transaction Logs JSON Builder
 *
 * How to build a JSON file for a Transaction Log.
 *
 * @author Sam Baskinger <sbaskinger@qualys.com>
 */

#include "txlog_json.hpp"

#include <boost/any.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/time_facet.hpp>
#include <boost/foreach.hpp>

extern "C" {
#include <yajl/yajl_common.h>
#include <yajl/yajl_gen.h>
}

#include <cstdlib>

TxLogJsonBuffer::TxLogJsonBuffer():
    m_json_buffer_len(0),
    m_json_buffer_sz(1024),
    m_json_buffer(reinterpret_cast<char *>(malloc(m_json_buffer_sz)))
 {
    if (m_json_buffer == NULL)
    {
        BOOST_THROW_EXCEPTION(
            IronBee::ealloc()
                << IronBee::errinfo_what("Allocating JSON buffer."));
    }
}

TxLogJsonBuffer::~TxLogJsonBuffer()
{
    if (m_json_buffer)
    {
        free(m_json_buffer);
    }
}

void TxLogJsonBuffer::divorce_buffer(char*& buf, size_t& buf_len)
{
    buf = m_json_buffer;
    buf_len = m_json_buffer_len;

    m_json_buffer_len = 0;
    m_json_buffer_sz = 0;
    m_json_buffer = NULL;
}

void TxLogJsonBuffer::append(const char *str, size_t str_len)
{
    size_t new_len = m_json_buffer_len + str_len;

    /* Grow the buffer by 1k as that should satisfy most conditions.
     * Should we be in an unexpected situation, loop until we get it right.
     */
    if (new_len > m_json_buffer_sz)
    {
        /* Compute the new buffer length on a 1024-length page. */
        m_json_buffer_sz = new_len + (new_len % 1024);
        m_json_buffer = reinterpret_cast<char *>(
            realloc(m_json_buffer, m_json_buffer_sz));

        if (m_json_buffer == NULL)
        {
            BOOST_THROW_EXCEPTION(
                IronBee::ealloc()
                    << IronBee::errinfo_what("Allocating JSON buffer."));
        }
    }

    std::copy(str, str + str_len, m_json_buffer + m_json_buffer_len);
    m_json_buffer_len = new_len;
}

extern "C" {

/**
 * @ref TxLogJson callback to print given JSON into a malloc'ed buffer.
 */
static void txlog_json_print_callback(void *ctx, const char *str, size_t len)
{
    TxLogJsonBuffer *buffer =
        boost::any_cast<TxLogJsonBuffer*>(
            *reinterpret_cast<boost::any *>(ctx));

    buffer->append(str, len);
}
} /* extern "C" */

TxLogJson::TxLogJson():
    m_any(&m_buffer),
    m_json_generator(yajl_gen_alloc(NULL))
{
    if (m_json_generator == NULL) {
        BOOST_THROW_EXCEPTION(
            TxLogJsonError()
                << IronBee::errinfo_what("Could not create JSON generator."));
    }

    yajl_gen_config(m_json_generator, yajl_gen_beautify, 0);
    yajl_gen_config(m_json_generator, yajl_gen_indent_string, "");
    yajl_gen_config(
        m_json_generator,
        yajl_gen_print_callback,
        &txlog_json_print_callback,
        reinterpret_cast<void *>(&m_any)
    );
}

TxLogJson::~TxLogJson()
{
    assert (m_json_generator);

    yajl_gen_free(m_json_generator);
}

void TxLogJson::render(char*& buf, size_t& buf_sz)
{
    m_buffer.divorce_buffer(buf, buf_sz);
}

TxLogJsonMap<TxLogJson> TxLogJson::withMap()
{
    return TxLogJsonMap<TxLogJson>(*this, *this);
}

TxLogJsonArray<TxLogJson> TxLogJson::withArray()
{
    return TxLogJsonArray<TxLogJson>(*this, *this);
}

void TxLogJson::withTime(const boost::posix_time::ptime& val)
{
    int yajl_rc;

    /* The output stream the does the formatting for the date using facets. */
    std::ostringstream osstream;

    /* The formatted date is stored here. */
    std::string str;

    /* Location of the last '.' character in the formatted date.
     * This is used to trim fractional second precision. */
    size_t dot_loc;

    /* Location of the last '-' character in the formatted date.
     * This is used to trim fractional second precision. */
    size_t dash_loc;

    /* Setup posix time formatting. The timezone %q is replaced with -00:00.*/
    osstream.imbue(
        std::locale(
            osstream.getloc(),
            /* NOTE: Facet object is destroyed with str. */
            new boost::date_time::time_facet
            <
                boost::posix_time::ptime,
                char
            > (
                "%Y-%m-%dT%H:%M:%S.%f-00:00"
            )
        )
    );

    /* Format the raw date string. */
    osstream << val;

    /* Caputre the formatted date string. */
    str = osstream.str();

    /* Find '.' & '-' surrounding the frac. seconds; Trim to 3 places. */
    dot_loc = str.rfind(".");
    dash_loc = str.rfind("-");
    str = str.replace(dot_loc + 4, dash_loc - dot_loc - 4, "");

    /* Finally, convert the string to JSON. */
    yajl_rc = yajl_gen_string(
        m_json_generator,
        reinterpret_cast<const unsigned char *>(str.data()),
        str.length());

    /* Check and throw if there was a problem. */
    if (yajl_rc != yajl_gen_status_ok)
    {
        BOOST_THROW_EXCEPTION(TxLogJsonError() <<
            IronBee::errinfo_what("Failed to write string."));
    }
}

void TxLogJson::withString(const std::string& val)
{
    withString(val.data(), val.length());
}

void TxLogJson::withString(const char* val)
{
    withString(val, strlen(val));
}

void TxLogJson::withString(const char* val, size_t len)
{
    int yajl_rc = yajl_gen_string(
        m_json_generator,
        reinterpret_cast<const unsigned char *>(val),
        len);

    if (yajl_rc != yajl_gen_status_ok) {
        BOOST_THROW_EXCEPTION(TxLogJsonError() <<
            IronBee::errinfo_what("Failed to write string."));
    }
}

void TxLogJson::withInt(int val)
{
    int yg_rc = yajl_gen_integer(m_json_generator, val);
    if (yg_rc != yajl_gen_status_ok) {
        BOOST_THROW_EXCEPTION(TxLogJsonError()
            << IronBee::errinfo_what("Failed to generate type."));
    }
}

void TxLogJson::withDouble(double val)
{
    int yg_rc = yajl_gen_double(m_json_generator, val);
    if (yg_rc != yajl_gen_status_ok) {
        BOOST_THROW_EXCEPTION(TxLogJsonError()
            << IronBee::errinfo_what("Failed to generate type."));
    }
}

void TxLogJson::withBool(bool val)
{
    int yg_rc = yajl_gen_bool(m_json_generator, (val)?1:0);
    if (yg_rc != yajl_gen_status_ok) {
        BOOST_THROW_EXCEPTION(TxLogJsonError()
            << IronBee::errinfo_what("Failed to generate type."));
    }
}

void TxLogJson::withNull()
{
    int yg_rc = yajl_gen_null(m_json_generator);
    if (yg_rc != yajl_gen_status_ok) {
        BOOST_THROW_EXCEPTION(TxLogJsonError()
            << IronBee::errinfo_what("Failed to generate type."));
    }
}
