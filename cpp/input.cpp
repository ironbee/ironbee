#include "input.hpp"

namespace IronBee {
namespace CLI {

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

} // CLI
} // IronBee
