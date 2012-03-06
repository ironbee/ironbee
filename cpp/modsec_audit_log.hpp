#ifndef __IRONBEE__CPP__MODSEC_AUDIT_LOG__
#define __IRONBEE__CPP__MODSEC_AUDIT_LOG__

#include <iostream>
#include <vector>
#include <map>
#include <string>

namespace IronBee {
namespace ModSecAuditLog {
/**
 * \class Entry
 * \brief An AuditLog entry.
 *
 * Represents an audit log entry.  Contains a buffer of each section.
 *
 * Format documented at: http://www.modsecurity.org/documentation/
 *
 * \sa Parser.
 **/
class Entry
{
  friend class Parser;

public:
  //! Access section \a section.
  /**
   * \param[in] section Section of audit log to return.
   * \return Section text as string.
   **/
  const std::string& operator[](const std::string& section) const;

  //! Clear entry.
  void clear();

private:
  std::map<std::string,std::string> m_sections;
};

/**
 * \class Parser
 * \brief Audit log parser.
 *
 * This class implements an audit log parser.  To use it, call operator()()
 * repeatedly.  If there is an error, you can use recover() to attempt to
 * recover from it.
 *
 * \sa Entry
 **/
class Parser
{
public:
  //! Constructor.
  /**
   * Any data in the input stream before the first A boundary is ignored.
   *
   * \param[in,out] in The input stream to parse.
   **/
  explicit
  Parser(std::istream& in);

  //! Fetch next entry.
  /**
   * Fetches the next entry from the input stream.  If there are no more
   * entries, it will return false.  If there is a parsing error, it will
   * throw a runtime_exception.  If an exception is thrown, its behavior for
   * future calls is undefined unless recover() is called.
   *
   * \param[out] out_entry Where to write the next entry.
   * \return true iff another entry was found.
   *
   * \throw runtime_exception on parse error.
   **/
  bool operator()(Entry& out_entry);

  //! Recover from an error.
  /**
   * This routines attempts to recover from a parsing error by looking for the
   * next A boundary.  This typically means that the entry the parse error
   * occurred on is discarded.  After this call, whether successful or not,
   * operator()() can be used again.
   *
   * \return true iff recovery was possible.  If false, then all future calls
   * to operator()() or recover() will also return false.
   **/
  bool recover();

private:
  std::istream& m_in;
  std::string   m_section;
  std::string   m_boundary;
  bool          m_have_entry;
};

} // AuditLog
} // IronBee

#endif
