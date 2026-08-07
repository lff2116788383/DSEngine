-- ============================================================================
-- ai_system.lua — AI 辅助系统 (武将 + 天使)
-- 1:1 移植自:
--   AI_General.cs  (401 行) — 武将召唤体: 跟随/游荡/攻击/受伤/死亡
--   AI_Asist.cs    (158 行) — 天使辅助: 跟随/锁定/射击/溅射
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
local function lerp_angle(a, b, t)
  local diff = b - a
  while diff > 180 do diff = diff - 360 end
  while diff < -180 do diff = diff + 360 end
  return a + diff * t
end

-- ============================================================================
-- 武将系统 (AI_General.cs)
-- ============================================================================
local general = nil  -- 武将实体数据

local function spawn_general(kind)
  if general then return general end
  local gdata = DB.DB_General[kind] or DB.DB_General[0]
  if not gdata then return nil end

  general = {
    e = nil,
    kind = gdata.kind,
    name = gdata.name,
    scale = gdata.scale or 1.0,
    x = Player.x,
    y = 0,
    z = Player.z,
    yaw = Player.yaw,
    -- 属性
    maxhp = gdata.maxhp,
    hp = gdata.maxhp,
    power = 1.0 + gdata.atk * 0.5,
    defence = 1 + gdata.def,
    runspeed = 0.3,
    atkspeed = gdata.atkspd,
    dash = 80,
    -- 状态
    life = true,
    disable = false,
    superarmor = false,
    call = false,
    attack_start = false,
    showme = false,
    -- 计时器
    delay_invicibility = 0,
    delay_call = 0,
    setdir_timer = 0,
    -- 移动
    rndpos_x = Player.x,
    rndpos_z = Player.z,
    direction_x = 0,
    direction_z = 0,
    look_yaw = Player.yaw,
    -- 状态机
    generalmovestat = 0,  -- 0=idle/move, 1=attack, 2=attack_i
    -- 攻击
    atkcount = -1,
    grade = 0,
    unique = -1,
    -- 视觉
    t = 0,
    visual_state = "idle",
    -- HP 条
    hp_bar_e = nil,
  }

  -- 创建 ECS 实体
  general.e = dse.ecs.create_entity()
  dse.ecs.add_transform(general.e, general.x, general.y, general.z,
    general.scale, general.scale, general.scale)
  pcall(dse.ecs.mesh_renderer_add, general.e, "assets/models/ball.glb")
  dse.ecs.set_mesh_shader_variant(general.e, "MESH_LIT")
  dse.ecs.set_mesh_color(general.e, 0.8, 0.6, 0.3, 1.0)

  -- HP 条
  general.hp_bar_e = dse.ecs.create_entity()
  dse.ecs.add_transform(general.hp_bar_e, general.x, 2.5, general.z, 0.6, 0.1, 0.6)
  local verts = {-0.5,0,0, 0.5,0,0, 0.5,0,0.1, -0.5,0,0.1}
  local idx = {0,1,2, 2,3,0}
  dse.ecs.add_mesh_renderer(general.hp_bar_e, 0.2, 0.8, 0.2, 0.9, verts, idx)
  dse.ecs.set_mesh_shader_variant(general.hp_bar_e, "MESH_UNLIT")

  -- 设置武将属性到 Player
  Player.general = true
  Player.general_kind = gdata.kind
  Player.general_maxhp = gdata.maxhp
  Player.general_hp = gdata.maxhp
  Player.general_atk = gdata.atk
  Player.general_def = gdata.def
  Player.general_atkspd = gdata.atkspd

  return general
end

local function despawn_general()
  if not general then return end
  if general.e then kill_entity(general.e); general.e = nil end
  if general.hp_bar_e then kill_entity(general.hp_bar_e); general.hp_bar_e = nil end
  general = nil
  Player.general = false
end

-- SetRndPosition (C# InvokeRepeating 1s)
local function general_set_rnd_position()
  if not general or not general.life then return end
  -- 随机位置 (C# Random.onUnitSphere * 0.2 + cha1.position + cha1.forward * 0.3)
  local fx, fz = yaw_to_dir(Player.yaw)
  local angle = math.random() * math.pi * 2
  general.rndpos_x = Player.x + fx * 0.3 + math.cos(angle) * 0.2
  general.rndpos_z = Player.z + fz * 0.3 + math.sin(angle) * 0.2

  local d = dist2d(general.x, general.z, Player.x, Player.z)
  if d > 0.35 then
    general.showme = true
    if general.generalmovestat ~= 0 then
      general.generalmovestat = 0
    end
    general.visual_state = "move"
  else
    general.showme = false
    if general.generalmovestat ~= 0 then
      general.generalmovestat = 0
    end
    general.visual_state = "idle"
  end

  -- 边界限制 (C# |x| > 2.6, |z| > 2.5)
  if math.abs(general.x) > 2.6 then
    general.x = 2.6 * (general.x > 0 and 1 or -1)
  end
  if math.abs(general.z) > 2.5 then
    general.z = 2.5 * (general.z > 0 and 1 or -1)
  end
end

-- Damaged (C# AI_General.Damaged)
local function general_damaged(damage, dir_x, dir_z)
  if not general or general.superarmor or not general.life then return end
  general.direction_x = dir_x
  general.direction_z = dir_z
  local actual_dmg = math.max(1, damage - general.defence)
  general.hp = general.hp - actual_dmg
  general.delay_invicibility = 3.5
  general.superarmor = true
  general.visual_state = "down"

  if general.hp <= 0 then
    general.hp = 1
    general.delay_invicibility = 0
    general.life = false
    general.visual_state = "dead"
    if AISystem.on_general_dead then AISystem.on_general_dead() end
  end
end

-- HPfull (C# AI_General.HPfull)
local function general_hpfull()
  if not general then return end
  general.life = true
  general.superarmor = false
  general.disable = false
  general.hp = general.maxhp
  general.x = Player.x
  general.z = Player.z
  general.visual_state = "idle"
  general.generalmovestat = 0
end

-- AttackOn (C# AI_General.AttackOn)
local function general_attack_on(enemy_x, enemy_z)
  if not general or not general.life or general.showme or general.call then return end
  if general.attack_start then return end

  -- C# atkcount = (atkcount + 1) % (5 - grade)
  if general.unique >= 0 then
    general.atkcount = (general.atkcount + 1) % (5 - general.grade)
  end

  general.direction_x = enemy_x - general.x
  general.direction_z = enemy_z - general.z
  local mag = math.sqrt(general.direction_x * general.direction_x + general.direction_z * general.direction_z)
  if mag > 0.001 then
    general.direction_x = general.direction_x / mag
    general.direction_z = general.direction_z / mag
  end
  general.look_yaw = dir_to_yaw(general.direction_x, general.direction_z)
  general.yaw = general.look_yaw
  general.attack_start = true
  general.generalmovestat = 1
  general.visual_state = "m_attack1"
end

-- Update (C# AI_General.Update)
local function update_general(dt)
  if not general then return end
  general.t = general.t + dt

  -- 同步 Player 状态
  Player.general_hp = general.hp
  Player.general_maxhp = general.maxhp

  if general.disable then return end

  if not general.life then
    general.delay_invicibility = general.delay_invicibility + dt
    if general.delay_invicibility > 2.0 then
      general.delay_invicibility = 0
      general.y = general.y + 14 * dt  -- 升空消失
      general.disable = true
      if general.e then dse.ecs.set_mesh_visible(general.e, false) end
    end
    return
  end

  -- 超级霸体恢复
  if general.superarmor then
    general.delay_invicibility = general.delay_invicibility - dt
    if general.delay_invicibility <= 0 then
      general.superarmor = false
    end
  end

  -- 召唤计时
  if general.call then
    general.delay_call = general.delay_call + dt
    if general.delay_call > 3.0 then
      general.delay_call = 0
      general.call = false
    end
  end

  -- SetRndPosition (每 1 秒)
  general.setdir_timer = general.setdir_timer - dt
  if general.setdir_timer <= 0 then
    general.setdir_timer = 1.0
    general_set_rnd_position()
  end

  -- 状态机 (C# myanimation.IsPlaying 检查)
  if general.visual_state == "down" then
    general.attack_start = true
    -- 倒地恢复
    if general.superarmor == false and general.delay_invicibility <= 0 then
      general.visual_state = "idle"
      general.generalmovestat = 0
    end
  elseif general.visual_state == "m_attack1_i" then
    -- 攻击冲击阶段 (C# m_attack1_i)
    if general.generalmovestat ~= 2 then
      general.generalmovestat = 2
      -- 武器冲击 (C# clone_weapon Instantiate)
      local dmg = general.power
      if general.atkcount ~= 0 then
        -- 普通攻击
        if AISystem.on_general_attack then
          AISystem.on_general_attack(general.x, general.z, general.yaw, dmg)
        end
      else
        -- 特殊攻击 (sp_selweapon)
        if AISystem.on_general_special then
          AISystem.on_general_special(general.x, general.z, general.yaw, dmg * 1.5)
        end
      end
      if AISystem.on_general_hitcam then AISystem.on_general_hitcam() end
    end
    -- 回到 idle
    if general.t % 0.3 < dt then
      general.visual_state = "idle"
      general.generalmovestat = 0
      general.attack_start = false
    end

  elseif general.visual_state == "m_attack1" then
    -- 攻击阶段 (C# m_attack1)
    if general.generalmovestat ~= 1 then
      general.generalmovestat = 1
      -- 冲刺 (C# rigidbody.AddForce(directionVector * dash))
      general.x = general.x + general.direction_x * 0.3
      general.z = general.z + general.direction_z * 0.3
      if general.atkcount == 0 then
        general.delay_invicibility = 1.5
        general.superarmor = true
      end
    end
    -- 转向
    general.yaw = lerp_angle(general.yaw, general.look_yaw, dt * 6)
    -- 进入冲击阶段
    if general.t % 0.25 < dt then
      general.visual_state = "m_attack1_i"
    end

  elseif general.visual_state == "move" then
    -- 移动阶段 (C# move)
    if general.generalmovestat ~= 0 then
      general.generalmovestat = 0
    end
    general.attack_start = false
    -- 朝玩家移动 (C# directionVector = cha1.position - mytransform.position)
    general.direction_x = Player.x - general.x
    general.direction_z = Player.z - general.z
    local mag = math.sqrt(general.direction_x * general.direction_x + general.direction_z * general.direction_z)
    if mag > 0.001 then
      general.direction_x = general.direction_x / mag
      general.direction_z = general.direction_z / mag
    end
    general.look_yaw = dir_to_yaw(general.direction_x, general.direction_z)
    general.yaw = lerp_angle(general.yaw, general.look_yaw, dt * 5)
    -- 移动 (C# mytransform.position += forward * Time.deltaTime * (runspeed - 0.1))
    general.x = general.x + general.direction_x * dt * (general.runspeed - 0.1) * 10
    general.z = general.z + general.direction_z * dt * (general.runspeed - 0.1) * 10

  elseif general.visual_state == "idle" then
    -- 待机阶段 (C# idle)
    if general.generalmovestat ~= 0 then
      general.generalmovestat = 0
    end
    general.attack_start = false
    -- 朝随机位置移动 (C# directionVector = rndpos - mytransform.position)
    general.direction_x = general.rndpos_x - general.x
    general.direction_z = general.rndpos_z - general.z
    local mag = math.sqrt(general.direction_x * general.direction_x + general.direction_z * general.direction_z)
    if mag > 0.001 then
      general.direction_x = general.direction_x / mag
      general.direction_z = general.direction_z / mag
    end
    general.look_yaw = dir_to_yaw(general.direction_x, general.direction_z)
    general.yaw = lerp_angle(general.yaw, general.look_yaw, dt * 2)
    -- 缓慢移动 (C# forward * Time.deltaTime * 0.1)
    general.x = general.x + general.direction_x * dt * 0.1
    general.z = general.z + general.direction_z * dt * 0.1

    -- 检测附近敌人 → 攻击 (C# OnTriggerEnter layer==8)
    for _, en in ipairs(Entities.enemies) do
      if not en.dead and en.monmovestat and en.monmovestat > 0 then
        if dist2d(general.x, general.z, en.x, en.z) < 1.0 then
          general_attack_on(en.x, en.z)
          break
        end
      end
    end
    if Entities.boss and not Entities.boss.dead then
      if dist2d(general.x, general.z, Entities.boss.x, Entities.boss.z) < 1.5 then
        general_attack_on(Entities.boss.x, Entities.boss.z)
      end
    end
  end

  -- 同步 ECS
  if general.e then
    dse.ecs.set_transform_position(general.e, general.x, general.y, general.z)
    dse.ecs.set_transform_rotation(general.e, 0, general.yaw, 0)
    local sx, sy, sz = general.scale, general.scale, general.scale
    local r, g, b, a = 0.8, 0.6, 0.3, 1.0
    if general.visual_state == "m_attack1" then
      sy = general.scale * 1.1; sx = general.scale * 0.9
    elseif general.visual_state == "m_attack1_i" then
      sy = general.scale * 0.85; sx = general.scale * 1.2
      sx = sx * (1.0 + 0.15 * math.sin(general.t * 30))
    elseif general.visual_state == "down" or general.visual_state == "dead" then
      sy = general.scale * 0.3; sx = general.scale * 1.5
    elseif general.visual_state == "move" then
      sx = general.scale * (1.0 + 0.06 * math.sin(general.t * 12))
    elseif general.visual_state == "idle" then
      sy = general.scale * (1.0 + 0.02 * math.sin(general.t * 4))
    end
    if not general.life then
      a = 1.0 - general.delay_invicibility / 2.0
      if a < 0 then a = 0 end
    end
    dse.ecs.set_transform_scale(general.e, sx, sy, sz)
    dse.ecs.set_mesh_color(general.e, r, g, b, a)
  end

  -- HP 条
  if general.hp_bar_e then
    local hp_ratio = general.hp / general.maxhp
    dse.ecs.set_transform_position(general.hp_bar_e, general.x, 2.5, general.z)
    dse.ecs.set_transform_scale(general.hp_bar_e, 0.6 * hp_ratio, 0.1, 0.6)
    if hp_ratio > 0.5 then
      dse.ecs.set_mesh_color(general.hp_bar_e, 0.2, 0.8, 0.2, 0.9)
    elseif hp_ratio > 0.25 then
      dse.ecs.set_mesh_color(general.hp_bar_e, 0.8, 0.8, 0.2, 0.9)
    else
      dse.ecs.set_mesh_color(general.hp_bar_e, 0.8, 0.2, 0.2, 0.9)
    end
    if not general.life then
      dse.ecs.set_mesh_visible(general.hp_bar_e, false)
    end
  end
end

-- ============================================================================
-- 天使系统 (AI_Asist.cs)
-- ============================================================================
local angel = nil

local function spawn_angel(index)
  if angel then return angel end
  angel = {
    e = nil,
    a_index = index or 1,
    x = Player.x,
    y = 0.2,
    z = Player.z,
    yaw = Player.yaw,
    -- 属性
    speed = 0.3,
    cur_speed = 0.3,
    firerate = 1.0,
    -- 状态
    shoot = false,
    delay_attack = 0,
    -- 移动
    direction_x = 0,
    direction_z = 0,
    look_yaw = Player.yaw,
    loaddir_timer = 0,
    -- 攻击目标
    target_x = 0,
    target_z = 0,
    -- 视觉
    t = 0,
  }

  -- 创建 ECS 实体
  angel.e = dse.ecs.create_entity()
  dse.ecs.add_transform(angel.e, angel.x, angel.y, angel.z, 0.5, 0.5, 0.5)
  pcall(dse.ecs.mesh_renderer_add, angel.e, "assets/models/ball.glb")
  dse.ecs.set_mesh_shader_variant(angel.e, "MESH_LIT")
  dse.ecs.set_mesh_color(angel.e, 0.6, 0.8, 1.0, 0.8)

  return angel
end

local function despawn_angel()
  if not angel then return end
  if angel.e then kill_entity(angel.e); angel.e = nil end
  angel = nil
end

-- LoadDir (C# InvokeRepeating 0.1s)
local function angel_load_dir()
  if not angel then return end
  angel.direction_x = Player.x - angel.x
  angel.direction_z = Player.z - angel.z
  if angel.direction_x ~= 0 or angel.direction_z ~= 0 then
    local mag = math.sqrt(angel.direction_x * angel.direction_x + angel.direction_z * angel.direction_z)
    if mag > 0.001 then
      angel.direction_x = angel.direction_x / mag
      angel.direction_z = angel.direction_z / mag
    end
    angel.look_yaw = dir_to_yaw(angel.direction_x, angel.direction_z)
  end
end

-- AttackOn (C# AI_Asist.AttackOn)
local function angel_attack_on(tx, tz)
  if not angel then return end
  angel.target_x = tx
  angel.target_z = tz
  angel.shoot = true
  angel.delay_attack = 0
end

-- AttackFinish (C# AI_Asist.AttackFinish)
local function angel_attack_finish()
  if not angel then return end
  angel.shoot = false
  -- 溅射特效 (C# ef_splash)
  if AISystem.on_angel_splash then
    AISystem.on_angel_splash(angel.target_x, angel.target_z)
  end
end

-- Update (C# AI_Asist.Update)
local function update_angel(dt)
  if not angel then return end
  angel.t = angel.t + dt

  -- LoadDir (每 0.1s)
  angel.loaddir_timer = angel.loaddir_timer - dt
  if angel.loaddir_timer <= 0 then
    angel.loaddir_timer = 0.1
    angel_load_dir()
  end

  -- 速度调整 (C# cur_speed = Lerp(cur_speed, 0/speed, dt * 4))
  if angel.shoot then
    angel.cur_speed = lerp(angel.cur_speed, 0, dt * 4)
  else
    angel.cur_speed = lerp(angel.cur_speed, angel.speed, dt * 4)
    -- 攻击冷却 (C# delay_atack > firerate)
    if angel.delay_attack > angel.firerate then
      angel.delay_attack = 0
      -- 检测附近敌人 → 攻击 (C# OnTriggerEnter layer==8)
      local nearest_dist = 999
      local nearest_en = nil
      for _, en in ipairs(Entities.enemies) do
        if not en.dead and en.monmovestat and en.monmovestat > 0 then
          local d = dist2d(angel.x, angel.z, en.x, en.z)
          if d < 3.0 and d < nearest_dist then
            nearest_dist = d
            nearest_en = en
          end
        end
      end
      if Entities.boss and not Entities.boss.dead then
        local d = dist2d(angel.x, angel.z, Entities.boss.x, Entities.boss.z)
        if d < 5.0 and d < nearest_dist then
          nearest_dist = d
          nearest_en = Entities.boss
        end
      end
      if nearest_en then
        angel_attack_on(nearest_en.x, nearest_en.z)
        -- 发射箭矢 (C# m_arrow)
        if AISystem.on_angel_shoot then
          AISystem.on_angel_shoot(angel.x, angel.y, angel.z, angel.target_x, angel.target_z)
        end
        -- 延迟结束射击
        local finish_timer = 0.3
        angel._finish_timer = finish_timer
      end
    else
      angel.delay_attack = angel.delay_attack + dt
    end
  end

  -- 射击完成计时
  if angel._finish_timer and angel._finish_timer > 0 then
    angel._finish_timer = angel._finish_timer - dt
    if angel._finish_timer <= 0 then
      angel_attack_finish()
      angel._finish_timer = nil
    end
  end

  -- 转向 + 移动 (C# rotation = Lerp(rotation, rotate, dt * cur_speed * 4))
  angel.yaw = lerp_angle(angel.yaw, angel.look_yaw, dt * angel.cur_speed * 4)
  local fx, fz = yaw_to_dir(angel.yaw)
  angel.x = angel.x + fx * dt * angel.cur_speed
  angel.z = angel.z + fz * dt * angel.cur_speed

  -- 同步 ECS
  if angel.e then
    dse.ecs.set_transform_position(angel.e, angel.x, angel.y, angel.z)
    dse.ecs.set_transform_rotation(angel.e, 0, angel.yaw, 0)
    local s = 0.5 + 0.05 * math.sin(angel.t * 8)
    dse.ecs.set_transform_scale(angel.e, s, s, s)
    local a = 0.6 + 0.2 * math.sin(angel.t * 6)
    dse.ecs.set_mesh_color(angel.e, 0.6, 0.8, 1.0, a)
  end
end

-- ============================================================================
-- 主更新函数
-- ============================================================================
local function update(dt)
  update_general(dt)
  update_angel(dt)
end

-- 清空
local function clear()
  despawn_general()
  despawn_angel()
end

-- ============================================================================
-- 模块导出
-- ============================================================================
local AISystem = {
  -- 武将
  spawn_general    = spawn_general,
  despawn_general  = despawn_general,
  general_damaged  = general_damaged,
  general_hpfull   = general_hpfull,
  general_attack_on = general_attack_on,
  get_general      = function() return general end,
  -- 天使
  spawn_angel      = spawn_angel,
  despawn_angel    = despawn_angel,
  angel_attack_on  = angel_attack_on,
  angel_attack_finish = angel_attack_finish,
  get_angel        = function() return angel end,
  -- 主循环
  update           = update,
  clear            = clear,
  -- 回调 (main.lua 注入)
  on_general_dead    = nil,
  on_general_attack  = nil,
  on_general_special = nil,
  on_general_hitcam  = nil,
  on_angel_shoot     = nil,
  on_angel_splash    = nil,
}

_G.AISystem = AISystem
return AISystem
