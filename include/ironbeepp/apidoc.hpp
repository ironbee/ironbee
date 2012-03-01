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
 * @brief IronBee++ &mdash; Top level doxygen page.
 *
 * This file contains only doxygen directives; no code is present.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

/**
 * \page ironbeepp IronBee++ &mdash; A C++ API for IronBee
 *
 * IronBee++ is a (currently partial) C++ API for IronBee.  It largely mimics
 * the C API, but with many adaptations to C++ including:
 *
 * - Object oriented: C global functions become object methods where
 *   appropriate.
 * - Exceptions: Exceptions used instead of ib_status_t return codes.  See
 *   @ref errors.
 * - Functionals: Callbacks can be set to function pointers, function classes,
 *   bind results, lambdas, etc.
 * - Type safety: What is void* in the C API is replaced with appropriate
 *   types or metatypes.
 * - Runtime Type Checking: The C/C++ layer does runtime type checking on all
 *   callback data that is cast to/from void*.
 * - Much else: ostream support, appropriate operators, etc.
 *
 * At present, the API is very incomplete and any non-trivial use will require
 * mixing the C++ and C APIs.  See @ref cpp_c below.  It is expected that
 * IronBee++ will grow to cover significant portions of the C API.
 *
 * IronBee++ makes significant use of boost.  In particular, you should be
 * familiar with boost::function and boost::exception.
 *
 * With a few exceptions (notably module_bootstrap.hpp), all IronBee++ API is
 * defined in the IronBee namespace.
 *
 * Most IronBee++ objects (those that directly correspond to IronBee objects)
 * are default constructable, copyable, assignable, totally ordered (<, <=,
 * >, >=), equality comparable (==, !=), and evaluatable as a predicate.
 * For more details, see @ref pointer_semantics.
 *
 * \section quick_start Quick Start
 *
 * IronBee++ is currently oriented at module writers.  Below is the complete
 * code for a simple IronBee++ module.
 *
 * @c my_module.hpp
 * @code
 * #include <ironbeepp/module_delegate.hpp>
 *
 * class MyModule : public IronBee::ModuleDelegate
 * {
 *     MyModule( IronBee::Module m );
 *     void context_open( IronBee::Context c ) const;
 * };
 * @endcode
 *
 * @c my_module.cpp
 * @code
 * #include "my_module.hpp"
 * #include <iostream>
 *
 * MyModule::MyModule( IronBee::Module m ) :
 *     IronBee::ModuleDelegate( m )
 * {
 *     std::cout << "MyModule Initialized as " << m.name() << std::endl;
 * }
 *
 * void MyModule::context_open( IronBee::Context c ) const
 * {
 *     std::cout << "context_open for module " << module().name() << std::endl;
 * }
 * @endcode
 *
 * @c ibmod_my_module.cpp
 * @code
 * #include "my_module.hpp"
 * #include <ironbeepp/module_bootstrap.hpp>
 *
 * IBPP_BOOTSTRAP_MODULE( "MyFirstIronBee++Module", MyModule );
 * @endcode
 *
 * The above code can be compiled into a shared library and then loaded into
 * IronBee with the @c LoadModule directive.  The module outputs a message on
 * initialization and whenever a configuration context is opened.  The module
 * can be expanded to other module hooks simply by defining the appropriate
 * methods (e.g., @c context_close).  See module_bootstrap.hpp for details.
 *
 * \section errors Reporting Errors
 *
 * In the C API, most functions and callbacks report errors via an
 * ib_status_t return code.  In the C++ API, status codes are converted to
 * and from exceptions.  Every ib_status_t value has a corresponding
 * exception class, e.g., IB_EINVAL corresponds to IronBee::einval.  Your
 * callbacks can throw these exceptions and they will be converted into
 * appropriate log messages and ib_status_t returns.  See exception.hpp for
 * details including how to control the log message and log level.
 *
 * \section pointer_semantics Pointer Semantics
 *
 * There is generally a one-to-one relationship between the C API structures
 * and the C++ classes.  E.g., ib_module_t and IronBee::Module.  The C++
 * classes are, semantically, pointers to internal object.  As such, it is
 * better to think of the relationship as being between pointers and classes.
 * E.g., @c ib_module_t* and IronBee::Module.
 *
 * As the classes simply refer to an internal object, they can be cheaply
 * copied, constructed, and destructed.  It also enables pass-by-copy, greatly
 * simplifying lifetime concerns.
 *
 * It is also possible to construct singular (equivalent to NULL) objects that
 * do not refer to any actual object.  This is useful, e.g., for enabling
 * storage of IronBee++ objects in standard containers.  You can test if an
 * object is singular by evaluating it in a boolean context, e.g.,
 * @code
 * if ( module ) { ... }
 * @endcode
 * All behavior of singular objects is undefined except for evaluating as
 * bool, copying, comparison, and assignment.  Singular objects are equal
 * to each other and less than all other objects.
 *
 * \section cpp_c C++/C Interoperability
 *
 * The long term goal for IronBee++ is to enable IronBee development without
 * ever using the C API.  In particular, it should, eventually, be possible to
 * develop in C++ without polluting the global namespace or macro space with
 * items from the C API.
 *
 * At the moment, IronBee++ is too limited to allow this.  As such, you will
 * need to, at times, make use of the C API.  To facilitate this, the IronBee
 * classes can provide the underlying C pointer.  This is done via an @c ib()
 * method.  E.g., IronBee::Module::ib() returns the @c ib_module_t*
 * pointing to the underlying ib_module_t.  Symmetrically, an IronBee++
 * object can be created from an IronBee pointer via a constructor.
 *
 * The @c ib() and constructor methods are only available if
 * IBPP_EXPOSE_C is defined when the header file is included.  This
 * preprocessor macro adds the appropriate global symbols (e.g., ib_module_t)
 * and methods.
 *
 * It is important to note that, even with IBPP_EXPOSE_C defined, the C
 * header files will not be included.  E.g., for IronBee::Module you will
 * likely need to include ironbeepp/module.hpp (with IBPP_EXPOSE_C defined
 * before) and ironbee/module.h.
 *
 * Without IBPP_EXPOSE_C defined, the only names added to the global
 * namespace or the macro space will be (a) the IronBee namespace, (b)
 * from the standard library, and (c) from boost.
 *
 * An important exception to the above is module_bootstrap.hpp which of
 * necessity must work at the C/C++ border and, as such, include some
 * IronBee headers.  It is recommended that you keep your file that includes
 * module_bootstrap.hpp as small as possible.  See module_bootstrap.hpp for
 * details.
 **/

/**
 * \namespace IronBee
 * Namespace for IronBee++.
 *
 * @sa @ref ironbeepp
 **/

/**
 * \namespace IronBee::Internal
 * Namespace for internal details of the IronBee++.
 *
 * You should never need to refer to anything inside the Internal namespace.
 * It exists in the public API in order to facilitate certain templates and
 * macros.
 **/
