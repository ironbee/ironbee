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

#include <ironbee/rule_engine.h>

#include <ironbeepp/c_trampoline.hpp>
#include <ironbeepp/configuration_directives.hpp>
#include <ironbeepp/configuration_parser.hpp>
#include <ironbeepp/module.hpp>
#include <ironbeepp/module_bootstrap.hpp>
#include <ironbeepp/module_delegate.hpp>
#include <ironbeepp/transaction.hpp>

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
 * Error Page Module implementaiton.
 */
class ErrorPageModule : public IronBee::ModuleDelegate
{
public:
    typedef ib_status_t(error_page_fn_t)(
        ib_tx_t        *tx,
        const uint8_t **page,
        size_t         *page_sz);
    /**
     * Constructor.
     * @param[in] module The IronBee++ module.
     */
    explicit ErrorPageModule(IronBee::Module module);

    ~ErrorPageModule();

private:

    /**
     * Memeber to store the C trampoline information.
     *
     * Note that m_trampoline_pair.second must be destroyed.
     */
    std::pair<ib_rule_error_page_fn_t, void*> m_trampoline_pair;

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

    void directive(
        IronBee::ConfigurationParser  cp,
        const char                   *name,
        const char                   *param1,
        const char                   *param2);
};

IBPP_BOOTSTRAP_MODULE_DELEGATE("ErrorPageModule", ErrorPageModule);

/**
 * Context configuration value for the ErrorPageModule.
 */
struct ErrorPageCtxConfig {
    /**
     * The mapping from an HTTP status code to the file to return.
     */
    std::map<ib_num_t, std::string> status_to_file;

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
            boost::bind(&ErrorPageModule::error_page_fn, this, _1, _2, _3)))
{
    /* Register the callback with the IronBee engine. */
    ib_rule_set_error_page_fn(
        module.engine().ib(),
        m_trampoline_pair.first,
        m_trampoline_pair.second);

    /* Setup the directive callbacks. */
    module.engine().register_configuration_directives().
        param2(
            "HttpStatusCodeContents",
            boost::bind(
                &ErrorPageModule::directive, this, _1, _2, _3, _4));

    module.set_configuration_data<ErrorPageCtxConfig>();
}

ErrorPageModule::~ErrorPageModule() {
    IronBee::delete_c_trampoline(m_trampoline_pair.second);
}

void ErrorPageModule::directive(
        IronBee::ConfigurationParser  cp,
        const char                   *name,
        const char                   *param1,
        const char                   *param2)
{
    ib_num_t num;

    ErrorPageCtxConfig &cfg =
        module().configuration_data<ErrorPageCtxConfig>(cp.current_context());

    /* Convert the incoming parameter. */
    IronBee::throw_if_error(ib_string_to_num(param1, 10, &num));

    /* Set the mapping in the context configuration. */
    cfg.status_to_file[num] = std::string(param2);

    try {
        cfg.status_to_mapped_file_source[num] =
            boost::iostreams::mapped_file_source(param2);
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
    assert(tx->block_method == IB_BLOCK_METHOD_STATUS);

    IronBee::Transaction txpp(tx);

    ErrorPageCtxConfig &cfg =
        module().configuration_data<ErrorPageCtxConfig>(
            txpp.context());

    ib_log_debug_tx(
        tx,
        "Returning custom error page with status %" PRId64 " for context %s.",
        tx->block_status,
        txpp.context().name());


    std::map<ib_num_t, std::string>::iterator itr =
        cfg.status_to_file.find(tx->block_status);

    /* If we can't find a mapping, decline. The default will take over. */
    if (itr == cfg.status_to_file.end()) {
        ib_log_debug_tx(
            tx,
            "No custom page mapped for status %" PRId64 " and context %s. "
            "Declining.",
            tx->block_status,
            txpp.context().name());
        return IB_DECLINED;
    }

    const std::string& file = itr->second;
    const boost::iostreams::mapped_file_source &source =
        cfg.status_to_mapped_file_source[tx->block_status];

    ib_log_debug_tx(
        tx,
        "Using custom error page file %.*s.",
        static_cast<int>(file.length()),
        file.data());

    *page = reinterpret_cast<const uint8_t*>(source.data());
    *len = source.size();

    /* Until we write this, decline the action. */
    return IB_OK;
}
