-- ============================================================================
-- pet_system.lua — 完整宠物/骑乘系统
-- 1:1 移植自 Unity 逆向 C# 源码:
--   petset.cs             (  14 行) — 宠物技能数据结构
--   DB_PetSkill.cs        ( 130 行) — 宠物技能数据库 (2×10)
--   angelkind.cs          (  14 行) — 天使数据结构
--   DB_angel.cs           (  58 行) — 天使数据库 (8种)
--   Pet_eagle.cs          ( 227 行) — 猎鹰宠物: 飞行/拾取/攻击
--   Pet_horse.cs          ( 162 行) — 战马坐骑: 骑乘/冲刺/消失
--   Pet_eagle_UI.cs       (  17 行) — 猎鹰 UI 动画
--   Pet_horse_UI.cs       (  16 行) — 战马 UI 动画
--   Shadow_eagle.cs       (  24 行) — 猎鹰阴影
--   Cha_Control.cs        (宠物部分) — CallHorse/Fly/RideHorse/PetSkillFinish
--   Cha_Skill.cs          (宠物部分) — PetSkillOn
--   Cha_Control_ride_cha.cs (213 行) — 骑乘角色控制
--   Cha_Control_ride_horse.cs(253行) — 骑乘战马控制
--   Cam_Move_ride.cs      (  46 行) — 骑乘摄像机
--   AI_Ride_Enemy.cs      ( 119 行) — 骑乘近战敌人
--   AI_Ride_Enemy2.cs     ( 106 行) — 骑乘远程敌人
--   Bullet_arrow_ride.cs  (  38 行) — 骑乘箭矢
--   Ef_swing1_ride.cs     ( 165 行) — 骑乘挥砍特效
--   UI_pet.cs             ( 840 行) — 宠物管理 UI
--   UI_result_ride.cs     ( 146 行) — 骑乘结算
--   Icon_Skill.cs         (宠物部分) — ispetready/Duration_reduce/pet_hunger
-- 依赖: state.lua
-- ============================================================================

local S = require "scripts.state"
local G, Player, Entities = S.G, S.Player, S.Entities
local clamp, dist2d, lerp, atan2 = S.clamp, S.dist2d, S.lerp, S.atan2
local kill_entity = S.kill_entity

-- ── 内部工具 ─────────────────────────────────────────────────────────
local function yaw_to_dir(yaw)
  local rad = math.rad(yaw)
  return math.sin(rad), -math.cos(rad)
end
local function dir_to_yaw(dx, dz)
  if dx == 0 and dz == 0 then return 0 end
  return math.deg(atan2(dx, -dz))
end
local function lerp_angle(a, b, t)
  local diff = b - a
  while diff > 180 do diff = diff - 360 end
  while diff < -180 do diff = diff + 360 end
  return a + diff * t
end
local function rand_range(a, b) return a + math.random() * (b - a) end
local function pforward() return yaw_to_dir(Player.yaw) end
local function pright()
  local fx, fz = yaw_to_dir(Player.yaw)
  return -fz, fx
end

-- ============================================================================
-- petset: 宠物技能数据结构 (C# petset.cs)
-- ============================================================================
-- _grade: 等级
-- _info: 信息索引
-- _attackpoint: 攻击力
-- _price: 升级价格(玉)
-- _passive: 被动技能值
-- _duration: 持续时间(秒)

-- ============================================================================
-- DB_PetSkill: 宠物技能数据库 (C# DB_PetSkill.cs)
-- ps[2, 10] — 2种宠物 × 10级
-- 宠物0: 战马 (攻击力高, 持续时间长)
-- 宠物1: 猎鹰 (攻击力低, 持续时间短)
-- ============================================================================
local DB_PetSkill = {}
DB_PetSkill[0] = {}  -- 战马
DB_PetSkill[1] = {}  -- 猎鹰

-- 战马数据 (ps[0, 0~9])
local horse_data = {
  {atk=100, dur=8,  grade=0, price=2, info=69, passive=0},
  {atk=110, dur=9,  grade=1, price=2, info=0,  passive=10},
  {atk=120, dur=10, grade=2, price=2, info=275,passive=20},
  {atk=130, dur=11, grade=3, price=2, info=277,passive=30},
  {atk=140, dur=12, grade=4, price=2, info=0,  passive=40},
  {atk=150, dur=13, grade=5, price=2, info=0,  passive=50},
  {atk=160, dur=14, grade=6, price=2, info=0,  passive=60},
  {atk=170, dur=15, grade=7, price=2, info=0,  passive=70},
  {atk=180, dur=16, grade=8, price=2, info=0,  passive=80},
  {atk=190, dur=17, grade=9, price=2, info=0,  passive=90},
}
for i, d in ipairs(horse_data) do
  DB_PetSkill[0][i-1] = {
    grade = d.grade, info = d.info, attackpoint = d.atk,
    price = d.price, passive = d.passive, duration = d.dur,
  }
end

-- 猎鹰数据 (ps[1, 0~9])
local eagle_data = {
  {atk=40, dur=7,  grade=0, price=2, info=70, passive=0},
  {atk=45, dur=8,  grade=1, price=2, info=0,  passive=10},
  {atk=50, dur=9,  grade=2, price=2, info=276,passive=20},
  {atk=55, dur=10, grade=3, price=2, info=278,passive=30},
  {atk=60, dur=11, grade=4, price=2, info=0,  passive=40},
  {atk=65, dur=12, grade=5, price=2, info=0,  passive=50},
  {atk=70, dur=13, grade=6, price=2, info=0,  passive=60},
  {atk=75, dur=14, grade=7, price=2, info=0,  passive=70},
  {atk=80, dur=15, grade=8, price=2, info=0,  passive=80},
  {atk=85, dur=16, grade=9, price=2, info=0,  passive=90},
}
for i, d in ipairs(eagle_data) do
  DB_PetSkill[1][i-1] = {
    grade = d.grade, info = d.info, attackpoint = d.atk,
    price = d.price, passive = d.passive, duration = d.dur,
  }
end

-- ============================================================================
-- DB_angel: 天使数据库 (C# DB_angel.cs)
-- 8种天使, 提供不同的支援射击
-- _firerate: 射击间隔 (越小越快)
-- _speed: 箭矢速度
-- _arrowkind: 箭矢类型 (对应 Bullet 子类)
-- _splashEF: 溅射特效类型 (0=无 2-8=各种溅射)
-- ============================================================================
local DB_angel = {
  [0] = {name=436, info=446, firerate=3.0, speed=0.30, arrowkind=25, splashEF=0},
  [1] = {name=437, info=447, firerate=3.0, speed=0.32, arrowkind=30, splashEF=2},
  [2] = {name=438, info=448, firerate=5.0, speed=0.34, arrowkind=23, splashEF=3},
  [3] = {name=439, info=449, firerate=4.0, speed=0.36, arrowkind=31, splashEF=4},
  [4] = {name=440, info=450, firerate=3.5, speed=0.38, arrowkind=31, splashEF=5},
  [5] = {name=441, info=451, firerate=3.4, speed=0.40, arrowkind=25, splashEF=6},
  [6] = {name=442, info=452, firerate=2.2, speed=0.60, arrowkind=25, splashEF=0},
  [7] = {name=443, info=453, firerate=3.2, speed=0.50, arrowkind=25, splashEF=8},
}

-- 天使名称映射
local AngelNames = {
  [436]="火灵", [437]="冰灵", [438]="雷灵", [439]="毒灵",
  [440]="风灵", [441]="光灵", [442]="暗灵", [443]="圣灵",
}

-- ============================================================================
-- 宠物持久化状态 (C# PlayerPrefsX 保存的 n23/n25/n27)
-- ============================================================================
local PetState = {
  -- pet_activeskill[0..1] — 主动技能等级 (0-9)
  pet_activeskill = {0, 0},
  -- pet_passiveskill[0..1] — 被动技能等级 (0-9)
  pet_passiveskill = {0, 0},
  -- pet_hunger[0..1] — 饥饿值 (0-5, 0=饥饿, 5=饱)
  pet_hunger = {5, 5},
  -- pet_skill_use[0..1] — 使用次数
  pet_skill_use = {0, 0},
  -- ispetready[0..1] — 是否可用
  ispetready = {true, true},
  -- cur_angel — 当前装备的天使 (0=无, 1-8=天使索引)
  cur_angel = 0,
  -- max_extreme_stage — 极限模式通关数 (影响天使解锁)
  max_extreme_stage = 1,
}

-- ============================================================================
-- Pet_eagle: 猎鹰宠物 (C# Pet_eagle.cs)
-- 行为: 飞行跟随玩家, 随机位置移动, 技能激活时贴身飞行
--       可拾取掉落物品, 有生命周期
-- ============================================================================
local Eagle = {
  -- 实体
  e = nil,
  -- 位置
  x = 0, y = 0.3, z = 0, yaw = 0,
  -- 边缘偏移 (C# edgepos)
  edge_x = 0.6, edge_y = 0.3, edge_z = 0.5,
  -- 飞行速度
  flyspeed = 0.5,
  -- 旋转目标
  target_yaw = 0,
  -- 技能激活
  skillon = false,
  -- 动画速度变化标志
  anispeedchange = false,
  -- 技能延迟 (贴身飞行过渡)
  skilldelay = 2.0,
  -- 生命周期
  finishdelay = -3.0,
  -- 拾取行为
  itembehaviour = 0,  -- 0=正常, 1=飞向物品, 2=带回物品
  finditem = nil,     -- 目标物品
  getitem_delay = 0,
  -- 随机位置更新计时
  _load_dir_timer = 0,
  _rnd_pos_timer = 0,
  -- 是否激活
  active = false,
  -- 攻击力 (来自 DB_PetSkill)
  attackpoint = 0,
  -- 攻击计时
  attack_timer = 0,
  -- 阴影实体
  shadow_e = nil,
}

-- 猎鹰激活 (C# Pet_eagle.OnEnable)
function Eagle.OnEnable()
  local num1 = (math.random(0, 1) * 2 - 1) * 0.6
  local num2 = (math.random(0, 1) * 2 - 1) * 0.5
  Eagle.edge_x = num1
  Eagle.edge_y = 0.3
  Eagle.edge_z = num2
  Eagle.x = Player.x + Eagle.edge_x
  Eagle.y = Eagle.edge_y
  Eagle.z = Player.z + Eagle.edge_z
  Eagle.flyspeed = 0.5
  Eagle.finishdelay = 0.0
  Eagle.skillon = false
  Eagle.itembehaviour = 0
  Eagle.active = true
  Eagle.skilldelay = 2.0
  Eagle.anispeedchange = true

  -- 创建 ECS 实体
  if not Eagle.e then
    Eagle.e = dse.ecs.create_entity()
    local bs = S.base_scale("assets/models/ball.dmesh")
    dse.ecs.add_transform(Eagle.e, Eagle.x, Eagle.y, Eagle.z, 0.3 * bs, 0.3 * bs, 0.3 * bs)
    pcall(dse.ecs.mesh_renderer_add, Eagle.e, "assets/models/ball.dmesh")
    dse.ecs.set_mesh_shader_variant(Eagle.e, "MESH_LIT")
    dse.ecs.set_mesh_color(Eagle.e, 0.8, 0.8, 1.0, 1.0)
  end
  -- 创建阴影
  if not Eagle.shadow_e then
    Eagle.shadow_e = dse.ecs.create_entity()
    local bs = S.base_scale("assets/models/ball.dmesh")
    dse.ecs.add_transform(Eagle.shadow_e, Eagle.x, 0.002, Eagle.z, 0.3 * bs, 0.02, 0.3 * bs)
    pcall(dse.ecs.mesh_renderer_add, Eagle.shadow_e, "assets/models/ball.dmesh")
    dse.ecs.set_mesh_shader_variant(Eagle.shadow_e, "MESH_LIT")
    dse.ecs.set_mesh_color(Eagle.shadow_e, 0, 0, 0, 0.3)
  end
end

-- 猎鹰停用
function Eagle.OnDisable()
  Eagle.active = false
  if Eagle.e then kill_entity(Eagle.e); Eagle.e = nil end
  if Eagle.shadow_e then kill_entity(Eagle.shadow_e); Eagle.shadow_e = nil end
end

-- 技能激活 (C# Pet_eagle.SkillOn)
function Eagle.SkillOn()
  Eagle.skillon = true
  Eagle.anispeedchange = true
end

-- 技能停用 (C# Pet_eagle.SkillOff)
function Eagle.SkillOff()
  Eagle.skillon = false
  Eagle.anispeedchange = true
end

-- 拾取物品 (C# Pet_eagle.GetItem)
function Eagle.GetItem(item_entity)
  if Eagle.itembehaviour == 0 and not Eagle.skillon then
    Eagle.finishdelay = 5.5
    Eagle.finditem = item_entity
    Eagle.itembehaviour = 1
    Eagle.attack_timer = 0
  end
end

-- 加载方向 (C# Pet_eagle.LoadDir) — 每0.1秒更新目标位置
local function eagle_load_dir()
  local target_x = Player.x + Eagle.edge_x
  local target_z = Player.z + Eagle.edge_z
  local dx = target_x - Eagle.x
  local dz = target_z - Eagle.z
  if dx ~= 0 or dz ~= 0 then
    Eagle.target_yaw = dir_to_yaw(dx, dz)
  end
end

-- 随机位置 (C# Pet_eagle.SetRndPosition) — 每2秒更新
local function eagle_set_rnd_position()
  local r = math.random() * 0.1
  local angle = math.random() * math.pi * 2
  Eagle.edge_x = math.cos(angle) * r
  Eagle.edge_y = 0.3
  Eagle.edge_z = math.sin(angle) * r
  -- 动画选择 (70% flap, 30% glide)
  local motion = math.random(0, 9)
  -- 在 DSEngine 中我们用颜色变化模拟动画切换
end

-- 猎鹰更新 (C# Pet_eagle.Update)
function Eagle.Update(dt)
  if not Eagle.active then return end

  -- 定时回调 (模拟 InvokeRepeating)
  Eagle._load_dir_timer = Eagle._load_dir_timer + dt
  if Eagle._load_dir_timer >= 0.1 then
    Eagle._load_dir_timer = 0
    if Eagle.itembehaviour == 0 and not Eagle.skillon then
      eagle_load_dir()
    end
  end
  Eagle._rnd_pos_timer = Eagle._rnd_pos_timer + dt
  if Eagle._rnd_pos_timer >= 2.0 then
    Eagle._rnd_pos_timer = 0
    if Eagle.itembehaviour == 0 and not Eagle.skillon then
      eagle_set_rnd_position()
    end
  end

  -- 技能激活模式: 贴身飞行
  if Eagle.skillon then
    if Eagle.skilldelay > 0 then
      -- 过渡: Lerp 到玩家头顶
      Eagle.x = lerp(Eagle.x, Player.x, dt * 6)
      Eagle.z = lerp(Eagle.z, Player.z, dt * 6)
      Eagle.y = lerp(Eagle.y, 0.3, dt * 6)
      Eagle.skilldelay = Eagle.skilldelay - dt
      local dx = Player.x - Eagle.x
      local dz = Player.z - Eagle.z
      if dx ~= 0 or dz ~= 0 then
        Eagle.yaw = lerp_angle(Eagle.yaw, dir_to_yaw(dx, dz), dt * 6)
      end
    else
      -- 贴身: 固定在玩家头顶
      Eagle.x = Player.x
      Eagle.z = Player.z
      Eagle.y = 0.3
      Eagle.yaw = lerp_angle(Eagle.yaw, Player.yaw, dt * 3)
    end

    -- 自动攻击附近敌人
    Eagle.attack_timer = Eagle.attack_timer + dt
    if Eagle.attack_timer >= 1.0 then
      Eagle.attack_timer = 0
      local nearest_dist = 999
      local nearest_en = nil
      for _, en in ipairs(Entities.enemies) do
        if not en.dead then
          local d = dist2d(Eagle.x, Eagle.z, en.x, en.z)
          if d < 6.0 and d < nearest_dist then
            nearest_dist = d
            nearest_en = en
          end
        end
      end
      if nearest_en and PetSystem.on_eagle_attack then
        PetSystem.on_eagle_attack(nearest_en, Eagle.attackpoint)
      end
    end

  -- 拾取物品模式
  elseif Eagle.itembehaviour == 1 then
    if Eagle.finditem and Eagle.finditem.active ~= false then
      Eagle.getitem_delay = Eagle.getitem_delay + dt
      if Eagle.getitem_delay > 2.5 then
        Eagle.getitem_delay = 0
        Eagle.itembehaviour = 2
      elseif Eagle.getitem_delay > 2.2 then
        -- 物品飞向猎鹰
        if Eagle.finditem.x then
          Eagle.finditem.x = lerp(Eagle.finditem.x, Eagle.x, dt * 2)
          Eagle.finditem.z = lerp(Eagle.finditem.z, Eagle.z, dt * 2)
        end
      end
      -- 猎鹰飞向物品
      if Eagle.finditem.x then
        Eagle.x = lerp(Eagle.x, Eagle.finditem.x, dt * 2)
        Eagle.z = lerp(Eagle.z, Eagle.finditem.z, dt * 2)
        local dx = Eagle.finditem.x - Eagle.x
        local dz = Eagle.finditem.z - Eagle.z
        if dx ~= 0 or dz ~= 0 then
          Eagle.yaw = dir_to_yaw(dx, dz)
        end
      end
    else
      Eagle.itembehaviour = 0
    end

  -- 带回物品模式
  elseif Eagle.itembehaviour == 2 then
    if Eagle.finditem and Eagle.finditem.active ~= false then
      Eagle.getitem_delay = Eagle.getitem_delay + dt
      if Eagle.getitem_delay > 2.0 then
        Eagle.itembehaviour = 0
        Eagle.getitem_delay = 0
      elseif Eagle.getitem_delay > 1.0 then
        -- 物品飞向玩家
        if Eagle.finditem.x then
          Eagle.finditem.x = lerp(Eagle.finditem.x, Player.x, dt * 1.2)
          Eagle.finditem.z = lerp(Eagle.finditem.z, Player.z, dt * 1.2)
        end
        -- 猎鹰前进
        local fx, fz = yaw_to_dir(Eagle.yaw)
        Eagle.x = Eagle.x + fx * dt * 0.8
        Eagle.z = Eagle.z + fz * dt * 0.8
      else
        -- 猎鹰转向玩家
        local dx = Player.x - Eagle.x
        local dz = Player.z - Eagle.z
        if dx ~= 0 or dz ~= 0 then
          Eagle.yaw = lerp_angle(Eagle.yaw, dir_to_yaw(dx, dz), dt * 3)
        end
        Eagle.x = lerp(Eagle.x, Player.x, dt * 2)
        Eagle.z = lerp(Eagle.z, Player.z, dt * 2)
        Eagle.y = lerp(Eagle.y, 0.3, dt * 2)
        if Eagle.finditem.x then
          Eagle.finditem.x = Eagle.x
          Eagle.finditem.z = Eagle.z
          Eagle.finditem.y = Eagle.y - 0.1
        end
      end
    else
      Eagle.itembehaviour = 0
    end

  -- 正常飞行模式
  else
    Eagle.finishdelay = Eagle.finishdelay + dt
    -- 8秒后加速, 9.5秒后消失
    if Eagle.finishdelay > 8.0 then
      Eagle.flyspeed = 1.0
      if Eagle.finishdelay > 9.5 then
        Eagle.OnDisable()
        return
      end
    end
    Eagle.skilldelay = 2.0
    -- 旋转插值
    Eagle.yaw = lerp_angle(Eagle.yaw, Eagle.target_yaw, dt * 1.6)
    -- 前进
    local fx, fz = yaw_to_dir(Eagle.yaw)
    Eagle.x = Eagle.x + fx * dt * Eagle.flyspeed
    Eagle.z = Eagle.z + fz * dt * Eagle.flyspeed
  end

  -- 同步 ECS
  if Eagle.e then
    dse.ecs.set_transform_position(Eagle.e, Eagle.x, Eagle.y, Eagle.z)
    dse.ecs.set_transform_rotation(Eagle.e, 0, Eagle.yaw, 0)
  end
  -- 同步阴影 (C# Shadow_eagle.cs)
  if Eagle.shadow_e then
    dse.ecs.set_transform_position(Eagle.shadow_e, Eagle.x, 0.002, Eagle.z)
  end
end

-- ============================================================================
-- Pet_horse: 战马坐骑 (C# Pet_horse.cs)
-- 行为: 技能召唤时跑向玩家, 接触后骑乘, 骑乘时跟随玩家
--       下马后奔跑消失
-- ============================================================================
local Horse = {
  -- 实体
  e = nil,
  -- 位置
  x = 0, y = 0, z = 0, yaw = 0,
  -- 骑乘状态
  rideon = false,
  -- 技能激活
  skillon = false,
  -- 消失状态
  disappear = false,
  disappeardelay = 0,
  -- 距离
  horsedistance = 0,
  -- 是否激活
  active = false,
  -- 阴影实体
  shadow_e = nil,
}

-- 战马技能激活 (C# Pet_horse.SkillOn)
function Horse.SkillOn()
  Horse.active = true
  -- 随机角度出现在玩家周围2米处
  local f = math.random() * math.pi * 2
  Horse.x = Player.x + math.cos(f) * 2.0
  Horse.z = Player.z + math.sin(f) * 2.0
  Horse.y = 0
  Horse.skillon = true
  Horse.disappear = false
  Horse.rideon = false
  -- 朝向玩家
  local dx = Player.x - Horse.x
  local dz = Player.z - Horse.z
  if dx ~= 0 or dz ~= 0 then
    Horse.yaw = dir_to_yaw(dx, dz)
  end

  -- 创建 ECS 实体
  if not Horse.e then
    Horse.e = dse.ecs.create_entity()
    local bs = S.base_scale("assets/models/horse.dmesh")
    dse.ecs.add_transform(Horse.e, Horse.x, Horse.y, Horse.z, bs, bs, bs)
    pcall(dse.ecs.mesh_renderer_add, Horse.e, "assets/models/horse.dmesh")
    dse.ecs.set_mesh_shader_variant(Horse.e, "MESH_LIT")
    dse.ecs.set_mesh_color(Horse.e, 0.8, 0.6, 0.2, 1.0)
  end
  if not Horse.shadow_e then
    Horse.shadow_e = dse.ecs.create_entity()
    local bs = S.base_scale("assets/models/ball.dmesh")
    dse.ecs.add_transform(Horse.shadow_e, Horse.x, 0.002, Horse.z, bs, 0.02, bs)
    pcall(dse.ecs.mesh_renderer_add, Horse.shadow_e, "assets/models/ball.dmesh")
    dse.ecs.set_mesh_shader_variant(Horse.shadow_e, "MESH_LIT")
    dse.ecs.set_mesh_color(Horse.shadow_e, 0, 0, 0, 0.3)
  end
end

-- 骑乘 (C# Pet_horse.Rideing)
function Horse.Riding(stop)
  if stop then
    Horse.rideon = false
    Horse.active = false
    Horse.x = 71; Horse.y = 71; Horse.z = 71
  else
    Horse.rideon = true
    Horse.x = Player.x
    Horse.z = Player.z
    Horse.yaw = Player.yaw
  end
end

-- 下马 (C# Pet_horse.GetOffHorse)
function Horse.GetOffHorse()
  Horse.rideon = false
  Horse.disappear = true
  Horse.disappeardelay = 0
end

-- 骑乘并嘶鸣 (C# Pet_horse.RideandCry)
function Horse.RideandCry()
  Horse.rideon = true
  Horse.x = Player.x
  Horse.z = Player.z
  Horse.yaw = Player.yaw
end

-- 开始/停止奔跑 (C# Pet_horse.RunStart)
function Horse.RunStart(run)
  -- 在 DSEngine 中通过颜色/缩放变化模拟
end

-- 战马更新 (C# Pet_horse.Update)
function Horse.Update(dt)
  if not Horse.active then return end

  -- 消失模式
  if Horse.disappear then
    if Horse.disappeardelay < 3.0 then
      Horse.disappeardelay = Horse.disappeardelay + dt
      local fx, fz = yaw_to_dir(Horse.yaw)
      Horse.x = Horse.x + fx * dt * 0.8
      Horse.z = Horse.z + fz * dt * 0.8
    else
      Horse.disappear = false
      Horse.disappeardelay = 0
      Horse.active = false
      Horse.x = 4; Horse.y = 4; Horse.z = 4
      if Horse.e then kill_entity(Horse.e); Horse.e = nil end
      if Horse.shadow_e then kill_entity(Horse.shadow_e); Horse.shadow_e = nil end
    end
  -- 技能激活: 跑向玩家
  elseif Horse.skillon then
    Horse.horsedistance = dist2d(Player.x, Player.z, Horse.x, Horse.z)
    if Horse.horsedistance <= 1.5 and not Horse.rideon then
      -- 接触玩家: 骑乘
      Horse.rideon = true
      Horse.skillon = false
      if PetSystem.on_horse_ride then PetSystem.on_horse_ride() end
    else
      -- 跑向玩家
      Horse.x = lerp(Horse.x, Player.x, dt * 3.5)
      Horse.z = lerp(Horse.z, Player.z, dt * 3.5)
      local dx = Player.x - Horse.x
      local dz = Player.z - Horse.z
      if dx ~= 0 or dz ~= 0 then
        Horse.yaw = lerp_angle(Horse.yaw, dir_to_yaw(dx, dz), dt * 5)
      end
    end
  end

  -- 骑乘时跟随玩家
  if Horse.rideon then
    Horse.x = Player.x
    Horse.z = Player.z
    Horse.yaw = Player.yaw
  end

  -- 同步 ECS
  if Horse.e then
    dse.ecs.set_transform_position(Horse.e, Horse.x, Horse.y, Horse.z)
    dse.ecs.set_transform_rotation(Horse.e, 0, Horse.yaw, 0)
  end
  if Horse.shadow_e then
    dse.ecs.set_transform_position(Horse.shadow_e, Horse.x, 0.002, Horse.z)
  end
end

-- 战马停用
function Horse.OnDisable()
  Horse.active = false
  Horse.rideon = false
  Horse.skillon = false
  Horse.disappear = false
  if Horse.e then kill_entity(Horse.e); Horse.e = nil end
  if Horse.shadow_e then kill_entity(Horse.shadow_e); Horse.shadow_e = nil end
end

-- ============================================================================
-- Angel: 天使支援射击系统 (C# DB_angel + UI_pet 天使部分)
-- 天使在战斗中自动射击敌人, 提供火力支援
-- ============================================================================
local Angel = {
  -- 当前天使索引 (0=无)
  cur = 0,
  -- 射击计时
  fire_timer = 0,
  -- 箭矢实体列表
  arrows = {},
}

-- 天使箭矢更新 (C# Bullet_arrow_ride.cs 变体)
local function update_angel_arrows(dt)
  for i = #Angel.arrows, 1, -1 do
    local a = Angel.arrows[i]
    a.timer = a.timer + dt
    -- 前进
    local fx, fz = yaw_to_dir(a.yaw)
    a.x = a.x + fx * dt * a.speed
    a.z = a.z + fz * dt * a.speed
    -- 同步 ECS
    if a.e then
      dse.ecs.set_transform_position(a.e, a.x, a.y, a.z)
      dse.ecs.set_transform_rotation(a.e, 0, a.yaw, 0)
    end
    -- 碰撞检测
    local hit = false
    for _, en in ipairs(Entities.enemies) do
      if not en.dead and en.monmovestat and en.monmovestat > 0 then
        if dist2d(a.x, a.z, en.x, en.z) < 0.5 then
          if PetSystem.on_angel_hit then PetSystem.on_angel_hit(en, a.damage, a.splashEF) end
          hit = true
          break
        end
      end
    end
    if not hit and Entities.boss and not Entities.boss.dead then
      if dist2d(a.x, a.z, Entities.boss.x, Entities.boss.z) < 0.8 then
        if PetSystem.on_angel_hit then PetSystem.on_angel_hit(Entities.boss, a.damage, a.splashEF) end
        hit = true
      end
    end
    -- 过期或命中后移除
    if hit or a.timer > 5.0 then
      if a.e then kill_entity(a.e); a.e = nil end
      table.remove(Angel.arrows, i)
    end
  end
end

-- 天使射击
local function angel_fire()
  if Angel.cur == 0 then return end
  local data = DB_angel[Angel.cur - 1]
  if not data then return end

  -- 找最近敌人
  local nearest_dist = 999
  local nearest_en = nil
  for _, en in ipairs(Entities.enemies) do
    if not en.dead and en.monmovestat and en.monmovestat > 0 then
      local d = dist2d(Player.x, Player.z, en.x, en.z)
      if d < 10.0 and d < nearest_dist then
        nearest_dist = d
        nearest_en = en
      end
    end
  end
  if Entities.boss and not Entities.boss.dead then
    local d = dist2d(Player.x, Player.z, Entities.boss.x, Entities.boss.z)
    if d < 12.0 and d < nearest_dist then
      nearest_dist = d
      nearest_en = Entities.boss
    end
  end

  if not nearest_en then return end

  -- 创建箭矢
  local arrow = {
    x = Player.x,
    y = 0.5,
    z = Player.z,
    yaw = dir_to_yaw(nearest_en.x - Player.x, nearest_en.z - Player.z),
    speed = data.speed * 10,
    damage = 20 + Angel.cur * 5,
    splashEF = data.splashEF,
    timer = 0,
    e = nil,
  }
  arrow.e = dse.ecs.create_entity()
  local bs = S.base_scale("assets/models/ball.dmesh")
  dse.ecs.add_transform(arrow.e, arrow.x, arrow.y, arrow.z, 0.15 * bs, 0.15 * bs, 0.15 * bs)
  pcall(dse.ecs.mesh_renderer_add, arrow.e, "assets/models/ball.dmesh")
  dse.ecs.set_mesh_shader_variant(arrow.e, "MESH_LIT")
  local colors = {
    [0]={1,0.3,0.3}, {0.3,0.6,1}, {1,1,0.3}, {0.3,0.8,0.3},
    {0.6,0.8,0.3}, {1,1,0.6}, {0.3,0.3,0.3}, {1,0.8,0.3}
  }
  local c = colors[data.splashEF] or {1,1,1}
  dse.ecs.set_mesh_color(arrow.e, c[1], c[2], c[3], 1.0)

  table.insert(Angel.arrows, arrow)
  if PetSystem.on_angel_fire then PetSystem.on_angel_fire() end
end

-- 天使更新
function Angel.Update(dt)
  if Angel.cur == 0 then return end
  local data = DB_angel[Angel.cur - 1]
  if not data then return end
  Angel.fire_timer = Angel.fire_timer + dt
  if Angel.fire_timer >= data.firerate then
    Angel.fire_timer = 0
    angel_fire()
  end
  update_angel_arrows(dt)
end

-- 天使清理
function Angel.Clear()
  for i = #Angel.arrows, 1, -1 do
    if Angel.arrows[i].e then kill_entity(Angel.arrows[i].e) end
    table.remove(Angel.arrows, i)
  end
  Angel.fire_timer = 0
end

-- 设置当前天使
function Angel.Set(idx)
  Angel.cur = idx
  PetState.cur_angel = idx
end

-- ============================================================================
-- Ride System: 骑乘关卡系统
-- 移植自: Cha_Control_ride_cha / Cha_Control_ride_horse / Cam_Move_ride
--         AI_Ride_Enemy / AI_Ride_Enemy2 / Bullet_arrow_ride
-- ============================================================================
local Ride = {
  -- 是否在骑乘关卡
  active = false,
  -- 骑乘角色状态
  cha = {
    x = 0, z = -0.5, y = 0, yaw = 0,
    isfinish = false,
    attackdelay = 0,
    particledelay = 0,
    falldowndelay = 0,
    amount_soulstone = 0,
    count_coin = 0,
    count_monster = 0,
    count_behit = 0,
    -- 导航条位置
    gauge_navi_x = 0,
  },
  -- 战马状态
  horse = {
    x = 0, z = -0.5, y = 0, yaw = 0,
    isintro = true,
    startdelay = 0,
    finish = false,
    changeScene = 0,
    movespeed = 0,
    dubbleclick = 0,
    dragSumposY = 0,
    monmovestat = 0,  -- 0=idle, 1=run, 2=jump, 3=cry, 4=action
    action_delay = 0,
    keydown = false,
    pickPoint_x = 0, pickPoint_z = 0,
    prevPoint_x = 0, prevPoint_y = 0,
  },
  -- 摄像机
  cam = {
    camPos_x = 0, camPos_y = 1.3, camPos_z = -0.8,
    dx = 1,
    fov = 38,
  },
  -- 敌人列表
  enemies = {},
  -- 远程敌人列表
  enemies2 = {},
  -- 箭矢列表
  arrows = {},
  -- 挥砍特效
  swings = {},
  -- 步骤雾纹理索引
  stepfog_kind = 1,
}

-- 骑乘关卡初始化
function Ride.Init()
  Ride.active = true
  Ride.cha.x = 0
  Ride.cha.z = -0.5
  Ride.cha.yaw = 0
  Ride.cha.isfinish = false
  Ride.cha.attackdelay = 0
  Ride.cha.particledelay = 0
  Ride.cha.falldowndelay = 0
  Ride.cha.amount_soulstone = 0
  Ride.cha.count_coin = 0
  Ride.cha.count_monster = 0
  Ride.cha.count_behit = 0
  Ride.cha.gauge_navi_x = 0

  Ride.horse.x = 0
  Ride.horse.z = -0.5
  Ride.horse.yaw = 0
  Ride.horse.isintro = true
  Ride.horse.startdelay = 0
  Ride.horse.finish = false
  Ride.horse.changeScene = 0
  Ride.horse.movespeed = 0
  Ride.horse.dubbleclick = 0
  Ride.horse.dragSumposY = 0
  Ride.horse.monmovestat = 0
  Ride.horse.action_delay = 0
  Ride.horse.keydown = false

  Ride.cam.camPos_x = 0
  Ride.cam.camPos_y = 1.3
  Ride.cam.camPos_z = -0.8
  Ride.cam.dx = 1
  Ride.cam.fov = 38

  Ride.enemies = {}
  Ride.enemies2 = {}
  Ride.arrows = {}
  Ride.swings = {}
end

-- 骑乘角色攻击 (C# Cha_Control_ride_cha.Attack)
function Ride.Attack(is_right)
  if Ride.cha.attackdelay <= 0 and Ride.cha.y < 0.01 then
    -- 创建挥砍特效
    local swing = {
      x = Ride.cha.x,
      y = 0.1,
      z = Ride.cha.z,
      yaw = Ride.cha.yaw,
      delay = 0.16,
      efon = true,
      timer = 0,
      index = 0,
      lastframe = 4,
      fps = 18,
      impactframe = 1,
      is_right = is_right,
      e = nil,
    }
    if is_right then
      swing.yaw = Ride.cha.yaw + 60
    else
      swing.yaw = Ride.cha.yaw - 60
    end
    -- 创建 ECS 实体
    swing.e = dse.ecs.create_entity()
    local bs = S.base_scale("assets/models/ball.dmesh")
    dse.ecs.add_transform(swing.e, swing.x, swing.y, swing.z, bs, bs, bs)
    pcall(dse.ecs.mesh_renderer_add, swing.e, "assets/models/ball.dmesh")
    dse.ecs.set_mesh_shader_variant(swing.e, "MESH_LIT")
    dse.ecs.set_mesh_color(swing.e, 0.9, 0.9, 0.6, 0.8)
    table.insert(Ride.swings, swing)

    Ride.cha.attackdelay = 1.0
    if PetSystem.on_ride_attack then PetSystem.on_ride_attack(is_right) end
  end
end

-- 骑乘角色受伤 (C# Cha_Control_ride_cha.Damaged)
function Ride.Damaged()
  if not Ride.cha.isfinish then
    Ride.GetSoulStone(-3)
  end
end

-- 获取魂石 (C# Cha_Control_ride_cha.GetSoulStone)
function Ride.GetSoulStone(a)
  Ride.cha.amount_soulstone = math.max(0, Ride.cha.amount_soulstone + a)
  if a == 1 then
    -- 拾取魂石
    if PetSystem.on_ride_getcoin then PetSystem.on_ride_getcoin(Ride.cha.x, Ride.cha.z) end
    Ride.cha.count_coin = Ride.cha.count_coin + 1
  elseif a < 0 then
    -- 受伤
    if PetSystem.on_ride_behit then PetSystem.on_ride_behit() end
    Ride.cha.count_behit = Ride.cha.count_behit + 1
  else
    -- 击杀怪物 (a == 3)
    Ride.cha.count_monster = Ride.cha.count_monster + 1
  end
  Ride.cha.particledelay = 0.5
end

-- 骑乘角色摔倒 (C# Cha_Control_ride_cha.FallDown)
function Ride.FallDown()
  if not Ride.cha.isfinish and Ride.cha.falldowndelay <= 0 then
    Ride.HorseFallDown()
    Ride.GetSoulStone(-3)
    Ride.cha.falldowndelay = 1.0
  end
end

-- 战马摔倒 (C# Cha_Control_ride_horse.FallDown)
function Ride.HorseFallDown()
  if not Ride.horse.finish then
    Ride.horse.monmovestat = 3  -- cry
  end
end

-- 战马跳跃 (C# Cha_Control_ride_horse.HorseJump)
function Ride.HorseJump()
  Ride.horse.monmovestat = 2  -- jump
  if PetSystem.on_ride_jump then PetSystem.on_ride_jump() end
end

-- 骑乘完成 (C# Cha_Control_ride_horse.RidingFinish)
function Ride.RidingFinish()
  Ride.horse.isintro = true
  Ride.horse.startdelay = 0
  Ride.horse.finish = true
  Ride.cha.isfinish = true
end

-- 摄像机震动 (C# Cam_Move_ride.Hitcam)
function Ride.Hitcam()
  Ride.cam.dx = -Ride.cam.dx
  if Ride.cam.fov > 23 then
    Ride.cam.fov = Ride.cam.fov - 1
  end
end

-- 生成骑乘敌人 (C# Spawn → AI_Ride_Enemy)
function Ride.SpawnEnemy(x, z)
  local en = {
    x = x, z = z, y = 0, yaw = 0,
    life = true,
    rnd_x = 0, rnd_z = 0,
    attackdelay = 0,
    timer = 0,
    -- 动画状态: 0=move, 1=down
    anim = "move",
    e = nil,
  }
  -- 随机目标位置
  en.rnd_x = rand_range(-1, 1)
  en.rnd_z = rand_range(-0.4, 0.6)

  en.e = dse.ecs.create_entity()
  local bs = S.base_scale("assets/models/mon_0.dmesh")
  dse.ecs.add_transform(en.e, en.x, en.y, en.z, 0.8 * bs, 0.8 * bs, 0.8 * bs)
  pcall(dse.ecs.mesh_renderer_add, en.e, "assets/models/mon_0.dmesh")
  dse.ecs.set_mesh_shader_variant(en.e, "MESH_LIT")
  dse.ecs.set_mesh_color(en.e, 0.8, 0.4, 0.3, 1.0)

  table.insert(Ride.enemies, en)
  return en
end

-- 生成骑乘远程敌人 (C# AI_Ride_Enemy2)
function Ride.SpawnEnemy2(x, z)
  local en = {
    x = x, z = z, y = 0, yaw = 0,
    live = true,
    impact = false,
    rot_limit = 0,
    rot_speed = 4,
    attack_timer = rand_range(4, 6),
    e = nil,
    bullet_e = nil,
  }
  en.e = dse.ecs.create_entity()
  local bs = S.base_scale("assets/models/mon_1.dmesh")
  dse.ecs.add_transform(en.e, en.x, en.y, en.z, bs, bs, bs)
  pcall(dse.ecs.mesh_renderer_add, en.e, "assets/models/mon_1.dmesh")
  dse.ecs.set_mesh_shader_variant(en.e, "MESH_LIT")
  dse.ecs.set_mesh_color(en.e, 0.6, 0.5, 0.3, 1.0)

  table.insert(Ride.enemies2, en)
  return en
end

-- 骑乘敌人更新 (C# AI_Ride_Enemy.Update)
local function update_ride_enemy(en, dt)
  if en.life then
    -- 移动到随机位置
    en.x = lerp(en.x, en.rnd_x, dt * 0.5)
    en.z = lerp(en.z, en.rnd_z, dt * 0.5)
  else
    -- 倒下后向后移动
    en.z = en.z - dt * 1.0
    if en.z < -1.0 then
      if en.e then kill_entity(en.e); en.e = nil end
      en.active = false
    end
  end

  -- 攻击检测
  local attackrangeZ = en.z - Ride.cha.z
  if en.attackdelay > 0 then
    en.attackdelay = en.attackdelay - dt
  else
    if math.abs(attackrangeZ) < 0.15 then
      local attackrangeX = en.x - Ride.cha.x
      if math.abs(attackrangeX) < 0.25 then
        if attackrangeX > 0 then
          Ride.Attack(true)
        else
          Ride.Attack(false)
        end
        en.attackdelay = 0.2
      end
    end
  end

  -- 随机位置更新 (C# InvokeRepeating SetRndPosition)
  en.timer = en.timer + dt
  if en.timer >= rand_range(1, 2) then
    en.timer = 0
    en.rnd_x = rand_range(-1, 1)
    en.rnd_z = rand_range(-0.4, 0.6)
  end

  -- 碰撞检测: 玩家武器击中敌人
  if en.life then
    for _, sw in ipairs(Ride.swings) do
      if sw.index == sw.impactframe or sw.index == sw.impactframe + 1 then
        if dist2d(en.x, en.z, Ride.cha.x, Ride.cha.z) < 0.5 then
          en.life = false
          en.anim = "down"
          Ride.Hitcam()
          Ride.GetSoulStone(3)
          if PetSystem.on_ride_kill_enemy then PetSystem.on_ride_kill_enemy(en.x, en.z) end
          break
        end
      end
    end
  end

  -- 同步 ECS
  if en.e then
    dse.ecs.set_transform_position(en.e, en.x, en.y, en.z)
  end
end

-- 骑乘远程敌人更新 (C# AI_Ride_Enemy2.Update)
local function update_ride_enemy2(en, dt)
  if not en.live then return end

  -- 攻击计时
  en.attack_timer = en.attack_timer - dt
  if en.attack_timer <= 0 then
    en.attack_timer = rand_range(3, 5)
    -- 射击: 创建箭矢
    local arrow = {
      x = en.x,
      y = 0.08,
      z = en.z,
      yaw = en.yaw,
      speed = 3.0,
      timer = 0,
      e = nil,
    }
    arrow.e = dse.ecs.create_entity()
    local bs = S.base_scale("assets/models/ball.dmesh")
    dse.ecs.add_transform(arrow.e, arrow.x, arrow.y, arrow.z, 0.1 * bs, 0.1 * bs, 0.1 * bs)
    pcall(dse.ecs.mesh_renderer_add, arrow.e, "assets/models/ball.dmesh")
    dse.ecs.set_mesh_shader_variant(arrow.e, "MESH_LIT")
    dse.ecs.set_mesh_color(arrow.e, 0.6, 0.3, 0.1, 1.0)
    table.insert(Ride.arrows, arrow)
    en.impact = true
  end

  -- 朝向玩家
  local dx = Ride.cha.x - en.x
  local dz = Ride.cha.z - en.z
  local target_yaw = dir_to_yaw(dx, dz)

  -- 限制转向 (不能朝后方)
  local target_dir_x, target_dir_z = yaw_to_dir(target_yaw)
  if target_dir_z > -1.0 then
    en.yaw = lerp_angle(en.yaw, target_yaw, dt * en.rot_speed)
  else
    en.yaw = lerp_angle(en.yaw, 0, dt * 6)
  end

  -- 同步 ECS
  if en.e then
    dse.ecs.set_transform_position(en.e, en.x, en.y, en.z)
    dse.ecs.set_transform_rotation(en.e, 0, en.yaw, 0)
  end
end

-- 箭矢更新 (C# Bullet_arrow_ride.Update)
local function update_ride_arrow(arrow, dt)
  arrow.timer = arrow.timer + dt
  local fx, fz = yaw_to_dir(arrow.yaw)
  arrow.x = arrow.x + fx * dt * arrow.speed
  arrow.z = arrow.z + fz * dt * arrow.speed

  -- 碰撞检测
  if dist2d(arrow.x, arrow.z, Ride.cha.x, Ride.cha.z) < 0.3 then
    Ride.Damaged()
    arrow.active = false
  end

  -- 过期
  if arrow.timer > 5.0 then
    arrow.active = false
  end

  -- 同步 ECS
  if arrow.e then
    dse.ecs.set_transform_position(arrow.e, arrow.x, arrow.y, arrow.z)
  end
end

-- 挥砍特效更新 (C# Ef_swing1_ride.Update)
local function update_ride_swing(sw, dt)
  if sw.efon then
    if sw.delay > 0 then
      sw.delay = sw.delay - dt
    else
      sw.efon = false
      sw.timer = 0
    end
  end

  if not sw.efon then
    sw.timer = sw.timer + dt
    sw.index = math.floor(sw.timer * sw.fps)

    if sw.index >= sw.lastframe then
      -- 特效结束
      sw.active = false
    elseif sw.index == sw.impactframe or sw.index == sw.impactframe + 1 then
      -- 碰撞激活
      sw.collision_on = true
    else
      sw.collision_on = false
    end

    -- UV 动画 (通过缩放/颜色模拟)
    local t = sw.index / sw.lastframe
    if sw.e then
      dse.ecs.set_mesh_color(sw.e, 0.9, 0.9, 0.6, 0.8 * (1 - t))
      dse.ecs.set_transform_scale(sw.e, 1.0 + t * 0.5, 1.0 + t * 0.5, 1.0 + t * 0.5)
    end
  end

  if sw.e then
    dse.ecs.set_transform_position(sw.e, sw.x, sw.y, sw.z)
    dse.ecs.set_transform_rotation(sw.e, 0, sw.yaw, 0)
  end
end

-- 骑乘角色更新 (C# Cha_Control_ride_cha.Update + Cha_Control_ride_horse.Update)
function Ride.Update(dt)
  if not Ride.active then return end

  -- 战马控制 (C# Cha_Control_ride_horse.Update)
  if Ride.horse.isintro then
    if Ride.horse.finish then
      -- 完成流程
      if Ride.horse.changeScene == 0 then
        if Ride.horse.z > 2.0 then
          Ride.horse.changeScene = 1
          if PetSystem.on_ride_finish then PetSystem.on_ride_finish() end
        else
          Ride.horse.z = Ride.horse.z + dt
          Ride.horse.yaw = lerp_angle(Ride.horse.yaw, 0, dt * 5)
        end
      end
    else
      if Ride.horse.startdelay > 1.0 then
        Ride.horse.isintro = false
      elseif Ride.horse.z < 0 then
        Ride.horse.z = Ride.horse.z + 0.4 * dt
      end
      Ride.horse.startdelay = Ride.horse.startdelay + dt
    end
  else
    -- 正常骑乘控制
    -- 位置边界检查
    if math.abs(Ride.horse.x) > 0.7 or Ride.horse.z > 1.0 or Ride.horse.z < -0.4 then
      Ride.horse.x = lerp(Ride.horse.x, 0, dt)
      Ride.horse.z = lerp(Ride.horse.z, 0, dt)
      Ride.horse.pickPoint_x = 0
      Ride.horse.pickPoint_z = 0
    else
      -- 输入处理由 main.lua 注入
      -- 移动到目标点
      Ride.horse.x = Ride.horse.x + (Ride.horse.pickPoint_x - Ride.horse.x) * dt * Ride.horse.movespeed
      Ride.horse.z = Ride.horse.z + (Ride.horse.pickPoint_z - Ride.horse.z) * dt * Ride.horse.movespeed

      -- 旋转倾斜
      local dx = Ride.horse.pickPoint_x - Ride.horse.x
      if dx > 0.01 then
        Ride.horse.yaw = lerp_angle(Ride.horse.yaw, 30, dt * 3)  -- right lean
      elseif dx < -0.01 then
        Ride.horse.yaw = lerp_angle(Ride.horse.yaw, -30, dt * 3)  -- left lean
      else
        Ride.horse.yaw = lerp_angle(Ride.horse.yaw, 0, dt * 3)
      end
    end

    -- 双击检测
    Ride.horse.dubbleclick = Ride.horse.dubbleclick + 2.0 * dt
    Ride.horse.dragSumposY = 0

    -- 速度衰减
    if Ride.horse.movespeed > 0 then
      Ride.horse.movespeed = Ride.horse.movespeed - dt
    else
      Ride.horse.movespeed = 0
    end

    -- 动画状态机
    if Ride.horse.monmovestat == 3 then
      -- cry (受击)
      Ride.horse.action_delay = 0.5
    elseif Ride.horse.action_delay > 0 then
      Ride.horse.action_delay = Ride.horse.action_delay - dt
      Ride.horse.monmovestat = 4
    elseif Ride.horse.monmovestat == 2 then
      -- jump
      Ride.horse.monmovestat = 2
    elseif Ride.horse.movespeed > 0 and Ride.horse.monmovestat ~= 1 then
      Ride.horse.monmovestat = 1  -- run
    end
  end

  -- 角色跟随战马 (C# Cha_Control_ride_cha: mytransform.position = horseSpine.position + Vector3.up * -0.085f)
  Ride.cha.x = Ride.horse.x
  Ride.cha.z = Ride.horse.z
  Ride.cha.yaw = Ride.horse.yaw

  -- 导航条前进 (C# Cha_Control_ride_cha: gauge_navi.position += Vector3.right * Time.deltaTime * 0.033f)
  if not Ride.cha.isfinish then
    Ride.cha.gauge_navi_x = Ride.cha.gauge_navi_x + dt * 0.033
  end

  -- 计时器
  if Ride.cha.falldowndelay > 0 then
    Ride.cha.falldowndelay = Ride.cha.falldowndelay - dt
  elseif Ride.cha.attackdelay > 0 then
    Ride.cha.attackdelay = Ride.cha.attackdelay - dt
  end
  if Ride.cha.particledelay > 0 then
    Ride.cha.particledelay = Ride.cha.particledelay - dt
  end

  -- 摄像机更新 (C# Cam_Move_ride.Update)
  Ride.cam.camPos_x = math.sin(G.time * 0.5) * 0.8
  if Ride.cam.fov < 38 then
    Ride.cam.fov = lerp(Ride.cam.fov, 38, dt * 3)
  end

  -- 更新敌人
  for i = #Ride.enemies, 1, -1 do
    local en = Ride.enemies[i]
    if en.active ~= false then
      update_ride_enemy(en, dt)
    else
      if en.e then kill_entity(en.e); en.e = nil end
      table.remove(Ride.enemies, i)
    end
  end

  -- 更新远程敌人
  for _, en in ipairs(Ride.enemies2) do
    update_ride_enemy2(en, dt)
  end

  -- 更新箭矢
  for i = #Ride.arrows, 1, -1 do
    local a = Ride.arrows[i]
    update_ride_arrow(a, dt)
    if a.active == false then
      if a.e then kill_entity(a.e); a.e = nil end
      table.remove(Ride.arrows, i)
    end
  end

  -- 更新挥砍特效
  for i = #Ride.swings, 1, -1 do
    local sw = Ride.swings[i]
    update_ride_swing(sw, dt)
    if sw.active == false then
      if sw.e then kill_entity(sw.e); sw.e = nil end
      table.remove(Ride.swings, i)
    end
  end
end

-- 骑乘输入处理 (C# Cha_Control_ride_horse.Update 输入部分)
function Ride.HandleInput(input_x, input_y, input_down, input_up, screen_w, screen_h)
  if not Ride.active or Ride.horse.isintro or Ride.horse.finish then return end

  if input_down then
    Ride.horse.keydown = true
    Ride.horse.prevPoint_x = input_x
    Ride.horse.prevPoint_y = input_y
  elseif input_up then
    Ride.horse.keydown = false
    Ride.horse.dubbleclick = 0
  elseif Ride.horse.keydown then
    -- 拖动检测
    local dragY = (input_y - Ride.horse.prevPoint_y)
    if dragY > 0.001 * screen_h then
      Ride.horse.dragSumposY = Ride.horse.dragSumposY + dragY
    else
      Ride.horse.dragSumposY = 0
    end
    Ride.horse.prevPoint_x = input_x
    Ride.horse.prevPoint_y = input_y

    -- 上滑跳跃
    if Ride.horse.dragSumposY > 0.003 * screen_h and Ride.horse.monmovestat == 1 then
      Ride.HorseJump()
      Ride.horse.dragSumposY = 0
    elseif Ride.horse.dubbleclick < 0.25 and Ride.horse.monmovestat == 1 then
      -- 双击跳跃
      local dist = math.abs(input_x - Ride.horse.prevPoint_x) + math.abs(input_y - Ride.horse.prevPoint_y)
      if dist < 50 then
        Ride.HorseJump()
      end
    end

    -- 设置目标点 (射线检测模拟)
    Ride.horse.pickPoint_x = clamp((input_x / screen_w - 0.5) * 1.4, -0.7, 0.7)
    Ride.horse.pickPoint_z = clamp((input_y / screen_h - 0.5) * 1.4, -0.4, 1.0)
    Ride.horse.movespeed = 0.5
  end
end

-- 骑乘清理
function Ride.Clear()
  Ride.active = false
  for _, en in ipairs(Ride.enemies) do
    if en.e then kill_entity(en.e); en.e = nil end
  end
  for _, en in ipairs(Ride.enemies2) do
    if en.e then kill_entity(en.e); en.e = nil end
  end
  for _, a in ipairs(Ride.arrows) do
    if a.e then kill_entity(a.e); a.e = nil end
  end
  for _, sw in ipairs(Ride.swings) do
    if sw.e then kill_entity(sw.e); sw.e = nil end
  end
  Ride.enemies = {}
  Ride.enemies2 = {}
  Ride.arrows = {}
  Ride.swings = {}
end

-- ============================================================================
-- Pet Skill: 宠物技能系统 (C# Cha_Skill.PetSkillOn + Cha_Control.Fly/CallHorse)
-- ============================================================================

-- 设置宠物技能等级 (C# Cha_Control.SetPetSkillLV)
local function set_pet_skill_lv(autorate, attack_horse, attack_eagle)
  Player._pet_autorate = autorate
  Player._pet_attack_horse = attack_horse
  Player._pet_attack_eagle = attack_eagle
end

-- 更新宠物技能数据 (从 PetState 读取)
local function update_pet_skill_data()
  local ps0 = DB_PetSkill[0][PetState.pet_activeskill[1] or 0] or DB_PetSkill[0][0]
  local ps1 = DB_PetSkill[1][PetState.pet_activeskill[2] or 0] or DB_PetSkill[1][0]
  local autorate = DB_PetSkill[1][PetState.pet_passiveskill[2] or 0]
    and DB_PetSkill[1][PetState.pet_passiveskill[2] or 0].passive or 0
  set_pet_skill_lv(autorate, ps0.attackpoint, ps1.attackpoint)
end

-- 宠物技能激活 (C# Cha_Skill.PetSkillOn)
local function pet_skill_on(index)
  -- 旋转雾气特效 + 摄像机缩放
  if PetSystem.on_pet_skill_start then PetSystem.on_pet_skill_start() end

  if index == 0 then
    -- 召唤战马 (C# Cha_Control.CallHorse)
    Player.pet_ing = true
    Player.currentPet = 0
    Player.movespeed = 0.8
    -- 攻击力提升
    local atk_boost = (Player._pet_attack_horse or 100) * 0.01
    if PetSystem.on_attack_up then PetSystem.on_attack_up(atk_boost) end
    -- 持续时间
    local dur = DB_PetSkill[0][PetState.pet_activeskill[1] or 0]
      and DB_PetSkill[0][PetState.pet_activeskill[1] or 0].duration or 8.0
    Player._pet_timer = dur
    Player._pet_atk = Player._pet_attack_horse or 100
    -- 激活战马实体
    Horse.SkillOn()
    -- 音效
    if PetSystem.on_play_audio then PetSystem.on_play_audio("horse") end

  elseif index == 1 then
    -- 召唤猎鹰 (C# Cha_Control.Fly)
    Player.pet_ing = true
    Player.currentPet = 1
    Player.movespeed = 0.3
    -- 攻击力提升
    local atk_boost = (Player._pet_attack_eagle or 40) * 0.01
    if PetSystem.on_attack_up then PetSystem.on_attack_up(atk_boost) end
    -- 持续时间
    local dur = DB_PetSkill[1][PetState.pet_activeskill[2] or 0]
      and DB_PetSkill[1][PetState.pet_activeskill[2] or 0].duration or 7.0
    Player._pet_timer = dur
    Player._pet_atk = Player._pet_attack_eagle or 40
    -- 激活猎鹰实体
    Eagle.OnEnable()
    Eagle.attackpoint = Player._pet_atk
    -- 音效
    if PetSystem.on_play_audio then PetSystem.on_play_audio("skillstart") end
  end

  -- 消耗饥饿值 (C# Icon_Skill.Duration_reduce)
  if PetState.pet_hunger[index + 1] and PetState.pet_hunger[index + 1] > 0 then
    PetState.pet_hunger[index + 1] = PetState.pet_hunger[index + 1] - 1
    PetState.ispetready[index + 1] = false
    PetState.pet_skill_use[index + 1] = PetState.pet_skill_use[index + 1] + 1
  end
end

-- 宠物技能结束 (C# Cha_Control.PetSkillFinish)
local function pet_skill_finish(index)
  Player.chamovestat = 180
  if index == 1 then
    -- 猎鹰结束
    Eagle.SkillOff()
    Eagle.OnDisable()
  elseif index == 0 then
    -- 战马结束
    Horse.GetOffHorse()
  end
  Player.pet_ing = false
  Player.currentPet = -1
  Player.movespeed = 0.48
  -- 重置攻击力
  if PetSystem.on_reset_atk then PetSystem.on_reset_atk() end
end

-- ============================================================================
-- 宠物管理 UI 数据接口 (C# UI_pet.cs 数据部分)
-- ============================================================================

-- 喂养宠物 (C# UI_pet confirm case 1/2)
local function feed_pet(pet_idx, feed_all)
  local cost_feed = 20
  local current_hunger = PetState.pet_hunger[pet_idx + 1] or 0
  local target_hunger = feed_all and 5 or math.min(5, current_hunger + 1)
  local cost = cost_feed * (target_hunger - current_hunger)

  if G.coin >= cost then
    G.coin = G.coin - cost
    PetState.pet_hunger[pet_idx + 1] = target_hunger
    -- 更新可用状态
    for i = 1, 2 do
      if PetState.pet_hunger[i] > 0 then
        PetState.ispetready[i] = true
      end
    end
    return true, cost
  end
  return false, cost
end

-- 升级被动技能 (C# UI_pet confirm case 3)
local function upgrade_passive(pet_idx)
  local current_lv = PetState.pet_passiveskill[pet_idx + 1] or 0
  if current_lv >= 9 then return false, "max_level" end
  local data = DB_PetSkill[pet_idx][current_lv]
  if not data then return false, "no_data" end
  local price = data.price
  if G.jade >= price then
    G.jade = G.jade - price
    PetState.pet_passiveskill[pet_idx + 1] = current_lv + 1
    update_pet_skill_data()
    return true, price
  end
  return false, "insufficient_jade"
end

-- 升级主动技能 (C# UI_pet confirm case 4)
local function upgrade_active(pet_idx)
  local current_lv = PetState.pet_activeskill[pet_idx + 1] or 0
  if current_lv >= 9 then return false, "max_level" end
  local data = DB_PetSkill[pet_idx][current_lv]
  if not data then return false, "no_data" end
  local price = data.price
  if G.jade >= price then
    G.jade = G.jade - price
    PetState.pet_activeskill[pet_idx + 1] = current_lv + 1
    update_pet_skill_data()
    return true, price
  end
  return false, "insufficient_jade"
end

-- 装备/卸下天使 (C# UI_pet menu_kind==2)
local function set_angel(idx)
  Angel.Set(idx)
end

-- 获取宠物信息 (C# UI_pet 显示数据)
local function get_pet_info(pet_idx)
  local active_lv = PetState.pet_activeskill[pet_idx + 1] or 0
  local passive_lv = PetState.pet_passiveskill[pet_idx + 1] or 0
  local hunger = PetState.pet_hunger[pet_idx + 1] or 0
  local active_data = DB_PetSkill[pet_idx][active_lv]
  local passive_data = DB_PetSkill[pet_idx][passive_lv]
  local next_active = DB_PetSkill[pet_idx][active_lv + 1]
  local next_passive = DB_PetSkill[pet_idx][passive_lv + 1]
  return {
    active_lv = active_lv,
    passive_lv = passive_lv,
    hunger = hunger,
    active_atk = active_data and active_data.attackpoint or 0,
    active_dur = active_data and active_data.duration or 0,
    passive_val = passive_data and passive_data.passive or 0,
    next_active_atk = next_active and next_active.attackpoint or nil,
    next_active_dur = next_active and next_active.duration or nil,
    next_passive_val = next_passive and next_passive.passive or nil,
    active_price = active_data and active_data.price or 0,
    passive_price = passive_data and passive_data.price or 0,
    ispetready = PetState.ispetready[pet_idx + 1] or false,
  }
end

-- 获取天使信息
local function get_angel_info(idx)
  local data = DB_angel[idx]
  if not data then return nil end
  return {
    name = AngelNames[data.name] or "???",
    firerate = 10 - data.firerate,
    speed = data.speed,
    arrowkind = data.arrowkind,
    splashEF = data.splashEF,
    unlocked = PetState.max_extreme_stage > idx,
    equipped = PetState.cur_angel == idx + 1,
  }
end

-- ============================================================================
-- 骑乘结算 (C# UI_result_ride.cs)
-- ============================================================================
local RideResult = {
  show_delay = 0,
  pos_x = 1000,
  show_ui = false,
  getpoint = false,
  movefinish = false,
  coin = 0,
  jade = 0,
  getcoin = 0,
  getcoin_og = 0,
  getcoin_f = 0,
  count_coin = 0,
  count_monster = 0,
  count_behit = 0,
  count_loss = 0,
  gonext = false,
}

function RideResult.Init()
  RideResult.show_delay = 0
  RideResult.pos_x = 1000
  RideResult.show_ui = false
  RideResult.getpoint = false
  RideResult.movefinish = false
  RideResult.getcoin_f = 0
  RideResult.gonext = false

  RideResult.coin = G.coin
  RideResult.jade = G.jade
  RideResult.getcoin = Ride.cha.amount_soulstone
  RideResult.getcoin_og = RideResult.getcoin
  RideResult.count_monster = Ride.cha.count_monster
  RideResult.count_coin = Ride.cha.count_coin
  RideResult.count_behit = Ride.cha.count_behit
  RideResult.count_loss = math.max(0, RideResult.count_monster * 3 + RideResult.count_coin - RideResult.getcoin)

  -- 被动技能加成 (C# getcoin += pet_passiveskill[0] * 0.1)
  local passive_lv = PetState.pet_passiveskill[1] or 0
  RideResult.getcoin = RideResult.getcoin + math.floor(RideResult.getcoin * passive_lv * 0.1)
end

function RideResult.Update(dt)
  if RideResult.getpoint then
    RideResult.getcoin_f = RideResult.getcoin_f + dt * 20
    if RideResult.getcoin_f >= RideResult.getcoin then
      RideResult.getcoin_f = RideResult.getcoin
      RideResult.getpoint = false
      G.coin = G.coin + RideResult.getcoin
      RideResult.gonext = true
    end
  elseif not RideResult.show_ui then
    RideResult.show_delay = RideResult.show_delay + dt
    if RideResult.show_delay > 1.0 then
      RideResult.getpoint = true
      RideResult.show_ui = true
    end
  end
  if not RideResult.movefinish then
    RideResult.pos_x = math.min(RideResult.pos_x + dt * 1200, 270)
    if RideResult.pos_x >= 270 then
      RideResult.movefinish = true
    end
  end
end

-- ============================================================================
-- 主更新函数
-- ============================================================================
local function update(dt)
  -- 宠物计时
  if Player._pet_timer and Player._pet_timer > 0 then
    Player._pet_timer = Player._pet_timer - dt
    if Player._pet_timer <= 0 then
      -- 宠物结束
      if Player.currentPet == 1 then
        pet_skill_finish(1)
      elseif Player.currentPet == 0 then
        pet_skill_finish(0)
      end
      Player.pet_ing = false
      Player.currentPet = -1
      Player.movespeed = 0.48
    end
  end

  -- 猎鹰更新
  Eagle.Update(dt)
  -- 战马更新
  Horse.Update(dt)
  -- 天使更新
  Angel.Update(dt)
  -- 骑乘模式更新
  Ride.Update(dt)
  -- 骑乘结算更新
  if RideResult.show_ui or RideResult.show_delay > 0 then
    RideResult.Update(dt)
  end
end

-- 清理所有宠物实体
local function clear()
  Eagle.OnDisable()
  Horse.OnDisable()
  Angel.Clear()
  Ride.Clear()
  RideResult.show_ui = false
  RideResult.show_delay = 0
  RideResult.getpoint = false
  RideResult.gonext = false
end

-- 初始化
local function init()
  update_pet_skill_data()
  Angel.cur = PetState.cur_angel
end

-- ============================================================================
-- 模块导出
-- ============================================================================
local PetSystem = {
  -- 数据库
  DB_PetSkill    = DB_PetSkill,
  DB_angel       = DB_angel,
  AngelNames     = AngelNames,
  -- 状态
  PetState       = PetState,
  -- 子系统
  Eagle          = Eagle,
  Horse          = Horse,
  Angel          = Angel,
  Ride           = Ride,
  RideResult     = RideResult,
  -- 技能
  pet_skill_on       = pet_skill_on,
  pet_skill_finish   = pet_skill_finish,
  set_pet_skill_lv   = set_pet_skill_lv,
  update_pet_skill_data = update_pet_skill_data,
  -- 管理
  feed_pet       = feed_pet,
  upgrade_passive= upgrade_passive,
  upgrade_active = upgrade_active,
  set_angel      = set_angel,
  get_pet_info   = get_pet_info,
  get_angel_info = get_angel_info,
  -- 生命周期
  init           = init,
  update         = update,
  clear          = clear,
  -- 回调 (main.lua 注入)
  on_eagle_attack    = nil,
  on_ride_attack     = nil,
  on_ride_kill_enemy = nil,
  on_ride_getcoin    = nil,
  on_ride_behit      = nil,
  on_ride_jump       = nil,
  on_ride_finish     = nil,
  on_horse_ride      = nil,
  on_angel_hit       = nil,
  on_angel_fire      = nil,
  on_pet_skill_start = nil,
  on_attack_up       = nil,
  on_reset_atk       = nil,
  on_play_audio      = nil,
}

_G.PetSystem = PetSystem
return PetSystem
