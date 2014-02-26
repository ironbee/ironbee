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
 *
 * @sa configuration_map.hpp
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <ironbeepp/configuration_map.hpp>
#include <ironbeepp/catch.hpp>
#include <ironbeepp/data.hpp>

namespace IronBee {

namespace Internal {

namespace Hooks {

extern "C" {

ib_status_t ibpp_cfgmap_get(
    const void*       base,
    void*             out_value,
    const ib_field_t* field,
    void*             cbdata
)
{
    assert(base != NULL);

    try {
        data_to_value<
            configuration_map_init_getter_translator_t
        >(cbdata)(
            base, out_value, field
        );
    }
    catch (...) {
        return convert_exception();
    }
    return IB_OK;
}

ib_status_t ibpp_cfgmap_set(
    void*       base,
    ib_field_t* field,
    void*       in_value,
    void*       cbdata
)
{
    assert(base != NULL);

    try {
        data_to_value<
            configuration_map_init_setter_translator_t
        >(cbdata)(
            base, field, in_value
        );
    }
    catch (...) {
        return convert_exception();
    }
    return IB_OK;
}

ib_status_t ibpp_cfgmap_handle_get(
    const void*       handle,
    void*             out_value,
    const ib_field_t* field,
    void*             cbdata
)
{
    assert(handle != NULL);

    return ibpp_cfgmap_get(
        *reinterpret_cast<const void* const*>(handle),
        out_value,
        field,
        cbdata
    );
}

ib_status_t ibpp_cfgmap_handle_set(
    void*       handle,
    ib_field_t* field,
    void*       value,
    void*       cbdata
)
{
    assert(handle != NULL);

    return ibpp_cfgmap_set(
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
    ib_mm_t mm,
    configuration_map_init_getter_translator_t getter_translator,
    configuration_map_init_setter_translator_t setter_translator,
    bool data_is_handle
)
{
    if (data_is_handle) {
        init.fn_get = Hooks::ibpp_cfgmap_handle_get;
        init.fn_set = Hooks::ibpp_cfgmap_handle_set;
    }
    else {
        init.fn_get = Hooks::ibpp_cfgmap_get;
        init.fn_set = Hooks::ibpp_cfgmap_set;
    }
    init.cbdata_get = value_to_data(
        getter_translator,
        mm
    );
    init.cbdata_set = value_to_data(
        setter_translator,
        mm
    );
}

} // Internal

} // IronBee
