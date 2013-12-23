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

--
-- IronBee Waggle --- Validator
--
-- Validates a collection of Waggle rules.
--
-- @author Sam Baskinger <sbaskinger@qualys.com>
--
--[[-------------------------------------------------------------------------
-- Validates a Signature Database and Plan for possible errors.
--]]-------------------------------------------------------------------------
local Util = require('ironbee/waggle/util')
local Rule = require('ironbee/waggle/rule')
local Action = require('ironbee/waggle/actionrule')
local SignatureDatabase = require('ironbee/waggle/signaturedatabase')
local StreamInspect = require('ironbee/waggle/streaminspect')

Validator = {}
Validator.__index = Validator
Validator.type = "Validator"
Validator.new = function(self)
    return setmetatable({
        warnings = {},
        errors = {}
    }, self)
end

local phases = {
    REQUEST_HEADER  = 1,
    REQUEST         = 1,
    RESPONSE_HEADER = 1,
    RESPONSE        = 1,
    POSTPROCESS     = 1
}

-- These are fields that the validator should assume exist.
-- Note that they are all all-upper-case.
local predefined_fields = {
        ARGS                   = 1,
        AUTH_PASSWORD          = 1,
        AUTH_TYPE              = 1,
        AUTH_USERNAME          = 1,
        CAPTURE                = 1,
        FIELD                  = 1,
        FIELD_NAME             = 1,
        FIELD_NAME_FULL        = 1,
        GEOIP                  = 1,
        HTP_REQUEST_FLAGS      = 1,
        HTP_RESPONSE_FLAGS     = 1,
        FLAGS                  = 1,
        LAST_MATCHED           = 1,
        REMOTE_ADDR            = 1,
        REMOTE_PORT            = 1,
        REQUEST_BODY           = 1,
        REQUEST_BODY_PARAMS    = 1,
        REQUEST_CONTENT_TYPE   = 1,
        REQUEST_COOKIES        = 1,
        REQUEST_FILENAME       = 1,
        REQUEST_HEADERS        = 1,
        REQUEST_HOST           = 1,
        REQUEST_LINE           = 1,
        REQUEST_METHOD         = 1,
        REQUEST_PROTOCOL       = 1,
        REQUEST_URI            = 1,
        REQUEST_URI_FRAGMENT   = 1,
        REQUEST_URI_HOST       = 1,
        REQUEST_URI_PARAMS     = 1,
        REQUEST_URI_PASSWORD   = 1,
        REQUEST_URI_PATH       = 1,
        REQUEST_URI_PORT       = 1,
        REQUEST_URI_RAW        = 1,
        REQUEST_URI_SCHEME     = 1,
        REQUEST_URI_QUERY      = 1,
        REQUEST_URI_USERNAME   = 1,
        RESPONSE_BODY          = 1,
        RESPONSE_CONTENT_TYPE  = 1,
        RESPONSE_COOKIES       = 1,
        RESPONSE_HEADERS       = 1,
        RESPONSE_LINE          = 1,
        RESPONSE_MESSAGE       = 1,
        RESPONSE_PROTOCOL      = 1,
        RESPONSE_STATUS        = 1,
        SERVER_ADDR            = 1,
        SERVER_PORT            = 1,
        SITE_NAME              = 1,
        TX                     = 1,
        UA                     = 1,
}

-- Build a table of information to report
-- into a warning or an error by.
local build_table = function(rule, msg)
    local dbi = debug.getinfo(4, "nSl")
    return {
        line = dbi.currentline,
        source = dbi.source,
        short_src = dbi.short_src,
        sig_id = rule.data.id,
        sig_rev = rule.data.version,
        msg = msg
    }
end

-- Check that a rule is ready from only defined fields.
-- Not part of the validator API.
local check_defined_fields = function(validator, rule, defined_fields)
    for i, field in ipairs(rule.data.fields) do
        if defined_fields[string.upper(field.collection)] == nil then
            validator:warning(rule, string.format("Field %s read before defined.", field.collection))
        end
    end
end

local define_fields = function(self, rule, defined_fields)
    for i, action in ipairs(rule.data.actions) do
        local action_name = string.upper(action.name)
        if ( action_name == "setvar" or
             action_name == "setflag" or
             action_name == "setRequestHeader" or
             action_name == "setResponseHeader") and
           action.argument then
            defined_fields[string.upper(action.argument)] = 1
        end
    end
end

Validator.error = function(self, rule, msg)
    table.insert(self.errors, build_table(rule, msg))
end

Validator.warning = function(self, rule, msg)
    table.insert(self.warnings, build_table(rule, msg))
end

Validator.has_warnings = function(self)
    return #self.warnings > 0
end

Validator.has_errors = function(self)
    return #self.errors > 0
end

Validator.validate = function(self, db,  plan)

    -- Fields that we've seen already stored as all upper-case.
    local defined_fields = setmetatable({}, { __index = predefined_fields })

    for rule_list_idx, rule_list in ipairs(plan) do
        for rule_idx, rule_exec in ipairs(rule_list) do

            local rule_id = rule_exec.rule
            local rule_result = rule_exec.result
            local rule = db.db[rule_id]

            if rule:is_a(Rule) and #rule.data.fields == 0 and not rule.data.has_predicate then
                self:warning(rule, "Missing fields")
            end

            if rule:is_a(Rule) and (rule.data.op == nil or rule.data.op == '') and not rule.data.has_predicate then
                self:warning(rule, "Missing operator. Eg :op(\"rx\", \".*\").")
            end

            if rule:is_a(Rule) then
                -- Check that we do not use a field before it is defined.
                check_defined_fields(self, rule, defined_fields)
            end

            if rule:is_a(Rule) or rule:is_a(Action) then
                -- Record fields we define.
                define_fields(self, rule, defined_fields)
            end

            if rule.data.phase == nil then
                self:error(rule, string.format("Undefined phase."))
            elseif phases[rule.data.phase] == nil then
                self:error(rule, string.format("Invalid phase %s", rule.data.phase))
            end

            if rule:is_a(StreamInspect) then
                if #rule.data.fields ~= 1 then
                    self:error(rule, "Stream Inspect rules may only have 1 field defined. "..
                                     "REQUEST_BODY_STREAM or RESPONSE_BODY_STREAM")
                elseif rule.data.fields[1].collection ~= "REQUEST_BODY_STREAM" and
                       rule.data.fields[1].collection ~= "RESPONSE_BODY_STREAM" then
                    self:error(rule, "Only REQUEST_BODY_STREAM or RESPONSE_BODY_STREAM are valid fields.")
                end
            end
        end
    end
end

return Validator
