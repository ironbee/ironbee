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
 * @brief IronBee &mdash; CLIPP
 *
 * A CLI for IronBee
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include "input.hpp"
#include "modsec_audit_log_generator.hpp"
#include "raw_generator.hpp"

#include <ironbeepp/all.hpp>

#include <boost/filesystem.hpp>

#include <string>

using namespace std;
using namespace IronBee::CLIPP;

using IronBee::CLIPP::input_t;
using IronBee::CLIPP::input_generator_t;
using IronBee::CLIPP::buffer_t;

//! A producer of input generators.
typedef boost::function<input_generator_t(const string&)> generator_factory_t;

//! A map of command line argument to factory.
typedef map<string,generator_factory_t> generator_factory_map_t;

input_generator_t init_modsec_generator(const string& arg);
input_generator_t init_raw_generator(const string& arg);

bool on_error(const string& message);

void load_configuration(IronBee::Engine engine, const std::string& path);
IronBee::Connection open_connection(
    IronBee::Engine engine,
    const input_t& input
);
void data_in(IronBee::Connection connection, buffer_t request);
void data_out(IronBee::Connection connection, buffer_t response);
void close_connection(IronBee::Connection connection);

void help()
{
    cerr <<
    "Usage: clipp <component>...\n"
    "<component> := <name>:<parameters>\n"
    "\n"
    "Generator components produce inputs.\n"
    "Consumer components consume inputs.\n"
    "Consumer must be unique (and come last).\n"
    "Generators are processed in order and fed to consumer.\n"
    "\n"
    "Generators:\n"
    "  modsec:<path> -- Read <path> as modsec audit log.\n"
    "                   One transaction per connection.\n"
    "  raw:<in>,<out> -- Read <in>,<out> as raw data in and out.\n"
    "                    Single transaction and connection.\n"
    "\n"
    "Consumers:\n"
    "  ironbee:<path> -- Internal IronBee using <path> as configuration.\n"
    ;
}

int main(int argc, char** argv)
{
    if (argc == 1) {
        help();
        return 1;
    }

    // Declare input types.
    generator_factory_map_t generator_factory_map;
    generator_factory_map["modsec"] = &init_modsec_generator;
    generator_factory_map["raw"]    = &init_raw_generator;

    // In the near future, consumers will be abstracted.  Currently, it's
    // special cased.

    // Convert argv into list of pairs of name, parameters.
    typedef pair<string,string> component_t;
    typedef list<component_t> components_t;
    components_t components;

    for (int i = 1; i < argc; ++i) {
        string s(argv[i]);
        size_t colon_i = s.find_first_of(':');
        if (colon_i == string::npos) {
            cerr << "Component " << s << " lacks :" << endl;
            help();
            return 1;
        }
        components.push_back(
            make_pair(
                s.substr(0, colon_i),
                s.substr(colon_i + 1, string::npos)
            )
        );
    }

    // Special case last component.
    if (components.back().first != "ironbee") {
        cerr << "Final component must be consumer. "
             << components.back().first << " is not a consumer." << endl;
        help();
        return 1;
    }
    string config_path = components.back().second;
    components.pop_back();

    // Generators can use significant memory, so delay instantiation.
    // But do validate that they exist.
    BOOST_FOREACH(const component_t& component, components) {
        generator_factory_map_t::const_iterator i =
            generator_factory_map.find(component.first);
        if (i == generator_factory_map.end()) {
            cerr << "Component " << component.first << " is not a generator."
                 << endl;
            help();
            return 1;
        }
    }

    // Initialize IronBee.
    IronBee::initialize();
    IronBee::ServerValue server_value(__FILE__, "clipp");
    IronBee::Engine engine = IronBee::Engine::create(server_value.get());

    try {
        load_configuration(engine, config_path);
    }
    catch (IronBee::error) {
        cerr << "Error loading configuration.  See log." << endl;
        return 1;
    }
    catch (const exception& e) {
        cerr << "Error loading configuration: " << e.what() << endl;
    }

    // Loop through components, generating and processing input generators
    // as needed to limit the scope of each input generator.  As input
    // generators can make use of significant memory, it is good to only have
    // one around at a time.
    BOOST_FOREACH(const component_t& component, components) {
        input_generator_t generator;
        try {
            generator_factory_map_t::iterator i =
                generator_factory_map.find(component.first);
            if (i != generator_factory_map.end()) {
                generator = i->second(component.second);
            }
            else {
                cerr << "Insanity error: Validated component is invalid."

                     << " Please report as bug."
                     << endl;
                return 1;
            }
        }
        catch (const exception& e) {
            cerr << "Error initializing "
                 << component.first << ":" << component.second << ".  "
                 << "Message = " << e.what()
                 << endl;
           return 1;
         }

        // Process inputs.
        input_t input;
        while (generator(input)) {
            IronBee::Connection connection = open_connection(engine, input);
            BOOST_FOREACH(
                const input_t::transaction_t& transaction,
                input.transactions
            ) {
                data_in(connection, transaction.request);
                data_out(connection, transaction.response);
            }
            close_connection(connection);
        }
    }

    engine.destroy();
    IronBee::shutdown();
    return 0;
}

input_generator_t init_modsec_generator(const string& str)
{
    return ModSecAuditLogGenerator(str, on_error);
}

input_generator_t init_raw_generator(const string& arg)
{
    size_t comma_i = arg.find_first_of(',');
    if (comma_i == string::npos) {
        throw runtime_error("Raw inputs must be _request_,_response_.");
    }

    return RawGenerator(
        arg.substr(0, comma_i),
        arg.substr(comma_i+1)
    );
}

bool on_error(const string& message)
{
    cerr << "ERROR: " << message << endl;
    return true;
}

void load_configuration(IronBee::Engine engine, const std::string& path)
{
    engine.notify().configuration_started();

    IronBee::ConfigurationParser parser
        = IronBee::ConfigurationParser::create(engine);

    parser.parse_file(path);

    parser.destroy();
    engine.notify().configuration_finished();
}

IronBee::Connection open_connection(
    IronBee::Engine engine,
    const input_t& input
)
{
    using namespace boost;

    IronBee::Connection conn
        = IronBee::Connection::create(engine);

    char* local_ip = strndup(
        input.local_ip.data,
        input.local_ip.length
    );
    conn.memory_pool().register_cleanup(bind(free,local_ip));
    char* remote_ip = strndup(
        input.remote_ip.data,
        input.remote_ip.length
    );
    conn.memory_pool().register_cleanup(bind(free,remote_ip));


    conn.set_local_ip_string(local_ip);
    conn.set_local_port(input.local_port);
    conn.set_remote_ip_string(remote_ip);
    conn.set_remote_port(input.remote_port);

    conn.engine().notify().connection_opened(conn);

    return conn;
}

void data_in(IronBee::Connection connection, buffer_t request)
{
    // Copy because IronBee needs mutable input.
    IronBee::ConnectionData data = IronBee::ConnectionData::create(
        connection,
        request.data,
        request.length
    );

    connection.engine().notify().connection_data_in(data);
}

void data_out(IronBee::Connection connection, buffer_t response)
{
    // Copy because IronBee needs mutable input.
    IronBee::ConnectionData data = IronBee::ConnectionData::create(
        connection,
        response.data,
        response.length
    );

    connection.engine().notify().connection_data_in(data);
}

void close_connection(IronBee::Connection connection)
{
    connection.engine().notify().connection_closed(connection);
}
