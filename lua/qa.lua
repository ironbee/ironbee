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
-- This is a lua module to help with QA tasks.
--
-- Author: Brian Rectanus <brectanus@qualys.com>
-- =========================================================================


local base = _G
local ironbee = require("ironbee-ffi")

module(...)
_COPYRIGHT = "Copyright (C) 2010-2011 Qualys, Inc."
_DESCRIPTION = "IronBee QA Lua module"
_VERSION = "0.1"

function onModuleLoad(ib)

    -- Print out a value at each log level
    local i = 0
    while i <= 9 do
        ironbee.ib_log(ib, i, "Logging at level " .. i)
        i = i + 1
    end

    return 0
end

