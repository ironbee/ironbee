#include "input.hpp"

namespace IronBee {

std::ostream& operator<<( std::ostream& out, const buffer_t& buffer )
{
  out << std::string( buffer.data, buffer.length );
  return out;
}

std::ostream& operator<<( std::ostream& out, const input_t& input )
{
  out << input.src_ip << " " << input.src_port << " -> "
      << input.dst_ip << " " << input.dst_port << ": "
      << "#transactions=" << input.transactions.size()
      ;
  return out;
}

buffer_t::buffer_t( const char* data_, size_t length_ ) :
  data( data_ ), length( length_ )
{
  // nop
}

buffer_t::buffer_t( const std::string& s ) :
  buffer_t( s.c_str(), s.length() )
{
  // nop
}

std::string buffer_t::to_s() const
{
  return std::string( data, length );
}

input_t::transaction_t::transaction_t(
  buffer_t request_,
  buffer_t response_
) :
  request( request_ ), response( response_ )
{
  // nop
}

} // IronBee
