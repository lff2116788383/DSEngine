-- ============================================================================
-- database.lua — 完整移植自 C# DB_Skill / DB_Monster / DB_Boss / DB_Stage
-- 100% 忠实还原 Unity 逆向源码数据
-- ============================================================================

local M = {}

-- ============================================================================
-- DB_Skill: 20 个技能集 × 5 级 = 100 条技能数据
-- _kind: 1=普通攻击技能 2=召唤技能 3=施法技能 4=终极技能
-- _txtkind: 0=玩家技能 1=宠物技能 2=武将技能
-- ============================================================================
M.DB_Skill = {}

-- 辅助函数：生成技能数据
local function sk(set, lv, reqLV, txtkind, atk, cool, price, pk, soul, name, info, kind)
  M.DB_Skill[set] = M.DB_Skill[set] or {}
  M.DB_Skill[set][lv] = {
    requireLV = reqLV, txtkind = txtkind, attackpoint = atk,
    cooltime = cool, price = price, pricekind = pk, soulprice = soul,
    name = name, info = info, kind = kind,
  }
end

-- 技能集 0: 剑舞 (blade dance) — kind=1, reqLV=1, soul=1
for lv = 0, 4 do
  local atk = {120,140,165,195,225}
  local cool = {15.1,14.1,12.1,10.1,7.1}
  local price = {0,300,600,900,1200}
  sk(0, lv, 1, 0, atk[lv+1], cool[lv+1], price[lv+1], 0, 1, 101, 121, 1)
end
-- 技能集 1: 旋风斩 (wheel wind) — kind=1, reqLV=3, soul=1
for lv = 0, 4 do
  local atk = {140,160,185,215,250}
  local cool = {15.1,14.1,12.1,10.1,7.1}
  local price = {0,300,600,900,1200}
  sk(1, lv, 3, 0, atk[lv+1], cool[lv+1], price[lv+1], 0, 1, 105, 125, 1)
end
-- 技能集 2: 火焰斩 (fire slash) — kind=1, reqLV=5, soul=2
for lv = 0, 4 do
  local atk = {65,75,90,110,135}
  local cool = {15.1,14.1,12.1,10.1,7.1}
  local price = {0,600,1200,1800,2400}
  sk(2, lv, 5, 0, atk[lv+1], cool[lv+1], price[lv+1], 0, 2, 103, 123, 1)
end
-- 技能集 3: 宠物鹰 (eagle summon) — kind=2, reqLV=1, soul=2
for lv = 0, 4 do
  local atk = {8,10,12,14,16}
  local cool = {15.1,14.1,12.1,10.1,7.1}
  local price = {2,4,6,8,10}
  sk(3, lv, 1, 1, atk[lv+1], cool[lv+1], price[lv+1], 1, 2, 110, 130, 2)
end
-- 技能集 4: 冰冻斩 (ice slash) — kind=1, reqLV=7, soul=2
for lv = 0, 4 do
  local atk = {70,80,95,115,140}
  local cool = {15.1,14.1,12.1,10.1,7.1}
  local price = {0,600,1200,1800,2400}
  sk(4, lv, 7, 0, atk[lv+1], cool[lv+1], price[lv+1], 0, 2, 102, 122, 1)
end
-- 技能集 5: 雷电斩 (lightning slash) — kind=1, reqLV=9, soul=2
for lv = 0, 4 do
  local atk = {160,180,205,235,265}
  local cool = {15.1,14.1,12.1,10.1,7.1}
  local price = {0,600,1200,1800,2400}
  sk(5, lv, 9, 0, atk[lv+1], cool[lv+1], price[lv+1], 0, 2, 108, 128, 1)
end
-- 技能集 6: 毒击 (poison strike) — kind=2, reqLV=11, soul=2
for lv = 0, 4 do
  local atk = {110,120,135,155,180}
  local cool = {15.1,14.1,12.1,10.1,7.1}
  local price = {0,600,1200,1800,2400}
  sk(6, lv, 11, 0, atk[lv+1], cool[lv+1], price[lv+1], 0, 2, 104, 124, 2)
end
-- 技能集 7: 宠物马 (horse summon) — kind=2, reqLV=1, soul=2
for lv = 0, 4 do
  local atk = {6,8,10,12,14}
  local cool = {15.1,14.1,12.1,10.1,7.1}
  local price = {2,4,6,8,10}
  sk(7, lv, 1, 1, atk[lv+1], cool[lv+1], price[lv+1], 1, 2, 109, 129, 2)
end
-- 技能集 8: 蓄力斩 (charge smash) — kind=3, reqLV=13, soul=3
for lv = 0, 4 do
  local atk = {5,6,7,8,9}
  local cool = {20.1,18.1,16.1,14.1,12.1}
  local price = {0,900,1800,2700,3600}
  sk(8, lv, 13, 0, atk[lv+1], cool[lv+1], price[lv+1], 0, 3, 112, 132, 3)
end
-- 技能集 9: 武将召唤 (general summon) — kind=3, reqLV=15, soul=3
for lv = 0, 4 do
  local atk = {50,60,70,80,90}
  local cool = {20.1,18.1,16.1,14.1,12.1}
  local price = {0,900,1800,2700,3600}
  sk(9, lv, 15, 2, atk[lv+1], cool[lv+1], price[lv+1], 0, 3, 113, 133, 3)
end
-- 技能集 10: 极限斩 (extreme slash) — kind=1, reqLV=17, soul=3
for lv = 0, 4 do
  local atk = {230,240,260,290,330}
  local cool = {20.1,18.1,16.1,14.1,12.1}
  local price = {0,900,1800,2700,3600}
  sk(10, lv, 17, 0, atk[lv+1], cool[lv+1], price[lv+1], 0, 3, 106, 126, 1)
end
-- 技能集 11: 时间减速 (time slow) — kind=4, reqLV=19, soul=3
for lv = 0, 4 do
  local atk = {190,200,220,250,290}
  local cool = {20.1,18.1,16.1,14.1,12.1}
  local price = {0,900,1800,2700,3600}
  sk(11, lv, 19, 0, atk[lv+1], cool[lv+1], price[lv+1], 0, 3, 107, 127, 4)
end
-- 技能集 12-19: 武将技能 (general skills) — soul=4-5
-- 12: 关羽 — kind=1, soul=4
for lv = 0, 4 do
  local atk = {110,120,140,170,210}
  local cool = {30.1,26.1,22.1,18.1,14.1}
  local price = {7,9,11,13,15}
  sk(12, lv, 1, 0, atk[lv+1], cool[lv+1], price[lv+1], 1, 4, 111, 131, 1)
end
-- 13: 张飞 — kind=2, soul=4
for lv = 0, 4 do
  local atk = {110,120,140,170,210}
  local cool = {30.1,26.1,22.1,18.1,14.1}
  local price = {7,9,11,13,15}
  sk(13, lv, 1, 0, atk[lv+1], cool[lv+1], price[lv+1], 1, 4, 115, 135, 2)
end
-- 14: 赵云 — kind=3, soul=4
for lv = 0, 4 do
  local atk = {140,180,220,260,300}
  local cool = {30.1,26.1,22.1,18.1,14.1}
  local price = {7,9,11,13,15}
  sk(14, lv, 1, 0, atk[lv+1], cool[lv+1], price[lv+1], 1, 4, 116, 136, 3)
end
-- 15: 马超 — kind=2, soul=4
for lv = 0, 4 do
  local atk = {120,150,180,210,240}
  local cool = {30.1,26.1,22.1,18.1,14.1}
  local price = {7,9,11,13,15}
  sk(15, lv, 1, 0, atk[lv+1], cool[lv+1], price[lv+1], 1, 4, 114, 134, 2)
end
-- 16: 黄忠 — kind=4, soul=5
for lv = 0, 4 do
  local atk = {80,90,100,110,120}
  local cool = {30.1,26.1,22.1,18.1,14.1}
  local price = {7,9,11,13,15}
  sk(16, lv, 1, 0, atk[lv+1], cool[lv+1], price[lv+1], 1, 5, 117, 137, 4)
end
-- 17: 吕布 — kind=4, soul=5
for lv = 0, 4 do
  local atk = {400,420,450,490,540}
  local cool = {30.1,26.1,22.1,18.1,14.1}
  local price = {7,9,11,13,15}
  sk(17, lv, 1, 0, atk[lv+1], cool[lv+1], price[lv+1], 1, 5, 118, 138, 4)
end
-- 18: 孙策 — kind=4, soul=5
for lv = 0, 4 do
  local atk = {110,120,140,170,210}
  local cool = {30.1,26.1,22.1,18.1,14.1}
  local price = {7,9,11,13,15}
  sk(18, lv, 1, 0, atk[lv+1], cool[lv+1], price[lv+1], 1, 5, 119, 139, 4)
end
-- 19: 周瑜 — kind=2, soul=5
for lv = 0, 4 do
  local atk = {420,440,470,510,560}
  local cool = {30.1,26.1,22.1,18.1,14.1}
  local price = {7,9,11,13,15}
  sk(19, lv, 1, 0, atk[lv+1], cool[lv+1], price[lv+1], 1, 5, 120, 140, 2)
end

-- 技能名称映射 (name ID -> 名称)
M.SkillNames = {
  [101] = "剑舞",    [105] = "旋风斩",  [103] = "火焰斩",  [110] = "召唤猎鹰",
  [102] = "冰冻斩",  [108] = "雷电斩",  [104] = "毒击",    [109] = "召唤战马",
  [112] = "蓄力斩",  [113] = "武将召唤", [106] = "极限斩",  [107] = "时间减速",
  [111] = "青龙斩",  [115] = "蛇矛突",   [116] = "龙胆枪",  [114] = "铁骑冲",
  [117] = "百步穿杨", [118] = "无双乱舞", [119] = "霸王斩",  [120] = "火攻",
}

-- ============================================================================
-- DB_Monster: 16 种怪物完整数据
-- _kind: 1=野兽型 2=人型
-- _sizekind: 10=小型 20=中型 24=大型
-- ============================================================================
M.DB_Monster = {
  [0]  = {maxhp=20,  power=15, haveExp=1, block=0,  firerange=0.24, runspeed=0.3,  backspeed=-0.05, dash=0,   moving_atk=0.3, attach_ef=false, speed_move=0.34, speed_m_attack1=0.4,  speed_m_attack1_i=0.4,  speed_idle=0.3,  sizekind=10, kind=2},
  [1]  = {maxhp=20,  power=15, haveExp=1, block=4,  firerange=0.24, runspeed=0.3,  backspeed=-0.05, dash=0,   moving_atk=0.2, attach_ef=false, speed_move=0.34, speed_m_attack1=0.4,  speed_m_attack1_i=0.4,  speed_idle=0.3,  sizekind=10, kind=2},
  [2]  = {maxhp=20,  power=10, haveExp=1, block=1,  firerange=0.5,  runspeed=0.2,  backspeed=-0.05, dash=0,   moving_atk=0,   attach_ef=false, speed_move=0.3,  speed_m_attack1=0.3,  speed_m_attack1_i=0.4,  speed_idle=0.3,  sizekind=10, kind=2},
  [3]  = {maxhp=25,  power=15, haveExp=2, block=2,  firerange=0.3,  runspeed=0.3,  backspeed=-0.06, dash=0,   moving_atk=0,   attach_ef=false, speed_move=0.3,  speed_m_attack1=0.3,  speed_m_attack1_i=0.4,  speed_idle=0.3,  sizekind=20, kind=2},
  [4]  = {maxhp=20,  power=15, haveExp=2, block=3,  firerange=0.3,  runspeed=0.4,  backspeed=-0.05, dash=0,   moving_atk=0.5, attach_ef=false, speed_move=0.42, speed_m_attack1=0.25, speed_m_attack1_i=0.4,  speed_idle=0.3,  sizekind=10, kind=2},
  [5]  = {maxhp=40,  power=15, haveExp=2, block=1,  firerange=0.5,  runspeed=0.2,  backspeed=-0.05, dash=0,   moving_atk=0,   attach_ef=false, speed_move=0.3,  speed_m_attack1=0.3,  speed_m_attack1_i=0.1,  speed_idle=0.3,  sizekind=10, kind=2},
  [6]  = {maxhp=45,  power=15, haveExp=2, block=2,  firerange=0.5,  runspeed=0.3,  backspeed=-0.06, dash=0,   moving_atk=0.6, attach_ef=true,  speed_move=0.3,  speed_m_attack1=0.2,  speed_m_attack1_i=0.25, speed_idle=0.3,  sizekind=20, kind=2},
  [7]  = {maxhp=60,  power=15, haveExp=2, block=10, firerange=0.5,  runspeed=0.3,  backspeed=-0.06, dash=0,   moving_atk=0.6, attach_ef=false, speed_move=0.3,  speed_m_attack1=0.3,  speed_m_attack1_i=0.2,  speed_idle=0.3,  sizekind=20, kind=2},
  [8]  = {maxhp=15,  power=15, haveExp=2, block=2,  firerange=0.4,  runspeed=0.5,  backspeed=-0.05, dash=80,  moving_atk=0.1, attach_ef=true,  speed_move=0.6,  speed_m_attack1=0.3,  speed_m_attack1_i=0.34, speed_idle=0.4,  sizekind=10, kind=1},
  [9]  = {maxhp=60,  power=30, haveExp=3, block=4,  firerange=0.4,  runspeed=0.5,  backspeed=-0.05, dash=100, moving_atk=0,   attach_ef=true,  speed_move=0.44, speed_m_attack1=0.3,  speed_m_attack1_i=0.3,  speed_idle=0.3,  sizekind=24, kind=1},
  [10] = {maxhp=60,  power=30, haveExp=3, block=4,  firerange=0.24, runspeed=0.3,  backspeed=-0.05, dash=0,   moving_atk=0,   attach_ef=false, speed_move=0.5,  speed_m_attack1=0.4,  speed_m_attack1_i=0.4,  speed_idle=0.3,  sizekind=10, kind=1},
  [11] = {maxhp=50,  power=25, haveExp=2, block=5,  firerange=0.6,  runspeed=0.3,  backspeed=-0.06, dash=0,   moving_atk=0,   attach_ef=true,  speed_move=0.3,  speed_m_attack1=0.25, speed_m_attack1_i=0.25, speed_idle=0.3,  sizekind=20, kind=2},
  [12] = {maxhp=50,  power=20, haveExp=3, block=5,  firerange=0.7,  runspeed=0.3,  backspeed=-0.06, dash=0,   moving_atk=0,   attach_ef=false, speed_move=0.3,  speed_m_attack1=0.3,  speed_m_attack1_i=0.4,  speed_idle=0.3,  sizekind=20, kind=2},
  [13] = {maxhp=50,  power=25, haveExp=2, block=4,  firerange=0.5,  runspeed=0.2,  backspeed=-0.05, dash=0,   moving_atk=0,   attach_ef=false, speed_move=0.3,  speed_m_attack1=0.3,  speed_m_attack1_i=0.4,  speed_idle=0.3,  sizekind=10, kind=2},
  [14] = {maxhp=30,  power=20, haveExp=3, block=4,  firerange=0.5,  runspeed=0.2,  backspeed=-0.05, dash=0,   moving_atk=0,   attach_ef=false, speed_move=0.3,  speed_m_attack1=0.3,  speed_m_attack1_i=0.4,  speed_idle=0.3,  sizekind=10, kind=2},
  [15] = {maxhp=30,  power=20, haveExp=2, block=4,  firerange=0.5,  runspeed=0.3,  backspeed=-0.05, dash=0,   moving_atk=0,   attach_ef=false, speed_move=0.3,  speed_m_attack1=0.3,  speed_m_attack1_i=0.4,  speed_idle=0.3,  sizekind=10, kind=2},
}

-- ============================================================================
-- DB_Boss: 12 种 Boss 完整数据 (3 种攻击模式)
-- ============================================================================
M.DB_Boss = {
  -- matk = {delay, speed}  aef = attach_ef(0/1/2)  coff = collideroff(bool)
  [0]  = {maxhp=500, power1=60, power2=60, power3=60,  haveExp=160, block=10, firerange1=0.5,  firerange2=0.6,  firerange3=0.7,  turnspeed=2,  runspeed=0.24, dash1=0,   dash2=0,    dash3=0,    matk1={0.2,0.3}, matk2={0.2,0.5}, matk3={0.2,0.3}, aef1=0, aef2=0, aef3=0, coff1=true,  coff2=true,  coff3=true,  speed_move=0.3,  speed_b_attack1=0.2,  speed_b_attack2=0.2,  speed_b_attack3=0.2,  speed_b_attack1_i=0.2,  speed_b_attack2_i=0.15, speed_b_attack3_i=0.15, speed_idle=0.1,  speed_down=0.22, sizekind=20},
  [1]  = {maxhp=300, power1=30, power2=30, power3=30,  haveExp=110, block=5,  firerange1=0.3,  firerange2=0.5,  firerange3=0.6,  turnspeed=1,  runspeed=0.2,  dash1=0,   dash2=0,    dash3=500,  matk1={0,0},     matk2={0,0},     matk3={0.5,0.15},aef1=0, aef2=0, aef3=2, coff1=false, coff2=false, coff3=false, speed_move=0.3,  speed_b_attack1=0.15, speed_b_attack2=0.25, speed_b_attack3=0.15, speed_b_attack1_i=0.2,  speed_b_attack2_i=0.25,  speed_b_attack3_i=0.2,  speed_idle=0.25, speed_down=0.22, sizekind=20},
  [2]  = {maxhp=410, power1=35, power2=35, power3=35,  haveExp=115, block=5,  firerange1=0.4,  firerange2=0.6,  firerange3=0.8,  turnspeed=1.5,runspeed=0.4,  dash1=500, dash2=0,    dash3=0,    matk1={0.2,0.3}, matk2={0,0},     matk3={0,0},     aef1=0, aef2=0, aef3=0, coff1=true,  coff2=false, coff3=false, speed_move=0.3,  speed_b_attack1=0.25, speed_b_attack2=0.2,  speed_b_attack3=0.2,  speed_b_attack1_i=0.35, speed_b_attack2_i=0.2,  speed_b_attack3_i=0.2,  speed_idle=0.25, speed_down=0.22, sizekind=20},
  [3]  = {maxhp=420, power1=40, power2=40, power3=40,  haveExp=120, block=7,  firerange1=0.4,  firerange2=0.5,  firerange3=0.7,  turnspeed=1,  runspeed=0.2,  dash1=0,   dash2=0,    dash3=1500, matk1={0,0},     matk2={0,0},     matk3={0,0},     aef1=0, aef2=0, aef3=1, coff1=false, coff2=false, coff3=false, speed_move=0.3,  speed_b_attack1=0.15, speed_b_attack2=0.15, speed_b_attack3=0.15, speed_b_attack1_i=0.2,  speed_b_attack2_i=0.2,  speed_b_attack3_i=0.2,  speed_idle=0.25, speed_down=0.22, sizekind=20},
  [4]  = {maxhp=430, power1=40, power2=40, power3=40,  haveExp=125, block=5,  firerange1=0.4,  firerange2=0.5,  firerange3=0.7,  turnspeed=1,  runspeed=0.2,  dash1=0,   dash2=0,    dash3=0,    matk1={0,0},     matk2={0,0},     matk3={0,0},     aef1=0, aef2=0, aef3=0, coff1=false, coff2=false, coff3=false, speed_move=0.5,  speed_b_attack1=0.15, speed_b_attack2=0.15, speed_b_attack3=0.15, speed_b_attack1_i=0.2,  speed_b_attack2_i=0.2,  speed_b_attack3_i=0.2,  speed_idle=0.25, speed_down=0.22, sizekind=20},
  [5]  = {maxhp=440, power1=45, power2=45, power3=60,  haveExp=130, block=7,  firerange1=0.4,  firerange2=0.5,  firerange3=0.7,  turnspeed=3,  runspeed=0.5,  dash1=600, dash2=1200, dash3=0,    matk1={0,0},     matk2={0,0.4},   matk3={1,0.5},   aef1=2, aef2=0, aef3=0, coff1=false, coff2=false, coff3=true,  speed_move=0.3,  speed_b_attack1=0.2,  speed_b_attack2=0.25, speed_b_attack3=0.2,  speed_b_attack1_i=0.2,  speed_b_attack2_i=0.12, speed_b_attack3_i=0.2,  speed_idle=0.25, speed_down=0.22, sizekind=20},
  [6]  = {maxhp=450, power1=50, power2=60, power3=60,  haveExp=135, block=9,  firerange1=0.4,  firerange2=0.3,  firerange3=0.6,  turnspeed=2,  runspeed=0.2,  dash1=0,   dash2=0,    dash3=0,    matk1={0,0},     matk2={0,0},     matk3={0.1,0.4}, aef1=0, aef2=0, aef3=1, coff1=false, coff2=false, coff3=false, speed_move=0.3,  speed_b_attack1=0.2,  speed_b_attack2=0.3,  speed_b_attack3=0.25, speed_b_attack1_i=0.2,  speed_b_attack2_i=0.3,  speed_b_attack3_i=0.25, speed_idle=0.25, speed_down=0.22, sizekind=20},
  [7]  = {maxhp=460, power1=50, power2=50, power3=70,  haveExp=140, block=9,  firerange1=0.4,  firerange2=0.5,  firerange3=0.7,  turnspeed=5,  runspeed=0.25, dash1=500, dash2=2000, dash3=0,    matk1={0,0},     matk2={0,0},     matk3={0,0},     aef1=0, aef2=2, aef3=0, coff1=false, coff2=false, coff3=true,  speed_move=0.3,  speed_b_attack1=0.12, speed_b_attack2=0.2,  speed_b_attack3=0.28, speed_b_attack1_i=0.2,  speed_b_attack2_i=0.2,  speed_b_attack3_i=0.28, speed_idle=0.25, speed_down=0.22, sizekind=20},
  [8]  = {maxhp=470, power1=65, power2=65, power3=65,  haveExp=145, block=5,  firerange1=0.4,  firerange2=0.6,  firerange3=0.7,  turnspeed=1,  runspeed=0.2,  dash1=0,   dash2=0,    dash3=0,    matk1={0,0},     matk2={0,0},     matk3={0,0},     aef1=0, aef2=0, aef3=0, coff1=false, coff2=true,  coff3=false, speed_move=0.3,  speed_b_attack1=0.2,  speed_b_attack2=0.2,  speed_b_attack3=0.2,  speed_b_attack1_i=0.2,  speed_b_attack2_i=0.2,  speed_b_attack3_i=0.2,  speed_idle=0.25, speed_down=0.22, sizekind=20},
  [9]  = {maxhp=480, power1=50, power2=50, power3=50,  haveExp=0,   block=5,  firerange1=0.4,  firerange2=0.45, firerange3=0.5,  turnspeed=4,  runspeed=0.3,  dash1=0,   dash2=0,    dash3=0,    matk1={0,0},     matk2={0,0},     matk3={0,0},     aef1=0, aef2=0, aef3=0, coff1=false, coff2=false, coff3=false, speed_move=0.5,  speed_b_attack1=0.2,  speed_b_attack2=0.2,  speed_b_attack3=0.2,  speed_b_attack1_i=0.2,  speed_b_attack2_i=0.15, speed_b_attack3_i=0.2,  speed_idle=0.25, speed_down=0.22, sizekind=20},
  [10] = {maxhp=490, power1=60, power2=60, power3=60,  haveExp=150, block=5,  firerange1=0.4,  firerange2=0.5,  firerange3=0.7,  turnspeed=3,  runspeed=0.4,  dash1=500, dash2=0,    dash3=0,    matk1={0,0.2},   matk2={0.2,0.5}, matk3={0.2,0.5}, aef1=1, aef2=0, aef3=0, coff1=false, coff2=true,  coff3=true,  speed_move=0.3,  speed_b_attack1=0.25, speed_b_attack2=0.25, speed_b_attack3=0.25, speed_b_attack1_i=0.25, speed_b_attack2_i=0.25, speed_b_attack3_i=0.25, speed_idle=0.25, speed_down=0.22, sizekind=20},
  [11] = {maxhp=800, power1=100,power2=100,power3=100, haveExp=180, block=15, firerange1=0.3,  firerange2=0.5,  firerange3=0.6,  turnspeed=6,  runspeed=0.5,  dash1=0,   dash2=0,    dash3=0,    matk1={0,0},     matk2={0,0},     matk3={0,0},     aef1=0, aef2=1, aef3=0, coff1=false, coff2=false, coff3=false, speed_move=0.3,  speed_b_attack1=0.25, speed_b_attack2=0.25, speed_b_attack3=0.25, speed_b_attack1_i=0.25, speed_b_attack2_i=0.08, speed_b_attack3_i=0.25, speed_idle=0.25, speed_down=0.22, sizekind=20},
}

-- Boss 名称
M.BossNames = {
  [0]="华雄", [1]="李傕", [2]="郭汜", [3]="樊稠",
  [4]="张济", [5]="董卓", [6]="李儒", [7]="牛辅",
  [8]="胡轸", [9]="徐荣", [10]="吕布", [11]="董卓(终极)",
}

-- ============================================================================
-- DB_Stage: 90 个关卡完整数据
-- _bosscount: Boss 数量 (0-3)
-- _basemon1/_basemon2: 基础怪物类型范围
-- _mainmon: 主力怪物类型
-- _stagenum: 关卡内波数
-- _boss1/_boss2/_boss3: Boss 索引 (-1=无)
-- _extramon: 额外怪物 (-1=无)
-- _reward: 奖励类型
-- ============================================================================
M.DB_Stage = {}

-- 辅助函数
local function st(idx, bc, bm1, bm2, mm, sn, b1, b2, b3, em, rw)
  M.DB_Stage[idx] = {
    bosscount = bc, basemon1 = bm1, basemon2 = bm2,
    mainmon = mm, stagenum = sn,
    boss1 = b1, boss2 = b2, boss3 = b3,
    extramon = em, reward = rw,
  }
end

-- Stage 0-2: 村庄前哨 (Stage 1)
st(0, 0, 0, 1, 1, 3, -1,-1,-1,-1, 1)
st(1, 0, 0, 1, 3, 1, -1,-1,-1,-1, 1)
st(2, 1, 0, 1, 4, 1, 1,-1,-1,-1, 201)

-- Stage 3-5: 森林伏击 (Stage 2)
st(3, 0, 0, 1, 2, 1, -1,-1,-1,-1, 1)
st(4, 0, 1, 3, 4, 1, -1,-1,-1,-1, 1)
st(5, 1, 1, 3, 4, 1, 2,-1,-1,-1, 101)

-- Stage 6-8: 山寨攻防 (Stage 3)
st(6, 0, 1, 3, 2, 1, -1,-1,-1,-1, 1)
st(7, 0, 1, 3, 2, 1, -1,-1,-1,-1, 1)
st(8, 1, 1, 3, 4, 1, 3,-1,-1,-1, 1)

-- Stage 9-11: 城门战斗 (Stage 4)
st(9, 0, 1, 3, 2, 1, -1,-1,-1,-1, 1)
st(10, 0, 1, 3, 2, 1, -1,-1,-1,-1, 1)
st(11, 1, 3, 4, 2, 1, 4,-1,-1,-1, 102)

-- Stage 12-14: 虎牢关 (Stage 5)
st(12, 0, 1, 2, 6, 1, -1,-1,-1,-1, 1)
st(13, 0, 1, 2, 6, 1, -1,-1,-1,-1, 1)
st(14, 1, 1, 2, 6, 1, 5,-1,-1,-1, 202)

-- Stage 15-17: 洛阳城 (Stage 6)
st(15, 0, 1, 3, 8, 1, -1,-1,-1,-1, 1)
st(16, 0, 1, 3, 8, 1, -1,-1,-1,-1, 1)

-- Stage 17 修正: C# st[17]._reward = 103 (原为 1)
st(17, 1, 1, 3, 8, 1, 6,-1,-1,-1, 103)

-- Stage 18-20: C# st[18-20] 真实数据
st(18, 0, 4, 2, 7, 1, -1,-1,-1,-1, 1)
st(19, 0, 4, 2, 7, 1, -1,-1,-1,-1, 1)
st(20, 1, 4, 2, 7, 1, 7,-1,-1,-1, 1)

-- Stage 21-23: C# st[21-23]
st(21, 0, 3, 2, 5, 1, -1,-1,-1,-1, 1)
st(22, 0, 3, 2, 5, 1, -1,-1,-1,-1, 1)
st(23, 1, 3, 2, 5, 1, 8,-1,-1,-1, 104)

-- Stage 24-26: C# st[24-26]
st(24, 0, 1, 4, 11, 1, -1,-1,-1,-1, 1)
st(25, 0, 1, 4, 11, 1, -1,-1,-1,-1, 1)
st(26, 1, 4, 4, 11, 1, 9,-1,-1,-1, 203)

-- Stage 27-29: C# st[27-29]
st(27, 0, 1, 8, 12, 1, -1,-1,-1,-1, 1)
st(28, 0, 1, 8, 12, 1, -1,-1,-1,-1, 1)
st(29, 1, 1, 8, 12, 1, 10,-1,-1,-1, 105)

-- Stage 30-32
st(30, 0, 3, 5, 13, 1, -1,-1,-1,-1, 1)
st(31, 0, 3, 5, 13, 1, -1,-1,-1,-1, 1)
st(32, 2, 3, 5, 13, 1, 1, 4,-1,-1, 1)

-- Stage 33-35
st(33, 0, 2, 6, 14, 1, -1,-1,-1,-1, 1)
st(34, 0, 2, 6, 14, 1, -1,-1,-1,-1, 1)
st(35, 2, 2, 6, 14, 1, 2, 1,-1,-1, 106)

-- Stage 36-38
st(36, 0, 1, 3, 9, 1, -1,-1,-1,-1, 1)
st(37, 0, 1, 3, 9, 1, -1,-1,-1,-1, 1)
st(38, 2, 1, 3, 9, 1, 3, 5,-1,-1, 204)

-- Stage 39-41
st(39, 0, 0, 2, 6, 1, -1,-1,-1,-1, 1)
st(40, 0, 0, 2, 6, 1, -1,-1,-1,-1, 1)
st(41, 2, 0, 2, 6, 1, 4, 1,-1,-1, 107)

-- Stage 42-44
st(42, 0, 1, 5, 8, 1, -1,-1,-1,-1, 1)
st(43, 0, 1, 5, 8, 1, -1,-1,-1,-1, 1)
st(44, 2, 1, 5, 8, 1, 5, 2,-1,-1, 1)

-- Stage 45-47
st(45, 0, 1, 3, 7, 1, -1,-1,-1,-1, 1)
st(46, 0, 1, 3, 7, 1, -1,-1,-1,-1, 1)
st(47, 2, 1, 3, 7, 1, 6, 3,-1,-1, 108)

-- Stage 48-50
st(48, 0, 1, 2, 5, 1, -1,-1,-1,-1, 1)
st(49, 0, 1, 2, 5, 1, -1,-1,-1,-1, 1)
st(50, 2, 1, 2, 5, 1, 9, 4,-1,-1, 205)

-- Stage 51-53
st(51, 0, 1, 3, 11, 1, -1,-1,-1,-1, 1)
st(52, 0, 1, 3, 11, 1, -1,-1,-1,-1, 1)
st(53, 2, 1, 3, 11, 1, 8, 5,-1,-1, 109)

-- Stage 54-56
st(54, 0, 2, 4, 12, 1, -1,-1,-1,-1, 1)
st(55, 0, 2, 4, 12, 1, -1,-1,-1,-1, 1)
st(56, 2, 2, 4, 12, 1, 7, 6,-1,-1, 1)

-- Stage 57-59
st(57, 0, 3, 2, 13, 1, -1,-1,-1,-1, 1)
st(58, 0, 3, 2, 13, 1, -1,-1,-1,-1, 1)
st(59, 2, 3, 2, 13, 1, 10, 7,-1,-1, 110)

-- Stage 60-62
st(60, 0, 1, 4, 14, 1, -1,-1,-1,-1, 1)
st(61, 0, 1, 4, 14, 1, -1,-1,-1,-1, 1)
st(62, 2, 1, 4, 14, 1, 1, 8,-1,-1, 206)

-- Stage 63-65
st(63, 0, 1, 8, 9, 1, -1,-1,-1,-1, 1)
st(64, 0, 1, 8, 9, 1, -1,-1,-1,-1, 1)
st(65, 2, 1, 8, 9, 1, 2, 9,-1,-1, 111)

-- Stage 66-68
st(66, 0, 3, 5, 6, 1, -1,-1,-1,-1, 1)
st(67, 0, 3, 5, 6, 1, -1,-1,-1,-1, 1)
st(68, 3, 3, 5, 6, 1, 3, 1, 5,-1, 1)

-- Stage 69-71
st(69, 0, 2, 6, 7, 1, -1,-1,-1,-1, 1)
st(70, 0, 2, 6, 7, 1, -1,-1,-1,-1, 1)
st(71, 3, 2, 6, 7, 1, 4, 2, 8,-1, 112)

-- Stage 72-74
st(72, 0, 4, 6, 5, 1, -1,-1,-1,-1, 1)
st(73, 0, 4, 6, 5, 1, -1,-1,-1,-1, 1)
st(74, 3, 4, 6, 5, 1, 5, 3, 2,-1, 207)

-- Stage 75-77
st(75, 0, 5, 7, 11, 1, -1,-1,-1,-1, 1)
st(76, 0, 5, 7, 11, 1, -1,-1,-1,-1, 1)
st(77, 3, 5, 7, 11, 1, 6, 1, 2,-1, 113)

-- Stage 78-80
st(78, 0, 7, 11, 12, 1, -1,-1,-1,-1, 1)
st(79, 0, 7, 11, 12, 1, -1,-1,-1,-1, 1)
st(80, 3, 7, 11, 12, 1, 7, 3, 5,-1, 1)

-- Stage 81-83
st(81, 0, 6, 12, 13, 1, -1,-1,-1,-1, 1)
st(82, 0, 6, 12, 13, 1, -1,-1,-1,-1, 1)
st(83, 3, 6, 12, 13, 1, 8, 4, 2,-1, 114)

-- Stage 84-86
st(84, 0, 2, 11, 14, 1, -1,-1,-1,-1, 1)
st(85, 0, 2, 11, 14, 1, -1,-1,-1,-1, 1)
st(86, 3, 2, 11, 14, 1, 9, 6, 7,-1, 208)

-- Stage 87-89
st(87, 0, 5, 9, 14, 1, -1,-1,-1,-1, 1)
st(88, 0, 5, 9, 14, 1, -1,-1,-1,-1, 1)
st(89, 1, 5, 9, 14, 1, 11,-1,-1,-1, 115)

-- ============================================================================
-- DB_Weapon: 6 种武器类型
-- 0=单刀 1=双刀 2=长枪 3=弓 4=法杖 5=法器
-- ============================================================================
M.DB_Weapon = {
  [0] = {name="单刀", collider_center={0,0.05,0.17}, collider_radius=0.13, swing_scale={1.6,2,2}, attackkind_factor=0,  speed_mod=0,    run_speed=0.6},
  [1] = {name="双刀", collider_center={0,0.05,0.16}, collider_radius=0.13, swing_scale={1.3,1.7,1.7}, attackkind_factor=10, speed_mod=-0.02, run_speed=0.6},
  [2] = {name="长枪", collider_center={0,0.05,0.2},  collider_radius=0.15, swing_scale={2,2.6,2.6},   attackkind_factor=20, speed_mod=0,    run_speed=0.6},
  [3] = {name="弓",   collider_center={0,0.05,0.25}, collider_radius=0.15, swing_scale={1.6,2,2},     attackkind_factor=30, speed_mod=0.02,  run_speed=0.6},
  [4] = {name="法杖", collider_center={0,0.05,0.2},  collider_radius=0.15, swing_scale={1.6,2,2},     attackkind_factor=40, speed_mod=-0.02, run_speed=0.6},
  [5] = {name="法器", collider_center={0,0.05,0.25}, collider_radius=0.15, swing_scale={1.6,2,2},     attackkind_factor=50, speed_mod=0.04,  run_speed=0.6},
}

-- ============================================================================
-- DB_WeaponShop: 26 把武器商店表 (C# DB_Weapon.cs 完整移植)
-- power/speed/name/info/mesh/kind/jadecost (name/info 为语言索引)
-- 供武器商店/背包系统使用 (Phase 3), 与运行时 DB_Weapon 类型表互不冲突
-- ============================================================================
M.DB_WeaponShop = {}
local function wsh(i, power, speed, name, info, mesh, kind, jade)
  M.DB_WeaponShop[i] = { power = power, speed = speed, name = name, info = info,
    mesh = mesh, kind = kind, jadecost = jade }
end
wsh( 0,   8, 1.0, 81, 61,  0, 0,   0)
wsh( 1,  15, 1.5, 41, 63,  1, 1,   0)
wsh( 2,  25, 2.0, 42, 64,  2, 2,   0)
wsh( 3,  35, 2.5, 43, 65,  3, 0,   0)
wsh( 4,  45, 3.0, 44, 66,  4, 1,   0)
wsh( 5,  55, 3.5, 45, 67,  5, 2,   0)
wsh( 6,  65, 4.0, 46, 68,  6, 0,   0)
wsh( 7,  75, 4.5, 47, 69,  7, 1,   0)
wsh( 8,  85, 5.0, 48, 70,  8, 2,   0)
wsh( 9,  95, 5.5, 49, 68,  9, 0,   0)
wsh(10, 105, 6.0, 50, 69, 10, 1,   0)
wsh(11, 115, 6.5, 51, 70, 11, 2,   0)
wsh(12, 125, 7.0, 52, 68, 12, 0,   0)
wsh(13, 135, 7.5, 53, 69, 13, 1,   0)
wsh(14, 145, 8.0, 54, 70, 14, 2,   0)
wsh(15, 155, 8.5, 55, 71, 15, 0,   0)
wsh(16,  55, 5.8, 71, 92, 16, 0,  45)
wsh(17,  55, 6.1, 72, 93, 17, 1,  60)
wsh(18,  55, 6.5, 73, 94, 18, 2,  60)
wsh(19, 135, 6.8, 74, 95, 19, 0,  70)
wsh(20, 135, 7.1, 75, 96, 20, 1, 115)
wsh(21, 135, 7.5, 76, 97, 21, 2, 115)
wsh(22, 220, 7.8, 77, 98, 22, 0, 100)
wsh(23, 220, 8.1, 78, 99, 23, 1, 230)
wsh(24, 220, 8.5, 79,100, 24, 2, 230)
wsh(25, 280, 9.5, 80,101, 25, 0, 200)

-- 武器特殊属性
M.DB_WeaponSpecial = {
  [-1] = {name="无"},
  [0]  = {name="火属性", attribute=1},  -- 灼烧
  [1]  = {name="冰属性", attribute=2},  -- 冰冻
  [2]  = {name="雷属性", attribute=3},  -- 麻痹
  [3]  = {name="毒属性", attribute=4},  -- 中毒
  [4]  = {name="破甲",   guard_break=true},
  [5]  = {name="吸血",   lifesteal=true},
  [6]  = {name="技能强化", skill_boost=true},
}

-- ============================================================================
-- DB_General: 8 种可召唤武将
-- ============================================================================
M.DB_General = {
  [0] = {name="关羽", weapon=2, kind=0, maxhp=300, atk=80, def=30, atkspd=0.04, voice=0, scale=1.4},
  [1] = {name="张飞", weapon=2, kind=1, maxhp=400, atk=70, def=40, atkspd=0.03, voice=1, scale=1.4},
  [2] = {name="赵云", weapon=2, kind=2, maxhp=280, atk=90, def=25, atkspd=0.05, voice=2, scale=1.0},
  [3] = {name="马超", weapon=2, kind=3, maxhp=300, atk=85, def=28, atkspd=0.04, voice=3, scale=1.1},
  [4] = {name="黄忠", weapon=3, kind=4, maxhp=250, atk=100,def=20, atkspd=0.06, voice=4, scale=1.0},
  [5] = {name="吕布", weapon=2, kind=5, maxhp=500, atk=120,def=50, atkspd=0.04, voice=5, scale=1.4},
  [6] = {name="孙策", weapon=0, kind=6, maxhp=320, atk=90, def=30, atkspd=0.05, voice=0, scale=1.0},
  [7] = {name="周瑜", weapon=4, kind=7, maxhp=260, atk=95, def=22, atkspd=0.06, voice=1, scale=1.0},
}

-- ============================================================================
-- DB_GeneralPool: 30 位武将卡牌池 (C# DB_General.cs 完整移植)
-- voice/kind/weapon/skillname/skillindex/skillkind/skillatk/cooltime/soulcost
-- 供武将管理/卡牌系统使用 (Phase 3), 与运行时 DB_General 召唤表互不冲突
-- ============================================================================
M.DB_GeneralPool = {}
local function gpool(i, voice, kind, weapon, sname, sidx, skind, satk, cool, soul)
  M.DB_GeneralPool[i] = { voice = voice, kind = kind, weapon = weapon, skillname = sname,
    skillindex = sidx, skillkind = skind, skillatk = satk, cooltime = cool, soulcost = soul }
end
gpool( 0, 5, 0, 7,  1, 21, 2,  40, 30, 1)
gpool( 1, 2, 1, 2,  2, 22, 1, 150, 30, 1)
gpool( 2, 4, 2, 3,  3, 23, 2,  40, 30, 1)
gpool( 3, 1, 3, 8,  4, 24, 2,  30, 30, 1)
gpool( 4, 5, 4, 5,  5, 25, 4, 110, 30, 1)
gpool( 5, 3, 0, 1,  6, 26, 4, 120, 30, 1)
gpool( 6, 1, 1, 2,  7, 27, 1,  70, 30, 1)
gpool( 7, 1, 2, 3,  8, 28, 3,  40, 30, 1)
gpool( 8, 4, 3, 4,  9, 29, 4, 100, 30, 1)
gpool( 9, 0, 4, 5, 10, 30, 3, 200, 30, 1)
gpool(10, 0, 0, 7, 11, 31, 4, 120, 30, 1)
gpool(11, 2, 1, 2, 12, 32, 1, 100, 30, 1)
gpool(12, 4, 2, 3, 13, 33, 3,  60, 30, 1)
gpool(13, 4, 3, 4, 14, 34, 1, 120, 30, 1)
gpool(14, 0, 4, 5, 15, 35, 4,  70, 30, 1)
gpool(15, 5, 0, 6, 16, 36, 1,  50, 30, 1)
gpool(16, 1, 1, 2, 17, 37, 2,  50, 30, 1)
gpool(17, 3, 2, 3, 18, 38, 1,   6, 30, 1)
gpool(18, 1, 3, 4, 19, 39, 1,  40, 30, 1)
gpool(19, 1, 4, 5, 20, 40, 3,  40, 30, 1)
gpool(20, 3, 0, 7,  0, -1, 0,   0,  0, 0)
gpool(21, 2, 1, 2,  0, -2, 0,   0,  0, 0)
gpool(22, 0, 2, 3,  0, -3, 0,   0,  0, 0)
gpool(23, 1, 3, 4,  0, -4, 0,   0,  0, 0)
gpool(24, 0, 4, 5,  0, -5, 0,   0,  0, 0)
gpool(25, 5, 0, 7,  0, -6, 0,   0,  0, 0)
gpool(26, 2, 1, 2,  0, -7, 0,   0,  0, 0)
gpool(27, 1, 2, 3,  0, -8, 0,   0,  0, 0)
gpool(28, 1, 3, 4,  0, -9, 0,   0,  0, 0)
gpool(29, 0, 4, 5,  0,-10, 0,   0,  0, 0)

-- ============================================================================
-- DB_Equipment: 装备系统
-- ============================================================================
M.DB_Accessory = {
  [0] = {name="攻击+10%", atk_mult=1.1},
  [1] = {name="防御+20%", def_mult=1.2},
  [2] = {name="灵魂初始+", soul_start=true},
  [3] = {name="武将攻击+10%", g_atk_mult=1.1},
  [4] = {name="SP恢复+20%", sp_plus=1.2},
  [5] = {name="攻速+2%", atkspd_plus=0.02},
}

-- 怪物受击状态 (10种)
M.DamageStates = {
  NORMAL   = 0,  -- 普通受击
  POISON   = 1,  -- 中毒
  BURN     = 2,  -- 燃烧
  FREEZE   = 3,  -- 冰冻
  ELECTRIC = 4,  -- 麻痹
  STUN     = 5,  -- 眩晕
  DOWN     = 6,  -- 倒地
  FLY      = 7,  -- 击飞
  GRAB     = 8,  -- 被抓
  PETRIFY  = 9,  -- 石化
}

-- 玩家状态机 (chamovestat)
M.ChaMoveStat = {
  IDLE       = 0,   -- 待机
  WALK       = 1,   -- 行走
  RUN        = 2,   -- 奔跑
  DASH       = 3,   -- 冲刺
  ATTACK1    = 11,  -- 攻击1
  ATTACK2    = 12,  -- 攻击2 (蓄力)
  ATTACK3    = 13,  -- 攻击3
  DASH_ATK   = 19,  -- 冲刺攻击
  DODGE      = -1,  -- 闪避
  BEHIT      = 20,  -- 受击
  BLOCKED    = 21,  -- 格挡
  DEAD       = 22,  -- 死亡
  GRAB       = 112, -- 抓取
  SKILL      = 180, -- 技能
  CHANGE     = 100, -- 换装
  RIDE       = 200, -- 骑乘
  EAGLE      = 190, -- 飞行
}

return M
