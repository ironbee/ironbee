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
 * @brief IronBee --- ibctl
 *
 * A CLI for sending control messages to a running IronBee engine manager.
 *
 * @author Sam Baskinger <sbaskinger@qualys.com>
 */

#include <iostream>
#include <string>
#include <vector>

#include <boost/algorithm/string/join.hpp>
#include <boost/program_options/cmdline.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/variables_map.hpp>

#include <ironbee/engine_manager_control_channel.h>

#include <ironbeepp/catch.hpp>
#include <ironbeepp/exception.hpp>
#include <ironbeepp/memory_pool_lite.hpp>
#include <ironbeepp/memory_manager.hpp>
#include <ironbeepp/throw.hpp>

namespace {

/**
 * The command line parser puts all its parsed output here.
 */
struct parsed_options_t {
    std::vector<std::string> cmd;       /**< Command to send to the server. */
    std::string              sock_path; /**< Server socket path. */
};

/**
 * A type of runtime error that signals this program to exit with non-zero.
 *
 * This is used to centralize the program termination.
 */
class exit_exception : public std::runtime_error
{
public:
    explicit exit_exception(const std::string& what):
        std::runtime_error(what)
    {

    }
};

/**
 * Parse the program options.
 *
 * This function may call @c exit().
 *
 * @param[in] argc The number of elements in @a argv.
 * @param[in] argv An array of nul-terminated strings.
 * @param[in] vm The variable map to populate with config values.
 *
 */
void parse_options(
    int               argc,
    const char**        argv,
    parsed_options_t& parsed_options
)
{
    namespace po = boost::program_options;

    try
    {
        po::variables_map vm;

        /* Positional options. These are non-flag command line elements. */
        po::positional_options_description desc_pos;

        /* Hidden options. */
        po::options_description desc_hidden("Hidden options");

        /* All options. */
        po::options_description desc_all("All options");

        /* Options whose help text we will show to the user. */
        po::options_description desc_visible(
            "ibctl [options] <command> <command options...>\n"
            "Commands:\n"
            "  echo <text to echo>\n"
            "    Echo the arguments to the caller.\n"
            "  version\n"
            "    Return the version of the IronBee engine.\n"
            "  enable\n"
            "    Reenable a disabled IronBee instance.\n"
            "  disable\n"
            "    Disable IronBee. Running transactions will complete.\n"
            "  cleanup\n"
            "    Force a cleanup of old idle IronBee engines.\n"
            "  engine_create <ironbee configuration file>\n"
            "    Change the current IronBee engine being used.\n"
            "Options"
        );

        desc_pos.add("cmd", -1);

        desc_visible.add_options()
            ("help,h", "Print this screen.")
            ("sock,s", po::value<std::string>(), "Socket path")
        ;

        desc_hidden.add_options()
            ("cmd", po::value<std::vector< std::string > >())
        ;

        /* Bind the visible and hidden options into a single description. */
        desc_all.add(desc_visible).add(desc_hidden);

        po::store(
            po::command_line_parser(argc, argv).
                options(desc_all).
                positional(desc_pos).
                run(),
            vm);

        po::notify(vm);

        if (vm.count("help") > 0) {
            std::cout << desc_visible;
            exit(0);
        }

        if (vm.count("cmd") > 0) {
            parsed_options.cmd = vm["cmd"].as<std::vector<std::string> >();
        }

        if (vm.count("sock") > 0) {
            parsed_options.sock_path = vm["sock"].as<std::string>();
        }
    }
    catch (const boost::program_options::multiple_occurrences& err)
    {
        BOOST_THROW_EXCEPTION(exit_exception(err.what()));
    }
    catch (const boost::program_options::unknown_option& err)
    {
        BOOST_THROW_EXCEPTION(exit_exception(err.what()));
    }
}

/**
 * Any command validation is done here.
 *
 * @throws exit_exception on a validation error.
 */
void validate_cmd(const parsed_options_t& opts)
{
    if (opts.cmd.size() == 0) {
        BOOST_THROW_EXCEPTION(
            exit_exception(
                "No command given to send to IronBee."
            )
        );
    }

    if (opts.cmd[0] == "engine_create") {
        if (opts.cmd.size() < 2) {
            BOOST_THROW_EXCEPTION(
                exit_exception(
                    "engine_create requires a path to a configuration file."
                )
            );
        }
    }
}

/**
 * Send a command.
 *
 * @param[in] opts The parsed program options that specify what to send.
 *
 * @throws IronBee::error on API errors.
 */
void send_cmd(const parsed_options_t& opts)
{
    std::string                   cmd = boost::algorithm::join(opts.cmd, " ");
    std::string                   sock;
    IronBee::ScopedMemoryPoolLite mp;
    IronBee::MemoryManager        mm(mp);
    const char*                   response;

    /* Pick a socket file (or use a default). */
    if (opts.sock_path == "") {
        sock = ib_engine_manager_control_channel_socket_path_default();
    }
    else {
        sock = opts.sock_path;
    }

    IronBee::throw_if_error(
        ib_engine_manager_control_send(
            sock.c_str(),
            cmd.c_str(),
            mm.ib(),
            &response
        ),
        (std::string("Failed to send message to server socket ")+sock).c_str()
    );

    /* On success, report the response string back to the user. */
    std::cout << response << std::endl;
}

} // anon namespace

int main(int argc, const char** argv)
{

    parsed_options_t parsed_options;

    try {
        parse_options(argc, argv, parsed_options);

        /* Validate the command. */
        validate_cmd(parsed_options);

        ib_util_initialize();

        send_cmd(parsed_options);

        ib_util_shutdown();
    }
    catch (const IronBee::error& err) {
        IronBee::convert_exception();
        return 1;
    }
    catch (const  std::exception& err) {
        std::cerr<< "Error: " << err.what() << std::endl;
        return 1;
    }

    return 0;
}
