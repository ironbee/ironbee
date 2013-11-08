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
 * @brief IronBee --- ParserSuite module.
 *
 * Exposes ParserSuite parsers as operators.  All operators are true iff
 * parse was successful and store the results in the capture field.
 *
 * Parsers:
 * - `parseAuthority ""` -- `username, password, host, port`
 * - `parseURI ""` -- `scheme, authority, path, query, fragment`
 * - `parseRequestLine ""` -- `method, uri, version`
 * - `parseResponseLine ""` -- `version, status, message`
 * - `parsePath "AB"` -- `directory, file, base, extension`.  A is used as
 *   directory separator (default `/`) and B is used as extension separator
 *   (default `.`).
 */

#include <ironbeepp/all.hpp>

#include "parser_suite.hpp"

#include <boost/assign.hpp>
#include <boost/bind.hpp>
#include <boost/bind/protect.hpp>
#include <boost/function.hpp>

using namespace std;

using boost::bind;

using IronBee::ParserSuite::span_t;

namespace {

//! Module delegate.
class Delegate :
    public IronBee::ModuleDelegate
{
public:
    //! Constructor.  Sets up operators.
    explicit
    Delegate(IronBee::Module module);
};

} // Anonymous namespace

IBPP_BOOTSTRAP_MODULE_DELEGATE("ps", Delegate)

// Implementation

namespace {

/**
 * Set a field.
 *
 * Adds a field to @a capture with name @a name and value an alias of @a span
 * but only if @a span is non-empty.
 *
 * @param[in] pool    Memory pool to use.
 * @param[in] capture List to add field to.
 * @param[in] name    Name of field (NUL terminated).
 * @param[in] span    Span to use for value.
 **/
void set_field(
    IronBee::MemoryPool pool,
    IronBee::Field      capture,
    const char *        name,
    span_t              span
)
{
    if (span.end() - span.begin() > 0) {
        capture.mutable_value_as_list<IronBee::Field>().push_back(
            IronBee::Field::create_no_copy_byte_string(
                pool,
                name, strlen(name),
                IronBee::ByteString::create_alias(
                    pool,
                    span.begin(), span.end() - span.begin()
                )
            )
        );
    }
}

//! Metafunction to calculate result list type.
template <typename ResultType>
struct result_list_type
{
    typedef list<pair<const char *, span_t ResultType::*> > type;
};

//! Operator executor function.
template <typename ResultType>
int executor(
    boost::function<ResultType(span_t&)>               parser,
    const typename result_list_type<ResultType>::type& result_list,
    IronBee::Transaction                        tx,
    IronBee::ConstField                         input,
    IronBee::Field                              capture
)
{
    IronBee::MemoryPool pool = tx.memory_pool();

    if (input.type() != IronBee::ConstField::BYTE_STRING) {
        return 0;
    }

    IronBee::ConstByteString bs = input.value_as_byte_string();
    span_t input_span;
    if (bs && bs.length() > 0) {
        input_span =
            span_t(bs.const_data(), bs.const_data() + bs.length());
    }

    ResultType result;
    try {
        result = parser(input_span);
    }
    catch (IronBee::ParserSuite::error) {
        return 0;
    }

    if (capture) {
        set_field(pool, capture, "remainder", input_span);
        BOOST_FOREACH(
            const typename result_list_type<ResultType>::type::value_type& v,
            result_list
        ) {
            set_field(pool, capture, v.first, result.*(v.second));
        }
    }

    return 1;
}

//! Operator generator function.
template <typename ResultType>
IronBee::Operator::operator_instance_t generator(
    boost::function<ResultType(span_t&)>               parser,
    const typename result_list_type<ResultType>::type& result_list,
    const char*                                 param
)
{
    if (string(param) != "") {
        BOOST_THROW_EXCEPTION(
            IronBee::einval() << IronBee::errinfo_what(
                "Parser operator takes no arguments."
            )
        );
    }

    return boost::bind<int>(
        executor<ResultType>,
        parser,
        boost::cref(result_list),
        _1, _2, _3
    );
}

//! parsePath generator function.
IronBee::Operator::operator_instance_t parse_path_generator(
    const result_list_type<
        IronBee::ParserSuite::parse_path_result_t
    >::type&    result_list,
    const char* param
)
{
    char directory_separator = '/';
    char extension_separator = '.';

    if (param[0]) {
        directory_separator = param[0];
        if (param[1]) {
            extension_separator = param[1];
        }
    }

    return boost::bind<int>(
        executor<IronBee::ParserSuite::parse_path_result_t>,
        boost::protect(
            boost::bind(
                IronBee::ParserSuite::parse_path,
                _1,
                directory_separator,
                extension_separator
            )
        ),
        boost::cref(result_list),
        _1, _2, _3
    );
}

Delegate::Delegate(IronBee::Module module) :
    IronBee::ModuleDelegate(module)
{
    using namespace IronBee::ParserSuite;
    using namespace boost::assign;

    IronBee::MemoryPool pool = module.engine().main_memory_pool();

    ib_flags_t capabilities =
        IB_OP_CAPABILITY_NON_STREAM |
        IB_OP_CAPABILITY_STREAM |
        IB_OP_CAPABILITY_CAPTURE
        ;

    {
        static const result_list_type<parse_uri_result_t>::type
            result_list = map_list_of
                ("scheme",    &parse_uri_result_t::scheme)
                ("authority", &parse_uri_result_t::authority)
                ("path",      &parse_uri_result_t::path)
                ("query",     &parse_uri_result_t::query)
                ("fragment",  &parse_uri_result_t::fragment)
            ;

        IronBee::Operator::create(
            pool,
            "parseURI",
            capabilities,
            bind<IronBee::Operator::operator_instance_t>(
                generator<parse_uri_result_t>,
                &parse_uri,
                boost::cref(result_list),
                _2
            )
        ).register_with(module.engine());
    }

    {
        static const result_list_type<parse_request_line_result_t>::type
            result_list = map_list_of
                ("method",  &parse_request_line_result_t::method)
                ("uri",     &parse_request_line_result_t::uri)
                ("version", &parse_request_line_result_t::version)
            ;

        IronBee::Operator::create(
            pool,
            "parseRequestLine",
            capabilities,
            bind<IronBee::Operator::operator_instance_t>(
                generator<parse_request_line_result_t>,
                &parse_request_line,
                boost::cref(result_list),
                _2
            )
        ).register_with(module.engine());
    }

    {
        static const result_list_type<parse_response_line_result_t>::type
            result_list = map_list_of
                ("version", &parse_response_line_result_t::version)
                ("status",  &parse_response_line_result_t::status)
                ("message", &parse_response_line_result_t::message)
            ;

        IronBee::Operator::create(
            pool,
            "parseResponseLine",
            capabilities,
            bind<IronBee::Operator::operator_instance_t>(
                generator<parse_response_line_result_t>,
                &parse_response_line,
                boost::cref(result_list),
                _2
            )
        ).register_with(module.engine());
    }

    {
        static const result_list_type<parse_authority_result_t>::type
            result_list = map_list_of
                ("username", &parse_authority_result_t::username)
                ("password", &parse_authority_result_t::password)
                ("host",     &parse_authority_result_t::host)
                ("port",     &parse_authority_result_t::port)
            ;

        IronBee::Operator::create(
            pool,
            "parseAuthority",
            capabilities,
            bind<IronBee::Operator::operator_instance_t>(
                generator<parse_authority_result_t>,
                &parse_authority,
                boost::cref(result_list),
                _2
            )
        ).register_with(module.engine());
    }

    {
        static const result_list_type<parse_path_result_t>::type
            result_list = map_list_of
                ("directory", &parse_path_result_t::directory)
                ("file",      &parse_path_result_t::file)
                ("base",      &parse_path_result_t::base)
                ("extension", &parse_path_result_t::extension)
            ;

        IronBee::Operator::create(
            pool,
            "parsePath",
            capabilities,
            bind<IronBee::Operator::operator_instance_t>(
                parse_path_generator,
                boost::cref(result_list),
                _2
            )
        ).register_with(module.engine());
    }
}

}
