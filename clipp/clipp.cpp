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
 * @brief IronBee --- CLIPP
 *
 * A CLI for IronBee.
 *
 * clipp is a framework for handling "inputs", which represent a connection
 * and sequence of transactions within that connection.  clipp attaches
 * input generators to an input consumer.  For example, modsec audit logs,
 * to an internal IronBee engine.  The primary purpose of clipp is to be
 * extendable, allowing additional generators and consumers to be written.
 *
 * To add a new generator:
 *   -# Write your generator.  This should be a functional that takes a
 *      @c input_p& as a single argument, fills that argument with a new
 *      input, and returns true.  If the input can not be produced, it should
 *      return false.
 *   -# Write a factory for your generator.  This should be a function(al) that
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
 * To add a new modifier: Follow the directions above for generators.
 * modifiers take a non-const @c input_p& as input and return true if
 * processing of that input should continue.  Modifiers will be passed a
 * singular (NULL) input once the generator is complete.  This singular
 * input can be used to detect end-of-input conditions.  Modifiers that are
 * not concerned with end-of-input conditions should immediately return true
 * when passed a singular input.  Modifiers may alter the input passed in or
 * even change it to point to a new input.  Modifiers should be added to
 * @c modifier_factory_map.
 *
 * All components can thrown special exceptions to change behavior.  See
 * control.hpp.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include "ironbee_config_auto.h"

#include <clipp/aggregate_modifier.hpp>
#include <clipp/apache_generator.hpp>
#include <clipp/configuration_parser.hpp>
#include <clipp/connection_modifiers.hpp>
#include <clipp/control.hpp>
#include <clipp/echo_generator.hpp>
#include <clipp/edit_modifier.hpp>
#include <clipp/fill_body_modifier.hpp>
#include <clipp/header_modifiers.hpp>
#include <clipp/htp_consumer.hpp>
#include <clipp/htp_generator.hpp>
#include <clipp/input.hpp>
#include <clipp/ironbee.hpp>
#include <clipp/limit_modifier.hpp>
#include <clipp/modsec_audit_log_generator.hpp>
#include <clipp/null_consumer.hpp>
#include <clipp/parse_modifier.hpp>
#include <clipp/pb_consumer.hpp>
#include <clipp/pb_generator.hpp>
#ifdef HAVE_NIDS
#include <clipp/pcap_generator.hpp>
#endif
#include <clipp/raw_consumer.hpp>
#include <clipp/raw_generator.hpp>
#include <clipp/select_modifier.hpp>
#include <clipp/split_modifier.hpp>
#include <clipp/suricata_generator.hpp>
#include <clipp/time_modifier.hpp>
#include <clipp/unparse_modifier.hpp>
#include <clipp/view.hpp>
#include <clipp/proxy.hpp>

#include <boost/algorithm/string/join.hpp>
#include <boost/assign/list_of.hpp>
#include <boost/bind.hpp>
#include <boost/call_traits.hpp>
#include <boost/exception/all.hpp>
#include <boost/filesystem.hpp>
#include <boost/foreach.hpp>
#include <boost/function.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/make_shared.hpp>

#include <string>

#include <csignal>

using namespace std;
using namespace IronBee::CLIPP;
using boost::bind;

using Input::input_p;

/**
 * A component.
 *
 * All components are functionals that share this signature.  There are three
 * types of components that are each interpreted differently:
 *
 * - Generators produce inputs.  They treat their argument as an out argument
 *   and return true iff they are able to produce an input.
 * - Modifiers modify inputs.  They treat their argument as an in/out
 *   argument and return true iff the input should continue to the next
 *   component in the chain.
 * - Consumers consume inputs.  They treat their argument as an in argument
 *   (although they may modify it) and return true iff they are able to
 *   continue consuming inputs.
 *
 * All components should report errors by throwing exceptions.  With a few
 * exceptions (see below), doing so results in the current input being
 * discarded but does not abort the chain.
 *
 * There are a few important caveats:
 *
 * - When a generator returns false, the chain does not stop.  Instead,
 *   singular (NULL) inputs are generated instead.  The chain stops when a
 *   singular input reaches the consumer.  This allows modifiers to
 *   manipulate the input stream across inputs, e.g., aggregation.  It does
 *   mean that all modifiers must gracefully handle singular inputs.  This is
 *   typically done with:
 *
 * @code
 * if (! input) {
 *     return true;
 * }
 * @endcode
 *
 * - A generator returning true but producing a singular input is an error.
 *   So is, a modifier returning true, but changing a non-singular input
 *   to singular.
 *
 * - Generators are passed non-singular inputs that point to a default
 *   constructed Input.  Thus generators need not be concerned with allocating
 *   an Input.  They are, however, free to change their argument to point to a
 *   different Input.
 *
 * - The input passed to a consumer is not used later.  Thus a consumer is
 *   free to do whatever they want with the argument.
 *
 * - Modifiers may do further control of the chain by throwing a clipp
 *   control exception.  See control.hpp.
 **/
typedef boost::function<bool(input_p&)> component_t;

//! A producer of components.
typedef boost::function<component_t(const string&)> component_factory_t;

//! A map of command line argument to factory.
typedef map<string, component_factory_t> component_factory_map_t;

/**
 * @name Constructor Helpers
 *
 * These functions can be used with bind to easily construct components.
 **/
///@{

//! Generic component constructor.
template <typename T>
component_t construct_component(const string& arg)
{
    return T(arg);
}

//! Generic component constructor for no-args.  Ignores @a arg.
template <typename T>
component_t construct_argless_component(const string& arg)
{
    return T();
}

//! Generic component constructor.  Converts @a arg to @a ArgType.
template <typename T, typename ArgType>
component_t construct_component(const string& arg)
{
    return T(boost::lexical_cast<ArgType>(arg));
}

///@}

/**
 * @name Specific component constructors.
 *
 * These routines construct components.  They are used when more complex
 * behavior (interpretation of @a arg) is needed than the generic constructors
 * above.
 **/

//! Construct threaded IronBee consumer, interpreting @a arg as @e path:n
component_t construct_ironbee_threaded_consumer(const string& arg);

//! Construct proxy consumer, interpreting @a arg as @e host:port:listen_port
component_t construct_proxy_consumer(const string& arg);

//! Construct raw generator, interpreting @a arg as @e request,response.
component_t construct_raw_generator(const string& arg);

#ifdef HAVE_NIDS
//! Construct pcap generator, interpreting @a arg as @e <path>:<filter>
component_t construct_pcap_generator(const string& arg);
#endif

//! Construct aggregate modifier.  An empty @a arg is 0, otherwise integer.
component_t construct_aggregate_modifier(const string& arg);

//! Construct split data modifier.  An empty @a arg is 0, otherwise integer.
component_t construct_splitdata_modifier(const string& arg);

//! Construct split header modifier.  An empty @a arg is 0, otherwise integer.
component_t construct_splitheader_modifier(const string& arg);

//! Construct ironbee modifiers.  @a arg is <config path>:<default behavior>.
component_t construct_ironbee_modifier(const string& arg);

/**
 * Construct select modifier.
 *
 * @param[in] arg @a arg is a comma separated list of either single indices
 *                are ranges: @a i-j.
 **/
component_t construct_select_modifier(const string& arg);

/**
 * Construct set modifier.
 *
 * @tparam mode Mode; see SetModifier.
 * @param[in] arg @a arg is either >key:value, <key:value, or key:value.
 **/
template <SetModifier::mode_e mode>
component_t construct_set_add_modifier(const string& arg);

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
    "  -e <path> -- Write last input to <path> as pb and exit on error.\n"
    "\n"
    "Generators:\n"
    "Note: For the following, paths can be - to use stdin.\n"
    "  pb:<path>       -- Read <path> as protobuf.\n"
    "  modsec:<path>   -- Read <path> as modsec audit log.\n"
    "                     One transaction per connection.\n"
    "  raw:<in>,<out>  -- Read <in>,<out> as raw data in and out.\n"
    "                     Single transaction and connection.\n"
    "  apache:<path>   -- Read <path> as apache NCSA format.\n"
    "  suricata:<path> -- Read <path> as suricata format.\n"
    "  htp:<path>      -- Read <path> as libHTP test format.\n"
    "  echo:<request>  -- Single connection with request as request line.\n"
#ifdef HAVE_NIDS
    "Note: pcap does not support reading from stdin.\n"
    "  pcap:<path>     -- Read <path> as PCAP containing only HTTP traffic.\n"
    "  pcap:<path>:<filter> --\n"
    "    Read <path> as PCAP using <filter> as PCAP filter selecting HTTP\n"
    "    traffic.\n"
#endif
    "\n"
    "Consumers:\n"
    "  ironbee:<path>  -- Internal IronBee using <path> as configuration.\n"
    "  ironbee_threaded:<path>:<n> -- Internal IronBee using <n> threads\n"
    "                                 and <path> as configuration.\n"
    "  writepb:<path>  -- Output to protobuf file at <path>.\n"
    "  writehtp:<path> -- Output in HTP test format at <path>.\n"
    "                     Best with unparsed format and only 1 connection.\n"
    "  view            -- Output to stdout for human consumption.\n"
    "  view:id         -- Output IDs to stdout for human consumption.\n"
    "  view:summary    -- Output summary to stdout for human consumption.\n"
    "  writeraw:<path> -- Output as raw files in a directory at <path>.\n"
    "  proxy:<proxy_host>:<proxy_port>:<listen_port> --\n"
    "    Send requests to a proxy and simulate the origin server.\n"
    "  null            -- Discard.\n"
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
    "  @aggregate:geometric:p -- \n"
    "    Aggregate transactions into a connections of n transactions\n"
    "    chosen at random from a geometric distribution with <p> chance of\n"
    "    success.\n"
    "  @aggregate:poisson:mean -- \n"
    "    Aggregate transactions into a connections of n transactions\n"
    "    chosen at random from a poisson distribution with mean <mean>.\n"
    "  @splitdata:<n> -- \n"
    "    Split data events into events of at most <n> bytes.\n"
    "  @splitdata:uniform:min,max -- \n"
    "    Split data events into events of <min> to <max> bytes chosen\n"
    "    uniformly at random.\n"
    "  @splitdata:binomial:t,p -- \n"
    "    Split data events into events of <n> bytes chosen at random from\n"
    "    a binomial distribution of <t> trials with <p> chance of success.\n"
    "  @splitdata:geometric:p -- \n"
    "    Split data events into events of <n> bytes chosen at random from\n"
    "    a geometric distribution with <p> chance of success.\n"
    "  @splitdata:poisson:mean -- \n"
    "    Split data events into events of <n> bytes chosen at random from\n"
    "    a poisson distribution with mean <mean>.\n"
    "  @splitheader -- \n"
    "    Split header events so that each header line has its own event."
    "  @splitheader:<n> -- \n"
    "    Split header into events of at most <n> lines.\n"
    "  @splitheader:uniform:min,max -- \n"
    "    Split header into events of <min> to <max> lines chosen\n"
    "    uniformly at random.\n"
    "  @splitheader:binomial:t,p -- \n"
    "    Split header into events of <n> lines chosen at random from\n"
    "    a binomial distribution of <t> trials with <p> chance of success.\n"
    "  @splitheader:geometric:p -- \n"
    "    Split header into events of <n> lines chosen at random from\n"
    "    a geometric distribution with <p> chance of success.\n"
    "  @splitheader:poisson:mean -- \n"
    "    Split header into events of <n> lines chosen at random from\n"
    "    a poisson distribution with mean <mean>.\n"
    "  @edit:which -- Edit part of each input with EDITOR.  <which> can be:\n"
    "    - request -- request line.\n"
    "    - request_header -- request header.\n"
    "    - request_body -- request body.\n"
    "    - response -- response line.\n"
    "    - response_header -- response header.\n"
    "    - response_body -- response body.\n"
    "    - connection_in -- connection data in.\n"
    "    - connection_out -- connection data out.\n"
    "  @limit:n -- Stop chain after <n> inputs.\n"
    "  @select:indices --\n"
    "    Only pass through <indices> inputs.\n"
    "    Indices are 1 based.\n"
    "    <indices> is comma separated list of single index or i-j ranges.\n"
    "  @set:key:value  -- Set all headers of <key> to <value>\n"
    "  @set:>key:value -- Set request headers of <key> to <value>\n"
    "  @set:<key:value -- Set response headers of <key> to <value>\n"
    "  @add:key:value  -- Add header <key> with value <value>.\n"
    "  @add:>key:value -- Add request header <key> with value <value>.\n"
    "  @add:<key:value -- Add response header <key> with value <value>.\n"
    "  @addmissing:key:value\n"
    "    Add header <key> with value <value> if header is missing..\n"
    "  @addmissing:>key:value\n"
    "    Add request header <key> with value <value> if header is missing..\n"
    "  @addmissing:<key:value\n"
    "    Add response header <key> with value <value> if header is missing..\n"
    "  @fillbody -- Add missing bodies and replace contents with @s.\n"
    "  @ironbee:config:behavior --\n"
    "    Run data through ironbee.\n"
    "    <behavior> is either 'allow' or 'block' and determines whether\n"
    "    the modifier passes data through or blocks data by default.\n"
    "    Rules may change the default behavior via the 'clipp' action.\n"
    "    clipp:allow passes data through; clipp:block blocks data;\n"
    "    and clipp:break stops the current chain.\n"
    "    <behavior> is optional and defaults to 'allow'.\n"
    "  @time -- Output timing of each transaction.\n"
    ;
}

/**
 * @name Helper methods for main().
 **/
///@{

/**
 * Constructs a component from a parsed representation.
 *
 * @param[in] component Parsed representation of component.
 * @param[in] map       Generator map for this type of component.
 * @returns Component.
 * @throw runtime_error if invalid component.
 **/
component_t build_component(
    const ConfigurationParser::component_t& component,
    const component_factory_map_t&          map
);

//! List of chains.
typedef list<ConfigurationParser::chain_t> chain_list_t;

/**
 * Load a configuration file.
 *
 * @param[out] chains List to append chains to.
 * @param[in]  path   Path to configuration file.
 * @throw Exception on error.
 **/
void load_configuration_file(
    chain_list_t& chains,
    const string& path
);

/**
 * Load configuration from text..
 *
 * @param[out] chains List to append chains to.
 * @param[in]  config Configuration text.
 * @throw Exception on error.
 **/
void load_configuration_text(
    chain_list_t& chains,
    const string& config
);

//! Modifier with name.
typedef pair<string, component_t> modifier_info_t;
//! List of modifier infos.
typedef list<modifier_info_t> modifier_info_list_t;

/**
 * Construct modifiers.
 *
 * On any exception, will output error message and rethrow.
 *
 * @param[out] modifier_infos       List to append to.
 * @param[in]  modifier_components  Modifiers to build.
 * @param[in]  modifier_factory_map Modifier factory map.
 **/
void build_modifiers(
     modifier_info_list_t&                       modifier_infos,
     const ConfigurationParser::component_vec_t& modifier_components,
     const component_factory_map_t&              modifier_factory_map
);

///@}

/**
 * Helper macro for catching exceptions.
 *
 * This macro turns exceptions into error messages.
 *
 * Example:
 * @code
 * try {
 *   ...
 * }
 * CLIPP_CATCH("Doing something", {return false;});
 * @endcode
 *
 * @param message Message to prepend to error message.  Best passed as string
 *                or @c const @c char*.
 * @param action  Action to take on exception.  Best passed as block, i.e.,
 *                @c {...}
 **/
#define CLIPP_CATCH(message, action) \
 catch (const boost::exception& e) { \
     cerr << (message) << ": " << diagnostic_information(e) << endl; \
     (action); \
 } \
 catch (const exception& e) { \
     cerr << (message) << ": " << e.what() << endl; \
     (action); \
 }

/**
 * Has a signal been triggered?
 *
 * Set to 0 by main() and 1 by handle_signal().
 */
static sig_atomic_t s_received_signal;

/**
 * Signal handler.
 *
 * Flips flag.
 */
void handle_signal(int)
{
    s_received_signal = 1;
}

/**
 * Main
 *
 * Interprets arguments, constructs the actual chains, and executes them.
 *
 * Component writers: Add your component to the generator maps at the top of
 * this function.
 *
 * @param[in] argc Number of arguments.
 * @param[in] argv Arguments.
 * @return Exit code.
 **/
int main(int argc, char** argv)
{
    int exit_status = 0;

    if (argc == 1) {
        help();
        return 1;
    }

    // Signals
    s_received_signal = 0;
    signal(SIGINT,  handle_signal);
    signal(SIGHUP,  handle_signal);
    signal(SIGTERM, handle_signal);
    signal(SIGQUIT, handle_signal);

    list<string> args;

    // Declare generators.
    component_factory_map_t generator_factory_map = boost::assign::map_list_of
        ("modsec",   construct_component<ModSecAuditLogGenerator>)
        ("raw",      construct_raw_generator)
        ("pb",       construct_component<PBGenerator>)
        ("apache",   construct_component<ApacheGenerator>)
        ("suricata", construct_component<SuricataGenerator>)
        ("htp",      construct_component<HTPGenerator>)
        ("echo",     construct_component<EchoGenerator>)
#ifdef HAVE_NIDS
        ("pcap",     construct_pcap_generator)
#endif
        ;

    // Declare consumers.
    component_factory_map_t consumer_factory_map = boost::assign::map_list_of
        ("ironbee",  construct_component<IronBeeConsumer>)
        ("ironbee_threaded",  construct_ironbee_threaded_consumer)
        ("writepb",  construct_component<PBConsumer>)
        ("writehtp", construct_component<HTPConsumer>)
        ("view",     construct_component<ViewConsumer>)
        ("writeraw", construct_component<RawConsumer>)
        ("proxy",    construct_proxy_consumer)
        ("null",     construct_argless_component<NullConsumer>)
        ;

    // Declare modifiers.
    component_factory_map_t modifier_factory_map = boost::assign::map_list_of
        ("view",            construct_component<ViewModifier>)
        ("set_local_ip",    construct_component<SetLocalIPModifier>)
        ("set_local_port",  construct_component<SetLocalPortModifier, uint32_t>)
        ("set_remote_ip",   construct_component<SetRemoteIPModifier>)
        ("set_remote_port", construct_component<SetRemotePortModifier, uint32_t>)
        ("parse",           construct_argless_component<ParseModifier>)
        ("unparse",         construct_argless_component<UnparseModifier>)
        ("aggregate",       construct_aggregate_modifier)
        ("splitdata",       construct_splitdata_modifier)
        ("splitheader",     construct_splitheader_modifier)
        ("edit",            construct_component<EditModifier>)
        ("limit",           construct_component<LimitModifier, size_t>)
        ("select",          construct_select_modifier)
        ("set",             construct_set_add_modifier<SetModifier::REPLACE_EXISTING>)
        ("add",             construct_set_add_modifier<SetModifier::ADD>)
        ("addmissing",      construct_set_add_modifier<SetModifier::ADD_MISSING>)
        ("fillbody",        construct_argless_component<FillBodyModifier>)
        ("ironbee",         construct_ironbee_modifier)
        ("time",            construct_argless_component<TimeModifier>)
        ;

    // Convert argv to args.
    for (int i = 1; i < argc; ++i) {
        args.push_back(argv[i]);
    }

    string write_on_error;
    list<ConfigurationParser::chain_t> chains;
    // Parse flags.
    while (! args.empty() && args.front()[0] == '-') {
        string arg = args.front();
        args.pop_front();
        if (arg == "-c") {
            if (args.empty()) {
                cerr << "-c requires an argument." << endl;
                help();
                return 1;
            }
            string path = args.front();
            args.pop_front();

            try {
                load_configuration_file(chains, path);
            }
            CLIPP_CATCH("Error parsing configuration file", {return 1;});
        }
        else if (arg == "-e") {
            if (args.empty()) {
                cerr << "-e requires an argument." << endl;
                help();
                return 1;
            }
            write_on_error = args.front();
            args.pop_front();
        }
        else {
            cerr << "Unrecognized flag: " << arg << endl;
            help();
            return 1;
        }
    }

    // Convert argv to configuration.
    string configuration = boost::algorithm::join(args, " ");

    try {
        load_configuration_text(chains, configuration);
    }
    CLIPP_CATCH("Error parsing configuration", {return 1;});

    // Basic validation.
    if (chains.size() < 2) {
        cerr << "Need at least a generator and a consumer." << endl;
        help();
        return 1;
    }
    // Last component must be consumer.
    component_t consumer;
    ConfigurationParser::chain_t consumer_chain = chains.back();
    chains.pop_back();
    try {
        consumer = build_component(
            consumer_chain.base,
            consumer_factory_map
        );
    }
    CLIPP_CATCH("Error constructing consumer", {return 1;});

    // Construct consumer modifiers.
    modifier_info_list_t consumer_modifiers;
    try {
        build_modifiers(
            consumer_modifiers,
            consumer_chain.modifiers,
            modifier_factory_map
        );
    }
    catch (...) {
        // build_modifiers() will output error message.
        return 1;
    }

    // Loop through components, generating and processing input generators
    // as needed to limit the scope of each input generator.  As input
    // generators can make use of significant memory, it is good to only have
    // one around at a time.
    BOOST_FOREACH(const ConfigurationParser::chain_t& chain, chains) {
        component_t generator;
        try {
            generator = build_component(
                chain.base,
                generator_factory_map
            );
        }
        CLIPP_CATCH(
            "Error constructing generator " + chain.base.name,
            {return 1;}
        );

        // Generate modifiers.
        modifier_info_list_t modifiers;
        try {
            build_modifiers(
                modifiers,
                chain.modifiers,
                modifier_factory_map
            );
        }
        catch (...) {
            // build_modifiers() will output error message.
            return 1;
        }

        // Append consumer modifiers.
        copy(
            consumer_modifiers.begin(), consumer_modifiers.end(),
            back_inserter(modifiers)
        );

        // Process inputs.
        input_p input;
        bool generator_continue = true;
        bool consumer_continue  = true;
        bool end_of_generator   = false;
        while (generator_continue && consumer_continue) {
            input.reset();

            if (! end_of_generator) {
                // Make new input for each run.  Extra allocations but avoids
                // some pitfalls.
                input = boost::make_shared<Input::Input>();

                try {
                    generator_continue = generator(input);
                }
                catch (clipp_break) {
                    break;
                }
                catch (clipp_continue) {
                    continue;
                }
                CLIPP_CATCH("Error generating input", {continue;});

                if (generator_continue && ! input) {
                    cerr << "Generator said it provided input, but didn't."
                         << endl;
                    continue;
                }

                if (! generator_continue) {
                    // Only stop if the singular input reaches the consumer.
                    input.reset();
                    end_of_generator = true;
                }
            }

            bool modifier_success  = true;
            bool modifier_break    = false;
            bool modifier_continue = false;
            BOOST_FOREACH(const modifier_info_t& modifier_info, modifiers) {
                try {
                    modifier_success = modifier_info.second(input);
                }
                catch (clipp_break) {
                    modifier_break = true;
                    break;
                }
                catch (clipp_continue) {
                    modifier_continue = true;
                    break;
                }
                CLIPP_CATCH(
                    "Error applying modifier " + modifier_info.first + ": ",
                    {modifier_success = false; break;}
                );

                // If pushing through a singular input, apply to all
                // modifiers.
                if (input && ! modifier_success) {
                    break;
                }
            }
            if (modifier_break) {
                end_of_generator = true;
                continue;
            }
            if (! modifier_success || modifier_continue) {
                continue;
            }

            if (! input && end_of_generator) {
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
            catch (clipp_break) {
                end_of_generator = true;
                continue;
            }
            catch (clipp_continue) {
                continue;
            }
            CLIPP_CATCH("Error consuming input", {
                if (write_on_error.empty()) {
                    cout << "Exiting." << endl;
                    exit_status = 1;
                    break;
                }
                else {
                    PBConsumer consumer(write_on_error);
                    consumer(input);
                    cout << "Wrote last input to " << write_on_error << endl;
                    cout << "Exiting." << endl;
                    exit_status = 1;
                    break;
                }
            });

            if (! consumer_continue) {
                cerr << "Consumer refusing input." << endl;
            }

            if (s_received_signal == 1) {
                break;
            }
        }

        if (s_received_signal == 1) {
            cout << "Received Signal: Exiting." << endl;
            exit_status = 1;
            break;
        }
    }

    return exit_status;
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

component_t construct_raw_generator(const string& arg)
{
    vector<string> subargs = split_on_char(arg, ',');
    if (subargs.size() != 2) {
        throw runtime_error("Raw inputs must be _request_,_response_.");
    }
    if (subargs[0] == "-" && subargs[1] == "-") {
        throw runtime_error("Only one input to raw can be stdin.");
    }

    return RawGenerator(subargs[0], subargs[1]);
}

#ifdef HAVE_NIDS
component_t construct_pcap_generator(const string& arg)
{
    vector<string> subargs = split_on_char(arg, ':');
    if (subargs.size() == 1) {
        subargs.push_back("");
    }
    if (subargs.size() != 2) {
        throw runtime_error("Could not parse pcap arg.");
    }

    return PCAPGenerator(subargs[0], subargs[1]);
}
#endif

template <typename ModifierType>
component_t construct_randomized_modifier(
    const string& name,
    const string& arg
)
{
    if (arg.empty()) {
        return ModifierType();
    }
    else {
        vector<string> subargs = split_on_char(arg, ':');
        if (subargs.size() == 1) {
            return construct_component<ModifierType, size_t>(subargs[0]);
        }
        else if (subargs.size() == 2) {
            vector<string> subsubargs = split_on_char(subargs[1], ',');
            if (subargs[0] == "uniform") {
                if (subsubargs.size() != 2) {
                    throw runtime_error(
                        "Expected two distribution arguments."
                    );
                }
                return ModifierType::uniform(
                    boost::lexical_cast<unsigned int>(subsubargs[0]),
                    boost::lexical_cast<unsigned int>(subsubargs[1])
                );
            }
            else if (subargs[0] == "binomial") {
                if (subsubargs.size() != 2) {
                    throw runtime_error(
                        "Expected two distribution arguments."
                    );
                }
                return ModifierType::binomial(
                    boost::lexical_cast<unsigned int>(subsubargs[0]),
                    boost::lexical_cast<double>(subsubargs[1])
                );
            }
            else if (subargs[0] == "geometric") {
                if (subsubargs.size() != 1) {
                    throw runtime_error(
                        "Expected one distribution argument."
                    );
                }
                return ModifierType::geometric(
                    boost::lexical_cast<double>(subsubargs[0])
                );
            }
            else if (subargs[0] == "poisson") {
                if (subsubargs.size() != 1) {
                    throw runtime_error(
                        "Expected one distribution argument."
                    );
                }
                return ModifierType::poisson(
                    boost::lexical_cast<double>(subsubargs[0])
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

component_t construct_aggregate_modifier(const string& arg)
{
    return construct_randomized_modifier<AggregateModifier>(
        "aggregate",
        arg
    );
}

component_t construct_splitdata_modifier(const string& arg)
{
    if (arg.empty()) {
        throw runtime_error("@splitdata requires an argument.");
    }
    return construct_randomized_modifier<SplitDataModifier>(
        "splitdata",
        arg
    );
}

component_t construct_splitheader_modifier(const string& arg)
{
    return construct_randomized_modifier<SplitHeaderModifier>(
        "splitheader",
        arg
    );
}

component_t construct_select_modifier(const string& arg)
{
    if (arg.empty()) {
        throw runtime_error("@select requires an argument.");
    }

    SelectModifier::range_list_t select;
    vector<string> subargs = split_on_char(arg, ',');
    BOOST_FOREACH(const string& subarg, subargs) {
        vector<string> subsubargs = split_on_char(subarg, '-');
        if (subsubargs.size() == 1) {
            subsubargs.push_back(subsubargs.front());
        }
        if (subsubargs.size() != 2) {
            throw runtime_error("Could not parse subarg: " + subarg);
        }
        size_t left;
        size_t right;
        try {
            left = boost::lexical_cast<size_t>(subsubargs.front());
        }
        catch (...) {
            throw runtime_error("Error parsing: " + subsubargs.front());
        }
        try {
            right = boost::lexical_cast<size_t>(subsubargs.back());
        }
        catch (...) {
            throw runtime_error("Error parsing: " + subsubargs.back());
        }

        if (left > right) {
            swap(left, right);
        }

        select.push_back(make_pair(left, right));
    }

    return SelectModifier(select);
}

template <SetModifier::mode_e mode>
component_t construct_set_add_modifier(const string& arg)
{
    SetModifier::which_e which = SetModifier::BOTH;

    string modified_arg;
    if (arg[0] == '<') {
        which = SetModifier::RESPONSE;
        modified_arg = arg.substr(1);
    }
    else if (arg[0] == '>') {
        which = SetModifier::REQUEST;
        modified_arg = arg.substr(1);
    }
    else {
        modified_arg = arg;
    }

    // We don't use split_on_char because we want all to : and all after :
    // even if there are later :s.
    size_t colon_i = modified_arg.find_first_of(':');
    if (colon_i == string::npos) {
        throw runtime_error("Could not parse: " + arg);
    }
    const string key   = modified_arg.substr(0, colon_i);
    const string value = modified_arg.substr(colon_i + 1);

    return SetModifier(which, mode, key, value);
}

component_t construct_ironbee_threaded_consumer(const string& arg)
{
    string config_path;
    size_t num_workers;

    vector<string> subargs = split_on_char(arg, ':');
    if (subargs.size() == 2) {
        config_path = subargs[0];
        num_workers = boost::lexical_cast<size_t>(subargs[1]);
    }
    else {
        throw runtime_error("Could not parse ironbee_threaded arg: " + arg);
    }

    return IronBeeThreadedConsumer(config_path, num_workers);
}

component_t construct_proxy_consumer(const string& arg)
{
    string proxy_host;
    uint16_t proxy_port;
    uint16_t listen_port;

    vector<string> subargs = split_on_char(arg, ':');
    if (subargs.size() == 2) {
        proxy_host = subargs[0];
        proxy_port = listen_port = boost::lexical_cast<uint32_t>(subargs[1]);
    }
    else if (subargs.size() == 3) {
        proxy_host = subargs[0];
        proxy_port = boost::lexical_cast<uint16_t>(subargs[1]);
        listen_port = boost::lexical_cast<uint16_t>(subargs[2]);
    }
    else {
        throw runtime_error("Could not parse proxy arg: " + arg);
    }
    return ProxyConsumer(proxy_host, proxy_port, listen_port);
}

component_t construct_ironbee_modifier(const string& arg)
{
    IronBeeModifier::behavior_e behavior = IronBeeModifier::ALLOW;
    string config_path;

    vector<string> subargs = split_on_char(arg, ':');
    if (subargs.size() == 2) {
        config_path = subargs[0];
        if (subargs[1] == "allow") {
            behavior = IronBeeModifier::ALLOW;
        }
        else if (subargs[1] == "block") {
            behavior = IronBeeModifier::BLOCK;
        }
        else {
            throw runtime_error("Unknown @ironbee behavior: " + subargs[1]);
        }
    }
    else if (subargs.size() == 1) {
        config_path = subargs[0];
    }
    else {
        throw runtime_error("Could not parse @ironbee arg: " + arg);
    }

    return IronBeeModifier(config_path, behavior);
}

component_t build_component(
    const ConfigurationParser::component_t& component,
    const component_factory_map_t&          map
)
{
    component_factory_map_t::const_iterator i = map.find(component.name);
    if (i == map.end()) {
        throw runtime_error("Unknown component: " + component.name);
    }

    return i->second(component.arg);
}

void load_configuration_file(
    chain_list_t& chains,
    const string& path
)
{
    ConfigurationParser::chain_vec_t file_chains;
    file_chains = ConfigurationParser::parse_file(path);
    copy(
        file_chains.begin(), file_chains.end(),
        back_inserter(chains)
    );
}

void load_configuration_text(
    chain_list_t& chains,
    const string& config
)
{
    ConfigurationParser::chain_vec_t text_chains;
    text_chains = ConfigurationParser::parse_string(config);
    copy(
        text_chains.begin(), text_chains.end(),
        back_inserter(chains)
    );
}

void build_modifiers(
    modifier_info_list_t&                       modifier_infos,
    const ConfigurationParser::component_vec_t& modifier_components,
    const component_factory_map_t&              modifier_factory_map
)
{
    BOOST_FOREACH(
        const ConfigurationParser::component_t& modifier_component,
        modifier_components
    ) {
        modifier_infos.push_back(modifier_info_t(
            modifier_component.name,
            component_t()
        ));
        modifier_info_t& modifier_info = modifier_infos.back();
        try {
            modifier_info.second = build_component(
                modifier_component,
                modifier_factory_map
            );
        }
        CLIPP_CATCH(
            "Error constructing modifier " + modifier_component.name,
            {throw;}
        );
    }
}
