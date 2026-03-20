-- 怪物数据库 (模拟)
local MonsterDB = {}

-- ID, Name, Level, HP, AC(Def), MAC(MDef), DC(Atk), Spd, Exp, Coin
MonsterDB.data = {
    [1001] = {id=1001, name="稻草人", level=1, hp=50, ac=0, mac=0, dc=5, spd=10, exp=10, coin=5},
    [1002] = {id=1002, name="多钩猫", level=3, hp=80, ac=1, mac=0, dc=8, spd=12, exp=25, coin=15},
    [1003] = {id=1003, name="钉耙猫", level=3, hp=85, ac=1, mac=0, dc=9, spd=12, exp=28, coin=18},
    [1004] = {id=1004, name="半兽人", level=10, hp=200, ac=3, mac=1, dc=20, spd=15, exp=100, coin=50},
}

function MonsterDB.GetMonster(id)
    return MonsterDB.data[id]
end

return MonsterDB
