-- =========================================================================
-- =========================================================================
-- Licensed to Qualys, Inc. (QUALYS) under one or more
-- contributor license agreements.  See the NOTICE file distributed with
-- this work for additional information regarding copyright ownership.
-- QUALYS licenses this file to You under the Apache License, Version 2.0
-- (the "License"); you may not use this file except in compliance with
-- the License.  You may obtain a copy of the License at
--
--     http://www.apache.org/licenses/LICENSE-2.0
--
-- Unless required by applicable law or agreed to in writing, software
-- distributed under the License is distributed on an "AS IS" BASIS,
-- WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
-- See the License for the specific language governing permissions and
-- limitations under the License.
-- =========================================================================
-- =========================================================================
--
-- This is an example IronBee lua module.  Essentially, this code is
-- executed on load, allowing the developer to register other functions
-- to get called when events fire at runtime.
--
-- This example registers a function to be executed when the main
-- configuration context closes. It then uses the C API directly to
-- iterate through the lists of registered objects (directives, operators,
-- actions, transformations) and dumps this information to the log. It was
-- intended to help make sure the documentation was in sync, but is a
-- useful example of how to use the C API directly in a lua module.
--
-- Author: Brian Rectanus <brectanus@qualys.com>
-- =========================================================================

ibmod = ...

local ibutil = require('ironbee/util')
local ffi = require('ffi')

local dump_registered = function(ib)
    local rc

    -- Only dump when the main context closes - ignore other contexts.
    if ib.ib_ctx ~= ffi.C.ib_context_main(ib.ib_engine) then
        return 0
    end

    -- Create a list to fetch the registered records
    local recs = ffi.new("ib_list_t*[1]")
    rc = ffi.C.ib_list_create(
        recs,
        ffi.C.ib_engine_mm_config_get(ib.ib_engine)
    )
    if rc ~= ffi.C.IB_OK then
        ib:logError("Failed to create new ib_list_t: %d",
                    tonumber(rc))
        return 1
    end

    -- Fetch the registered records.
    ffi.C.ib_list_clear(recs[0])
    rc = ffi.C.ib_config_registered_directives(ib.ib_engine,
                                               recs[0])
    if rc ~= ffi.C.IB_OK then
        ib:logError("Failed to fetch registered directives: %d",
                    tonumber(rc))
        return 1
    end

    ibutil.each_list_node(
        recs[0],
        function(data)
            local t
            if data.type == ffi.C.IB_DIRTYPE_ONOFF then
                t = "ONOFF"
            elseif data.type == ffi.C.IB_DIRTYPE_PARAM1 then
                t = "PARAM1"
            elseif data.type == ffi.C.IB_DIRTYPE_PARAM2 then
                t = "PARAM2"
            elseif data.type == ffi.C.IB_DIRTYPE_LIST then
                t = "LIST"
            elseif data.type == ffi.C.IB_DIRTYPE_OPFLAGS then
                t = "OPFLAGS"
            elseif data.type == ffi.C.IB_DIRTYPE_SBLK1 then
                t = "SBLK1"
            else
                t = "UNKNOWN"
            end
            ib:logInfo("DUMP DIRECTIVE %s [%s]", ffi.string(data.name), t)
        end,
        "ib_dirmap_init_t*"
    )

    -- Fetch the registered operators.
    ffi.C.ib_list_clear(recs[0])
    rc = ffi.C.ib_config_registered_operators(ib.ib_engine,
                                              recs[0])
    if rc ~= ffi.C.IB_OK then
        ib:logError("Failed to fetch registered operators: %d",
                    tonumber(rc))
        return 1
    end

    ibutil.each_list_node(
        recs[0],
        function(data)
            ib:logInfo("DUMP OPERATOR %s", ffi.string( ffi.C.ib_operator_name(data) ))
        end,
        "ib_operator_t*"
    )

    -- Fetch the registered transformations.
    ffi.C.ib_list_clear(recs[0])
    rc = ffi.C.ib_config_registered_transformations(ib.ib_engine,
                                                    recs[0])
    if rc ~= ffi.C.IB_OK then
        ib:logError("Failed to fetch registered transformations: %d",
                    tonumber(rc))
        return 1
    end

    ibutil.each_list_node(
        recs[0],
        function(data)
            ib:logInfo("DUMP TRANSFORMATION %s", ffi.string( ffi.C.ib_transformation_name(data) ))
        end,
        "ib_transformation_t*"
    )

    -- Fetch the registered actions.
    ffi.C.ib_list_clear(recs[0])
    rc = ffi.C.ib_config_registered_actions(ib.ib_engine,
                                            recs[0])
    if rc ~= ffi.C.IB_OK then
        ib:logError("Failed to fetch registered actions: %d",
                    tonumber(rc))
        return 1
    end

    ibutil.each_list_node(
        recs[0],
        function(data)
            ib:logInfo("DUMP ACTION %s", ffi.string( ffi.C.ib_action_name(data) ))
        end,
        "ib_action_t*"
    )

    return 0
end

ibmod:context_close_event(dump_registered)

return 0
