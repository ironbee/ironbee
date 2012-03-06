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
    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;

    // Calls close().
    ~Connection();

    //! Close the connection.  Do not use data_in() or data_out() after this.
    void close();

    // This copies data in because IronBee expects to be able to modify the
    // buffers you give it.

    //! Send local->remote \a data to ironbee.
    /**
     * \param[in] data Data to send to IronBee.  Copied in.
     * \throw runtime_error on any error.
     **/
    void data_in(const buffer_t& data);

    //! Send remote->local \a data to ironbee.
    /**
     * \param[in] data Data to send to IronBee.  Copied in.
     * \throw runtime_error on any error.
     **/
    void data_out(const buffer_t& data);

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

  //! Pointer to a connection.
  using connection_p = boost::shared_ptr<Connection>;

  //! Constructor.
  /**
   * This will initialize the IronBee library and create an IronBee engine.
   *
   * Currently it sets trace output to stderr.
   *
   * \todo Allow alternative trace outputs.
   * \todo Move library initialization to separate code so that it is called
   *       once only.
   **/
  IronBee();

  // Noncopyable.
  IronBee(const IronBee&) = delete;
  IronBee& operator=(const IronBee&) = delete;

  //! Load a config file.
  /**
   * \param[in] config_path Path to file to load.
   * \throw runtime_error on any error.
   **/
  void load_config(const std::string& config_path);

  //! Open a connection.
  /**
   * \param[in] local_ip    Local IP address.
   * \param[in] local_port  Local port.
   * \param[in] remote_ip   Remote IP address.
   * \param[in] remote_port Remote port.
   *
   * \return Connection.
   **/
  connection_p open_connection(
    const buffer_t&    local_ip,
    uint16_t           local_port,
    const buffer_t&    remote_ip,
    uint16_t           remote_port
  );

  //! As above, but using an input_t for IPs and ports.
  /**
   * This does *not* feed the transaction in.  Use Connection::data_in().
   *
   * \param[in] input Input with connection information.
   * \return Connection.
   **/
  connection_p open_connection(const input_t& input);

private:
  ib_plugin_t                    m_plugin;
  boost::shared_ptr<ib_engine_t> m_ironbee;
};

} // IronBee

#endif
