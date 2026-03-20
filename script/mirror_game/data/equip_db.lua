-- 装备数据库 (模拟)
local EquipDB = {}

-- ID, Name, Icon, Lv, Luck, Spd, Hp, Ac, Mac, Dc1, Dc2, Mc1, Mc2, Sc1, Sc2, Need, NeedLvl, Price
EquipDB.data = {
    -- 武器
    [101] = {id=101, name="木剑", icon=101, lv=1, luck=0, spd=0, hp=0, ac=0, mac=0, dc1=2, dc2=5, mc1=0, mc2=0, sc1=0, sc2=0, need=0, needLvl=1, price=100},
    [102] = {id=102, name="匕首", icon=102, lv=1, luck=0, spd=1, hp=0, ac=0, mac=0, dc1=4, dc2=5, mc1=0, mc2=0, sc1=0, sc2=0, need=0, needLvl=1, price=150},
    [103] = {id=103, name="乌木剑", icon=103, lv=1, luck=0, spd=0, hp=0, ac=0, mac=0, dc1=4, dc2=8, mc1=0, mc2=1, sc1=0, sc2=0, need=0, needLvl=1, price=200},
    
    -- 衣服
    [201] = {id=201, name="布衣(男)", icon=201, lv=1, luck=0, spd=0, hp=0, ac=2, mac=1, dc1=0, dc2=0, mc1=0, mc2=0, sc1=0, sc2=0, need=0, needLvl=1, price=100},
    [202] = {id=202, name="轻型盔甲(男)", icon=202, lv=11, luck=0, spd=0, hp=0, ac=3, mac=2, dc1=0, dc2=0, mc1=0, mc2=0, sc1=0, sc2=0, need=0, needLvl=11, price=500},
}

function EquipDB.GetEquip(id)
    return EquipDB.data[id]
end

return EquipDB
