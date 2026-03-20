local EquipDB = require("script/mirror_game/data/equip_db")
local Enums = require("script/mirror_game/core/enums")

local Equip = {}
Equip.__index = Equip

function Equip.new(id)
    local self = setmetatable({}, Equip)
    self.id = id
    self.unique_id = tostring(os.time()) .. "_" .. tostring(math.random(1000, 9999)) -- 简单的唯一ID生成
    
    -- 动态属性
    self.lvUp = 0
    self.extraAmount = 0
    self.extra = {} -- 列表: {type=Enums.EquipExtraType, value=10}
    
    -- 加载静态数据
    local data = EquipDB.GetEquip(id)
    if data then
        self.static_data = data
        self.name = data.name
    else
        print("Error: Equip ID " .. tostring(id) .. " not found in DB")
        self.name = "Unknown Equip"
    end
    
    return self
end

-- 获取总攻击力 (基础 + 强化 + 附加)
function Equip:GetDC()
    if not self.static_data then return 0, 0 end
    local min_dc = self.static_data.dc1
    local max_dc = self.static_data.dc2
    
    -- TODO: 加上强化属性和附加属性
    return min_dc, max_dc
end

return Equip
