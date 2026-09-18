-- R6 save/load test: player, equipment, map id, weather metadata.
local D = require("data")
local Loot = require("loot")
local Save = require("save")
local function check(c,m) if not c then error("[r6-save] FAIL "..(m or "assert")) end end
function Awake()
    Loot.init(12345)
    local item = Loot.roll(7, "rare")
    local P = { x=3.5, z=9.5, hp=90, mp=40, hp_max=150, mp_max=80, level=5, exp=12, exp_next=120,
                gold=66, atk=17, def=9, items={item}, item_seq=1,
                equip={ weapon=nil, armor=nil, accessory=nil } }
    P.equip[item.slot] = item
    local ok, err = Save.save(P, 12345, { map_id="blackwind_stronghold", weather="storm" })
    check(ok, "save "..tostring(err))
    local P2 = { items={}, equip={} }
    local ok2, seed, data = Save.load(P2)
    check(ok2 and seed==12345, "load seed")
    check(data.map_id=="blackwind_stronghold" and data.weather=="storm", "map/weather metadata")
    check(#P2.items==1 and P2.items[1].uid==item.uid, "item roundtrip")
    local eq = P2.equip and P2.equip[item.slot]
    check(eq and eq.uid==item.uid, "equip relink")
    D.load_map(data.map_id)
    check(D.current_id=="blackwind_stronghold", "map exists")
    print(string.format("[r6-save] PASS map=%s weather=%s items=%d", data.map_id, data.weather, #P2.items))
    dse.app.quit()
end
function Update(dt) local _=dt end