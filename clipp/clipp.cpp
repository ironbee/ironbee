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

#include "modsec_audit_log_generator.hpp"
#include "raw_generator.hpp"
#include "pb_generator.hpp"
#include "apache_generator.hpp"
#include "suricata_generator.hpp"

#include "ironbee_consumer.hpp"
#include "pb_consumer.hpp"
#include "view_consumer.hpp"

#include <boost/function.hpp>
#include <boost/filesystem.hpp>
#include <boost/foreach.hpp>

#include <string>

using namespace std;
using namespace IronBee::CLIPP;

using IronBee::CLIPP::input_t;
using IronBee::CLIPP::buffer_t;

/**
 * A generator of inputs.
 *
 * Should be a function that takes an input_p as an output argument.  If
 * input is available it should fill its argument and return true.  If no
 * more input is available, it should return false.
 *
 * Errors should be reported via exceptions.  Exceptions will not halt
 * processing, so generators should arrange to do nothing and return false if
 * they are also unable to continue.
 **/
typedef boost::function<bool(input_t&)> input_generator_t;

/**
 * A consumer of inputs.
 *
 * Should take a const input_t as an input argument.  Should return true if
 * the input was accepted and false if it can not accept additional inputs.
 *
 * Exceptions are as per generators, i.e., use to report errors; does not
 * halt processing.
 **/
typedef boost::function<bool(const input_t&)> input_consumer_t;

//! A producer of input generators.
typedef boost::function<input_generator_t(const string&)> generator_factory_t;

//! A producer of input consumers.
typedef boost::function<input_consumer_t(const string&)> consumer_factory_t;

//! A map of command line argument to factory.
typedef map<string,generator_factory_t> generator_factory_map_t;

//! A map of command line argument to factory.
typedef map<string,consumer_factory_t> consumer_factory_map_t;

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

bool on_error(const string& message);

void help()
{
    cerr <<
    "Usage: clipp [<flags>] <component>...\n"
    "<component> := <name>:<parameters>\n"
    "\n"
    "Generator components produce inputs.\n"
    "Consumer components consume inputs.\n"
    "Consumer must be unique (and come last).\n"
    "Generators are processed in order and fed to consumer.\n"
    "\n"
    "Flags:\n"
    " --verbose,-v -- Output ID for each input.\n"
    "\n"
    "Generators:\n"
    "  pb:<path>       -- Read <path> as protobuf.\n"
    "  modsec:<path>   -- Read <path> as modsec audit log.\n"
    "                    One transaction per connection.\n"
    "  raw:<in>,<out>  -- Read <in>,<out> as raw data in and out.\n"
    "                    Single transaction and connection.\n"
    "  apache:<path>   -- Read <path> as apache NCSA format.\n"
    "  suricata:<path> -- Read <path> as suricata format.\n"
    "\n"
    "Consumers:\n"
    "  ironbee:<path> -- Internal IronBee using <path> as configuration.\n"
    "  writepb:<path> -- Output to protobuf file at <path>.\n"
    "  view:          -- Output to stdout for human consumption.\n"
    ;
}

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

    // Convert argv to args.
    for (int i = 1; i < argc; ++i) {
        args.push_back(argv[i]);
    }

    // Parse flags.
    if (args.front() == "--verbose" || args.front() == "-v") {
        verbose = true;
        args.pop_front();
    }

    // Convert argv into list of pairs of name, parameters.
    typedef pair<string,string> component_t;
    typedef list<component_t> components_t;
    components_t components;

    BOOST_FOREACH(const string& arg, args) {
        size_t colon_i = arg.find_first_of(':');
        if (colon_i == string::npos) {
            cerr << "Component " << arg << " lacks :" << endl;
            help();
            return 1;
        }
        components.push_back(
            make_pair(
                arg.substr(0, colon_i),
                arg.substr(colon_i + 1, string::npos)
            )
        );
    }

    // Last component must be consumer.
    input_consumer_t consumer;
    {
        component_t consumer_component = components.back();
        components.pop_back();
        consumer_factory_map_t::const_iterator i =
            consumer_factory_map.find(consumer_component.first);
        if (i == consumer_factory_map.end()) {
            cerr << "Final component must be consumer. "
                 << components.back().first << " is not a consumer." << endl;
            help();
            return 1;
        }
        try {
            consumer = i->second(consumer_component.second);
        }
        catch (const exception& e) {
            cerr << "Error initializing consumer: " << e.what() << endl;
            return 1;
        }
    }

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
        bool generator_continue = true;
        bool consumer_continue  = true;
        while (generator_continue && consumer_continue) {
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

            if (verbose ) {
                cout << input.id << endl;
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

bool on_error(const string& message)
{
    cerr << "ERROR: " << message << endl;
    return true;
}
