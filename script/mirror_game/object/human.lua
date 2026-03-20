local Equip = require("script/mirror_game/object/equip")
local Enums = require("script/mirror_game/core/enums")

local Human = {}
Human.__index = Human

function Human.new(job)
    local self = setmetatable({}, Human)
    self.job = job or Enums.JobType.Warrior
    self.level = 1
    self.exp = 0
    self.hp = 100
    self.max_hp = 100
    self.mp = 50
    self.max_mp = 50
    self.rage = 0       -- 怒气
    self.max_rage = 100 -- 最大怒气
    
    -- 装备栏 (假设有 10 个部位)
    self.equipments = {} 
    
    -- 属性
    self.dc = {min=0, max=0} -- 物理攻击
    self.mc = {min=0, max=0} -- 魔法攻击
    self.sc = {min=0, max=0} -- 道术攻击
    self.ac = {min=0, max=0} -- 防御
    self.mac = {min=0, max=0} -- 魔防
    
    return self
end

function Human:WearEquip(slot, equip)
    if not equip then return end
    
    -- 卸下旧装备
    local old_equip = self.equipments[slot]
    self.equipments[slot] = equip
    
    self:UpdateStats()
    return old_equip
end

function Human:TakeOffEquip(slot)
    local old_equip = self.equipments[slot]
    self.equipments[slot] = nil
    self:UpdateStats()
    return old_equip
end

function Human:UpdateStats()
    -- 重置基础属性
    self.dc = {min=0, max=0}
    self.ac = {min=0, max=0}
    
    -- 遍历装备累加属性
    for slot, equip in pairs(self.equipments) do
        if equip and equip.static_data then
            local data = equip.static_data
            self.dc.min = self.dc.min + (data.dc1 or 0)
            self.dc.max = self.dc.max + (data.dc2 or 0)
            self.ac.min = self.ac.min + (data.ac or 0)
            self.ac.max = self.ac.max + (data.ac or 0)
            -- TODO: 其他属性
        end
    end
    
    print(string.format("Stats Updated: DC %d-%d, AC %d-%d", self.dc.min, self.dc.max, self.ac.min, self.ac.max))
end

return Human
