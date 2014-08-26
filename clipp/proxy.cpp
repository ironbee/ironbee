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

#include <sstream>

#include "proxy.hpp"

#include <boost/algorithm/string/replace.hpp>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/foreach.hpp>
#include <boost/scoped_array.hpp>

using namespace std;
using boost::asio::ip::tcp;

namespace IronBee {
namespace CLIPP {

namespace {

using namespace Input;

void accept_handler(const boost::system::error_code& e)
{
    if (e) {
        cout << "Error in accept handler" << endl;
        exit(1);
    }
}

class ProxyDelegate :
    public Delegate
{
public:
    stringstream to_origin;
    stringstream from_proxy;

    ProxyDelegate(const std::string& proxy_ip, uint16_t proxy_port,
                  uint16_t listen_port)
        : m_proxy_ip(proxy_ip), m_proxy_port(proxy_port), 
          m_listen_port(listen_port),
          m_client_sock(m_io_service), m_origin_sock(m_io_service),
          m_listener(m_io_service)
    {
        // nop
    }

    void read_data(tcp::socket& sock, stringstream& rstream, int timeout=5)
    {
        for (int i=0; i < (timeout * 10); ++i) {
            if (sock.available() > 0) {
                break;
            }
            usleep(100000);
        }

        while (sock.available() > 0) {
            boost::scoped_array<char> data(new char[8192]);
            boost::system::error_code error;

            size_t length = sock.read_some(
                boost::asio::buffer(data.get(), 8192), error);
            if (error == boost::asio::error::eof) {
                break; // Connection closed cleanly by peer.
            }
            else if (error) {
                BOOST_THROW_EXCEPTION(
                    boost::system::system_error(error)
                );
            }
            rstream.write(data.get(), length);
        }
    }

    void connection_opened(const ConnectionEvent& event)
    {
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
        read_data(m_client_sock, from_proxy);
        if (m_origin_sock.is_open()) {
            m_origin_sock.close();
        }
        m_client_sock.close();
    }

    void request_started(const RequestEvent& event)
    {
        boost::asio::write(m_client_sock,
                           boost::asio::buffer(event.raw.data,
                                               event.raw.length));
        boost::asio::write(m_client_sock, boost::asio::buffer("\r\n", 2));
    }

    void request_header(const HeaderEvent& event)
    {
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
        boost::asio::write(m_client_sock,
                           boost::asio::buffer(event.data.data,
                                               event.data.length));
    }

    void request_finished(const NullEvent& event)
    {
        // This event may not occur, so do no work here.
    }

    void response_started(const ResponseEvent& event)
    {
        m_listener.async_accept(m_origin_sock,
            boost::bind(&accept_handler, boost::asio::placeholders::error));
        // Retry for 5 seconds in 0.1 second intervals
        for (int i=0; i < 50 ; ++i) {
            m_io_service.poll();
            if (m_origin_sock.is_open()) {
                break;
            }
            usleep(100000);
        }

        if (m_origin_sock.is_open()) {
            read_data(m_origin_sock, to_origin);

            boost::asio::streambuf b;
            std::ostream out(&b);
            out << event.raw.data << "\r\n";
            boost::asio::write(m_origin_sock, b);
        }
        else {
            to_origin << "[ERROR: Failed Accepting Connection]";
        }
    }

    void response_header(const HeaderEvent& event)
    {
        boost::asio::streambuf b;
        std::ostream out(&b);
        BOOST_FOREACH(const header_t& header, event.headers) {
            out << header.first.data
                << ": "
                << header.second.data
                << "\r\n";
        }
        out << "\r\n";
        if (m_origin_sock.is_open()) {
            boost::asio::write(m_origin_sock, b);
        }
    }

    void response_body(const DataEvent& event)
    {
        if (m_origin_sock.is_open()) {
            boost::asio::write(m_origin_sock,
                               boost::asio::buffer(event.data.data,
                                                   event.data.length));
        }
    }

    void response_finished(const NullEvent& event)
    {
        // This event may not occur, so do no work here.
    }

private:
    typedef boost::shared_ptr<tcp::socket> socket_ptr;
    boost::asio::io_service m_io_service;
    std::string m_proxy_ip;
    uint16_t m_proxy_port;
    uint16_t m_listen_port;
    tcp::socket m_client_sock;
    tcp::socket m_origin_sock;
    tcp::acceptor m_listener;
};

} // anonymous namespace

ProxyConsumer::ProxyConsumer(const std::string& proxy_host,
                             uint16_t proxy_port,
                             uint16_t listen_port)
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

    ProxyDelegate proxyer(m_proxy_host, m_proxy_port, m_listen_port);
    input->connection.dispatch(proxyer);

    cout << "Connection Id:" << input->id << endl;
    string outstr = proxyer.to_origin.str();
    boost::algorithm::replace_all(outstr, "\\", "\\\\");
    boost::algorithm::replace_all(outstr, "\n", "\\n");
    boost::algorithm::replace_all(outstr, "\r", "\\r");
    cout << "[" << input->id << "] Origin Request:" << outstr << endl;

    outstr = proxyer.from_proxy.str();
    boost::algorithm::replace_all(outstr, "\\", "\\\\");
    boost::algorithm::replace_all(outstr, "\n", "\\n");
    boost::algorithm::replace_all(outstr, "\r", "\\r");
    cout << "[" << input->id << "] Proxy Response:" << outstr << endl;

    return true;
}

} // CLIPP
} // IronBee
