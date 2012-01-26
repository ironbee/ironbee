#ifndef __IRONBEE__CPP__IRONBEE__
#define __IRONBEE__CPP__IRONBEE__

#include "input.hpp"

#include <ironbee/engine.h>
#include <ironbee/plugin.h>

#include <boost/function.hpp>
#include <boost/shared_ptr.hpp>

/**
 * \file ironbee.hpp
 * \brief Defines IronBee, a wrapper around IronBee.
 *
 * \sa IronBee
 **/

namespace IronBee {

/**
 * \class IronBee ironbee.hpp ironbee.hpp
 * \brief A wrapper around IronBee.
 *
 * This class sets up IronBee and provides methods for feeding it data.  It is
 * limited and, currently, focused on supporting \c ibcli_cpp.
 **/
class IronBee
{
public:
  IronBee();

  // Noncopyable.
  IronBee( const IronBee& ) = delete;
  IronBee& operator=( const IronBee& ) = delete;

  void load_config( const std::string& config_path );

  void open_connection(
    const buffer_t&    local_ip,
    uint16_t           local_port,
    const buffer_t&    remote_ip,
    uint16_t           remote_port
  );

  void open_connection( const input_t& input );

  void close_connection();

private:
  ib_plugin_t                    m_plugin;
  boost::shared_ptr<ib_engine_t> m_ironbee;
  boost::shared_ptr<ib_conn_t>   m_current_connection;
  // IronBee needs null terminated IPs, so we will need to format and store
  // them as such (they are provided as buffer_t's).
  std::string                    m_current_local_ip;
  std::string                    m_current_remote_ip;
};

} // IronBee

#endif
