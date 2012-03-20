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
 * @brief IronBee++ Configuration Map Implementation
 * @internal
 *
 * @sa configuration_map.hpp
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <ironbeepp/configuration_map.hpp>
#include <ironbeepp/internal/catch.hpp>
#include <ironbeepp/internal/data.hpp>

#include <ironbee/debug.h>

namespace IronBee {

namespace Internal {

namespace Hooks {

extern "C" {

const void* cfgmap_get(
    const void*       base,
    const ib_field_t* field,
    void*             cbdata
)
{
    IB_FTRACE_INIT();

    assert(base != NULL);

    const void* result;
    ib_status_t rc = IBPP_TRY_CATCH(NULL,(
        result = Internal::data_to_value<
            configuration_map_init_getter_translator_t
        >(cbdata)(
            base, field
        )
    ));
    if (rc == IB_OK) {
        return result;
    } else {
        return NULL;
    }
}

ib_status_t cfgmap_set(
    void*       base,
    ib_field_t* field,
    const void* value,
    void*       cbdata
)
{
    IB_FTRACE_INIT();

    assert(base != NULL);

    IB_FTRACE_RET_STATUS(IBPP_TRY_CATCH(NULL,
        Internal::data_to_value<
            configuration_map_init_setter_translator_t
        >(cbdata)(
            base, field, value
        )
    ));
}

const void* cfgmap_handle_get(
    const void*       handle,
    const ib_field_t* field,
    void*             cbdata
)
{
    IB_FTRACE_INIT();

    assert(handle != NULL);

    return cfgmap_get(
        *reinterpret_cast<const void* const*>(handle),
        field,
        cbdata
    );
}

ib_status_t cfgmap_handle_set(
    void*       handle,
    ib_field_t* field,
    const void* value,
    void*       cbdata
)
{
    assert(handle != NULL);
    
    return cfgmap_set(
        *reinterpret_cast<void**>(handle),
        field,
        value,
        cbdata
    );
}

} // extern "C"

} // Hooks

void set_configuration_map_init_translators(
    ib_cfgmap_init_t& init,
    ib_mpool_t* mpool,
    configuration_map_init_getter_translator_t getter_translator,
    configuration_map_init_setter_translator_t setter_translator,
    bool data_is_handle
)
{
    if (data_is_handle) {
        init.fn_get = Hooks::cfgmap_handle_get;
        init.fn_set = Hooks::cfgmap_handle_set;
    }
    else {
        init.fn_get = Hooks::cfgmap_get;
        init.fn_set = Hooks::cfgmap_set;
    }
    init.cbdata_get = Internal::value_to_data(
        getter_translator,
        mpool
    );
    init.cbdata_set = Internal::value_to_data(
        setter_translator,
        mpool
    );
}

} // Internal

} // IronBee
