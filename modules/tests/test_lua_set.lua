local mod = ...

mod:declare_config {
    mod:num("num", 0),
    mod:string("str", ''),
    mod:string('str2', '')
}

mod:conn_started_state(function(conn)
    conn:logInfo("Num is %d", tonumber(conn.config.num))
    conn:logInfo("Str is %s", ffi.string(conn.config.str))
    conn:logInfo("Str2 is %s", ffi.string(conn.config.str2))
    return 0
end)

mod:register_param1_directive("LuaTestDirectiveP1", function(mod, ctx, name, p1)
    mod:logInfo("Processing directive "..name)
    mod:logInfo("Setting str2 to "..p1)
    local cfg = mod:get_config(ctx)
    local mm = ffi.C.ib_engine_mm_main_get(mod.ib_engine)
    cfg.str2 = ffi.C.ib_mm_strdup(mm, p1)
end)

return 0