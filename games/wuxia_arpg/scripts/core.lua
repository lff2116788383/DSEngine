-- New game core helpers: paths, math, input, acceptance logging.
local M = {}
M.ROOT = "games/wuxia_arpg/"
M.KEY = { W=87,A=65,S=83,D=68,J=74,K=75,U=85,I=73,E=69,R=82,SPACE=32,ENTER=257,TAB=258,ESC=256,ONE=49,TWO=50 }
M.accept_enabled = (os and os.getenv and os.getenv("DSE_WUXIA_ACCEPT") == "1") or false

function M.accept_log(fmt, ...)
    if M.accept_enabled then print(string.format("[wuxia] " .. fmt, ...)) end
end
function M.clamp(v,a,b) if v<a then return a end if v>b then return b end return v end
function M.lerp(a,b,t) return a+(b-a)*t end
function M.sign(v) if v>0 then return 1 end if v<0 then return -1 end return 0 end
function M.dist(x1,z1,x2,z2) local dx,dz=x1-x2,z1-z2 return math.sqrt(dx*dx+dz*dz) end
function M.round(v,n) local p=10^(n or 0) return math.floor(v*p+0.5)/p end
function M.file_exists(p) local f=io.open(p,"rb") if f then f:close() return true end return false end
function M.resolve(p)
    for _,c in ipairs({ M.ROOT..p, M.ROOT.."assets/"..p, p, "assets/"..p }) do
        if M.file_exists(c) then return c end
    end
    return M.ROOT..p
end
function M.axis_x()
    local app=dse.app
    if app.get_key(M.KEY.A) or app.get_key(263) then return -1 end
    if app.get_key(M.KEY.D) or app.get_key(262) then return 1 end
    return 0
end
function M.axis_z()
    local app=dse.app
    if app.get_key(M.KEY.W) or app.get_key(265) then return -1 end
    if app.get_key(M.KEY.S) or app.get_key(264) then return 1 end
    return 0
end
function M.key_down(k) return dse.app.get_key_down(k) end
function M.key(k) return dse.app.get_key(k) end
function M.dir_from_vec(dx,dz)
    if math.abs(dx)>math.abs(dz) then return dx>=0 and "r" or "l" end
    return dz<0 and "u" or "d"
end
return M