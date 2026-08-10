-- ============================================================================
-- bullet_system.lua — 完整子弹/投射物系统
-- 1:1 移植自 31 个 Bullet_*.cs 文件
-- 依赖: state.lua (G, Player, Entities, 工具函数)
-- ============================================================================

local S = require "scripts.state"
local G, Player, Entities = S.G, S.Player, S.Entities
local clamp, dist2d, lerp, atan2 = S.clamp, S.dist2d, S.lerp, S.atan2
local kill_entity = S.kill_entity

-- ── 内部工具 ─────────────────────────────────────────────────────────
local function yaw_to_dir(yaw)
  -- yaw 度 → forward 方向向量 (x, z)
  local rad = math.rad(yaw)
  return math.sin(rad), -math.cos(rad)
end

local function dir_to_yaw(dx, dz)
  if dx == 0 and dz == 0 then return 0 end
  return math.deg(atan2(dx, -dz))
end

local function rand_range(a, b) return a + math.random() * (b - a) end

-- ── 子弹对象池 ────────────────────────────────────────────────────────
-- 每颗子弹是一个 table, 复用而非频繁 GC
local pool = {}
local active = {}

-- 从池中获取一个子弹 table
local function obtain()
  local b = table.remove(pool)
  if not b then b = {} end
  return b
end

-- 回收到池
local function release(b)
  if b.e then kill_entity(b.e); b.e = nil end
  for k in pairs(b) do if k ~= "e" then b[k] = nil end end
  table.insert(pool, b)
end

-- ============================================================================
-- 创建子弹 — 通用入口
-- @param cfg  table, 必须包含 type 字段
-- ============================================================================
local function spawn(cfg)
  local b = obtain()
  -- 通用字段
  b.type     = cfg.type or "arrow"
  b.x        = cfg.x or 0
  b.y        = cfg.y or 0.2
  b.z        = cfg.z or 0
  b.yaw      = cfg.yaw or 0
  b.dx       = cfg.dx or 0       -- 速度 x 分量
  b.dz       = cfg.dz or 0       -- 速度 z 分量
  b.dy       = cfg.dy or 0
  b.scale_x  = cfg.sx or 1
  b.scale_y  = cfg.sy or 1
  b.scale_z  = cfg.sz or 1
  b.origin_sx = b.scale_x
  b.origin_sy = b.scale_y
  b.origin_sz = b.scale_z
  b.damage   = cfg.damage or 0
  b.attack_type = cfg.attack_type or "arrow"
  b.life     = cfg.life or 3.0
  b.timer    = 0
  b.active   = true
  b.e        = nil               -- ECS 实体 (延迟创建)
  b.color_r  = cfg.r or 1
  b.color_g  = cfg.g or 1
  b.color_b  = cfg.b or 1
  b.color_a  = cfg.a or 1
  b.model    = cfg.model or "assets/models/ball.dmesh"

  -- 类型特定字段 (按需)
  b.speed       = cfg.speed or 0        -- 直线速度
  b.homing_rate = cfg.homing_rate or 5  -- 追踪旋转速率
  b.homing_spd  = cfg.homing_spd or 5   -- 追踪移动速度
  b.close_time  = cfg.close_time or 3   -- 追踪关闭时间
  b.show_delay  = cfg.show_delay or 0   -- 延迟显示
  b.disable_delay = cfg.disable_delay or 1.5 -- 延迟消失
  b.active_delay  = cfg.active_delay or 0.5  -- 碰撞激活延迟
  b.start_delay   = cfg.start_delay or 0    -- 粒子启动延迟
  b.expand_factor = cfg.expand_factor or 0  -- 膨胀系数
  b.distance   = cfg.distance or 0     -- 前进距离 (explode)
  b.rnddir     = cfg.rnddir or 1       -- 随机方向 (tornado)
  b.side       = cfg.side or 1         -- 侧向 (angel4_m)
  b.parent_enemy = cfg.parent_enemy    -- 父敌人 (tornado/wheelwind)
  b.sub_count  = cfg.sub_count or 0    -- 分裂箭数
  b.sub_angle  = cfg.sub_angle or 15   -- 分裂角度
  b.target_x   = cfg.target_x          -- 目标 x
  b.target_z   = cfg.target_z          -- 目标 z
  b.grow_vec_x = cfg.grow_x or 0       -- 生长向量
  b.grow_vec_y = cfg.grow_y or 0
  b.grow_vec_z = cfg.grow_z or 0
  b.grow_rate  = cfg.grow_rate or 6    -- 生长速率
  b.tune       = cfg.tune or 0         -- 位置微调
  b.collide_enemies = cfg.collide_enemies ~= false  -- 是否检测敌人碰撞
  b.collide_player  = cfg.collide_player or false    -- 是否检测玩家碰撞
  b.emit_on   = false                   -- 粒子发射状态
  b.render_on = true                    -- 渲染状态
  b.collision_on = false                -- 碰撞状态
  b.phase     = 0                       -- 通用阶段标志
  b.sintime   = 0                       -- 正弦时间
  b.impact    = false                   -- 冲击标志

  table.insert(active, b)
  return b
end

-- ============================================================================
-- 创建子弹的 ECS 实体 (延迟创建, 第一次 update 时)
-- ============================================================================
local function ensure_entity(b)
  if b.e then return end
  b.e = dse.ecs.create_entity()
  -- 模型基准缩放烘焙进 scale (model_scale.lua), 仅创建时乘一次, 后续 sync 沿用
  local bs = S.base_scale(b.model or "assets/models/ball.dmesh")
  b.scale_x, b.scale_y, b.scale_z = b.scale_x * bs, b.scale_y * bs, b.scale_z * bs
  b.origin_sx, b.origin_sy, b.origin_sz = b.origin_sx * bs, b.origin_sy * bs, b.origin_sz * bs
  dse.ecs.add_transform(b.e, b.x, b.y, b.z, b.scale_x, b.scale_y, b.scale_z)
  local model = b.model or "assets/models/ball.dmesh"
  pcall(dse.ecs.mesh_renderer_add, b.e, model)
  dse.ecs.set_mesh_shader_variant(b.e, "MESH_LIT")
  dse.ecs.set_mesh_color(b.e, b.color_r, b.color_g, b.color_b, b.color_a)
end

-- ============================================================================
-- 更新子弹的 ECS transform
-- ============================================================================
local function sync_entity(b)
  if not b.e then return end
  dse.ecs.set_transform_position(b.e, b.x, b.y, b.z)
  dse.ecs.set_transform_scale(b.e, b.scale_x, b.scale_y, b.scale_z)
  if b.color_a < 1 or b.render_on == false then
    dse.ecs.set_mesh_color(b.e, b.color_r, b.color_g, b.color_b, b.render_on and b.color_a or 0)
  end
end

-- ============================================================================
-- 碰撞检测: 子弹 vs 敌人
-- ============================================================================
local function check_hit_enemies(b, hit_radius)
  if not b.collide_enemies then return false end
  hit_radius = hit_radius or 0.8
  for _, en in ipairs(Entities.enemies) do
    if not en.dead and en.monmovestat > 0 then
      if dist2d(b.x, b.z, en.x, en.z) < hit_radius then
        -- 调用 main.lua 注入的伤害回调
        if BulletSystem.on_hit_enemy then
          BulletSystem.on_hit_enemy(en, b)
        end
        return true
      end
    end
  end
  -- Boss
  if Entities.boss and not Entities.boss.dead then
    if dist2d(b.x, b.z, Entities.boss.x, Entities.boss.z) < hit_radius * 1.5 then
      if BulletSystem.on_hit_boss then
        BulletSystem.on_hit_boss(Entities.boss, b)
      end
      return true
    end
  end
  return false
end

-- ============================================================================
-- 碰撞检测: 子弹 vs 玩家
-- ============================================================================
local function check_hit_player(b, hit_radius)
  if not b.collide_player then return false end
  hit_radius = hit_radius or 0.8
  if dist2d(b.x, b.z, Player.x, Player.z) < hit_radius then
    if BulletSystem.on_hit_player then
      BulletSystem.on_hit_player(b)
    end
    return true
  end
  return false
end

-- ============================================================================
-- 各类型子弹更新逻辑
-- ============================================================================

-- Bullet_arrow: 直线前进
local function update_arrow(b, dt)
  local fx, fz = yaw_to_dir(b.yaw)
  b.x = b.x + fx * b.speed * dt
  b.z = b.z + fz * b.speed * dt
  if b.y < 0 then b.active = false end
end

-- Bullet_sword: 向上漂浮
local function update_sword(b, dt)
  b.y = b.y + 0.1 * dt
end

-- Bullet_chaarrow: 速度4直线
local function update_chaarrow(b, dt)
  local fx, fz = yaw_to_dir(b.yaw)
  b.x = b.x + 4 * fx * dt
  b.z = b.z + 4 * fz * dt
  if b.y < 0 then b.active = false end
end

-- Bullet_chaarrow_multi: 膨胀+前进
local function update_chaarrow_multi(b, dt)
  if b.timer > 0.2 then
    b.active = false
    b.scale_x, b.scale_y, b.scale_z = 0.1, 0.1, 0.1
  else
    b.scale_x = b.scale_x + 4 * dt
    b.scale_y = b.scale_y + 4 * dt
    b.scale_z = b.scale_z + 4 * dt
    local fx, fz = yaw_to_dir(b.yaw)
    b.x = b.x + 4 * fx * dt
    b.z = b.z + 4 * fz * dt
  end
end

-- Bullet_explode: 延迟爆炸
local function update_explode(b, dt)
  -- 前进到目标位置 (OnEnable)
  if b.timer == 0 then
    local fx, fz = yaw_to_dir(b.yaw)
    b.x = b.x + fx * b.distance
    b.z = b.z + fz * b.distance
    b.collision_on = true
  end
  if b.timer > 1.5 then
    b.active = false
    b.timer = 0
    b.collision_on = false
  elseif not b.impact and b.timer > 0.5 then
    b.collision_on = false
    b.impact = true
  end
  if b.collision_on then
    check_hit_enemies(b, 1.0)
  end
end

-- Bullet_lightning: 从天而降的闪电
local function update_lightning(b, dt)
  if b.timer == 0 then
    b.x = Player.x + rand_range(-0.02, 0.04)
    b.z = Player.z + rand_range(-0.02, 0.04)
    b.y = 1.5
  end
  if b.y > 0 then
    b.y = b.y - 5 * dt
  else
    b.y = 0
  end
  if b.timer > 0.5 then
    b.active = false
  end
end

-- Bullet_magicmissile: 追踪导弹
local function update_magicmissile(b, dt)
  -- 计算朝向玩家的方向
  local dx, dz = Player.x - b.x, Player.z - b.z
  if dx ~= 0 or dz ~= 0 then
    local target_yaw = dir_to_yaw(dx, dz)
    b.yaw = lerp_angle(b.yaw, target_yaw, dt * b.homing_rate)
  end
  local fx, fz = yaw_to_dir(b.yaw)
  b.x = b.x + fx * b.homing_spd * dt
  b.z = b.z + fz * b.homing_spd * dt
  if b.timer > b.close_time then
    -- C# magicmissile: (0.5,1,1)*1.5dt → x 0.75/s, y 1.5/s, z 1.5/s
    b.scale_x = b.scale_x - 0.75 * dt
    b.scale_y = b.scale_y - 1.5 * dt
    b.scale_z = b.scale_z - 1.5 * dt
    if b.scale_z < 0.02 then
      b.active = false
    end
  end
  check_hit_player(b, 0.8)
end

-- Bullet_tornado: 龙卷风 (敌人武器)
local function update_tornado(b, dt)
  if not b.parent_enemy or b.parent_enemy.monmovestat <= 0 then
    if not b.phase or b.phase == 0 then
      b.active = false
      return
    end
  end
  if b.timer < 1.2 then
    -- 膨胀阶段
    b.scale_x = b.scale_x + 0.5 * dt
    b.scale_y = b.scale_y + 0.5 * dt
    b.scale_z = b.scale_z + 0.5 * dt
  else
    if b.phase == 0 then
      b.phase = 1
      b.scale_x, b.scale_y, b.scale_z = 2, 2, 2
    end
    -- 移动阶段: 正弦曲线
    b.sintime = b.sintime + dt
    local fx, fz = yaw_to_dir(b.yaw)
    local rx, rz = -fz, fx  -- right 向量
    local curve = 0.1 * math.cos(b.sintime * 8) * b.rnddir
    local tx = fx * 0.8 + rx * curve
    local tz = fz * 0.8 + rz * curve
    b.yaw = dir_to_yaw(tx, tz)
    b.x = b.x + tx * dt
    b.z = b.z + tz * dt
    if b.scale_y > 2.2 then
      b.scale_x = b.scale_x - dt
      b.scale_z = b.scale_z - dt
      if b.scale_x < 1 then
        b.active = false
      end
    else
      b.scale_y = b.scale_y + 0.2 * dt
    end
  end
  check_hit_player(b, 0.8)
end

-- Bullet_tornado_b: 简化龙卷风
local function update_tornado_b(b, dt)
  local s = 0.5 * dt
  b.scale_x = b.scale_x - 5 * s
  b.scale_y = b.scale_y + s
  b.scale_z = b.scale_z - 5 * s
  local fx, fz = yaw_to_dir(b.yaw)
  b.x = b.x + fx * s
  b.z = b.z + fz * s
  if b.scale_y > 4 then
    b.active = false
  end
  check_hit_player(b, 0.8)
end

-- Bullet_arrow_general: 加速箭矢
local function update_arrow_general(b, dt)
  -- 生长到原始大小
  local grow_spd = b.grow_rate or 2
  if b.scale_z < b.origin_sz then
    b.scale_z = math.min(b.scale_z + grow_spd * dt, b.origin_sz)
  end
  -- 加速
  b.speed = lerp(b.speed, b.homing_spd, dt * 3)
  if b.speed > 0 then
    local fx, fz = yaw_to_dir(b.yaw)
    b.x = b.x + fx * b.speed * dt
    b.z = b.z + fz * b.speed * dt
  end
  if b.timer > b.disable_delay then
    b.active = false
  end
  check_hit_enemies(b, 0.8)
end

-- Bullet_arrow_spread: 分裂箭
local function update_arrow_spread(b, dt)
  if b.timer == 0 and b.sub_count > 0 then
    -- 创建分裂子箭
    for i = 1, b.sub_count do
      local a_yaw = b.yaw + b.sub_angle * i
      spawn({
        type = "arrow", x = b.x, y = b.y, z = b.z, yaw = a_yaw,
        speed = b.speed, damage = b.damage, attack_type = b.attack_type,
        life = 1.0, model = b.model, collide_enemies = b.collide_enemies,
      })
      local b_yaw = b.yaw - b.sub_angle * i
      spawn({
        type = "arrow", x = b.x, y = b.y, z = b.z, yaw = b_yaw,
        speed = b.speed, damage = b.damage, attack_type = b.attack_type,
        life = 1.0, model = b.model, collide_enemies = b.collide_enemies,
      })
    end
  end
  if b.y > 0 then
    local fx, fz = yaw_to_dir(b.yaw)
    b.x = b.x + fx * b.speed * dt
    b.z = b.z + fz * b.speed * dt
  end
end

-- Bullet_arrowshower: 箭雨
local function update_arrowshower(b, dt)
  if b.timer == 0 then
    -- 创建4支子箭
    for _ = 1, 4 do
      local sx = b.x + rand_range(-0.5, 0.5)
      local sz = b.z + rand_range(-0.5, 0.5)
      spawn({
        type = "arrowshower_sub", x = sx, y = b.y, z = sz, yaw = b.yaw,
        damage = b.damage, attack_type = b.attack_type, life = 1.5,
        model = b.model, collide_enemies = b.collide_enemies,
      })
    end
  end
  if b.y > 0.2 then
    b.y = b.y - 3 * dt
  else
    b.active = false
  end
end

-- Bullet_arrowshower_sub: 箭雨子箭
local function update_arrowshower_sub(b, dt)
  if b.y > 0.2 then
    b.y = b.y - 3 * dt
  elseif b.scale_y > 0 then
    b.y = b.y - dt
    b.scale_y = b.scale_y - dt
  else
    b.active = false
  end
  check_hit_enemies(b, 0.5)
end

-- Bullet_delay: 延迟出现
local function update_delay(b, dt)
  if b.timer >= b.disable_delay then
    b.active = false
    b.render_on = false
    b.collision_on = false
    b.scale_x = b.origin_sx * 0.1
    b.scale_y = b.origin_sy * 0.1
    b.scale_z = b.origin_sz * 0.1
  elseif b.render_on then
    if b.scale_x < b.origin_sx then
      b.scale_x = b.scale_x + 4 * dt
      b.scale_y = b.scale_y + 4 * dt
      b.scale_z = b.scale_z + 4 * dt
    end
    if b.collision_on then
      check_hit_enemies(b, 0.8)
    end
  elseif b.timer >= b.show_delay then
    b.collision_on = true
    b.render_on = true
  end
end

-- Bullet_delay2: 延迟碰撞
local function update_delay2(b, dt)
  if b.timer >= b.show_delay and not b.collision_on then
    b.collision_on = true
  end
  if b.timer >= b.disable_delay then
    b.active = false
    b.collision_on = false
  end
  if b.collision_on then
    check_hit_enemies(b, 0.8)
  end
  if b.tune > 0 then -- moveleft
    local rx, rz = yaw_to_dir(b.yaw + 90)
    b.x = b.x - rx * 0.2 * dt
    b.z = b.z - rz * 0.2 * dt
  end
end

-- Bullet_particle: 粒子弹
local function update_particle(b, dt)
  if b.timer >= b.disable_delay then
    b.active = false
  end
  if b.timer >= b.active_delay then
    b.collision_on = false
  elseif b.timer >= b.start_delay then
    b.collision_on = true
  end
  if b.collision_on then
    check_hit_enemies(b, 1.0)
  end
end

-- Bullet_particle2: 膨胀粒子弹
local function update_particle2(b, dt)
  if b.timer >= b.disable_delay then
    b.active = false
  elseif b.timer >= b.show_delay then
    b.collision_on = false
  end
  b.scale_x = b.scale_x + b.expand_factor * dt
  b.scale_y = b.scale_y + b.expand_factor * dt
  b.scale_z = b.scale_z + b.expand_factor * dt
  if b.collision_on then
    check_hit_enemies(b, 1.0)
  end
end

-- Bullet_punch: 冲拳 (玩家技能)
local function update_punch(b, dt)
  if b.phase == 0 then
    -- 冲刺阶段
    if b.timer < b.show_delay then
      -- 玩家向前冲刺
      local tx, tz = b.target_x - Player.x, b.target_z - Player.z
      Player.x = Player.x + tx * 5 * dt
      Player.z = Player.z + tz * 5 * dt
    else
      b.phase = 1
      b.x = Player.x
      b.z = Player.z
      b.y = 0.1 + b.tune * 0.1
      local fx, fz = yaw_to_dir(Player.yaw)
      b.x = b.x + fx * 0.15
      b.z = b.z + fz * 0.15
      b.scale_x, b.scale_y, b.scale_z = 0.5, 2, 0.5
      b.collision_on = true
    end
  elseif b.phase == 1 then
    b.scale_x = b.scale_x + b.grow_vec_x * b.grow_rate * dt
    b.scale_z = b.scale_z + b.grow_vec_z * b.grow_rate * dt
    if b.scale_x > 3 then
      b.active = false
    elseif b.scale_x > 1 then
      b.collision_on = false
    end
    if b.collision_on then
      check_hit_enemies(b, 1.5)
    end
  end
end

-- Bullet_hammer / Bullet_jumpsplash: 锤击/跳跃飞溅
local function update_hammer(b, dt)
  if b.timer == 0 then
    local fx, fz = yaw_to_dir(b.yaw)
    b.x = b.x + fx * b.distance
    b.z = b.z + fz * b.distance
    b.scale_x, b.scale_y, b.scale_z = 1, 8, 1
    b.collision_on = true
  end
  b.scale_x = b.scale_x + b.grow_vec_x * 7 * dt
  b.scale_y = b.scale_y + b.grow_vec_y * 7 * dt
  b.scale_z = b.scale_z + b.grow_vec_z * 7 * dt
  if b.scale_y < 1 then
    b.active = false
  elseif b.scale_y < 4 then
    b.collision_on = false
  end
  -- 颜色淡出
  b.color_a = lerp(b.color_a, 0, dt * 5)
  if b.collision_on then
    check_hit_enemies(b, 1.0)
  end
end

-- Bullet_spear: 矛 (C# OnEnable 一次性位移 forward*tune + 固定速度 0.2)
local function update_spear(b, dt)
  local fx, fz = yaw_to_dir(b.yaw)
  if not b._tune_applied then
    b._tune_applied = true
    b.x = b.x + fx * b.tune
    b.z = b.z + fz * b.tune
  end
  b.x = b.x + fx * 0.2 * dt
  b.z = b.z + fz * 0.2 * dt
  b.scale_x = b.scale_x - 0.6 * dt
  if b.scale_x < 0 then
    b.active = false
    b.scale_x = b.origin_sx
  end
  check_hit_enemies(b, 0.6)
end

-- Bullet_spear_Dash: 冲刺矛
local function update_spear_dash(b, dt)
  if b.timer < 0.4 then
    -- 等待阶段
  elseif b.phase == 0 then
    b.phase = 1
    b.scale_z = b.origin_sz
  elseif b.phase == 1 then
    -- 延伸阶段
    local fx, fz = yaw_to_dir(b.yaw)
    b.x = b.x + fx * 1.5 * dt
    b.z = b.z + fz * 1.5 * dt
    b.scale_z = b.scale_z + 30 * dt
    if b.scale_z > 3 then
      -- C# spear_Dash: scale.z>3 → localScale=(2,2,4)
      b.scale_x = 2
      b.scale_y = 2
      b.scale_z = 4
      b.phase = 2
    end
  elseif b.phase == 2 then
    -- 收缩阶段
    b.scale_z = b.scale_z - 3 * dt
    local fx, fz = yaw_to_dir(b.yaw)
    b.x = b.x + fx * 0.5 * dt
    b.z = b.z + fz * 0.5 * dt
    if b.scale_z < 0.1 then
      b.active = false
    end
  end
  if b.phase >= 1 then
    check_hit_enemies(b, 0.8)
  end
end

-- Bullet_trigger: 触发式子弹
local function update_trigger(b, dt)
  local fx, fz = yaw_to_dir(b.yaw)
  b.x = b.x + fx * b.speed * dt
  b.z = b.z + fz * b.speed * dt
  if check_hit_enemies(b, 0.6) then
    -- 创建溅射特效
    if BulletSystem.on_splash then
      BulletSystem.on_splash(b.x, b.z)
    end
    b.active = false
  end
  if b.timer > 0.4 then
    b.active = false
  end
end

-- Bullet_wheelwind: 轮风 (跟随敌人)
local function update_wheelwind(b, dt)
  b.yaw = b.yaw - 1000 * dt  -- 旋转
  if b.parent_enemy then
    b.x = b.parent_enemy.x
    b.z = b.parent_enemy.z
  end
  if not b.parent_enemy or b.parent_enemy.monmovestat <= 0 then
    b.active = false
  end
  check_hit_player(b, 0.8)
end

-- Bullet_runswing_b06: 挥砍弹
local function update_runswing(b, dt)
  if b.timer > b.disable_delay then
    b.active = false
    b.collision_on = false
  elseif b.timer > b.active_delay then
    b.collision_on = true
  end
  if b.collision_on then
    check_hit_enemies(b, 0.8)
  end
end

-- Bullet_arrow_ride: 骑乘箭矢
local function update_arrow_ride(b, dt)
  local fx, fz = yaw_to_dir(b.yaw)
  b.x = b.x + fx * b.speed * dt
  b.z = b.z + fz * b.speed * dt
  check_hit_player(b, 0.8)
end

-- Bullet_Angel1: 天使箭矢
local function update_angel1(b, dt)
  local fx, fz = yaw_to_dir(b.yaw)
  b.x = b.x + fx * b.speed * dt
  b.z = b.z + fz * b.speed * dt
  if b.y < 0 then
    if BulletSystem.on_angel_finish then BulletSystem.on_angel_finish() end
    b.active = false
  end
  check_hit_enemies(b, 0.8)
end

-- Bullet_Angel2: 天使范围弹
local function update_angel2(b, dt)
  if b.timer > b.disable_delay then
    b.active = false
  elseif not b.impact and b.timer > b.active_delay then
    if BulletSystem.on_angel_finish then BulletSystem.on_angel_finish() end
    b.collision_on = true
    b.impact = true
  end
  if b.collision_on then
    check_hit_enemies(b, 1.0)
  end
end

-- Bullet_Angel3: 天使光束
local function update_angel3(b, dt)
  -- 曲线移动
  local fx, fz = yaw_to_dir(b.yaw)
  local rx, rz = -fz, fx
  b.dx = b.dx + rx * 6 * dt
  b.dz = b.dz + rz * 6 * dt
  b.target_x = (b.target_x or b.x) + b.dx * dt
  b.target_z = (b.target_z or b.z) + b.dz * dt
  b.x = b.target_x
  b.z = b.target_z
  if b.timer > 2.8 then
    if BulletSystem.on_angel_finish then BulletSystem.on_angel_finish() end
    b.active = false
  end
  check_hit_enemies(b, 0.8)
end

-- Bullet_Angel4: 天使连射矛
local function update_angel4(b, dt)
  -- 每0.2秒发射一支子矛
  if b.timer > 0.1 + b.sub_count * 0.2 and b.sub_count < 6 then
    spawn({
      type = "angel4_m", x = b.x, y = b.y, z = b.z, yaw = b.yaw,
      damage = b.damage, attack_type = b.attack_type, life = 2.0,
      side = b.sub_count % 2 == 0 and 1 or -1,
      model = b.model, collide_enemies = b.collide_enemies,
    })
    b.sub_count = b.sub_count + 1
  end
  if b.sub_count >= 6 then
    if BulletSystem.on_angel_finish then BulletSystem.on_angel_finish() end
    b.active = false
  end
end

-- Bullet_Angel4_m: 天使连射子矛
local function update_angel4_m(b, dt)
  if b.timer == 0 then
    b.scale_x, b.scale_y, b.scale_z = 0, 0, 0
    b.speed = -2
    b.side_speed = 0.2 + rand_range(-0.1, 0.1)
    local fx, fz = yaw_to_dir(b.yaw)
    b.dx = fx
    b.dz = fz
  end
  b.speed = b.speed + 6 * dt
  b.side_speed = lerp(b.side_speed, 0, dt * 2)
  b.scale_x = lerp(b.scale_x, b.origin_sx, dt * 4)
  b.scale_y = lerp(b.scale_y, b.origin_sy, dt * 4)
  b.scale_z = lerp(b.scale_z, b.origin_sz, dt * 4)
  local fx, fz = yaw_to_dir(b.yaw)
  local rx, rz = -fz, fx
  b.x = b.x + rx * b.side * b.side_speed * dt
  b.z = b.z + rz * b.side * b.side_speed * dt
  b.x = b.x + fx * b.speed * dt
  b.z = b.z + fz * b.speed * dt
  if b.y < 0 then
    b.active = false
  end
  check_hit_enemies(b, 0.6)
end

-- ── 类型→更新函数映射 ─────────────────────────────────────────────
local updaters = {
  arrow           = update_arrow,
  sword           = update_sword,
  chaarrow        = update_chaarrow,
  chaarrow_multi  = update_chaarrow_multi,
  explode         = update_explode,
  lightning       = update_lightning,
  magicmissile    = update_magicmissile,
  tornado         = update_tornado,
  tornado_b       = update_tornado_b,
  arrow_general   = update_arrow_general,
  arrow_spread    = update_arrow_spread,
  arrowshower     = update_arrowshower,
  arrowshower_sub = update_arrowshower_sub,
  delay           = update_delay,
  delay2          = update_delay2,
  particle        = update_particle,
  particle2       = update_particle2,
  punch           = update_punch,
  hammer          = update_hammer,
  jumpsplash      = update_hammer,  -- 相同逻辑
  spear           = update_spear,
  spear_dash      = update_spear_dash,
  trigger         = update_trigger,
  wheelwind       = update_wheelwind,
  runswing        = update_runswing,
  arrow_ride      = update_arrow_ride,
  angel1          = update_angel1,
  angel2          = update_angel2,
  angel3          = update_angel3,
  angel4          = update_angel4,
  angel4_m        = update_angel4_m,
}

-- ============================================================================
-- 角度插值 (处理 360° 环绕)
-- ============================================================================
function lerp_angle(a, b, t)
  local diff = b - a
  while diff > 180 do diff = diff - 360 end
  while diff < -180 do diff = diff + 360 end
  return a + diff * t
end

-- ============================================================================
-- 主更新函数
-- ============================================================================
local function update(dt)
  for i = #active, 1, -1 do
    local b = active[i]
    b.timer = b.timer + dt

    -- 创建 ECS 实体 (延迟)
    if b.active and b.render_on then
      ensure_entity(b)
    end

    -- 执行类型特定更新
    local fn = updaters[b.type]
    if fn then
      fn(b, dt)
    else
      -- 默认: 直线前进
      b.x = b.x + b.dx * dt
      b.z = b.z + b.dz * dt
    end

    -- 同步 ECS
    if b.e then sync_entity(b) end

    -- 超出边界或过期
    if not b.active or b.timer >= b.life
      or math.abs(b.x) > 30 or math.abs(b.z) > 30 then
      release(b)
      table.remove(active, i)
    end
  end
end

-- ============================================================================
-- 清空所有子弹 (关卡切换时调用)
-- ============================================================================
local function clear()
  for i = #active, 1, -1 do
    release(active[i])
    table.remove(active, i)
  end
end

-- ============================================================================
-- 获取当前活跃子弹数
-- ============================================================================
local function count() return #active end

-- ============================================================================
-- 模块导出
-- ============================================================================
local BulletSystem = {
  spawn   = spawn,
  update  = update,
  clear   = clear,
  count   = count,
  active  = active,  -- 暴露列表供外部遍历
  -- 回调 (main.lua 注入)
  on_hit_enemy  = nil,
  on_hit_boss   = nil,
  on_hit_player = nil,
  on_splash     = nil,
  on_angel_finish = nil,
}

-- 写入全局供 main.lua require
_G.BulletSystem = BulletSystem

return BulletSystem
