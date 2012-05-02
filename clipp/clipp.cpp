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

#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>

#include <string>

using namespace std;
using namespace IronBee::CLIPP;

using IronBee::CLIPP::input_t;
using IronBee::CLIPP::input_generator_t;
using IronBee::CLIPP::buffer_t;

//! A producer of input generators.
typedef boost::function<input_generator_t(const string&)> input_factory_t;

//! A map of command line argument to factory.
typedef map<string,input_factory_t> input_factory_map_t;

input_generator_t init_audit_input(const string& arg);
input_generator_t init_raw_input(const string& arg);

bool on_error(const string& message);

void load_configuration(IronBee::Engine engine, const std::string& path);
IronBee::Connection open_connection(
    IronBee::Engine engine,
    const input_t& input
);
void data_in(IronBee::Connection connection, buffer_t request);
void data_out(IronBee::Connection connection, buffer_t response);
void close_connection(IronBee::Connection connection);

int main(int argc, char** argv)
{
    namespace po = boost::program_options;

    bool   show_help = false;
    string config_path;

    po::options_description desc(
        "All input options can be repeated.  Inputs will be processed in the "
        "order listed."
    );

    po::options_description general_desc("General:");
    general_desc.add_options()
        ("help", po::bool_switch(&show_help), "Output help message.")
        ("config,C", po::value<string>(&config_path),
            "IronBee config file.  REQUIRED"
        )
    ;

    po::options_description input_desc("Input Options:");
    input_desc.add_options()
        ("audit,A", po::value<vector<string> >(),
            "Mod Security Audit Log"
        )
        ("raw,R", po::value<vector<string> >(),
            "Raw inputs.  Use comma separated pair: request path,response "
            "path. Raw input will use bogus connection information."
        )
    ;
    desc.add(general_desc).add(input_desc);

    po::basic_parsed_options<char> options
        = po::parse_command_line(argc, argv, desc);

    po::variables_map vm;
    po::store(options, vm);
    po::notify(vm);

    if (show_help) {
        cerr << desc << endl;
        return 1;
    }

    if (config_path.empty()) {
        cerr << "Config required." << endl;
        cout << desc << endl;
        return 1;
    }

    // Declare input types.
    input_factory_map_t input_factory_map;
    input_factory_map["audit"] = &init_audit_input;
    input_factory_map["raw"]   = &init_raw_input;

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

    // Loop through the options, generating and processing input generators
    // as needed to limit the scope of each input generator.  As input
    // generators can make use of significant memory, it is good to only have
    // one around at a time.
    BOOST_FOREACH(const po::basic_option<char>& option, options.options) {
        input_generator_t generator;
        try {
            input_factory_map_t::iterator i =
                input_factory_map.find(option.string_key);
            if (i != input_factory_map.end()) {
                generator = i->second(option.value[0]);
            }
            else {
                continue;
            }
        }
        catch (const exception& e) {
            cerr << "Error initializing "
                 << option.string_key << " " << option.value[0] << ".  "
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

input_generator_t init_audit_input(const string& str)
{
    return ModSecAuditLogGenerator(str, on_error);
}

input_generator_t init_raw_input(const string& arg)
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
