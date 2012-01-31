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
  class Connection
  {
    friend class IronBee;
  public:
    Connection( const Connection& ) = delete;
    Connection& operator=( const Connection& ) = delete;

    // Calls close().
    ~Connection();

    void close();

    // This copies data in because IronBee expects to be able to modify the
    // buffers you give it.
    void data_in(  const buffer_t& data );
    void data_out( const buffer_t& data );

  private:
    Connection(
      IronBee&           ib,
      const buffer_t&    local_ip,
      uint16_t           local_port,
      const buffer_t&    remote_ip,
      uint16_t           remote_port
    );

    IronBee&                       m_ib;
    boost::shared_ptr<ib_conn_t>   m_connection;
    // IronBee needs null terminated IPs, so we will need to format and store
    // them as such (they are provided as buffer_t's).
    std::string                    m_local_ip;
    std::string                    m_remote_ip;
  };
  friend class Connection;
  using connection_p = boost::shared_ptr<Connection>;

  IronBee();

  // Noncopyable.
  IronBee( const IronBee& ) = delete;
  IronBee& operator=( const IronBee& ) = delete;

  void load_config( const std::string& config_path );

  connection_p open_connection(
    const buffer_t&    local_ip,
    uint16_t           local_port,
    const buffer_t&    remote_ip,
    uint16_t           remote_port
  );

  connection_p open_connection( const input_t& input );

private:
  ib_plugin_t                    m_plugin;
  boost::shared_ptr<ib_engine_t> m_ironbee;
};

} // IronBee

#endif
