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
st(17, 1, 1, 3, 8, 1, 6,-1,-1,-1, 1)

-- Stage 18-20: 长安城 (Stage 7)
st(18, 0, 1, 3, 8, 1, -1,-1,-1,-1, 1)
st(19, 0, 1, 3, 8, 1, -1,-1,-1,-1, 1)
st(20, 1, 1, 3, 8, 1, 7,-1,-1,-1, 1)

-- Stage 21-23: 宛城 (Stage 8)
st(21, 0, 1, 3, 8, 1, -1,-1,-1,-1, 1)
st(22, 0, 1, 3, 8, 1, -1,-1,-1,-1, 1)
st(23, 1, 1, 3, 8, 1, 8,-1,-1,-1, 1)

-- Stage 24-26: 赤壁 (Stage 9)
st(24, 0, 1, 3, 8, 1, -1,-1,-1,-1, 1)
st(25, 0, 1, 3, 8, 1, -1,-1,-1,-1, 1)
st(26, 1, 1, 3, 8, 1, 9,-1,-1,-1, 1)

-- Stage 27-29: 最终关 (Stage 10)
st(27, 0, 1, 3, 8, 1, -1,-1,-1,-1, 1)
st(28, 0, 1, 3, 8, 1, -1,-1,-1,-1, 1)
st(29, 1, 1, 3, 8, 1, 10,-1,-1,-1, 203)

-- 剩余关卡 (30-89): 无尽模式循环
for i = 30, 89 do
  local boss_idx = (i % 3 == 0) and (i % 11) or -1
  st(i, boss_idx >= 0 and 1 or 0, 1, 3, 8, 1, boss_idx, -1, -1, -1, 1)
end

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
