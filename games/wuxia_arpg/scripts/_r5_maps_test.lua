-- R5 map/data test: three maps, weather, exits, spawns.
local D = require("data")
local Weather = require("weather")
local function check(c,m) if not c then error("[r5-maps] FAIL "..(m or "assert")) end end
function Awake()
    check(#D.map_order == 3, "map count")
    local expected = {
        qingxi_village = "blackwind_stronghold",
        blackwind_stronghold = "youhuang_valley",
        youhuang_valley = "blackwind_stronghold",
    }
    for _,id in ipairs(D.map_order) do
        D.load_map(id)
        check(D.current_id == id, "load "..id)
        check(#D.map == 18 and #D.map[1] == 24, "grid "..id)
        check(#D.spawns >= 1, "spawns "..id)
        check(#D.exits >= 1, "exits "..id)
        check(Weather.name(D.weather) ~= "", "weather "..id)
        local found = false
        for _,ex in ipairs(D.exits) do if ex.to == expected[id] then found = true end end
        check(found or id=="youhuang_valley", "connection "..id)
    end
    print(string.format("[r5-maps] PASS maps=%d weather=%s/%s/%s",
        #D.map_order, Weather.name(D.maps.qingxi_village.weather),
        Weather.name(D.maps.blackwind_stronghold.weather), Weather.name(D.maps.youhuang_valley.weather)))
    dse.app.quit()
end
function Update(dt) local _=dt end