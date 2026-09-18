-- New game save/load using dse.serialize.
local core = require("core")
local S = {}
local FILE = core.ROOT .. "wuxia_arpg_save.dat"
function S.save(P, seed, extra)
    local data = {ver=1, seed=seed, x=P.x, z=P.z, hp=P.hp, mp=P.mp, hp_max=P.hp_max, mp_max=P.mp_max,
                  level=P.level, exp=P.exp, exp_next=P.exp_next, gold=P.gold, atk=P.atk, def=P.def,
                  items=P.items, item_seq=P.item_seq, equip=P.equip,
                  map_id=extra and extra.map_id, weather=extra and extra.weather}
    local ok, blob = pcall(dse.serialize.encode, data)
    if not ok then return false, "encode" end
    local f = io.open(FILE, "wb")
    if not f then return false, "open" end
    f:write(blob); f:close(); return true
end
function S.load(P)
    local f = io.open(FILE, "rb")
    if not f then return false, "missing" end
    local blob = f:read("*a"); f:close()
    local ok, data = pcall(dse.serialize.decode, blob)
    if not ok or type(data)~="table" then return false, "decode" end
    for _,k in ipairs({"x","z","hp","mp","hp_max","mp_max","level","exp","exp_next","gold","atk","def","item_seq"}) do
        if data[k] ~= nil then P[k]=data[k] end
    end
    P.items = data.items or P.items or {}
    P.equip = data.equip or P.equip or { weapon=nil, armor=nil, accessory=nil }
    for slot,item in pairs(P.equip) do
        if item and item.uid then
            for _,candidate in ipairs(P.items) do
                if candidate.uid == item.uid then P.equip[slot] = candidate; break end
            end
        else P.equip[slot] = nil end
    end
    return true, data.seed, data
end
return S