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
 * @brief IronBee --- Example Module: Set (C++ Version)
 *
 * This file is the C++ implementation of the Set example module.  There is
 * also a C (ibmod_set.c) implementation.
 *
 * @par Summary
 * This module provides set membership of named sets.  It is similar
 * to the `@match` and `@imatch` operators except that sets are defined
 * outside of rules via directives rather than inline as arguments to the
 * operator.  Defining sets via directives is superior when sets will be
 * reused across multiple rules.
 *
 * @par Operators
 * - `@set_match set` -- True iff input is in set named `set`.  Supports
 *   streaming and non-streaming rules as well as NULL input but does not
 *   capture.
 *
 * @par Directives
 * - `SetDefine set member1...` -- Create a case sensitive set named `set`
 *   with members given by later arguments.
 * - `SetDefineInsensitive set member1...` -- As `SetDefine` but case
 *   insensitive.
 * - `SetDefineFromFile set path` -- As `SetDefine` but members are read
 *   from file at `path`, one item per line.
 * - `SetDefineInsensitiveFromFile` -- As `SetDefineFromFile` but case
 *   insensitive.
 *
 * @par Configuration
 * - `Set set.debug 1` -- Turn on debugging information for the current
 *   context.  Will log every membership query.
 *
 * @par Note
 * The operator has access to all the sets defined in its context and any
 * ancestor context.  It does not have access to sets defined in other
 * contexts.  Similarly, it is an error to create a new set with the same
 * name as a set in current context or any ancestor context, but not an error
 * to create a set with the same name as a set in other contexts.
 *
 * @par C++ specific comments
 * - This implementation closely parallels the C version, using IronBee::Hash
 *   with common (`&c_true`) values and memory pools to manager state
 *   lifetimes.
 * - Just as the C API makes frequent use of function pointers, the C++ API
 *   makes frequent use of functionals.  If you are unfamiliar with
 *   functionals or `bind`, please review the boost::bind (C++03) or
 *   std::bind (C++11) documentation.
 * - IronBee++ is based around pointer analogues.  It defines classes that
 *   behave like pointers.  E.g., IronBee::Hash is analogous to `ib_hash_t *`
 *   and IronBee::ConstHash is analogous to `const ib_hash_t *`.  Such classes
 *   provide a variety of common services including conversion, const casting,
 *   and comparison.  See common_semantics.hpp.  As pointer analogues,
 *   instances are usually passed by copy for in variables and reference for
 *   out variables.
 * - Errors are represented as exceptions in IronBee++ and translated to log
 *   messages and `ib_status_t` at the C/C++ boundary.  As such, in this code,
 *   only handleable errors are caught and handled.  Others are allowed to
 *   continue up the stack.
 * - Not all of the C API is supported by the C++ API.  For certain tasks, the
 *   C API must be used.  See, e.g., the use of ib_log_info_tx() below.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

/* IronBee has a canonical header order, exemplified in this module.  It is
 * not required for third party development.
 *
 * Headers are divided into sections, more or less:
 *
 * - The autoconf config file containing information about configure.
 * - For implementation files, the public header file.
 * - Any corresponding private header file.
 * - Headers for the framework the current file is party of.
 * - IronBee++
 * - IronBee
 * - Third party libraries, e.g., boost.
 * - Standard library.
 *
 * Within each section, includes are arranged alphabetically.
 *
 * The order is, more or less, specific to general and is arranged as such to
 * increase the chance of catching missing includes.
 */

// Include all of IronBee++
#include <ironbeepp/all.hpp>

#include <boost/bind.hpp>
#include <boost/format.hpp>

#include <fstream>

using boost::bind;
using namespace std;

// Wrap all module specific declarations in an anonymous namespace to prevent
// pollution of the symbol table.
namespace {

// This module uses IronBee hashes to represent sets as they are reasonably
// efficient and can easily be created as either case sensitive or case
// insensitive with the same type.
//
// IronBee::Hash<T> is templated on the value type which can be a pointer or
// an IronBee++ pointer analogue.  IronBee::Hash<void*> can be used for
// generic, C style, hashes.

//! Type to use for sets.
typedef IronBee::Hash<const int*> set_t;
//! Const-version of above.
typedef IronBee::ConstHash<const int*> const_set_t;

//! `&c_true` will be the value used for all members of a @ref set_t.
const int c_true = 1;

/**
 * Per configuration context data.
 *
 * A PerContext will be created for each configuration context and will hold
 * module data specific to that context.  The initial instance will be
 * constructed using the default constructor.  Later instances will be
 * constructed from the instances of the parent context via the copy
 * constructor.
 **/
class PerContext
{
public:
    /**
     * Constructor.
     **/
    PerContext();

    /**
     * Add a set for this context.
     *
     * @warning Will overwrite any existing set with name @a name.
     *
     * @param[in] name Name of set.
     * @param[in] set  Set.
     **/
    void add_set(
        const string& name,
        set_t         set
    );

    /**
     * Fetch a set with name @a name.
     *
     * @param[in] name Name of set to fetch.
     * @return Set with name @a name.
     * @throw IronBee::enoent if no such set exists.
     **/
    set_t fetch_set(
        const string& name
    ) const;

    /**
     * Test for set existence by name.
     *
     * @param[in] name Name of set to query.
     * @return true iff a set with name @a name exists.
     **/
    bool set_exists(
        const string& name
    ) const;

    // The following two accessor routines are used in the configuration map.
    // They allow the user to turn the debug setting on via
    //
    // @code
    // Set set.debug 1
    // @endcode
    //
    // The use of @ref ib_num_t is required by the configuration map, which is
    // field based.  Internally, it is converted into a bool: @ref m_bool.

    //! Debug read accessor.
    ib_num_t get_debug() const;

    //! Debug write accessor.
    void set_debug(
        ib_num_t new_debug
    );

    /**
     * Create an operator instance for `\@set_member set_name`.
     *
     * Constructs an operator instance, a functional, for a given set.
     *
     * @param[in] set_name Set to construct for.
     * @return Operator instance functional.
     * @throw IronBee::enoent if no set with name @a set_name.
     **/
    IronBee::Operator::operator_instance_t make_operator_instance(
        const char* set_name
    );

private:
    /**
     * Handle execution of the `set_member` operator.
     *
     * @note The @a set parameter is redundant as we could look it up in
     * @ref m_sets via @a set_name.  Instead of doing so, we look it up once
     * in make_operator_instance() and bind it to the @a set parameter.  This
     * approach saves the cost of a map lookup for every operator execution.
     *
     * Will emit a log message of query if @ref m_debug is true.
     *
     * @param[in] set      Set to look for @a input in.
     * @param[in] set_name Name of set; used for error messages.
     * @param[in] tx       Current transaction; used for error messages.
     * @param[in] input    Byte string field to look for in set.
     * @return 1 if @a input is in @a set and 0 otherwise.
     **/
    int operator_execute(
        const_set_t          set,
        const string&        set_name,
        IronBee::Transaction tx,
        IronBee::ConstField  input
    ) const;

    //! Type used to track sets by name.
    typedef map<string, set_t> sets_t;

    /**
     * All the sets known for this context.
     *
     * It is important that this member is *copied* for each next context.
     * This copying allows each child to know about all the sets of its
     * ancestors but not those of siblings or descendants.
     **/
    sets_t m_sets;

    //! If true, operator_execute() will emit log message of query.
    bool m_debug;
};

/**
 * Handle modules callbacks.
 *
 * IronBee++ provides two methods of defining modules.  The low level method,
 * not used here, is to provide a function that takes an IronBee::Module and
 * does whatever it needs to do to handle module loading: usually set a
 * callback to be called on module initialization.  The second method, used
 * below, is to provide a module Delegate.  An instance of the Delegate is
 * constructed at initialization, various methods are called for each
 * callback, and the instance is destructed at module finish.  The Delegate
 * must define a certain set of methods, but can easily use defaults by
 * inheriting from IronBee::ModuleDelegate.
 *
 * In this module, the Delegate defines none of the special methods except
 * the constructor.
 **/
class Delegate :
    public IronBee::ModuleDelegate
{
public:
    /**
     * Constructor.
     *
     * Called at module initialization.  Note that in a multi-engine IronBee
     * situation, multiple instances may exist: be sure to store per-module
     * state as instance data and not as static data.
     *
     * The corresponding IronBee::Module is provided.  When using
     * IronBee::ModuleDelegate, this should be passed to the parent
     * constructor.  It will then be available to members via module().
     *
     * @param[in] module The module being initialized.
     **/
    explicit
    Delegate(IronBee::Module module);

private:
    // These are used in bind() statement inside the constructor and the
    // resulting functionals provided to IronBee++.  As such, they can be
    // private.

    /**
     * Handle `SetDefine` and `SetDefineInsensitive` directives.
     *
     * @param[in] cp               Configuration parser; used for current
     *                             configuration context and better error
     *                             messages.
     * @param[in] directive_name   Name of directive.  Used for better error
     *                             messages.
     * @param[in] params           Parameters of directive.
     * @param[in] case_insensitive If true, define a case insensitive set.
     *                             Bound at registration time.
     * @throw IronBee::einval on too few parameters or already existent set.
     **/
    void dir_define(
        IronBee::ConfigurationParser cp,
        const char*                  directive_name,
        IronBee::List<const char*>   params,
        bool                         case_insensitive
    ) const;

    /**
     * Handle `SetDefineFromFile` and `SetDefineFromFileInsensitive`
     * directives.
     *
     * @param[in] cp               Configuration parser; used for current
     *                             configuration context and better error
     *                             messages.
     * @param[in] directive_name   Name of directive.  Used for better error
     *                             messages.
     * @param[in] set_name         Name of set to define.
     * @param[in] path             Path to file of set contents.
     * @param[in] case_insensitive If true, define a case insensitive set.
     *                             Bound at registration time.
     * @throw IronBee::einval on file IO error or already existent set.
     **/
    void dir_define_from_file(
        IronBee::ConfigurationParser cp,
        const char*                  directive_name,
        const char*                  set_name,
        const char*                  path,
        bool                         case_insensitive
    ) const;

    /**
     * Helper function to define a set from a sequence of items.
     *
     * Both dir_define() and dir_define_from_file() call this helper to create
     * the set.  In the former case, iterator into the parameter list are
     * provided.  In the latter case, the file is read into a list and
     * iterators into that list are provided.
     *
     * @tparam Iterator Type of @a begin and @a end.
     * @param[in] cp               Configuration parser; used for current
     *                             configuration context and better error
     *                             messages.
     * @param[in] directive_name   Name of directive; used in error messages.
     * @param[in] case_insensitive If true, define a case insensitive set.
     * @param[in] set_name         Name of set to define.
     * @param[in] begin            Beginning of item sequence.
     * @param[in] end              End of item sequence.
     * @throw IronBee::einval if a set with name @a set_name already exists.
     **/
    template <typename Iterator>
    void define_set(
        IronBee::ConfigurationParser cp,
        const char*                  directive_name,
        bool                         case_insensitive,
        const char*                  set_name,
        Iterator                     begin,
        Iterator                     end
    ) const;

    /**
     * Create an instance for `\@set_member set_name` in @a context.
     *
     * This method looks up the PerContext for @a context and forwards to
     * PerContext::make_operator_instance().
     *
     * @param[in] context  Configuration context of operator.
     * @param[in] set_name Name of set to create instance for.
     * @return Operator instance.
     * @throw IronBee::enoent if no set named @a set_name exists.
     **/
    IronBee::Operator::operator_instance_t make_operator_instance(
        IronBee::Context context,
        const char*      set_name
    );
};

} // Anonymous namespace

// This macro sets up the public module API that IronBee used to load modules.
// It takes the name of the module and the *type* of the Delegate.  The symbol
// this macro defines must be externally visible, so this macro must be
// outside the anonymous namespace.

IBPP_BOOTSTRAP_MODULE_DELEGATE("set", Delegate)

// Implementation

// PerContext

// This reopening of the anonymous namespace is required to keep doxygen
// happy.  C++ doesn't care.
namespace {

PerContext::PerContext() :
    m_debug(false)
{
    // nop
}

void PerContext::add_set(const string& name, set_t set)
{
    m_sets[name] = set;
}

set_t PerContext::fetch_set(const string& name) const
{
    sets_t::const_iterator i = m_sets.find(name);
    if (i == m_sets.end()) {
        // IronBee++ uses exceptions to reports errors.  There is an
        // exception class for each status code in the C API except IB_OK.
        // These are boost exceptions and should be thrown with
        // BOOST_THROW_EXCEPTION().  Boost exceptions allow arbitrary data
        // to be attached to an exception.  See below for an example.  In
        // addition, BOOST_THROW_EXCEPTION() automatically attaches the
        // current function, file, and line which is then logged if the log
        // level is set to debug or higher.
        //
        // In this case, we do not wish to emit a message, simply indicate
        // that the set was not found, so we throw without any additional
        // information.
        BOOST_THROW_EXCEPTION(IronBee::enoent());
    }
    return i->second;
}

bool PerContext::set_exists(const string& name) const
{
    return m_sets.find(name) != m_sets.end();
}

// As mentioned above, get_debug() and set_debug() use @ref ib_num_t in order
// to properly interact with the configuration map code which is based on
// fields.

ib_num_t PerContext::get_debug() const
{
    return m_debug ? 1 : 0;
}

void PerContext::set_debug(ib_num_t new_debug)
{
    m_debug = (new_debug != 0);
}

// In IronBee++, there are two ways to define new operators.  The low level
// method closely mirrors the C API and involves providing create, destroy,
// and execute functionals that operate on `void *` instance data.  The higher
// level method, illustrated here, uses a generator functional that returns
// an instance functional.  The generator functional is:
// @code
// bind(&Delegate::make_operator_instance, this, _1, _2)
// @endcode
// And is produced and set in Delegate::Delegate().
// Delegate::make_operator_instance() looks up the PerContext and forwards
// to this function which produces the instance functional by binding
// PerContext::operator_execute().

IronBee::Operator::operator_instance_t PerContext::make_operator_instance(
    const char* set_name
)
{
    return bind(
        &PerContext::operator_execute,
        this,
        fetch_set(set_name),
        set_name,
        _1, // Current transaction.
        _2  // Input field.
        // A capture field is also provided as _3 but we don't use it and
        // so omit it from the bind.
    );
}

// Operator instances are provided the transaction (@a tx), input (@a input),
// and  a capture collection (not used or bound in this example) and return
// true (1) or false (0).  This function also has set and set_name which are
// bound at operator instance creation (see above).

int PerContext::operator_execute(
    const_set_t          set,
    const string&        set_name,
    IronBee::Transaction tx,
    IronBee::ConstField  input
) const
{
    // Used to log queries of a null input.
    static const char* c_null_input("null");

    int         result        = 1;
    const char* input_data;
    size_t      input_length;

    if (! input) {
        // Null query.
        result       = 0;
        input_data   = c_null_input;
        input_length = sizeof(c_null_input);
    }
    else {
        input_data   = input.value_as_byte_string().const_data();
        input_length = input.value_as_byte_string().length();
        try {
            set.get(input_data, input_length);
        }
        catch (IronBee::enoent) {
            result = 0;
        }
    }

    if (m_debug) {
        // There is currently no C++ API for logging.  Error cases can be
        // handled by attaching a message to the thrown exception, but
        // non-error cases, such as this, must be handled via the C API.
        //
        // All Ironbee pointer analogues provide access to the underlying
        // C pointer via the ib() method.  `Thus tx.ib()` is the `ib_tx_t*`
        // corresponding to tx.
        ib_log_info_tx(
            tx.ib(),
            "set_member %s for %.*s = %s",
            set_name.c_str(),
            static_cast<int>(input_length), input_data,
            (result == 1 ? "yes" : "no")
        );
    }

    return result;
}

// Delegate

Delegate::Delegate(IronBee::Module module) :
    IronBee::ModuleDelegate(module)
{
    /* Configuration data */

    // Module::set_configuration_data() is used to define configuration data
    // with C++ style semantics including use of the copy constructor.  For
    // POD data with either a default copier (`memcpy`) or a provider copier,
    // use Module::set_configuration_data_pod().
    module.set_configuration_data<PerContext>()
        .number(
          "set.debug",
          bind(&PerContext::get_debug, _1),
          bind(&PerContext::set_debug, _1, _3)
        )
        ;

    /* Directives */
    module.engine().register_configuration_directives()
        // _1 = configuration parser
        // _2 = directive name
        // _3 = parameters list
        // false | true = case insensitivity
        .list(
            "SetDefine",
            bind(&Delegate::dir_define, this, _1, _2, _3, false)
        )
        .list(
            "SetDefineInsensitive",
            bind(&Delegate::dir_define, this, _1, _2, _3, true)
        )
        // _1 = configuration parser
        // _2 = directive name
        // _3 = first parameter: set name
        // _4 = second parameter: path
        // false | true = case insensitivity
        .param2(
            "SetDefineFromFile",
            bind(&Delegate::dir_define_from_file, this, _1, _2, _3, _4, false)
        )
        .param2(
            "SetDefineInsensitiveFromFile",
            bind(&Delegate::dir_define_from_file, this, _1, _2, _3, _4, true)
        )
        ;

    /* Operator */
    IronBee::Operator::create(
        module.engine().main_memory_mm(),
        "set_member",
        IB_OP_CAPABILITY_ALLOW_NULL,
        // _1 = Configuration context.
        // _2 = Parameter.
        bind(&Delegate::make_operator_instance, this, _1, _2)
    ).register_with(module.engine());
}

void Delegate::dir_define(
    IronBee::ConfigurationParser cp,
    const char*                  directive_name,
    IronBee::List<const char*>   params,
    bool                         case_insensitive
) const
{
    // Errors are reported by throwing an exception.  A variety of information
    // can be attached to the exception to control the log message emitted.
    // Here we use IronBee::errinfo_what() to set a log message and
    // IronBee::errinfo_configuration_parser() to provide the current
    // configuration parser.  This information is used at the C++/C boundary
    // to emit an error message with the desired message and indicating the
    // current configuration file and line number.
    //
    // IronBee++ provides helper functions to handle exceptions
    // (IronBee::convert_exception()) and to convert status codes from a C API
    // call to an exception (IronBee::throw_if_error()).  See exception.hpp
    // for all exceptions and error infos.
    if (params.size() < 2) {
        BOOST_THROW_EXCEPTION(IronBee::einval()
            << IronBee::errinfo_what(
                "SetDefine requires 2 or more arguments."
            )
            << IronBee::errinfo_configuration_parser(cp)
        );
    }

    // IronBee::List provides bidirectional non-mutating iterators.
    IronBee::List<const char*>::iterator params_i = params.begin();
    const char* set_name = *params_i;
    ++params_i;

    define_set(
        cp,
        directive_name,
        case_insensitive,
        set_name,
        params_i, params.end()
    );
}

void Delegate::dir_define_from_file(
    IronBee::ConfigurationParser cp,
    const char*                  directive_name,
    const char*                  set_name,
    const char*                  path,
    bool                         case_insensitive
) const
{
    ifstream input(path);

    // Another example of error reporting.  See discussion above.
    if (! input) {
        BOOST_THROW_EXCEPTION(IronBee::einval()
            << IronBee::errinfo_what((boost::format(
                    "%s unable to open file %s"
               ) % directive_name % path).str())
            << IronBee::errinfo_configuration_parser(cp)
        );
    }

    list<string> contents;
    string line;
    while (input) {
        getline(input, line);
        if (! line.empty()) {
            contents.push_back(string());
            contents.back().swap(line);
        }
    }

    define_set(
        cp,
        directive_name,
        case_insensitive,
        set_name,
        contents.begin(), contents.end()
    );
}

template <typename Iterator>
void Delegate::define_set(
    IronBee::ConfigurationParser cp,
    const char*                  directive_name,
    bool                         case_insensitive,
    const char*                  set_name,
    Iterator                     begin,
    Iterator                     end
) const
{
    IronBee::MemoryManager mm = module().engine().main_memory_mm();
    PerContext& per_context =
        module().configuration_data<PerContext>(cp.current_context());

    // See above for discussion.
    if (per_context.set_exists(set_name)) {
        BOOST_THROW_EXCEPTION(IronBee::einval()
            << IronBee::errinfo_what((boost::format(
                    "%s tried to define an already existent set: %s"
               ) % directive_name % set_name).str())
            << IronBee::errinfo_configuration_parser(cp)
        );
    }

    set_t set;
    if (case_insensitive) {
        set = set_t::create_nocase(mm);
    }
    else {
        set = set_t::create(mm);
    }

    for (Iterator i = begin; i != end; ++i) {
        set.set(IronBee::ByteString::create(mm, *i), &c_true);
    }

    per_context.add_set(set_name, set);
}

IronBee::Operator::operator_instance_t Delegate::make_operator_instance(
    IronBee::Context context,
    const char*      set_name
)
{
    // Forward to context.
    return module().configuration_data<PerContext>(context)
        .make_operator_instance(set_name);
}

}
