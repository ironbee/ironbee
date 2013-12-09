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
 * @brief IronBee --- Example Server: Parsed C++ Edition
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 *
 * This example demonstrates a minimalistic server and the use of
 * ParserSuite.  It creates an Engine, loads a configuration file of the
 * users choice, parses a single request and response, and feeds them to the
 * engine. This  example is single threaded, although some multithreaded
 * considerations are commented on.
 *
 * This is intended as an example only.  IronBee comes with a command line
 * interface, clipp, with a wide feature set including a variety of input
 * formats and multithreaded support.
 *
 * For an example server in C, see parsed_c.c.  The C edition does not
 * include parsing, and uses a hardcoded request/response pair.
 *
 * @section unparsed_cpp_ironbeepp IronBee++
 *
 * IronBee++ is the C++ API for IronBee.  It wraps the C API and provides a
 * variety of C++ capabilities such as functions instead of function pointers.
 * All IronBee++ objects that corresponds to C API types, e.g.,
 * IronBee::Server for @ref ib_server_t, behave like pointers or references.
 * In particular, they are cheap to copy and all copies refer to the
 * underlying C object.  They come in non-const and const versions, e.g.,
 * IronBee::Server is as `ib_server_t*` and `IronBee::ConstServer` is as
 * `const ib_server_t*`.  See ironbeepp/common_semantics.hpp for a complete
 * discussion and @ref ironbeepp for an overview of IronBee++.
 *
 * @section unparsed_cpp_parsersuite ParserSuite
 *
 * ParserSuite is a collection of pure, copy-less parsers.  Parsers are pure
 * in that they have no effects besides modifying their input parameters
 * and returning a value.  They are copy-less in that they make no copies of
 * input, but inside provide results by aliasing segments of the input.
 * ParserSuite revolves around the IronBee::ParserSuite::span_t type which is
 * a non-mutable range of bytes.  All parsers take an input spanwhich, on
 * completion, is set to be the remaining input (i.e., will become empty if
 * all data is parsed), and return a result structure.
 *
 * ParserSuite is currently included in the IronBee source but it has no
 * dependency on any other portion of IronBee.
 **/

#include <ironbeepp/all.hpp>
#include <modules/parser_suite.hpp>

#include <fstream>

using namespace std;

/**
 * An example WAF using IronBee.
 *
 * This class creates a server and an IronBee engine using that server and
 * then provides a simple public API for interacting with IronBee.
 *
 * All methods report errors by throwing descendents of IronBee::error.
 * See ironbeepp/exception.hpp.
 **/
class ExampleIronBee
{
public:
    /**
     * Constructor.
     *
     * Handles server and engine creation.
     *
     * @param[in] name What to name the server.
     **/
    explicit
    ExampleIronBee(const string& name);

    /**
     * Destructor.
     *
     * Handles server callback and engine teardown.
     **/
    ~ExampleIronBee();

    /**
     * Load configuration file at @a path into IronBee.
     *
     * @warning This method must be called exactly once and before
     * send_to_ironbee() is used.
     *
     * @param[in] path Path to configuration file to load.
     **/
    void load_configuration(
        const string& path
    ) const;

    /**
     * Send unparsed data to IronBee.
     *
     * Parses data and calls parsed version (see below).
     *
     * @param[in] request  Unparsed request data.
     * @param[in] response Unparsed response data.
     **/
    void send_to_ironbee(
        const vector<char>& request,
        const vector<char>& response
    ) const;

    /**
     * Send ParserSuite parsed data to IronBee.
     *
     * The request and response will be combined to make a single transaction
     * which in turn is put inside a connection.  This method may be called
     * multiple times to send multiple connections to IronBee, but this
     * example does not support multiple transactions per connection.
     *
     * @param[in] request       Request line and headers.
     * @param[in] request_body  Request body.
     * @param[in] response      Response line and headers.
     * @param[in] response_body Response body.
     **/
    void send_to_ironbee(
        const IronBee::ParserSuite::parse_request_result_t&  request,
        const IronBee::ParserSuite::span_t&                  request_body,
        const IronBee::ParserSuite::parse_response_result_t& response,
        const IronBee::ParserSuite::span_t&                  response_body
    ) const;

private:

    /** @name Callbacks
     * Server callbacks.
     *
     * This methods are bound into functionals and then stored in the
     * IronBee::Server.  The IronBee::Engine will use them to communicate back
     * to the server.  They have little purpose for passive use of IronBee but
     * are vital for inline use.
     *
     * In this example, all of these callbacks do nothing but output a
     * message to cout.
     *
     * All callbacks should throw IronBee::edeclined if they do not wish to do
     * what is asked of them.  Callbacks do not need to be specified.  Any
     * missing callbacks implicitly throw IronBee::enotimpl.
     *
     * There are also callbacks for modifying bodies in flight.  These are an
     * advanced optional feature and are not included in this example.
     *
     * See ironbeepp/server.hpp for additional documentation.
     **/
    ///@{

    /**
     * IronBee requests that server respond with an error status.
     *
     * This call may be followed by one or more calls to on_error_header to
     * set headers for the error response and a on_error_data() call to set
     * the body.
     *
     * @param[in] transaction Transaction to respond to.
     * @param[in] status      Status code to use.
     **/
    void on_error(
        IronBee::Transaction transaction,
        int                  status
    ) const;

    /**
     * IronBee requests that server provide a certain header in error
     * response.
     *
     * @param[in] transaction  Transaction.
     * @param[in] name         Name of header.
     * @param[in] name_length  Length of @a name.
     * @param[in] value        Value of header.
     * @param[in] value_length Length of @a value.
     **/
    void on_error_header(
        IronBee::Transaction transaction,
        const char*          name,
        size_t               name_length,
        const char*          value,
        size_t               value_length
    ) const;

    /**
     * IronBee requests that server provide a certain body in error response.
     *
     * @param[in] transaction Transaction.
     * @param[in] data        Data of body.
     * @param[in] data_length Length of @a data.
     **/
    void on_error_data(
        IronBee::Transaction transaction,
        const char*          data,
        size_t               data_length
    ) const;

    /**
     * IronBee requests that server modify headers before further processing.
     *
     * @param[in] transaction  Transaction.
     * @param[in] dir          Which of request and response to modify
     *                         headers of.
     * @param[in] action       Type of modification to do.  Options are set
     *                         (replace), append (add), merge (add if
     *                         missing), and unset (remove).
     * @param[in] name         Name of header.
     * @param[in] name_length  Length of @a name.
     * @param[in] value        Value of header.
     * @param[in] value_length Length of @a value.
     **/
    void on_header(
        IronBee::Transaction             transaction,
        IronBee::Server::direction_e     direction,
        IronBee::Server::header_action_e action,
        const char*                      name,
        size_t                           name_length,
        const char*                      value,
        size_t                           value_length
    ) const;

    /**
     * IronBee requests that server close connection.
     *
     * @param[in] connection  Connection to close.
     * @param[in] transaction Transaction.
     **/
    void on_close(
        IronBee::Connection  connection,
        IronBee::Transaction transaction
    ) const;

    ///@}

    /**
     * IronBee server by value.
     *
     * IronBee::ServerValue is the value of an IronBee::Server object
     * (recall that IronBee++ objects behave like pointers/references).
     * An IronBee::Server can be accessed via `m_server_value.get()`.
     **/
    IronBee::ServerValue m_server_value;

    /**
     * IronBee engine.
     **/
    IronBee::Engine m_engine;
};

/* Helper Functions */

namespace {

/**
 * Load the entire contents of the file at @a path into @a to.
 *
 * @param[in] to   Buffer to write contents to.  Any existing content will be
 *                 lost.
 * @param[in] path Path to file to load.
 * @throw @ref runtime_error on any file system error.
 **/
void load_file(vector<char>& to, const string& path);

}

/* Implementation */

int main(int argc, char** argv)
{
    if (argc != 4) {
        printf("Usage: %s <configuration> <request> <response>\n", argv[0]);
        return 1;
    }

    /* Initialize IronBee */
    IronBee::initialize();

    /* All interactions are wrapped in a try block to convert exceptions
     * into error messages.
     */
    try {
        /* Construct an instance of our implementation. */
        ExampleIronBee example("example");

        /* Load configuration */
        example.load_configuration(argv[1]);

        /* Read and feed to data */
        {
            vector<char> request_data;
            vector<char> response_data;

            /* Read request/response */
            load_file(request_data, argv[2]);
            load_file(response_data, argv[3]);

            /* Send some traffic to the m_engine. */
            example.send_to_ironbee(request_data, response_data);
        }
    }
    catch (const IronBee::error& e)
    {
        /* All IronBee++ exceptions are based on boost::exception and thus
         * can be used with boost::diagnostic_information which produces a
         * report including the file and line number the exception was thrown
         * at.  Such information may not be appropriate for production.
         * See ironbeepp/exception.hpp for additional discussion.
         */
        cerr << "Error occurred: "
             << diagnostic_information(e)
             << endl;
    }

    /* Shutdown IronBee */
    IronBee::shutdown();
}

ExampleIronBee::ExampleIronBee(const string& name) :
    m_server_value(name.c_str(), __FILE__)
{
    using boost::bind;

    IronBee::Server mutable_server = m_server_value.get();

    /* One of the services IronBee++ is to allow the use of C++ functionals
     * in place of C function pointers.  This service does require some
     * allocations.  In most of IronBee++ this is handled via memory pools.
     * However, as the server must be created before an Engine, no memory
     * pools are available.  So, for server callbacks, memory is allocated via
     * new and must be manually freed by calling
     * IronBee::Server::destroy_callbacks() as we do in ~ExampleIronBee.
     */
    mutable_server.set_error_callback(
        bind(&ExampleIronBee::on_error, this, _1, _2)
    );
    mutable_server.set_error_header_callback(
        bind(&ExampleIronBee::on_error_header, this, _1, _2, _3, _4, _5)
    );
    mutable_server.set_error_data_callback(
        bind(&ExampleIronBee::on_error_data, this, _1, _2, _3)
    );
    mutable_server.set_header_callback(
        bind(&ExampleIronBee::on_header, this, _1, _2, _3, _4, _5, _6, _7)
    );
    mutable_server.set_close_callback(
        bind(&ExampleIronBee::on_close, this, _1, _2)
    );

    m_engine = IronBee::Engine::create(mutable_server);
}

ExampleIronBee::~ExampleIronBee()
{
    m_engine.destroy();
    // See ExampleIronBee().
    m_server_value.get().destroy_callbacks();
}

void ExampleIronBee::load_configuration(
    const string& path
) const
{
    /* This example shows how to configure from a file.  The configuration
     * parse includes a variety of ways to configure, including the ability
     * to build up a parsed configuration and then apply to the m_engine.
     */
    IronBee::ConfigurationParser parser =
        IronBee::ConfigurationParser::create(m_engine);

    m_engine.configuration_started(parser);

    parser.parse_file(path);

    m_engine.configuration_finished();

    parser.destroy();
}

void ExampleIronBee::send_to_ironbee(
    const vector<char>& request,
    const vector<char>& response
) const
{
    IronBee::ParserSuite::span_t request_span(
        &request.front(), &request.front() + request.size()
    );
    IronBee::ParserSuite::parse_request_result_t parsed_request =
        IronBee::ParserSuite::parse_request(request_span);
    if (! parsed_request.headers.terminated) {
        BOOST_THROW_EXCEPTION(
            IronBee::eother()
                << IronBee::errinfo_what("Unterminated request headers.")
        );
    }
    // request_span is now the body.

    IronBee::ParserSuite::span_t response_span(
        &response.front(), &response.front() + response.size()
    );
    IronBee::ParserSuite::parse_response_result_t parsed_response =
        IronBee::ParserSuite::parse_response(response_span);
    if (! parsed_response.headers.terminated) {
        BOOST_THROW_EXCEPTION(
            IronBee::eother()
                << IronBee::errinfo_what("Unterminated responses headers.")
        );
    }
    // response_span is now the body.

    send_to_ironbee(
        parsed_request,
        request_span,
        parsed_response,
        response_span
    );
}


void ExampleIronBee::send_to_ironbee(
    const IronBee::ParserSuite::parse_request_result_t&  request,
    const IronBee::ParserSuite::span_t&                  request_body,
    const IronBee::ParserSuite::parse_response_result_t& response,
    const IronBee::ParserSuite::span_t&                  response_body
) const
{
    /*
     * Create Connection
     *
     * A connection is some TCP/IP information and a sequence of transactions.
     * Its primary purpose is to associate transactions.
     *
     * IronBee allows for multithreading so long as a single connection (and
     * its tranactions) is only be used in one thread at a time.  Thus, a
     * typical multithreaded IronBee setup would pass connections off to
     * worker  threads, but never use multiple threads for a single
     * connection.
     */
    IronBee::Connection connection = IronBee::Connection::create(m_engine);

    // IronBee also supports IPv6 addresses.
    connection.set_local_ip_string("1.2.3.4");
    connection.set_local_port(80);
    connection.set_remote_ip_string("5.6.7.8");
    connection.set_remote_port(1234);

    /* IronBee::Engine supports state notification via a sub-object,
     * IronBee::Notifier accessed via IronBee::Engine::notify().  The use of
     * sub-objects allows for subsections of IronBee::Engine functionality to
     * be defined and implemented in other objects/files.  This pattern helps
     * organize IronBee::Engine and limit dependencies.
     */

    /*
     * Connection Opened
     *
     * Here is our first state-notify call.  All communication of data and
     * events to IronBee is via state notify calls.  We begin with
     * Notifier::connection_opened() which tells IronBee that a new connection
     * has been established.  Its dual is Notifier::connection_closed() which
     * we call at the end.
     */
    m_engine.notify().connection_opened(connection);

    /*
     * Create Transaction
     *
     * The transaction object holds all per-transaction information.  Besides
     * using it to indicate which transaction we are providing data for, it
     * will allow us to control the lifetime of all our created objects.  We
     * do this by allocating all memory from `transaction.memory_pool()`, a
     * memory pool whose lifetime is equal to that of the transaction.
     */
    IronBee::Transaction transaction =
        IronBee::Transaction::create(connection);

    /*
     * The next several sections go through the typical transaction lifecycle:
     *
     * - Request Started which provides the request line.
     * - Request Header which provides headers.  May be repeated.
     * - Request Header Finishes indicating no more headers.
     * - Request Body which provides body data.  May be repeated.
     * - Request Finished indicating the end of the request.
     * - A similar sequence of events for the response.
     *
     * Each transaction is a single request and response.  A connection may
     * contain multiple transactions.
     */

    /* Request Started */
    {
        IronBee::ParsedRequestLine request_line =
            IronBee::ParsedRequestLine::create_alias(
                transaction.memory_pool(),
                request.raw_request_line.begin(),
                request.raw_request_line.size(),
                request.request_line.method.begin(),
                request.request_line.method.size(),
                request.request_line.uri.begin(),
                request.request_line.uri.size(),
                request.request_line.version.begin(),
                request.request_line.version.size()
            );
        m_engine.notify().request_started(transaction, request_line);
    }

    /* Request Header */
    {
        IronBee::psheader_to_parsed_header_const_range_t headers =
            IronBee::psheaders_to_parsed_headers(
                transaction.memory_pool(),
                request.headers.headers
            );

        m_engine.notify().request_header_data(
            transaction,
            headers.begin(), headers.end()
        );
    }

    /* Request Header Finished */
    m_engine.notify().request_header_finished(transaction);

    /* Request Body */
    m_engine.notify().request_body_data(
        transaction,
        request_body.begin(),
        request_body.size()
    );

    /* Request Finished */
    m_engine.notify().request_finished(transaction);

    /* Respone Started */
    {
        IronBee::ParsedResponseLine response_line =
            IronBee::ParsedResponseLine::create_alias(
                transaction.memory_pool(),
                response.raw_response_line.begin(),
                response.raw_response_line.size(),
                response.response_line.version.begin(),
                response.response_line.version.size(),
                response.response_line.status.begin(),
                response.response_line.status.size(),
                response.response_line.message.begin(),
                response.response_line.message.size()
            );
        m_engine.notify().response_started(transaction, response_line);
    }

    /* Response Headers */
    {
        IronBee::psheader_to_parsed_header_const_range_t headers =
            IronBee::psheaders_to_parsed_headers(
                transaction.memory_pool(),
                response.headers.headers
            );
        m_engine.notify().response_header_data(
            transaction,
            headers.begin(), headers.end()
        );
    }

    /* Response Header Finished */
    m_engine.notify().response_header_finished(transaction);

    /* Response Body */
    m_engine.notify().response_body_data(
        transaction,
        response_body.begin(),
        response_body.size()
    );

    /* Response Finished */
    m_engine.notify().response_finished(transaction);

    /* Transaction Done */
    transaction.destroy();

    /* Connection Closed */
    m_engine.notify().connection_closed(connection);

    /* Connection Done */
    connection.destroy();
}

void ExampleIronBee::on_close(
    IronBee::Connection  connection,
    IronBee::Transaction transaction
) const
{
    cout << "SERVER: CLOSE " << transaction.id() << endl;
}

void ExampleIronBee::on_error(
    IronBee::Transaction transaction,
    int                  status
) const
{
    cout << "SERVER: ERROR: " << transaction.id() << " " << status << endl;
}

void ExampleIronBee::on_error_header(
    IronBee::Transaction  transaction,
    const char           *name,
    size_t                name_length,
    const char           *value,
    size_t                value_length
) const
{
    cout << "SERVER: ERROR HEADER: "
         << transaction.id() << " "
         << string(name, name_length) << " "
         << string(value, value_length)
         << endl;
}

void ExampleIronBee::on_error_data(
    IronBee::Transaction transaction,
    const char *data,
    size_t      data_length
) const
{
    cout << "SERVER: ERROR DATA: "
         << transaction.id()
         << string(data, data_length)
         << endl;
}

void ExampleIronBee::on_header(
    IronBee::Transaction             transaction,
    IronBee::Server::direction_e     direction,
    IronBee::Server::header_action_e header_action,
    const char*                      name,
    size_t                           name_length,
    const char*                      value,
    size_t                           value_length
) const
{
    const char* header_action_string;
    switch (header_action) {
        case IronBee::Server::SET:
            header_action_string = "SET";
            break;
        case IronBee::Server::APPEND:
            header_action_string = "APPEND";
            break;
        case IronBee::Server::MERGE:
            header_action_string = "MERGE";
            break;
        case IronBee::Server::ADD:
            header_action_string = "ADD";
            break;
        case IronBee::Server::UNSET:
            header_action_string = "UNSET";
            break;
        case IronBee::Server::EDIT:
            header_action_string = "EDIT";
            break;
        default:
            header_action_string = "unknown";
    };

    cout << "SERVER: HEADER: "
         << transaction.id()
         << (direction == IronBee::Server::REQUEST ? "request" : "response")
         << header_action_string
         << string(name, name_length)
         << string(value, value_length)
         << endl;
}

namespace {

void load_file(vector<char>& to, const string& path)
{
    ifstream in(path.c_str());
    if (! in) {
        throw runtime_error("Could not read " + path);
    }
    size_t length;
    in.seekg(0, ios::end);
    length = in.tellg();
    in.seekg(0, ios::beg);
    to = vector<char>(length);
    in.read(&*to.begin(), length);
}

}
