/*****************************************************************************
 * Licensed to Qualys, Inc. (QUALYS) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * QUALYS licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 ****************************************************************************/

/**
 * @file
 * @brief IronBee Modules --- Error Page
 *
 * The Error Page module allows the user to select a custom error page.
 *
 * @author Sam Baskinger <sbaskinger@qualys.com>
 */

#include "ironbee_config_auto.h"

#include <ironbeepp/configuration_directives.hpp>
#include <ironbeepp/configuration_parser.hpp>
#include <ironbeepp/c_trampoline.hpp>
#include <ironbeepp/module.hpp>
#include <ironbeepp/module_bootstrap.hpp>
#include <ironbeepp/module_delegate.hpp>
#include <ironbeepp/transaction.hpp>

#include <ironbee/path.h>
#include <ironbee/rule_engine.h>
#include <ironbee/string.h>

#include <boost/bind.hpp>
#include <boost/function.hpp>
#include <boost/iostreams/device/mapped_file.hpp>

/* Enable PRId64 printf. */
extern "C" {
#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif
#include <inttypes.h>
}

/**
 * Error Page Module implementation.
 */
class ErrorPageModule : public IronBee::ModuleDelegate
{
public:
    /**
     * A version of @ref ib_rule_error_page_fn_t without the callback data.
     *
     * This will be used by @ref IronBee::make_c_trampoline to replace
     * IronBee's default error page function.
     *
     * @param[in] tx The transaction.
     * @param[out] page A pointer to the error page.
     * @param[out] page_sz The length of @a page.
     */
    typedef ib_status_t(error_page_fn_t)(
        ib_tx_t        *tx,
        const uint8_t **page,
        size_t         *page_sz);
    /**
     * Constructor.
     *
     * @param[in] module The IronBee++ module.
     */
    explicit ErrorPageModule(IronBee::Module module);

private:

    /**
     * Member to store the C trampoline information.
     *
     * Note that m_trampoline_pair.second must be destroyed.
     */
    std::pair<ib_rule_error_page_fn_t, void*> m_trampoline_pair;

    /**
     * Holds callback data associated with @ref ib_rule_error_page_fn_t.
     *
     * This member ensures that IronBee::delete_c_trampoline is called.
     */
    boost::shared_ptr<void> m_trampoline_data;

    /**
     * Produce an error page for the given transaction.
     *
     * @param[in] tx The transaction.
     * @param[out] page The page data is placed here.
     * @param[out] page_sz The page size is placed here.
     *
     * @returns
     * - IB_OK On success.
     * - IB_DECLINED If the error page is not available but not because
     *               of an error.
     * - Other on error.
     */
    ib_status_t error_page_fn(
        ib_tx_t        *tx,
        const uint8_t **page,
        size_t         *page_sz);

    /**
     * Implement the HTTPStatusCodeContents directive.
     *
     * DEPRECATED: Use renamed errorPageMapDirective.
     *
     * @param[in] cp Configuration parser.
     * @param[in] name The directive.
     * @param[in] param1 The status code.
     * @param[in] param2 The error page file name.
     *
     * @throws @ref IronBee::exception.
     */
    void httpStatusCodeContentsDirective(
        IronBee::ConfigurationParser  cp,
        const char                   *name,
        const char                   *param1,
        const char                   *param2);

    /**
     * Implement the ErrorPageMap directive.
     *
     * @param[in] cp Configuration parser.
     * @param[in] name The directive.
     * @param[in] param1 The status code.
     * @param[in] param2 The error page file name.
     *
     * @throws @ref IronBee::exception.
     */
    void errorPageMapDirective(
        IronBee::ConfigurationParser  cp,
        const char                   *name,
        const char                   *param1,
        const char                   *param2);
};

IBPP_BOOTSTRAP_MODULE_DELEGATE("ErrorPageModule", ErrorPageModule);

/**
 * A map from the status number to a file name.
 */
typedef std::map<ib_num_t, std::string> status_to_file_map_t;

/**
 * Context configuration value for the ErrorPageModule.
 */
struct ErrorPageCtxConfig {

    /**
     * The mapping from an HTTP status code to the file to return.
     */
    status_to_file_map_t status_to_file;

    /**
     * The mapping from an HTTP status code to the memory mapped file.
     */
    std::map<ib_num_t, boost::iostreams::mapped_file_source>
        status_to_mapped_file_source;
};

/* Implementation */

ErrorPageModule::ErrorPageModule(IronBee::Module module):
    /* Initialize the parent. */
    IronBee::ModuleDelegate(module),

    /* Build the C callback for ib_rule_set_error_page_fn(). */
    m_trampoline_pair(
        IronBee::make_c_trampoline<error_page_fn_t>(
            boost::bind(&ErrorPageModule::error_page_fn, this, _1, _2, _3))),

    m_trampoline_data(
        m_trampoline_pair.second,
        IronBee::delete_c_trampoline)
{

    /* Register the callback with the IronBee engine. */
    ib_rule_set_error_page_fn(
        module.engine().ib(),
        m_trampoline_pair.first,
        m_trampoline_pair.second);

    /* Setup the directive callbacks. */
    // DEPRECATED: Use renamed ErrorPageMap instead.
    module.engine().register_configuration_directives().
        param2(
            "HttpStatusCodeContents",
            boost::bind(
                &ErrorPageModule::httpStatusCodeContentsDirective,
                this, _1, _2, _3, _4));

    module.engine().register_configuration_directives().
        param2(
            "ErrorPageMap",
            boost::bind(
                &ErrorPageModule::errorPageMapDirective,
                this, _1, _2, _3, _4));

    module.set_configuration_data<ErrorPageCtxConfig>();
}

void ErrorPageModule::httpStatusCodeContentsDirective(
        IronBee::ConfigurationParser  cp,
        const char                   *name,
        const char                   *param1,
        const char                   *param2)
{
    ib_num_t     num;

    ib_log_notice(
        cp.engine().ib(),
        "Use of %s is deprecated. Use renamed \"ErrorPageMap %s %s\" instead.",
        name, param1, param2);

    ErrorPageCtxConfig &cfg =
        module().configuration_data<ErrorPageCtxConfig>(cp.current_context());

    /* Convert the incoming parameter. */
    IronBee::throw_if_error(ib_string_to_num(param1, 10, &num));

    /* Set the mapping in the context configuration. */
    cfg.status_to_file[num] = ib_util_relative_file(
        ib_engine_mm_config_get(cp.engine().ib()),
        cp.current_file(),
        param2
    );

    try {
        cfg.status_to_mapped_file_source[num] =
            boost::iostreams::mapped_file_source(cfg.status_to_file[num]);
    }
    catch (const std::exception& e) {
        BOOST_THROW_EXCEPTION(
            IronBee::enoent()
                << IronBee::errinfo_what(e.what()));
    }
}

void ErrorPageModule::errorPageMapDirective(
        IronBee::ConfigurationParser  cp,
        const char                   *name,
        const char                   *param1,
        const char                   *param2)
{
    ib_num_t     num;

    ErrorPageCtxConfig &cfg =
        module().configuration_data<ErrorPageCtxConfig>(cp.current_context());

    /* Convert the incoming parameter. */
    IronBee::throw_if_error(ib_string_to_num(param1, 10, &num));

    /* Set the mapping in the context configuration. */
    cfg.status_to_file[num] = ib_util_relative_file(
        ib_engine_mm_config_get(cp.engine().ib()),
        cp.current_file(),
        param2
    );

    try {
        cfg.status_to_mapped_file_source[num] =
            boost::iostreams::mapped_file_source(cfg.status_to_file[num]);
    }
    catch (const std::exception& e) {
        BOOST_THROW_EXCEPTION(
            IronBee::enoent()
                << IronBee::errinfo_what(e.what()));
    }
}

ib_status_t ErrorPageModule::error_page_fn(
    ib_tx_t *tx,
    const uint8_t **page,
    size_t *len)
{
    IronBee::Transaction ib_tx(tx);

    ErrorPageCtxConfig &cfg =
        module().configuration_data<ErrorPageCtxConfig>(
            ib_tx.context());

    status_to_file_map_t::iterator itr =
        cfg.status_to_file.find(ib_tx.ib()->block_status);

    /* If we can't find a mapping, decline. The default will take over. */
    if (itr == cfg.status_to_file.end()) {
        ib_log_debug2_tx(
            ib_tx.ib(),
            "No custom page mapped for status %" PRId64 " and context %s. "
            "Declining.",
            ib_tx.ib()->block_status,
            ib_tx.context().name());
        return IB_DECLINED;
    }

    const std::string& file = itr->second;
    const boost::iostreams::mapped_file_source &source =
        cfg.status_to_mapped_file_source[tx->block_status];

    ib_log_debug2_tx(
        ib_tx.ib(),
        "Using custom error page file %.*s.",
        static_cast<int>(file.length()),
        file.data());

    *page = reinterpret_cast<const uint8_t*>(source.data());
    *len = source.size();

    return IB_OK;
}
