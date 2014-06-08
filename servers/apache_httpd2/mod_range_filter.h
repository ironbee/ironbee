#ifndef MOD_RANGE_FILTER_H
#define MOD_RANGE_FILTER_H

#include <httpd.h>
#include <apr_optional.h>

APR_DECLARE_OPTIONAL_FN(apr_status_t, range_substitute_in,
                            (request_rec*, apr_off_t, apr_size_t,
                             const char*, apr_size_t));
APR_DECLARE_OPTIONAL_FN(apr_status_t, range_substitute_out,
                            (request_rec*, apr_off_t, apr_size_t,
                             const char*, apr_size_t));

#endif
