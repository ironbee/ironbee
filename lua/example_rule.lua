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
-- Example lua rule
-- This example compares the Host from the request header
--  to a constant string, and returns 1 if it matches,
--  or 0 if not.
--
-- Author: Nick LeRoy <nleroy@qualys.com>
-- =========================================================================

-- To use this rule in an ironbee.conf file add the line
--   RuleExt lua:rule.lua id:luarule01 phase:REQUEST_HEADER
--   Add additional actions such as block:immediate or "msg:A a rule fired."


-- Capture the table of inputs to this script.
t=...

-- The ib value which is the Ironbee API object. Most of our work is dispatched
-- off of this object.
local ib = t.ib

-- The IronBee transaction C struct.
local tx = t.ib_tx

-- The IronBee engine C struct.
local engine = t.ib_engine

-- The IronBee rule execution context C struct.
local ruleexec = t.ib_rule_exec

-- Log using the rule logging framework that we are starting to execute our rule.
ib:logDebug("Executing rule.")

-- Grab the HTTP Host header value.
local host = ib:get("REQUEST_HEADERS:Host")

-- Do our check.
if host ~= nil and host == "myhost.mydomain.com" then
    ib:logDebug("Host is myhost.mydomain.com. Creating an event.")

    -- Suppress all previous events that may have been generated.
    -- This is not necessary, but is how we want our rule to operate.
    ib:forEachEvent(function(evt)
        evt:setSuppress("other")
    end)

    -- Now create a event that we want to be generated for this transaction.
    ib:addEvent("Found target hostname.", {
            recommended_action = "log",
            action = "log",
            confidence = 1,
            severity = 0.1,
            tags = { "hostname", "mydomain" },
            fields = { "REQUEST_HEADESR:Host" }
        })
    return 1
else
    ib:logDebug("Unrecognized host.")
    return 0
end

