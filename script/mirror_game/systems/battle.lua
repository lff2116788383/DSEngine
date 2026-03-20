local BattleSystem = {}

-- 宝石类型对应的效果
BattleSystem.GemEffect = {
    [1] = "PHYSICAL_ATTACK", -- Red
    [2] = "HEAL",            -- Green
    [3] = "MAGIC_ATTACK",    -- Blue
    [4] = "RAGE",            -- Yellow
    [5] = "SPECIAL"          -- Purple
}

function BattleSystem.ProcessMatch(player, monster, gem_type, count)
    if not player or not monster then return end
    
    local effect = BattleSystem.GemEffect[gem_type]
    local human = player:GetCurrentHuman()
    
    if effect == "PHYSICAL_ATTACK" then
        local base_dmg = math.random(human.dc.min, human.dc.max)
        local total_dmg = base_dmg * count -- 简单的倍率
        local actual_dmg, is_dead = monster:TakeDamage(total_dmg, 1) -- 1 = Physical
        
        if is_dead then
            print("Battle: Monster Defeated!")
            BattleSystem.OnMonsterDead(player, monster)
        else
            -- 怪物反击
            BattleSystem.MonsterCounterAttack(monster, human)
        end
        
    elseif effect == "HEAL" then
        local heal_amount = 10 * count -- 基础治疗量
        -- TODO: 加上装备加成
        human.hp = math.min(human.hp + heal_amount, human.max_hp)
        print(string.format("Battle: Heal! Amount: %d, Current HP: %d/%d", heal_amount, human.hp, human.max_hp))
        
        -- 治疗回合怪物通常也会攻击
        BattleSystem.MonsterCounterAttack(monster, human)
        
    elseif effect == "MAGIC_ATTACK" then
        local base_dmg = math.random(human.mc.min, human.mc.max)
        local total_dmg = base_dmg * count
        local actual_dmg, is_dead = monster:TakeDamage(total_dmg, 2) -- 2 = Magic
        
        if is_dead then
            print("Battle: Monster Defeated!")
            BattleSystem.OnMonsterDead(player, monster)
        else
            BattleSystem.MonsterCounterAttack(monster, human)
        end
        
    elseif effect == "RAGE" then
        local rage_amount = 5 * count
        human.rage = math.min(human.rage + rage_amount, human.max_rage)
        print(string.format("Battle: Rage increased! +%d (%d/%d)", rage_amount, human.rage, human.max_rage))
        
        -- 怒气满时自动释放必杀技
        if human.rage >= human.max_rage then
            print("Battle: ULTIMATE SKILL ACTIVATED!")
            local ult_dmg = (human.dc.max + human.mc.max) * 3
            monster:TakeDamage(ult_dmg, 0) -- 0 = True Damage
            human.rage = 0
        end
        
        -- 怒气增加通常不算作回合结束，或者算作弱回合
        BattleSystem.MonsterCounterAttack(monster, human)
    else
        print("Battle: Unknown effect")
    end
end

function BattleSystem.OnMonsterDead(player, monster)
    print("--- Victory! ---")
    
    -- 经验值奖励
    local exp = monster.exp or 0
    -- TODO: 增加玩家经验
    print("Gained Exp: " .. exp)
    
    -- 金币奖励
    local coin = monster.coin or 0
    player.coin = player.coin + coin
    print("Gained Coin: " .. coin)
    
    -- 物品掉落 (模拟)
    if math.random() < 0.5 then
        player:AddItem(1001, 1) -- 掉落金疮药
        print("Loot: 小型金疮药 x1")
    end
    
    -- 装备掉落 (模拟)
    if math.random() < 0.2 then
        player:AddEquip(102) -- 掉落匕首
        print("Loot: 匕首")
    end
    
    print("----------------")
    
    -- 通知场景切换或生成新怪物
    -- 这里可以通过回调或事件系统通知 Match3Scene
end

function BattleSystem.MonsterCounterAttack(monster, human)
    if not monster or not human or monster.hp <= 0 then return end
    
    local dmg = monster:Attack(human)
    -- 简单的减伤计算: 伤害 - 防御
    local reduction = math.random(human.ac.min, human.ac.max)
    local final_dmg = math.max(1, dmg - reduction)
    
    human.hp = math.max(0, human.hp - final_dmg)
    print(string.format("Battle: Monster Attacks! Damage: %d (Raw: %d, Red: %d). Player HP: %d/%d", 
        final_dmg, dmg, reduction, human.hp, human.max_hp))
        
    if human.hp <= 0 then
        print("Battle: Player Defeated!")
    end
end

return BattleSystem
