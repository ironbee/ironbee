#ifndef __IRONBEE__CPP__INPUT__
#define __IRONBEE__CPP__INPUT__

/**
 * \file input.hpp
 * \brief Defines the inputs the CLI will give to IronBee.
 *
 * Defines input_t, the fundamental unit of input the CLI will give to
 * IronBee and related structures: buffer_t (a copyless substring) and
 * input_generator_t (a generic producer of input).
 **/

#include <boost/function.hpp>

#include <string>
#include <vector>
#include <iostream>

namespace IronBee {

/**
 * \class buffer_t
 * \brief Simple representation of memory buffer.
 *
 * This structure is a data pointer and length.  It's primary use is to refer
 * to substrings without copying.
 **/
struct buffer_t
{
  //! Default constructor.
  buffer_t() = default;

  //! Constructor.
  /**
   * \param[in] data   Pointer to buffer.  Not necessarily null terminated.
   * \param[in] length Length of buffer.
   **/
  buffer_t( const char* data_, size_t length_ );

  //! Construct from string.
  /**
   * The parameter \a s will need to outlive this buffer_t.
   *
   * \param[in] s String to initialize buffer from.
   **/
  explicit
  buffer_t( const std::string& s );

  //! Convert to string.  Makes a copy.
  std::string to_s() const;

  //! Pointer to buffer.  Not necessarily null terminated.
  const char* data;
  //! Length of buffer.
  size_t      length;
};

std::ostream& operator<<( std::ostream& out, const buffer_t& buffer );

/**
 * \class input_t
 * \brief An input for IronBee to process.
 *
 * This structure represents the basic input for IronBee.  It is connection
 * information and 0 or more transactions.
 **/
struct input_t
{
  //! Local IP address.  Must outlive input_t.
  buffer_t local_ip;
  //! Local port.
  uint16_t local_port;

  //! Remote IP address.  Must outlive input_t.
  buffer_t remote_ip;
  //! Remote port.
  uint16_t remote_port;

  /**
   * \class input_t::transaction_t
   * \brief A transaction for IronBee to process.
   **/
  struct transaction_t {
    //! Constructor.
    transaction_t( buffer_t request_, buffer_t response_ );

    //! Request data.  Must outlive input_t.
    buffer_t request;
    //! Response data.  Must outlive input_t.
    buffer_t response;
  };

  //! Zero or more transactions.
  std::vector<transaction_t> transactions;
};

//! Ostream output operator for an input.
std::ostream& operator<<( std::ostream& out, const input_t& input );

//! A generator of inputs.
/**
 * Should be a function that takes an input_t as an output argument.  If
 * input is available it should fill its argument and return true.  If no
 * more input is available, it should return false.
 *
 * It should not make any assumptions about the existing value of its
 * argument, i.e., it should set every field.
 **/
using input_generator_t = boost::function<bool(input_t&)>;

} // IronBee

#endif
