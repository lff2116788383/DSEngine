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
D.enemy = { bandit = { hp=42, atk=8, speed=1.9, sight=7.0, range=1.15, cd=1.2, exp=14, gold=8, size={30,44}, ilvl=1 } }
D.spawns = {
    {kind="bandit", x=5.5, z=4.5},
    {kind="bandit", x=18.5, z=5.5},
    {kind="bandit", x=6.5, z=12.5},
    {kind="bandit", x=17.5, z=12.5},
    {kind="bandit", x=12.5, z=8.5},
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