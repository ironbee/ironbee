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
 * @brief IronBee --- Top level API documentation.
 *
 * This file contains no code, only API documentations.  It functions as the
 * main page of the API documentation.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 **/

/**
 * @mainpage IronBee Documentation
 *
 * Welcome to the API documentation for IronBee.
 *
 * If you are a module, rule, or server writer, you are probably interested in
 * the public API.  See the Modules list and the files in include/ironbee.
 *
 * Some general documentation on IronBee and development from the codebase:
 * - https://github.com/ironbee/ironbee/blob/master/README.adoc
 * - https://github.com/ironbee/ironbee/blob/master/CHANGES.adoc
 * - https://github.com/ironbee/ironbee/blob/master/DEVELOPMENT.adoc
 * - https://github.com/ironbee/ironbee/blob/master/CODESTYLE.adoc
 *
 * Of particular interest:
 * - Module writers: module.h, module_sym.h, example_modules/ibmod_set_c.c,
 *   example_modules/ibmod_set_cpp.cpp.
 * - Server writers: server.h, state_notify.h, example_servers/parsed_c.c,
 *   example_modules/unparsed_cpp.cpp.
 * - Lua Rule and Module writers: @ref LuaAPI
 * - Everyone: types.h, mpool.h
 *
 * If you are interested in developing the above in C++, see @ref
 * ironbeepp page.
 *
 * if you are interested in IronAutomata, an automata creation and execution
 * framework that targets IronBee's needs but has minimal dependence on
 * IronBee, see @ref ironautomata.
 *
 * If you are an IronBee developer, you may be interested in the private API.
 * First, make sure you are viewing the internal version of this
 * documentation.  That version includes all internal modules, functions,
 * files, etc.  Run '@c make @c doxygen' in the docs directory and look at
 * @c doxygen_internal/html/index.html
 *
 * This page is defined in include/ironbee/apidoc.h.
 **/

/**
 * @defgroup IronBee IronBee WAF
 **/
