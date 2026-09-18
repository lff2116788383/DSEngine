local D = {}
D.map = {
    "########################",
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
}
D.w, D.h = 24, 18
D.spawn = { x = 12.0, z = 15.0 }
D.solid = {}
for row=1,#D.map do
    D.solid[row] = {}
    for col=1,#D.map[row] do
        local ch = D.map[row]:sub(col,col)
        D.solid[row][col] = (ch=="T" or ch=="H" or ch=="R" or ch=="#")
    end
end
D.tile_type = function(row,col)
    local ch = D.map[row] and D.map[row]:sub(col,col) or "#"
    if ch=="," then return "dirt" end
    if ch=="~" then return "water" end
    if ch=="T" then return "grass" end
    if ch=="H" then return "grass" end
    if ch=="R" then return "grass" end
    if ch=="#" then return "stone" end
    return "grass"
end
D.enemy = {
    bandit = { hp=42, atk=8, speed=1.9, sight=7.0, range=1.15, cd=1.2, exp=14, gold=8, size={30,44}, ilvl=1, rank="normal", actor="bandit" },
    bandit_elite = { hp=74, atk=10, speed=2.15, sight=7.5, range=1.2, cd=1.05, exp=28, gold=18, size={34,48}, ilvl=4, rank="elite", actor="bandit" },
    bandit_champion = { hp=116, atk=13, speed=2.3, sight=8.0, range=1.25, cd=0.95, exp=46, gold=32, size={38,52}, ilvl=6, rank="champion", actor="bandit" },
    boss_blood_blade = { hp=320, atk=15, speed=2.0, sight=9.0, range=1.65, cd=1.2, exp=180, gold=140, size={44,62}, ilvl=9, rank="boss", actor="boss" },
}
D.spawns = {
    {kind="bandit", x=5.5, z=4.5},
    {kind="bandit", x=18.5, z=5.5},
    {kind="bandit_elite", x=6.5, z=12.5},
    {kind="bandit_elite", x=17.5, z=12.5},
    {kind="bandit_champion", x=12.5, z=8.5},
    {kind="boss_blood_blade", x=12.5, z=3.5},
}
D.prop_for = function(row,col)
    local ch = D.map[row] and D.map[row]:sub(col,col) or ""
    if ch=="T" then return "tree" end
    if ch=="H" then return "house" end
    if ch=="R" then return "rock" end
    if ch=="L" then return "lantern" end
    return nil
end
return D