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

#include <boost/algorithm/string/replace.hpp>
#include <boost/bind.hpp>
#include <boost/function.hpp>
#include <boost/iostreams/device/mapped_file.hpp>

#if defined(__cplusplus) && !defined(__STDC_FORMAT_MACROS)
/* C99 requires that inttypes.h only exposes PRI* macros
 * for C++ implementations if this is defined: */
#define __STDC_FORMAT_MACROS
#endif
#include <inttypes.h>

/**
 * Error Page Module implementation.
 */
class ErrorPageModule : public IronBee::ModuleDelegate
{
public:
    /**
     * Constructor.
     *
     * @param[in] module The IronBee++ module.
     */
    explicit ErrorPageModule(IronBee::Module module);

private:
    /**
     * A post-block hook to send the error page to the server.
     *
     * @param[in] tx The transaction.
     * @param[in] info The block info.
     **/
    void post_block(
        IronBee::Transaction   tx,
        const ib_block_info_t& info
    ) const;

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
        const char                   *param2
    );
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
    IronBee::ModuleDelegate(module)
{
    module.engine().register_block_post_hook(
        "ErrorPage",
        boost::bind(&ErrorPageModule::post_block, this, _1, _2)
    );

    /* Setup the directive callbacks. */
    module.engine().register_configuration_directives().
        param2(
            "ErrorPageMap",
            boost::bind(
                &ErrorPageModule::errorPageMapDirective,
                this, _1, _2, _3, _4));

    module.set_configuration_data<ErrorPageCtxConfig>();
}

void ErrorPageModule::errorPageMapDirective(
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

void ErrorPageModule::post_block(
    IronBee::Transaction   tx,
    const ib_block_info_t& info
) const
{
    ErrorPageCtxConfig &cfg =
        module().configuration_data<ErrorPageCtxConfig>(tx.context());

    status_to_file_map_t::iterator itr =
        cfg.status_to_file.find(info.status);

    /* If we can't find a mapping, decline. The default will take over. */
    if (itr == cfg.status_to_file.end()) {
        ib_log_debug2_tx(
            tx.ib(),
            "No custom page mapped for status %d and context %s. "
            "Declining.",
            info.status,
            tx.context().name()
        );
        return;
    }

    const std::string& file = itr->second;
    const boost::iostreams::mapped_file_source &source =
        cfg.status_to_mapped_file_source[info.status];

    /* The error page to be handed to the server is written to this
     * output string stream. */
    std::ostringstream error_page_output_stream;

    /* Optimization: Reserve the document size + 37 characters for a UUID.
     * We assume that the transaction ID is only replaced once. */
    error_page_output_stream.str().reserve(source.size() + 37);

    /* Replace holder text with transaction id. */
    boost::algorithm::replace_all_copy(
        /* Output iterator. */
        std::ostream_iterator<char>(error_page_output_stream),

        /* The range. */
        std::pair<const char *, const char *>(
            source.data(),
            source.data()+ source.size()
        ),

        /* The text to replace. */
        "${TRANSACTION_ID}",

        /* What to replace the text with. */
        tx.id()
    );

    ib_log_debug2_tx(
        tx.ib(),
        "Using custom error page file %.*s.",
        static_cast<int>(file.length()),
        file.data()
    );

    /* Report the error page back to the server. */
    ib_status_t rc = ib_server_error_body(
        ib_engine_server_get(tx.engine().ib()),
        tx.ib(),
        error_page_output_stream.str().data(),
        error_page_output_stream.str().size()
    );
    if ((rc == IB_DECLINED) || (rc == IB_ENOTIMPL)) {
        ib_log_debug2_tx(
            tx.ib(),
            "Server not willing to set HTTP error response data."
        );
    }
    else if (rc != IB_OK) {
        ib_log_notice_tx(
            tx.ib(),
            "Server failed to set HTTP error response data: %s",
            ib_status_to_string(rc)
        );
        IronBee::throw_if_error(rc);
    }
}
