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
 *****************************************************************************/

/* NOTE: There is no header guard as this file must be re-included
 *       to allow for symbol renaming.
 */

/**
 * @file
 * @brief IronBee - Module
 *
 * @author Brian Rectanus <brectanus@qualys.com>
 */

/**
 * @defgroup IronBeeModule Module
 * @ingroup IronBee
 * @{
 */

/* Allow for a custom prefix. */
#ifndef IB_MODULE_SYM_PREFIX
/**
 * Default module symbol name prefix.
 *
 * @note This can be defined before including @a module.h in order
 *       to avoid clashing symbols for static modules.
 *
 * @example
 * #ifdef IB_MODULE_SYM_PREFIX
 * #undef IB_MODULE_SYM_PREFIX
 * #endif
 * #define IB_MODULE_SYM_PREFIX myprefix
 * #include "modules/mymodule.c"
 */
#define IB_MODULE_SYM_PREFIX          ibsym
#endif

/* Undefine private module definitions. */
#ifdef MODULE_NAME
#undef MODULE_NAME
#endif
#ifdef MODULE_NAME_STR
#undef MODULE_NAME_STR
#endif

/* Undefine any existing symbols. */
#ifdef IB_MODULE_SYM
#undef IB_MODULE_SYM
#endif
#ifdef IB_XMODULE_SYM
#undef IB_XMODULE_SYM
#endif
#ifdef IB_MODULE_SYM_NAME
#undef IB_MODULE_SYM_NAME
#endif
#ifdef IB_MODULE_STRUCT
#undef IB_MODULE_STRUCT
#endif
#ifdef IB_XMODULE_STRUCT
#undef IB_XMODULE_STRUCT
#endif
#ifdef IB_MODULE_STRUCT_PTR
#undef IB_MODULE_STRUCT_PTR
#endif
#ifdef IB_MODULE_DECLARE
#undef IB_MODULE_DECLARE
#endif
#ifdef IB_MODULE_INIT
#undef IB_MODULE_INIT
#endif

/* Macros to allow for expanding arguments with concatination. */
#define IB_XMODULE_SYM(prefix)        prefix ## __module_sym
#define IB_XXMODULE_SYM(prefix)       IB_XMODULE_SYM(prefix)
#define IB_XMODULE_STRUCT(prefix)     prefix ## __module_struct
#define IB_XXMODULE_STRUCT(prefix)    IB_XMODULE_STRUCT(prefix)

/** Module symbol name. */
#define IB_MODULE_SYM                 IB_XXMODULE_SYM(IB_MODULE_SYM_PREFIX)

/** Module symbol name as a string. */
#define IB_MODULE_SYM_NAME            IB_XSTRINGIFY(IB_MODULE_SYM)

/** Module structure. */
#define IB_MODULE_STRUCT              IB_XXMODULE_STRUCT(IB_MODULE_SYM_PREFIX)

/** Address of module structure. */
#define IB_MODULE_STRUCT_PTR          &IB_MODULE_STRUCT

/**
 * Module declaration.
 *
 * This macro needs to be called towards the beginning of a module if
 * the module needs to refer to @ref IB_MODULE_STRUCT or
 * @ref IB_MODULE_STRUCT_PTR before the module structure is initialized
 * with @ref IB_MODULE_INIT.
 */
#ifdef __cplusplus
/* C++ cannot do forward declarations for IB_MODULE_STRUCT. */
#define IB_MODULE_DECLARE() \
    ib_module_t DLL_PUBLIC *IB_MODULE_SYM(void); \
    extern ib_module_t IB_MODULE_STRUCT
#else
#define IB_MODULE_DECLARE() \
    ib_module_t DLL_PUBLIC *IB_MODULE_SYM(void); \
    static ib_module_t IB_MODULE_STRUCT
#endif

/**
 * Module structure initialization.
 *
 * This is typically the last macro called in a module. It initializes
 * the module structure (@ref IB_MODULE_STRUCT), which allows a module
 * to be registered with the engine. The macro takes a list of all
 * @ref ib_module_t field values.
 */
#ifdef __cplusplus
/* C++ cannot do forward declarations for IB_MODULE_STRUCT. */
#define IB_MODULE_INIT(...) \
    ib_module_t IB_MODULE_STRUCT = { \
        __VA_ARGS__ \
    }; \
    ib_module_t *IB_MODULE_SYM(void) { return IB_MODULE_STRUCT_PTR; } \
    typedef int ib_require_semicolon_hack_
#else
#define IB_MODULE_INIT(...) \
    static ib_module_t IB_MODULE_STRUCT = { \
        __VA_ARGS__ \
    }; \
    ib_module_t *IB_MODULE_SYM(void) { return IB_MODULE_STRUCT_PTR; } \
    typedef int ib_require_semicolon_hack_
#endif

/**
 * @} IronBeeModule
 */

