-- ========================================================================
-- Licensed to Qualys, Inc. (QUALYS) under one or more
-- contributor license agreements.  See the NOTICE file distributed with
-- this work for additional information regarding copyright ownership.
-- QUALYS licenses this file to You under the Apache License, Version 2.0
-- (the "License"); you may not use this file except in compliance with
-- the License.  You may obtain a copy of the License at
--
-- http://www.apache.org/licenses/LICENSE-2.0
--
-- Unless required by applicable law or agreed to in writing, software
-- distributed under the License is distributed on an "AS IS" BASIS,
-- WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
-- See the License for the specific language governing permissions and
-- limitations under the License.
-- ========================================================================

local ibmod = ...

-- ========================================================================
-- This module processes events to:
--
-- * Supress potential False Positives
-- * Categorize
-- * Promote events to alerts
--
-- Categories for events must be specified via tags in the following form:
--
--     tag:cat/<category-name>
--
--   Example:
--     tag:cat/XSS
--
-- This module is designed to be used with the threat_level.lua module.
--
--
-- Directives:
--
--  EventProcessorCategoryFilter <category> <min-confidence>
--
--    Defines the minimum acceptable event confidence for a given
--    category.
--
--  EventProcessorDefaultFilter <min-confidence>
--
--    Defines the minimum acceptable event confidence for an
--    undefined category.
--
-- Usage:
--
--  # Load the lua support module
--  LoadModule "ibmod_lua.so"
--
--  # Load this lua module
--  LuaLoadModule "event_processor.lua"
--  LuaLoadModule "threat_level.lua"
--
--  # Set the Filters
--  EventProcessorCategoryFilter XSS 75
--  EventProcessorCategoryFilter SQLi 25
--  EventProcessorCategoryFilter LFi 25
--  EventProcessorCategoryFilter RFi 50
--  EventProcessorDefaultFilter 50
--
--  # Set the minimum allowed event confidence
--  # to be considered in the threat score calculation
--  ThreatLevelMinConfidence 25
--
--  # Define a site with rules to utilize this module
--  <Site default>
--      SiteId 138E7FB0-1129-4FA5-8A63-432CE0BBD37A
--      Service *:*
--      Hostname *
--
--      # Generate some events
--      Rule ARGS @rx foo id:test/1 rev:1 msg:TEST1 \
--                        tag:cat/XSS \
--                        event confidence:50 severity:70
--      Rule ARGS @rx bar id:test/2 rev:1 msg:TEST2 \
--                        tag:cat/SQLi \
--                        event confidence:50 severity:80
--      Rule ARGS @rx boo id:test/3 rev:1 msg:TEST3 \
--                        tag:cat/LFi \
--                        event confidence:10 severity:100
--
--      # Check the threat level, blocking >=75
--      Rule THREAT_LEVEL @ge 75 id:test/100 rev:1 phase:REQUEST \
--                               "msg:Testing THREAT_LEVEL" \
--                               event block:phase
--  </Site>
-- ========================================================================


-- ---------------------------------------------------------
-- Handle "EventProcessorCategoryFilter <category> <num>"
-- directive.
-- ---------------------------------------------------------
ibmod:register_param2_directive(
    "EventProcessorCategoryFilter",
    function(ib_module, module_config, name, param1, param2)
        local numval = tonumber(param2)

        -- Validate parameter
        if ((numval < 0) or (numval > 100)) then
            ibmod:logError("Directive \"%s %s %s\" confidence value must be in range 0-100",
                            name, param1, param2)
            return nil
        end

        -- Store in the config
        if module_config["cat-min"] == nil then
            module_config["cat-min"] = {}
        end
        module_config["cat-min"][param1] = numval

        return 0
    end
)

-- ---------------------------------------------------------
-- Handle "EventProcessorDefaultFilter <num>"
-- directive.
-- ---------------------------------------------------------
ibmod:register_param1_directive(
    "EventProcessorDefaultFilter",
    function(ib_module, module_config, name, param1)
        local numval = tonumber(param1)

        -- Validate parameter
        if ((numval < 0) or (numval > 100)) then
            ibmod:logError("Directive \"%s %s\" confidence value must be in range 0-100",
                            name, param1)
            return nil
        end

        -- Store in the config
        module_config["def-min"] = numval

        return 0
    end
)

-- ---------------------------------------------------------
-- Get the category from an event
-- ---------------------------------------------------------
local get_category = function(evt)
    local cat

    --[[ Run through all tags in the event and pick
         the last "cat/<cat-name>" tag, extracting
         the <cat-name> as the category. ]]
    evt:forEachTag(
        function(tag)
            local cat_str = string.match(tag, [[^cat/(.*)]])
            if cat_str ~= nil then
                cat = cat_str
            end
        end
    )

    return cat
end

-- ---------------------------------------------------------
-- Process the events.
-- ---------------------------------------------------------
local process_events = function(ib)
    --[[ Run through events, supressing events if needed. ]]
    for i,evt in ib:events() do
        if evt:getSuppress() == "none" then
            local cat = get_category(evt)

            if cat ~= nil then
                --[[ Fetch the minimum confidence based on the
                     event category or the default. ]]
                local min_confidence = ib.config["cat-min"][cat]
                if min_confidence == nil then
                    min_confidence = ib.config["def-min"]
                    if min_confidence == nil then
                        min_confidence = 0
                    end
                end

                --[[ Suppress if confidence is too low. ]]
                local c = evt:getConfidence()
                if c < min_confidence then
                    ib:logDebug("Supressing low confidence event: %s", evt:getMsg())
                    evt:setSuppress("false_positive")
                end
            end
        end
    end

    return 0
end

-- ---------------------------------------------------------
-- Generate alerts.
-- ---------------------------------------------------------
local generate_alerts = function(ib)
    local evt_cat = {}

    --[[ Run through events, generating scores for categorization. ]]
    for i,evt in ib:events() do
        if evt:getSuppress() == "none" then
            local cat = get_category(evt)

            if cat ~= nil then
                local s = evt:getSeverity()

                if evt_cat[cat] == nil then
                    evt_cat[cat] = { 1, s, { evt } }
                else
                    table.insert(evt_cat[cat][3], evt)
                    evt_cat[cat] = { evt_cat[cat][1] + 1, evt_cat[cat][2] + s, evt_cat[cat][3] }
                end
            end
        end
    end

    --[[ Run through events in the category and promote the
         events to alerts. ]]
    local cat_final
    local cat_severity = 0
    for cat,val in pairs(evt_cat) do
        if val[2] > cat_severity then
            cat_severity = val[2]
            cat_final = cat
        end
    end
    if cat_final ~= nil then
        for i,evt in pairs(evt_cat[cat_final][3]) do
            if evt:getType() ~= "alert" then
                ib:logInfo("Promoting event to alert status: %s", evt:getMsg())
                evt:setType("alert")
            end
        end
    end

    return 0
end

-- ---------------------------------------------------------
-- Process events when an event is triggered.
-- ---------------------------------------------------------
ibmod:handle_logevent_state(process_events)

-- ---------------------------------------------------------
-- Generate alerts before logging.
-- ---------------------------------------------------------
ibmod:handle_postprocess_state(generate_alerts)

-- Return IB_OK.
return 0
