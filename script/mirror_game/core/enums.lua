-- 游戏枚举定义
local Enums = {}

-- 物品效果类型 (对应 ItemDefine.h)
Enums.EffectType = {
    NoEffect = 0,
    ImmediateCoin = 1000,
    ImmediateGold = 1001,
    ImmediateReputation = 1002,
    ImmediateLevel = 1003,
    ImmediateExp = 1004,
    ImmediateStrength = 1005,
    ImmediateWisdom = 1006,
    ImmediateSpirit = 1007,
    ImmediateLife = 1008,
    ImmediateAgility = 1009,
    ImmediatePotential = 1010,
    ImmediateHp = 1100,
    ImmediateMp = 1101,
    Skill = 5001,
}

-- 装备附加属性类型 (对应 def_item_equip.h)
Enums.EquipExtraType = {
    FixedHp = 0,
    FixedMp = 1,
    FixedHpr = 2,
    FixedMpr = 3,
    FixedDc = 4,
    FixedMc = 5,
    FixedSc = 6,
    FixedAc = 7,
    FixedMac = 8,
    FixedSpd = 9,
    FixedLuck = 10,
    FixedHit = 11,
    FixedDodge = 12,

    PercentDc = 15,
    PercentMc = 16,
    PercentSc = 17,
    PercentAc = 18,
    PercentMac = 19,
    Limit = 20
}

-- 职业类型
Enums.JobType = {
    Warrior = 1, -- 战士
    Mage = 2,    -- 法师
    Taoist = 3   -- 道士
}

return Enums
