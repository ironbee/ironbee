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
 * @brief IronBee --- Constant Module
 *
 * This module adds constants to IronBee.  Constants can be set at 
 * configuration time and used via a variety of APIs:
 *
 * - Rules can access constants via the `CONSTANT` var.  E.g., `CONSTANT:foo`.
 * - Configuration files can set constants via the `ConstantSet` directive.
 * - Other modules can access constants via ib_constant_get() and 
 *   ib_constant_set().
 *
 * The `ConstantSet` directive can be called in two ways:
 * - `ConstantSet key` sets the constant `key` to the empty string.  This is
 *   useful for sending "boolean" constants that are either true (defined) or
 *   false (not defined).
 * - `ConstantSet key value` sets the constant `key` to the string `value`.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include "constant.h"

#include <ironbeepp/all.hpp>

#include <ironbee/string.h>

#include <boost/bind.hpp>

using namespace std;
using namespace IronBee;

namespace {

//! Name of oracle variable.
const char* c_oracle_var = "CONSTANT";


/**
 * Map of constant key to constant value.
 *
 * We store lists of fields because that's what the oracle is required
 * to return and it can't allocate them on the fly because it doesn't
 * know an appropriate lifetime.
*/
typedef map<string, List<ConstField> > map_t;

class Delegate;

//! Per context data.
struct per_context_t
{
    //! Constructor.
    explicit
    per_context_t(Delegate* delegate_) : delegate(delegate_) {}
    
    //! Constructor.
    per_context_t() : delegate(NULL) {}
    
    //! Constants.  Note: Copied from parent by copy constructor.
    map_t constants;
    
    //! Delegate.  Used by external API.
    Delegate* delegate;
};

//! Delegate
class Delegate :
    public IronBee::ModuleDelegate
{
public:
    //! Constructor.
    explicit
    Delegate(IronBee::Module module);

    /**
     * Set a constant.
     *
     * @param[in] context Context of constant.
     * @param[in] value   Value; name will be used as key.
     * @throw
     * - einval if constant with given key already exists.
     * - bad_alloc on allocation failure.
     **/
    void set(Context context, ConstField value);
    
    /**
     * Get a constant.
     *
     * @note Returns a singular Field if no such constant rather than throwing
     *       enoent.
     *
     * @param[in] context    Context of constant.
     * @param[in] key        Key.
     * @param[in] key_length Length of @a key.
     * @returns Value if constant with given key exists and Field() if not.
     **/
    ConstField get(ConstContext context, const char* key, size_t key_length) const;
     
private:    
    //! Get per-context data for context.
    per_context_t& get_per_context(Context context);

    //! Get per-context data for context.
    const per_context_t& get_per_context(ConstContext context) const;
    
    //! Hook for context transaction event.  Setup Oracle.
    void on_context_transaction(IronBee::Transaction tx) const;
    
    /**
     * Get a dynamic field for accessing constants.
     *
     * @param[in] context Context of constants.
     * @param[in] mm      Memory manager to determine lifetime of field.
     * @returns Dynamic field for accessing constants of this context.
     **/
    Field oracle(Context context, MemoryManager mm) const;
    
    //! Oracle getter function; forwards to set().
    ConstList<ConstField> oracle_get(ConstContext context, const char* key, size_t key_length) const;
    //! Oracle setter function; throws IronBee::einval.
    void oracle_set() const;
    
    /**
     * Handle `ConstantSet` directive.
     *
     * @param[in] cp               Configuration parser.
     * @param[in] directive_name   Name of directive.  
     * @param[in] params           Parameters of directive.
     * @throw IronBee::einval on too few parameters or already existent set.
     **/
    void dir_set(
        IronBee::ConfigurationParser cp,
        const char*                  directive_name,
        IronBee::List<const char*>   params
    );
    
    //! Var source for oracle.
    VarSource m_oracle_source;
    
    //! An empty list to return for no-such-constant.
    ConstList<ConstField> m_empty_list;
};

} // Anonymous

IBPP_BOOTSTRAP_MODULE_DELEGATE("constant", Delegate)

// Implementation

// Reopen for doxygen; not needed by C++.
namespace {
    
Delegate::Delegate(IronBee::Module module) :
    IronBee::ModuleDelegate(module),
    m_empty_list(
        List<ConstField>::create(module.engine().main_memory_mm())
    )
{
    module.set_configuration_data<per_context_t>(per_context_t(this));
    module.engine().register_configuration_directives()
        .list(
            "ConstantSet",
            boost::bind(&Delegate::dir_set, this, _1, _2, _3)
        )
        ;

    module.engine().register_hooks()
        .handle_context_transaction(
            boost::bind(&Delegate::on_context_transaction, this, _2)
        )
        ;
    
    m_oracle_source = VarSource::register_(
        module.engine().var_config(),
        IB_S2SL(c_oracle_var),
        IB_PHASE_REQUEST_HEADER, IB_PHASE_REQUEST_HEADER
    );
}

void Delegate::on_context_transaction(IronBee::Transaction tx) const
{
    m_oracle_source.set(
        tx.var_store(), 
        oracle(tx.context(), tx.memory_manager())
    );
}

void Delegate::set(Context context, ConstField value)
{
    MemoryManager mm = module().engine().main_memory_mm();
    List<ConstField> list_value;
    
    map_t& constants = get_per_context(context).constants;
    const string key_s(value.name(), value.name_length());
    map_t::const_iterator i = constants.find(key_s);
    if (i != constants.end()) {
        BOOST_THROW_EXCEPTION(einval() << errinfo_what(
            "Constant " + key_s + " already exists."
        ));
    }
    
    list_value = List<ConstField>::create(mm);
    list_value.push_back(value);
    constants.insert(map_t::value_type(key_s, list_value));
}

ConstField Delegate::get(ConstContext context, const char* key, size_t key_length) const
{
    ConstList<ConstField> result = oracle_get(context, key, key_length);
    if (result.empty()) {
        return ConstField();
    }
    else {
        assert(result.size() == 1);
        return result.front();
    }
}
    
Field Delegate::oracle(Context context, MemoryManager mm) const
{
    return Field::create_dynamic_list<ConstField>(
        mm,
        IB_S2SL(c_oracle_var),
        boost::bind(&Delegate::oracle_get, this, context, _2, _3),
        boost::bind(&Delegate::oracle_set, this)
    );
}

ConstList<ConstField> Delegate::oracle_get(ConstContext context, const char* key, size_t key_length) const
{
    const map_t& constants = get_per_context(context).constants;
    map_t::const_iterator i = constants.find(string(key, key_length));
    if (i == constants.end()) {
        return m_empty_list;
    }
    return i->second;
}

void Delegate::oracle_set() const
{
    BOOST_THROW_EXCEPTION(
        einval() << errinfo_what("Can not set constants through oracle.")
    );
}

void Delegate::dir_set(
    IronBee::ConfigurationParser cp,
    const char*                  directive_name,
    List<const char*>            params
)
{
    const char* key;
    const char* value_string;
    MemoryManager mm = module().engine().main_memory_mm();
    
    if (params.size() < 1 || params.size() > 2) {
        ib_cfg_log_error(
            cp.ib(), 
            "%s takes 1 or 2 arguments; has %zd.",
            directive_name,
            params.size()
        );
        BOOST_THROW_EXCEPTION(einval());
    }
    
    List<const char*>::const_iterator i = params.begin();
    key = *i;
    ++i;
    if (i != params.end()) {
        value_string = *i;
    }
    else {
        value_string = "";
    }
    
    set(
        cp.current_context(), 
        Field::create_byte_string(
            mm, 
            IB_S2SL(key),
            ByteString::create(mm, value_string)
        )
    );
}

const per_context_t& Delegate::get_per_context(ConstContext context) const
{
    // Immediately adding appropriate const to result.
    return module().configuration_data<per_context_t>(
        Context::remove_const(context)
    );
}

per_context_t& Delegate::get_per_context(Context context)
{
    return module().configuration_data<per_context_t>(context);
}
    
} // Anonymous

extern "C" {
    
ib_status_t ib_module_constant_get(
    const ib_field_t** value, 
    const ib_context_t *ctx, 
    const char *key,
    size_t key_length
) 
{
    assert(value);
    assert(ctx);
    assert(key);
    
    ConstContext context(ctx);
    Module m = Module::with_name(context.engine(), "constant");
    if (! m) {
        return IB_EOTHER;
    }
    
    try {
        // We promise not to modify the data.  But need non-const context
        // to access it.
        *value = m.configuration_data<per_context_t>(
            Context::remove_const(context)
        ).delegate->get(context, key, key_length).ib();
    }
    catch (...) {
        return convert_exception();
    }    
        
    return IB_OK;
}

ib_status_t ib_module_constant_set(
    ib_context_t* ctx,
    const ib_field_t *value
)
{
    assert(ctx);
    assert(value);
    
    Context context(ctx);
    Module m = Module::with_name(context.engine(), "constant");
    if (! m) {
        return IB_EOTHER;
    }
    
    try {
        m.configuration_data<per_context_t>(context).delegate->set(
            context,
            ConstField(value)
        );
    }
    catch (...) {
        return convert_exception();
    }
        
    return IB_OK;
}

}

namespace IronBee {
namespace Constant {
    
void set(Context ctx, ConstField value)
{
    throw_if_error(ib_module_constant_set(ctx.ib(), value.ib()));
}

ConstField get(ConstContext ctx, const char* key, size_t key_length)
{
    const ib_field_t* result;
    
    throw_if_error(ib_module_constant_get(&result, ctx.ib(), key, key_length));
    
    return ConstField(result);
}

ConstField get(ConstContext ctx, const char* key)
{
    return get(ctx, IB_S2SL(key));
}

ConstField get(ConstContext ctx, ConstByteString key)
{
    return get(ctx, key.const_data(), key.size());
}

ConstField get(ConstContext ctx, const std::string& key)
{
    return get(ctx, key.data(), key.length());
}
    
}
}
