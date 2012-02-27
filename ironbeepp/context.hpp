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
 * @brief IronBee++ -- Context (PLACEHOLDER)
 *
 * This is a placeholder for future functionality.  Do not use.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#ifndef __IBPP__CONTEXT__
#define __IBPP__CONTEXT__

#include <boost/shared_ptr.hpp>

#ifdef IBPP_EXPOSE_C
struct ib_context_t;
#endif

namespace IronBee {

namespace Internal {

struct ContextData;
class Builder;

};

class Context
{
public:

#ifdef IBPP_EXPOSE_C
    ib_context_t* ib();
#endif

private:
    friend class Internal::Builder;

    typedef boost::shared_ptr<Internal::ContextData> data_t;

    explicit
    Context( const data_t& data );

    data_t m_data;
};

} // IronBee

#endif
