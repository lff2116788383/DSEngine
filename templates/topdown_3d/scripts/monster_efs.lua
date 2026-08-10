-- ============================================================================
-- monster_efs.lua — 特效系统 (Monster_efs.cs / Hp_bar.cs / WeaponDrop.cs 完整移植)
-- 伤害数字池 / 血溅特效池 / 影子池 / 掉落物池 / 血条 / 挥砍 / 粒子
-- ============================================================================
local State = require("state")

-- dse.ui.set_visible 只接受 number, 这里包装布尔转换
local function ui_set_visible(e, v) dse.ui.set_visible(e, v and 1 or 0) end
local G = State.G
local Entities = State.Entities
local kill_entity = State.kill_entity

-- ============================================================================
-- 伤害数字池 (C# c_damagenum[10])
-- ============================================================================
local DamageNumPool = { items = {} }
local function GetDamageNum()
  for _, s in ipairs(DamageNumPool.items) do
    if not s.used then
      s.used = true
      ui_set_visible(s.e, true)
      return s
    end
  end
  local e = dse.ecs.create_entity()
  dse.ecs.add_transform(e, 0, 0, 0, 1, 1, 1)
  local font_tex = G._font_tex or 0
  dse.ui.add_label(e, "", font_tex, 1, 1, 1, 1.0, 22, 28, 1.0, 16, 6, 32, 0, 0)
  local s = { e = e, used = true }
  table.insert(DamageNumPool.items, s)
  return s
end
local function FreeDamageNum(s)
  if s then
    s.used = false
    ui_set_visible(s.e, false)
  end
end

-- ============================================================================
-- 血溅特效池 (C# c_ef_blood + Ef_blood) — 原版 blood.png 4x4 序列动画
-- ============================================================================
local BloodPool = { items = {} }
local function GetBlood()
  for _, s in ipairs(BloodPool.items) do
    if not s.used then
      s.used = true
      dse.ecs.set_mesh_visible(s.e, true)
      return s
    end
  end
  local e = dse.ecs.create_entity()
  dse.ecs.add_transform(e, 0, 0.05, 0, 0.5, 0.5, 0.5)
  -- 4 顶点 quad 承接 blood.png UV (C# Ef_blood: size=0.25)
  local v = {-0.5,0,-0.5, 0.5,0,-0.5, 0.5,0,0.5, -0.5,0,0.5}
  local idx = {0,1,2, 2,3,0}
  dse.ecs.add_mesh_renderer(e, 1.0, 1.0, 1.0, 0.9, v, idx)
  dse.ecs.set_mesh_shader_variant(e, "MESH_UNLIT")
  if State.TEX and State.TEX.blood then
    dse.ecs.set_mesh_texture(e, "albedo", State.TEX.blood)
  end
  -- 初始 UV: 第 0 帧 (左上角格子)
  dse.ecs.set_mesh_uvs(e, {0, 0.75, 0.25, 0.75, 0.25, 1.0, 0, 1.0})
  local s = { e = e, used = true, frame = 0 }
  table.insert(BloodPool.items, s)
  return s
end
local function FreeBlood(s)
  if s then
    s.used = false
    dse.ecs.set_mesh_visible(s.e, false)
  end
end

-- ============================================================================
-- 影子池 (C# c_shadow[15]) — 原版 shadow.glb 染黑半透明
-- ============================================================================
local ShadowPool = { items = {} }
local function CreateShadowEntity()
  local e = dse.ecs.create_entity()
  local bs = State.base_scale("assets/models/shadow.dmesh")
  dse.ecs.add_transform(e, 0, 0.03, 0, bs, bs, bs)
  dse.ecs.mesh_renderer_add(e, State.resolve_path("assets/models/shadow.dmesh"))
  dse.ecs.set_mesh_shader_variant(e, "MESH_UNLIT")
  dse.ecs.set_mesh_color(e, 0.0, 0.0, 0.0, 0.4)
  return e
end
local function GetShadow()
  for _, s in ipairs(ShadowPool.items) do
    if not s.used then
      s.used = true
      dse.ecs.set_mesh_visible(s.e, true)
      return s
    end
  end
  local s = { e = CreateShadowEntity(), used = true }
  table.insert(ShadowPool.items, s)
  return s
end
local function FreeShadow(s)
  if s then
    s.used = false
    dse.ecs.set_mesh_visible(s.e, false)
  end
end

-- ============================================================================
-- 掉落物池 (C# c_item[10]) — 原版 giftbox.glb + giftbox_n.png
-- ============================================================================
local ItemPool = { items = {} }
local function GetItemBoxEntity()
  for _, s in ipairs(ItemPool.items) do
    if not s.used then
      s.used = true
      dse.ecs.set_mesh_visible(s.e, true)
      return s.e
    end
  end
  local te = dse.ecs.create_entity()
  local bs = State.base_scale("assets/models/giftbox.dmesh")
  dse.ecs.add_transform(te, 0, 0.5, 0, 0.4 * bs, 0.4 * bs, 0.4 * bs)
  dse.ecs.mesh_renderer_add(te, State.resolve_path("assets/models/giftbox.dmesh"))
  dse.ecs.set_mesh_shader_variant(te, "MESH_LIT")
  if State.TEX and State.TEX.giftbox then
    dse.ecs.set_mesh_texture(te, "albedo", State.TEX.giftbox)
  end
  local s = { e = te, used = true }
  table.insert(ItemPool.items, s)
  return te
end
local function FreeItemBoxEntity(e)
  for _, s in ipairs(ItemPool.items) do
    if s.e == e then
      s.used = false
      dse.ecs.set_mesh_visible(e, false)
      return
    end
  end
  if e and e ~= 0 then pcall(dse.ecs.destroy_entity, e) end
end

-- ============================================================================
-- 血条 (C# Hp_bar) — 原版 bar_hp.png 2x4 UV 偏移
--   _amount = (1 - hp/maxhp) * 0.5, amountU = right*_amount,
--   amuontV = up * 0.25 * status → UV = originUV + amountU + amuontV
--   originUV 覆盖列0 (u=0~0.5 填充色); 掉血后 u 右移露出列1 (深色底槽)
-- ============================================================================
local function CreateHpBarEntity(y_offset)
  local e = dse.ecs.create_entity()
  dse.ecs.add_transform(e, 0, y_offset, 0, 1, 1, 1)
  local v = {-0.5,0,0.02, 0.5,0,0.02, 0.5,0.09,0.02, -0.5,0.09,0.02}
  local idx = {0,1,2, 2,3,0}
  dse.ecs.add_mesh_renderer(e, 1.0, 1.0, 1.0, 0.9, v, idx)
  dse.ecs.set_mesh_shader_variant(e, "MESH_UNLIT")
  if State.TEX and State.TEX.bar_hp then
    dse.ecs.set_mesh_texture(e, "albedo", State.TEX.bar_hp)
  end
  -- 初始 UV: 满血 (amount=0, status=0) → 列0 整格
  dse.ecs.set_mesh_uvs(e, {0, 0, 0.5, 0, 0.5, 0.25, 0, 0.25})
  dse.ecs.set_mesh_visible(e, false)
  return e
end
-- ratio = 剩余血量比例; status = 血条行 (0=蓝 1=灰 2=绿 3=红)
local function UpdateHpBar(hp_e, x, y_offset, z, ratio, status)
  if not hp_e then return end
  if ratio >= 1 then
    dse.ecs.set_mesh_visible(hp_e, false)
    return
  end
  dse.ecs.set_mesh_visible(hp_e, true)
  dse.ecs.set_transform_position(hp_e, x, y_offset, z)
  -- C# Hp_bar: _amount = (1 - hp/maxhp) * 0.5
  local amount = (1.0 - math.max(0, math.min(1, ratio))) * 0.5
  local st = status or 0
  if st < 0 then st = 0 elseif st > 3 then st = 3 end
  local v0 = st * 0.25
  dse.ecs.set_mesh_uvs(hp_e, {
    amount, v0, 0.5 + amount, v0,
    0.5 + amount, v0 + 0.25, amount, v0 + 0.25,
  })
end

-- ============================================================================
-- 生成特效 (C# Monster_efs.SetDamageNum / CreatBlood)
-- ============================================================================

-- 伤害飘字 (对象池)
local function spawn_damage_text(x, y, z, text, r, g, b, size)
  local s = GetDamageNum()
  dse.ui.set_label_text(s.e, text)
  dse.ui.set_color(s.e, r or 1, g or 1, b or 1, 1.0)
  table.insert(Entities.damage_texts, {
    e = s.e, pool = s, x = x, y = y, z = z, vy = 3.0, life = 0.8, t = 0,
  })
end

-- 血溅特效 (C# CreatBlood — 对象池)
local function spawn_blood_effect(x, y, z, dx, dz, scale)
  local s = GetBlood()
  local sx, sz = 0, 0
  if dx and dz and (dx ~= 0 or dz ~= 0) then
    local m = math.sqrt(dx * dx + dz * dz)
    if m > 0.001 then sx, sz = dx / m, dz / m end
  end
  -- 命中点偏移 (C# _pos + _dir * 0.1 + up * 0.05)
  local bx, bz = x + sx * 0.15, z + sz * 0.15
  dse.ecs.set_transform_position(s.e, bx, y + 0.05, bz)
  dse.ecs.set_transform_rotation(s.e, 0, math.random() * 360, 0)
  dse.ecs.set_transform_scale(s.e, scale or 0.5, scale or 0.5, scale or 0.5)
  table.insert(Entities.particles, {
    e = s.e, pool = s, x = bx, y = y + 0.05, z = bz,
    vx = 0, vy = 0.8, vz = 0, life = 0.25, t = 0, kind = "blood",
  })
end

-- 命中粒子
local function spawn_hit_effect(x, y, z, count, color)
  count = count or 5
  for _ = 1, count do
    local e = dse.ecs.create_entity()
    dse.ecs.add_transform(e, x, y, z, 0.15, 0.15, 0.15)
    local verts = {-0.5,0,0, 0.5,0,0, 0,0.5,0, -0.5,0,0}
    local indices = {0,1,2, 2,3,0}
    local r, g, b = 1.0, 0.2, 0.1
    if color == "ice" then r, g, b = 0.3, 0.7, 1.0
    elseif color == "electric" then r, g, b = 1.0, 1.0, 0.2
    elseif color == "poison" then r, g, b = 0.5, 1.0, 0.2
    elseif color == "skill" then r, g, b = 0.5, 0.5, 1.0
    end
    dse.ecs.add_mesh_renderer(e, r, g, b, 0.9, verts, indices)
    dse.ecs.set_mesh_shader_variant(e, "MESH_UNLIT")
    table.insert(Entities.particles, {
      e = e, x = x, y = y, z = z,
      vx = (math.random() - 0.5) * 4.0,
      vy = math.random() * 3.0 + 1.0,
      vz = (math.random() - 0.5) * 4.0,
      life = 0.3 + math.random() * 0.2, t = 0,
    })
  end
end

-- 挥砍特效 (C# Ef_swing1) — 原版 ef_swordslash.png 2x2 UV 序列动画
-- 原版 SwingOn(delay, cnt_x, cnt_y, uvspeed=20, impact=1, addforce)
local function spawn_swing_ef(x, y, z, yaw, scale, duration, color)
  local e = dse.ecs.create_entity()
  dse.ecs.add_transform(e, x, y, z, scale or 1, scale or 1, scale or 1)
  dse.ecs.set_transform_rotation(e, 0, yaw or 0, 0)
  -- 贴图 quad 承接 ef_swordslash.png (128x128, 2x2 序列)
  local verts = {-0.5,0,0, 0.5,0,0, 0.5,0,0.5, -0.5,0,0.5}
  local indices = {0,1,2, 2,3,0}
  dse.ecs.add_mesh_renderer(e, 1.0, 1.0, 1.0, 0.85, verts, indices)
  dse.ecs.set_mesh_shader_variant(e, "MESH_UNLIT")
  if State.TEX and State.TEX.ef_swing then
    dse.ecs.set_mesh_texture(e, "albedo", State.TEX.ef_swing)
  end
  -- 帧 0: 2x2 网格左上角
  dse.ecs.set_mesh_uvs(e, {0, 0.5, 0.5, 0.5, 0.5, 1.0, 0, 1.0})
  table.insert(Entities.swing_ef, {
    e = e, x = x, y = y, z = z, yaw = yaw or 0,
    life = duration or 0.2, t = 0, max_scale = scale or 1,
    frame = 0, old_frame = -1, frame_speed = 20, cnt_x = 2, cnt_y = 2,
    color = color,
  })
end

-- 冲刺雾效
local function spawn_step_fog(x, z, yaw)
  for _ = 1, 3 do
    local e = dse.ecs.create_entity()
    local fx = x + (math.random() - 0.5) * 0.5
    local fz = z + (math.random() - 0.5) * 0.5
    dse.ecs.add_transform(e, fx, 0.05, fz, 0.3, 0.3, 0.3)
    local verts = {-0.5,0,0.5, 0.5,0,0.5, 0.5,0,-0.5, -0.5,0,-0.5}
    local indices = {0,1,2, 2,3,0}
    dse.ecs.add_mesh_renderer(e, 0.7, 0.7, 0.8, 0.5, verts, indices)
    dse.ecs.set_mesh_shader_variant(e, "MESH_UNLIT")
    table.insert(Entities.particles, {
      e = e, x = fx, y = 0.05, z = fz,
      vx = 0, vy = 0.5, vz = 0,
      life = 0.4, t = 0,
    })
  end
end

-- ============================================================================
-- 武器掉落 (C# WeaponDrop: 原版 blade_s01.glb 旋转下落动画, 2.5s 后消失)
-- ============================================================================
local function spawn_weapon_drop(x, z, yaw)
  local e = dse.ecs.create_entity()
  local bs = State.base_scale("assets/models/blade_s01.dmesh")
  dse.ecs.add_transform(e, x, 0.3, z, 1.2 * bs, 1.2 * bs, 1.2 * bs)
  dse.ecs.mesh_renderer_add(e, State.resolve_path("assets/models/blade_s01.dmesh"))
  dse.ecs.set_mesh_shader_variant(e, "MESH_LIT")
  table.insert(Entities.drops, {
    e = e, x = x, z = z, y = 0.3,
    maxy = 1.7, drop = true, life = 2.5, t = 0, spin = 0,
  })
end

local function UpdateDrops(dt)
  for i = #Entities.drops, 1, -1 do
    local d = Entities.drops[i]
    d.t = d.t + dt
    if d.drop then
      if d.y > 0 then
        -- C# mytransform.Rotate(rotateaxis * dt * 720) — 绕右轴翻转
        d.spin = d.spin + dt * 720
        d.maxy = d.maxy - 3.5 * dt
        d.y = d.y + d.maxy * dt
        if d.y < 0 then d.y = 0 end
      else
        -- C# 落地: 位置归零 y, drop = false
        d.y = 0
        d.drop = false
      end
      if d.e then
        dse.ecs.set_transform_position(d.e, d.x, d.y, d.z)
        dse.ecs.set_transform_rotation(d.e, d.spin, 0, 0)
      end
    end
    if d.t >= d.life then
      kill_entity(d.e)
      table.remove(Entities.drops, i)
    end
  end
end

-- ============================================================================
-- 特效逐帧更新
-- ============================================================================
local function UpdateEffects(dt)
  -- 伤害飘字 (对象池回收)
  for i = #Entities.damage_texts, 1, -1 do
    local dt2 = Entities.damage_texts[i]
    dt2.t = dt2.t + dt
    dt2.y = dt2.y + dt2.vy * dt
    dt2.vy = dt2.vy - 5.0 * dt
    if dt2.e then
      dse.ecs.set_transform_position(dt2.e, dt2.x, dt2.y, dt2.z)
      local s = 1.0 - dt2.t / dt2.life
      if s < 0.1 then s = 0.1 end
    end
    if dt2.t >= dt2.life then
      FreeDamageNum(dt2.pool)
      table.remove(Entities.damage_texts, i)
    end
  end

  -- 粒子 (blood 池化 / 其余直接销毁)
  for i = #Entities.particles, 1, -1 do
    local p = Entities.particles[i]
    p.t = p.t + dt
    p.x = p.x + p.vx * dt
    p.y = p.y + p.vy * dt
    p.z = p.z + p.vz * dt
    p.vy = p.vy - 10.0 * dt
    if p.e then
      dse.ecs.set_transform_position(p.e, p.x, p.y, p.z)
      if p.kind == "blood" then
        -- C# Ef_blood: index = starttime*22, size=0.25, uIndex=idx%4, vIndex=idx/4
        local prog = p.t / p.life
        local bs = 0.6 * (0.6 + prog * 1.6)
        dse.ecs.set_transform_scale(p.e, bs, bs, bs)
        local frame = math.floor(p.t * 22)
        if frame ~= (p.old_frame or -1) then
          p.old_frame = frame
          local uIndex = frame % 4
          local vIndex = math.floor(frame / 4)
          if vIndex > 3 then vIndex = 3 end
          local u0 = uIndex * 0.25
          local v1 = 1.0 - vIndex * 0.25
          local v0 = v1 - 0.25
          dse.ecs.set_mesh_uvs(p.e, {u0, v0, u0 + 0.25, v0, u0 + 0.25, v1, u0, v1})
        end
      else
        local s = 0.15 * (1.0 - p.t / p.life)
        if s < 0.01 then s = 0.01 end
        dse.ecs.set_transform_scale(p.e, s, s, s)
      end
    end
    if p.t >= p.life then
      if p.kind == "blood" then FreeBlood(p.pool) else kill_entity(p.e) end
      table.remove(Entities.particles, i)
    end
  end

  -- 挥砍特效
  for i = #Entities.swing_ef, 1, -1 do
    local sw = Entities.swing_ef[i]
    sw.t = sw.t + dt
    if sw.e then
      local progress = sw.t / sw.life
      local s = sw.max_scale * (1.0 - progress * 0.5)
      dse.ecs.set_transform_scale(sw.e, s, s, s)
      dse.ecs.set_transform_rotation(sw.e, 0, sw.yaw + progress * 60, 0)
      -- C# Ef_swing1: index = starttime * framesPerSecond
      local frame = math.floor(sw.t * sw.frame_speed)
      if frame ~= sw.old_frame then
        sw.old_frame = frame
        if frame < sw.cnt_x * sw.cnt_y then
          local uIndex = frame % sw.cnt_x
          local vIndex = math.floor(frame / sw.cnt_x)
          local u0 = uIndex * (1.0 / sw.cnt_x)
          local u1 = u0 + (1.0 / sw.cnt_x)
          local v1 = 1.0 - vIndex * (1.0 / sw.cnt_y)
          local v0 = v1 - (1.0 / sw.cnt_y)
          dse.ecs.set_mesh_uvs(sw.e, {u0, v0, u1, v0, u1, v1, u0, v1})
        end
      end
    end
    if sw.t >= sw.life then
      kill_entity(sw.e)
      table.remove(Entities.swing_ef, i)
    end
  end

  -- 武器掉落动画 (C# WeaponDrop)
  UpdateDrops(dt)
end

local M = {
  GetItemBoxEntity = GetItemBoxEntity,
  FreeItemBoxEntity = FreeItemBoxEntity,
  FreeShadow = FreeShadow,
  GetShadow = GetShadow,
  FreeDamageNum = FreeDamageNum,
  FreeBlood = FreeBlood,
  CreateHpBarEntity = CreateHpBarEntity,
  UpdateHpBar = UpdateHpBar,
  spawn_damage_text = spawn_damage_text,
  spawn_hit_effect = spawn_hit_effect,
  spawn_blood_effect = spawn_blood_effect,
  spawn_swing_ef = spawn_swing_ef,
  spawn_step_fog = spawn_step_fog,
  spawn_weapon_drop = spawn_weapon_drop,
  UpdateDrops = UpdateDrops,
  UpdateEffects = UpdateEffects,
}

return M
