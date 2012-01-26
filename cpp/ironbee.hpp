#ifndef __IRONBEE__CPP__IRONBEE__
#define __IRONBEE__CPP__IRONBEE__

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

private:
  ib_plugin_t                    m_plugin;
  boost::shared_ptr<ib_engine_t> m_ironbee;
};

} // IronBee

#endif
