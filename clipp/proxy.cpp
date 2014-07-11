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
 * @brief IronBee --- CLIPP Proxy Consumer Implementation
 *
 * @author Craig Forbes <cforbes@qualys.com>
 */

#include "ironbee_config_auto.h"

#include <vector>
#include <sstream>

#include "proxy.hpp"

#include <boost/asio.hpp>
#include <boost/foreach.hpp>

using namespace std;
using boost::asio::ip::tcp;

namespace IronBee {
namespace CLIPP {

namespace {

using namespace Input;

class ProxyDelegate :
    public Delegate
{
public:
    stringstream to_origin;
    stringstream from_proxy;

    explicit
    ProxyDelegate(const std::string& proxy_ip, uint32_t proxy_port,
                 uint32_t listen_port)
        : m_client_sock(m_io_service),
          m_listener(m_io_service), m_origin_sock(m_io_service),
          m_proxy_ip(proxy_ip), m_proxy_port(proxy_port),
          m_listen_port(listen_port)
    {
        cout << "Creating proxy delegate" << endl;
    }

    void read_data(tcp::socket& sock, stringstream& rstream)
    {
        while (sock.available() > 0) {
            char data[8096];

            boost::system::error_code error;
            size_t length = sock.read_some(boost::asio::buffer(data), error);
            if (error == boost::asio::error::eof)
                break; // Connection closed cleanly by peer.
            else if (error)
                throw boost::system::system_error(error); // Some other error.

            rstream.write(data, length);
        }
    }

    void connection_opened(const ConnectionEvent& event)
    {
        cout << __func__ << endl;
        tcp::endpoint origin_endpoint(tcp::v4(), m_listen_port);
        m_listener.open(origin_endpoint.protocol());
        m_listener.set_option(boost::asio::socket_base::reuse_address(true));
        m_listener.bind(origin_endpoint);
        m_listener.listen();

        tcp::endpoint proxy_endpoint(
            boost::asio::ip::address::from_string(m_proxy_ip), m_proxy_port);
        m_client_sock.connect(proxy_endpoint);
    }

    void connection_closed(const NullEvent& event)
    {
        cout << __func__ << endl;
        read_data(m_client_sock, from_proxy);
        m_client_sock.close();
        m_origin_sock.close();
    }

    void request_started(const RequestEvent& event)
    {
        cout << __func__ << endl;
        boost::asio::write(m_client_sock, boost::asio::buffer(event.raw.data,
                                                         event.raw.length));
        boost::asio::write(m_client_sock, boost::asio::buffer("\r\n", 2));
    }

    void request_header(const HeaderEvent& event)
    {
        cout << __func__ << endl;
        boost::asio::streambuf b;
        std::ostream out(&b);
        BOOST_FOREACH(const header_t& header, event.headers) {
            out << header.first.data
                << ": "
                << header.second.data
                << "\r\n";
        }
        out << "\r\n";

        boost::asio::write(m_client_sock, b);
    }
    void request_body(const DataEvent& event)
    {
        cout << __func__ << endl;
        boost::asio::write(m_client_sock, boost::asio::buffer(event.data.data,
                                                         event.data.length));
    }

    void request_finished(NullEvent& event)
    {
        cout << __func__ << endl;
    }

    void response_started(const ResponseEvent& event)
    {
        cout << __func__ << endl;
        m_listener.accept(m_origin_sock);

        read_data(m_origin_sock, to_origin);

        boost::asio::streambuf b;
        std::ostream out(&b);
        out << event.raw.data << "\r\n";
        boost::asio::write(m_origin_sock, b);
    }

    void response_header(const HeaderEvent& event)
    {
        cout << __func__ << endl;
        boost::asio::streambuf b;
        std::ostream out(&b);
        BOOST_FOREACH(const header_t& header, event.headers) {
            out << header.first.data << ": " << header.second.data
                << "\r\n";
        }
        out << "\r\n";
        boost::asio::write(m_origin_sock, b);
    }

    void response_body(const DataEvent& event)
    {
        cout << __func__ << endl;
        boost::asio::write(m_origin_sock,
                           boost::asio::buffer(event.data.data,
                                               event.data.length));
    }

    void response_finished(const NullEvent& event)
    {
        cout << __func__ << endl;
    }

private:
    typedef boost::shared_ptr<tcp::socket> socket_ptr;
    boost::asio::io_service m_io_service;
    std::string m_proxy_ip;
    uint32_t m_proxy_port;
    uint32_t m_listen_port;
    tcp::socket m_client_sock;
    tcp::socket m_origin_sock;
    tcp::acceptor m_listener;
};

} // anonymous namespace

ProxyConsumer::ProxyConsumer(const std::string& proxy_host,
                             uint32_t proxy_port,
                             uint32_t listen_port)
    : m_proxy_host(proxy_host),
      m_proxy_port(proxy_port),
      m_listen_port(listen_port)
{
    // nop
}

bool ProxyConsumer::operator()(const input_p& input)
{
    if ( ! input ) {
        return true;
    }

    cout << "Starting proxy" << endl;
    ProxyDelegate proxyer(m_proxy_host, m_proxy_port, m_listen_port);
    input->connection.dispatch(proxyer);
    cout << "Proxy Done" << endl;
    cout << "TO ORIGIN>" << proxyer.to_origin.str() << "<" << endl;
    cout << "From proxy>" << proxyer.from_proxy.str() << "<" << endl;
    return true;
}

} // CLIPP
} // IronBee
