-- 物品数据库 (模拟)
local ItemDB = {}

-- ID, Name, Icon, Vocation, Level, Coin, Gold, Type, Value, Descr
ItemDB.data = {
    [1001] = {id=1001, name="小型金疮药", icon=1001, vocation=0, level=1, coin=50, gold=0, type=1100, value=20, descr="恢复少量生命值"},
    [1002] = {id=1002, name="中型金疮药", icon=1002, vocation=0, level=10, coin=100, gold=0, type=1100, value=50, descr="恢复中量生命值"},
    [1003] = {id=1003, name="强效金疮药", icon=1003, vocation=0, level=20, coin=200, gold=0, type=1100, value=100, descr="恢复大量生命值"},
    [1101] = {id=1101, name="魔法药(小)", icon=1101, vocation=0, level=1, coin=50, gold=0, type=1101, value=20, descr="恢复少量魔法值"},
}

function ItemDB.GetItem(id)
    return ItemDB.data[id]
end

return ItemDB
