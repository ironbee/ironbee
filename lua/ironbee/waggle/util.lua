#!/usr/bin/lua

--[[--------------------------------------------------------------------------
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
--]]--------------------------------------------------------------------------

-- ###########################################################################
-- Utility functions.
-- ###########################################################################
local Util = {}

-- Return the type of the given object.
--
-- This first checks if the object has a field named "type" of type string.
-- If so, that is returned. Otherwise the normal type(...) call is called.
Util.type = function(self, obj)
    if obj == nil and self ~= _M then
        obj = self
    end

    if type(obj) == 'table' and type(obj.type) == 'string' then
        return obj.type
    end

    return type(obj)
end

Util.fatal = function(self, msg)
    if type(self) == 'string' then
        msg = self
    end

    local level = 1
    local info = debug.getinfo(level, 'lSn')

    print("--[[FATAL: ", msg, "--]]")
    print("--[[FATAL-Stack Trace---------")
    while info do
        print(string.format("-- %s:%s", info.short_src, info.currentline))
        level = level + 1
        info = debug.getinfo(level, 'lSn')
    end
    print("--]]")
end

return Util
