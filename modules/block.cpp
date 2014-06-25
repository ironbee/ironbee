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
 * @brief IronBee --- Block Module
 *
 * This module register a block handler and provides directives for
 * configuration it.
 *
 * - `BlockStatus` directive: Sets what status number to use for status
 *    method.
 * - `BlockMethod` directive: Set whether to use status or close blocking
 *    method by default.
 *
 * The block handler also knows to use the close method, regardless of
 * configuration, if the response line has already been sent to the server.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <ironbeepp/all.hpp>

#include <boost/bind.hpp>

using boost::bind;
using namespace std;

namespace {

//! Module delegate.
class Delegate :
    public IronBee::ModuleDelegate
{
public:
    //! Constructor.
    explicit
    Delegate(IronBee::Module module);

private:
    //! Handle BlockStatus directive.
    void dir_block_status(
        IronBee::ConfigurationParser cp,
        const char*                  directive_name,
        const char*                  status
    ) const;

    //! Handle BlockMethod directive.
    void dir_block_method(
        IronBee::ConfigurationParser cp,
        const char*                  directive_name,
        const char*                  method
    ) const;

    //! Block handler.
    void block_handler(
        IronBee::Transaction tx,
        ib_block_info_t&     block_info
    ) const;
};

} // Anonymous namespace

IBPP_BOOTSTRAP_MODULE_DELEGATE("block", Delegate)

// Implementation
namespace {

Delegate::Delegate(IronBee::Module module) :
    IronBee::ModuleDelegate(module)
{
    ib_block_info_t default_per_context;
    default_per_context.status = 403;
    default_per_context.method = IB_BLOCK_METHOD_STATUS;

    module.set_configuration_data<ib_block_info_t>(default_per_context);

    module.engine().register_configuration_directives()
        .param1(
            "BlockStatus",
            bind(&Delegate::dir_block_status, this, _1, _2, _3)
        )
        .param1(
            "BlockMethod",
            bind(&Delegate::dir_block_method, this, _1, _2, _3)
        )
        ;

    module.engine().register_block_handler(
        "Block Module",
        boost::bind(&Delegate::block_handler, this, _1, _2)
    );
}

void Delegate::dir_block_status(
    IronBee::ConfigurationParser cp,
    const char*                  directive_name,
    const char*                  status
) const
{
    ib_block_info_t& per_context =
        module().configuration_data<ib_block_info_t>(cp.current_context());
    try {
        per_context.status = boost::lexical_cast<int>(status);
    }
    catch (const boost::bad_lexical_cast&) {
        BOOST_THROW_EXCEPTION(
            IronBee::einval() << IronBee::errinfo_what(
                string("Could not convert ") +  status + " to integer."
            )
        );
    }
}

void Delegate::dir_block_method(
    IronBee::ConfigurationParser cp,
    const char*                  directive_name,
    const char*                  method
) const
{
    ib_block_info_t& per_context =
        module().configuration_data<ib_block_info_t>(cp.current_context());
    string method_s(method);
    if (method_s == "close") {
        per_context.method = IB_BLOCK_METHOD_CLOSE;
    }
    else if (method_s == "status") {
        per_context.method = IB_BLOCK_METHOD_STATUS;
    }
    else {
        BOOST_THROW_EXCEPTION(
            IronBee::einval() << IronBee::errinfo_what(
                string("Invalid block method: ") + method
            )
        );
    }
}

void Delegate::block_handler(
    IronBee::Transaction tx,
    ib_block_info_t&     info
) const
{
    // Note by copy.
    info = module().configuration_data<ib_block_info_t>(tx.context());

    if (tx.is_response_line()) {
        info.method = IB_BLOCK_METHOD_CLOSE;
    }
}

} // Anonymous
