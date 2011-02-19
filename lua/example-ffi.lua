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
-- This is an example IronBee lua module using the new FFI interface.
--
-- Author: Brian Rectanus <brectanus@qualys.com>
-- =========================================================================


-- ===============================================
-- Define local aliases of any globals to be used.
-- ===============================================
local base = _G
local ibffi = require("ironbee-ffi")

-- ===============================================
-- Declare the rest of the file as a module and
-- register the module table with ironbee.
-- ===============================================
module(...)
_COPYRIGHT = "Copyright (C) 2010-2011 Qualys, Inc."
_DESCRIPTION = "IronBee example Lua module"
_VERSION = "0.1"

-- TODO: This needs to eventually go away
ibffi.register_module(_M)


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
    local c_tx = ibffi.cast_tx(tx);

    ibffi.ib_log_debug(ib, 4, "%s.onEventHandleRequestHeaders ib=%p tx=%p",
                       _NAME, ib, tx)
 
    local req_line = ibffi.ib_data_get(c_tx.dpi, "request_line")
    ibffi.ib_log_debug(ib, 4, "Request Line: %s", req_line);

    -- Do something interesting

    return 0
end

-- ===============================================
-- This is called when there is incoming data for
-- the transaction.
--
-- ib: IronBee engine handle
-- txdata: IronBee transaction data handle
-- ===============================================
function onEventTxDataIn(ib, txdata)
    local c_txdata = ibffi.cast_txdata(txdata);

    -- Just dump the data to the logs
    ibffi.ib_log_debug(ib, 4, "%s.onEventTxDataIn[%d]: %.*s",
                       _NAME,
                       ibffi.cast_int(c_txdata.dtype),
                       ibffi.cast_int(c_txdata.dlen),
                       c_txdata.data)
 
    return 0
end

-- ===============================================
-- This is called when there is outgoing data for
-- the transaction.
--
-- ib: IronBee engine handle
-- txdata: IronBee transaction data handle
-- ===============================================
function onEventTxDataOut(ib, txdata)
    local c_txdata = ibffi.cast_txdata(txdata);

    -- Just dump the data to the logs
    ibffi.ib_log_debug(ib, 4, "%s.onEventTxDataOut[%d]: %.*s",
                       _NAME,
                       ibffi.cast_int(c_txdata.dtype),
                       ibffi.cast_int(c_txdata.dlen),
                       c_txdata.data)
 
    return 0
end

