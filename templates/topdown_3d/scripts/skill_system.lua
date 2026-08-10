-- ============================================================================
-- skill_system.lua — 完整技能系统
-- 1:1 移植自:
--   Cha_Skill.cs        (1095 行) — 技能控制器: 施法/发射/重复/延迟
--   SwordDance.cs       (  51 行) — 剑舞 UV 滚动 + 周期碰撞
--   SpiritSword.cs      ( 139 行) — 御剑术 v1 (7 把子剑)
--   SpiritSword2.cs     ( 137 行) — 御剑术 v2 (2 把子剑, 12 次命中)
--   SpiritSword3.cs     ( 136 行) — 御剑术 v3 (4 把子剑, 24 次命中)
--   SpiritSword_p.cs    (  82 行) — 飞剑 v1 (追踪目标)
--   SpiritSword_p2.cs   ( 103 行) — 飞剑 v2 (返回+重新锁定)
--   SpiritSword_p3.cs   ( 126 行) — 飞剑 v3 (突击+编队返回)
--   Sk_fincanon.cs      (  96 行) — 光束炮 (creat→beam→destroy)
--   Sk_meteo.cs         (  66 行) — 陨石坠落
--   Sk_machinegun.cs    (  72 行) — 机枪扫射
--   Sk_chainbreak.cs    (  91 行) — 锁链爆裂 (粒子→爆炸)
--   Sk_groundbreak.cs   (  24 行) — 地裂
--   Sk_flybug.cs        (  52 行) — 飞虫群
--   Junwui.cs           ( 144 行) — 武将召唤体 (出现→游荡→攻击→消失)
-- 依赖: state.lua, database.lua
-- ============================================================================

local S = require "scripts.state"
local G, Player, Entities = S.G, S.Player, S.Entities
local DB = require "database"
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
local function rand_range(a, b) return a + math.random() * (b - a) end
local function lerp_angle(a, b, t)
  local diff = b - a
  while diff > 180 do diff = diff - 360 end
  while diff < -180 do diff = diff + 360 end
  return a + diff * t
end
local function pforward() return yaw_to_dir(Player.yaw) end
local function pright()
  local fx, fz = yaw_to_dir(Player.yaw)
  return -fz, fx
end

-- ============================================================================
-- 技能实体池
-- ============================================================================
local pool = {}
local active = {}

local function obtain()
  local s = table.remove(pool)
  if not s then s = {} end
  return s
end

local function release(s)
  if s.e then kill_entity(s.e); s.e = nil end
  if s.children then
    for _, c in ipairs(s.children) do
      if c.e then kill_entity(c.e); c.e = nil end
    end
    s.children = nil
  end
  for k in pairs(s) do if k ~= "e" then s[k] = nil end end
  table.insert(pool, s)
end

-- ECS 实体管理
local function ensure_entity(s, model)
  if s.e then return end
  s.e = dse.ecs.create_entity()
  -- 模型基准缩放烘焙进 scale (model_scale.lua), 仅创建时乘一次, 后续 sync 沿用
  local bs = S.base_scale(model or "assets/models/ball.dmesh")
  s.scale_x, s.scale_y, s.scale_z = (s.scale_x or 1) * bs, (s.scale_y or 1) * bs, (s.scale_z or 1) * bs
  s.origin_sx, s.origin_sy, s.origin_sz = s.scale_x, s.scale_y, s.scale_z
  dse.ecs.add_transform(s.e, s.x or 0, s.y or 0, s.z or 0, s.scale_x, s.scale_y, s.scale_z)
  pcall(dse.ecs.mesh_renderer_add, s.e, model or "assets/models/ball.dmesh")
  dse.ecs.set_mesh_shader_variant(s.e, "MESH_LIT")
  if s.color_r then
    dse.ecs.set_mesh_color(s.e, s.color_r, s.color_g or 1, s.color_b or 1, s.color_a or 1)
  end
end

local function sync_entity(s)
  if not s.e then return end
  dse.ecs.set_transform_position(s.e, s.x or 0, s.y or 0, s.z or 0)
  dse.ecs.set_transform_scale(s.e, s.scale_x or 1, s.scale_y or 1, s.scale_z or 1)
  if s.color_a and s.color_a < 1 then
    dse.ecs.set_mesh_color(s.e, s.color_r or 1, s.color_g or 1, s.color_b or 1, s.color_a)
  end
end

-- 创建技能实体
local function spawn_skill(cfg)
  local s = obtain()
  s.type     = cfg.type or "generic"
  s.x        = cfg.x or Player.x
  s.y        = cfg.y or 0.1
  s.z        = cfg.z or Player.z
  s.yaw      = cfg.yaw or Player.yaw
  s.scale_x  = cfg.sx or 1
  s.scale_y  = cfg.sy or 1
  s.scale_z  = cfg.sz or 1
  s.origin_sx = s.scale_x
  s.origin_sy = s.scale_y
  s.origin_sz = s.scale_z
  s.damage   = cfg.damage or 0
  s.attack_type = cfg.attack_type or "skill"
  s.life     = cfg.life or 5.0
  s.timer    = 0
  s.active   = true
  s.e        = nil
  s.model    = cfg.model
  s.color_r  = cfg.r or 1
  s.color_g  = cfg.g or 1
  s.color_b  = cfg.b or 1
  s.color_a  = cfg.a or 1
  s.collide_enemies = cfg.collide_enemies ~= false
  s.collision_on = false
  s.hit_count = 0
  s.hit_rate  = cfg.hit_rate or 0.2
  s.hit_timer = 0
  s.start_delay = cfg.start_delay or 0
  s.disable_delay = cfg.disable_delay or 2.0
  s.hit_radius = cfg.hit_radius or 1.0
  s.front_offset = cfg.front_offset or 0.25
  s.speed = cfg.speed or 5
  s.fall_speed = cfg.fall_speed or 3
  s.rotate_speed = cfg.rotate_speed or 500
  s.children = nil
  s.fireon   = false
  s.target_x = 0; s.target_z = 0; s.target_y = 0
  s.dt       = 0
  s.f_speed  = 0
  s.rndpos_x = 0; s.rndpos_z = 0
  s.n_pos_x  = 0; s.n_pos_z  = 0
  s.old_target = nil
  s.creatindex = 0
  s.creatfinish = false
  s.homing = false
  s.finish = false
  s.attack_start = true
  s.attack_count = 0
  s.shooton = false
  s.stat = false
  s.p_step = 0
  s.uv_offset = 0
  s.uv_speed = cfg.uv_speed or 1.0
  s.parent_x = Player.x
  s.parent_z = Player.z
  s.parent_yaw = Player.yaw
  table.insert(active, s)
  return s
end

-- 碰撞检测
local function check_hit_enemies(s, radius)
  if not s.collide_enemies then return false end
  radius = radius or s.hit_radius or 1.0
  local hit_any = false
  for _, en in ipairs(Entities.enemies) do
    if not en.dead and en.monmovestat and en.monmovestat > 0 then
      if dist2d(s.x, s.z, en.x, en.z) < radius then
        if SkillSystem.on_hit_enemy then SkillSystem.on_hit_enemy(en, s) end
        hit_any = true
      end
    end
  end
  if Entities.boss and not Entities.boss.dead then
    if dist2d(s.x, s.z, Entities.boss.x, Entities.boss.z) < radius * 1.5 then
      if SkillSystem.on_hit_boss then SkillSystem.on_hit_boss(Entities.boss, s) end
      hit_any = true
    end
  end
  return hit_any
end

-- 定时回调 (替代 C# Invoke / StartCoroutine)
local callbacks = {}
local function add_callback(delay, fn)
  table.insert(callbacks, { time = delay, fn = fn })
end
local function update_callbacks(dt)
  for i = #callbacks, 1, -1 do
    callbacks[i].time = callbacks[i].time - dt
    if callbacks[i].time <= 0 then
      local fn = callbacks[i].fn
      table.remove(callbacks, i)
      fn()
    end
  end
end

-- ============================================================================
-- 各技能实体更新逻辑
-- ============================================================================

-- SwordDance: UV 滚动 + 周期碰撞
local function update_sworddance(s, dt)
  s.uv_offset = s.uv_offset + dt * s.uv_speed
  if s.uv_offset >= 0.9 then s.active = false; return end
  s.hit_timer = s.hit_timer + dt
  if s.hit_timer >= s.hit_rate then
    s.hit_timer = 0
    check_hit_enemies(s, 1.5)
  end
end

-- SpiritSword v1: 7 把子剑, 跟随玩家, 碰撞发射飞剑
local function update_spiritsword(s, dt)
  s.x = Player.x; s.y = 0.2; s.z = Player.z
  s.yaw = lerp_angle(s.yaw, Player.yaw, dt * 3)
  if not s.creatfinish then
    if s.start_delay > 0.1 then
      s.start_delay = 0
      local idx = s.creatindex
      if not s.children then s.children = {} end
      local child_yaw = 30 * idx + s.yaw - 270
      if not s.children[idx + 1] then
        local c = spawn_skill({
          type = "spiritsword_p", x = s.x, y = 0.2, z = s.z, yaw = child_yaw,
          damage = s.damage, collide_enemies = false, life = 10.0,
        })
        s.children[idx + 1] = c
      else
        local c = s.children[idx + 1]
        c.x = s.x; c.z = s.z; c.yaw = child_yaw; c.active = true
      end
      if s.creatindex < 6 then s.creatindex = s.creatindex + 1
      else s.creatfinish = true end
    else
      s.start_delay = s.start_delay + dt
    end
  elseif s.start_delay > 0.4 then
    s.homing = false; s.collision_on = true; s.start_delay = -1
  elseif s.start_delay > -1 then
    s.start_delay = s.start_delay + dt
  else
    s.start_delay = s.start_delay - dt
    if s.start_delay < -2 then s.collision_on = true; s.old_target = nil end
  end
  -- 碰撞 → 发射子剑
  if s.collision_on and not s.homing then
    for _, en in ipairs(Entities.enemies) do
      if not en.dead and en.monmovestat and en.monmovestat > 0 and en ~= s.old_target then
        if dist2d(s.x, s.z, en.x, en.z) < 1.5 then
          local idx = s.hit_count
          if s.children and s.children[idx + 1] then
            local c = s.children[idx + 1]
            c.fireon = true
            c.target_x = en.x; c.target_z = en.z; c.target_y = en.y or 0.5
            c.collide_enemies = true
          end
          s.old_target = en
          s.hit_count = s.hit_count + 1
          s.homing = true; s.collision_on = false; s.start_delay = 0
          if s.hit_count >= 7 then s.active = false; return end
          break
        end
      end
    end
    -- Boss
    if Entities.boss and not Entities.boss.dead and Entities.boss ~= s.old_target then
      if dist2d(s.x, s.z, Entities.boss.x, Entities.boss.z) < 2.0 then
        local idx = s.hit_count
        if s.children and s.children[idx + 1] then
          local c = s.children[idx + 1]
          c.fireon = true
          c.target_x = Entities.boss.x; c.target_z = Entities.boss.z; c.target_y = 0.5
          c.collide_enemies = true
        end
        s.old_target = Entities.boss
        s.hit_count = s.hit_count + 1
        s.homing = true; s.collision_on = false; s.start_delay = 0
        if s.hit_count >= 7 then s.active = false; return end
      end
    end
  end
end

-- SpiritSword v2: 2 把子剑, 12 次命中
local function update_spiritsword2(s, dt)
  s.x = Player.x; s.y = 0.2; s.z = Player.z
  s.yaw = lerp_angle(s.yaw, Player.yaw, dt * 3)
  if not s.creatfinish then
    if s.start_delay > 0.2 then
      s.start_delay = 0
      local idx = s.creatindex
      if not s.children then s.children = {} end
      if not s.children[idx + 1] then
        local c = spawn_skill({
          type = "spiritsword_p2", x = s.x, y = 0.2, z = s.z, yaw = s.yaw,
          damage = s.damage, collide_enemies = true, life = 10.0,
        })
        s.children[idx + 1] = c
      else
        local c = s.children[idx + 1]
        c.x = s.x; c.z = s.z; c.yaw = s.yaw; c.active = true; c.fireon = true
      end
      if s.creatindex < 1 then s.creatindex = s.creatindex + 1
      else s.creatfinish = true end
    else
      s.start_delay = s.start_delay + dt
    end
  elseif s.start_delay > 0.4 then
    s.homing = false; s.collision_on = true; s.start_delay = -1
  elseif s.start_delay > -1 then
    s.start_delay = s.start_delay + dt
  else
    s.start_delay = s.start_delay - dt
    if s.start_delay < -2 then s.collision_on = true; s.old_target = nil end
  end
  if s.collision_on and not s.homing then
    for _, en in ipairs(Entities.enemies) do
      if not en.dead and en.monmovestat and en.monmovestat > 0 and en ~= s.old_target then
        if dist2d(s.x, s.z, en.x, en.z) < 1.5 then
          local idx = s.hit_count % 2
          if s.children and s.children[idx + 1] then
            local c = s.children[idx + 1]
            c.fireon = true
            c.target_x = en.x; c.target_z = en.z; c.target_y = en.y or 0.5
          end
          s.old_target = en
          s.hit_count = s.hit_count + 1
          s.homing = true; s.collision_on = false; s.start_delay = 0
          if s.hit_count >= 12 then
            if s.children then for _, c in ipairs(s.children) do c.fireon = false; c.finish = true end end
            s.active = false; return
          end
          break
        end
      end
    end
  end
end

-- SpiritSword v3: 4 把子剑, 24 次命中
local function update_spiritsword3(s, dt)
  s.x = Player.x; s.y = 0.1; s.z = Player.z
  s.yaw = lerp_angle(s.yaw, Player.yaw, dt * 3)
  if not s.creatfinish then
    if s.start_delay > 0.2 then
      s.start_delay = 0
      local idx = s.creatindex
      local n_pos = {
        [0] = {x = 0.13, z = 0.13}, [1] = {x = 0.13, z = -0.13},
        [2] = {x = -0.13, z = 0.13}, [3] = {x = -0.13, z = -0.13},
      }
      local np = n_pos[idx] or {x = 0, z = 0}
      if not s.children then s.children = {} end
      if not s.children[idx + 1] then
        local c = spawn_skill({
          type = "spiritsword_p3", x = s.x, y = 0.1, z = s.z, yaw = s.yaw,
          damage = s.damage, collide_enemies = false, life = 10.0,
        })
        c.n_pos_x = np.x; c.n_pos_z = np.z
        s.children[idx + 1] = c
      else
        local c = s.children[idx + 1]
        c.x = s.x; c.z = s.z; c.yaw = s.yaw; c.active = true
      end
      if s.creatindex < 3 then s.creatindex = s.creatindex + 1
      else s.creatfinish = true end
    else
      s.start_delay = s.start_delay + dt
    end
  elseif s.start_delay > 0.3 then
    s.homing = false; s.collision_on = true; s.start_delay = -1
  elseif s.start_delay > -1 then
    s.start_delay = s.start_delay + dt
  else
    s.start_delay = s.start_delay - dt
    if s.start_delay < -2 then s.collision_on = true; s.old_target = nil end
  end
  if s.collision_on and not s.homing then
    for _, en in ipairs(Entities.enemies) do
      if not en.dead and en.monmovestat and en.monmovestat > 0 and en ~= s.old_target then
        if dist2d(s.x, s.z, en.x, en.z) < 1.5 then
          local idx = s.hit_count % 4
          if s.children and s.children[idx + 1] then
            local c = s.children[idx + 1]
            c.fireon = true
            c.target_x = en.x; c.target_z = en.z; c.target_y = 0
            c.collide_enemies = true
          end
          s.old_target = en
          s.hit_count = s.hit_count + 1
          s.homing = true; s.collision_on = false; s.start_delay = 0
          if s.hit_count >= 24 then
            if s.children then for _, c in ipairs(s.children) do c.finish = true; c.fireon = false end end
            s.active = false; return
          end
          break
        end
      end
    end
  end
end

-- 飞剑 v1: 追踪目标飞行
local function update_spiritsword_p(s, dt)
  if s.fireon then
    if s.y > 0 then
      if s.dt < 10 then s.dt = s.dt + dt * 5 end
      local dx, dz = s.target_x - s.x, s.target_z - s.z
      if dx ~= 0 or dz ~= 0 then
        s.yaw = lerp_angle(s.yaw, dir_to_yaw(dx, dz), 4 * s.dt * dt)
      end
      local fx, fz = yaw_to_dir(s.yaw)
      s.x = s.x + fx * 1.8 * dt
      s.z = s.z + fz * 1.8 * dt
      check_hit_enemies(s, 0.6)
    else
      s.active = false
    end
  else
    if s.scale_z < 1 then s.scale_z = s.scale_z + dt * 6
    else s.scale_z = 1 end
  end
end

-- 飞剑 v2: 返回+重新锁定
local function update_spiritsword_p2(s, dt)
  if s.fireon then
    if s.y > 0 then
      if s.dt < 10 then s.dt = s.dt + dt * 5 end
      local dx, dz = s.target_x - s.x, s.target_z - s.z
      if dx ~= 0 or dz ~= 0 then
        s.yaw = lerp_angle(s.yaw, dir_to_yaw(dx, dz), 4 * s.dt * dt)
      end
      local fx, fz = yaw_to_dir(s.yaw)
      s.x = s.x + fx * 1.8 * dt
      s.z = s.z + fz * 1.8 * dt
      check_hit_enemies(s, 0.6)
    else
      s.y = s.y + 1.8 * dt
      s.dt = 0
    end
  else
    s.y = s.y + 1.8 * dt
    if s.y > 1 then s.active = false end
  end
end

-- 飞剑 v3: 突击+编队返回
local function update_spiritsword_p3(s, dt)
  if s.finish then
    s.y = s.y + 3 * dt
    if s.y > 1 then s.active = false end
  elseif s.fireon then
    s.f_speed = s.f_speed - dt
    if s.f_speed < 0 then s.fireon = false; return end
    local dx, dz = s.target_x - s.x, s.target_z - s.z
    local len = math.sqrt(dx * dx + dz * dz)
    if len > 0 then
      s.x = s.x + dx / len * s.f_speed * 20 * dt
      s.z = s.z + dz / len * s.f_speed * 20 * dt
    end
    s.yaw = dir_to_yaw(dx, dz)
    check_hit_enemies(s, 0.6)
  else
    -- 返回编队位置
    local pryaw = s.parent_yaw or 0
    local rfx, rfz = yaw_to_dir(pryaw)
    local rrx, rrz = -rfz, rfx
    local tx = s.parent_x + rfx * s.n_pos_z + rrx * s.n_pos_x
    local tz = s.parent_z + rfz * s.n_pos_z + rrz * s.n_pos_x
    local dx, dz = tx - s.x, tz - s.z
    if dx ~= 0 or dz ~= 0 then
      s.yaw = lerp_angle(s.yaw, dir_to_yaw(dx, dz), dt * 12)
    end
    local len = math.sqrt(dx * dx + dz * dz)
    if len > 0 then
      s.x = s.x + dx / len * 3 * dt
      s.z = s.z + dz / len * 3 * dt
    end
  end
end

-- Junwui: 出现→游荡→攻击→消失
local function update_junwui(s, dt)
  if s.phase == 0 then
    if s.timer > 1.0 then s.phase = 1; s.attack_start = false; s.collision_on = true end
  elseif s.phase == 1 then
    s.hit_timer = (s.hit_timer or 0) + dt
    if s.hit_timer >= 2.0 then
      s.hit_timer = 0
      local angle = math.random() * math.pi * 2
      local r = math.random() * 0.4
      s.rndpos_x = Player.x + math.cos(angle) * r
      s.rndpos_z = Player.z + math.sin(angle) * r
    end
    local dx, dz = s.rndpos_x - s.x, s.rndpos_z - s.z
    if dx ~= 0 or dz ~= 0 then
      s.yaw = lerp_angle(s.yaw, dir_to_yaw(dx, dz), dt * 2)
    end
    s.x = lerp(s.x, s.rndpos_x, dt)
    s.z = lerp(s.z, s.rndpos_z, dt)
    if s.collision_on then
      for _, en in ipairs(Entities.enemies) do
        if not en.dead and en.monmovestat and en.monmovestat > 0 then
          if dist2d(s.x, s.z, en.x, en.z) < 1.0 then
            s.phase = 2
            s.attack_target_yaw = dir_to_yaw(en.x - s.x, en.z - s.z)
            s.collision_on = false
            break
          end
        end
      end
    end
  elseif s.phase == 2 then
    s.yaw = lerp_angle(s.yaw, s.attack_target_yaw or s.yaw, dt * 8)
    check_hit_enemies(s, 1.5)
    s.attack_count = s.attack_count + 1
    if SkillSystem.on_camera_hitcam then SkillSystem.on_camera_hitcam(1.5) end
    if s.attack_count >= 12 then s.phase = 3
    else s.phase = 1; s.collision_on = false; s.hit_timer = 1.9 end
  elseif s.phase == 3 then
    s.scale_x = s.scale_x - dt * 3
    s.scale_y = s.scale_y - dt * 3
    s.scale_z = s.scale_z - dt * 3
    if s.scale_x < 0.1 then s.active = false end
  end
end

-- Sk_fincanon: 光束炮
local function update_sk_fincanon(s, dt)
  s.hit_timer = (s.hit_timer or 0) + dt
  if s.hit_timer >= 0.2 and s.timer > 0.5 and s.timer < 2.2 then
    s.hit_timer = 0
    s.x = Player.x; s.z = Player.z
    check_hit_enemies(s, 3.0)
  end
  if s.timer > 3.4 then s.active = false; s.scale_x = 0
  elseif s.timer > 2.2 then
    s.scale_x = lerp(s.scale_x or 1, 0, dt * 15)
    s.scale_y = lerp(s.scale_y or 1, 0, dt * 15)
  elseif s.timer > 1.0 then
    s.scale_x = lerp(s.scale_x or 0.1, 1, dt * 15)
    s.scale_y = lerp(s.scale_y or 0.1, 1, dt * 15)
  elseif s.timer > 0.8 then
    s.scale_x = lerp(s.scale_x or 0, 1, dt * 6)
    s.scale_y = lerp(s.scale_y or 0, 1, dt * 6)
  end
end

-- Sk_meteo: 陨石坠落
local function update_sk_meteo(s, dt)
  if not s.stat then
    if s.y > 0 then
      local fx, fz = yaw_to_dir(s.yaw)
      s.x = s.x + fx * 0.08; s.z = s.z + fz * 0.08
      s.y = s.y - 0.08 * 0.7
    else
      s.y = 0; s.stat = true; s.timer = 0
      check_hit_enemies(s, 2.0)
      if SkillSystem.on_boom then SkillSystem.on_boom(0, s.x, s.z, true) end
      if SkillSystem.on_camera_hitcam then SkillSystem.on_camera_hitcam(3.0) end
    end
  else
    if s.timer > 2.0 then s.active = false end
  end
end

-- Sk_machinegun: 机枪
local function update_sk_machinegun(s, dt)
  s.hit_timer = (s.hit_timer or 0) + dt
  if s.timer > 0.6 and s.timer < 4.0 and s.hit_timer >= 0.2 then
    s.hit_timer = 0
    check_hit_enemies(s, 2.0)
  end
  if s.timer > 4.5 then s.active = false
  elseif s.timer > 4.0 then s.shooton = false
  elseif s.timer > 0.6 and not s.shooton then s.shooton = true end
end

-- Sk_chainbreak: 锁链爆裂
local function update_sk_chainbreak(s, dt)
  s.hit_timer = (s.hit_timer or 0) + dt
  if s.p_step == 0 then
    if s.hit_timer >= 0.4 then
      s.hit_timer = 0
      check_hit_enemies(s, 1.0)
    end
    if s.timer > 2.0 then s.p_step = 1; s.hit_timer = 0 end
  elseif s.p_step == 1 then
    if s.timer > 2.5 then s.p_step = 2 end
  elseif s.p_step == 2 then
    if s.timer > 4.0 then s.active = false end
  end
  if s.y < 0.1 then s.y = s.y + 0.6 * dt end
end

-- Sk_groundbreak: 地裂
local function update_sk_groundbreak(s, dt)
  if s.timer == 0 then check_hit_enemies(s, 1.5) end
  if s.timer > 0.4 and not s.stat then s.stat = true end
  if s.timer > 2.0 then s.active = false end
end

-- Sk_flybug: 飞虫群
local function update_sk_flybug(s, dt)
  s.hit_timer = (s.hit_timer or 0) + dt
  if s.timer < 3.0 and s.hit_timer >= 0.3 then
    s.hit_timer = 0
    check_hit_enemies(s, 1.0)
  end
  if s.timer > 6.0 then s.active = false
  elseif s.timer > 3.0 and not s.stat then s.stat = true end
end

-- 通用持续伤害
local function update_generic_periodic(s, dt)
  s.hit_timer = s.hit_timer + dt
  if s.hit_timer >= s.hit_rate then
    s.hit_timer = 0
    check_hit_enemies(s, s.hit_radius)
  end
  if s.timer >= s.disable_delay then s.active = false end
end

-- 通用范围爆发
local function update_generic_aoe(s, dt)
  if s.timer < s.start_delay then
  elseif s.timer < s.start_delay + 0.3 then
    local t = (s.timer - s.start_delay) / 0.3
    s.scale_x = lerp(0, s.origin_sx, t)
    s.scale_z = lerp(0, s.origin_sz, t)
    check_hit_enemies(s, s.hit_radius)
  else
    s.color_a = lerp(s.color_a, 0, dt * 3)
    if s.color_a < 0.05 then s.active = false end
  end
end

-- 跟随玩家持续伤害
local function update_follow_player(s, dt)
  s.x = Player.x; s.z = Player.z
  s.yaw = s.yaw - s.rotate_speed * dt
  s.hit_timer = s.hit_timer + dt
  if s.hit_timer >= s.hit_rate then
    s.hit_timer = 0
    check_hit_enemies(s, s.hit_radius)
  end
  if s.timer >= s.disable_delay then s.active = false end
end

-- 前方持续伤害
local function update_front_persistent(s, dt)
  local fx, fz = yaw_to_dir(Player.yaw)
  s.x = Player.x + fx * s.front_offset
  s.z = Player.z + fz * s.front_offset
  s.yaw = Player.yaw
  s.hit_timer = s.hit_timer + dt
  if s.hit_timer >= s.hit_rate then
    s.hit_timer = 0
    check_hit_enemies(s, s.hit_radius)
  end
  if s.timer >= s.disable_delay then s.active = false end
end

-- 能量柱
local function update_energypillar(s, dt)
  if s.timer >= 1.5 then s.active = false; return end
  if s.timer < 0.3 then s.scale_y = lerp(0, s.origin_sy, s.timer / 0.3) end
  s.hit_timer = s.hit_timer + dt
  if s.hit_timer >= 0.3 then s.hit_timer = 0; check_hit_enemies(s, 1.0) end
  if SkillSystem.on_camera_hitcam then SkillSystem.on_camera_hitcam(1.5) end
end

-- 竹子
local function update_bamboo(s, dt)
  if s.timer >= 2.0 then s.active = false; return end
  if s.y < 0 then s.y = s.y + 3 * dt end
  s.hit_timer = s.hit_timer + dt
  if s.hit_timer >= 0.3 then s.hit_timer = 0; check_hit_enemies(s, 0.8) end
  if SkillSystem.on_camera_hitcam then SkillSystem.on_camera_hitcam(1.5) end
end

-- 翅膀
local function update_wing(s, dt)
  s.x = Player.x; s.z = Player.z; s.yaw = Player.yaw
  if s.timer >= 1.5 then s.active = false end
end

-- 剑雨
local function update_swordrain(s, dt)
  if s.y > 0.1 then
    s.y = s.y - 5 * dt
  else
    s.y = 0
    if not s.stat then
      s.stat = true
      check_hit_enemies(s, 2.0)
      if SkillSystem.on_camera_hitcam then SkillSystem.on_camera_hitcam(2.0) end
    end
    s.color_a = lerp(s.color_a, 0, dt * 2)
    if s.color_a < 0.05 then s.active = false end
  end
end

-- 炸弹球
local function update_bombsphere(s, dt)
  if s.timer >= 1.2 then s.active = false; return end
  s.scale_x = s.scale_x + 3 * dt
  s.scale_y = s.scale_y + 3 * dt
  s.scale_z = s.scale_z + 3 * dt
  if s.timer < 0.3 then check_hit_enemies(s, 2.0) end
end

-- 防御/攻击增益
local function update_defenceup(s, dt)
  s.x = Player.x; s.z = Player.z
  s.yaw = s.yaw + 60 * dt
end
local function update_attackup(s, dt)
  s.x = Player.x; s.z = Player.z
  s.yaw = s.yaw + 80 * dt
end

-- 毒雾
local function update_poison(s, dt)
  s.x = Player.x; s.z = Player.z
  s.hit_timer = s.hit_timer + dt
  if s.hit_timer >= 0.5 then s.hit_timer = 0; check_hit_enemies(s, 2.0) end
  if s.timer >= 5.0 then s.active = false end
end

-- 死亡之手
local function update_deathhand(s, dt)
  s.x = Player.x; s.z = Player.z; s.yaw = Player.yaw
  s.hit_timer = s.hit_timer + dt
  if s.hit_timer >= 0.3 then s.hit_timer = 0; check_hit_enemies(s, 1.5) end
  if s.timer >= 3.0 then s.active = false end
end

-- 超级剑
local function update_supersword(s, dt)
  local fx, fz = yaw_to_dir(Player.yaw)
  s.x = Player.x + fx * 0.3; s.z = Player.z + fz * 0.3; s.yaw = Player.yaw
  s.hit_timer = s.hit_timer + dt
  if s.hit_timer >= 0.2 then s.hit_timer = 0; check_hit_enemies(s, 1.5) end
  if s.timer >= 2.0 then s.active = false end
end

-- 龙首
local function update_dragonhead(s, dt)
  local fx, fz = yaw_to_dir(Player.yaw)
  s.x = Player.x - fx * 0.2; s.z = Player.z - fz * 0.2; s.yaw = Player.yaw
  s.hit_timer = s.hit_timer + dt
  if s.hit_timer >= 0.3 then s.hit_timer = 0; check_hit_enemies(s, 2.0) end
  if s.timer >= 3.0 then s.active = false end
end

-- 闪电刃
local function update_lightningblade(s, dt)
  s.x = Player.x; s.z = Player.z; s.yaw = Player.yaw
  s.hit_timer = s.hit_timer + dt
  if s.hit_timer >= 0.15 then s.hit_timer = 0; check_hit_enemies(s, 1.5) end
  if s.timer >= 2.0 then s.active = false end
end

-- 快速突刺
local function update_rapidstab(s, dt)
  local fx, fz = yaw_to_dir(Player.yaw)
  s.x = Player.x + fx * 0.25; s.z = Player.z + fz * 0.25; s.yaw = Player.yaw
  s.hit_timer = s.hit_timer + dt
  if s.hit_timer >= 0.15 then s.hit_timer = 0; check_hit_enemies(s, 1.0) end
  if s.timer >= 1.5 then s.active = false end
end

-- 剑气
local function update_swordwind(s, dt)
  s.x = Player.x; s.z = Player.z; s.yaw = Player.yaw
  s.hit_timer = s.hit_timer + dt
  if s.hit_timer >= 0.2 then s.hit_timer = 0; check_hit_enemies(s, 2.0) end
  if s.timer >= 1.0 then s.active = false end
end

-- 旋风斩
local function update_wheelwind(s, dt)
  s.x = Player.x; s.z = Player.z
  s.yaw = s.yaw - 1000 * dt
  s.hit_timer = s.hit_timer + dt
  if s.hit_timer >= 0.2 then s.hit_timer = 0; check_hit_enemies(s, 1.5) end
  if s.timer >= 2.0 then s.active = false end
end

-- 冰矛 (general repeat)
local function update_ice_spear(s, dt)
  if s.timer >= 1.5 then s.active = false; return end
  if s.y < 0 then s.y = s.y + 4 * dt end
  s.hit_timer = s.hit_timer + dt
  if s.hit_timer >= 0.2 then s.hit_timer = 0; check_hit_enemies(s, 0.8) end
end

-- 突击矛 (general repeat)
local function update_rise_spear(s, dt)
  if s.timer >= 1.5 then s.active = false; return end
  if s.y < 0 then s.y = s.y + 5 * dt end
  s.hit_timer = s.hit_timer + dt
  if s.hit_timer >= 0.2 then s.hit_timer = 0; check_hit_enemies(s, 0.8) end
end

-- 幻影 (general repeat)
local function update_mirage(s, dt)
  if s.timer >= 2.0 then s.active = false; return end
  s.hit_timer = s.hit_timer + dt
  if s.hit_timer >= 0.2 then s.hit_timer = 0; check_hit_enemies(s, 1.0) end
end

-- 闪避轮
local function update_edge_wheel(s, dt)
  local fx, fz = yaw_to_dir(s.yaw)
  s.x = s.x + fx * 8 * dt; s.z = s.z + fz * 8 * dt
  s.yaw = s.yaw + 720 * dt
  s.hit_timer = s.hit_timer + dt
  if s.hit_timer >= 0.15 then s.hit_timer = 0; check_hit_enemies(s, 1.2) end
  if s.timer >= 1.5 then s.active = false end
end

-- 箭雨 (general)
local function update_arrowrain(s, dt)
  if s.y > 0.1 then
    s.y = s.y - 8 * dt
  else
    s.y = 0
    if not s.stat then
      s.stat = true
      check_hit_enemies(s, 2.5)
      if SkillSystem.on_camera_hitcam then SkillSystem.on_camera_hitcam(2.0) end
    end
    s.color_a = lerp(s.color_a, 0, dt * 3)
    if s.color_a < 0.05 then s.active = false end
  end
end

-- 通用武将技能实体
local function update_general_combat(s, dt)
  s.x = Player.x; s.z = Player.z
  s.yaw = lerp_angle(s.yaw, Player.yaw, dt * 3)
  s.hit_timer = s.hit_timer + dt
  if s.hit_timer >= s.hit_rate then
    s.hit_timer = 0
    check_hit_enemies(s, s.hit_radius)
  end
  if s.timer >= s.disable_delay then s.active = false end
end

-- 更新函数映射表
local updaters = {
  swordwind = update_swordwind, wheelwind = update_wheelwind,
  rapidstab = update_rapidstab, lightningblade = update_lightningblade,
  spiritsword = update_spiritsword, spiritsword2 = update_spiritsword2,
  spiritsword3 = update_spiritsword3,
  spiritsword_p = update_spiritsword_p, spiritsword_p2 = update_spiritsword_p2,
  spiritsword_p3 = update_spiritsword_p3,
  poison = update_poison, deathhand = update_deathhand,
  supersword = update_supersword, sworddance = update_sworddance,
  junwui = update_junwui, dragonhead = update_dragonhead,
  chosun = update_follow_player, fincanon = update_sk_fincanon,
  energypillar = update_energypillar, bamboo = update_bamboo,
  crow = update_follow_player, bamboobase = update_follow_player,
  swordrain = update_swordrain, swordrain_b = update_swordrain,
  swordrain_s = update_follow_player, wing = update_wing,
  defenceup = update_defenceup, attackup = update_attackup,
  bombsphere = update_bombsphere, groundbreak = update_sk_groundbreak,
  sk_fincanon = update_sk_fincanon, sk_meteo = update_sk_meteo,
  sk_machinegun = update_sk_machinegun, sk_chainbreak = update_sk_chainbreak,
  sk_groundbreak = update_sk_groundbreak, sk_flybug = update_sk_flybug,
  general_spiritsword = update_general_combat,
  general_spinaxe = update_general_combat,
  general_fly = update_follow_player,
  general_sister = update_general_combat,
  general_ice_spear = update_ice_spear,
  general_sworddance = update_sworddance,
  general_axedance = update_follow_player,
  general_arrowrain = update_arrowrain,
  general_risespear = update_rise_spear,
  general_meteo = update_sk_meteo,
  general_mirage = update_mirage,
  general_edge_wheel = update_edge_wheel,
  general_swamp = update_generic_aoe,
  general_multispear = update_generic_periodic,
  general_fincanon = update_sk_fincanon,
  general_flowerfan = update_follow_player,
  general_chain = update_sk_chainbreak,
  general_arrow_poison = update_front_persistent,
  general_rapidthrust = update_front_persistent,
  general_elec_shock = update_generic_aoe,
  jin = update_wing, gather = update_wing, rotfog = update_follow_player,
}

-- ============================================================================
-- Repeatskill — 重复技能 (C# Cha_Skill.Repeatskill)
-- ============================================================================
local function repeatskill(kind, count, delay)
  Player.repeat_skill = true
  Player._repeat_kind = kind
  Player.repeat_time = count
  Player.repeat_delay = delay
  Player._repeat_current_delay = delay
  local fx, fz = pforward()
  local rx, rz = pright()
  local dmg = Player.repeat_atk

  if kind == 11 then
    local px = Player.x + fx * 0.3 + rx * rand_range(-0.2, 0.2)
    local pz = Player.z + fz * 0.3 + rz * rand_range(-0.2, 0.2)
    spawn_skill({
      type = "energypillar", x = px, y = 0.01, z = pz,
      damage = dmg, life = 1.5, sy = 2, hit_rate = 0.3,
      r = 0.8, g = 0.4, b = 1.0,
    })
    if SkillSystem.on_camera_hitcam then SkillSystem.on_camera_hitcam(1.5) end

  elseif kind == 17 then
    local offset = (2 - count) * 0.5
    local px = Player.x + fx * rand_range(0.3, 0.45) + rx * offset
    local pz = Player.z + fz * rand_range(0.3, 0.45) + rz * offset
    spawn_skill({
      type = "bamboo", x = px, y = -1, z = pz,
      yaw = rand_range(0, 360), damage = dmg, life = 2.0,
      sy = 3, r = 0.2, g = 0.8, b = 0.2,
    })
    if SkillSystem.on_camera_hitcam then SkillSystem.on_camera_hitcam(1.5) end

  elseif kind == 25 then
    local angle = ((3 - count) * 45 - Player.yaw + 90)
    local rad = math.rad(angle)
    local px = Player.x + math.cos(rad) * 0.3
    local pz = Player.z + math.sin(rad) * 0.3
    local s = spawn_skill({
      type = "general_ice_spear", x = px, y = -1, z = pz,
      yaw = rand_range(0, 360), damage = dmg, life = 1.5,
      r = 0.4, g = 0.6, b = 1.0,
    })
    local sy = 1 + rand_range(-0.1, 0.5)
    local sc = rand_range(0.7, 1.2)
    s.scale_x = sc; s.scale_y = sy * sc; s.scale_z = sc
    s.origin_sx = sc; s.origin_sy = sy * sc; s.origin_sz = sc

  elseif kind == 29 then
    local fwd = 0.1 + 0.14 * (6 - count)
    local px = Player.x + fx * fwd + rx * rand_range(-0.1, 0.1)
    local pz = Player.z + fz * fwd + rz * rand_range(-0.1, 0.1)
    spawn_skill({
      type = "general_risespear", x = px, y = -1, z = pz,
      yaw = rand_range(0, 360), damage = dmg, life = 1.5,
      r = 0.8, g = 0.8, b = 1.0,
    })

  elseif kind == 31 then
    spawn_skill({
      type = "general_mirage", x = Player.x, y = 0.1, z = Player.z,
      yaw = count * 60, damage = dmg, life = 2.0,
      r = 0.6, g = 0.6, b = 1.0,
    })
  end
end

-- ============================================================================
-- Buff 回调
-- ============================================================================
local function sk_attack_up()
  if SkillSystem.on_reset_atk then SkillSystem.on_reset_atk() end
  Player.attack_up = 0; Player.attack_up_factor = 1.0
end
local function sk_defence_up()
  Player.defence_up = 0; Player.defence_up_factor = 1.0
  if SkillSystem.on_reset_def then SkillSystem.on_reset_def() end
end

-- ============================================================================
-- LaunchSkill — 技能发射 (C# Cha_Skill.LaunchSkill)
-- ============================================================================
local function launch_skill(index)
  local skillatk = Player.skillatk
  local basedamage = Player.basedamage
  local dmg = skillatk * basedamage * 0.01
  local fx, fz = pforward()
  local rx, rz = pright()

  if SkillSystem.on_play_audio then SkillSystem.on_play_audio("jin") end

  -- 玩家技能 0-19
  if index == 0 then
    spawn_skill({ type = "swordwind", x = Player.x, y = 0.06, z = Player.z, yaw = Player.yaw,
      damage = dmg, life = 1.0, hit_rate = 0.2, hit_radius = 2.0, r = 0.9, g = 0.9, b = 0.6 })
    if SkillSystem.on_spawn_swing then SkillSystem.on_spawn_swing(Player.x, 0.06, Player.z, Player.yaw, 3.0, 0.4) end

  elseif index == 1 then
    Player.y = 2.0
    if SkillSystem.on_spawn_swing then SkillSystem.on_spawn_swing(Player.x, 0.1, Player.z, Player.yaw, 4.0, 0.5, "skill") end
    for _, en in ipairs(Entities.enemies) do
      if not en.dead and dist2d(Player.x, Player.z, en.x, en.z) <= 8.0 then
        if SkillSystem.on_hit_enemy then SkillSystem.on_hit_enemy(en, { x = Player.x, z = Player.z, damage = dmg * 1.2, attack_type = "strong" }) end
      end
    end
    if Entities.boss and not Entities.boss.dead and dist2d(Player.x, Player.z, Entities.boss.x, Entities.boss.z) <= 9.0 then
      if SkillSystem.on_hit_boss then SkillSystem.on_hit_boss(Entities.boss, { x = Player.x, z = Player.z, damage = dmg * 1.2, attack_type = "strong" }) end
    end
    if SkillSystem.on_camera_hitcam then SkillSystem.on_camera_hitcam(2.0) end

  elseif index == 2 then
    spawn_skill({ type = "wheelwind", x = Player.x, y = 0.1, z = Player.z, yaw = 180,
      damage = dmg, life = 2.0, hit_rate = 0.2, hit_radius = 1.5, rotate_speed = 1000, r = 0.6, g = 0.8, b = 1.0 })

  elseif index == 3 then
    spawn_skill({ type = "defenceup", x = Player.x, y = 0.1, z = Player.z, life = skillatk, r = 0.3, g = 0.8, b = 0.3 })
    if SkillSystem.on_boom then SkillSystem.on_boom(1, Player.x, Player.z, true) end
    if SkillSystem.on_defence_up then SkillSystem.on_defence_up() end
    Player.defence_up = skillatk; Player.defence_up_factor = 1.5
    add_callback(skillatk, sk_defence_up)

  elseif index == 4 then
    spawn_skill({ type = "rapidstab", x = Player.x + fx * 0.25, y = 0.04, z = Player.z + fz * 0.25,
      yaw = Player.yaw, damage = dmg, life = 1.5, hit_rate = 0.15, hit_radius = 1.0, r = 0.9, g = 0.7, b = 0.3 })

  elseif index == 5 then
    spawn_skill({ type = "lightningblade", x = Player.x, y = 0.05, z = Player.z, yaw = Player.yaw,
      damage = dmg, life = 2.0, hit_rate = 0.15, hit_radius = 1.5, r = 0.8, g = 0.8, b = 1.0 })

  elseif index == 6 then
    spawn_skill({ type = "spiritsword", x = Player.x, y = 0.2, z = Player.z, yaw = Player.yaw,
      damage = dmg, life = 15.0, r = 0.6, g = 0.8, b = 1.0 })

  elseif index == 7 then
    if SkillSystem.on_boom then SkillSystem.on_boom(0, Player.x, Player.z, true) end
    if SkillSystem.on_attack_up then SkillSystem.on_attack_up(1.5) end
    Player.attack_up = skillatk; Player.attack_up_factor = 1.5
    add_callback(skillatk, sk_attack_up)

  elseif index == 8 then
    local mass = dmg; if mass < 1 then mass = 1 end
    spawn_skill({ type = "poison", x = Player.x, y = 0.22, z = Player.z,
      damage = mass, life = 5.0, hit_rate = 0.5, hit_radius = 2.0, r = 0.3, g = 0.8, b = 0.2 })

  elseif index == 9 then
    spawn_skill({ type = "deathhand", x = Player.x, y = 0.05, z = Player.z, yaw = Player.yaw,
      damage = skillatk, life = 3.0, hit_rate = 0.3, hit_radius = 1.5, r = 0.5, g = 0.2, b = 0.3 })

  elseif index == 10 then
    spawn_skill({ type = "supersword", x = Player.x + fx * 0.3, y = 0.1, z = Player.z + fz * 0.3,
      yaw = Player.yaw, damage = dmg, life = 2.0, hit_rate = 0.2, hit_radius = 1.5, r = 1.0, g = 0.8, b = 0.3 })

  elseif index == 11 then
    if SkillSystem.on_invincibility then SkillSystem.on_invincibility(5.0) end
    Player.repeat_atk = dmg
    repeatskill(11, 5, 0.8)

  elseif index == 12 then
    local s = spawn_skill({ type = "sworddance", x = Player.x, y = 0.05, z = Player.z,
      yaw = dir_to_yaw(-fx + rx, -fz + rz), damage = dmg, life = 5.0, hit_rate = 0.2, r = 0.9, g = 0.9, b = 0.6 })
    s.uv_speed = 1.0
    if SkillSystem.on_disappear then SkillSystem.on_disappear() end
    if SkillSystem.on_stop_control then SkillSystem.on_stop_control() end
    add_callback(0.5, function()
      if SkillSystem.on_appear then SkillSystem.on_appear() end
      if SkillSystem.on_start_control then SkillSystem.on_start_control() end
    end)

  elseif index == 13 then
    if SkillSystem.on_invincibility then SkillSystem.on_invincibility(4.0) end
    local s = spawn_skill({ type = "junwui", x = Player.x + fx * 0.2, y = 0.1, z = Player.z + fz * 0.2,
      yaw = Player.yaw, damage = dmg, life = 10.0, sx = 1.5, sy = 1.5, sz = 1.5, r = 0.8, g = 0.6, b = 0.3 })
    s.rndpos_x = Player.x; s.rndpos_z = Player.z
    if SkillSystem.on_camera_look then SkillSystem.on_camera_look(s, 25, 0.7) end

  elseif index == 14 then
    if SkillSystem.on_invincibility then SkillSystem.on_invincibility(4.0) end
    spawn_skill({ type = "dragonhead", x = Player.x - fx * 0.2, y = 0.1, z = Player.z - fz * 0.2,
      yaw = Player.yaw, damage = dmg, life = 3.0, hit_rate = 0.3, hit_radius = 2.0, r = 0.6, g = 0.3, b = 0.1 })

  elseif index == 15 then
    if SkillSystem.on_invincibility then SkillSystem.on_invincibility(4.0) end
    local s = spawn_skill({ type = "chosun", x = Player.x + fx * 0.2, y = 0.1, z = Player.z + fz * 0.2,
      yaw = Player.yaw, damage = dmg, life = 4.0, hit_rate = 0.3, hit_radius = 2.0, r = 0.9, g = 0.7, b = 0.2 })
    if SkillSystem.on_camera_look then SkillSystem.on_camera_look(s, 28, 0.5) end

  elseif index == 16 then
    if SkillSystem.on_play_anim then SkillSystem.on_play_anim("skill03") end

  elseif index == 17 then
    spawn_skill({ type = "crow", x = Player.x + fx * 0.35, y = 0.3, z = Player.z + fz * 0.35,
      yaw = Player.yaw, damage = 0, life = 5.0, r = 0.2, g = 0.2, b = 0.2 })
    spawn_skill({ type = "bamboobase", x = Player.x + fx * 0.35, y = 0.01, z = Player.z + fz * 0.35,
      yaw = Player.yaw, damage = 0, life = 5.0, r = 0.3, g = 0.5, b = 0.2 })
    Player.repeat_atk = dmg
    repeatskill(17, 3, 0.2)
    add_callback(3.5, function()
      for _, s in ipairs(active) do
        if s.type == "crow" or s.type == "bamboobase" then s.active = false end
      end
    end)

  elseif index == 18 then
    spawn_skill({ type = "swordrain_s", x = Player.x, y = 0.1, z = Player.z, damage = 0, life = 3.0, r = 0.6, g = 0.6, b = 0.8 })
    spawn_skill({ type = "swordrain", x = Player.x, y = 1.0, z = Player.z, yaw = Player.yaw, damage = dmg, life = 3.0, r = 0.8, g = 0.8, b = 1.0 })
    spawn_skill({ type = "swordrain_b", x = Player.x, y = -0.1, z = Player.z, yaw = Player.yaw, damage = dmg, life = 3.0, r = 0.5, g = 0.5, b = 0.7 })

  elseif index == 19 then
    spawn_skill({ type = "wing", x = Player.x, y = 0.1, z = Player.z, yaw = Player.yaw, damage = 0, life = 1.5, r = 1.0, g = 0.9, b = 0.5 })

  -- 武将技能 21-40
  elseif index == 21 then
    spawn_skill({ type = "general_spiritsword", x = Player.x, y = 0.2, z = Player.z, yaw = Player.yaw,
      damage = skillatk, life = 5.0, hit_rate = 0.3, hit_radius = 2.0, r = 0.6, g = 0.8, b = 1.0 })
  elseif index == 22 then
    spawn_skill({ type = "general_spinaxe", x = Player.x + fx * 0.2, y = 0.1, z = Player.z + fz * 0.2,
      yaw = Player.yaw, damage = skillatk, life = 3.0, hit_rate = 0.2, hit_radius = 1.5, r = 0.8, g = 0.5, b = 0.2 })
  elseif index == 23 then
    spawn_skill({ type = "general_fly", x = Player.x, y = 0.1, z = Player.z,
      damage = skillatk, life = 4.0, hit_rate = 0.3, hit_radius = 1.5, r = 0.4, g = 0.6, b = 1.0 })
  elseif index == 24 then
    spawn_skill({ type = "general_sister", x = Player.x, y = 0.1, z = Player.z, yaw = Player.yaw,
      damage = skillatk, life = 4.0, hit_rate = 0.3, hit_radius = 1.5, r = 0.9, g = 0.5, b = 0.6 })
  elseif index == 25 then
    Player.repeat_atk = skillatk
    repeatskill(25, 5, 0.12)
  elseif index == 26 then
    local s = spawn_skill({ type = "general_sworddance", x = Player.x, y = 0.1, z = Player.z,
      damage = skillatk, life = 3.0, hit_rate = 0.2, r = 0.9, g = 0.9, b = 0.6 })
    s.uv_speed = 1.0
  elseif index == 27 then
    spawn_skill({ type = "general_axedance", x = Player.x, y = 0.1, z = Player.z, yaw = Player.yaw,
      damage = skillatk, life = 3.0, hit_rate = 0.2, hit_radius = 2.0, r = 0.7, g = 0.4, b = 0.2 })
  elseif index == 28 then
    spawn_skill({ type = "general_arrowrain", x = Player.x, y = 1.0, z = Player.z, yaw = Player.yaw,
      damage = skillatk, life = 3.0, r = 0.6, g = 0.6, b = 0.8 })
    spawn_skill({ type = "general_arrowrain", x = Player.x + fx * 0.35, y = -0.03, z = Player.z + fz * 0.35,
      yaw = Player.yaw, damage = skillatk, life = 3.0, r = 0.5, g = 0.5, b = 0.7 })
  elseif index == 29 then
    Player.repeat_atk = skillatk
    repeatskill(29, 6, 0.1)
  elseif index == 30 then
    spawn_skill({ type = "general_meteo", x = Player.x + 4.2 - fx * 5.5, y = 4.2, z = Player.z - fz * 5.5,
      yaw = Player.yaw, damage = skillatk, life = 5.0, r = 0.9, g = 0.5, b = 0.2 })
  elseif index == 31 then
    Player.repeat_atk = skillatk
    repeatskill(31, 6, 0.05)
    if SkillSystem.on_disappear then SkillSystem.on_disappear() end
    if SkillSystem.on_stop_control then SkillSystem.on_stop_control() end
    add_callback(1.0, function()
      if SkillSystem.on_appear then SkillSystem.on_appear() end
      if SkillSystem.on_start_control then SkillSystem.on_start_control() end
    end)
  elseif index == 32 then
    if SkillSystem.on_play_anim then SkillSystem.on_play_anim("dodge") end
    Player.x = Player.x + fx * 0.5
    spawn_skill({ type = "general_edge_wheel", x = Player.x + fx * 0.2, y = 0.1, z = Player.z + fz * 0.2,
      yaw = Player.yaw, damage = skillatk, life = 1.5, hit_rate = 0.15, hit_radius = 1.2, r = 0.8, g = 0.8, b = 1.0 })
  elseif index == 33 then
    spawn_skill({ type = "general_swamp", x = Player.x + fx * 0.5, y = 0.01, z = Player.z + fz * 0.5,
      damage = skillatk, life = 3.0, hit_radius = 2.0, start_delay = 0.1, r = 0.3, g = 0.4, b = 0.2 })
  elseif index == 34 then
    spawn_skill({ type = "general_multispear", x = Player.x, y = 0.1, z = Player.z, yaw = Player.yaw,
      damage = skillatk, life = 3.0, hit_rate = 0.2, hit_radius = 1.5, r = 0.6, g = 0.7, b = 1.0 })
  elseif index == 35 then
    spawn_skill({ type = "general_fincanon", x = Player.x, y = 0.1, z = Player.z, yaw = Player.yaw,
      damage = skillatk, life = 4.0, sx = 1.5, sy = 1.5, sz = 1.5, r = 0.8, g = 0.6, b = 0.3 })
    if SkillSystem.on_invincibility then SkillSystem.on_invincibility(3.0) end
  elseif index == 36 then
    spawn_skill({ type = "general_flowerfan", x = Player.x, y = 0.1, z = Player.z, yaw = Player.yaw,
      damage = skillatk, life = 3.0, hit_rate = 0.2, hit_radius = 2.0, r = 0.9, g = 0.4, b = 0.6 })
  elseif index == 37 then
    spawn_skill({ type = "general_chain", x = Player.x, y = -0.1, z = Player.z,
      damage = skillatk, life = 4.0, r = 0.5, g = 0.5, b = 0.5 })
  elseif index == 38 then
    spawn_skill({ type = "general_arrow_poison", x = Player.x + fx * 0.05, y = 0.07, z = Player.z + fz * 0.05,
      yaw = Player.yaw, damage = skillatk, life = 2.0, hit_rate = 0.2, hit_radius = 1.0, front_offset = 0.1, r = 0.3, g = 0.8, b = 0.2 })
  elseif index == 39 then
    spawn_skill({ type = "general_rapidthrust", x = Player.x + fx * 0.15, y = 0.05, z = Player.z + fz * 0.15,
      yaw = Player.yaw, damage = skillatk, life = 2.0, hit_rate = 0.15, hit_radius = 1.0, front_offset = 0.15, r = 0.9, g = 0.7, b = 0.3 })
  elseif index == 40 then
    spawn_skill({ type = "general_elec_shock", x = Player.x + fx * 0.22, y = 0.06, z = Player.z + fz * 0.22,
      yaw = Player.yaw, damage = skillatk, life = 2.0, hit_radius = 1.5, start_delay = 0.1, r = 0.8, g = 0.8, b = 1.0 })
  end

  G.combo = G.combo + 2
  G.combo_timer = 2.0
  Player.skill_index = -1
end

-- ============================================================================
-- SkillOn — 施法开始 (C# Cha_Skill.SkillOn)
-- ============================================================================
local function skill_on(index, is_general)
  if SkillSystem.on_skill_start then SkillSystem.on_skill_start() end
  Player.chamovestat = 180

  local skill_index, skillatk, motionkind
  if not is_general then
    local grade = math.max(0, Player.skill_grades[index] or 0)
    local skill = DB.DB_Skill[index] and DB.DB_Skill[index][grade]
    if not skill then return end
    motionkind = skill.kind
    skillatk = skill.attackpoint
    skill_index = index
  else
    motionkind = Player._g_motionkind or 3
    skillatk = Player._g_skillatk or 50
    skill_index = index + 21
  end

  Player.skill_index = skill_index
  Player.skillatk = skillatk
  Player.motionkind = motionkind
  Player.casting = true
  Player.casting_delay = 0.18

  if SkillSystem.on_spawn_effect then SkillSystem.on_spawn_effect(Player.x, 0.5, Player.z, 10, "skill") end
  if SkillSystem.on_play_audio then SkillSystem.on_play_audio("skill"); SkillSystem.on_play_audio("timewoosh") end

  G.time_scale = 0.1
  G.time_scale_timer = 0.5

  if motionkind == 1 then
    if SkillSystem.on_play_anim then SkillSystem.on_play_anim("cast1") end
  elseif motionkind == 2 then
    if SkillSystem.on_camera_zoom then SkillSystem.on_camera_zoom(20, 16, 0.3) end
    if SkillSystem.on_play_anim then SkillSystem.on_play_anim("cast2") end
    if skill_index == 13 or skill_index == 15 then
      if SkillSystem.on_play_audio then SkillSystem.on_play_audio("jin") end
      spawn_skill({ type = "jin", x = Player.x + pforward() * 0.2, y = 0.01, z = Player.z, life = 1.0, r = 1.0, g = 0.8, b = 0.3 })
    elseif skill_index == 19 then
      spawn_skill({ type = "wing", x = Player.x, y = 0.1, z = Player.z, yaw = Player.yaw, life = 1.0, r = 1.0, g = 0.9, b = 0.5 })
    end
  elseif motionkind == 3 then
    if SkillSystem.on_play_anim then SkillSystem.on_play_anim("cast3") end
    if skill_index == 8 then
      spawn_skill({ type = "gather", x = Player.x, y = 0.22, z = Player.z, life = 0.5, r = 0.8, g = 0.6, b = 0.3 })
    elseif skill_index == 9 then
      spawn_skill({ type = "gather", x = Player.x, y = 0.22, z = Player.z, life = 0.5, r = 0.3, g = 0.6, b = 0.8 })
    end
  elseif motionkind == 4 then
    if SkillSystem.on_play_anim then SkillSystem.on_play_anim("cast4") end
    if skill_index == 16 then
      spawn_skill({ type = "fincanon", x = Player.x, y = 0.1, z = Player.z, yaw = Player.yaw,
        damage = skillatk * Player.basedamage * 0.01, life = 4.0, sx = 1.5, sy = 1.5, sz = 1.5, r = 0.8, g = 0.6, b = 0.3 })
    end
  end
end

-- ============================================================================
-- DelaySkill — 延迟技能 (C# Cha_Skill.DelaySkill)
-- ============================================================================
local function delay_skill(index)
  local dmg = Player.skillatk * Player.basedamage * 0.01
  spawn_skill({ type = "groundbreak", x = Player.x, y = 0.01, z = Player.z,
    damage = dmg, life = 2.0, r = 0.6, g = 0.4, b = 0.2 })

  if index == 2 then
    local s = active[#active]
    if s then s.scale_x = 0.5; s.scale_y = 0.5; s.scale_z = 0.5 end
    if SkillSystem.on_camera_hitcam then SkillSystem.on_camera_hitcam(2.0) end
  elseif index == 19 then
    local s = active[#active]
    if s then s.scale_x = 1; s.scale_y = 1; s.scale_z = 1 end
    if SkillSystem.on_camera_hitcam then SkillSystem.on_camera_hitcam(4.0) end
    G.time_scale = 0.6
    G.time_scale_timer = 0.5
    add_callback(0.5, function()
      G.time_scale = 1.0; G.time_scale_timer = 0
      if SkillSystem.on_boom then SkillSystem.on_boom(0, Player.x, Player.z, false) end
    end)
    spawn_skill({ type = "bombsphere", x = Player.x, y = 0.1, z = Player.z,
      damage = dmg, life = 1.2, r = 1.0, g = 0.6, b = 0.2 })
  end
end

-- BoomOn
local function boom_on(index, collider)
  if SkillSystem.on_boom then SkillSystem.on_boom(index, Player.x, Player.z, collider) end
end

-- PetSkillOn
local function pet_skill_on(index)
  spawn_skill({ type = "rotfog", x = Player.x, y = 0.1, z = Player.z, life = 3.0, r = 0.5, g = 0.5, b = 0.8 })
  if SkillSystem.on_camera_zoom then SkillSystem.on_camera_zoom(5, 15, 1.0) end
  if index == 0 then
    Player.pet_ing = true; Player.currentPet = 0; Player.movespeed = 0.8
    if SkillSystem.on_play_audio then SkillSystem.on_play_audio("horse") end
  elseif index == 1 then
    Player.pet_ing = true; Player.currentPet = 1
  end
end

-- SetGeneralSkill
local function set_general_skill(maxatk, skillatk, motionkind)
  Player._g_skillatk = math.max(math.floor(maxatk * skillatk * 0.01), 1)
  Player._g_motionkind = motionkind
end

-- SetBaseDamage
local function set_base_damage(dmg)
  Player.basedamage = dmg
end

-- PlayerSkill — 玩家施放技能入口
local function player_skill()
  if Player.casting then return end
  if Player.chamovestat < -1 or Player.chamovestat > 50 then return end
  local slot = Player.current_skill_slot
  local set = Player.skill_slots[slot]
  if not set then return end
  local grade = Player.skill_grades[set] or 0
  local skill = DB.DB_Skill[set] and DB.DB_Skill[set][grade]
  if not skill then return end
  if Player.skill_cd[set] and Player.skill_cd[set] > 0 then return end
  local sp_cost = 20 + grade * 10
  if skill.soulprice and skill.soulprice > 0 then
    if G.soul < skill.soulprice then return end
    G.soul = G.soul - skill.soulprice
    sp_cost = 0
  end
  if Player.sp < sp_cost then return end
  if SkillSystem.on_sp_charge then SkillSystem.on_sp_charge(-sp_cost) end
  Player.skill_cd[set] = skill.cooltime
  skill_on(set, false)
end

-- ============================================================================
-- 主更新函数
-- ============================================================================
local function update(dt)
  -- 1. 施法计时 (C# Update: castingdelay > 0.18f)
  if Player.casting and Player.skill_index ~= -1 then
    Player.casting_delay = Player.casting_delay - dt
    if Player.casting_delay <= 0 then
      -- 恢复时间流速 (C# drag=5, timeScale=1)
      G.time_scale = 1.0
      G.time_scale_timer = 0
      -- 停止施法动画
      if SkillSystem.on_stop_anim then SkillSystem.on_stop_anim() end
      -- 发射技能
      launch_skill(Player.skill_index)
      Player.casting = false
      Player.chamovestat = 0
      Player.visual_state = "idle"
    end
  end

  -- 2. 重复技能计时 (C# Update: repeat && currentdelay > 0)
  if Player.repeat_skill and Player._repeat_current_delay and Player._repeat_current_delay > 0 then
    Player._repeat_current_delay = Player._repeat_current_delay - dt
    if Player._repeat_current_delay <= 0 then
      Player.repeat_time = Player.repeat_time - 1
      if Player.repeat_time > 0 then
        repeatskill(Player._repeat_kind, Player.repeat_time, Player.repeat_delay)
      else
        Player.repeat_skill = false
      end
    end
  end

  -- 3. 定时回调
  update_callbacks(dt)

  -- 4. 更新所有技能实体
  for i = #active, 1, -1 do
    local s = active[i]
    s.timer = s.timer + dt

    -- 创建 ECS 实体 (延迟)
    if s.active then
      ensure_entity(s, s.model)
    end

    -- 执行类型特定更新
    local fn = updaters[s.type]
    if fn then
      fn(s, dt)
    else
      -- 默认: 持续伤害 + 定时消失
      s.hit_timer = s.hit_timer + dt
      if s.hit_timer >= (s.hit_rate or 0.3) then
        s.hit_timer = 0
        check_hit_enemies(s, s.hit_radius or 1.0)
      end
      if s.timer >= (s.disable_delay or s.life or 3.0) then
        s.active = false
      end
    end

    -- 同步 ECS
    sync_entity(s)

    -- 更新子实体
    if s.children then
      for _, c in ipairs(s.children) do
        if c.active then
          c.timer = c.timer + dt
          ensure_entity(c, c.model)
          local cfn = updaters[c.type]
          if cfn then cfn(c, dt) end
          sync_entity(c)
          if not c.active or c.timer >= c.life then
            if c.e then kill_entity(c.e); c.e = nil end
          end
        end
      end
    end

    -- 过期清理
    if not s.active or s.timer >= s.life then
      release(s)
      table.remove(active, i)
    end
  end
end

-- 清空所有技能实体
local function clear()
  for i = #active, 1, -1 do
    release(active[i])
    table.remove(active, i)
  end
  callbacks = {}
  Player.casting = false
  Player.skill_index = -1
  Player.repeat_skill = false
end

local function count() return #active end

-- ============================================================================
-- 模块导出
-- ============================================================================
local SkillSystem = {
  spawn_skill       = spawn_skill,
  skill_on          = skill_on,
  launch_skill      = launch_skill,
  delay_skill       = delay_skill,
  boom_on           = boom_on,
  pet_skill_on      = pet_skill_on,
  repeatskill       = repeatskill,
  set_general_skill = set_general_skill,
  set_base_damage   = set_base_damage,
  player_skill      = player_skill,
  update            = update,
  clear             = clear,
  count             = count,
  active            = active,
  -- 回调 (main.lua 注入)
  on_hit_enemy      = nil,
  on_hit_boss       = nil,
  on_invincibility  = nil,
  on_attack_up      = nil,
  on_defence_up     = nil,
  on_reset_atk      = nil,
  on_reset_def      = nil,
  on_stop_control   = nil,
  on_start_control  = nil,
  on_disappear      = nil,
  on_appear         = nil,
  on_skill_start    = nil,
  on_camera_zoom    = nil,
  on_camera_hitcam  = nil,
  on_camera_look    = nil,
  on_spawn_effect   = nil,
  on_spawn_swing    = nil,
  on_play_anim      = nil,
  on_stop_anim      = nil,
  on_play_audio     = nil,
  on_boom           = nil,
  on_sp_charge      = nil,
}

_G.SkillSystem = SkillSystem
return SkillSystem