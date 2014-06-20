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
 * @brief IronBee --- Header Order Module.
 *
 * This modules tracks header presence and order, exposing that information
 * in a var.
 *
 * By default, the module tracks the headers listed at
 * @ref c_default_request_config and @ref c_default_response_config
 * abbreviating each with a camel cased abbreviation.  The user can define a
 * different list via directives on a per-context basis.
 *
 * The result is stored at the REQUEST_HEADER and RESPONSE_HEADER phases in
 * the vars named by @ref c_request_var and @ref c_response_var.
 *
 * Case in header keys is ignored.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <ironbeepp/all.hpp>

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/bind.hpp>

using boost::bind;
using namespace std;
using namespace IronBee;

namespace {

// CONFIGURATION

//! Default configuration for request headers.
static const char* c_default_request_config =
    "H=Host "
    "U=User-Agent "
    "A=Accept "
    "E=Accept-Encoding "
    "L=Accept-Language "
    "N=Transfer-Encoding "
    "N=TE "
    "P=Pragma "
    "C=Cache-Control "
    "O=Cookie "
    "T=Content-Type "
    "L=Content-Length "
    "I=Connection "
    "R=Referer "
    "G=Range "
    ;

//! Default configuration for response headers.
static const char* c_default_response_config =
    "S=Server "
    "A=Location "
    "N=Transfer-Encoding "
    "N=TE "
    "D=Date "
    "M=Last-Modified "
    "C=Cache-Control "
    "O=Set-Cookie "
    "T=Content-Type "
    "L=Content-Length "
    "E=Content-Encoding "
    "L=Content-Language "
    "I=Connection "
    "X=Expires "
    "V=Via "
    "Y=Vary "
    "R=Trailer "
    ;

//! Var to store request header order in.
static const char* c_request_var = "REQUEST_HEADER_ORDER";
//! Var to store response header order in.
static const char* c_response_var = "RESPONSE_HEADER_ORDER";

//! Directive to configure request header order.
static const char* c_request_directive = "HeaderOrderRequest";
//! Directive to configure response header order.
static const char* c_response_directive = "HeaderOrderResponse";

// END CONFIGURATION

//! Map of header key to abbreviation.  Keys must be lowercase.
typedef map<string, string> header_map_t;

//! Per context data.
struct PerContext
{
    //! Map of header key to abbreviation for request headers.
    header_map_t request;

    //! Map of header key to abbreviation for response headers.
    header_map_t response;
};

/**
 * Configure a header map.
 *
 * @param[in] header_map Header map to configure.
 * @param[in] config     Configuration string.
 **/
void configure_header_map(
    header_map_t& header_map,
    const char*   config
);

//! Module delegate.
class Delegate :
    public ModuleDelegate
{
public:
    //! Constructor.
    explicit
    Delegate(Module module);

private:
    /**
     * Handle @ref c_request_directive and @ref c_response_directive.
     *
     * @param[in] request True if request, false if response.
     * @param[in] cp      Configuration parser.
     * @param[in] name    Name of directive.
     * @param[in] config  Configuration string.
     * @throws einval on failure.
     **/
    void order_directive(
        bool                request,
        ConfigurationParser cp,
        const char*         name,
        const char*         config
    );

    /**
     * Handle REQUEST_HEADER and RESPONSE_HEADER phase.
     *
     * @param[in] tx Current transaction.
     * @param[in] event Which event fired for.
     **/
    void handle_header_event(
        Transaction           tx,
        Engine::state_event_e event
    ) const;

    //! Request header order var.  @sa c_request_var.
    VarSource m_request_var;
    //! Response header order var.  @sa c_response_var.
    VarSource m_response_var;
};

} // Anonymous namespace

IBPP_BOOTSTRAP_MODULE_DELEGATE("header_order", Delegate)

// Implementation

// PerContext

// Keep doxygen happy.
namespace {

void configure_header_map(
    header_map_t& header_map,
    const char*   config
)
{
    header_map.clear();

    list<string> parts;
    boost::algorithm::split(parts, config, boost::is_any_of(" \t\r\n"));

    BOOST_FOREACH(const string& part, parts) {
        if (part.empty()) {
            continue;
        }
        size_t equal_pos = part.find_first_of('=');
        if (equal_pos == string::npos) {
            BOOST_THROW_EXCEPTION(
                einval() << errinfo_what(
                    part + " does not match key=abbreviation."
                )
            );
        }

        string abbrev = part.substr(0, equal_pos);
        string key = part.substr(equal_pos + 1);

        transform(key.begin(), key.end(), key.begin(), ::tolower);

        header_map.insert(header_map_t::value_type(key, abbrev));
    }
}

Delegate::Delegate(Module module) :
    ModuleDelegate(module)
{
    assert(module);

    Engine engine = module.engine();

    PerContext base;
    configure_header_map(base.request, c_default_request_config);
    configure_header_map(base.response, c_default_response_config);

    module.set_configuration_data<PerContext>(base);

    engine.register_hooks()
        .request_header_finished(
            bind(&Delegate::handle_header_event, this, _2, _3)
        )
        .response_header_finished(
            bind(&Delegate::handle_header_event, this, _2, _3)
        )
        ;

    engine.register_configuration_directives()
        .param1(
            c_request_directive,
            bind(&Delegate::order_directive, this, true, _1, _2, _3)
        )
        .param1(
            c_response_directive,
            bind(&Delegate::order_directive, this, false, _1, _2, _3)
        )
        ;

    m_request_var =
        VarSource::register_(engine.var_config(), c_request_var);
    m_response_var =
        VarSource::register_(engine.var_config(), c_response_var);
}

void Delegate::order_directive(
    bool                request,
    ConfigurationParser cp,
    const char*         name,
    const char*         config
)
{
    assert(cp);
    assert(config);

    PerContext& per_context =
        module().configuration_data<PerContext>(cp.current_context());
    header_map_t* header_map =
        request ? &per_context.request : &per_context.response;

    configure_header_map(*header_map, config);
}

void Delegate::handle_header_event(
    Transaction           tx,
    Engine::state_event_e event
) const
{
    assert(tx);

    PerContext& per_context =
        module().configuration_data<PerContext>(tx.context());

    const header_map_t* header_map;
    VarSource var_source;
    ConstParsedHeader header;

    if (event == Engine::request_header_finished) {
        header_map = &per_context.request;
        var_source = m_request_var;
        header = tx.request_header();
    }
    else if (event == Engine::response_header_finished) {
        header_map = &per_context.response;
        var_source = m_response_var;
        header = tx.response_header();
    }
    else {
        BOOST_THROW_EXCEPTION(
            eother() << errinfo_what(
                "Insanity: Handle header event handler called"
                " for non-handle header event."
            )
        );
    }

    string result;
    string key;
    header_map_t::const_iterator key_i;
    while (header) {
        key = header.name().to_s();
        transform(key.begin(), key.end(), key.begin(), ::tolower);

        key_i = header_map->find(key);
        if (key_i != header_map->end()) {
            result += key_i->second;
        }

        header = header.next();
    }

    var_source.set(
        tx.var_store(),
        Field::create_byte_string(
            tx.memory_manager(),
            "", 0,
            ByteString::create(
                tx.memory_manager(),
                result.data(), result.length()
            )
        )
    );
}

}
