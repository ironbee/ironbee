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
local ironbee = require("ironbee-ffi")

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
-- Normally only the onEventHandle* functions are
-- used for detection, but they are all listed
-- here.
--
-- NOTE: As a best practice, you should avoid
-- using the "onEvent" prefix in any public
-- functions that are NOT to be used as event
-- handlers as these may be treated specially
-- by the engine.
-- ===============================================
-- ===============================================

-- ===============================================
-- This is called when a connection is started.
--
-- ib: IronBee engine handle
-- conn: IronBee connection handle
-- ===============================================
function onEventConnStarted(ib, conn)
    ironbee.ib_log_debug(ib, 4, "%s.onEventConnStarted ib=%p conn=%p",
                       _NAME, ib.cvalue(), conn.cvalue())
    return 0
end

-- ===============================================
-- This is called when a connection is opened.
--
-- ib: IronBee engine handle
-- conn: IronBee connection handle
-- ===============================================
function onEventConnOpened(ib, conn)
    ironbee.ib_log_debug(ib, 4, "%s.onEventConnOpened ib=%p conn=%p",
                       _NAME, ib.cvalue(), conn.cvalue())
    return 0
end

-- ===============================================
-- This is called when a connection context was
-- chosen and is ready to be handled.
--
-- ib: IronBee engine handle
-- conn: IronBee connection handle
-- ===============================================
function onEventHandleContextConn(ib, conn)
    ironbee.ib_log_debug(ib, 4, "%s.onEventHandleContextConn ib=%p conn=%p",
                       _NAME, ib.cvalue(), conn.cvalue())
    return 0
end

-- ===============================================
-- This is called when the connection is ready to
-- be handled.
--
-- ib: IronBee engine handle
-- conn: IronBee connection handle
-- ===============================================
function onEventHandleConnect(ib, conn)
    ironbee.ib_log_debug(ib, 4, "%s.onEventHandleConnect ib=%p conn=%p",
                       _NAME, ib.cvalue(), conn.cvalue())
    return 0
end

-- ===============================================
-- This is called when the transaction starts.
--
-- ib: IronBee engine handle
-- tx: IronBee transaction handle
-- ===============================================
function onEventTxStarted(ib, tx)
    ironbee.ib_log_debug(ib, 4, "%s.onEventTxStarted ib=%p tx=%p",
                       _NAME, ib.cvalue(), tx.cvalue())
    return 0
end

-- ===============================================
-- This is called when a request starts.
--
-- ib: IronBee engine handle
-- tx: IronBee transaction handle
-- ===============================================
function onEventRequestStarted(ib, tx)
    ironbee.ib_log_debug(ib, 4, "%s.onEventRequestStarted ib=%p tx=%p",
                       _NAME, ib.cvalue(), tx.cvalue())
    return 0
end

-- ===============================================
-- This is called when the request headers are
-- available.
--
-- ib: IronBee engine handle
-- tx: IronBee transaction handle
-- ===============================================
function onEventRequestHeaders(ib, tx)
    ironbee.ib_log_debug(ib, 4, "%s.onEventRequestHeaders ib=%p tx=%p",
                       _NAME, ib.cvalue(), tx.cvalue())
    return 0
end

-- ===============================================
-- This is called when the transaction context
-- is ready to be handled.
--
-- ib: IronBee engine handle
-- tx: IronBee transaction handle
-- ===============================================
function onEventHandleContextTx(ib, tx)
    ironbee.ib_log_debug(ib, 4, "%s.onEventHandleContextTx ib=%p tx=%p",
                       _NAME, ib.cvalue(), tx.cvalue())
    return 0
end

-- ===============================================
-- This is called when the request headers are
-- avalable to inspect.
--
-- ib: IronBee engine handle
-- tx: IronBee transaction handle
-- ===============================================
function onEventHandleRequestHeaders(ib, tx)
    ironbee.ib_log_debug(ib, 4, "%s.onEventHandleRequestHeaders ib=%p tx=%p",
                       _NAME, ib.cvalue(), tx.cvalue())

    -- Request line is a scalar value (a field object type)
    local req_line = ironbee.ib_data_get(tx.dpi(), "request_line")
    ironbee.ib_log_debug(ib, 4, "Request line is a field type: %d", req_line.type())
    ironbee.ib_log_debug(ib, 4, "Request Line: %s", req_line.value())

    -- Request headers are a collection (table of field objects)
    local req_headers = ironbee.ib_data_get(tx.dpi(), "request_headers")
    ironbee.ib_log_debug(ib, 4, "Request headers is a field type: %d", req_headers.type())
    if req_headers.type() == ironbee.IB_FTYPE_LIST then
        for k,f in base.pairs(req_headers.value()) do
            if f.type() == ironbee.IB_FTYPE_LIST then
                ironbee.ib_log_debug(ib, 4, "Request Header: %s=<list>", k)
            else
                ironbee.ib_log_debug(ib, 4, "Request Header: %s=%s", k, f.value())
            end
        end
    end

    -- Request URI params are a collection (table of field objects)
    local req_uri_params = ironbee.ib_data_get(tx.dpi(), "request_uri_params")
    ironbee.ib_log_debug(ib, 4, "Request URI Params is a field type: %d", req_uri_params.type())
    if req_uri_params.type() == ironbee.IB_FTYPE_LIST then
        for k,f in base.pairs(req_uri_params.value()) do
            if f.type() == ironbee.IB_FTYPE_LIST then
                ironbee.ib_log_debug(ib, 4, "Request URI Param: %s=<list>", k)
            else
                ironbee.ib_log_debug(ib, 4, "Request URI Param: %s=%s", k, f.value())
            end
        end
    end
    return 0
end

-- ===============================================
-- This is called when the request body is
-- available.
--
-- ib: IronBee engine handle
-- tx: IronBee transaction handle
-- ===============================================
function onEventRequestBody(ib, tx)
    ironbee.ib_log_debug(ib, 4, "%s.onEventRequestBody ib=%p tx=%p",
                       _NAME, ib.cvalue(), tx.cvalue())
    return 0
end

-- ===============================================
-- This is called when the complete request is
-- ready to be handled.
--
-- ib: IronBee engine handle
-- tx: IronBee transaction handle
-- ===============================================
function onEventHandleRequest(ib, tx)
    ironbee.ib_log_debug(ib, 4, "%s.onEventHandleRequest ib=%p tx=%p",
                       _NAME, ib.cvalue(), tx.cvalue())
    return 0
end

-- ===============================================
-- This is called when the request is finished.
--
-- ib: IronBee engine handle
-- tx: IronBee transaction handle
-- ===============================================
function onEventRequestFinished(ib, tx)
    ironbee.ib_log_debug(ib, 4, "%s.onEventRequestFinished ib=%p tx=%p",
                       _NAME, ib.cvalue(), tx.cvalue())
    return 0
end

-- ===============================================
-- This is called when the transaction is ready
-- to be processed.
--
-- ib: IronBee engine handle
-- tx: IronBee transaction handle
-- ===============================================
function onEventTxProcess(ib, tx)
    ironbee.ib_log_debug(ib, 4, "%s.onEventTxProcess ib=%p tx=%p",
                       _NAME, ib.cvalue(), tx.cvalue())
    return 0
end

-- ===============================================
-- This is called when the response is started.
--
-- ib: IronBee engine handle
-- tx: IronBee transaction handle
-- ===============================================
function onEventResponseStarted(ib, tx)
    ironbee.ib_log_debug(ib, 4, "%s.onEventResponseStarted ib=%p tx=%p",
                       _NAME, ib.cvalue(), tx.cvalue())
    return 0
end

-- ===============================================
-- This is called when the response headers are
-- available.
--
-- ib: IronBee engine handle
-- tx: IronBee transaction handle
-- ===============================================
function onEventResponseHeaders(ib, tx)
    ironbee.ib_log_debug(ib, 4, "%s.onEventResponseHeaders ib=%p tx=%p",
                       _NAME, ib.cvalue(), tx.cvalue())
    return 0
end

-- ===============================================
-- This is called when the response headers are
-- ready to be handled.
--
-- ib: IronBee engine handle
-- tx: IronBee transaction handle
-- ===============================================
function onEventHandleResponseHeaders(ib, tx)
    ironbee.ib_log_debug(ib, 4, "%s.onEventHandleResponseHeaders ib=%p tx=%p",
                       _NAME, ib.cvalue(), tx.cvalue())
    return 0
end

-- ===============================================
-- This is called when the response body is
-- available.
--
-- ib: IronBee engine handle
-- tx: IronBee transaction handle
-- ===============================================
function onEventResponseBody(ib, tx)
    ironbee.ib_log_debug(ib, 4, "%s.onEventResponseBody ib=%p tx=%p",
                       _NAME, ib.cvalue(), tx.cvalue())
    return 0
end

-- ===============================================
-- This is called when the complete response is
-- ready to be handled.
--
-- ib: IronBee engine handle
-- tx: IronBee transaction handle
-- ===============================================
function onEventHandleResponse(ib, tx)
    ironbee.ib_log_debug(ib, 4, "%s.onEventHandleResponse ib=%p tx=%p",
                       _NAME, ib.cvalue(), tx.cvalue())
    return 0
end

-- ===============================================
-- This is called when the response is finished.
--
-- ib: IronBee engine handle
-- tx: IronBee transaction handle
-- ===============================================
function onEventResponseFinished(ib, tx)
    ironbee.ib_log_debug(ib, 4, "%s.onEventResponseFinished ib=%p tx=%p",
                       _NAME, ib.cvalue(), tx.cvalue())
    return 0
end

-- ===============================================
-- This is called after the transaction is done
-- and any post processing can be done.
--
-- ib: IronBee engine handle
-- tx: IronBee transaction handle
-- ===============================================
function onEventHandlePostprocess(ib, tx)
    ironbee.ib_log_debug(ib, 4, "%s.onEventHandlePostprocess ib=%p tx=%p",
                       _NAME, ib.cvalue(), tx.cvalue())
    return 0
end

-- ===============================================
-- This is called when the transaction is
-- finished.
--
-- ib: IronBee engine handle
-- tx: IronBee transaction handle
-- ===============================================
function onEventTxFinished(ib, tx)
    ironbee.ib_log_debug(ib, 4, "%s.onEventTxFinished ib=%p tx=%p",
                       _NAME, ib.cvalue(), tx.cvalue())
    return 0
end

-- ===============================================
-- This is called when a connection is closed.
--
-- ib: IronBee engine handle
-- conn: IronBee connection handle
-- ===============================================
function onEventConnClosed(ib, conn)
    ironbee.ib_log_debug(ib, 4, "%s.onEventConnClosed ib=%p conn=%p",
                       _NAME, ib.cvalue(), conn.cvalue())
    return 0
end

-- ===============================================
-- This is called when the connection disconnect
-- is ready to handle.
--
-- ib: IronBee engine handle
-- conn: IronBee connection handle
-- ===============================================
function onEventHandleDisconnect(ib, conn)
    ironbee.ib_log_debug(ib, 4, "%s.onEventHandleDisconnect ib=%p conn=%p",
                       _NAME, ib.cvalue(), conn.cvalue())
    return 0
end

-- ===============================================
-- This one cannot be used in Lua as the Lua
-- state is destroyed before it is called.
--
-- -- ===============================================
-- -- This is called when a connection is finished.
-- --
-- -- ib: IronBee engine handle
-- -- conn: IronBee connection handle
-- -- ===============================================
-- function onEventConnFinished(ib, conn)
--     ironbee.ib_log_debug(ib, 4, "%s.onEventConnFinished ib=%p conn=%p",
--                        _NAME, ib, conn)
--     return 0
-- end
-- ===============================================

-- ===============================================
-- This is called when there is incoming data for
-- the connection.
--
-- ib: IronBee engine handle
-- conndata: IronBee connection data handle
-- ===============================================
function onEventConnDataIn(ib, conndata)
    ironbee.ib_log_debug(ib, 4, "%s.onEventConnDataIn: %.*s",
                       _NAME,
                       conndata.dlen(), conndata.data())
    return 0
end

-- ===============================================
-- This is called when there is outgoing data for
-- the connection.
--
-- ib: IronBee engine handle
-- conndata: IronBee connection data handle
-- ===============================================
function onEventConnDataOut(ib, conndata)
    ironbee.ib_log_debug(ib, 4, "%s.onEventConnDataOut: %.*s",
                       _NAME,
                       conndata.dlen(), conndata.data())
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
    ironbee.ib_log_debug(ib, 4, "%s.onEventTxDataIn[%d]: %.*s",
                       _NAME,
                       txdata.dtype(),
                       txdata.dlen(), txdata.data())
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
    ironbee.ib_log_debug(ib, 4, "%s.onEventTxDataOut[%d]: %.*s",
                       _NAME,
                       txdata.dtype(),
                       txdata.dlen(), txdata.data())
    return 0
end

