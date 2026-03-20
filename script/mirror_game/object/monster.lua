local MonsterDB = require("script/mirror_game/data/monster_db")

local Monster = {}
Monster.__index = Monster

function Monster.new(id)
    local self = setmetatable({}, Monster)
    self.id = id
    
    local data = MonsterDB.GetMonster(id)
    if data then
        self.static_data = data
        self.name = data.name
        self.level = data.level
        self.max_hp = data.hp
        self.hp = data.hp
        self.ac = data.ac
        self.mac = data.mac
        self.dc = data.dc
        self.spd = data.spd
        self.exp = data.exp
        self.coin = data.coin
    else
        print("Error: Monster ID " .. tostring(id) .. " not found in DB")
        self.name = "Unknown Monster"
        self.hp = 100
        self.max_hp = 100
        self.dc = 10
    end
    
    return self
end

function Monster:TakeDamage(damage, type)
    -- type: 1=Physical, 2=Magic
    local reduction = 0
    if type == 1 then
        reduction = self.ac
    elseif type == 2 then
        reduction = self.mac
    end
    
    local final_damage = math.max(1, damage - reduction)
    self.hp = math.max(0, self.hp - final_damage)
    
    print(string.format("Monster %s took %d damage (Raw: %d, Red: %d). HP: %d/%d", 
        self.name, final_damage, damage, reduction, self.hp, self.max_hp))
        
    return final_damage, self.hp <= 0
end

function Monster:Attack(target)
    if not target then return 0 end
    -- 简单的攻击逻辑，只算 DC
    return self.dc
end

return Monster
