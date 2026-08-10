-- ============================================================================
-- state.lua — 共享游戏状态
-- 供 main.lua 与各功能模块 (cam_move / monster_efs / weapon_damage / cutin01)
-- require 共享, 避免模块间循环依赖。表为可变引用, 原地清空而非整表替换。
-- ============================================================================

-- ── 数学工具 ──────────────────────────────────────────────────────────
local atan2 = math.atan2 or math.atan
local function clamp(v, lo, hi) return v < lo and lo or (v > hi and hi or v) end
local function dist2d(x1, z1, x2, z2) local dx, dz = x1-x2, z1-z2; return math.sqrt(dx*dx+dz*dz) end
local function lerp(a, b, t) return a + (b - a) * t end
local function sign(v) return v > 0 and 1 or (v < 0 and -1 or 0) end

-- 实体销毁 (安全)
local function kill_entity(e)
  if e and e ~= 0 then pcall(dse.ecs.destroy_entity, e) end
end

-- 资产路径解析 (data/ → assets/ → 模板仓库相对路径)
local function file_exists(path)
  local f = io.open(path, "rb")
  if f then f:close(); return true end
  return false
end
local function resolve_path(path)
  if file_exists(path) then return path end
  for _, p in ipairs({"data/" .. path, "assets/" .. path, "templates/topdown_3d/" .. path}) do
    if file_exists(p) then return p end
  end
  return path
end

-- 模型基准缩放 (见 model_scale.lua): 由 AssetBuilder 转换 dmesh 后按原始尺寸计算,
-- 乘到实体 scale 上使模型放大到目标世界尺寸 (角色高约 1.7 世界单位)
local MODEL_SCALE = require("model_scale")
local function mesh_key(mesh)
  local m = tostring(mesh or "")
  local base = m:match("([^/\\]+)%.[A-Za-z0-9]+$")
  return base or m
end
local function base_scale(mesh)
  return MODEL_SCALE[mesh_key(mesh)] or 1
end

-- ============================================================================
-- 全局游戏状态
-- ============================================================================
local G = {
  mode = "play",         -- play / level_complete / game_over / win / boss_intro
  stage_index = 0,       -- 当前关卡索引 (0-89)
  stage_kind = 0,        -- 0=普通 1=骑乘 2=无限
  wave = 1,              -- 当前波数
  finalstage = 3,        -- 最终波数
  time = 0,
  lives = 3,
  score = 0,
  coin = 0,
  soul = 1,
  jade = 0,
  combo = 0,
  combo_timer = 0,
  combo_max = 0,
  enemykill = 0,
  totalkill = 0,
  grappling = 0,
  difficulty = 1,        -- 1=普通 2=困难
  infinitymode = false,
  time_scale = 1.0,      -- 时间减速
  time_scale_timer = 0,
  cam = nil,
  cam_shake = 0,
  cam_zoom_target = 30,
  cam_zoom_current = 30,
  cam_zoom_timer = 0,
  cam_zoom_duration = 0,
  cam_look_target = nil,
  cam_look_timer = 0,
  player_e = nil,
  boss_e = nil,
  boss_active = false,
  boss_intro_timer = 0,
  level_complete_timer = 0,
  sp_recover_timer = 0,
  spawn_timer = 0,
  spawn_interval = 1.0,
  max_enemies = 15,
  enemies_alive = 0,
  -- 存档/进度 (Phase 3)
  stage_clear = {},         -- 每关最佳星级 [stage] = 0-3 (C# n15)
  max_stage_index = 0,      -- 已解锁的最大关卡 (C# n06)
  current_stage = 0,        -- 地图中当前选中的关卡 (C# sel_stage_index)
}

-- ============================================================================
-- 玩家状态 (Cha_Control 完整移植)
-- ============================================================================
local Player = {
  -- 位置/朝向
  x = 0, z = 0, y = 0, yaw = 0,
  -- 属性
  level = 1, exp = 0,
  maxhp = 100, hp = 100,
  maxsp = 100, sp = 100,
  maxatk = 50, minatk = 30, atk = 40,
  defence = 10, hitrate = 90, evasion = 10,
  critical = 15, endurance = 50, vitality = 100,
  atkspd = 0.02,
  movespeed = 0.48,
  -- 武器
  weapon_kind = 0,        -- 0=刀 1=双刀 2=枪 3=弓 4=杖 5=法器
  battlestyle = 0,
  cur_weapon = 0,
  -- 连击
  curruntattack = 0,      -- 当前攻击段数
  attackkind_factor = 0,  -- 武器攻击系数
  combotime = 0,          -- 连击窗口
  -- 状态机
  chamovestat = 0,        -- 移动状态 (对应 C# chamovestat)
  -- 计时器
  attacking = 0,          -- 攻击动画计时
  attack_queued = nil,    -- 排队中的下一个攻击
  invuln = 0,             -- 无敌时间
  dodge_timer = 0,        -- 闪避计时
  block_timer = 0,        -- 格挡计时
  grab_timer = 0,         -- 抓取计时
  skill_timer = 0,        -- 技能计时
  skill_cd = {},          -- 技能冷却表
  -- 控制锁 (Cutin01 特写期间锁定输入)
  control_lock = 0,
  -- 标志
  life = true,
  isplaycha = true,
  isinvincibility = false,
  target_invincibility = 0,
  delay_invincibility = 0,
  -- 特殊
  superMode = 0,          -- 超级模式
  special_kind = -2,      -- 武器特殊属性
  guard_break = 0,        -- 破甲值
  speedfactor = 7,        -- 移动速度因子
  attack_rising = false,  -- 升龙攻击
  -- 视觉状态 (用于程序化动画)
  visual_state = "idle",
  visual_timer = 0,
  visual_duration = 0,
  hit_flash = 0,
  -- 宠物
  pet_ing = false,
  currentPet = -1,        -- -1=无 0=马 1=鹰
  -- 武将
  general = false,
  general_kind = 0,
  general_hp = 0,
  general_maxhp = 0,
  general_atk = 0,
  general_def = 0,
  general_atkspd = 0,
  change_cha = false,
  -- 闪避方向
  dodge_dx = 0, dodge_dz = 0,
  -- 击退
  knockback_x = 0, knockback_z = 0, knockback_timer = 0,
  -- 方向向量 (C# directionVector)
  dir_x = 0, dir_z = 0,
  -- 攻击方向
  atk_dir_x = 0, atk_dir_z = 0,
  -- 限制区域
  limit_x = 2.65, limit_y_b = -2.6, limit_y_f = 2.5,
  -- 技能系统 (Cha_Skill 完整移植)
  skill_slots = {0, 2, 4, 5, 8, 11},  -- 6 个技能槽位对应的技能集
  skill_grades = {},                   -- 技能集等级 [set] = grade(0-4)
  current_skill_slot = 1,              -- 当前选择的技能槽 (1-6)
  casting = false,                     -- 是否正在施法
  casting_timer = 0,                   -- 施法计时
  casting_delay = 0,                   -- 施法延迟 (到技能发射)
  motionkind = 0,                      -- 施法动作类型 (1=cast1 2=cast2 3=cast3 4=cast4)
  skillatk = 0,                        -- 技能攻击力
  skill_index = -1,                    -- 当前技能集索引
  basedamage = 1.0,                    -- 基础伤害系数
  -- 重复技能 (Repeatskill)
  repeat_skill = false,                -- 是否重复技能
  repeat_time = 0,                     -- 重复次数
  repeat_delay = 0,                    -- 重复延迟
  repeat_atk = 0,                      -- 重复攻击力
  -- 防御/攻击增益
  defence_up = 0,                      -- 防御增益计时
  attack_up = 0,                       -- 攻击增益计时
  attack_up_factor = 1.0,              -- 攻击增益系数
  defence_up_factor = 1.0,             -- 防御增益系数
}

-- ============================================================================
-- 敌人/实体管理
-- ============================================================================
local Entities = {
  enemies = {},     -- 活跃的敌人列表
  boss = nil,       -- Boss 数据
  structures = {},  -- 建筑
  treasures = {},   -- 宝物
  decor = {},       -- 装饰物
  damage_texts = {},-- 伤害飘字
  particles = {},   -- 粒子特效
  bullets = {},     -- 子弹/投射物
  effects = {},     -- 临时特效实体
  ground = nil,
  swing_ef = {},    -- 挥砍特效
  spawn_points = {},-- 刷怪点
  boss_weapons = {},-- Boss 武器实体 (C# clone_weapon)
  drops = {},       -- 武器掉落 (C# WeaponDrop)
}

-- 原地清空实体表 (各模块持有同一引用, 不能整表替换)
local function reset_entities()
  for k in pairs(Entities) do Entities[k] = nil end
  Entities.enemies = {}
  Entities.boss = nil
  Entities.structures = {}
  Entities.treasures = {}
  Entities.decor = {}
  Entities.damage_texts = {}
  Entities.particles = {}
  Entities.bullets = {}
  Entities.effects = {}
  Entities.ground = nil
  Entities.swing_ef = {}
  Entities.spawn_points = {}
  Entities.boss_weapons = {}
  Entities.drops = {}
end

local M = {
  G = G,
  Player = Player,
  Entities = Entities,
  TEX = nil,                 -- main.lua LoadTextures 后注入的原版贴图路径表
  atan2 = atan2,
  clamp = clamp,
  dist2d = dist2d,
  lerp = lerp,
  sign = sign,
  kill_entity = kill_entity,
  resolve_path = resolve_path,
  base_scale = base_scale,
  mesh_key = mesh_key,
  reset_entities = reset_entities,
}

return M
