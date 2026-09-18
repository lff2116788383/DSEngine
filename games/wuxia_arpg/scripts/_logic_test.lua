-- R3 logic test for the new game: deterministic RNG, loot prototype, save/load.
local Rng = require("rng")
local Loot = require("loot")
local Save = require("save")
local function check(c,m) if not c then error("[r3-logic] FAIL "..(m or "assert")) end end
function Awake()
    local a,b = Rng.new(12345), Rng.new(12345)
    local x1 = a:next_u32()
    check(x1 == b:next_u32(), "rng repeat")
    Loot.init(12345)
    local items = {}
    local seen = {}
    for i=1,30 do
        local it = Loot.roll(5,"normal")
        check(type(it)=="table" and type(it.base_id)=="string", "item shape")
        items[i]=it; seen[it.rarity]=true
    end
    check(seen.common or seen.magic or seen.rare or seen.legendary, "rarity")
    Loot.init(12345); local s1=Loot.roll(5,"normal")
    Loot.init(12345); local s2=Loot.roll(5,"normal")
    check(s1.base_id==s2.base_id and s1.rarity==s2.rarity and #s1.affixes==#s2.affixes, "deterministic loot")
    local P = {x=1.25,z=2.5,hp=72,mp=31,hp_max=120,mp_max=60,level=3,exp=5,exp_next=84,gold=42,
               atk=16,def=7,items=items,item_seq=#items}
    local ok,err = Save.save(P,12345); check(ok, "save "..tostring(err))
    local P2 = {}
    local ok2,seed = Save.load(P2); check(ok2, "load "..tostring(seed))
    check(P2.level==3 and P2.gold==42 and #(P2.items or {})==#items and seed==12345, "roundtrip values")
    print(string.format("[r3-logic] PASS rng=%d items=%d save_items=%d", x1, #items, #(P2.items or {})))
    dse.app.quit()
end
function Update(dt) local _=dt end