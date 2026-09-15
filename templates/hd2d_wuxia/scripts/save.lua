-- ============================================================================
-- save.lua  存档（dse.serialize + 文件）
-- ============================================================================
local S = {}
local FILE = "hd2d_save.dat"

function S.save(G, P)
    local data = {
        ver = 1,
        map_id = G.map_id,
        quest_index = G.quest_index,
        kills = G.kills,
        px = P.x, py = P.y,
        hp = P.hp, mp = P.mp, level = P.level, exp = P.exp, gold = P.gold,
        inv = P.inv, equip = P.equip,
        atk = P.atk, def = P.def, hp_max = P.hp_max, mp_max = P.mp_max,
    }
    local ok, blob = pcall(dse.serialize.encode, data)
    if not ok then return false, "序列化失败" end
    local f = io.open(FILE, "wb")
    if not f then return false, "无法写入存档" end
    f:write(blob)
    f:close()
    return true
end

function S.load(G, P)
    local f = io.open(FILE, "rb")
    if not f then return false, "没有存档" end
    local blob = f:read("*a")
    f:close()
    local ok, data = pcall(dse.serialize.decode, blob)
    if not ok or type(data) ~= "table" then return false, "存档损坏" end
    G.map_id = data.map_id or G.map_id
    G.quest_index = data.quest_index or 1
    G.kills = data.kills or {}
    P.x, P.y = data.px or P.x, data.py or P.y
    P.level, P.exp, P.gold = data.level or 1, data.exp or 0, data.gold or 0
    P.inv = data.inv or P.inv
    P.equip = data.equip or P.equip
    P.atk, P.def = data.atk or P.atk, data.def or P.def
    P.hp_max, P.mp_max = data.hp_max or P.hp_max, data.mp_max or P.mp_max
    P.hp = math.min(data.hp or P.hp_max, P.hp_max)
    P.mp = math.min(data.mp or P.mp_max, P.mp_max)
    return true
end

function S.exists()
    local f = io.open(FILE, "rb")
    if f then f:close() return true end
    return false
end

return S