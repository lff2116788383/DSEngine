-- ============================================================================
-- weapon_damage.lua — Boss 武器系统 (WeaponDamage.cs + AI_Boss01 武器生成)
-- clone_weapon 生命周期: colliderofftime / destroytime / impactDestroy
-- 距离检测代替 OnTriggerEnter, 对玩家伤害经 main.lua 注入 (打破循环 require)
-- ============================================================================
local State = require("state")
local Entities = State.Entities
local Player = State.Player
local dist2d = State.dist2d

-- main.lua 注入的玩家伤害入口 (PlayerDamaged)
local damage_player = function(damage, from_x, from_z)
end

local function set_damage_player(fn)
  damage_player = fn
end

-- ============================================================================
-- 武器实体 (C# clone_weapon) — 原版 blade_n01.glb
-- ============================================================================
local function BossWeaponCreate(boss, atk_index)
  local e = dse.ecs.create_entity()
  local bs = State.base_scale("assets/models/blade_n01.dmesh")
  dse.ecs.add_transform(e, 0, 0, 0, bs, bs, bs)
  dse.ecs.mesh_renderer_add(e, State.resolve_path("assets/models/blade_n01.dmesh"))
  dse.ecs.set_mesh_shader_variant(e, "MESH_LIT")
  dse.ecs.set_mesh_material(e, 0.3, 0.35, 1.0, 0, 0, 0, 1.0, true, false)  -- 金属武器材质
  dse.ecs.set_mesh_visible(e, false)
  local w = {
    e = e, boss = boss, index = atk_index,
    damage = 0, impactDestroy = false,
    destroytime = 0.3, colliderofftime = 0.15,
    currenttime = 0, active = false, hit_done = false,
    collider_enabled = true,
  }
  table.insert(Entities.boss_weapons, w)
  return w
end

-- 武器激活 (C# AI_Boss01.Update: clone_weapon 定位 + WeaponDamage.PressDamage)
local function BossWeaponActivate(boss, atk_index)
  local ws = boss.clone_weapons
  if not ws[atk_index] then
    ws[atk_index] = BossWeaponCreate(boss, atk_index)
  end
  local w = ws[atk_index]
  -- 武器位置: boss + forward * localPos.z + right * localPos.x (C# ef_weapon.localPosition)
  local fx = math.sin(math.rad(boss.yaw))
  local fz = -math.cos(math.rad(boss.yaw))
  local rx = math.cos(math.rad(boss.yaw))
  local rz = math.sin(math.rad(boss.yaw))
  local px = boss.x + fx * 1.4 + rx * 0.15
  local pz = boss.z + fz * 1.4 + rz * 0.15
  dse.ecs.set_transform_position(w.e, px, 0.4, pz)
  dse.ecs.set_transform_rotation(w.e, 0, boss.yaw, 0)
  dse.ecs.set_mesh_visible(w.e, true)
  w.damage = boss.power[atk_index + 1] or 30
  w.currenttime = 0
  w.active = true
  w.collider_enabled = true
  w.hit_done = boss._impact_hit_player or false
end

-- 武器生命周期 (C# WeaponDamage.Update: 距离检测代替 OnTriggerEnter)
local function BossWeaponUpdate(dt)
  for i = #Entities.boss_weapons, 1, -1 do
    local w = Entities.boss_weapons[i]
    if w.active then
      w.currenttime = w.currenttime + dt
      if w.destroytime > 0 then
        if w.colliderofftime > 0 and w.currenttime >= w.colliderofftime then
          w.collider_enabled = false
        end
        -- 碰撞检测 (C# OnTriggerEnter: other.layer == 15)
        if w.collider_enabled and not w.hit_done then
          local wx, _, wz = dse.ecs.get_transform_position(w.e)
          if wx and dist2d(wx, wz, Player.x, Player.z) < 1.8 then
            w.hit_done = true
            damage_player(w.damage, w.boss.x, w.boss.z)
            if w.impactDestroy then
              w.active = false
              dse.ecs.set_mesh_visible(w.e, false)
            end
          end
        end
        if w.currenttime >= w.destroytime then
          w.currenttime = 0
          w.active = false
          dse.ecs.set_mesh_visible(w.e, false)
        end
      end
    end
  end
end

-- ============================================================================
-- 第二武器 (C# ef_secondweapon, attach_weaponEf == 2)
-- ============================================================================
local function BossSecondWeaponCreate(boss)
  local e = dse.ecs.create_entity()
  local bs = State.base_scale("assets/models/blade_s01.dmesh")
  dse.ecs.add_transform(e, 0, 0, 0, 1.3 * bs, 1.3 * bs, 1.3 * bs)
  dse.ecs.mesh_renderer_add(e, State.resolve_path("assets/models/blade_s01.dmesh"))
  dse.ecs.set_mesh_shader_variant(e, "MESH_LIT")
  dse.ecs.set_mesh_material(e, 0.3, 0.35, 1.0, 0, 0, 0, 1.0, true, false)  -- 金属武器材质
  dse.ecs.set_mesh_visible(e, false)
  boss.ef_secondweapon = {
    e = e, boss = boss, active = false,
    damage = 0, currenttime = 0, hit_done = false,
    destroytime = 0.4, colliderofftime = 0.2, collider_enabled = true,
  }
  return boss.ef_secondweapon
end

local function BossSecondWeaponShow(boss)
  local sw = boss.ef_secondweapon
  if not sw then sw = BossSecondWeaponCreate(boss) end
  sw.active = true
  sw.currenttime = 0
  sw.collider_enabled = true
  sw.hit_done = false
  if sw.e then dse.ecs.set_mesh_visible(sw.e, true) end
end

local function BossSecondWeaponHide(boss)
  local sw = boss.ef_secondweapon
  if not sw then return end
  sw.active = false
  if sw.e then dse.ecs.set_mesh_visible(sw.e, false) end
end

-- C# Impact2: 第二武器 PressDamage + Hitcam (Hitcam 由 main 侧调用)
local function BossSecondWeaponImpact(boss)
  local sw = boss.ef_secondweapon
  if not sw then sw = BossSecondWeaponCreate(boss) end
  sw.active = true
  sw.currenttime = 0
  sw.collider_enabled = true
  sw.hit_done = boss._impact_hit_player or false
  sw.damage = boss.power[boss.currentAtk + 1] or 30
end

-- 第二武器生命周期 (同 WeaponDamage)
local function BossSecondWeaponTick(boss, dt)
  local sw = boss.ef_secondweapon
  if not sw or not sw.active then return end
  -- 跟随 Boss (C# ef_secondweapon.parent = mytransform)
  if sw.e then
    dse.ecs.set_transform_position(sw.e, boss.x, 1.2, boss.z)
    dse.ecs.set_transform_rotation(sw.e, 0, boss.yaw, 0)
  end
  sw.currenttime = sw.currenttime + dt
  if sw.colliderofftime > 0 and sw.currenttime >= sw.colliderofftime then
    sw.collider_enabled = false
  end
  if sw.collider_enabled and not sw.hit_done then
    if dist2d(boss.x, boss.z, Player.x, Player.z) < 2.2 then
      sw.hit_done = true
      damage_player(sw.damage, boss.x, boss.z)
    end
  end
  if sw.currenttime >= sw.destroytime then
    BossSecondWeaponHide(boss)
  end
end

local M = {
  BossWeaponCreate = BossWeaponCreate,
  BossWeaponActivate = BossWeaponActivate,
  BossWeaponUpdate = BossWeaponUpdate,
  BossSecondWeaponShow = BossSecondWeaponShow,
  BossSecondWeaponHide = BossSecondWeaponHide,
  BossSecondWeaponImpact = BossSecondWeaponImpact,
  BossSecondWeaponTick = BossSecondWeaponTick,
  set_damage_player = set_damage_player,
}

return M
