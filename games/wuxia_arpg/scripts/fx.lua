-- Tiny FX/toast bus for the new game (R3 keeps visuals in HUD; no template FX reuse).
local core = require("core")
local Fx = { msg = "", msg_t = 0, demo_target = nil }
function Fx.toast(msg)
    Fx.msg, Fx.msg_t = msg or "", 2.0
    core.accept_log("toast=%s", tostring(msg))
end
function Fx.popup(text, x, z)
    local _ = {x,z}
    core.accept_log("popup=%s", tostring(text))
end
function Fx.update(dt)
    if Fx.msg_t > 0 then Fx.msg_t = math.max(0, Fx.msg_t - dt) end
end
return Fx