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
--
-- =========================================================================
--
-- API - The base class holding generic and utility functions.
--
-- Author: Sam Baskinger <sbaskinger@qualys.com>
--
-- =========================================================================

local _M = {}
_M.__index = _M
_M._COPYRIGHT = "Copyright (C) 2010-2014 Qualys, Inc."
_M._DESCRIPTION = "IronBee Lua API Base Class"
_M._VERSION = "1.0"

-- Expose the Engine object.
_M.engineapi = require('ironbee/engine')

-- Expose the Tx object. This inherits from Engine.
_M.txapi = require('ironbee/tx')

-- Expose the Rule object. This inherits from Tx.
_M.ruleapi = require('ironbee/rules')

return _M
