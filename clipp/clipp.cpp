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
 * A CLI for IronBee.
 *
 * clipp is a framework for handling "inputs", which represent a connection
 * and sequence of transactions within that connection.  clipp attaches
 * input generators to an input consumer.  For example, modsec audit logs,
 * to an internal IronBee engine.  The primary purpose of clipp is to be
 * extendible, allowing additional generators and consumers to be written.
 *
 * In the near future, clipp will also support transformations which take
 * inputs and produce inputs.  Transformations can be used to filter, modify,
 * or aggregate inputs.
 *
 * To add a new generator:
 *   -# Write your generator.  This should be a functional that takes a
 *      @c input_t& as a single argument, fills that argument with a new
 *      input, and returns true.  If the input can not be produced, it should
 *      return false.
 *   -# Write a factory for your generator.  This should be a functin(al) that
 *      takes a single string argument (the second half of the component) and
 *      returns a generator.
 *   -# Add documentation to help() below.
 *   -# Add your factory to the @c generator_factory_map at the top of main.
 *
 * To add a new consumer: Follow the directions above for generators, except
 * that consumers take a @c const @c input_t& and return true if it is able
 * to consume the input.  It should also be added to the
 * @c consumer_factory_map instead of the @c generator_factory_map.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include "input.hpp"
 #include "configuration_parser.hpp"

#include "modsec_audit_log_generator.hpp"
#include "raw_generator.hpp"
#include "pb_generator.hpp"
#include "apache_generator.hpp"
#include "suricata_generator.hpp"

#include "ironbee_consumer.hpp"
#include "pb_consumer.hpp"
#include "view_consumer.hpp"

#include "connection_modifiers.hpp"

#include <boost/function.hpp>
#include <boost/filesystem.hpp>
#include <boost/foreach.hpp>
#include <boost/make_shared.hpp>
#include <boost/bind.hpp>
#include <boost/algorithm/string/join.hpp>

#include <string>

using namespace std;
using namespace IronBee::CLIPP;
using boost::bind;

using ConfigurationParser::component_t;
using ConfigurationParser::component_vec_t;
using ConfigurationParser::chain_t;
using ConfigurationParser::chain_vec_t;

/**
 * A generator of inputs.
 *
 * Should be a function that takes an input_p as an output argument.  If
 * input is available it should fill its argument and return true.  If no
 * more input is available, it should return false.
 *
 * The input_p can be changed but it is guaranteed to be an already allocated
 * input_t which can be reused.
 *
 * Errors should be reported via exceptions.  Exceptions will not halt
 * processing, so generators should arrange to do nothing and return false if
 * they are also unable to continue.
 **/
typedef boost::function<bool(input_p&)> input_generator_t;

/**
 * A consumer of inputs.
 *
 * Should take a const input_p as an input argument.  Should return true if
 * the input was accepted and false if it can not accept additional inputs.
 *
 * Exceptions are as per generators, i.e., use to report errors; does not
 * halt processing.
 **/
typedef boost::function<bool(const input_p&)> input_consumer_t;

/**
 * A modiifier of inputs.
 *
 * Should take a input_p as an in/out parameter.  The parameter is guaranteed
 * to point to an already allocated input_t.  That input_t can be modified or
 * the pointer can be changed to point to a new input_t.
 *
 * The return value should be true if the result is to be used and false if
 * the result is to be discard.
 *
 * Exception semantics is the same as generators and consumers.
 **/
typedef boost::function<bool(input_p&)> input_modifier_t;

//! A producer of input generators.
typedef boost::function<input_generator_t(const string&)> generator_factory_t;

//! A producer of input consumers.
typedef boost::function<input_consumer_t(const string&)> consumer_factory_t;

//! A producer of input modifiers.
typedef boost::function<input_modifier_t(const string&)> modifier_factory_t;

//! A map of command line argument to factory.
typedef map<string,generator_factory_t> generator_factory_map_t;

//! A map of command line argument to factory.
typedef map<string,consumer_factory_t> consumer_factory_map_t;

//! A map of command line argument to factory.
typedef map<string,modifier_factory_t> modifier_factory_map_t;

// Generators
input_generator_t init_modsec_generator(const string& arg);
input_generator_t init_raw_generator(const string& arg);
input_generator_t init_pb_generator(const string& arg);
input_generator_t init_apache_generator(const string& arg);
input_generator_t init_suricata_generator(const string& arg);

// Consumers
input_consumer_t init_ironbee_consumer(const string& arg);
input_consumer_t init_pb_consumer(const string& arg);
input_consumer_t init_view_consumer(const string& arg);

// Modifier
input_modifier_t init_set_local_ip_modifier(const string& arg);

bool on_error(const string& message);

vector<string> split_on_char(const string& src, char c);

void help()
{
    cerr <<
    "Usage: clipp [<flags>] <generator>... <consumer>\n"
    "<generator> := <component>\n"
    "<consumer>  := <component>\n"
    "<modifier>  := <component>\n"
    "<component> := <name>:<parameters>\n"
    "             | <name>\n"
    "             | <component> @<modifier>\n"
    "\n"
    "Generator components produce inputs.\n"
    "Consumer components consume inputs.\n"
    "Modifier consumer and produce inputs:\n"
    "  Filters only let some inputs through.\n"
    "  Transforms modify aspects of inputs.\n"
    "  Aggregators convert multiple inputs into a single input.\n"
    "\n"
    "Consumer must be unique (and come last).\n"
    "Generators are processed in order and fed to consumer.\n"
    "Each input passes through the modifiers of its generator and the\n"
    "modifiers of the consumer.\n"
    "\n"
    "Flags:\n"
    " --verbose,-v -- Output ID for each input.\n"
    "\n"
    "Generators:\n"
    "  pb:<path>       -- Read <path> as protobuf.\n"
    "  modsec:<path>   -- Read <path> as modsec audit log.\n"
    "                     One transaction per connection.\n"
    "  raw:<in>,<out>  -- Read <in>,<out> as raw data in and out.\n"
    "                     Single transaction and connection.\n"
    "  apache:<path>   -- Read <path> as apache NCSA format.\n"
    "  suricata:<path> -- Read <path> as suricata format.\n"
    "\n"
    "Consumers:\n"
    "  ironbee:<path> -- Internal IronBee using <path> as configuration.\n"
    "  writepb:<path> -- Output to protobuf file at <path>.\n"
    "  view:          -- Output to stdout for human consumption.\n"
    "\n"
    "Modifiers:\n"
    "  set_local_ip:<ip> -- Change local IP to <ip>.\n"
    ;
}

input_generator_t modify_generator(
    input_generator_t generator,
    input_modifier_t  modifier
);

template <typename ResultType, typename MapType>
ResultType
construct_component(const component_t& component, const MapType& map);

int main(int argc, char** argv)
{
    if (argc == 1) {
        help();
        return 1;
    }

    list<string> args;
    bool verbose = false;

    // Declare generators.
    generator_factory_map_t generator_factory_map;
    generator_factory_map["modsec"]   = &init_modsec_generator;
    generator_factory_map["raw"]      = &init_raw_generator;
    generator_factory_map["pb"]       = &init_pb_generator;
    generator_factory_map["apache"]   = &init_apache_generator;
    generator_factory_map["suricata"] = &init_suricata_generator;

    // Declare consumers.
    consumer_factory_map_t consumer_factory_map;
    consumer_factory_map["ironbee"] = &init_ironbee_consumer;
    consumer_factory_map["writepb"] = &init_pb_consumer;
    consumer_factory_map["view"]    = &init_view_consumer;

    // Declare modifiers.
    modifier_factory_map_t modifier_factory_map;
    modifier_factory_map["set_local_ip"] = &init_set_local_ip_modifier;

    // Convert argv to args.
    for (int i = 1; i < argc; ++i) {
        args.push_back(argv[i]);
    }

    // Parse flags.
    if (args.front() == "--verbose" || args.front() == "-v") {
        verbose = true;
        args.pop_front();
    }

    // Convert argv to configuration.
    // In the future, configuration can also be loaded from files.
    string configuration = boost::algorithm::join(args, " ");

    chain_vec_t chains;
    try {
        chains = ConfigurationParser::parse_string(configuration);
    }
    catch (const exception& e) {
        cerr << "Error parsing configuration: " << e.what() << endl;
        return 1;
    }

    // Last component must be consumer.
    input_consumer_t consumer;
    chain_t consumer_chain = chains.back();
    chains.pop_back();
    try {
        consumer = construct_component<input_consumer_t>(
            consumer_chain.base,
            consumer_factory_map
        );
    }
    catch (const exception& e) {
        cerr << "Error constructing consumer: " << e.what() << endl;
        return 1;
    }

    // Loop through components, generating and processing input generators
    // as needed to limit the scope of each input generator.  As input
    // generators can make use of significant memory, it is good to only have
    // one around at a time.
    BOOST_FOREACH(chain_t& chain, chains) {
        input_generator_t generator;
        try {
            generator = construct_component<input_generator_t>(
                chain.base,
                generator_factory_map
            );
        }
        catch (const exception& e) {
            cerr << "Error constructing generator " << chain.base.name
                 << ": " << e.what() << endl;
           return 1;
        }

        // Append consumer modifiers.
        copy(
            consumer_chain.modifiers.begin(), consumer_chain.modifiers.end(),
            back_inserter(chain.modifiers)
        );
        BOOST_FOREACH(
            const component_t& modifier_component,
            chain.modifiers
        ) {
            input_modifier_t modifier;
            try {
                modifier = construct_component<input_modifier_t>(
                    modifier_component,
                    modifier_factory_map
                );
            }
            catch (const exception& e) {
                cerr << "Error constructing modifier "
                     << modifier_component.name << ": " << e.what() << endl;
                return 1;
            }
            generator = modify_generator(generator, modifier);
        }

        // Process inputs.
        input_p input;
        bool generator_continue = true;
        bool consumer_continue  = true;
        while (generator_continue && consumer_continue) {
            if (! input) {
                input = boost::make_shared<input_t>();
            }

            try {
                generator_continue = generator(input);
            }
            catch (const exception& e) {
                cerr << "Error generating input: " << e.what() << endl;
                continue;
            }

            if (! generator_continue) {
                break;
            }

            if (! input) {
                cerr << "Generator said it provided input, but didn't."
                     << endl;
                continue;
            }
            if (verbose) {
                cout << input->id << endl;
            }

            try {
                consumer_continue = consumer(input);
            }
            catch (const exception& e) {
                cerr << "Error consuming input: " << e.what() << endl;
                continue;
            }
            if (! consumer_continue) {
                cerr << "Consumer refusing input." << endl;
            }
        }
    }

    return 0;
}

vector<string> split_on_char(const string& src, char c)
{
    size_t i = 0;
    vector<string> r;

    for (;;) {
        size_t j = src.find_first_of(c, i);
        if (j == string::npos) {
            r.push_back(src.substr(i));
            break;
        }

        r.push_back(src.substr(i, j-i));
        i = j+1;
    }

    return r;
}

input_generator_t init_modsec_generator(const string& str)
{
    return ModSecAuditLogGenerator(str, on_error);
}

input_generator_t init_raw_generator(const string& arg)
{
    vector<string> subargs = split_on_char(arg, ',');
    if (subargs.size() != 2) {
        throw runtime_error("Raw inputs must be _request_,_response_.");
    }

    return RawGenerator(subargs[0], subargs[1]);
}

input_generator_t init_pb_generator(const string& arg)
{
    return PBGenerator(arg);
}

input_generator_t init_apache_generator(const string& arg)
{
    return ApacheGenerator(arg);
}

input_generator_t init_suricata_generator(const string& arg)
{
    return SuricataGenerator(arg);
}

input_consumer_t init_ironbee_consumer(const string& arg)
{
    return IronBeeConsumer(arg);
}

input_consumer_t init_pb_consumer(const string& arg)
{
    return PBConsumer(arg);
}

input_consumer_t init_view_consumer(const string&)
{
    return ViewConsumer();
}

input_modifier_t init_set_local_ip_modifier(const string& arg)
{
    return SetLocalIpModifier(arg);
}

bool on_error(const string& message)
{
    cerr << "ERROR: " << message << endl;
    return true;
}

bool modify_generator_function(
    input_generator_t generator,
    input_modifier_t  modifier,
    input_p&          input
)
{
    if (generator(input)) {
        return modifier(input);
    }
    else {
        return false;
    }
}

input_generator_t modify_generator(
    input_generator_t generator,
    input_modifier_t  modifier
)
{
    return bind(modify_generator_function, generator, modifier, _1);
}

template <typename ResultType, typename MapType>
ResultType
construct_component(const component_t& component, const MapType& map)
{
    typename MapType::const_iterator i = map.find(component.name);
    if (i == map.end()) {
        throw runtime_error("Unknown component: " + component.name);
    }

    return i->second(component.arg);
}

component_t parse_component(const string& s)
{
    vector<string> split = split_on_char(s, ':');
    if (split.size() > 2) {
        throw runtime_error("Too many colons in: " + s);
    }

    component_t component;
    component.name = split[0];
    if (split.size() == 2) {
        component.arg = split[1];
    }

    return component;
}

chain_t parse_chain(const vector<string>& tokens)
{
    chain_t chain;
    chain.base = parse_component(tokens[0]);
    for (int i = 1; i < tokens.size(); ++i) {
        const string& token = tokens[i];
        if (token[0] != '@') {
            throw logic_error("Modifier does not begin with @.");
        }
        chain.modifiers.push_back(
            parse_component(token.substr(1))
        );
    }

    return chain;
}
