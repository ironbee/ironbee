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
 * To add a new generator:
 *   -# Write your generator.  This should be a functional that takes a
 *      @c input_p& as a single argument, fills that argument with a new
 *      input, and returns true.  If the input can not be produced, it should
 *      return false.
 *   -# Write a factory for your generator.  This should be a functin(al) that
 *      takes a single string argument (the second half of the component) and
 *      returns a generator.
 *   -# Add documentation to help() below.
 *   -# Add your factory to the @c generator_factory_map at the top of main.
 *
 * To add a new consumer: Follow the directions above for generators, except
 * that consumers take a @c const @c input_p& and return true if it is able
 * to consume the input.  It should also be added to the
 * @c consumer_factory_map instead of the @c generator_factory_map.
 *
 * To add a new modifier: Follow the direcitons above for generators.
 * modifiers take a non-const @c input_p& as input and return true if
 * processing of that input should continue.  Modifiers will be passed a
 * singular (NULL) input once the generator is complete.  This singular
 * input can be used to detect end-of-input conditions.  Modifiers that are
 * not concerned with end-of-input conditions should immediately return true
 * when passed a singular input.  Modifiers may alter the input passed in or
 * even change it to point to a new input.  Modifiers should be added to
 * @c modifier_factory_map.
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
#include "htp_generator.hpp"

#include "ironbee_consumer.hpp"
#include "pb_consumer.hpp"

#include "connection_modifiers.hpp"
#include "parse_modifier.hpp"
#include "unparse_modifier.hpp"
#include "aggregate_modifier.hpp"

// Consumer and Modifier
#include "view.hpp"

#include <boost/function.hpp>
#include <boost/filesystem.hpp>
#include <boost/foreach.hpp>
#include <boost/make_shared.hpp>
#include <boost/bind.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/call_traits.hpp>
#include <boost/lexical_cast.hpp>

#include <string>

using namespace std;
using namespace IronBee::CLIPP;
using boost::bind;

using Input::input_p;
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

/**
 * @name Constructor Helpers
 *
 * This functions can be used with bind to easily construct components.
 **/
///@{

//! Generic generator constructor.
template <typename T>
input_generator_t construct_generator(const string& arg)
{
    return T(arg);
}

//! Generic consumer constructor.
template <typename T>
input_consumer_t construct_consumer(const string& arg)
{
    return T(arg);
}

//! Generic modifier constructor.  Converts @a arg to @a ArgType.
template <typename T, typename ArgType>
input_modifier_t construct_modifier(const string& arg)
{
    return T(boost::lexical_cast<ArgType>(arg));
}

//! Generic modifier constructor.
template <typename T>
input_modifier_t construct_modifier(const string& arg)
{
    return T(arg);
}

//! Generic modifier constructor for no-args.  Ignores @a arg.
template <typename T>
input_modifier_t construct_argless_modifier(const string& arg)
{
    return T();
}

///@}

/**
 * @name Specific component constructors.
 *
 * These routines construct components.  They are used when more complex
 * behavior (interpretation of @a arg) is needed than the generic constructors
 * above.
 **/

//! Construct raw generator, interpreting @a arg as @e request,response.
input_generator_t init_raw_generator(const string& arg);

//! Construct aggregate modifier.  An empty @a arg is 0, otherwise integer.
input_modifier_t init_aggregate_modifier(const string& arg);

///@}

/**
 * Split @a src into substrings separated by @a c.
 *
 * @param[in] src String to split.
 * @param[in] c   Character to split on.
 * @returns Vector of substrings.
 **/
vector<string> split_on_char(const string& src, char c);

/**
 * Display help.
 *
 * Component writers: Add your component below.
 **/
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
    "  -c <path> -- Load <path> as CLIPP configuration.\n"
    "\n"
    "Generators:\n"
    "  pb:<path>       -- Read <path> as protobuf.\n"
    "  modsec:<path>   -- Read <path> as modsec audit log.\n"
    "                     One transaction per connection.\n"
    "  raw:<in>,<out>  -- Read <in>,<out> as raw data in and out.\n"
    "                     Single transaction and connection.\n"
    "  apache:<path>   -- Read <path> as apache NCSA format.\n"
    "  suricata:<path> -- Read <path> as suricata format.\n"
    "  htp:<path>      -- Read <path> as libHTP test format.\n"
    "\n"
    "Consumers:\n"
    "  ironbee:<path> -- Internal IronBee using <path> as configuration.\n"
    "  writepb:<path> -- Output to protobuf file at <path>.\n"
    "  view           -- Output to stdout for human consumption.\n"
    "  view:id        -- Output IDs to stdout for human consumption.\n"
    "  view:summary   -- Output summary to stdout for human consumption.\n"
    "\n"
    "Modifiers:\n"
    "  @view                   -- Output to stdout for human consumption.\n"
    "  @view:id                -- Output IDs to stdout for human\n"
    "                             consumption.\n"
    "  @view:summary           -- Output summary to stdout for human\n"
    "                             consumption.\n"
    "  @set_local_ip:<ip>      -- Change local IP to <ip>.\n"
    "  @set_local_port:<port>  -- Change local port to <port>.\n"
    "  @set_remote_ip:<ip>     -- Change remote IP to <ip>.\n"
    "  @set_remote_port:<port> -- Change remote port to <port>.\n"
    "  @parse                  -- Parse connection data events.\n"
    "  @unparse                -- Unparse parsed events.\n"
    "  @aggregate              -- Aggregate all transactions into a single\n"
    "                             connection.\n"
    "  @aggregate:<n>          -- Aggregate transactions into a connections\n"
    "                             of at least <n> transactions.\n"
    "  @aggregate:uniform:min,max -- \n"
    "    Aggregate transactions into a connections of <min> to <max>\n"
    "    transactions chosen uniformly at random.\n"
    "  @aggregate:binomial:t,p -- \n"
    "    Aggregate transactions into a connections of n transactions\n"
    "    chosen at random from a binomial distribution of <t> trials with\n"
    "    <p> chance of success.\n"
    ;
}

/**
 * @name Helper methods for main().
 **/
///@{

/**
 * Combine @a generator and @a modifier into a single generator.
 *
 * Returns a generator that calls @a generator, passes the result to
 * @a modifier, and then provides the result as its own.  It will only call
 * @a modifier if @a generator returns true.
 *
 * @param[in] generator
 * @param[in] modifier
 * @returns Composition of @a modifier and @a generator.
 **/
input_generator_t modify_generator(
    input_generator_t generator,
    input_modifier_t  modifier
);

/**
 * Constructs a component from a parsed representation.
 *
 * @tparam ResultType Type of component to construct.
 * @tparam MapType Type of @a map.
 * @param[in] component Parsed representation of component.
 * @param[in] map       Generator map for this type of component.
 * @returns Component.
 * @throw runtime_error if invalid component.
 **/
template <typename ResultType, typename MapType>
ResultType
construct_component(const component_t& component, const MapType& map);

///@}

/**
 * Main
 *
 * Interprets arguments, constructs the actual chains, and executes them.
 *
 * Component writers: Add your component to the generator maps at the top of
 * this function.
 **/
int main(int argc, char** argv)
{
    if (argc == 1) {
        help();
        return 1;
    }

    list<string> args;

    // Declare generators.
    generator_factory_map_t generator_factory_map;
    generator_factory_map["modsec"]   =
        construct_generator<ModSecAuditLogGenerator>;
    generator_factory_map["raw"]      = init_raw_generator;
    generator_factory_map["pb"]       = construct_generator<PBGenerator>;
    generator_factory_map["apache"]   = construct_generator<ApacheGenerator>;
    generator_factory_map["suricata"] =
        construct_generator<SuricataGenerator>;
    generator_factory_map["htp"]      = construct_generator<HTPGenerator>;

    // Declare consumers.
    consumer_factory_map_t consumer_factory_map;
    consumer_factory_map["ironbee"] = construct_consumer<IronBeeConsumer>;
    consumer_factory_map["writepb"] = construct_consumer<PBConsumer>;
    consumer_factory_map["view"]    = construct_consumer<ViewConsumer>;

    // Declare modifiers.
    modifier_factory_map_t modifier_factory_map;
    modifier_factory_map["view"] = construct_modifier<ViewModifier>;
    modifier_factory_map["set_local_ip"] =
        construct_modifier<SetLocalIPModifier>;
    modifier_factory_map["set_local_port"] =
        construct_modifier<SetLocalPortModifier, uint32_t>;
    modifier_factory_map["set_remote_ip"] =
        construct_modifier<SetRemoteIPModifier>;
    modifier_factory_map["set_remote_port"] =
        construct_modifier<SetRemotePortModifier, uint32_t>;
    modifier_factory_map["parse"] = construct_argless_modifier<ParseModifier>;
    modifier_factory_map["unparse"] =
        construct_argless_modifier<UnparseModifier>;
    modifier_factory_map["aggregate"] = init_aggregate_modifier;

    // Convert argv to args.
    for (int i = 1; i < argc; ++i) {
        args.push_back(argv[i]);
    }

    list<chain_t> all_chains;
    chain_vec_t   chains;
    // Parse flags.
    while (args.front() == "-c") {
        args.pop_front();
        if (args.empty()) {
            cerr << "-c requires an argument." << endl;
            help();
            return 1;
        }
        string path = args.front();
        args.pop_front();

        try {
            chains = ConfigurationParser::parse_file(path);
        }
        catch (const exception& e) {
            cerr << "Error parsing configuration file " << path
                 << ": " << e.what() << endl;
            return 1;
        }

        copy(
            chains.begin(), chains.end(),
            back_inserter(all_chains)
        );
    }

    // Convert argv to configuration.
    // In the future, configuration can also be loaded from files.
    string configuration = boost::algorithm::join(args, " ");

    try {
        chains = ConfigurationParser::parse_string(configuration);
    }
    catch (const exception& e) {
        cerr << "Error parsing configuration: " << e.what() << endl;
        return 1;
    }
    copy(
        chains.begin(), chains.end(),
        back_inserter(all_chains)
    );

    // Basic validation.
    if (all_chains.size() < 2) {
        cerr << "Need at least a generator and a consumer." << endl;
        help();
        return 1;
    }
    // Last component must be consumer.
    input_consumer_t consumer;
    chain_t consumer_chain = all_chains.back();
    all_chains.pop_back();
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
    BOOST_FOREACH(chain_t& chain, all_chains) {
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

        // Append consumer modifiers
        copy(
            consumer_chain.modifiers.begin(), consumer_chain.modifiers.end(),
            back_inserter(chain.modifiers)
        );

        // Generate modifiers.
        typedef pair<string, input_modifier_t> modifier_info_t;
        list<modifier_info_t> modifiers;
        BOOST_FOREACH(
            const component_t& modifier_component,
            chain.modifiers
        ) {
            modifiers.push_back(modifier_info_t(
                modifier_component.name,
                input_modifier_t()
            ));
            modifier_info_t& modifier_info = modifiers.back();
            try {
                modifier_info.second = construct_component<input_modifier_t>(
                    modifier_component,
                    modifier_factory_map
                );
            }
            catch (const exception& e) {
                cerr << "Error constructing modifier "
                     << modifier_component.name << ": " << e.what() << endl;
                return 1;
            }
        }

        // Process inputs.
        input_p input;
        bool generator_continue = true;
        bool consumer_continue  = true;
        while (generator_continue && consumer_continue) {
            // Make new input for each run.  Extra allocations but avoids
            // some pitfalls.
            input = boost::make_shared<Input::Input>();

            try {
                generator_continue = generator(input);
            }
            catch (const exception& e) {
                cerr << "Error generating input: " << e.what() << endl;
                continue;
            }

            if (generator_continue && ! input) {
                cerr << "Generator said it provided input, but didn't."
                     << endl;
                continue;
            }

            if (! generator_continue) {
                // Only stop if the singular input reaches the consumer.
                input.reset();
            }

            bool modifier_continue = true;
            BOOST_FOREACH(const modifier_info_t& modifier_info, modifiers) {
                try {
                    modifier_continue = modifier_info.second(input);
                }
                catch (const exception& e) {
                    cerr << "Error applying modifier "
                         << modifier_info.first << ": " << e.what()
                         << endl;
                    modifier_continue = false;
                    break;
                }
                // If pushing through a singular input, apply to all
                // modifier.
                if (input && ! modifier_continue) {
                    break;
                }
            }
            if (! modifier_continue) {
                continue;
            }

            if (! input && ! generator_continue) {
                // Chain complete; leave loop.
                break;
            }

            if (! input) {
                cerr << "Input lost during modification." << endl;
                continue;
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

input_generator_t init_raw_generator(const string& arg)
{
    vector<string> subargs = split_on_char(arg, ',');
    if (subargs.size() != 2) {
        throw runtime_error("Raw inputs must be _request_,_response_.");
    }

    return RawGenerator(subargs[0], subargs[1]);
}

input_modifier_t init_aggregate_modifier(const string& arg)
{
    if (arg.empty()) {
        return AggregateModifier();
    }
    else {
        vector<string> subargs = split_on_char(arg, ':');
        if (subargs.size() == 1) {
            return construct_modifier<AggregateModifier, size_t>(subargs[0]);
        }
        else if (subargs.size() == 2) {
            vector<string> subsubargs = split_on_char(subargs[1], ',');
            if (subsubargs.size() != 2) {
                throw runtime_error("Error parsing aggregate distribution.");
            }
            if (subargs[0] == "uniform") {
                return AggregateModifier::uniform(
                    boost::lexical_cast<unsigned int>(subsubargs[0]),
                    boost::lexical_cast<unsigned int>(subsubargs[1])
                );
            }
            else if (subargs[0] == "binomial") {
                return AggregateModifier::binomial(
                    boost::lexical_cast<unsigned int>(subsubargs[0]),
                    boost::lexical_cast<double>(subsubargs[1])
                );
            }
            else {
                throw runtime_error(
                    "Unknown distribution: " +
                    subargs[0]
                );
            }
        }
        else {
            throw runtime_error("Error parsing aggregate arguments.");
        }
    }
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
