-- RPG Stats Component
-- Handles Health, Mana, Attributes
local RPGStats = class("RPGStats", Component)

function RPGStats:ctor()
    RPGStats.super.ctor(self)
    
    self.hp = 100
    self.max_hp = 100
    self.mp = 50
    self.max_mp = 50
    
    self.attributes = {
        strength = 10,
        dexterity = 10,
        intelligence = 10,
        vitality = 10
    }
end

function RPGStats:LoadFromConfig(config)
    if not config then return end
    -- Parse Flare-like entity definition
    if config.stats then
        self.attributes.strength = tonumber(config.stats.strength) or self.attributes.strength
        -- ...
    end
end

function RPGStats:TakeDamage(amount)
    self.hp = self.hp - amount
    if self.hp < 0 then self.hp = 0 end
    print(self:game_object():name() .. " took " .. amount .. " damage. HP: " .. self.hp)
    if self.hp == 0 then
        self:OnDeath()
    end
end

function RPGStats:OnDeath()
    print(self:game_object():name() .. " died.")
    -- Trigger death animation or logic
end

return RPGStats
