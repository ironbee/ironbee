#include "audit_log.hpp"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wparentheses"
#pragma clang diagnostic ignored "-Wchar-subscripts"
#pragma clang diagnostic ignored "-Wunused-function"
#pragma clang diagnostic ignored "-Wdelete-non-virtual-dtor"
#include <boost/regex.hpp>
#pragma clang diagnostic pop

using namespace std;

namespace IronBee {
namespace AuditLog {

const std::string& Entry::operator[](const std::string& section) const
{
  auto i = m_sections.find( section );
  if ( i == m_sections.end() ) {
    throw runtime_error( "No such section: " + section );
  }
  return i->second;
}

void Entry::clear()
{
  m_sections.clear();
}

namespace  {

static boost::regex re_boundary("^--([0-9a-z]+)-([A-Z])--$");

}

Parser::Parser( std::istream& in ) :
  m_in( in )
{
  // Find first entry.
  m_have_entry = recover();
}

bool Parser::operator()( Entry& out_entry )
{
  if ( ! m_have_entry ) {
    return false;
  }

  // We can now assume we are at the beginning of an entry, just after the
  // A boundary, and m_section and m_boundary are properly set.

  out_entry.clear();
  out_entry.m_sections["A"] = "";

  boost::smatch match;
  string        line;
  string        b;
  string        s;

  while ( m_in.good() ) {
    getline( m_in, line );
    if ( regex_match( line, match, re_boundary ) ) {
      b = match[1];
      s = match[2];
      if ( b != m_boundary || s == "A" ) {
        // new record
        m_boundary = b;
        m_section  = s;
        return true;
      }
      if ( out_entry.m_sections.count( s ) != 0 ) {
        throw runtime_error(
          "Duplicate section " + s + " for boundary " + b + "."
        );
      }
      out_entry.m_sections[s] = "";
      m_section = s;
    } else if ( ! m_section.empty() ) {
      out_entry.m_sections[m_section] += line + "\n";
    } else if ( ! line.empty() ) {
      throw runtime_error(
        "Data found outside of section: " + line
      );
    }
  }

  // Out of input.  We have an entry to return, but won't for the next call.
  m_have_entry = false;
  return true;
}

bool Parser::recover()
{
  boost::smatch match;
  string        line;
  string        b;
  string        s;

  while ( m_in.good() ) {
    getline( m_in, line );
    if ( regex_match( line, match, re_boundary ) ) {
      b = match[1];
      s = match[2];
      if ( s == "A" ) {
        m_boundary   = b;
        m_section    = s;
        m_have_entry = true;
        return true;
      }
    }
  }
  return false;
}

} // AuditLog
} // IronBee
