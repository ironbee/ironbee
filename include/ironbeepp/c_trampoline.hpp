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
 * @brief IronBee++ -- C Trampoline Support
 *
 * This file provides support for C trampolines, that is, converting C++
 * functionals to pairs of C function pointers and @c void*.  See
 * make_c_trampoline() for details.
 *
 * This file has no dependencies on any other part of IronBee or IronBee++.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#ifndef __IBPP__C_TRAMPOLINE__
#define __IBPP__C_TRAMPOLINE__

#include <boost/any.hpp>
#include <boost/function.hpp>

#include <boost/preprocessor/arithmetic/add.hpp>
#include <boost/preprocessor/arithmetic/sub.hpp>
#include <boost/preprocessor/cat.hpp>
#include <boost/preprocessor/repetition.hpp>

#ifndef IRONBEEPP_C_TRAMPOLINE_MAX_ARGS
/**
 * Maximum number of arguments of a function passed to make_c_trampoline().
 *
 * Can be defined before this file is included to change how many arguments
 * are supported.  The length of this file post-preprocessing is linear in the
 * value.
 *
 * Defaults to 10.
 */
#define IRONBEEPP_C_TRAMPOLINE_MAX_ARGS 10
#endif

namespace IronBee {

/// @cond Internal
namespace c_trampoline_implementation {

//! Generates member of boost::function that contains type of argument @a n.
#define CT_ARG(n) BOOST_PP_CAT(BOOST_PP_CAT(arg, n), _type)
//! Pulls type of argument @a n into current class.
#define CT_ARG_USING(z, n, unused) \
    typedef typename type_of_function::CT_ARG(n) BOOST_PP_CAT(type_of_arg, n);
//! Defines a partial specialization of make_c_trampoline_helper for n-1 args.
#define CT_TOP(n)                                                            \
    template <>                                                              \
    struct make_c_trampoline_helper<BOOST_PP_SUB(n, 1)>                      \
    {                                                                        \
        template <typename F>                                                \
        struct impl {                                                        \
            typedef boost::function<F> type_of_function;                     \
            typedef typename type_of_function::result_type type_of_result;   \
            BOOST_PP_REPEAT_FROM_TO(1, n, CT_ARG_USING, ~)                   \
            typedef type_of_result(*type_of_cptr)(                           \
                BOOST_PP_ENUM_SHIFTED_PARAMS(n, type_of_arg),                \
                void*                                                        \
            );                                                               \
            typedef std::pair<type_of_cptr, void*> type_of_pair;             \
                                                                             \
            static type_of_result trampoline(                                \
                BOOST_PP_ENUM_SHIFTED_BINARY_PARAMS(n, type_of_arg, a),      \
                void* cdata                                                  \
            )                                                                \
            {                                                                \
                return boost::any_cast<type_of_function>(                    \
                    *reinterpret_cast<boost::any*>(cdata)                    \
                )(                                                           \
                    BOOST_PP_ENUM_SHIFTED_PARAMS(n, a)                       \
                );                                                           \
            }                                                                \
        };                                                                   \
    };

/**
 * Helper struct for make_c_trampoline().
 *
 * When @a N is between 1 and IRONBEEPP_C_TRAMPOLINE_MAX_ARGS, this struct
 * will contain an internal template @c impl, templated on the function
 * signature.  The @c impl template will define:
 *
 * - @c type_of_cptr -- Function pointer type.
 * - @c type_of_pair -- Pair of @c type_of_cptr and @c void*.
 * - @c trampoline   -- Static trampoline function.
 *
 * See implementation of make_c_trampoline() for details on usage.
 *
 * @tparam N Arity of function to make trampoline for.
 */
template <int N>
struct make_c_trampoline_helper
{
    // Error case.
};

#ifndef DOXYGEN_SKIP
#include <boost/preprocessor/iteration/local.hpp>
#define BOOST_PP_LOCAL_MACRO(n) CT_TOP(BOOST_PP_ADD(n, 1))
#define BOOST_PP_LOCAL_LIMITS   (1, IRONBEEPP_C_TRAMPOLINE_MAX_ARGS)
#include BOOST_PP_LOCAL_ITERATE()

#undef CT_ARG
#undef CT_ARG_USING
#undef CT_TOP

// Special case for no args.
template<>
struct make_c_trampoline_helper<0>
{
    template <typename F>
    struct impl {
        typedef boost::function<F> type_of_function;
        typedef typename type_of_function::result_type type_of_result;
        typedef type_of_result(*type_of_cptr)(void*);
        typedef std::pair<type_of_cptr, void*> type_of_pair;

        static type_of_result trampoline(void* cdata)
        {
            return boost::any_cast<type_of_function>(
                *reinterpret_cast<boost::any*>(cdata)
            )();
        }
    };
};

#endif

} // c_trampoline_impl
/// @endcond

/**
 * Convert a C++ functional of signature @a F into a C trampoline.
 *
 * Example:
 *
 * @code
 * std::pair<int(*)(int, int, void*), void*> trampoline =
 *   IronBee::make_c_trampoline<int(int, int)>(
 *     std::plus<int>()
 *   );
 * int x = trampoline.first(1, 2, trampoline.second);
 * assert(x == 3);
 * IronBee::delete_c_trampoline(x.second);
 *
 * // C++11 example.
 * auto trampoline =
 *   IronBee::make_c_trampoline<int(int, int)>(
 *     [](int a, int b) {return a + b;}
 *   );
 * int x = trampoline.first(1, 2, trampoline.second);
 * assert(x == 3);
 * IronBee::delete_c_trampoline(x.second);
 * @endcode
 *
 * @tparam F Signature of @a f.  In many cases the compiler will not be able
 *           to deduce @a F from @a f and it will need to be explicitly
 *           specified.  Supports any number of arguments up to
 *           IRONBEEPP_C_TRAMPOLINE_MAX_ARGS which defaults to 10 but can be
 *           modified by defining before this file is included.
 * @param[in] f Function to convert.
 * @return pair of function pointer and @c void*.  Function pointer points to
 *         function with same return type as @a F initial arguments identical
 *         to @a F, plus a final void* argument.  When passed the @c void*
 *         portion to the final argument, will evaluate @a f with arguments
 *         and return result.  Caller is responsible for calling
 *         delete_c_trampoline() on @c void* portion to reclaim memory.
 */
template <typename F>
typename c_trampoline_implementation::make_c_trampoline_helper<
    boost::function<F>::arity
>::template impl<F>::type_of_pair
make_c_trampoline(boost::function<F> f)
{
    return std::make_pair(
        &c_trampoline_implementation::make_c_trampoline_helper<
            boost::function<F>::arity
        >::template impl<F>::trampoline,
        new boost::any(f)
    );
}

/**
 * Reclaim memory from a trampoline.
 *
 * @param[in] cdata @c void* portion of a trampoline returned from
 *                  make_c_trampoline().
 */
inline void delete_c_trampoline(void* cdata)
{
    delete reinterpret_cast<boost::any*>(cdata);
}

} // IronBee

#endif
