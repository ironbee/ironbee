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
 * @brief IronBee --- Response Module
 *
 * @author Sam Baskinger <sbaskinger@qualys.com>
 */

#include <ironbee/path.h>

#include <ironbeepp/action.hpp>
#include <ironbeepp/configuration_parser.hpp>
#include <ironbeepp/module.hpp>
#include <ironbeepp/module_delegate.hpp>
#include <ironbeepp/module_bootstrap.hpp>
#include <ironbeepp/memory_manager.hpp>
#include <ironbeepp/transformation.hpp>

#ifdef __clang__
#pragma clang diagnostic push
#if __has_warning("-Wc++11-extensions")
#pragma clang diagnostic ignored "-Wc++11-extensions"
#endif
#if __has_warning("-Wunused-local-typedef")
#pragma clang diagnostic ignored "-Wunused-local-typedef"
#endif
#endif
#include <boost/bind.hpp>
#include <boost/format.hpp>
#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/lex_lexertl.hpp>
#include <boost/spirit/include/phoenix_operator.hpp>
#include <boost/spirit/include/phoenix_statement.hpp>
#include <boost/spirit/include/phoenix_container.hpp>

#include <boost/iostreams/code_converter.hpp>
#include <boost/iostreams/device/mapped_file.hpp>
#include <boost/iostreams/operations.hpp>
#include <boost/iostreams/read.hpp>

#ifdef __clang__
#pragma clang diagnostic pop
#endif

#include <ios>

namespace {

using namespace IronBee;

//! Config structure for response data.
struct ResponseContextData {

};

//! Tx structure for response data.
struct ResponseTxData {

};

//! The response action.
class ResponseAction {
public:
    ResponseAction(IronBee::Engine engine);
    static Action::action_instance_t generate(
        MemoryManager mm,
        Context       ctx,
        const char *  arg
    );

    /**
     * Implement this function object.
     */
    void operator()(const ib_rule_exec_t * rule_exec);
    void addHeader(const std::string& name, const std::string& value);
    void addHeader(const std::vector<std::string>& headers);
    void setStatus(int status);
    void setFile(const std::string& file);

    //! Get the status.
    int status() const;
    const std::string& file() const;

private:
    IronBee::Engine m_engine;
    int m_status;
    std::string m_file;
    ib_parsed_headers_t *m_headers;
};

ResponseAction::ResponseAction(IronBee::Engine engine)
:
    m_engine(engine)
{
    IronBee::throw_if_error(
        ib_parsed_headers_create(&m_headers, engine.main_memory_mm().ib())
    );
}

void ResponseAction::addHeader(
    const std::string& name,
    const std::string& value
)
{
    ib_log_debug(
        m_engine.ib(),
        "Adding header %.*s=%.*s.",
        static_cast<int>(name.length()),
        name.data(),
        static_cast<int>(value.length()),
        value.data()
    );

    IronBee::throw_if_error(
        ib_parsed_headers_add(
            m_headers,
            name.data(),
            name.length(),
            value.data(),
            value.length()
        )
    );
}

void ResponseAction::addHeader(const std::vector<std::string>& headers)
{
    assert(headers.size() == 2 && "Header must be length 2.");
    addHeader(headers[0], headers[1]);
}

int ResponseAction::status() const
{
    return m_status;
}

void ResponseAction::setStatus(int status)
{
    ib_log_debug(
        m_engine.ib(),
        "Setting status to %d.",
        status
    );
    m_status = status;
}

const std::string& ResponseAction::file() const
{
    return m_file;
}

void ResponseAction::setFile(const std::string& file)
{
    ib_log_debug(
        m_engine.ib(),
        "Setting response file to %s.",
        file.c_str()
    );

    ib_status_t rc;
    const ib_cfgparser_t *ib_cp;

    rc = ib_engine_cfgparser_get(m_engine.ib(), &ib_cp);
    if (rc == IB_OK) {
        IronBee::ConstConfigurationParser cp(ib_cp);

        m_file = ib_util_relative_file(
            ib_engine_mm_config_get(m_engine.ib()),
            cp.current_file(),
            file.c_str()
        );

        ib_log_debug(
            m_engine.ib(),
            "Response file set to relative file %s.", m_file.c_str()
        );
    }
    else {
        ib_log_error(
            m_engine.ib(),
            "Failed to get cfg parser. "
            "Cannot set relative path of error page."
        );
        m_file = file;
    }
}

class ResponseModuleDelegate : public ModuleDelegate
{
public:
    ResponseModuleDelegate(Module m);
private:
}; // end class ResponseModuleDelegate

Action::action_instance_t ResponseAction::generate(
    MemoryManager mm,
    Context       ctx,
    const char *  arg
)
{
    // Build a response action that will be populated by the parser.
    ResponseAction r(ctx.engine());

    namespace qi = boost::spirit::qi;
    namespace phoenix = boost::phoenix;

    const char *first = arg;
    const char *last  = first + strlen(arg);

    /* Parse a single comment. */
    bool parse_success = qi::phrase_parse(
        // Range to parse.
        first, last,

        // Begin grammar.
        // Parse the status.
        qi::omit[qi::int_ [ phoenix::bind(&ResponseAction::setStatus, boost::ref(r), qi::_1)]]
        // Optionally parse the headers.
        >> -(
                qi::lit(",") >>
                (
                    qi::as< std::vector< std::string > >()
                    [
                        qi::as_string[+(qi::char_ - ':')] >>
                        qi::omit[qi::lit(":")]    >>
                        qi::as_string[+(qi::char_ - ',')]
                    ]
                    [
                        phoenix::bind(&ResponseAction::addHeader, boost::ref(r), qi::_1)
                    ] % qi::lit(",")
                )
        )
        // Optionally parse the file.
        >> -(
            qi::lit(",") >>
            qi::as_string[+qi::char_][
                phoenix::bind(&ResponseAction::setFile, boost::ref(r), qi::_1)
            ]
        )
        ,
        // End grammar.

        // Skip parser.
        qi::space
    );

    /* If
     * (1) we fully matched
     * (2) parsed successfully
     */
    if (first == last && parse_success) {
        ib_log_debug(
            ctx.engine().ib(),
            "Built response with status %d and content from %s.",
            r.status(),
            r.file().c_str()
        );
        return r;
    }

    BOOST_THROW_EXCEPTION(
        IronBee::einval()
            << IronBee::errinfo_what(
                str(boost::format("Failed to parse argument: %s") % arg)
            )
    );
}

void ResponseAction::operator()(const ib_rule_exec_t * rule_exec)
{
    assert(rule_exec != NULL);
    assert(rule_exec->tx != NULL);

    ib_status_t rc;

    IronBee::MemoryManager mm = IronBee::MemoryManager(rule_exec->tx->mm);

    IronBee::ByteString page_bs;
    boost::iostreams::mapped_file_source page;

    try
    {
        // Throws an error if the file does not exist.
        page.open(m_file);

        if (page.is_open()) {
            page_bs =
                IronBee::ByteString::create_alias(mm, page.data(), page.size());
        }
        else {
            page_bs = IronBee::ByteString::create_alias(mm, "", 0);
        }
    }
    catch (const std::ios_base::failure& e)
    {
        ib_log_error_tx(
            rule_exec->tx,
            "Failed to read error page file %s: %s", m_file.c_str(), e.what()
        );
        page_bs = IronBee::ByteString::create(mm, "", 0);
    }

    rc = ib_tx_response(
        rule_exec->tx,
        m_status,
        m_headers,
        page_bs.ib()
    );
    if (rc != IB_OK) {
        ib_log_debug_tx(rule_exec->tx, "Failed to send custom response.");
    }

    // Close out here because we want the file to stay mapped until
    // the response is delivered.
    if (page.is_open()) {
        page.close();
    }

}

ResponseModuleDelegate::ResponseModuleDelegate(Module m)
:
ModuleDelegate(m)
{
    ResponseContextData config;
    m.set_configuration_data(config);
    MemoryManager mm = m.engine().main_memory_mm();

    // Action response =
    Action::create(
        mm,
        "response",
        boost::bind(ResponseAction::generate, _1, _2, _3)
    ).register_with(m.engine());
}

} // Anonymous Namespace

IBPP_BOOTSTRAP_MODULE_DELEGATE("response", ResponseModuleDelegate);
