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
-- IronBee Developer Lua Code
--
-- Populate the transaction with some development-focused targets.
--
-- @author Sam Baskinger <sbaskinger@qualys.com>
--
local mod = ...

mod:tx_started_state(function(tx, evt)
    local ib_tx = ffi.cast("ib_tx_t *", tx.ib_tx)
    tx:set("TX_ID", ffi.string(ib_tx.id))
    tx:set("CONN_ID", ffi.string(ib_tx.conn.id))
    tx:set("CTX_NAME_FULL",
        ffi.string(ffi.C.ib_context_full_get(ib_tx.ctx)))
    return 0
end)

return 0