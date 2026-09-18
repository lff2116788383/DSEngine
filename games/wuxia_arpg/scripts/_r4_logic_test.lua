-- R4 logic test: loot stats, auto-equip, save/load with equipment.
local Rng = require("rng")
local Loot = require("loot")
local P = require("player")
local Save = require("save")
local function check(c,m) if not c then error("[r4-logic] FAIL "..(m or "assert")) end end
function Awake()
    Loot.init(12345); math.randomseed(12345)
    local item = Loot.roll(6,"rare")
    check(item and item.stats, "item stats")
    check(type(item.affixes)=="table" and #item.affixes>=0, "affixes")
    check((item.stats.atk or 0) + (item.stats.def or 0) + (item.stats.hp or 0) > 0, "positive stats")
    local p1 = Loot.power(item)
    local item2 = Loot.roll(6,"rare")
    check(type(p1)=="number" and p1>0, "power")
    P.items, P.item_seq = {}, 0
    P.equip = { weapon=nil, armor=nil, accessory=nil }
    P.add_equipment(item)
    check(P.equip[item.slot] ~= nil, "auto equip")
    local s = P.stats()
    check(s.atk >= P.atk and s.hp_max >= P.hp_max, "equip stat applied")
    local ok, err = Save.save(P, 12345); check(ok, "save "..tostring(err))
    local P2 = {items={},equip={}}
    local ok2, seed = Save.load(P2); check(ok2, "load "..tostring(seed))
    check(#P2.items == #P.items and seed == 12345, "item roundtrip")
    local eq = P2.equip and P2.equip[item.slot]
    check(eq and eq.uid == item.uid, "equip relink")
    print(string.format("[r4-logic] PASS power=%.1f items=%d equip=%s", p1, #P.items, item.slot))
    dse.app.quit()
end
function Update(dt) local _=dt end