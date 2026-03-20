local ItemDB = require("script/mirror_game/data/item_db")

local Item = {}
Item.__index = Item

function Item.new(id, count)
    local self = setmetatable({}, Item)
    self.id = id
    self.count = count or 1
    
    -- 加载静态数据
    local data = ItemDB.GetItem(id)
    if data then
        self.static_data = data
        self.name = data.name
        self.type = data.type
    else
        print("Error: Item ID " .. tostring(id) .. " not found in DB")
        self.name = "Unknown Item"
    end
    
    return self
end

function Item:Use(target)
    if not self.static_data then return false end
    
    -- TODO: 实现物品使用效果
    print("Using item: " .. self.name)
    return true
end

return Item
