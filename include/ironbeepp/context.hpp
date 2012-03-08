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
 * @brief IronBee++ &mdash; Context (PLACEHOLDER)
 *
 * This is a placeholder for future functionality.  Do not use.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#ifndef __IBPP__CONTEXT__
#define __IBPP__CONTEXT__

// IronBee C
typedef struct ib_context_t ib_context_t;

namespace IronBee {

class Context
{
public:
    ib_context_t* ib();
    const ib_context_t* ib() const;

    explicit
    Context(ib_context_t* ib_context);

private:
    ib_context_t* m_ib;
};

} // IronBee

#endif
