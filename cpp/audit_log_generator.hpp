#ifndef __IRONBEE_CPP__AUDIT_LOG_GENERATOR__
#define __IRONBEE_CPP__AUDIT_LOG_GENERATOR__

#include "input.hpp"
#include "audit_log.hpp"

#include <boost/shared_ptr.hpp>
#include <boost/function.hpp>

#include <iostream>
#include <string>

namespace IronBee {
namespace CLI {

/**
 * \class AuditLogGenerator
 * \brief Input generator from modsec audit logs.
 *
 * Produces input_t's from an modsec audit log.  This uses
 * IronBee::AuditLog::Parser to parse the audit log.  It requires that the
 * audit log provide sections B and F.
 **/
class AuditLogGenerator
{
public:
  //! Default Constructor.
  /**
   * Behavior except for assinging to is undefined.
   **/
  AuditLogGenerator() = default;

  //! Type of on_error.  See AuditLogGenerator()
  using on_error_t = boost::function<bool(const std::string&)>;

  //! Constructor.
  /**
   * \param[in] path     Path to audit log.
   * \param[in] on_error Function to call if an error occurs.  Messae will be
   *                     passed in.  If returns true, generator will try to
   *                     recover, otherwise generator will stop parsing.  If
   *                     default, then generator will throw exception on
   *                     error.
   **/
  explicit
  AuditLogGenerator(
    const std::string& path,
    on_error_t on_error = on_error_t()
  );

  //! Produce an input.  See input_t and input_generator_t.
  bool operator()( input_t& out_input );

private:
  on_error_t                      m_on_error;
  boost::shared_ptr<std::istream> m_input;
  AuditLog::Parser                m_parser;
};

} // CLI
} // IronBee

#endif
