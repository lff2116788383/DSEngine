local Human = require("script/mirror_game/object/human")
local Item = require("script/mirror_game/object/item")
local Equip = require("script/mirror_game/object/equip")
local Enums = require("script/mirror_game/core/enums")

local Player = {}
Player.__index = Player

local _instance = nil

function Player.GetInstance()
    if not _instance then
        _instance = Player.new()
    end
    return _instance
end

function Player.new()
    local self = setmetatable({}, Player)
    
    self.coin = 0
    self.gold = 0
    self.reputation = 0
    
    -- 分身 (目前只实现主分身)
    self.humans = {
        [1] = Human.new(Enums.JobType.Warrior), -- 本尊
        [2] = Human.new(Enums.JobType.Warrior), -- 战士分身
        [3] = Human.new(Enums.JobType.Mage),    -- 法师分身
        [4] = Human.new(Enums.JobType.Taoist)   -- 道士分身
    }
    
    self.current_human_index = 1
    
    -- 背包
    self.bag_items = {}   -- 物品列表
    self.bag_equips = {}  -- 装备列表
    
    return self
end

function Player:GetCurrentHuman()
    return self.humans[self.current_human_index]
end

function Player:AddItem(id, count)
    count = count or 1
    -- 简单的堆叠逻辑
    for _, item in ipairs(self.bag_items) do
        if item.id == id then
            item.count = item.count + count
            print("Added item count: " .. item.name .. " x" .. count)
            return
        end
    end
    
    local new_item = Item.new(id, count)
    table.insert(self.bag_items, new_item)
    print("Added new item: " .. new_item.name .. " x" .. count)
end

function Player:AddEquip(id)
    local new_equip = Equip.new(id)
    table.insert(self.bag_equips, new_equip)
    print("Added new equip: " .. new_equip.name)
end

return Player
