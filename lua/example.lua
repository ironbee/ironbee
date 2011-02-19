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
-- This is an example IronBee lua module.
--
-- Author: Brian Rectanus <brectanus@qualys.com>
-- =========================================================================


-- ===============================================
-- Define local aliases of any globals to be used.
-- ===============================================
local base = _G
local ironbee = require("ironbee")

-- ===============================================
-- Declare the rest of the file as a module and
-- register the module table with ironbee.
-- ===============================================
module(...)
_COPYRIGHT = "Copyright (C) 2010-2011 Qualys, Inc."
_DESCRIPTION = "IronBee example Lua module"
_VERSION = "0.1"

-- TODO: This needs to eventually go away
ironbee.register_module(_M)


-- ===============================================
-- ===============================================
-- Event Handlers
--
-- NOTE: As a best practice, you should avoid
-- using the "onEvent" prefix in any public
-- functions that are NOT to be used as event
-- handlers as these may be treated specially
-- by the engine.
-- ===============================================
-- ===============================================

-- ===============================================
-- This is called when the request headers are
-- avalable to inspect.
--
-- ib: IronBee engine handle
-- tx: IronBee transaction handle
-- ===============================================
function onEventHandleRequestHeaders(ib, tx)
    ironbee.log_debug(ib, 4, "Lua: %s.onEventHandleRequestHeaders", _NAME)
 
    -- Do something interesting

    return 0
end
