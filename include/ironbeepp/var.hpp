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
 * @brief IronBee++ --- Var
 *
 * This file defines var objects, a wrapper for @ref IronBeeEngineVar.
 *
 * @remark Developers should be familiar with @ref ironbeepp to understand
 * aspects of this code, e.g., the public/non-virtual inheritance.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#ifndef __IBPP__VAR__
#define __IBPP__VAR__

#include <ironbeepp/common_semantics.hpp>
#include <ironbeepp/field.hpp>

#include <ironbee/rule_defs.h> // ib_rule_phase_num_t

#include <ostream>

// IronBee C Type
typedef struct ib_var_config_t ib_var_config_t;
typedef struct ib_var_store_t ib_var_store_t;
typedef struct ib_var_source_t ib_var_source_t;
typedef struct ib_var_filter_t ib_var_filter_t;
typedef struct ib_var_target_t ib_var_target_t;
typedef struct ib_var_expand_t ib_var_expand_t;

namespace IronBee {

class MemoryManager;
template <typename T> class List;
template <typename T> class ConstList;

class ConstVarExpand;

// VarConfig

/**
 * Const VarConfig; equivalent to a const pointer to @ref ib_var_config_t.
 *
 * Provides operators ==, !=, <, >, <=, >= and evaluation as a boolean for
 * singularity via CommonSemantics.
 *
 * See @ref IronBeeEngineVarConfiguration.
 *
 * @sa Var
 * @sa ironbeepp
 * @sa ib_var_config_t
 * @nosubgrouping
 **/
class ConstVarConfig :
    public CommonSemantics<ConstVarConfig>
{
public:
    //! C Type.
    typedef const ib_var_config_t* ib_type;

    /**
     * Construct singular ConstVarConfig.
     *
     * All behavior of a singular ConstVarConfig is undefined except for
     * assignment, copying, comparison, and evaluate-as-bool.
     **/
    ConstVarConfig();

    /**
     * @name C Interoperability
     * Methods to access underlying C types.
     **/
    ///@{

    //! const ib_var_config_t accessor.
    // Intentionally inlined.
    ib_type ib() const
    {
        return m_ib;
    }

    //! Construct Var from ib_var_config_t.
    explicit
    ConstVarConfig(ib_type ib_var_config);

    ///@}

    //! Access memory manager.
    MemoryManager memory_manager() const;

private:
    ib_type m_ib;
};

/**
 * VarConfig; equivalent to a pointer to ib_var_config_t.
 *
 * VarConfig can be treated as ConstVarConfig.  See @ref ironbeepp for
 * details on IronBee++ object semantics.
 *
 * See @ref IronBeeEngineVarConfiguration.
 *
 * @sa ConstVarConfig
 * @sa ironbeepp
 * @sa ib_var_config_t
 * @nosubgrouping
 **/
class VarConfig :
    public ConstVarConfig
{
public:
    //! C Type.
    typedef ib_var_config_t* ib_type;

    /**
     * Remove the constness of a ConstVarConfig.
     *
     * @warning This is as dangerous as a @c const_cast, use carefully.
     *
     * @param[in] var ConstVarConfig to remove const from.
     * @returns Var pointing to same underlying var as @a var.
     **/
    static VarConfig remove_const(ConstVarConfig var);

    /**
     * Construct singular VarConfig.
     *
     * All behavior of a singular VarConfig is undefined except for
     * assignment, copying, comparison, and evaluate-as-bool.
     **/
    VarConfig();

    /**
     * @name C Interoperability
     * Methods to access underlying C types.
     **/
    ///@{

    //! ib_var_config_t accessor.
    ib_type ib() const
    {
        return m_ib;
    }

    //! Construct VarConfig from ib_var_config_t.
    explicit
    VarConfig(ib_type ib_var_config);

    ///@}

    //! See ib_var_config_acquire().
    static VarConfig acquire(MemoryManager mm);

private:
    ib_type m_ib;
};

/**
 * Output operator for VarConfig.
 *
 * Output IronBee::VarConfig[@e value] where @e value is based on the
 * underlying pointer.
 *
 * @param[in] o Ostream to output to.
 * @param[in] var_config VarConfig to output.
 * @return @a o
 **/
std::ostream& operator<<(std::ostream& o, const ConstVarConfig& var_config);

// VarStore

/**
 * Const VarStore; equivalent to a const pointer to @ref ib_var_store_t.
 *
 * Provides operators ==, !=, <, >, <=, >= and evaluation as a boolean for
 * singularity via CommonSemantics.
 *
 * See @ref IronBeeEngineVarStore.
 *
 * @sa Var
 * @sa ironbeepp
 * @sa ib_var_store_t
 * @nosubgrouping
 **/
class ConstVarStore :
    public CommonSemantics<ConstVarStore>
{
public:
    //! C Type.
    typedef const ib_var_store_t* ib_type;

    /**
     * Construct singular ConstVarStore.
     *
     * All behavior of a singular ConstVarStore is undefined except for
     * assignment, copying, comparison, and evaluate-as-bool.
     **/
    ConstVarStore();

    /**
     * @name C Interoperability
     * Methods to access underlying C types.
     **/
    ///@{

    //! const ib_var_store_t accessor.
    // Intentionally inlined.
    ib_type ib() const
    {
        return m_ib;
    }

    //! Construct Var from ib_var_store_t.
    explicit
    ConstVarStore(ib_type ib_var_store);

    ///@}

    //! Access memory mm.
    MemoryManager memory_manager() const;

    //! Access var config.
    ConstVarConfig config() const;

private:
    ib_type m_ib;
};

/**
 * VarStore; equivalent to a pointer to ib_var_store_t.
 *
 * VarStore can be treated as ConstVarStore.  See @ref ironbeepp for
 * details on IronBee++ object semantics.
 *
 * See @ref IronBeeEngineVarStore.
 *
 * @sa ConstVarStore
 * @sa ironbeepp
 * @sa ib_var_store_t
 * @nosubgrouping
 **/
class VarStore :
    public ConstVarStore
{
public:
    //! C Type.
    typedef ib_var_store_t* ib_type;

    /**
     * Remove the constness of a ConstVarStore.
     *
     * @warning This is as dangerous as a @c const_cast, use carefully.
     *
     * @param[in] var ConstVarStore to remove const from.
     * @returns Var pointing to same underlying var as @a var.
     **/
    static VarStore remove_const(ConstVarStore var);

    /**
     * Construct singular VarStore.
     *
     * All behavior of a singular VarStore is undefined except for
     * assignment, copying, comparison, and evaluate-as-bool.
     **/
    VarStore();

    /**
     * @name C Interoperability
     * Methods to access underlying C types.
     **/
    ///@{

    //! ib_var_store_t accessor.
    ib_type ib() const
    {
        return m_ib;
    }

    //! Construct VarStore from ib_var_store_t.
    explicit
    VarStore(ib_type ib_var_store);

    ///@}

    //! See ib_var_config_acquire().
    static VarStore acquire(MemoryManager mm, ConstVarConfig config);

    //! See ib_var_store_export().
    // "export" is a keyword.
    void export_(List<Field> to) const;

private:
    ib_type m_ib;
};

/**
 * Output operator for VarStore.
 *
 * Output IronBee::VarStore[@e value] where @e value is based on the
 * underlying pointer.
 *
 * @param[in] o Ostream to output to.
 * @param[in] var_store VarStore to output.
 * @return @a o
 **/
std::ostream& operator<<(std::ostream& o, const ConstVarStore& var_store);

// VarSource

/**
 * Const VarSource; equivalent to a const pointer to @ref ib_var_source_t.
 *
 * Provides operators ==, !=, <, >, <=, >= and evaluation as a boolean for
 * singularity via CommonSemantics.
 *
 * See @ref IronBeeEngineVarSource.
 *
 * @sa Var
 * @sa ironbeepp
 * @sa ib_var_source_t
 * @nosubgrouping
 **/
class ConstVarSource :
    public CommonSemantics<ConstVarSource>
{
public:
    //! C Type.
    typedef const ib_var_source_t* ib_type;

    /**
     * Construct singular ConstVarSource.
     *
     * All behavior of a singular ConstVarSource is undefined except for
     * assignment, copying, comparison, and evaluate-as-bool.
     **/
    ConstVarSource();

    /**
     * @name C Interoperability
     * Methods to access underlying C types.
     **/
    ///@{

    //! const ib_var_source_t accessor.
    // Intentionally inlined.
    ib_type ib() const
    {
        return m_ib;
    }

    //! Construct Var from ib_var_source_t.
    explicit
    ConstVarSource(ib_type ib_var_source);

    ///@}

    //! Access var config.
    ConstVarConfig config() const;

    //! Access name.
    std::string name_s() const;

    //! Access name without copy.
    std::pair<const char *, size_t> name() const;

    //! Access initial phase.
    ib_rule_phase_num_t initial_phase() const;

    //! Access final phase.
    ib_rule_phase_num_t final_phase() const;

    //! Access is_indexed.
    bool is_indexed() const;

    //! See ib_var_source_get_const().
    ConstField get(ConstVarStore var_store) const;

private:
    ib_type m_ib;
};

/**
 * VarSource; equivalent to a pointer to ib_var_source_t.
 *
 * VarSource can be treated as ConstVarSource.  See @ref ironbeepp for
 * details on IronBee++ object semantics.
 *
 * See @ref IronBeeEngineVarSource.
 *
 * @sa ConstVarSource
 * @sa ironbeepp
 * @sa ib_var_source_t
 * @nosubgrouping
 **/
class VarSource :
    public ConstVarSource
{
public:
    //! C Type.
    typedef ib_var_source_t* ib_type;

    /**
     * Remove the constness of a ConstVarSource.
     *
     * @warning This is as dangerous as a @c const_cast, use carefully.
     *
     * @param[in] var ConstVarSource to remove const from.
     * @returns Var pointing to same underlying var as @a var.
     **/
    static VarSource remove_const(ConstVarSource var);

    /**
     * Construct singular VarSource.
     *
     * All behavior of a singular VarSource is undefined except for
     * assignment, copying, comparison, and evaluate-as-bool.
     **/
    VarSource();

    /**
     * @name C Interoperability
     * Methods to access underlying C types.
     **/
    ///@{

    //! ib_var_source_t accessor.
    ib_type ib() const
    {
        return m_ib;
    }

    //! Construct VarSource from ib_var_source_t.
    explicit
    VarSource(ib_type ib_var_source);

    ///@}

    //! See ib_var_source_register().
    // register is a keyword.
    static VarSource register_(
        VarConfig config,
        const char *name,
        size_t name_length,
        ib_rule_phase_num_t initial_phase = IB_PHASE_NONE,
        ib_rule_phase_num_t final_phase = IB_PHASE_NONE
    );

    //! See ib_var_source_register().
    static VarSource register_(
        VarConfig config,
        const std::string& name,
        ib_rule_phase_num_t initial_phase = IB_PHASE_NONE,
        ib_rule_phase_num_t final_phase = IB_PHASE_NONE
    );

    //! See ib_var_source_acquire().
    static VarSource acquire(
        MemoryManager   mm,
        ConstVarConfig  config,
        const char     *name,
        size_t          name_length
    );

    //! See ib_var_source_acquire().
    static VarSource acquire(
        MemoryManager      mm,
        ConstVarConfig     config,
        const std::string& name
    );

    //! See ib_var_source_get().
    Field get(VarStore store) const;

    //! See ib_var_source_set().
    void set(VarStore store, Field value) const;

    //! See ib_var_source_initialize().
    Field initialize(VarStore store, ConstField::type_e type) const;

    //! See ib_var_source_append().
    void append(VarStore store, Field value) const;

private:
    ib_type m_ib;
};

/**
 * Output operator for VarSource.
 *
 * Output IronBee::VarSource[@e value] where @e value is based on the
 * name.
 *
 * @param[in] o Ostream to output to.
 * @param[in] var_source VarSource to output.
 * @return @a o
 **/
std::ostream& operator<<(std::ostream& o, const ConstVarSource& var_source);

// VarFilter

/**
 * Const VarFilter; equivalent to a const pointer to @ref ib_var_filter_t.
 *
 * Provides operators ==, !=, <, >, <=, >= and evaluation as a boolean for
 * singularity via CommonSemantics.
 *
 * See @ref IronBeeEngineVarFilter.
 *
 * @sa Var
 * @sa ironbeepp
 * @sa ib_var_filter_t
 * @nosubgrouping
 **/
class ConstVarFilter :
    public CommonSemantics<ConstVarFilter>
{
public:
    //! C Type.
    typedef const ib_var_filter_t* ib_type;

    /**
     * Construct singular ConstVarFilter.
     *
     * All behavior of a singular ConstVarFilter is undefined except for
     * assignment, copying, comparison, and evaluate-as-bool.
     **/
    ConstVarFilter();

    /**
     * @name C Interoperability
     * Methods to access underlying C types.
     **/
    ///@{

    //! const ib_var_filter_t accessor.
    // Intentionally inlined.
    ib_type ib() const
    {
        return m_ib;
    }

    //! Construct Var from ib_var_filter_t.
    explicit
    ConstVarFilter(ib_type ib_var_filter);

    ///@}

    //! See ib_var_filter_apply().
    ConstList<ConstField> apply(MemoryManager mm, Field field) const;

    //! See ib_var_filter_remove().
    List<ConstField> remove(MemoryManager mm, Field field) const;

    //! See ib_var_filter_remove().
    void remove_without_result(MemoryManager mm, Field field) const;

private:
    ib_type m_ib;
};

/**
 * VarFilter; equivalent to a pointer to ib_var_filter_t.
 *
 * VarFilter can be treated as ConstVarFilter.  See @ref ironbeepp for
 * details on IronBee++ object semantics.
 *
 * See @ref IronBeeEngineVarFilter.
 *
 * @sa ConstVarFilter
 * @sa ironbeepp
 * @sa ib_var_filter_t
 * @nosubgrouping
 **/
class VarFilter :
    public ConstVarFilter
{
public:
    //! C Type.
    typedef ib_var_filter_t* ib_type;

    /**
     * Remove the constness of a ConstVarFilter.
     *
     * @warning This is as dangerous as a @c const_cast, use carefully.
     *
     * @param[in] var ConstVarFilter to remove const from.
     * @returns Var pointing to same underlying var as @a var.
     **/
    static VarFilter remove_const(ConstVarFilter var);

    /**
     * Construct singular VarFilter.
     *
     * All behavior of a singular VarFilter is undefined except for
     * assignment, copying, comparison, and evaluate-as-bool.
     **/
    VarFilter();

    /**
     * @name C Interoperability
     * Methods to access underlying C types.
     **/
    ///@{

    //! ib_var_filter_t accessor.
    ib_type ib() const
    {
        return m_ib;
    }

    //! Construct VarFilter from ib_var_filter_t.
    explicit
    VarFilter(ib_type ib_var_filter);

    ///@}

    //! See ib_var_filter_acquire().
    static VarFilter acquire(
        MemoryManager  mm,
        const char*    filter_string,
        size_t         filter_string_length
    );

    //! See ib_var_filter_acquire().
    static VarFilter acquire(
        MemoryManager      mm,
        const std::string& filter_string
    );

private:
    ib_type m_ib;
};

/**
 * Output operator for VarFilter.
 *
 * Output IronBee::VarFilter[@e value] where @e value is based on the
 * underlying pointer.
 *
 * @param[in] o Ostream to output to.
 * @param[in] var_filter VarFilter to output.
 * @return @a o
 **/
std::ostream& operator<<(std::ostream& o, const ConstVarFilter& var_filter);

// VarTarget

/**
 * Const VarTarget; equivalent to a const pointer to @ref ib_var_target_t.
 *
 * Provides operators ==, !=, <, >, <=, >= and evaluation as a boolean for
 * singularity via CommonSemantics.
 *
 * See @ref IronBeeEngineVarTarget.
 *
 * @sa Var
 * @sa ironbeepp
 * @sa ib_var_target_t
 * @nosubgrouping
 **/
class ConstVarTarget :
    public CommonSemantics<ConstVarTarget>
{
public:
    //! C Type.
    typedef const ib_var_target_t* ib_type;

    /**
     * Construct singular ConstVarTarget.
     *
     * All behavior of a singular ConstVarTarget is undefined except for
     * assignment, copying, comparison, and evaluate-as-bool.
     **/
    ConstVarTarget();

    /**
     * @name C Interoperability
     * Methods to access underlying C types.
     **/
    ///@{

    //! const ib_var_target_t accessor.
    // Intentionally inlined.
    ib_type ib() const
    {
        return m_ib;
    }

    //! Construct Var from ib_var_target_t.
    explicit
    ConstVarTarget(ib_type ib_var_target);

    ///@}

    //! See ib_var_target_get().
    ConstList<ConstField> get(MemoryManager mm, ConstVarStore var_store) const;

    //! See ib_var_target_expand_const().
    ConstVarTarget expand(MemoryManager mm, ConstVarStore var_store) const;

private:
    ib_type m_ib;
};

/**
 * VarTarget; equivalent to a pointer to ib_var_target_t.
 *
 * VarTarget can be treated as ConstVarTarget.  See @ref ironbeepp for
 * details on IronBee++ object semantics.
 *
 * See @ref IronBeeEngineVarTarget.
 *
 * @sa ConstVarTarget
 * @sa ironbeepp
 * @sa ib_var_target_t
 * @nosubgrouping
 **/
class VarTarget :
    public ConstVarTarget
{
public:
    //! C Type.
    typedef ib_var_target_t* ib_type;

    /**
     * Remove the constness of a ConstVarTarget.
     *
     * @warning This is as dangerous as a @c const_cast, use carefully.
     *
     * @param[in] var ConstVarTarget to remove const from.
     * @returns Var pointing to same underlying var as @a var.
     **/
    static VarTarget remove_const(ConstVarTarget var);

    /**
     * Construct singular VarTarget.
     *
     * All behavior of a singular VarTarget is undefined except for
     * assignment, copying, comparison, and evaluate-as-bool.
     **/
    VarTarget();

    /**
     * @name C Interoperability
     * Methods to access underlying C types.
     **/
    ///@{

    //! ib_var_target_t accessor.
    ib_type ib() const
    {
        return m_ib;
    }

    //! Construct VarTarget from ib_var_target_t.
    explicit
    VarTarget(ib_type ib_var_target);

    ///@}

    //! See ib_var_target_acquire().
    static VarTarget acquire(
        MemoryManager     mm,
        VarSource      source,
        ConstVarExpand expand,
        ConstVarFilter filter
    );

    //! See ib_var_target_acquire_from_string().
    static VarTarget acquire_from_string(
        MemoryManager  mm,
        VarConfig   var_config,
        const char *target_string,
        size_t      target_string_length
    );

    //! See ib_var_target_acquire_from_string().
    static VarTarget acquire_from_string(
        MemoryManager         mm,
        VarConfig          var_config,
        const std::string &target_string
    );

    //! See ib_var_target_get().
    ConstList<Field> get(MemoryManager mm, VarStore var_store) const;

    //! See ib_var_target_remove().
    List<Field> remove(MemoryManager mm, VarStore var_store) const;

    //! See ib_var_target_remove().
    void remove_without_result(MemoryManager mm, VarStore var_store) const;

    //! See ib_var_target_expand().
    VarTarget expand(MemoryManager mm, ConstVarStore var_store) const;

    //! See ib_var_target_set().
    void set(MemoryManager mm, VarStore var_store, Field field) const;

    //! See ib_var_target_remove_and_set().
    void remove_and_set(
        MemoryManager mm,
        VarStore   var_store,
        Field      field
    ) const;

private:
    ib_type m_ib;
};

/**
 * Output operator for VarTarget.
 *
 * Output IronBee::VarTarget[@e value] where @e value is based on the
 * underlying pointer value.
 *
 * @param[in] o Ostream to output to.
 * @param[in] var_target VarTarget to output.
 * @return @a o
 **/
std::ostream& operator<<(std::ostream& o, const ConstVarTarget& var_target);

// VarExpand

/**
 * Const VarExpand; equivalent to a const pointer to @ref ib_var_target_t.
 *
 * Provides operators ==, !=, <, >, <=, >= and evaluation as a boolean for
 * singularity via CommonSemantics.
 *
 * See @ref IronBeeEngineVarExpand.
 *
 * @sa Var
 * @sa ironbeepp
 * @sa ib_var_target_t
 * @nosubgrouping
 **/
class ConstVarExpand :
    public CommonSemantics<ConstVarExpand>
{
public:
    //! C Type.
    typedef const ib_var_expand_t* ib_type;

    /**
     * Construct singular ConstVarExpand.
     *
     * All behavior of a singular ConstVarExpand is undefined except for
     * assignment, copying, comparison, and evaluate-as-bool.
     **/
    ConstVarExpand();

    /**
     * @name C Interoperability
     * Methods to access underlying C types.
     **/
    ///@{

    //! const ib_var_target_t accessor.
    // Intentionally inlined.
    ib_type ib() const
    {
        return m_ib;
    }

    //! Construct Var from ib_var_expand_t.
    explicit
    ConstVarExpand(ib_type ib_var_expand);

    ///@}

    //! See ib_var_expand_execute().
    std::pair<const char*, size_t> execute(
        MemoryManager mm,
        VarStore   var_store
    ) const;

    //! See ib_var_expand_execute().
    std::string execute_s(
        MemoryManager mm,
        VarStore   var_store
    ) const;

    //! See ib_var_expand_test().
    static bool test(const char* str, size_t str_length);

    //! See ib_var_expand_test().
    static bool test(const std::string &s);

private:
    ib_type m_ib;
};

/**
 * VarExpand; equivalent to a pointer to ib_var_expand_t.
 *
 * VarExpand can be treated as ConstVarExpand.  See @ref ironbeepp for
 * details on IronBee++ object semantics.
 *
 * See @ref IronBeeEngineVarExpand.
 *
 * @sa ConstVarExpand
 * @sa ironbeepp
 * @sa ib_var_expand_t
 * @nosubgrouping
 **/
class VarExpand :
    public ConstVarExpand
{
public:
    //! C Type.
    typedef ib_var_expand_t* ib_type;

    /**
     * Remove the constness of a ConstVarExpand.
     *
     * @warning This is as dangerous as a @c const_cast, use carefully.
     *
     * @param[in] var ConstVarExpand to remove const from.
     * @returns Var pointing to same underlying var as @a var.
     **/
    static VarExpand remove_const(ConstVarExpand var);

    /**
     * Construct singular VarExpand.
     *
     * All behavior of a singular VarExpand is undefined except for
     * assignment, copying, comparison, and evaluate-as-bool.
     **/
    VarExpand();

    /**
     * @name C Interoperability
     * Methods to access underlying C types.
     **/
    ///@{

    //! ib_var_expand_t accessor.
    ib_type ib() const
    {
        return m_ib;
    }

    //! Construct VarExpand from ib_var_expand_t.
    explicit
    VarExpand(ib_type ib_var_expand);

    ///@}

    //! See ib_var_expand_acquire().
    static VarExpand acquire(
        MemoryManager  mm,
        const char *str,
        size_t      str_length,
        VarConfig   config
    );

    //! See ib_var_expand_acquire().
    static VarExpand acquire(
        MemoryManager         mm,
        const std::string &s,
        VarConfig          config
    );

private:
    ib_type m_ib;
};

/**
 * Output operator for VarExpand.
 *
 * Output IronBee::VarExpand[@e value] where @e value is based on the
 * underlying pointer value.
 *
 * @param[in] o Ostream to output to.
 * @param[in] var_target VarExpand to output.
 * @return @a o
 **/
std::ostream& operator<<(std::ostream& o, const ConstVarExpand& var_target);

} // IronBee

#endif
