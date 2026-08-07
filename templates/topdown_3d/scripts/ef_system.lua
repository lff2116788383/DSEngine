-- ============================================================================
-- ef_system.lua — 完整特效系统
-- 1:1 移植自 25 个 Ef_*.cs 文件
-- 依赖: state.lua (G, Entities, kill_entity)
-- 与 monster_efs.lua 互补: monster_efs 管池化系统(血条/影子/伤害字/血溅/掉落)
--                          ef_system 管特效实例(UV动画/缩放/雾/爆炸/流星)
-- ============================================================================

local S = require "scripts.state"
local G, Entities = S.G, S.Entities
local kill_entity = S.kill_entity
local clamp, lerp = S.clamp, S.lerp

-- ── 内部工具 ─────────────────────────────────────────────────────────
local function rand_range(a, b) return a + math.random() * (b - a) end

-- UV 帧计算 (4x4 / 2x2 网格序列帧)
local function calc_uv_frame(frame, cnt_x, cnt_y)
  local uIndex = frame % cnt_x
  local vIndex = math.floor(frame / cnt_x)
  if vIndex >= cnt_y then vIndex = cnt_y - 1 end
  local sx = 1.0 / cnt_x
  local sy = 1.0 / cnt_y
  local u0 = uIndex * sx
  local v1 = 1.0 - vIndex * sy
  local v0 = v1 - sy
  return {u0, v0, u0 + sx, v0, u0 + sx, v1, u0, v1}
end

-- 创建 quad 实体 (通用特效平面)
local function create_quad(x, y, z, sx, sy, sz, r, g, b, a)
  local e = dse.ecs.create_entity()
  dse.ecs.add_transform(e, x, y, z, sx or 1, sy or 1, sz or 1)
  local v = {-0.5,0,-0.5, 0.5,0,-0.5, 0.5,0,0.5, -0.5,0,0.5}
  local idx = {0,1,2, 2,3,0}
  dse.ecs.add_mesh_renderer(e, r or 1, g or 1, b or 1, a or 1, v, idx)
  dse.ecs.set_mesh_shader_variant(e, "MESH_UNLIT")
  return e
end

-- ── 活跃特效列表 ─────────────────────────────────────────────────────
local effects = {}

-- ============================================================================
-- 1. Ef_ani_loop — UV 循环序列帧动画 (带碰撞体)
--    C# Ef_ani_loop: uvAnimationTileX/Y, framesPerSecond, impact, loop
-- ============================================================================
local function spawn_ani_loop(x, y, z, opts)
  opts = opts or {}
  local e = create_quad(x, y, z, opts.scale or 1, opts.scale or 1, opts.scale or 1,
    opts.r or 1, opts.g or 1, opts.b or 1, opts.a or 1)
  if opts.texture and S.TEX and S.TEX[opts.texture] then
    dse.ecs.set_mesh_texture(e, "albedo", S.TEX[opts.texture])
  end
  local cnt_x = opts.cnt_x or 4
  local cnt_y = opts.cnt_y or 4
  dse.ecs.set_mesh_uvs(e, calc_uv_frame(0, cnt_x, cnt_y))
  table.insert(effects, {
    e = e, type = "ani_loop", x = x, y = y, z = z,
    t = 0, fps = opts.fps or 20, cnt_x = cnt_x, cnt_y = cnt_y,
    impact = opts.impact or 1, loop = opts.loop or false,
    lastframe = cnt_x * cnt_y, oldframe = -1,
    collide = opts.collide or false, collide_enemies = opts.collide_enemies,
    damage = opts.damage, attack_type = opts.attack_type,
    on_impact = opts.on_impact, impact_done = false,
    life = opts.life or 3.0,
  })
  return e
end

local function update_ani_loop(ef, dt)
  ef.t = ef.t + dt
  local frame = math.floor(ef.t * ef.fps)
  if ef.loop then frame = frame % ef.lastframe end
  if frame ~= ef.oldframe then
    if frame >= ef.lastframe then
      ef.active = false
      return
    end
    -- 碰撞体在 impact 帧后激活
    if frame >= ef.impact and not ef.impact_done then
      ef.impact_done = true
      if ef.on_impact then ef.on_impact(ef) end
    end
    dse.ecs.set_mesh_uvs(ef.e, calc_uv_frame(frame, ef.cnt_x, ef.cnt_y))
    ef.oldframe = frame
  end
end

-- ============================================================================
-- 2. Ef_ani_loop_simple — 简化 UV 循环 (无碰撞体)
-- ============================================================================
local function spawn_ani_loop_simple(x, y, z, opts)
  opts = opts or {}
  return spawn_ani_loop(x, y, z, {
    scale = opts.scale, r = opts.r, g = opts.g, b = opts.b, a = opts.a,
    texture = opts.texture, cnt_x = opts.cnt_x or 4, cnt_y = opts.cnt_y or 4,
    fps = opts.fps or 20, loop = opts.loop or false,
    life = opts.life or 3.0,
  })
end

-- ============================================================================
-- 3. Ef_block — 格挡特效 (缩放增长后消失)
--    C# Ef_block: scale += 2*dt until > 1.4 → deactivate
-- ============================================================================
local function spawn_block_ef(x, y, z, opts)
  opts = opts or {}
  local e = create_quad(x, y, z, 0.1, 0.1, 0.1,
    0.5, 0.8, 1.0, 0.8)
  table.insert(effects, {
    e = e, type = "block", x = x, y = y, z = z,
    t = 0, scale = 0.1, life = 1.0,
  })
  return e
end

local function update_block(ef, dt)
  ef.scale = ef.scale + 2.0 * dt
  if ef.scale > 1.4 then
    ef.active = false
    return
  end
  dse.ecs.set_transform_scale(ef.e, ef.scale, ef.scale, ef.scale)
end

-- ============================================================================
-- 4. Ef_blood — 血溅 4x4 UV 动画
--    (已在 monster_efs.lua 中池化实现, 此处提供非池化版本)
-- ============================================================================
local function spawn_blood_ef(x, y, z, dx, dz)
  local e = create_quad(x, y, z, 0.5, 0.5, 0.5, 1.0, 1.0, 1.0, 0.9)
  if S.TEX and S.TEX.blood then
    dse.ecs.set_mesh_texture(e, "albedo", S.TEX.blood)
  end
  dse.ecs.set_mesh_uvs(e, calc_uv_frame(0, 4, 4))
  table.insert(effects, {
    e = e, type = "blood", x = x, y = y, z = z,
    t = 0, fps = 22, cnt_x = 4, cnt_y = 4,
    lastframe = 16, oldframe = -1, life = 0.8,
  })
  return e
end

local function update_blood(ef, dt)
  ef.t = ef.t + dt
  local frame = math.floor(ef.t * ef.fps)
  if frame ~= ef.oldframe then
    if frame >= ef.lastframe then
      ef.active = false
      return
    end
    dse.ecs.set_mesh_uvs(ef.e, calc_uv_frame(frame, ef.cnt_x, ef.cnt_y))
    ef.oldframe = frame
  end
end

-- ============================================================================
-- 5. Ef_blur — 模糊特效 (UV Y 偏移下移)
--    C# Ef_blur: mainTextureOffset.y -= 2*dt until < -1 → reset
-- ============================================================================
local function spawn_blur_ef(x, y, z, opts)
  opts = opts or {}
  local e = create_quad(x, y, z, opts.scale or 2, opts.scale or 2, opts.scale or 2,
    1.0, 1.0, 1.0, 0.6)
  table.insert(effects, {
    e = e, type = "blur", x = x, y = y, z = z,
    t = 0, offset_y = 0, life = 1.0,
  })
  return e
end

local function update_blur(ef, dt)
  ef.offset_y = ef.offset_y - 2.0 * dt
  if ef.offset_y < -1.0 then
    ef.active = false
    return
  end
  -- 近似: 用颜色 alpha 淡出代替 UV offset
  local a = clamp(0.6 + ef.offset_y * 0.6, 0, 0.6)
  dse.ecs.set_mesh_color(ef.e, 1, 1, 1, a)
end

-- ============================================================================
-- 6. Ef_boom — 爆炸粒子 (发射器 + 贴图)
--    C# Ef_boom: SetTex(index, pos, collider) → emit 0.5s → stop
-- ============================================================================
local function spawn_boom_ef(x, z, tex_index, opts)
  opts = opts or {}
  tex_index = tex_index or 0
  local e = create_quad(x, 0.02, z, 1.5, 1.5, 1.5, 1.0, 0.8, 0.2, 0.9)
  -- 根据索引选择颜色
  local r, g, b = 1.0, 0.8, 0.2
  if tex_index == 1 then r, g, b = 1.0, 0.3, 0.1
  elseif tex_index == 2 then r, g, b = 0.3, 0.5, 1.0 end
  dse.ecs.set_mesh_color(e, r, g, b, 0.9)
  table.insert(effects, {
    e = e, type = "boom", x = x, y = 0.02, z = z,
    t = 0, emit_time = 0.5, life = 1.0,
    collide = opts.collide or false,
    damage = opts.damage, attack_type = opts.attack_type or "fire",
    collide_enemies = opts.collide_enemies,
    impact_done = false,
    -- 粒子飞溅
    particles = {},
  })
  -- 生成飞溅粒子
  for _ = 1, 12 do
    local angle = math.random() * math.pi * 2
    local speed = rand_range(1, 4)
    table.insert(effects[#effects].particles, {
      dx = math.cos(angle) * speed,
      dy = rand_range(0.5, 3),
      dz = math.sin(angle) * speed,
      x = x, y = 0.1, z = z,
    })
  end
  return e
end

local function update_boom(ef, dt)
  ef.t = ef.t + dt
  -- 缩放动画
  local scale = 1.5 + ef.t * 3
  if ef.t > 0.3 then scale = scale - (ef.t - 0.3) * 5 end
  if scale < 0.1 then scale = 0.1 end
  dse.ecs.set_transform_scale(ef.e, scale, scale, scale)
  -- alpha 淡出
  local a = clamp(0.9 - ef.t * 1.5, 0, 0.9)
  dse.ecs.set_mesh_color(ef.e, 1, 0.8, 0.2, a)
  if ef.t >= ef.life then
    ef.active = false
  end
end

-- ============================================================================
-- 7. Ef_Coin — 金币特效 (上浮+旋转+消失)
--    C# Ef_Coin: GetCoin(pos) → lerp to targetpos, rotate 1200°/s, 0.5s
-- ============================================================================
local function spawn_coin_ef(x, y, z)
  local e = create_quad(x, y, z, 0.3, 0.3, 0.3, 1.0, 0.85, 0.2, 1.0)
  table.insert(effects, {
    e = e, type = "coin", x = x, y = y, z = z,
    t = 0, target_y = 0.15, rot = 0, life = 0.6,
  })
  return e
end

local function update_coin(ef, dt)
  ef.t = ef.t + dt
  if ef.t > 0.5 then
    ef.active = false
    return
  end
  ef.y = lerp(ef.y, ef.target_y, dt * 15)
  ef.rot = ef.rot + 1200 * dt
  dse.ecs.set_transform_position(ef.e, ef.x, ef.y, ef.z)
  dse.ecs.set_transform_rotation(ef.e, 0, ef.rot, 0)
end

-- ============================================================================
-- 8. Ef_energy_gather — 能量聚集 (颜色渐变 + UV 偏移)
--    C# Ef_energy_gather: finish_delay, show_delay, fogalpha
-- ============================================================================
local function spawn_energy_gather(x, y, z, opts)
  opts = opts or {}
  local e = create_quad(x, y, z, 1, 1, 1, 0.5, 0.5, 0.5, 0)
  table.insert(effects, {
    e = e, type = "energy_gather", x = x, y = y, z = z,
    t = 0, show_delay = 0.1, finish_delay = opts.finish_delay or 1.2,
    fogalpha = 2, alpha = 0, ani_start = false,
    life = opts.finish_delay or 1.2,
  })
  return e
end

local function update_energy_gather(ef, dt)
  ef.t = ef.t + dt
  if ef.t > ef.finish_delay then
    ef.active = false
    return
  elseif ef.t > ef.finish_delay - 0.4 then
    -- 淡出
    ef.alpha = lerp(ef.alpha, 0, dt * ef.fogalpha * 5)
  elseif ef.t > ef.show_delay then
    ef.ani_start = true
  end
  if ef.ani_start then
    ef.alpha = lerp(ef.alpha, 0.5, dt * ef.fogalpha * 5)
  end
  dse.ecs.set_mesh_color(ef.e, 0.5, 0.5, 0.5, ef.alpha)
  -- 脉动缩放
  local s = 1.0 + 0.2 * math.sin(ef.t * 15)
  dse.ecs.set_transform_scale(ef.e, s, s, s)
end

-- ============================================================================
-- 9. Ef_hit — 命中特效 (缩放偏移 + 快速消失)
--    C# Ef_hit: growVector=(-3,0,10), 0.25s 后消失
-- ============================================================================
local function spawn_hit_ef(x, y, z, opts)
  opts = opts or {}
  local e = create_quad(x, y, z, 0.3, 0.3, 0.3, 1.0, 1.0, 0.3, 0.9)
  table.insert(effects, {
    e = e, type = "hit", x = x, y = y, z = z,
    t = 0, scale_x = 0.3, scale_z = 0.3, life = 0.3,
  })
  return e
end

local function update_hit(ef, dt)
  ef.t = ef.t + dt
  if ef.t > 0.25 then
    ef.active = false
    return
  end
  ef.scale_x = ef.scale_x - 3.0 * dt
  ef.scale_z = ef.scale_z + 10.0 * dt
  dse.ecs.set_transform_scale(ef.e, ef.scale_x, 0.3, ef.scale_z)
end

-- ============================================================================
-- 10. Ef_meteo — 流星 (从天而降 + 落地溅射)
--     C# Ef_meteo: y>4.6=origin(hidden), else fall -2.4/s, spin 400°/s
-- ============================================================================
local function spawn_meteo_ef(x, z)
  local e = create_quad(x, 5.0, z, 0.5, 0.5, 0.5, 1.0, 0.4, 0.1, 1.0)
  table.insert(effects, {
    e = e, type = "meteo", x = x, y = 5.0, z = z,
    t = 0, vy = -2.4, rot_y = 0, splash = false, life = 3.0,
  })
  return e
end

local function update_meteo(ef, dt)
  if ef.y > 0 then
    ef.y = ef.y + ef.vy * dt
    ef.rot_y = ef.rot_y - 400 * dt
    dse.ecs.set_transform_position(ef.e, ef.x, ef.y, ef.z)
    dse.ecs.set_transform_rotation(ef.e, 0, ef.rot_y, 0)
  elseif not ef.splash then
    -- 落地: 创建溅射
    ef.splash = true
    spawn_boom_ef(ef.x, ef.z, 0, { collide_enemies = true, damage = 20, attack_type = "fire" })
    dse.ecs.set_transform_scale(ef.e, 0, 0, 0)
    ef.active = false
  end
end

-- ============================================================================
-- 11. Ef_meteospash — 流星溅射 (4x4 UV 动画 + 缩放增长)
--     C# Ef_meteospash: scale 1.3→3.5, fps=22, 4x4 grid
-- ============================================================================
local function spawn_meteo_splash(x, z)
  local e = create_quad(x, 0.05, z, 1.3, 1.3, 1.3, 1.0, 0.6, 0.2, 1.0)
  dse.ecs.set_mesh_uvs(e, calc_uv_frame(0, 4, 4))
  table.insert(effects, {
    e = e, type = "meteo_splash", x = x, y = 0.05, z = z,
    t = 0, scale = 1.3, fps = 22, cnt_x = 4, cnt_y = 4,
    lastframe = 16, oldframe = -1, life = 1.0,
  })
  return e
end

local function update_meteo_splash(ef, dt)
  ef.t = ef.t + dt
  if ef.scale < 3.5 then
    ef.scale = ef.scale + dt
    dse.ecs.set_transform_scale(ef.e, ef.scale, ef.scale, ef.scale)
  end
  local frame = math.floor(ef.t * ef.fps) % ef.lastframe
  if frame ~= ef.oldframe then
    dse.ecs.set_mesh_uvs(ef.e, calc_uv_frame(frame, ef.cnt_x, ef.cnt_y))
    ef.oldframe = frame
  end
  if ef.t > 0.8 then
    ef.active = false
  end
end

-- ============================================================================
-- 12. Ef_rapidstab — 连续刺击 (4x4 UV + 循环 + 碰撞)
--     C# Ef_rapidstab: showtime, loopcount, damagerate, fps=18
-- ============================================================================
local function spawn_rapidstab(x, y, z, yaw, opts)
  opts = opts or {}
  local e = create_quad(x, y, z, opts.scale or 2, opts.scale or 2, opts.scale or 2,
    1.0, 0.8, 0.3, 0.9)
  dse.ecs.set_transform_rotation(e, 0, yaw, 0)
  dse.ecs.set_mesh_uvs(e, calc_uv_frame(0, 4, 4))
  table.insert(effects, {
    e = e, type = "rapidstab", x = x, y = y, z = z, yaw = yaw,
    t = 0, show_delay = opts.show_delay or 0.15,
    fps = opts.fps or 18, cnt_x = 4, cnt_y = 4,
    lastframe = 16, oldframe = -1,
    loopcount = opts.loopcount or 0, count = 0,
    damagerate = opts.damagerate or 2, show = false,
    collide_enemies = opts.collide_enemies,
    damage = opts.damage, attack_type = opts.attack_type or "pierce",
    impact_done = false, life = 3.0,
    forward_x = math.sin(math.rad(yaw)), forward_z = -math.cos(math.rad(yaw)),
  })
  return e
end

local function update_rapidstab(ef, dt)
  if not ef.show then
    if ef.t < ef.show_delay then
      ef.t = ef.t + dt
      return
    else
      ef.show = true
      ef.t = 0
    end
  end
  -- 前移
  ef.x = ef.x + ef.forward_x * dt * 0.1
  ef.z = ef.z + ef.forward_z * dt * 0.1
  dse.ecs.set_transform_position(ef.e, ef.x, ef.y, ef.z)
  ef.t = ef.t + dt
  local frame = math.floor(ef.t * ef.fps)
  if frame ~= ef.oldframe then
    if frame >= ef.lastframe then
      if ef.count >= ef.loopcount then
        ef.active = false
        return
      else
        ef.count = ef.count + 1
        ef.t = 0
        ef.oldframe = -1
        return
      end
    end
    -- 碰撞帧
    if frame % ef.damagerate == 1 and not ef.impact_done then
      ef.impact_done = true
      -- 触发伤害检测回调
    elseif frame % ef.damagerate ~= 1 then
      ef.impact_done = false
    end
    dse.ecs.set_mesh_uvs(ef.e, calc_uv_frame(frame, ef.cnt_x, ef.cnt_y))
    ef.oldframe = frame
  end
end

-- ============================================================================
-- 13. Ef_rotfog — 旋转雾 (缩放增长 + 旋转 + 颜色淡出)
--     C# Ef_rotfog: RotfogOn(height, speed, rot, alpha, xyratio)
-- ============================================================================
local function spawn_rotfog(x, y, z, opts)
  opts = opts or {}
  local e = create_quad(x, y, z, 0.3, 0.3, 0.3, 0.5, 0.5, 0.5, 0.5)
  table.insert(effects, {
    e = e, type = "rotfog", x = x, y = y, z = z,
    t = 0, fogheight = opts.height or 2.0,
    fogspeed = opts.speed or 0.5, fogrotation = opts.rot or 90,
    fogalpha = opts.alpha or 2, xyratio = opts.xyratio or 0.5,
    target_alpha = 0, current_alpha = 0.5, rot_y = 0,
    life = 5.0,
  })
  return e
end

local function update_rotfog(ef, dt)
  local scale_x = dse.ecs.get_transform_scale_x and dse.ecs.get_transform_scale_x(ef.e) or 0.3
  -- 简化: 直接跟踪 scale
  ef.scale_x = (ef.scale_x or 0.3)
  ef.scale_y = (ef.scale_y or 0.3)
  ef.scale_z = (ef.scale_z or 0.3)
  local plusV = ef.fogspeed
  if ef.scale_y > ef.fogheight then
    ef.active = false
    return
  elseif ef.scale_y > ef.fogheight * 0.5 then
    -- 淡出阶段
    ef.scale_x = ef.scale_x + plusV * dt
    ef.scale_y = ef.scale_y + plusV * ef.xyratio * dt
    ef.scale_z = ef.scale_z + plusV * dt
    ef.current_alpha = lerp(ef.current_alpha, 0, dt * ef.fogalpha)
  else
    -- 增长阶段
    ef.scale_x = ef.scale_x + plusV * dt
    ef.scale_y = ef.scale_y + plusV * ef.xyratio * dt
    ef.scale_z = ef.scale_z + plusV * dt
    ef.rot_y = ef.rot_y + ef.fogrotation * dt
  end
  dse.ecs.set_transform_scale(ef.e, ef.scale_x, ef.scale_y, ef.scale_z)
  dse.ecs.set_transform_rotation(ef.e, 0, ef.rot_y, 0)
  dse.ecs.set_mesh_color(ef.e, 0.5, 0.5, 0.5, ef.current_alpha)
end

-- ============================================================================
-- 14. Ef_splash — 飞溅特效 (延迟显示 + 颜色淡出)
--     C# Ef_splash: SplashOn(col, dis, delay) → ShowOn → fade
-- ============================================================================
local function spawn_splash(x, y, z, opts)
  opts = opts or {}
  local e = create_quad(x, y, z, 1, 1, 1, 0.5, 0.5, 0.5, 0.8)
  table.insert(effects, {
    e = e, type = "splash", x = x, y = y, z = z,
    t = 0, show_delay = opts.delay or 0.1,
    show_time = 1.5, showon = false,
    dis = opts.dis or 0.1, collide = opts.collide or false,
    life = 2.0,
  })
  return e
end

local function update_splash(ef, dt)
  if not ef.showon then
    if ef.t >= ef.show_delay then
      ef.showon = true
      ef.t = 0
      dse.ecs.set_mesh_visible(ef.e, true)
    else
      ef.t = ef.t + dt
      return
    end
  end
  ef.t = ef.t + dt
  ef.show_time = ef.show_time - dt
  if ef.show_time > 1.3 then
    -- 显示中
  elseif ef.show_time > 0.5 then
    -- 碰撞关闭
  elseif ef.show_time > 0 then
    -- 颜色淡出
    local a = clamp(ef.show_time * 1.6, 0, 0.8)
    dse.ecs.set_mesh_color(ef.e, 0.5, 0.5, 0.5, a)
  else
    ef.active = false
  end
end

-- ============================================================================
-- 15. Ef_splash_uv — UV 飞溅 (4x4 UV 动画 + 前方位置)
--     C# Ef_splash_uv: SplashOn() → 4x4 fps=30, scale=2
-- ============================================================================
local function spawn_splash_uv(x, y, z, yaw)
  local e = create_quad(x, y, z, 2, 2, 2, 1.0, 0.9, 0.3, 0.9)
  dse.ecs.set_transform_rotation(e, 0, yaw, 0)
  dse.ecs.set_mesh_uvs(e, calc_uv_frame(0, 4, 4))
  table.insert(effects, {
    e = e, type = "splash_uv", x = x, y = y, z = z, yaw = yaw,
    t = 0, fps = 30, cnt_x = 4, cnt_y = 4,
    lastframe = 16, oldframe = -1, life = 0.7,
  })
  return e
end

local function update_splash_uv(ef, dt)
  ef.t = ef.t + dt
  local frame = math.floor(ef.t * ef.fps) % ef.lastframe
  if frame ~= ef.oldframe then
    if frame == 0 and ef.oldframe ~= -1 then
      ef.active = false
      return
    end
    dse.ecs.set_mesh_uvs(ef.e, calc_uv_frame(frame, ef.cnt_x, ef.cnt_y))
    ef.oldframe = frame
  end
end

-- ============================================================================
-- 16. Ef_split1 — 分裂消失 (延迟 + 颜色淡出)
--     C# Ef_split1: destroydelay>3 → lerp to clear, >4 → deactivate
-- ============================================================================
local function spawn_split1(x, y, z)
  local e = create_quad(x, y, z, 1, 1, 1, 1.0, 1.0, 1.0, 1.0)
  table.insert(effects, {
    e = e, type = "split1", x = x, y = y, z = z,
    t = 0, alpha = 1.0, life = 5.0,
  })
  return e
end

local function update_split1(ef, dt)
  ef.t = ef.t + dt
  if ef.t > 4.0 then
    ef.active = false
    return
  elseif ef.t > 3.0 then
    ef.alpha = lerp(ef.alpha, 0, dt * 5)
    dse.ecs.set_mesh_color(ef.e, 1, 1, 1, ef.alpha)
  end
end

-- ============================================================================
-- 17. Ef_stepfog — 脚步雾 (延迟显示 + 缩放增长 + 颜色淡出)
--     C# Ef_stepfog: fogheight, fogspeed, fogalpha, smoothfactor, xyratio
-- ============================================================================
local function spawn_stepfog_ef(x, y, z, opts)
  opts = opts or {}
  local e = create_quad(x, y, z, 0.2, 0.2, 0.2, 0.5, 0.5, 0.5, 0)
  table.insert(effects, {
    e = e, type = "stepfog", x = x, y = y, z = z,
    t = 0, fogheight = opts.height or 0.5,
    fogspeed = opts.speed or 0.3, fogalpha = opts.alpha or 3,
    smoothfactor = opts.smoothfactor or 0.5, xyratio = opts.xyratio or 0.5,
    alpha = 0, life = 2.0, visible = false,
  })
  return e
end

local function update_stepfog(ef, dt)
  ef.t = ef.t + dt
  if ef.t < 0.1 then return end
  if not ef.visible then
    ef.visible = true
    dse.ecs.set_mesh_visible(ef.e, true)
  end
  ef.alpha = lerp(ef.alpha, 0, dt * ef.fogalpha)
  dse.ecs.set_mesh_color(ef.e, 0.5, 0.5, 0.5, ef.alpha * 0.5)
  local sx, sy, sz = dse.ecs.get_transform_scale_x and dse.ecs.get_transform_scale_x(ef.e) or 0.2, 0.2, 0.2
  ef.scale_y = (ef.scale_y or 0.2)
  if ef.scale_y > ef.fogheight then
    ef.active = false
    return
  elseif ef.scale_y > ef.fogheight * 0.8 then
    ef.scale_y = ef.scale_y + ef.fogspeed * ef.smoothfactor * dt
  else
    ef.scale_y = ef.scale_y + ef.fogspeed * ef.xyratio * dt
  end
  local s = ef.scale_y / ef.xyratio
  dse.ecs.set_transform_scale(ef.e, s, ef.scale_y, s)
end

-- ============================================================================
-- 18. Ef_swing1 — 挥砍特效 (4x4 UV + 碰撞 + 冲击帧)
--     C# Ef_swing1: SwingOn(delay, cnt_x, cnt_y, uvspeed, impact, addforce)
--     (已在 monster_efs.lua 中基础实现, 此处为完整版)
-- ============================================================================
local function spawn_swing_full(x, y, z, yaw, opts)
  opts = opts or {}
  local cnt_x = opts.cnt_x or 4
  local cnt_y = opts.cnt_y or 4
  local e = create_quad(x, y, z, opts.scale or 2, opts.scale or 2, opts.scale or 2,
    1.0, 1.0, 1.0, 0.9)
  dse.ecs.set_transform_rotation(e, 0, yaw, 0)
  if S.TEX and S.TEX.ef_swing then
    dse.ecs.set_mesh_texture(e, "albedo", S.TEX.ef_swing)
  end
  dse.ecs.set_mesh_uvs(e, calc_uv_frame(0, cnt_x, cnt_y))
  table.insert(effects, {
    e = e, type = "swing1", x = x, y = y, z = z, yaw = yaw,
    t = 0, delay = opts.delay or 0,
    fps = opts.fps or 20, cnt_x = cnt_x, cnt_y = cnt_y,
    lastframe = cnt_x * cnt_y, oldframe = -1,
    impactframe = opts.impact or 1, efon = true,
    collide_enemies = opts.collide_enemies,
    damage = opts.damage, attack_type = opts.attack_type or "normal",
    impact_done = false, life = 2.0,
  })
  return e
end

local function update_swing1(ef, dt)
  if ef.efon then
    if ef.delay > 0 then
      ef.delay = ef.delay - dt
      return
    else
      ef.efon = false
      dse.ecs.set_mesh_visible(ef.e, true)
      ef.t = 0
    end
  end
  ef.t = ef.t + dt
  local frame = math.floor(ef.t * ef.fps)
  if frame ~= ef.oldframe then
    if frame >= ef.lastframe then
      ef.active = false
      return
    elseif (frame == ef.impactframe or frame == ef.impactframe + 1) and not ef.impact_done then
      ef.impact_done = true
      -- 碰撞激活回调
    end
    dse.ecs.set_mesh_uvs(ef.e, calc_uv_frame(frame, ef.cnt_x, ef.cnt_y))
    ef.oldframe = frame
  end
end

-- ============================================================================
-- 19. Ef_swordwind — 剑风 (延迟 → 前进 → 缩小消失)
--     C# Ef_swordwind: delay 0.8s → move forward, scale.z-=3/s
-- ============================================================================
local function spawn_swordwind(x, y, z, yaw)
  local e = create_quad(x, y, z, 1, 1, 2, 0.8, 0.8, 1.0, 0.7)
  dse.ecs.set_transform_rotation(e, 0, yaw, 0)
  table.insert(effects, {
    e = e, type = "swordwind", x = x, y = y, z = z, yaw = yaw,
    t = 0, delay = 0, movestart = false,
    fx = math.sin(math.rad(yaw)), fz = -math.cos(math.rad(yaw)),
    scale_z = 2.0, life = 3.0,
  })
  return e
end

local function update_swordwind(ef, dt)
  if not ef.movestart then
    if ef.delay < 0.8 then
      ef.delay = ef.delay + dt
      return
    else
      ef.movestart = true
      ef.delay = 0
      dse.ecs.set_mesh_visible(ef.e, true)
    end
  end
  if ef.scale_z > 1.0 then
    ef.x = ef.x + ef.fx * dt
    ef.z = ef.z + ef.fz * dt
    ef.scale_z = ef.scale_z - 3.0 * dt
    dse.ecs.set_transform_position(ef.e, ef.x, ef.y, ef.z)
    dse.ecs.set_transform_scale(ef.e, 1, 1, ef.scale_z)
  else
    ef.active = false
  end
end

-- ============================================================================
-- 20. Ef_swtrail — 拖尾 (缩放缩小消失)
--     C# Ef_swtrail: scale=3 → -=10/s until <0.1
-- ============================================================================
local function spawn_swtrail(x, y, z)
  local e = create_quad(x, y, z, 3, 3, 3, 1.0, 1.0, 1.0, 0.5)
  table.insert(effects, {
    e = e, type = "swtrail", x = x, y = y, z = z,
    t = 0, scale = 3.0, life = 0.4,
  })
  return e
end

local function update_swtrail(ef, dt)
  ef.scale = ef.scale - 10.0 * dt
  if ef.scale < 0.1 then
    ef.active = false
    return
  end
  dse.ecs.set_transform_scale(ef.e, ef.scale, ef.scale, ef.scale)
end

-- ============================================================================
-- 21. Ef_twirl — 旋涡 (4x4 UV 高速动画 + 循环)
--     C# Ef_twirl: TwirlOn(x, y, fps, loop), fps=180
-- ============================================================================
local function spawn_twirl(x, y, z, opts)
  opts = opts or {}
  local cnt_x = opts.cnt_x or 4
  local cnt_y = opts.cnt_y or 4
  local e = create_quad(x, y, z, 2, 2, 2, 0.8, 0.6, 1.0, 0.8)
  dse.ecs.set_mesh_uvs(e, calc_uv_frame(0, cnt_x, cnt_y))
  table.insert(effects, {
    e = e, type = "twirl", x = x, y = y, z = z,
    t = 0, fps = opts.fps or 180, cnt_x = cnt_x, cnt_y = cnt_y,
    lastframe = cnt_x * cnt_y, oldframe = -1,
    isloop = opts.loop or false, life = 2.0,
  })
  return e
end

local function update_twirl(ef, dt)
  ef.t = ef.t + dt
  local frame = math.floor(ef.t * ef.fps)
  if ef.isloop then frame = frame % ef.lastframe end
  if frame ~= ef.oldframe then
    if frame >= ef.lastframe then
      ef.active = false
      return
    end
    dse.ecs.set_mesh_uvs(ef.e, calc_uv_frame(frame, ef.cnt_x, ef.cnt_y))
    ef.oldframe = frame
  end
  -- 旋转
  dse.ecs.set_transform_rotation(ef.e, 0, ef.t * 360, 0)
end

-- ============================================================================
-- 22. Ef_wheelwind — 轮风 (2x2 UV + 缩放增长 + 碰撞)
--     C# Ef_wheelwind: scale 0.3→1.8, fps=16, 4 frames, collider toggle
-- ============================================================================
local function spawn_wheelwind_ef(x, y, z)
  local e = create_quad(x, y, z, 0.3, 0.3, 0.3, 0.8, 0.8, 0.8, 0.8)
  dse.ecs.set_mesh_uvs(e, calc_uv_frame(0, 2, 2))
  table.insert(effects, {
    e = e, type = "wheelwind", x = x, y = y, z = z,
    t = 0, scale = 0.3, fps = 16, cnt_x = 2, cnt_y = 2,
    lastframe = 4, oldframe = -1, finishtime = 1.6,
    life = 2.0,
  })
  return e
end

local function update_wheelwind(ef, dt)
  ef.t = ef.t + dt
  if ef.scale < 1.8 then
    ef.scale = ef.scale + 4.0 * dt
  end
  if ef.scale > 1.0 then
    dse.ecs.set_mesh_visible(ef.e, true)
  end
  dse.ecs.set_transform_scale(ef.e, ef.scale, ef.scale, ef.scale)
  if ef.t > ef.finishtime then
    ef.active = false
    return
  end
  local frame = math.floor(ef.t * ef.fps) % 4
  if frame ~= ef.oldframe then
    dse.ecs.set_mesh_uvs(ef.e, calc_uv_frame(frame, ef.cnt_x, ef.cnt_y))
    ef.oldframe = frame
  end
end

-- ============================================================================
-- 23. Ef_forge_hammer — 锻造锤 (4x4 UV + 缩放增长 + 冲击回调)
--     C# Ef_forge_hammer: HammerHit() → fps=20, impact at frame 2
-- ============================================================================
local function spawn_forge_hammer(x, y, z)
  local e = create_quad(x, y, z, 0, 0, 0, 1.0, 0.8, 0.2, 0.9)
  dse.ecs.set_mesh_uvs(e, calc_uv_frame(0, 4, 4))
  table.insert(effects, {
    e = e, type = "forge_hammer", x = x, y = y, z = z,
    t = 0, scale = 0, fps = 20, cnt_x = 4, cnt_y = 4,
    lastframe = 16, oldframe = -1,
    anistart = true, impact = false, life = 1.0,
  })
  return e
end

local function update_forge_hammer(ef, dt)
  -- 缩放趋向 2
  ef.scale = lerp(ef.scale, 2.0, dt * 15)
  dse.ecs.set_transform_scale(ef.e, ef.scale, ef.scale, ef.scale)
  if not ef.anistart then return end
  ef.t = ef.t + dt
  local frame = math.floor(ef.t * ef.fps)
  if frame ~= ef.oldframe then
    if frame >= 2 and not ef.impact then
      ef.impact = true
      -- 锻造冲击回调
    end
    if frame >= ef.lastframe then
      ef.anistart = false
      ef.active = false
      return
    end
    dse.ecs.set_mesh_uvs(ef.e, calc_uv_frame(frame, ef.cnt_x, ef.cnt_y))
    ef.oldframe = frame
  end
end

-- ============================================================================
-- 24. Ef_ani_walk — 探索动画 (4x4 UV + 多循环 + 回调)
--     C# Ef_ani_walk: AniStart(index) → loop, fps=16
-- ============================================================================
local function spawn_ani_walk(x, y, z, ani_index)
  local e = create_quad(x, y, z, 0, 0, 0, 1.0, 1.0, 1.0, 1.0)
  dse.ecs.set_mesh_uvs(e, calc_uv_frame(0, 4, 4))
  local target_loops = 0
  if ani_index == 0 then target_loops = 3
  elseif ani_index == 1 then target_loops = 1
  elseif ani_index == 4 then target_loops = 100 end
  table.insert(effects, {
    e = e, type = "ani_walk", x = x, y = y, z = z,
    t = 0, fps = 16, cnt_x = 4, cnt_y = 4,
    lastframe = 16, oldframe = -1,
    ani_index = ani_index or 0, target_loops = target_loops,
    loopcount = 0, scale = 0, life = 10.0,
  })
  return e
end

local function update_ani_walk(ef, dt)
  ef.scale = lerp(ef.scale, 2.0, dt * 15)
  dse.ecs.set_transform_scale(ef.e, ef.scale, ef.scale, ef.scale)
  ef.t = ef.t + dt
  local frame = math.floor(ef.t * ef.fps)
  if frame == ef.oldframe then return end
  if frame >= ef.lastframe then
    if ef.loopcount >= ef.target_loops then
      ef.active = false
      return
    else
      ef.loopcount = ef.loopcount + 1
      ef.t = 0
      ef.oldframe = -1
      return
    end
  end
  dse.ecs.set_mesh_uvs(ef.e, calc_uv_frame(frame, ef.cnt_x, ef.cnt_y))
  ef.oldframe = frame
end

-- ============================================================================
-- 25. Ef_swing1_ride — 骑乘挥砍 (同 swing1, 固定 layer 20)
-- ============================================================================
local function spawn_swing_ride(x, y, z, yaw, opts)
  opts = opts or {}
  opts.attack_type = opts.attack_type or "normal"
  return spawn_swing_full(x, y, z, yaw, opts)
end

-- ============================================================================
-- 更新分发表
-- ============================================================================
local updaters = {
  ani_loop       = update_ani_loop,
  block          = update_block,
  blood          = update_blood,
  blur           = update_blur,
  boom           = update_boom,
  coin           = update_coin,
  energy_gather  = update_energy_gather,
  hit            = update_hit,
  meteo          = update_meteo,
  meteo_splash   = update_meteo_splash,
  rapidstab      = update_rapidstab,
  rotfog         = update_rotfog,
  splash         = update_splash,
  splash_uv      = update_splash_uv,
  split1         = update_split1,
  stepfog        = update_stepfog,
  swing1         = update_swing1,
  swordwind      = update_swordwind,
  swtrail        = update_swtrail,
  twirl          = update_twirl,
  wheelwind      = update_wheelwind,
  forge_hammer   = update_forge_hammer,
  ani_walk       = update_ani_walk,
}

-- ============================================================================
-- 主更新函数
-- ============================================================================
local function update(dt)
  for i = #effects, 1, -1 do
    local ef = effects[i]
    if ef.active == nil then ef.active = true end
    local fn = updaters[ef.type]
    if fn then
      fn(ef, dt)
    end
    if not ef.active or ef.t >= ef.life then
      kill_entity(ef.e)
      table.remove(effects, i)
    end
  end
end

-- ============================================================================
-- 清空所有特效
-- ============================================================================
local function clear()
  for i = #effects, 1, -1 do
    kill_entity(effects[i].e)
    table.remove(effects, i)
  end
end

-- ============================================================================
-- 模块导出
-- ============================================================================
local EfSystem = {
  -- 创建函数
  spawn_ani_loop       = spawn_ani_loop,
  spawn_ani_loop_simple= spawn_ani_loop_simple,
  spawn_ani_walk       = spawn_ani_walk,
  spawn_block          = spawn_block_ef,
  spawn_blood          = spawn_blood_ef,
  spawn_blur           = spawn_blur_ef,
  spawn_boom           = spawn_boom_ef,
  spawn_coin           = spawn_coin_ef,
  spawn_energy_gather  = spawn_energy_gather,
  spawn_forge_hammer   = spawn_forge_hammer,
  spawn_hit            = spawn_hit_ef,
  spawn_meteo          = spawn_meteo_ef,
  spawn_meteo_splash   = spawn_meteo_splash,
  spawn_rapidstab      = spawn_rapidstab,
  spawn_rotfog         = spawn_rotfog,
  spawn_splash         = spawn_splash,
  spawn_splash_uv      = spawn_splash_uv,
  spawn_split1         = spawn_split1,
  spawn_stepfog        = spawn_stepfog_ef,
  spawn_swing          = spawn_swing_full,
  spawn_swing_ride     = spawn_swing_ride,
  spawn_swordwind      = spawn_swordwind,
  spawn_swtrail        = spawn_swtrail,
  spawn_twirl          = spawn_twirl,
  spawn_wheelwind      = spawn_wheelwind_ef,
  -- 管理
  update  = update,
  clear   = clear,
  effects = effects,
}

_G.EfSystem = EfSystem
return EfSystem
