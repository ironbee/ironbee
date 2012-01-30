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
t=...

tx = ironbee.newTx(t.tx)
local host = ironbee.ib_data_get(tx.dpi(), "request_headers.Host")
if host ~= nil then
  if host.value() == "pages.cs.wisc.edu" then
    ironbee.ib_log_debug(tx.ib(), 4, "Host is pages.cs.wisc.edu")
    return 1
  end      
end

return 0
