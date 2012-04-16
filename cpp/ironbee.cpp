#include "ironbee.hpp"

#include <ironbee/config.h>
#include <ironbee/mpool.h>
#include <ironbee/release.h>
#include <ironbee/debug.h>
#include <ironbee/util.h>
#include <ironbee/state_notify.h>

#include <boost/lexical_cast.hpp>

#include <stdexcept>
#include <string>

using namespace std;

namespace  {

void expect_ok(ib_status_t rc, const char* message)
{
  if (rc != IB_OK) {
    throw runtime_error(
      string("Error (") + ib_status_to_string(rc) + "): " + message
    );
  }
}

ib_conndata_t* buffer_to_conndata(
  ib_conn_t*               conn,
  const IronBee::buffer_t& buffer
)
{
  ib_conndata_t* conndata;
  // We hope IronBee eventually destroys this...
  expect_ok(ib_conn_data_create(conn, &conndata, buffer.length),
    "Allocating connection data."
  );
  conndata->dlen = buffer.length;
  std::copy(
    buffer.data, buffer.data + buffer.length,
    reinterpret_cast<char*>(conndata->data)
  );

  return conndata;
}

}

namespace IronBee {

IronBee::IronBee() :
  m_server {
    IB_SERVER_HEADER_DEFAULTS,
    "cpp_ironbee"
  }
{
  // Trace to stderr.
  ib_trace_init(NULL);

  // Initialize.
  expect_ok(ib_initialize(), "Initializing IronBee.");
  {
    ib_engine_t* engine;
    expect_ok(ib_engine_create(&engine, &m_server), "Creating engine.");
    m_ironbee.reset(engine, &ib_engine_destroy);
  }
  expect_ok(ib_engine_init(m_ironbee.get()), "Initializing engine.");
}

void IronBee::load_config(const std::string& config_path)
{
  boost::shared_ptr<ib_cfgparser_t> parser;

  expect_ok(ib_state_notify_cfg_started(m_ironbee.get()),
    "Starting config."
  );
  {
    ib_cfgparser_t* cp;
    expect_ok(ib_cfgparser_create(&cp, m_ironbee.get()),
      "Creating config parser."
    );
    parser.reset(cp, &ib_cfgparser_destroy);
  }

  expect_ok(ib_cfgparser_parse(parser.get(), config_path.c_str()),
    "Parsing config file."
  );

  ib_state_notify_cfg_finished(m_ironbee.get());

  // Do some sanity checks.
  auto ctx = ib_context_main(m_ironbee.get());
  if (! ctx) {
    throw runtime_error("IronBee has no main context.");
  }
  if (ib_context_get_engine(ctx) != m_ironbee.get()) {
    throw runtime_error("IronBee has corrupt context.");
  }
}

IronBee::connection_p IronBee::open_connection(
  const buffer_t&    local_ip,
  uint16_t           local_port,
  const buffer_t&    remote_ip,
  uint16_t           remote_port
)
{
  return IronBee::connection_p(new Connection(
    *this,
    local_ip,
    local_port,
    remote_ip,
    remote_port
  ));
}

IronBee::connection_p IronBee::open_connection(const input_t& input)
{
  return open_connection(
    input.local_ip,
    input.local_port,
    input.remote_ip,
    input.remote_port
  );
}

IronBee::Connection::Connection(
  IronBee&           ib,
  const buffer_t&    local_ip,
  uint16_t           local_port,
  const buffer_t&    remote_ip,
  uint16_t           remote_port
) :
  m_ib(ib)
{
  {
    ib_conn_t* conn;
    expect_ok(ib_conn_create(m_ib.m_ironbee.get(), &conn, NULL),
      "Creating connection."
    );
    // This will destroy any existing current connection.
    m_connection.reset(conn, &ib_conn_destroy);
  }

  m_local_ip                 = local_ip.to_s();
  m_remote_ip                = remote_ip.to_s();
  m_connection->local_ipstr  = m_local_ip.c_str();
  m_connection->local_port   = local_port;
  m_connection->remote_ipstr = m_remote_ip.c_str();
  m_connection->remote_port  = remote_port;

  expect_ok(
    ib_state_notify_conn_opened(
      m_ib.m_ironbee.get(),
      m_connection.get()
    ),
    "Opening connection."
  );
}

IronBee::Connection::~Connection()
{
  if (m_connection) {
    close();
  }
}

void IronBee::Connection::close()
{
  expect_ok(
    ib_state_notify_conn_closed(m_ib.m_ironbee.get(), m_connection.get()),
    "Closing connection."
  );
  m_connection.reset();
}

void IronBee::Connection::data_in(const buffer_t& data)
{
  auto conndata = buffer_to_conndata(
    m_connection.get(),
    data
  );

  expect_ok(
    ib_state_notify_conn_data_in(
        m_ib.m_ironbee.get(),
        conndata,
        NULL
    ),
    "Connection data in."
  );
}

void IronBee::Connection::data_out(const buffer_t& data)
{
  auto conndata = buffer_to_conndata(
    m_connection.get(),
    data
  );

  expect_ok(
    ib_state_notify_conn_data_out(
      m_ib.m_ironbee.get(),
      conndata,
      NULL
    ),
    "Connection data in."
  );
}

} // IronBee
