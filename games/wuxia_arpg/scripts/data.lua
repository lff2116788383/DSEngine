local D = {}
D.map_order = { "qingxi_village", "blackwind_stronghold", "youhuang_valley" }

D.maps = {
    qingxi_village = {
        id = "qingxi_village", name = "暮色青溪村",
        w = 24, h = 18,
        spawn = { x = 12.5, z = 15.5 },
        weather = "leaf",
        weather_cycle = { "leaf", "rain", "clear" },
        grid = {
            "###########....#########",
            "#....TT.....,,,,,,,,...#",
            "#...,,,.....,,HH,,.....#",
            "#...,,,.....,,HH,,..L..#",
            "#...,,,,,,,,,,,,,,,,...#",
            "#....RR....,,,,,..TT...#",
            "#..........,,,,........#",
            "#..L...,,,,........L...#",
            "#......,,,,,,..........#",
            "#......,,,,,,....RR....#",
            "#..........,,,,........#",
            "#....TT....,,,,....TT..#",
            "#..........,,,,........#",
            "#......L...,,,,........#",
            "#..........,,,,........#",
            "#..........,,,,........#",
            "#..........,,,,........#",
            "########################",
        },
        exits = {
            { x = 12.5, z = 0.6, w = 4.0, h = 1.6, to = "blackwind_stronghold", entry = "south" },
        },
        spawns = {
            {kind="bandit", x=5.5, z=4.5},
            {kind="bandit", x=18.5, z=5.5},
            {kind="bandit", x=6.5, z=12.5},
            {kind="bandit_elite", x=17.5, z=12.5},
            {kind="bandit_elite", x=12.5, z=8.5},
        },
    },

    blackwind_stronghold = {
        id = "blackwind_stronghold", name = "夜雨黑风寨",
        w = 24, h = 18,
        spawn = { x = 12.5, z = 15.5 },
        weather = "storm",
        weather_cycle = { "storm", "rain", "clear" },
        grid = {
            "###########....#########",
            "#......TT......,,,,,,..#",
            "#......,,......,HH,,...#",
            "#...,,,,,,,,,,,,HH,,...#",
            "#...,,,,,,,,,,,,,,,,...#",
            "#....RR....,,,,,...L...#",
            "#..........,,,,........#",
            "#..L...,,,,,,,,,,,,....#",
            "#......,,,,,,,,,,,,....#",
            "#......,,....RR....,...#",
            "#..........,,,,........#",
            "#....TT....,,,,....TT..#",
            "#..........,,,,........#",
            "#......L...,,,,........#",
            "#..........,,,,........#",
            "#..........,,,,........#",
            "#..........,,,,........#",
            "#####....###############",
        },
        exits = {
            { x = 12.5, z = 17.4, w = 4.0, h = 1.6, to = "qingxi_village", entry = "north" },
            { x = 12.5, z = 0.6, w = 4.0, h = 1.6, to = "youhuang_valley", entry = "south" },
        },
        spawns = {
            {kind="bandit", x=5.5, z=4.5},
            {kind="bandit_elite", x=18.5, z=5.5},
            {kind="bandit_elite", x=6.5, z=12.5},
            {kind="bandit_champion", x=17.5, z=12.5},
            {kind="bandit_champion", x=12.5, z=8.5},
            {kind="boss_blood_blade", x=12.5, z=3.5},
        },
    },

    youhuang_valley = {
        id = "youhuang_valley", name = "幽篁秘谷",
        w = 24, h = 18,
        spawn = { x = 12.5, z = 15.5 },
        weather = "fog",
        weather_cycle = { "fog", "rain", "leaf" },
        grid = {
            "###########....#########",
            "#..BBBB......RR....BB..#",
            "#..BBBB......RR....BB..#",
            "#....,,........,,,,....#",
            "#....,,..TT....,,,,....#",
            "#..L.,,..TT....,,,,..L.#",
            "#....,,........,,,,....#",
            "#..BBBB......BBBB......#",
            "#..BBBB......BBBB......#",
            "#....,,........,,,,....#",
            "#....,,..RR....,,,,....#",
            "#..L.,,..RR....,,,,..L.#",
            "#....,,........,,,,....#",
            "#..BBBB......TT....BB..#",
            "#..BBBB......TT....BB..#",
            "#....,,,,,,,,,,,,,,....#",
            "#....,,,,,,,,,,,,,,....#",
            "#####....###############",
        },
        exits = {
            { x = 12.5, z = 17.4, w = 4.0, h = 1.6, to = "blackwind_stronghold", entry = "north" },
        },
        spawns = {
            {kind="bandit_elite", x=5.5, z=4.5},
            {kind="bandit_champion", x=18.5, z=5.5},
            {kind="bandit_champion", x=6.5, z=12.5},
            {kind="bandit_champion", x=17.5, z=12.5},
            {kind="bandit_elite", x=12.5, z=8.5},
        },
    },
}

function D.load_map(id)
    local m = D.maps[id] or D.maps[D.map_order[1]]
    D.current_id = m.id
    D.current = m
    D.name = m.name
    D.map = m.grid
    D.w, D.h = m.w, m.h
    D.spawn = { x = m.spawn.x, z = m.spawn.z }
    D.entries = m.entries or {}
    D.exits = m.exits or {}
    D.weather = m.weather
    D.weather_cycle = m.weather_cycle
    D.spawns = m.spawns
    D.solid = {}
    for row=1,D.h do
        D.solid[row] = {}
        for col=1,D.w do
            local ch = D.map[row]:sub(col,col)
            D.solid[row][col] = (ch=="T" or ch=="H" or ch=="R" or ch=="B" or ch=="#")
        end
    end
end

function D.tile_type(row,col)
    local ch = D.map[row] and D.map[row]:sub(col,col) or "#"
    if ch=="," then return "dirt" end
    if ch=="~" then return "water" end
    if ch=="T" or ch=="H" or ch=="R" or ch=="B" then return "grass" end
    if ch=="#" then return "stone" end
    return "grass"
end

function D.prop_for(row,col)
    local ch = D.map[row] and D.map[row]:sub(col,col) or ""
    if ch=="T" then return "tree" end
    if ch=="H" then return "house" end
    if ch=="R" then return "rock" end
    if ch=="L" then return "lantern" end
    if ch=="B" then return "bamboo" end
    return nil
end

D.enemy = {
    bandit = { hp=42, atk=8, speed=1.9, sight=7.0, range=1.15, cd=1.2, exp=14, gold=8, size={30,44}, ilvl=1, rank="normal", actor="bandit" },
    bandit_elite = { hp=74, atk=10, speed=2.15, sight=7.5, range=1.2, cd=1.05, exp=28, gold=18, size={34,48}, ilvl=4, rank="elite", actor="bandit" },
    bandit_champion = { hp=116, atk=13, speed=2.3, sight=8.0, range=1.25, cd=0.95, exp=46, gold=32, size={38,52}, ilvl=6, rank="champion", actor="bandit" },
    boss_blood_blade = { hp=320, atk=15, speed=2.0, sight=9.0, range=1.65, cd=1.2, exp=180, gold=140, size={44,62}, ilvl=9, rank="boss", actor="boss" },
}

D.load_map(D.map_order[1])
return D