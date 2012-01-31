#ifndef __IRONBEE_CPP__RAW_GENERATOR__
#define __IRONBEE_CPP__RAW_GENERATOR__

#include "input.hpp"

#include <string>
#include <vector>

namespace IronBee {
namespace CLI {

/**
 * \class RawGenerator
 * \brief Input generator from a request/response pair of files.
 *
 * Will use bogus connection information.
 *
 * This produces a single input.
 **/
class RawGenerator
{
public:
  //! Local IP address to use for raw inputs.
  static const std::string local_ip;
  //! Remote IP address to use for raw inputs.
  static const std::string remote_ip;
  //! Remote port to use for raw inputs.
  static const uint16_t    local_port;
  //! Remote port to use for raw inputs.
  static const uint16_t    remote_port;

  //! Default Constructor.
  /**
   * Behavior except for assinging to is undefined.
   **/
  RawGenerator() = default;

  //! Constructor.
  /**
   * \param[in] request_path  Path to request data.
   * \param[in] response_path Path to request data.
   **/
  RawGenerator(
    const std::string& request_path,
    const std::string& response_path
  );

  //! Produce an input.  See input_t and input_generator_t.
  bool operator()( input_t& out_input );

private:
  bool               m_produced_input;
  std::vector<char>  m_request_buffer;
  std::vector<char>  m_response_buffer;
};

} // CLI
} // IronBee

#endif
