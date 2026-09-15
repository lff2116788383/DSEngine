-- ============================================================================
-- data.lua  静态数据：技能 / 物品 / 对话 / 任务
-- ============================================================================
local D = {}

D.skills = {
    { id = "combo1", name = "青溪剑法起", dmg = 9, range = 1.25, arc = 120, kb = 3.0,
      mp = 0, stamina = 0, cd = 0, hitstop = 0.05, sfx = "slash" },
    { id = "combo2", name = "青溪剑法承", dmg = 11, range = 1.35, arc = 130, kb = 3.4,
      mp = 0, stamina = 0, cd = 0, hitstop = 0.06, sfx = "slash" },
    { id = "combo3", name = "青溪剑法转", dmg = 17, range = 1.55, arc = 170, kb = 5.5,
      mp = 0, stamina = 0, cd = 0, hitstop = 0.10, sfx = "crit" },
    { id = "fenhua", name = "分花拂柳", dmg = 30, range = 2.9, arc = 360, kb = 6.0,
      mp = 18, stamina = 0, cd = 4.0, hitstop = 0.12, sfx = "skill" },
    { id = "zixia", name = "紫霞真气", dmg = 0, range = 0, arc = 0, kb = 0,
      mp = 22, stamina = 0, cd = 10.0, heal = 0.35, buff = 1.35, buff_time = 8.0, sfx = "heal" },
}
D.skill_icon = { "ui/skill_icon_0.png", "ui/skill_icon_1.png", "ui/skill_icon_2.png", "ui/skill_icon_3.png" }
D.hotkey = { "J", "K", "U", "I" }
D.skill_bar = { { id = "combo1", key = "J", icon = 1 }, { id = "dodge", key = "K", icon = 2 },
                { id = "fenhua", key = "U", icon = 3 }, { id = "zixia", key = "I", icon = 4 } }

D.items = {
    hp_potion = { name = "金创药", heal = 45, icon = "pickup_hp", desc = "恢复 45 点生命" },
    mp_potion = { name = "回气散", mana = 35, icon = "pickup_mp", desc = "恢复 35 点内力" },
    coin = { name = "铜钱", gold = 1, icon = "pickup_coin", desc = "通用货币" },
    iron_sword = { name = "铁剑", atk = 6, icon = "ui/icon_sword.png", desc = "攻击 +6" },
    leather_armor = { name = "皮甲", def = 4, icon = "ui/icon_armor.png", desc = "防御 +4" },
    manual = { name = "剑谱残页", quest = true, icon = "ui/icon_manual.png", desc = "记载着失传剑招" },
    boss_token = { name = "寨主令", quest = true, icon = "ui/icon_coin.png", desc = "黑风寨主信物" },
}

D.enemy_cfg = {
    bandit = { name = "刀客", hp = 46, atk = 9, def = 2, exp = 16, gold = 8, speed = 2.0,
               sight = 6.5, atk_range = 1.5, atk_cd = 1.4, size = { 35, 46 },
               drops = { { "hp_potion", 0.28 }, { "coin", 0.75 } } },
    wolf = { name = "野狼", hp = 34, atk = 8, def = 1, exp = 13, gold = 5, speed = 3.1,
             sight = 7.5, atk_range = 1.35, atk_cd = 1.1, size = { 51, 34 },
             drops = { { "coin", 0.5 } } },
    ghost = { name = "怨魂", hp = 40, atk = 11, def = 0, exp = 20, gold = 12, speed = 1.5,
              sight = 8.0, atk_range = 5.2, atk_cd = 2.0, size = { 39, 46 }, ranged = true,
              drops = { { "mp_potion", 0.4 }, { "coin", 0.5 } } },
    boss = { name = "寨主血刀", hp = 420, atk = 18, def = 6, exp = 200, gold = 150, speed = 2.2,
             sight = 9.0, atk_range = 2.1, atk_cd = 1.8, size = { 65, 84 }, boss = true,
             drops = { { "boss_token", 1.0 }, { "mp_potion", 1.0 } } },
}

D.dialogues = {
    elder = {
        { speaker = "村中长者", text = "少侠留步。黑风寨的贼人近日又在山口出没，村中人心惶惶。" },
        { speaker = "村中长者", text = "若要上山，先在此处歇息。老夫这儿有几句要紧的话，你且听好。" },
        { speaker = "村中长者", text = "剑走轻灵，心要静。那伙贼人虽凶，却怕快剑。" },
        { quest = "q_boss" },
    },
    smith = {
        { speaker = "铁匠", text = "这把刀是我打的，便宜卖你哎，客官别走啊！" },
        { speaker = "铁匠", text = "铁料不够了，改日再来。要打剑，先得有剑谱残页。" },
    },
    villager = {
        { speaker = "村民", text = "多谢少侠出手相救！那伙贼人抢了我们的粮车，往寨子里去了。" },
        { speaker = "村民", text = "寨子就在北面山口，少侠千万小心。" },
        { quest = "q_rescue" },
    },
    boss_intro = {
        { speaker = "寨主血刀", text = "哼，区区一个毛头小子，也敢闯我黑风寨？" },
        { speaker = "寨主血刀", text = "既然来了，就把命留下！" },
    },
    boss_dead = {
        { speaker = "寨主血刀", text = "少侠好身手山寨的宝藏，都在后堂" },
    },
}

D.quests = {
    { id = "q_start", title = "寻访村中长者", desc = "在青溪村找到长者，问明山寨虚实", target = "talk_elder" },
    { id = "q_rescue", title = "救出村民", desc = "前往黑风寨，击溃贼人", target = "kill_stronghold", count = 5 },
    { id = "q_boss", title = "击败寨主", desc = "击败黑风寨寨主血刀", target = "kill_boss" },
}

D.tips = {
    "WASD 移动　J 连招　K 闪避(无敌帧)　U 分花拂柳　I 紫霞真气",
    "1 金创药　2 回气散　E 交互　Esc 暂停　M 地图　R 重开",
}

D.story = {
    map1_title = "第 1 章　暮色青溪",
    map2_title = "第 2 章　夜雨黑风",
}

return D