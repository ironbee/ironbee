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
 * @brief Predicate --- Predicate module extension API.
 *
 * Defines API for extending predicate functionality.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#ifndef __PREDICATE__IBMOD_PREDICATE__
#define __PREDICATE__IBMOD_PREDICATE__

namespace IronBee {

class Engine;

namespace Predicate {

class CallFactory;

}

}

/**
 * Access call factory of the predicate module.
 *
 * This function can be used to add calls to the call factory used by the
 * predicate module.  It is only valid after the predicate module has been
 * loaded and initialized.  For example, it could be used in module
 * initialization functions of modules loaded after the predicate module.
 *
 * @param[in] engine IronBee engine.
 * @return Call factory used by Predicate module.
 **/
IronBee::Predicate::CallFactory& IBModPredicateCallFactory(IronBee::Engine engine);

#endif
