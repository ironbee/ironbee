#include "ironbee.hpp"

#include <ironbee/config.h>

#include <boost/lexical_cast.hpp>

#include <stdexcept>
#include <string>

using namespace std;

namespace  {

void expect_ok( ib_status_t rc, const char* message )
{
  if ( rc != IB_OK ) {
    throw runtime_error(
      string( "Error (" ) + ib_status_to_string( rc ) + "): " + message
    );
  }
}

}

namespace IronBee {

IronBee::IronBee() :
  m_plugin {
    IB_PLUGIN_HEADER_DEFAULTS,
    "cpp_ironbee"
  }
{
  // Trace to stderr.
  ib_trace_init( NULL );

  // Initialize.
  expect_ok( ib_initialize(), "Initializing IronBee." );
  {
    ib_engine_t* engine;
    expect_ok( ib_engine_create( &engine, &m_plugin ), "Creating engine." );
    m_ironbee.reset( engine, &ib_engine_destroy );
  }
  expect_ok( ib_engine_init( m_ironbee.get() ), "Initializing engine." );
}

void IronBee::load_config( const std::string& config_path )
{
  boost::shared_ptr<ib_cfgparser_t> parser;

  expect_ok( ib_state_notify_cfg_started( m_ironbee.get() ),
    "Starting config."
  );
  {
    ib_cfgparser_t* cp;
    expect_ok( ib_cfgparser_create( &cp, m_ironbee.get() ),
      "Creating config parser."
    );
    parser.reset( cp, &ib_cfgparser_destroy );
  }

  expect_ok( ib_cfgparser_parse( parser.get(), config_path.c_str() ),
    "Parsing config file."
  );

  ib_state_notify_cfg_finished( m_ironbee.get() );
}

} // IronBee
