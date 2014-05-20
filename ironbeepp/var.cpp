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
 * @brief IronBee++ --- Var Implementation
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <ironbeepp/var.hpp>

#include <ironbee/var.h>

namespace IronBee {

// VarConfig

// ConstVarConfig

ConstVarConfig::ConstVarConfig() :
    m_ib(NULL)
{
    // nop
}

ConstVarConfig::ConstVarConfig(ib_type ib_var_config) :
    m_ib(ib_var_config)
{
    // nop
}

MemoryManager ConstVarConfig::memory_manager() const
{
    return MemoryManager(ib_var_config_mm(ib()));
}

// VarConfig

VarConfig VarConfig::remove_const(ConstVarConfig var)
{
    return VarConfig(const_cast<ib_type>(var.ib()));
}

VarConfig::VarConfig() :
    m_ib(NULL)
{
    // nop
}

VarConfig::VarConfig(ib_type ib_var_config) :
    ConstVarConfig(ib_var_config),
    m_ib(ib_var_config)
{
    // nop
}

VarConfig VarConfig::acquire(MemoryManager mm)
{
    ib_var_config_t* var_config;

    throw_if_error(
        ib_var_config_acquire(&var_config, mm.ib()),
        "Error acquiring var config."
    );

    return VarConfig(var_config);
}

std::ostream& operator<<(std::ostream& o, const ConstVarConfig& var_config)
{
    if (! var_config) {
        o << "IronBee::VarConfig[!singular!]";
    } else {
        o << "IronBee::VarConfig["
          << reinterpret_cast<const void *>(var_config.ib())
          << "]";
    }
    return o;
}

// VarStore

// ConstVarStore

ConstVarStore::ConstVarStore() :
    m_ib(NULL)
{
    // nop
}

ConstVarStore::ConstVarStore(ib_type ib_var_store) :
    m_ib(ib_var_store)
{
    // nop
}

MemoryManager ConstVarStore::memory_manager() const
{
    return MemoryManager(ib_var_store_mm(ib()));
}

ConstVarConfig ConstVarStore::config() const
{
    return ConstVarConfig(ib_var_store_config(ib()));
}

// VarStore

VarStore VarStore::remove_const(ConstVarStore var)
{
    return VarStore(const_cast<ib_type>(var.ib()));
}

VarStore::VarStore() :
    m_ib(NULL)
{
    // nop
}

VarStore::VarStore(ib_type ib_var_store) :
    ConstVarStore(ib_var_store),
    m_ib(ib_var_store)
{
    // nop
}

VarStore VarStore::acquire(MemoryManager mm, ConstVarConfig config)
{
    ib_var_store_t* var_store;

    throw_if_error(
        ib_var_store_acquire(&var_store, mm.ib(), config.ib()),
        "Error acquiring var store."
    );

    return VarStore(var_store);
}

void VarStore::export_(List<Field> to) const
{
    ib_var_store_export(ib(), to.ib());
}

std::ostream& operator<<(std::ostream& o, const ConstVarStore& var_store)
{
    if (! var_store) {
        o << "IronBee::VarStore[!singular!]";
    } else {
        o << "IronBee::VarStore["
          << reinterpret_cast<const void *>(var_store.ib())
          << "]";
    }
    return o;
}

// VarSource

// ConstVarSource

ConstVarSource::ConstVarSource() :
    m_ib(NULL)
{
    // nop
}

ConstVarSource::ConstVarSource(ib_type ib_var_source) :
    m_ib(ib_var_source)
{
    // nop
}

ConstVarConfig ConstVarSource::config() const
{
    return ConstVarConfig(ib_var_source_config(ib()));
}

std::string ConstVarSource::name_s() const
{
    std::pair<const char*, size_t> result = name();
    return std::string(result.first, result.second);
}

std::pair<const char*, size_t> ConstVarSource::name() const
{
    const char* s;
    size_t s_len;

    ib_var_source_name(ib(), &s, &s_len);

    return std::make_pair(s, s_len);
}

ib_rule_phase_num_t ConstVarSource::initial_phase() const
{
    return ib_var_source_initial_phase(ib());
}

ib_rule_phase_num_t ConstVarSource::final_phase() const
{
    return ib_var_source_final_phase(ib());
}

bool ConstVarSource::is_indexed() const
{
    return ib_var_source_is_indexed(ib());
}

ConstField ConstVarSource::get(ConstVarStore var_store) const
{
    const ib_field_t* f;

    throw_if_error(
        ib_var_source_get_const(ib(), &f, var_store.ib()),
        "Error getting value from const var source."
    );

    return ConstField(f);
}

// VarSource

VarSource VarSource::remove_const(ConstVarSource var)
{
    return VarSource(const_cast<ib_type>(var.ib()));
}

VarSource::VarSource() :
    m_ib(NULL)
{
    // nop
}

VarSource::VarSource(ib_type ib_var_source) :
    ConstVarSource(ib_var_source),
    m_ib(ib_var_source)
{
    // nop
}

VarSource VarSource::register_(
    VarConfig            config,
    const char          *name,
    size_t               name_length,
    ib_rule_phase_num_t  initial_phase,
    ib_rule_phase_num_t  final_phase
)
{
    ib_var_source_t* source;

    throw_if_error(
        ib_var_source_register(
            &source,
            config.ib(),
            name, name_length,
            initial_phase, final_phase
        ),
        "Error registering source."
    );

    return VarSource(source);
}

VarSource VarSource::register_(
    VarConfig           config,
    const std::string&  name,
    ib_rule_phase_num_t initial_phase,
    ib_rule_phase_num_t final_phase
)
{
    return register_(
        config,
        name.data(), name.length(),
        initial_phase, final_phase
    );
}

VarSource VarSource::acquire(
    MemoryManager      mm,
    ConstVarConfig  config,
    const char     *name,
    size_t          name_length
)
{
    ib_var_source_t* source;

    throw_if_error(
        ib_var_source_acquire(
            &source,
            mm.ib(),
            config.ib(),
            name, name_length
        ),
        "Error acquiring var source."
    );

    return VarSource(source);
}

VarSource VarSource::acquire(
    MemoryManager         mm,
    ConstVarConfig     config,
    const std::string& name
)
{
    return acquire(mm, config, name.data(), name.length());
}

Field VarSource::get(VarStore store) const
{
    ib_field_t* f;

    throw_if_error(
        ib_var_source_get(ib(), &f, store.ib()),
        "Error getting value of var source."
    );

    return Field(f);
}

void VarSource::set(VarStore store, Field value) const
{
    throw_if_error(
        ib_var_source_set(ib(), store.ib(), value.ib()),
        "Error setting var source."
    );
}

Field VarSource::initialize(VarStore store, ConstField::type_e type) const
{
    ib_field_t* f;
    throw_if_error(
        ib_var_source_initialize(
            ib(),
            &f,
            store.ib(),
            static_cast<ib_ftype_t>(type)
        ),
        "Error initializing source."
    );

    return Field(f);
}

void VarSource::append(VarStore store, Field value) const
{
    throw_if_error(
        ib_var_source_append(ib(), store.ib(), value.ib()),
        "Error appending to var source."
    );
}

std::ostream& operator<<(std::ostream& o, const ConstVarSource& var_source)
{
    if (! var_source) {
        o << "IronBee::VarSource[!singular!]";
    } else {
        o << "IronBee::VarSource[" << var_source.name_s() << "]";
    }
    return o;
}

// VarFilter

// ConstVarFilter

ConstVarFilter::ConstVarFilter() :
    m_ib(NULL)
{
    // nop
}

ConstVarFilter::ConstVarFilter(ib_type ib_var_filter) :
    m_ib(ib_var_filter)
{
    // nop
}

ConstList<ConstField> ConstVarFilter::apply(
    MemoryManager mm,
    Field      field
) const
{
    const ib_list_t* l;

    throw_if_error(
        ib_var_filter_apply(ib(), &l, mm.ib(), field.ib()),
        "Error applying var filter."
    );

    return ConstList<ConstField>(l);
}

List<ConstField> ConstVarFilter::remove(
    MemoryManager mm,
    Field      field
) const
{
    ib_list_t* l;

    throw_if_error(
        ib_var_filter_remove(ib(), &l, mm.ib(), field.ib()),
        "Error removing with var filter."
    );

    return List<ConstField>(l);
}

void ConstVarFilter::remove_without_result(
    MemoryManager mm,
    Field      field
) const
{
    throw_if_error(
        ib_var_filter_remove(ib(), NULL, mm.ib(), field.ib()),
        "Error removing with var filter."
    );
}

// VarFilter

VarFilter VarFilter::remove_const(ConstVarFilter var)
{
    return VarFilter(const_cast<ib_type>(var.ib()));
}

VarFilter::VarFilter() :
    m_ib(NULL)
{
    // nop
}

VarFilter::VarFilter(ib_type ib_var_filter) :
    ConstVarFilter(ib_var_filter),
    m_ib(ib_var_filter)
{
    // nop
}

VarFilter VarFilter::acquire(
    MemoryManager  mm,
    const char* filter_string,
    size_t      filter_string_length
)
{
    ib_var_filter_t* filter;

    throw_if_error(
        ib_var_filter_acquire(
            &filter,
            mm.ib(),
            filter_string, filter_string_length
        ),
        "Error acquiring var filter."
    );

    return VarFilter(filter);
}

VarFilter VarFilter::acquire(
    MemoryManager         mm,
    const std::string& filter_string
)
{
    return acquire(mm, filter_string.data(), filter_string.length());
}


std::ostream& operator<<(std::ostream& o, const ConstVarFilter& var_filter)
{
    if (! var_filter) {
        o << "IronBee::VarFilter[!singular!]";
    } else {
        o << "IronBee::VarFilter["
            << reinterpret_cast<const void *>(var_filter.ib())
            << "]";
    }
    return o;
}

// VarTarget

// ConstVarTarget

ConstVarTarget::ConstVarTarget() :
    m_ib(NULL)
{
    // nop
}

ConstVarTarget::ConstVarTarget(ib_type ib_var_target) :
    m_ib(ib_var_target)
{
    // nop
}

ConstList<ConstField> ConstVarTarget::get(
    MemoryManager    mm,
    ConstVarStore var_store
) const
{
    const ib_list_t* l;

    throw_if_error(
        ib_var_target_get_const(ib(), &l, mm.ib(), var_store.ib()),
        "Error getting value of const target."
    );

    return ConstList<ConstField>(l);
}

ConstVarTarget ConstVarTarget::expand(
    MemoryManager    mm,
    ConstVarStore var_store
) const
{
    const ib_var_target_t* target;

    throw_if_error(
        ib_var_target_expand_const(
            ib(),
            &target,
            mm.ib(),
            var_store.ib()
        ),
        "Error expanding const target."
    );

    return ConstVarTarget(target);
}

// VarTarget

VarTarget VarTarget::remove_const(ConstVarTarget var)
{
    return VarTarget(const_cast<ib_type>(var.ib()));
}

VarTarget::VarTarget() :
    m_ib(NULL)
{
    // nop
}

VarTarget::VarTarget(ib_type ib_var_target) :
    ConstVarTarget(ib_var_target),
    m_ib(ib_var_target)
{
    // nop
}


VarTarget VarTarget::acquire(
    MemoryManager     mm,
    VarSource      source,
    ConstVarExpand expand,
    ConstVarFilter filter
)
{
    ib_var_target_t* target;

    throw_if_error(
        ib_var_target_acquire(
            &target,
            mm.ib(),
            source.ib(),
            expand.ib(),
            filter.ib()
        ),
        "Error acquiring var target."
    );

    return VarTarget(target);
}

VarTarget VarTarget::acquire_from_string(
    MemoryManager  mm,
    VarConfig   var_config,
    const char* target_string,
    size_t      target_string_length
)
{
    ib_var_target_t* target;

    throw_if_error(
        ib_var_target_acquire_from_string(
            &target,
            mm.ib(),
            var_config.ib(),
            target_string, target_string_length
        ),
        "Error acquiring var target from string."
    );

    return VarTarget(target);
}

VarTarget VarTarget::acquire_from_string(
    MemoryManager         mm,
    VarConfig          var_config,
    const std::string &target_string
)
{
    return acquire_from_string(
        mm,
        var_config,
        target_string.data(), target_string.length()
    );
}

ConstList<Field> VarTarget::get(MemoryManager mm, VarStore var_store) const
{
    const ib_list_t* l;

    throw_if_error(
        ib_var_target_get(ib(), &l, mm.ib(), var_store.ib()),
        "Error getting value of var target."
    );

    return ConstList<Field>(l);
}

List<Field> VarTarget::remove(MemoryManager mm, VarStore var_store) const
{
    ib_list_t* l;

    throw_if_error(
        ib_var_target_remove(ib(), &l, mm.ib(), var_store.ib()),
        "Error removing values of var target."
    );

    return List<Field>(l);
}

void VarTarget::remove_without_result(
    MemoryManager mm,
    VarStore   var_store
) const
{
    throw_if_error(
        ib_var_target_remove(ib(), NULL, mm.ib(), var_store.ib()),
        "Error removing values of var target."
    );
}

VarTarget VarTarget::expand(MemoryManager mm, ConstVarStore var_store) const
{
    ib_var_target_t* target;

    throw_if_error(
        ib_var_target_expand(ib(), &target, mm.ib(), var_store.ib()),
        "Error expanding var target."
    );

    return VarTarget(target);
}

void VarTarget::set(MemoryManager mm, VarStore var_store, Field field) const
{
    throw_if_error(
        ib_var_target_set(ib(), mm.ib(), var_store.ib(), field.ib()),
        "Error setting var target value."
    );
}

void VarTarget::remove_and_set(
    MemoryManager mm,
    VarStore   var_store,
    Field      field
) const
{
    throw_if_error(
        ib_var_target_remove_and_set(
            ib(),
            mm.ib(),
            var_store.ib(),
            field.ib()
        ),
        "Error remove-and-setting var target value."
    );
}

std::ostream& operator<<(std::ostream& o, const ConstVarTarget& var_target)
{
    if (! var_target) {
        o << "IronBee::VarTarget[!singular!]";
    } else {
        o << "IronBee::VarTarget["
          << reinterpret_cast<const void *>(var_target.ib())
          << "]";
    }
    return o;
}

// VarExpand

// ConstVarExpand

ConstVarExpand::ConstVarExpand() :
    m_ib(NULL)
{
    // nop
}

ConstVarExpand::ConstVarExpand(ib_type ib_var_target) :
    m_ib(ib_var_target)
{
    // nop
}

std::pair<const char*, size_t> ConstVarExpand::execute(
    MemoryManager mm,
    VarStore   var_store
) const
{
    const char* s;
    size_t s_length;

    throw_if_error(
        ib_var_expand_execute(ib(), &s, &s_length, mm.ib(), var_store.ib()),
        "Error executing var expand."
    );

    return std::make_pair(s, s_length);
}

std::string ConstVarExpand::execute_s(
    MemoryManager mm,
    VarStore   var_store
) const
{
    std::pair<const char*, size_t> result = execute(mm, var_store);
    return std::string(result.first, result.second);
}

bool ConstVarExpand::test(const char* str, size_t str_length)
{
    return ib_var_expand_test(str, str_length);
}

bool ConstVarExpand::test(const std::string &s)
{
    return test(s.data(), s.length());
}

// VarExpand

VarExpand VarExpand::remove_const(ConstVarExpand var)
{
    return VarExpand(const_cast<ib_type>(var.ib()));
}

VarExpand::VarExpand() :
    m_ib(NULL)
{
    // nop
}

VarExpand::VarExpand(ib_type ib_var_target) :
    ConstVarExpand(ib_var_target),
    m_ib(ib_var_target)
{
    // nop
}

VarExpand VarExpand::acquire(
    MemoryManager mm,
    const char*   str,
    size_t        str_length,
    VarConfig     config
)
{
    ib_var_expand_t* expand;

    throw_if_error(
        ib_var_expand_acquire(
            &expand,
            mm.ib(),
            str, str_length,
            config.ib()
        ),
        "Error acquiring var expand."
    );

    return VarExpand(expand);
}

VarExpand VarExpand::acquire(
    MemoryManager      mm,
    const std::string& s,
    VarConfig          config
)
{
    return acquire(mm, s.data(), s.length(), config);
}

std::ostream& operator<<(std::ostream& o, const ConstVarExpand& var_expand)
{
    if (! var_expand) {
        o << "IronBee::VarExpand[!singular!]";
    } else {
        o << "IronBee::VarExpand["
          << reinterpret_cast<const void *>(var_expand.ib())
          << "]";
    }
    return o;
}

} // IronBee
