-- ============================================================================
-- main.lua — 完整移植自 Unity 逆向 C# 源码
-- 忠实复刻: Cha_Control / AI_Enemy01 / AI_Boss01 / Spawn / UI_Ingame
--           Cam_Move / Cha_Skill / Combat / Effects
-- 操作:
--   WASD/方向键   移动
--   J/鼠标左键    普通攻击 (5段连击)
--   K/鼠标右键    技能
--   L             闪避
--   O             格挡(按住)
--   P             抓取
--   U             切换武器
--   I             召唤武将
--   R             重开
-- ============================================================================

local app = dse.app
local DB = require("database")
-- 共享状态 + 工具函数 (各功能模块经 state 共享, 避免循环 require)
local State = require("state")
local G, Player, Entities = State.G, State.Player, State.Entities
local atan2, clamp, dist2d, lerp, sign = State.atan2, State.clamp, State.dist2d, State.lerp, State.sign
local kill_entity = State.kill_entity
-- 模型基准缩放表 (AssetBuilder 转换 dmesh 后按原始尺寸生成)
local MODEL_SCALE = require("model_scale")

-- 功能模块 (按 C# 源码文件对应拆分)
local CamMove = require("cam_move")
local MonsterEfs = require("monster_efs")
local WeaponDamage = require("weapon_damage")
local Cutin01 = require("cutin01")
local BulletSystem = require("bullet_system")
local EfSystem = require("ef_system")
local SkillSystem = require("skill_system")
local AISystem = require("ai_system")
local UISystem = require("ui_system")
local PetSystem = require("pet_system")
-- 存档系统 (Phase 3, C# Crypto/DataSave/ConvertSaveData)
local SaveSystem = require("save")
-- 游戏外 UI (Phase 3, C# UI_intro/UI_map/UI_skill)
local MenuSystem = require("menu_system")
-- 剧情演出 (Phase 4, C# scenario.cs + DB_Scenario)
local ScenarioSystem = require("scenario_system")

-- 解构为局部变量, 保持原调用点不变
local CAM = CamMove.CAM
local CameraReset = CamMove.CameraReset
local CameraLookTarget = CamMove.CameraLookTarget
local CameraZoomIn = CamMove.CameraZoomIn
local CameraTopview = CamMove.CameraTopview
local CameraHitcam = CamMove.CameraHitcam
local CameraHitcam2 = CamMove.CameraHitcam2
local UpdateCamera = CamMove.UpdateCamera

local spawn_damage_text = MonsterEfs.spawn_damage_text
local spawn_hit_effect = MonsterEfs.spawn_hit_effect
local spawn_blood_effect = MonsterEfs.spawn_blood_effect
local spawn_swing_ef = MonsterEfs.spawn_swing_ef
local spawn_step_fog = MonsterEfs.spawn_step_fog
local CreateHpBarEntity = MonsterEfs.CreateHpBarEntity
local UpdateHpBar = MonsterEfs.UpdateHpBar
local UpdateEffects = MonsterEfs.UpdateEffects
local UpdateDrops = MonsterEfs.UpdateDrops
local spawn_weapon_drop = MonsterEfs.spawn_weapon_drop
local GetItemBoxEntity = MonsterEfs.GetItemBoxEntity
local FreeItemBoxEntity = MonsterEfs.FreeItemBoxEntity
local FreeShadow = MonsterEfs.FreeShadow
local FreeDamageNum = MonsterEfs.FreeDamageNum
local FreeBlood = MonsterEfs.FreeBlood
local GetShadow = MonsterEfs.GetShadow

local BossWeaponActivate = WeaponDamage.BossWeaponActivate
local BossWeaponUpdate = WeaponDamage.BossWeaponUpdate
local BossSecondWeaponShow = WeaponDamage.BossSecondWeaponShow
local BossSecondWeaponHide = WeaponDamage.BossSecondWeaponHide
local BossSecondWeaponImpact = WeaponDamage.BossSecondWeaponImpact
local BossSecondWeaponTick = WeaponDamage.BossSecondWeaponTick

local CutinOn = Cutin01.CutinOn
local UpdateCutin = Cutin01.UpdateCutin

-- 依赖注入 (WeaponDamage 需要 main.lua 的 PlayerDamaged, 在 Awake 中注入;
-- Cutin01 需要 CameraReset, 此处直接注入模块导出)
Cutin01.set_camera(CamMove.CameraReset)

-- ── 键码 ──────────────────────────────────────────────────────────────
local KEY_A, KEY_D, KEY_W, KEY_S = 65, 68, 87, 83
local KEY_LEFT, KEY_RIGHT, KEY_UP, KEY_DOWN = 263, 262, 265, 264
local KEY_J, KEY_K, KEY_L, KEY_O, KEY_P, KEY_R = 74, 75, 76, 79, 80, 82
local KEY_U, KEY_I, KEY_Q, KEY_E, KEY_F = 85, 73, 81, 69, 70
local KEY_SPACE = 32
local KEY_ESCAPE = 256
local KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6 = 49, 50, 51, 52, 53, 54

-- ── 资产路径 ──────────────────────────────────────────────────────────
local function file_exists(path)
  local f = io.open(path, "rb")
  if f then f:close(); return true end
  return false
end
local function resolve_path(path)
  if file_exists(path) then return path end
  -- 依次尝试: data/ → assets/ → 模板仓库相对路径 (从引擎根目录运行时可用)
  for _, p in ipairs({"data/" .. path, "assets/" .. path, "templates/topdown_3d/" .. path}) do
    if file_exists(p) then return p end
  end
  return path
end
local function audio_path(path) return resolve_path(path) end
local function model_path(path) return resolve_path(path) end

-- ── 音频 (原版素材) ──────────────────────────────────────────────────
local S = {}
local function LoadAudio()
  S.bgm_stage1 = audio_path("assets/audio/bgm_stage1.mp3")
  S.bgm_stage2 = audio_path("assets/audio/bgm_stage2.mp3")
  S.bgm_stage3 = audio_path("assets/audio/bgm_stage3.mp3")
  S.bgm_boss   = audio_path("assets/audio/bgm_boss.mp3")
  S.bgm_intro  = audio_path("assets/audio/bgm_intro.mp3")
  S.bgm_victory= audio_path("assets/audio/bgm_victory.mp3")
  S.bgm_fail   = audio_path("assets/audio/bgm_fail.mp3")
  S.slash      = audio_path("assets/audio/slash0.mp3")
  S.slash1     = audio_path("assets/audio/swing1.mp3")
  S.slash2     = audio_path("assets/audio/spear.mp3")
  S.coin       = audio_path("assets/audio/coin.mp3")
  S.hurt       = audio_path("assets/audio/breath_pain.mp3")
  S.mon_die    = audio_path("assets/audio/mon_scream1.mp3")
  S.skill      = audio_path("assets/audio/skillstart.mp3")
  S.getitem    = audio_path("assets/audio/getitem.mp3")
  S.horse      = audio_path("assets/audio/horse_cry.mp3")
  S.dodge      = audio_path("assets/audio/dodge.mp3")
  S.block      = audio_path("assets/audio/block.mp3")
  S.boom       = audio_path("assets/audio/boom1.mp3")
  S.footstep   = audio_path("assets/audio/footstep.mp3")
  S.guard_break= audio_path("assets/audio/guard_break.mp3")
  S.timewoosh  = audio_path("assets/audio/timewoosh.mp3")
  S.boxdrop    = audio_path("assets/audio/boxdrop.mp3")
  S.boxopen    = audio_path("assets/audio/boxopen.mp3")
  S.chain      = audio_path("assets/audio/chain_multi.mp3")
  S.hammer     = audio_path("assets/audio/hammer.mp3")
  S.explode    = audio_path("assets/audio/explode1.mp3")
  S.npc        = audio_path("assets/audio/npc_scream.mp3")
  S.wood_break = audio_path("assets/audio/wood_break.mp3")
  S.bite       = audio_path("assets/audio/bite.mp3")
  S.wagon      = audio_path("assets/audio/wagon.mp3")
  S.click      = audio_path("assets/audio/click2.mp3")
  S.destroy    = audio_path("assets/audio/destroy_exp.mp3")
  S.forge      = audio_path("assets/audio/forge.mp3")
  S.jin        = audio_path("assets/audio/jin.mp3")
  S.hup1       = audio_path("assets/audio/main_hup1.mp3")
  S.hup2       = audio_path("assets/audio/main_hup2.mp3")
  S.hup3       = audio_path("assets/audio/main_hup3.mp3")
  S.skill1     = audio_path("assets/audio/main_skill1.mp3")
  S.skill2     = audio_path("assets/audio/main_skill2.mp3")
  S.ride_stage = audio_path("assets/audio/ride_stage.mp3")
  S.split_blood= audio_path("assets/audio/split_blood.mp3")
  S.uns_growl  = audio_path("assets/audio/uns_growl.mp3")
  S.unm_growl  = audio_path("assets/audio/unm_growl.mp3")
  S.unl_growl  = audio_path("assets/audio/unl_growl.mp3")
  S.uns_damage = {
    audio_path("assets/audio/uns_damage1.mp3"),
    audio_path("assets/audio/uns_damage2.mp3"),
    audio_path("assets/audio/uns_damage3.mp3"),
    audio_path("assets/audio/uns_damage4.mp3"),
  }
end

-- ── 武将模型映射 (原版 cha01_01~17) ──────────────────────────────────
local CHA_MODELS = {
  "assets/models/cha01_01.dmesh", "assets/models/cha01_02.dmesh",
  "assets/models/cha01_03.dmesh", "assets/models/cha01_04.dmesh",
  "assets/models/cha01_05.dmesh", "assets/models/cha01_06.dmesh",
  "assets/models/cha01_07.dmesh", "assets/models/cha01_08.dmesh",
  "assets/models/cha01_09.dmesh", "assets/models/cha01_10.dmesh",
  "assets/models/cha01_11.dmesh", "assets/models/cha01_12.dmesh",
  "assets/models/cha01_13.dmesh", "assets/models/cha01_14.dmesh",
  "assets/models/cha01_15.dmesh", "assets/models/cha01_16.dmesh",
  "assets/models/cha01_17.dmesh",
}
-- 普通怪物 (DB_Monster 0-15 → mon_0~15)
local MON_MODELS = {
  "assets/models/mon_0.dmesh", "assets/models/mon_1.dmesh",
  "assets/models/mon_2.dmesh", "assets/models/mon_3.dmesh",
  "assets/models/mon_4.dmesh", "assets/models/mon_5.dmesh",
  "assets/models/mon_6.dmesh", "assets/models/mon_7.dmesh",
  "assets/models/mon_8.dmesh", "assets/models/mon_9.dmesh",
  "assets/models/mon_10.dmesh", "assets/models/mon_11.dmesh",
  "assets/models/mon_12.dmesh", "assets/models/mon_13.dmesh",
  "assets/models/mon_14.dmesh", "assets/models/mon_15.dmesh",
}
-- Boss (DB_Boss 0-11 → mon_16~25 + mon0285 + mon2)
local BOSS_MODELS = {
  "assets/models/mon_16.dmesh", "assets/models/mon_17.dmesh",
  "assets/models/mon_18.dmesh", "assets/models/mon_19.dmesh",
  "assets/models/mon_20.dmesh", "assets/models/mon_21.dmesh",
  "assets/models/mon_22.dmesh", "assets/models/mon_23.dmesh",
  "assets/models/mon_24.dmesh", "assets/models/mon_25.dmesh",
  "assets/models/mon0285.dmesh", "assets/models/mon2.dmesh",
}
local MAP_MODELS = {
  "assets/models/map01.dmesh", "assets/models/map02.dmesh",
  "assets/models/map03.dmesh", "assets/models/map4.dmesh",
  "assets/models/map05.dmesh",
}
local STRUCT_MODELS = {
  barrack = "assets/models/barrack.dmesh",
  tower   = "assets/models/tower.dmesh",
  barricade = "assets/models/barricade.dmesh",
  basecamp = "assets/models/basecamp.dmesh",
  tank    = "assets/models/tank.dmesh",
  cart    = "assets/models/cart.dmesh",
  horse   = "assets/models/horse.dmesh",
}

-- ── 原版贴图 (Texture2D/*.png) ────────────────────────────────────────
-- 注意: dse.ecs.set_mesh_texture 的第三参数是文件路径字符串, 内部自行加载
local TEX = {}
local function LoadTextures()
  local t = "assets/textures/"
  local function T(p) return resolve_path(t .. p) end
  -- 角色时装贴图 cha01_XX → costumeXX
  TEX.costume = {}
  for i = 1, 17 do
    TEX.costume[i] = T(string.format("costume%02d.png", i))
  end
  -- Boss 贴图 mon_16~25 → boss01~10; mon0285/mon2 → mon0285/bosssp
  TEX.boss = {}
  for i = 1, 10 do
    TEX.boss[i] = T(string.format("boss%02d.png", i))
  end
  TEX.mon0285 = T("mon0285.png")
  TEX.bosssp  = T("bosssp.png")
  TEX.finalboss = T("finalboss_b.png")
  -- 普通怪物贴图池 (原版怪物武器/兵种贴图按索引分配)
  TEX.mon = {}
  local mon_texs = {
    "mon_axe01","mon_bow01","mon_crossbow01","mon_dagger01",
    "mon_greataxe","mon_hammer01","mon_shield01","mon_spear01",
    "mon_sword01","m_beast1","m_beast2","m_beast3",
    "wolfhead","knight","jangkak","junwui",
  }
  for i, name in ipairs(mon_texs) do
    TEX.mon[i] = T(name .. ".png")
  end
  -- 建筑/地图贴图 (同名映射)
  TEX.barrack  = T("barrack.png")
  TEX.basecamp = T("basecamp.png")
  TEX.tank     = T("tank.png")
  TEX.cart     = T("cart.png")
  TEX.map = {}
  for i = 1, 5 do
    TEX.map[i] = T(string.format("map%02d.png", i))
  end
  TEX.sky_ground = {}
  for i = 1, 5 do
    TEX.sky_ground[i] = T("sky_ground" .. i .. ".png")
  end
  -- 血条/血溅/掉落 (C# Hp_bar / Monster_efs / Itemdrop)
  TEX.bar_hp  = T("bar_hp.png")
  TEX.blood   = T("blood.png")
  TEX.blood_split = T("blood_split.png")
  -- 挥砍特效 (C# Ef_swing1 使用原版贴图 2x2 序列)
  TEX.ef_swing = T("ef_swordslash.png")
  TEX.treasure = {}
  for i = 1, 24 do
    TEX.treasure[i] = T("treasure" .. i .. ".png")
  end
  TEX.giftbox = T("giftbox_n.png")
  -- Boss 名图 (Cutin01 大图)
  TEX.boss_name = {}
  for i = 1, 11 do
    TEX.boss_name[i] = T("name_boss" .. i .. ".png")
  end
  -- 共享给功能模块 (monster_efs / weapon_damage / cutin01)
  State.TEX = TEX
end

-- ============================================================================
-- 实体创建辅助
-- ============================================================================
-- 从模型路径提取模型名 (去掉目录与扩展名) 用于查 MODEL_SCALE 表
local function mesh_key(mesh)
  local m = tostring(mesh or "")
  local base = m:match("([^/\\]+)%.[A-Za-z0-9]+$")
  return base or m
end
-- 模型基准缩放 (按原始尺寸放大到目标尺寸, 见 model_scale.lua)
local function base_scale(mesh)
  return MODEL_SCALE[mesh_key(mesh)] or 1
end
local function spawn_model(mesh, x, y, z, sx, sy, sz, tex)
  local bs = base_scale(mesh)
  local e = dse.ecs.create_entity()
  dse.ecs.add_transform(e, x, y, z, (sx or 1) * bs, (sy or 1) * bs, (sz or 1) * bs)
  dse.ecs.mesh_renderer_add(e, mesh)
  dse.ecs.set_mesh_shader_variant(e, "MESH_LIT")
  dse.ecs.set_mesh_material(e, 0.0, 0.7, 1.0, 0, 0, 0, 1.0, true, false)
  if tex then dse.ecs.set_mesh_texture(e, "albedo", tex) end
  return e, bs
end

local function spawn_ground_plane(scale, r, g, b)
  local e = dse.ecs.create_entity()
  dse.ecs.add_transform(e, 0, -0.05, 0, 40 * (scale or 2), 0.1, 40 * (scale or 2))
  local verts = {-0.5,0,0.5, 0.5,0,0.5, 0.5,0,-0.5, -0.5,0,-0.5}
  local indices = {0,1,2, 2,3,0}
  dse.ecs.add_mesh_renderer(e, r or 0.35, g or 0.40, b or 0.30, 1.0, verts, indices)
  dse.ecs.set_mesh_shader_variant(e, "MESH_LIT")
  dse.ecs.set_mesh_material(e, 0.0, 0.85, 1.0, 0, 0, 0, 1.0, true, true)
  return e
end

-- ============================================================================
-- 清理
-- ============================================================================
local function ClearLevel()
  for _, en in ipairs(Entities.enemies) do
    if en.arrow_e then kill_entity(en.arrow_e); en.arrow_e = nil end
    if en.hp_bar_e then kill_entity(en.hp_bar_e); en.hp_bar_e = nil end
    if en.shadow then FreeShadow(en.shadow); en.shadow = nil end
    kill_entity(en.e)
  end
  if Entities.boss then
    kill_entity(Entities.boss.e)
    if Entities.boss.hp_bar_e then kill_entity(Entities.boss.hp_bar_e) end
    if Entities.boss.shadow then FreeShadow(Entities.boss.shadow) end
    if Entities.boss.ef_secondweapon and Entities.boss.ef_secondweapon.e then
      kill_entity(Entities.boss.ef_secondweapon.e)
    end
  end
  for _, s in ipairs(Entities.structures) do kill_entity(s.e) end
  for _, t in ipairs(Entities.treasures) do FreeItemBoxEntity(t.e) end
  for _, d in ipairs(Entities.decor) do kill_entity(d) end
  for _, dt in ipairs(Entities.damage_texts) do FreeDamageNum(dt.pool) end
  for _, p in ipairs(Entities.particles) do
    if p.kind == "blood" then FreeBlood(p.pool) else kill_entity(p.e) end
  end
  for _, sw in ipairs(Entities.swing_ef) do kill_entity(sw.e) end
  -- 子弹系统清理 (BulletSystem 内部管理, 不再使用 Entities.bullets)
  BulletSystem.clear()
  SkillSystem.clear()
  AISystem.clear()
  UISystem.clear()
  for _, ef in ipairs(Entities.effects) do kill_entity(ef.e) end
  for _, w in ipairs(Entities.boss_weapons) do
    w.active = false
    if w.e then dse.ecs.set_mesh_visible(w.e, false) end
  end
  for _, d in ipairs(Entities.drops) do kill_entity(d.e) end
  kill_entity(Entities.ground)
  kill_entity(G.player_e)
  -- 特殊关目标 (运粮车/大本营)
  if Entities.objective and Entities.objective.e then
    kill_entity(Entities.objective.e)
  end
  Entities.objective = nil
  -- 原地清空实体表 (各模块持有同一引用, 不能整表替换)
  State.reset_entities()
  G.player_e = nil
  G.boss_e = nil
  G.boss_active = false
  -- 相机复位 (C# ResetCam)
  CameraReset()
end

-- ============================================================================
-- 玩家控制器 (Cha_Control 完整移植)
-- ============================================================================

-- 随机攻击力 (RndAtk)
local function RndAtk(critical_hit)
  Player.atk = math.random(Player.minatk, Player.maxatk + 1)
  if critical_hit and Player.critical >= math.random(0, 100) then
    Player.atk = Player.maxatk
    spawn_damage_text(Player.x, 2.5, Player.z, "暴击!", 1.0, 0.2, 0.2, 20)
  end
end

-- SP 充能 (Spcharge)
local function Spcharge(amount)
  Player.sp = clamp(Player.sp + amount, 0, Player.maxsp)
end

-- 无敌 (Invincibility, C# 同时置 hitrate=200 提高敌人命中率)
local function Invincibility(t)
  Player.isinvincibility = true
  Player.target_invincibility = t
  Player.delay_invincibility = 0
  Player.hitrate = 200
end

-- 攻击力提升 (AttakUp)
local function AttakUp(factor)
  Player.maxatk = math.floor(Player.maxatk * (1 + factor))
  Player.minatk = math.floor(Player.minatk * (1 + factor))
end

-- 重置攻击力 (ResetAtk)
local function ResetAtk()
  local wdata = DB.DB_Weapon[Player.weapon_kind]
  Player.attackkind_factor = wdata.attackkind_factor
end

-- 重置力量 (ResetPower)
local function ResetPower()
  Player.maxatk = Player.level * 5 + 30
  Player.minatk = math.floor(Player.maxatk * 0.6)
  SkillSystem.set_base_damage(Player.maxatk)
end

-- 切换武器 (ChangeCharacter)
local function ChangeCharacter(weapon_kind)
  local wdata = DB.DB_Weapon[weapon_kind]
  if not wdata then return end
  Player.weapon_kind = weapon_kind
  Player.battlestyle = weapon_kind
  Player.attackkind_factor = wdata.attackkind_factor
  Player.atkspd = 0.02 + wdata.speed_mod
  Player.movespeed = 0.48
  -- 更新模型 (原版 cha01_XX + costumeXX)
  if G.player_e then
    local model = CHA_MODELS[weapon_kind + 1] or CHA_MODELS[1]
    dse.ecs.set_mesh_path(G.player_e, model)
    dse.ecs.set_mesh_shader_variant(G.player_e, "MESH_LIT")
    if TEX.costume then
      dse.ecs.set_mesh_texture(G.player_e, "albedo", TEX.costume[weapon_kind + 1] or TEX.costume[1])
    end
  end
  Player.curruntattack = Player.attackkind_factor
end

-- 攻击启动 (AttackOn) — 核心战斗逻辑
local function AttackOn(mon_x, mon_z)
  -- 计算攻击方向
  local dx, dz = mon_x - Player.x, mon_z - Player.z
  local mag = math.sqrt(dx*dx + dz*dz)
  if mag > 0.001 then dx, dz = dx/mag, dz/mag end
  local attackDot = dx * Player.dir_x + dz * Player.dir_z

  -- 距离检查
  if mag > 0.2 then
    if attackDot < 0.5 then
      Player.chamovestat = 17 -- 转身攻击
      return
    end
  elseif attackDot < -0.2 then
    Player.chamovestat = 17
    return
  end

  RndAtk(true)
  Player.speedfactor = 5.2

  -- 冲刺攻击
  if Player.chamovestat == 3 then
    Player.curruntattack = -1
    Invincibility(1.0)
  -- 连击递增
  elseif Player.combotime > 0 then
    Player.curruntattack = (Player.curruntattack + 1 - Player.attackkind_factor) % 5 + Player.attackkind_factor
  else
    Player.curruntattack = Player.attackkind_factor
  end

  -- 设置攻击方向
  Player.atk_dir_x, Player.atk_dir_z = dx, dz
  Player.yaw = math.deg(atan2(dx, -dz))
  Player.dir_x, Player.dir_z = dx, dz

  -- 攻击音效
  if math.random(0, 4) < 2 then
    if S.slash then dse.audio.play_sfx(S.slash, 0.8, 0) end
  end

  -- 根据武器和攻击段数执行
  local ca = Player.curruntattack
  local atkspd = Player.atkspd

  if ca == -1 then
    -- 冲刺攻击
    Player.chamovestat = 19
    Player.attacking = 0.48
    Player.visual_state = "dash_attack"
    Player.visual_duration = 0.48
    spawn_swing_ef(Player.x, 0.055, Player.z, Player.yaw, 2.4, 0.48)
    Player.knockback_x = dx * 50
    Player.knockback_z = dz * 50
    Player.knockback_timer = 0.1
    if S.boom then dse.audio.play_sfx(S.boom, 0.7, 0) end

  elseif ca >= 0 and ca < 5 then
    -- 单刀 5 段连击
    local atk_dur = {0.15, 0.15, 0.20, 0.28, 0.21}
    local atk_force = {110, 110, 110, 30, 0}
    Player.chamovestat = (ca == 3) and 12 or 11
    Player.attacking = atk_dur[ca + 1] - atkspd
    Player.visual_state = "attack" .. (ca + 1)
    Player.visual_duration = atk_dur[ca + 1] - atkspd
    spawn_swing_ef(Player.x, 0.055 + ca * 0.02, Player.z, Player.yaw, 1.6, atk_dur[ca + 1])
    if atk_force[ca + 1] > 0 then
      Player.knockback_x = dx * atk_force[ca + 1]
      Player.knockback_z = dz * atk_force[ca + 1]
      Player.knockback_timer = 0.05
    end
    -- 第 2、5 段是升龙攻击
    Player.attack_rising = (ca == 1 or ca == 4)

  elseif ca >= 10 and ca < 15 then
    -- 双刀 5 段连击
    local atk_dur = {0.15, 0.20, 0.15, 0.11, 0.40}
    local atk_force = {120, 120, 0, 150, 0}
    Player.chamovestat = 11
    Player.attacking = atk_dur[ca - 9] - atkspd
    Player.visual_state = "attack" .. (ca - 9)
    Player.visual_duration = atk_dur[ca - 9] - atkspd
    spawn_swing_ef(Player.x, 0.06, Player.z, Player.yaw, 1.3, atk_dur[ca - 9])
    if atk_force[ca - 9] > 0 then
      Player.knockback_x = dx * atk_force[ca - 9]
      Player.knockback_z = dz * atk_force[ca - 9]
      Player.knockback_timer = 0.05
    end
    Player.attack_rising = (ca == 12 or ca == 14)

  elseif ca >= 20 and ca < 25 then
    -- 长枪 5 段连击
    local atk_dur = {0.15, 0.42, 0.50, 0.36, 0.40}
    local atk_force = {160, 130, 90, 0, 0}
    Player.chamovestat = (ca == 22) and 12 or 11
    Player.attacking = atk_dur[ca - 19] - atkspd
    Player.visual_state = "attack" .. (ca - 19)
    Player.visual_duration = atk_dur[ca - 19] - atkspd
    spawn_swing_ef(Player.x, 0.06, Player.z, Player.yaw, 2.0, atk_dur[ca - 19])
    if atk_force[ca - 19] > 0 then
      Player.knockback_x = dx * atk_force[ca - 19]
      Player.knockback_z = dz * atk_force[ca - 19]
      Player.knockback_timer = 0.05
    end
    Player.attack_rising = (ca == 20 or ca == 21 or ca == 24)

  elseif ca >= 30 and ca < 35 then
    -- 弓箭 5 段连击 — 延迟射击
    local shoot_delay = {0.3, 0.7, 0.8, 0.8, 0.8}
    Player.chamovestat = (ca == 32) and 12 or 11
    Player.attacking = shoot_delay[ca - 29] - atkspd
    Player.visual_state = "attack" .. (ca - 29)
    Player.visual_duration = shoot_delay[ca - 29] - atkspd
    -- 在攻击中途发射箭矢
    Player._arrow_delay = shoot_delay[ca - 29] * 0.5
    Player._arrow_ready = true
    Player.knockback_x = -dx * 30
    Player.knockback_z = -dz * 30
    Player.knockback_timer = 0.05
    -- C# 弓 3/4/5 段为升龙攻击
    Player.attack_rising = (ca >= 32)

  elseif ca >= 40 and ca < 45 then
    -- 法杖 5 段连击 — 拳/弹
    local atk_dur = {0.10, 0.30, 0.24, 0.40, 0.60}
    Player.chamovestat = (ca == 43) and 12 or 11
    Player.attacking = atk_dur[ca - 39] - atkspd
    Player.visual_state = "attack" .. (ca - 39)
    Player.visual_duration = atk_dur[ca - 39] - atkspd
    spawn_swing_ef(Player.x, 0.06, Player.z, Player.yaw, 1.0, atk_dur[ca - 39])
    if ca == 43 then Invincibility(1.5 - atkspd) end
    -- 发射法术弹
    Player._bullet_delay = atk_dur[ca - 39] * 0.5
    Player._bullet_ready = true
    -- C# 法杖 2/4 段为升龙攻击
    Player.attack_rising = (ca == 41 or ca == 43)

  elseif ca >= 50 and ca < 55 then
    -- 法器 5 段连击 — 魔法
    local atk_dur = {0.20, 0.40, 0.30, 0.50, 0.30}
    local atk_force = {10, 10, 10, -60, 80}
    Player.chamovestat = (ca == 53) and 12 or 11
    Player.attacking = atk_dur[ca - 49] - atkspd
    Player.visual_state = "attack" .. (ca - 49)
    Player.visual_duration = atk_dur[ca - 49] - atkspd
    spawn_swing_ef(Player.x, 0.06, Player.z, Player.yaw, 1.6, atk_dur[ca - 49], "skill")
    if atk_force[ca - 49] ~= 0 then
      Player.knockback_x = dx * atk_force[ca - 49]
      Player.knockback_z = dz * atk_force[ca - 49]
      Player.knockback_timer = 0.05
    end
    -- 发射魔法弹
    if ca < 53 then
      Player._magic_delay = atk_dur[ca - 49] * 0.5
      Player._magic_ready = true
    else
      -- 火焰飞溅
      Player._firesplash = true
    end
    -- C# 法器 2/4/5 段为升龙攻击
    Player.attack_rising = (ca == 51 or ca == 53 or ca == 54)
  end

  Player.combotime = 1.0
end

-- 玩家攻击检测
local function PlayerAttackInput()
  if Player.attacking > 0 or Player.chamovestat < -1 or Player.chamovestat > 50 then return end
  if not Player.isplaycha then return end

  -- 寻找最近敌人
  local nearest_dist = 999
  local nearest_x, nearest_z = 0, 0
  local found = false

  for _, en in ipairs(Entities.enemies) do
    if not en.dead then
      local d = dist2d(Player.x, Player.z, en.x, en.z)
      if d < nearest_dist then
        nearest_dist = d
        nearest_x, nearest_z = en.x, en.z
        found = true
      end
    end
  end
  if Entities.boss and not Entities.boss.dead then
    local d = dist2d(Player.x, Player.z, Entities.boss.x, Entities.boss.z)
    if d < nearest_dist then
      nearest_dist = d
      nearest_x, nearest_z = Entities.boss.x, Entities.boss.z
      found = true
    end
  end

  -- 如果有目标且在范围内，朝向目标攻击
  if found and nearest_dist < 5.0 then
    AttackOn(nearest_x, nearest_z)
  else
    -- 无目标时朝面向方向攻击
    local dx, dz = math.sin(math.rad(Player.yaw)), -math.cos(math.rad(Player.yaw))
    AttackOn(Player.x + dx, Player.z + dz)
  end
end

-- 闪避 (Dodge)
local function PlayerDodge()
  if Player.chamovestat < -1 or Player.chamovestat > 50 then return end
  if Player.attacking > 0 then return end
  if Player.sp < 4 then return end  -- C# weaponweight*2 = 4

  Spcharge(-4)
  Player.chamovestat = -1
  Player.dodge_timer = 0.5
  Player.visual_state = "dodge"
  Player.visual_duration = 0.5
  Invincibility(0.5)

  -- 闪避方向 = 移动方向
  local dx, dz = 0, 0
  if app.get_key(KEY_A) or app.get_key(KEY_LEFT)  then dx = dx - 1 end
  if app.get_key(KEY_D) or app.get_key(KEY_RIGHT) then dx = dx + 1 end
  if app.get_key(KEY_W) or app.get_key(KEY_UP)    then dz = dz - 1 end
  if app.get_key(KEY_S) or app.get_key(KEY_DOWN)  then dz = dz + 1 end
  if dx == 0 and dz == 0 then
    dx = math.sin(math.rad(Player.yaw))
    dz = -math.cos(math.rad(Player.yaw))
  end
  local mag = math.sqrt(dx*dx + dz*dz)
  if mag > 0.001 then dx, dz = dx/mag, dz/mag end
  Player.dodge_dx = dx * 8.0
  Player.dodge_dz = dz * 8.0
  Player.yaw = math.deg(atan2(dx, -dz))

  if S.dodge then dse.audio.play_sfx(S.dodge, 0.7, 0) end
  spawn_step_fog(Player.x, Player.z, Player.yaw)
end

-- 格挡 (Block)
local function PlayerBlock(dt)
  if Player.chamovestat < -1 or Player.chamovestat > 50 then return end
  if Player.attacking > 0 then return end

  Player.chamovestat = 21
  Player.visual_state = "block"
  Player.visual_duration = 0.1
  Player.block_timer = 0.1

  if S.block then dse.audio.play_sfx(S.block, 0.5, 0) end
end

-- 抓取 (Grab)
local function PlayerGrab()
  if Player.chamovestat < -1 or Player.chamovestat > 50 then return end
  if Player.attacking > 0 then return end
  if Player.sp < 50 then return end

  -- 寻找最近的可抓取敌人
  local nearest_dist = 999
  local nearest_en = nil
  for _, en in ipairs(Entities.enemies) do
    if not en.dead and not en.grabed then
      local d = dist2d(Player.x, Player.z, en.x, en.z)
      if d < 2.0 and d < nearest_dist then
        nearest_dist = d
        nearest_en = en
      end
    end
  end

  if nearest_en then
    -- 抓取动画按怪物 kind/sizekind 选择 (C# 7 种抓取; 大型不可抓)
    local mdata = DB.DB_Monster[nearest_en.enemykind]
    local sizekind = (mdata and mdata.sizekind) or 10
    if sizekind >= 24 then
      spawn_damage_text(Player.x, 2.5, Player.z, "目标太大", 0.7, 0.7, 0.7, 14)
      return
    end
    Spcharge(-50)
    -- C# Grab: Heal(cha_maxhp * 0.1) 抓取回复
    local heal = math.floor(Player.maxhp * 0.1)
    Player.hp = math.min(Player.hp + heal, Player.maxhp)
    spawn_damage_text(Player.x, 2.5, Player.z, "+" .. heal .. " HP", 0.3, 1.0, 0.3, 18)
    Player.chamovestat = 112
    Player.grab_timer = 1.6
    -- grabstyle: 1=小型甩出 2=中型举摔 3=人型投掷 (C# grabstyle 按 sizekind/kind)
    local grabstyle = 1
    if sizekind <= 10 then
      grabstyle = (mdata and mdata.kind == 1) and 1 or 3
    else
      grabstyle = 2
    end
    Player.visual_state = (grabstyle == 2) and "grab_lift" or "grab"
    Player.visual_duration = 1.6
    nearest_en.grabed = true
    nearest_en.grabstyle = grabstyle
    nearest_en.grab_phase = 1
    nearest_en.monmovestat = -2
    nearest_en.visual_state = "grabbed"
    nearest_en.grab_timer = 1.6
    nearest_en.movespeed = 0
    nearest_en.attackdir_x = nearest_en.x - Player.x
    nearest_en.attackdir_z = nearest_en.z - Player.z
    local gmag = math.sqrt(nearest_en.attackdir_x * nearest_en.attackdir_x + nearest_en.attackdir_z * nearest_en.attackdir_z)
    if gmag > 0.001 then nearest_en.attackdir_x = nearest_en.attackdir_x / gmag; nearest_en.attackdir_z = nearest_en.attackdir_z / gmag end
    G.grappling = G.grappling + 1
    G.combo = G.combo + 1
    G.combo_timer = 2.0
    if S.slash then dse.audio.play_sfx(S.slash, 0.9, 0) end

    -- 抓取伤害 (由 UpdateEnemies 中的抓取阶段处理)
  end
end

-- 追击攻击 (C# attackex1: 蓄力 Eximpact 后 QTE 成功触发, 前方大范围伤害)
local function PlayerAttackEx1()
  Player.attacking = 0.4
  Player.visual_state = "attackex1"
  Player.visual_duration = 0.4
  Player.chamovestat = 101
  local dx, dz = math.sin(math.rad(Player.yaw)), -math.cos(math.rad(Player.yaw))
  for _, en in ipairs(Entities.enemies) do
    if not en.dead then
      local d = dist2d(Player.x, Player.z, en.x, en.z)
      if d <= 5.0 then
        local adx, adz = en.x - Player.x, en.z - Player.z
        local ang = math.deg(atan2(adx, -adz))
        local diff = ((ang - Player.yaw + 180) % 360) - 180
        if math.abs(diff) < 100 then
          EnemyDamaged(en, math.floor(Player.atk * 1.5), Player.x, Player.z, "rising")
        end
      end
    end
  end
  spawn_swing_ef(Player.x, 0.06, Player.z, Player.yaw, 4.0, 0.5)
  CameraHitcam()
  if S.boom then dse.audio.play_sfx(S.boom, 0.7, 0) end
end

-- 受伤 (Damaged)
local function PlayerDamaged(damage, from_x, from_z)
  if Player.isinvincibility or Player.chamovestat > 50 then return end
  if not Player.life then return end

  local dx, dz = Player.x - from_x, Player.z - from_z
  local mag = math.sqrt(dx*dx + dz*dz)
  if mag > 0.001 then dx, dz = dx/mag, dz/mag end

  -- 闪避判定
  if Player.evasion >= math.random(0, 100) and Player.chamovestat >= 0 then
    Player.chamovestat = -1
    Invincibility(1.0)
    Player.yaw = math.deg(atan2(dx, -dz))
    Player.knockback_x = dx * 140
    Player.knockback_z = dz * 140
    Player.knockback_timer = 0.15
    Player.dodge_timer = 0.3
    Player.visual_state = "evade"
    Player.visual_duration = 0.3
    spawn_damage_text(Player.x, 2.5, Player.z, "闪避!", 0.5, 1.0, 0.5, 20)
    return
  end

  -- 格挡判定
  if Player.chamovestat == 21 then
    if Player.guard_break > math.random(0, 100) then
      -- 破甲
      if S.guard_break then dse.audio.play_sfx(S.guard_break, 0.8, 0) end
      spawn_damage_text(Player.x, 2.5, Player.z, "破甲!", 1.0, 0.2, 0.2, 20)
    else
      -- 成功格挡
      if S.block then dse.audio.play_sfx(S.block, 0.8, 0) end
      spawn_damage_text(Player.x, 2.5, Player.z, "格挡!", 0.5, 0.8, 1.0, 20)
      Player.knockback_x = -dx * 80
      Player.knockback_z = -dz * 80
      Player.knockback_timer = 0.1
      return
    end
  end

  -- 受伤
  Player.chamovestat = -1
  local actual_damage = math.max(1, damage - Player.defence)
  Player.hp = Player.hp - actual_damage
  Player.combotime = 0
  G.combo = 0

  if S.hurt then dse.audio.play_sfx(S.hurt, 0.7, 0) end
  spawn_damage_text(Player.x, 2.5, Player.z, tostring(actual_damage), 1.0, 0.3, 0.3, 22)
  spawn_hit_effect(Player.x, 1.0, Player.z, 5)

  Player.knockback_x = dx * 180
  Player.knockback_z = dz * 180
  Player.knockback_timer = 0.15
  Player.visual_state = "behit"
  Player.visual_duration = 0.3

  CameraHitcam()

  if Player.hp <= 0 then
    Player.hp = 0
    Player.life = false
    Player.visual_state = "dead"
    Player.visual_duration = 999
    G.mode = "game_over"
    if S.bgm_fail then dse.audio.play_bgm(S.bgm_fail, 0.6, false) end
    -- 显示复活界面 (C# UI_Ingame_GUI.ChanceOn)
    UISystem.show_chance()
  else
    -- 耐力判定 (是否倒地)
    if math.random(0, 100) > Player.endurance then
      Player.visual_state = "behitdown"
      Player.visual_duration = 0.5
      Invincibility(2.0)
    else
      Invincibility(0.5)
    end
  end
end

-- 获取经验 (GainExp)
local function GainExp(amount)
  if Player.level >= 199 then Player.exp = 0; return end
  Player.exp = Player.exp + amount
  if Player.exp >= Player.level * 100 then
    Player.level = Player.level + 1
    Player.exp = 0
    Player.maxhp = Player.maxhp + 5
    Player.hp = Player.hp + 5
    spawn_damage_text(Player.x, 3.0, Player.z, "LEVEL UP!", 0.3, 1.0, 0.3, 28)
  end
end

-- 获取物品 (GetItem)
local function GetItem(kind, level)
  if S.getitem then dse.audio.play_sfx(S.getitem, 0.8, 0) end
  if kind == 0 then
    -- 金币
    local amount = math.floor(1.34 * level + 2)
    G.coin = G.coin + amount
    spawn_damage_text(Player.x, 2.5, Player.z, "+" .. amount .. " 金", 1.0, 0.85, 0.2, 18)
  elseif kind == 1 then
    -- SP 恢复
    Spcharge(10)
    spawn_damage_text(Player.x, 2.5, Player.z, "+SP", 0.3, 0.8, 1.0, 18)
  elseif kind == 2 then
    -- HP 恢复 (C# GetItem: (100 + (level-1)) * 0.1)
    local heal = math.floor((100 + (Player.level - 1)) * 0.1)
    Player.hp = math.min(Player.hp + heal, Player.maxhp)
    spawn_damage_text(Player.x, 2.5, Player.z, "+" .. heal .. " HP", 0.3, 1.0, 0.3, 18)
  elseif kind == 3 then
    -- 灵魂
    G.soul = G.soul + 1
    spawn_damage_text(Player.x, 2.5, Player.z, "+灵魂", 0.8, 0.3, 1.0, 18)
  elseif kind == 4 then
    -- 玉石
    G.jade = G.jade + 1
    spawn_damage_text(Player.x, 2.5, Player.z, "+玉石", 0.3, 1.0, 0.8, 18)
  end
end

-- ============================================================================
-- 技能系统 (Cha_Skill 完整移植)
-- 12 个玩家技能集 + 8 个武将技能集
-- motionkind: 1=前方释放 2=自身范围 3=蓄力 4=终极
-- ============================================================================

-- 获取当前技能信息
local function GetCurrentSkill()
  local slot = Player.current_skill_slot
  local set = Player.skill_slots[slot]
  if not set then return nil end
  local grade = Player.skill_grades[set] or 0
  local skill = DB.DB_Skill[set] and DB.DB_Skill[set][grade]
  if not skill then return nil end
  return skill, set, grade
end

-- 技能发射 (LaunchSkill) — 每个技能集的独立效果
local function LaunchSkill(index, skillatk, basedamage)
  local dmg = skillatk * basedamage * 0.01 + Player.atk
  local dx, dz = math.sin(math.rad(Player.yaw)), -math.cos(math.rad(Player.yaw))

  if index == 0 then
    -- 剑舞 (sword wind) — 前方扇形伤害
    spawn_swing_ef(Player.x, 0.06, Player.z, Player.yaw, 3.0, 0.4)
    for _, en in ipairs(Entities.enemies) do
      if not en.dead then
        local d = dist2d(Player.x, Player.z, en.x, en.z)
        if d <= 6.0 then
          local adx, adz = en.x - Player.x, en.z - Player.z
          local angle = math.deg(atan2(adx, -adz))
          local diff = ((angle - Player.yaw + 180) % 360) - 180
          if math.abs(diff) < 80 then
            EnemyDamaged(en, dmg, Player.x, Player.z, "normal")
          end
        end
      end
    end
    if Entities.boss and not Entities.boss.dead then
      local d = dist2d(Player.x, Player.z, Entities.boss.x, Entities.boss.z)
      if d <= 7.0 then BossDamaged(Entities.boss, dmg, Player.x, Player.z, "normal") end
    end

  elseif index == 1 then
    -- 旋风斩 (wheel wind) — 跳劈 + 范围伤害
    Player.y = 2.0  -- 跳起
    spawn_swing_ef(Player.x, 0.1, Player.z, Player.yaw, 4.0, 0.5, "skill")
    for _, en in ipairs(Entities.enemies) do
      if not en.dead then
        local d = dist2d(Player.x, Player.z, en.x, en.z)
        if d <= 8.0 then
          EnemyDamaged(en, dmg * 1.2, Player.x, Player.z, "strong")
        end
      end
    end
    if Entities.boss and not Entities.boss.dead then
      local d = dist2d(Player.x, Player.z, Entities.boss.x, Entities.boss.z)
      if d <= 9.0 then BossDamaged(Entities.boss, dmg * 1.2, Player.x, Player.z, "strong") end
    end
    CameraHitcam2(2.0)

  elseif index == 2 then
    -- 火焰斩 (fire slash) — 前方火焰柱
    for i = 0, 5 do
      local fx = Player.x + dx * i * 0.8
      local fz = Player.z + dz * i * 0.8
      spawn_hit_effect(fx, 0.5, fz, 8, "fire")
      spawn_swing_ef(fx, 0.06, fz, Player.yaw, 1.5, 0.3, "fire")
    end
    for _, en in ipairs(Entities.enemies) do
      if not en.dead then
        local d = dist2d(Player.x, Player.z, en.x, en.z)
        if d <= 5.0 then
          local adx, adz = en.x - Player.x, en.z - Player.z
          local dot = adx * dx + adz * dz
          if dot > 0 then
            EnemyDamaged(en, dmg, Player.x, Player.z, "fire")
          end
        end
      end
    end
    if Entities.boss and not Entities.boss.dead then
      local d = dist2d(Player.x, Player.z, Entities.boss.x, Entities.boss.z)
      if d <= 6.0 then BossDamaged(Entities.boss, dmg, Player.x, Player.z, "fire") end
    end

  elseif index == 3 then
    -- 召唤猎鹰 (eagle summon) — 宠物 (C# Cha_Skill.PetSkillOn → Cha_Control.Fly)
    PetSystem.pet_skill_on(1)

  elseif index == 4 then
    -- 冰冻斩 (ice slash) — 前方冰冻
    for i = 0, 4 do
      local fx = Player.x + dx * i * 0.7
      local fz = Player.z + dz * i * 0.7
      spawn_hit_effect(fx, 0.3, fz, 6, "ice")
    end
    spawn_swing_ef(Player.x, 0.06, Player.z, Player.yaw, 2.5, 0.4, "ice")
    for _, en in ipairs(Entities.enemies) do
      if not en.dead then
        local d = dist2d(Player.x, Player.z, en.x, en.z)
        if d <= 5.0 then
          local adx, adz = en.x - Player.x, en.z - Player.z
          local dot = adx * dx + adz * dz
          if dot > 0 then
            EnemyDamaged(en, dmg, Player.x, Player.z, "ice")
          end
        end
      end
    end
    if Entities.boss and not Entities.boss.dead then
      local d = dist2d(Player.x, Player.z, Entities.boss.x, Entities.boss.z)
      if d <= 6.0 then BossDamaged(Entities.boss, dmg, Player.x, Player.z, "ice") end
    end

  elseif index == 5 then
    -- 雷电斩 (lightning slash) — 链式闪电
    spawn_swing_ef(Player.x, 0.06, Player.z, Player.yaw, 3.0, 0.3, "electric")
    local hit_count = 0
    local last_x, last_z = Player.x, Player.z
    for _, en in ipairs(Entities.enemies) do
      if not en.dead and hit_count < 5 then
        local d = dist2d(last_x, last_z, en.x, en.z)
        if d <= 6.0 then
          EnemyDamaged(en, dmg, last_x, last_z, "electric")
          spawn_hit_effect(en.x, 1.0, en.z, 5, "electric")
          last_x, last_z = en.x, en.z
          hit_count = hit_count + 1
        end
      end
    end
    if Entities.boss and not Entities.boss.dead then
      local d = dist2d(Player.x, Player.z, Entities.boss.x, Entities.boss.z)
      if d <= 7.0 then BossDamaged(Entities.boss, dmg, Player.x, Player.z, "electric") end
    end

  elseif index == 6 then
    -- 毒击 (poison strike) — 毒雾范围
    for _ = 1, 15 do
      local angle = math.random() * math.pi * 2
      local r = math.random() * 4.0
      spawn_hit_effect(Player.x + math.cos(angle) * r, 0.3, Player.z + math.sin(angle) * r, 1, "poison")
    end
    for _, en in ipairs(Entities.enemies) do
      if not en.dead then
        local d = dist2d(Player.x, Player.z, en.x, en.z)
        if d <= 4.0 then
          EnemyDamaged(en, dmg * 0.5, Player.x, Player.z, "poison")
          -- 额外持续伤害
          en.poison = true
          en.poison_delay = 5.0
          en.old_delay = math.floor(en.poison_delay)
          en.poison_damage = math.floor(Player.atk * 0.6)
        end
      end
    end
    if Entities.boss and not Entities.boss.dead then
      local d = dist2d(Player.x, Player.z, Entities.boss.x, Entities.boss.z)
      if d <= 5.0 then
        BossDamaged(Entities.boss, dmg * 0.5, Player.x, Player.z, "poison")
        Entities.boss.poison = true
        Entities.boss.poison_delay = 5.0
        Entities.boss.old_delay = math.floor(Entities.boss.poison_delay)
        Entities.boss.poison_damage = math.floor(Player.atk * 0.6)
      end
    end

  elseif index == 7 then
    -- 召唤战马 (horse summon) — 骑乘 (C# Cha_Skill.PetSkillOn → Cha_Control.CallHorse)
    PetSystem.pet_skill_on(0)

  elseif index == 8 then
    -- 蓄力斩 (charge smash) — 超级剑气
    Invincibility(2.0)
    Player.visual_state = "charge"
    Player.visual_duration = 1.0
    -- 前方大范围剑气
    for i = 0, 10 do
      local fx = Player.x + dx * i * 0.5
      local fz = Player.z + dz * i * 0.5
      spawn_swing_ef(fx, 0.1, fz, Player.yaw, 2.0, 0.3 + i * 0.05, "skill")
    end
    for _, en in ipairs(Entities.enemies) do
      if not en.dead then
        local d = dist2d(Player.x, Player.z, en.x, en.z)
        if d <= 8.0 then
          local adx, adz = en.x - Player.x, en.z - Player.z
          local dot = adx * dx + adz * dz
          if dot > -0.3 then
            EnemyDamaged(en, dmg * 2.0, Player.x, Player.z, "rising")
          end
        end
      end
    end
    if Entities.boss and not Entities.boss.dead then
      local d = dist2d(Player.x, Player.z, Entities.boss.x, Entities.boss.z)
      if d <= 9.0 then BossDamaged(Entities.boss, dmg * 2.0, Player.x, Player.z, "skill") end
    end
    CameraHitcam2(3.0)

  elseif index == 9 then
    -- 武将召唤 (general summon) — 巨手从天而降
    Invincibility(3.0)
    for _ = 1, 30 do
      local angle = math.random() * math.pi * 2
      local r = math.random() * 10.0
      spawn_hit_effect(Player.x + math.cos(angle) * r, 2.0 + math.random() * 3, Player.z + math.sin(angle) * r, 1, "skill")
    end
    for _, en in ipairs(Entities.enemies) do
      if not en.dead then
        local d = dist2d(Player.x, Player.z, en.x, en.z)
        if d <= 10.0 then
          EnemyDamaged(en, dmg * 1.5, Player.x, Player.z, "strong")
          en.knockback_x = (en.x - Player.x) * 100
          en.knockback_z = (en.z - Player.z) * 100
          en.knockback_timer = 0.3
        end
      end
    end
    if Entities.boss and not Entities.boss.dead then
      local d = dist2d(Player.x, Player.z, Entities.boss.x, Entities.boss.z)
      if d <= 11.0 then BossDamaged(Entities.boss, dmg * 1.5, Player.x, Player.z, "strong") end
    end
    CameraHitcam2(4.0)

  elseif index == 10 then
    -- 极限斩 (extreme slash) — 超级大范围斩击
    Invincibility(2.0)
    for _ = 1, 40 do
      local angle = math.random() * math.pi * 2
      local r = math.random() * 12.0
      spawn_hit_effect(Player.x + math.cos(angle) * r, 1.0, Player.z + math.sin(angle) * r, 1, "skill")
    end
    spawn_swing_ef(Player.x, 0.1, Player.z, Player.yaw, 6.0, 0.8, "skill")
    for _, en in ipairs(Entities.enemies) do
      if not en.dead then
        local d = dist2d(Player.x, Player.z, en.x, en.z)
        if d <= 12.0 then
          EnemyDamaged(en, dmg * 2.5, Player.x, Player.z, "strong")
          en.knockback_x = (en.x - Player.x) * 150
          en.knockback_z = (en.z - Player.z) * 150
          en.knockback_timer = 0.4
        end
      end
    end
    if Entities.boss and not Entities.boss.dead then
      local d = dist2d(Player.x, Player.z, Entities.boss.x, Entities.boss.z)
      if d <= 13.0 then BossDamaged(Entities.boss, dmg * 2.5, Player.x, Player.z, "strong") end
    end
    CameraHitcam2(5.0)

  elseif index == 11 then
    -- 时间减速 (time slow) — 全场减速 + 范围伤害
    Invincibility(5.0)
    G.time_scale = 0.2
    G.time_scale_timer = 5.0
    for _ = 1, 25 do
      local angle = math.random() * math.pi * 2
      local r = math.random() * 8.0
      spawn_hit_effect(Player.x + math.cos(angle) * r, 1.0, Player.z + math.sin(angle) * r, 1, "skill")
    end
    for _, en in ipairs(Entities.enemies) do
      if not en.dead then
        local d = dist2d(Player.x, Player.z, en.x, en.z)
        if d <= 8.0 then
          EnemyDamaged(en, dmg, Player.x, Player.z, "skill")
        end
      end
    end
    if Entities.boss and not Entities.boss.dead then
      local d = dist2d(Player.x, Player.z, Entities.boss.x, Entities.boss.z)
      if d <= 9.0 then BossDamaged(Entities.boss, dmg, Player.x, Player.z, "skill") end
    end
    spawn_damage_text(Player.x, 3.5, Player.z, "时间减速!", 0.5, 0.5, 1.0, 28)
  end

  -- 通用：经验/连击
  G.combo = G.combo + 2
  G.combo_timer = 2.0
end

-- 施法开始 (SkillOn)
local function SkillOn(index, grade, is_general)
  local skill
  if is_general then
    -- 武将技能
    skill = DB.DB_Skill[index] and DB.DB_Skill[index][grade or 0]
    if not skill then return end
    Player.skillatk = skill.attackpoint
    Player.motionkind = skill.kind
    Player.skill_index = index + 21  -- 武将技能索引偏移
  else
    skill = DB.DB_Skill[index] and DB.DB_Skill[index][grade or 0]
    if not skill then return end
    Player.skillatk = skill.attackpoint
    Player.motionkind = skill.kind
    Player.skill_index = index
  end

  Player.casting = true
  Player.casting_delay = 0.3  -- 施法延迟
  Player.chamovestat = 180
  Player.visual_state = "cast" .. Player.motionkind
  Player.visual_duration = 0.6

  -- 施法特效
  spawn_hit_effect(Player.x, 0.5, Player.z, 10, "skill")
  if S.skill then dse.audio.play_sfx(S.skill, 0.9, 0) end
  if S.timewoosh then dse.audio.play_sfx(S.timewoosh, 0.7, 0) end

  -- 时间减速 (施法时)
  G.time_scale = 0.3
  G.time_scale_timer = 0.5

  -- 方向箭头 (程序化)
  spawn_damage_text(Player.x, 2.0, Player.z, "<<", 1.0, 1.0, 0.4, 16)
end

-- 技能施放
local function PlayerSkill()
  if Player.casting then return end
  if Player.chamovestat < -1 or Player.chamovestat > 50 then return end

  local skill, set, grade = GetCurrentSkill()
  if not skill then return end

  -- 冷却检查
  if Player.skill_cd[set] and Player.skill_cd[set] > 0 then return end

  -- SP 消耗
  local sp_cost = 20 + grade * 10
  if skill.soulprice and skill.soulprice > 0 then
    -- 灵魂消耗
    if G.soul < skill.soulprice then return end
    G.soul = G.soul - skill.soulprice
    sp_cost = 0
  end
  if Player.sp < sp_cost then return end
  Spcharge(-sp_cost)

  -- 设置冷却
  Player.skill_cd[set] = skill.cooltime

  -- 开始施法
  SkillOn(set, grade, false)
end

-- 召唤武将
local function CallGeneral()
  if Player.general then return end
  if Player.sp < 100 then return end
  if not DB.DB_General[0] then return end

  Spcharge(-100)
  local g = DB.DB_General[0]
  Player.general = true
  Player.general_kind = g.kind
  Player.general_maxhp = g.maxhp
  Player.general_hp = g.maxhp
  Player.general_atk = g.atk
  Player.general_def = g.def
  Player.general_atkspd = g.atkspd

  -- 切换到武将属性
  local old_maxhp = Player.maxhp
  Player.maxhp = g.maxhp
  Player.hp = g.maxhp
  Player.maxatk = g.atk
  Player.minatk = math.floor(g.atk * 0.8)
  Player.defence = g.def
  Player.atkspd = g.atkspd

  Player.chamovestat = 100
  Player.change_cha = true
  Player.visual_state = "change_out"
  Player.visual_duration = 0.5

  spawn_damage_text(Player.x, 3.0, Player.z, "召唤武将: " .. g.name, 1.0, 0.8, 0.2, 24)
  if S.skill then dse.audio.play_sfx(S.skill, 1.0, 0) end
end

-- 武将结束
local function GeneralOff()
  if not Player.general then return end
  Player.general = false
  Player.maxhp = 95 + Player.level * 5
  Player.hp = math.min(Player.hp, Player.maxhp)
  ResetAtk()
  Player.chamovestat = 100
  Player.visual_state = "change_in"
  Player.visual_duration = 0.5
end

-- ============================================================================
-- 玩家更新 (Update)
-- ============================================================================
local function UpdatePlayer(dt)
  if not Player.life and G.mode == "game_over" then return end
  if not Player.isplaycha and G.mode == "play" then return end

  -- 计时器递减
  if Player.attacking > 0 then Player.attacking = Player.attacking - dt end
  if Player.invuln > 0 then Player.invuln = Player.invuln - dt end
  if Player.dodge_timer > 0 then Player.dodge_timer = Player.dodge_timer - dt end
  if Player.block_timer > 0 then Player.block_timer = Player.block_timer - dt end
  if Player.grab_timer > 0 then Player.grab_timer = Player.grab_timer - dt end
  if Player.skill_timer > 0 then Player.skill_timer = Player.skill_timer - dt end
  if Player.combotime > 0 then Player.combotime = Player.combotime - dt end
  if Player.hit_flash > 0 then Player.hit_flash = Player.hit_flash - dt end
  if Player.visual_timer > 0 then Player.visual_timer = Player.visual_timer - dt end
  if Player.knockback_timer > 0 then Player.knockback_timer = Player.knockback_timer - dt end
  if Player.control_lock > 0 then Player.control_lock = Player.control_lock - dt end

  -- 技能冷却
  for k, v in pairs(Player.skill_cd) do
    if v > 0 then Player.skill_cd[k] = v - dt end
  end

  -- 无敌计时
  if Player.isinvincibility then
    Player.delay_invincibility = Player.delay_invincibility + dt
    if Player.delay_invincibility >= Player.target_invincibility then
      Player.isinvincibility = false
      Player.target_invincibility = 0
    end
  end

  -- SP 自然恢复
  G.sp_recover_timer = G.sp_recover_timer + dt
  if G.sp_recover_timer >= 3.0 then
    G.sp_recover_timer = 0
    Spcharge(3.0)
  end

  -- 连击计时
  if G.combo_timer > 0 then
    G.combo_timer = G.combo_timer - dt
    if G.combo_timer <= 0 then
      if G.combo > G.combo_max then G.combo_max = G.combo end
      G.combo = 0
    end
  end

  -- 施法系统由 SkillSystem.update 处理, 不再在此重复

  -- 跳起后落地
  if Player.y > 0 then
    Player.y = Player.y - dt * 8.0
    if Player.y < 0 then Player.y = 0 end
  end

  -- 宠物系统更新由主 Update 循环统一调用 PetSystem.update(dt)

  -- 增益计时
  if Player.attack_up > 0 then
    Player.attack_up = Player.attack_up - dt
    if Player.attack_up <= 0 then
      Player.attack_up_factor = 1.0
      ResetPower()
    end
  end
  if Player.defence_up > 0 then
    Player.defence_up = Player.defence_up - dt
    if Player.defence_up <= 0 then
      Player.defence_up_factor = 1.0
    end
  end

  -- 箭矢发射延迟
  if Player._arrow_delay and Player._arrow_delay > 0 then
    Player._arrow_delay = Player._arrow_delay - dt
    if Player._arrow_delay <= 0 and Player._arrow_ready then
      Player._arrow_ready = false
      -- 发射箭矢 (Bullet_arrow)
      local dx, dz = math.sin(math.rad(Player.yaw)), -math.cos(math.rad(Player.yaw))
      BulletSystem.spawn({
        type = "arrow", x = Player.x, y = 0.24, z = Player.z, yaw = Player.yaw,
        speed = 15, damage = Player.atk, attack_type = "arrow",
        life = 2.0, model = "assets/models/ball.dmesh",
      })
      if S.slash then dse.audio.play_sfx(S.slash, 0.6, 0) end
    end
  end

  -- 法术弹发射
  if Player._bullet_delay and Player._bullet_delay > 0 then
    Player._bullet_delay = Player._bullet_delay - dt
    if Player._bullet_delay <= 0 and Player._bullet_ready then
      Player._bullet_ready = false
      local dx, dz = math.sin(math.rad(Player.yaw)), -math.cos(math.rad(Player.yaw))
      BulletSystem.spawn({
        type = "arrow", x = Player.x, y = 0.5, z = Player.z, yaw = Player.yaw,
        speed = 10, damage = Player.atk, attack_type = "magic",
        life = 1.5, r = 0.3, g = 0.5, b = 1.0,
      })
    end
  end

  -- 魔法弹发射
  if Player._magic_delay and Player._magic_delay > 0 then
    Player._magic_delay = Player._magic_delay - dt
    if Player._magic_delay <= 0 and Player._magic_ready then
      Player._magic_ready = false
      local dx, dz = math.sin(math.rad(Player.yaw)), -math.cos(math.rad(Player.yaw))
      BulletSystem.spawn({
        type = "magicmissile", x = Player.x, y = 0.3, z = Player.z, yaw = Player.yaw,
        damage = Player.atk, attack_type = "magic_missile",
        life = 4.0, r = 0.6, g = 0.5, b = 1.0,
        homing_rate = 5, homing_spd = 5, close_time = 3,
        collide_player = false, collide_enemies = true,
      })
    end
  end

  -- 火焰飞溅
  if Player._firesplash then
    Player._firesplash = false
    for _ = 1, 10 do
      local angle = math.random() * math.pi * 2
      local r = math.random() * 3.0
      spawn_hit_effect(Player.x + math.cos(angle) * r, 0.5, Player.z + math.sin(angle) * r, 1, "fire")
    end
    -- 范围伤害
    for _, en in ipairs(Entities.enemies) do
      if not en.dead then
        local d = dist2d(Player.x, Player.z, en.x, en.z)
        if d <= 3.0 then
          en.hp = en.hp - Player.atk
          en.hit_flash = 0.3
          spawn_damage_text(en.x, 2.0, en.z, tostring(Player.atk), 1.0, 0.4, 0.1, 20)
          if en.hp <= 0 then
            en.dead = true
            en.death_timer = 0.8
            G.enemykill = G.enemykill + 1
            G.totalkill = G.totalkill + 1
            GainExp(en.haveExp or 1)
          end
        end
      end
    end
  end

  -- 击退处理
  if Player.knockback_timer > 0 then
    Player.x = Player.x + Player.knockback_x * dt * 0.5
    Player.z = Player.z + Player.knockback_z * dt * 0.5
  end

  -- 闪避位移
  if Player.dodge_timer > 0 then
    Player.x = Player.x + Player.dodge_dx * dt
    Player.z = Player.z + Player.dodge_dz * dt
  end

  -- 攻击中不可移动 (control_lock: Cutin01 特写期间锁定输入)
  local can_move = (Player.attacking <= 0 and Player.chamovestat >= -1 and Player.chamovestat <= 10
                    and Player.grab_timer <= 0 and Player.skill_timer <= 0
                    and Player.knockback_timer <= 0 and Player.dodge_timer <= 0
                    and Player.control_lock <= 0
                    and not Player.casting)

  -- 输入：移动
  if can_move and G.mode == "play" then
    local dx, dz = 0, 0
    if app.get_key(KEY_A) or app.get_key(KEY_LEFT)  then dx = dx - 1 end
    if app.get_key(KEY_D) or app.get_key(KEY_RIGHT) then dx = dx + 1 end
    if app.get_key(KEY_W) or app.get_key(KEY_UP)    then dz = dz - 1 end
    if app.get_key(KEY_S) or app.get_key(KEY_DOWN)  then dz = dz + 1 end

    if dx ~= 0 or dz ~= 0 then
      if dx ~= 0 and dz ~= 0 then dx = dx * 0.707; dz = dz * 0.707 end
      local speed = Player.movespeed * Player.speedfactor
      Player.x = Player.x + dx * speed * dt
      Player.z = Player.z + dz * speed * dt
      Player.dir_x, Player.dir_z = dx, dz
      Player.yaw = math.deg(atan2(dx, -dz))
      Player.chamovestat = 2 -- run
      Player.visual_state = "run"

      -- C# 冲刺态: 连续跑 3s → chamovestat=3 + 残影 (Cha_Control ef_blur)
      Player.longdash = (Player.longdash or 0) + dt
      if Player.longdash >= 3.0 then
        Player.chamovestat = 3
        Player.visual_state = "sprint"
        Player._blur_timer = (Player._blur_timer or 0) + dt
        if Player._blur_timer >= 0.15 then
          Player._blur_timer = 0
          if EfSystem.spawn_blur then
            EfSystem.spawn_blur(Player.x, Player.y + 1.2, Player.z, {})
          end
        end
      end

      -- 脚步声
      if math.random() < dt * 3 then
        if S.footstep then dse.audio.play_sfx(S.footstep, 0.3, 0) end
      end
    else
      Player.chamovestat = 0 -- idle
      Player.longdash = 0
      Player._blur_timer = 0
      if Player.attacking <= 0 and Player.dodge_timer <= 0 and Player.knockback_timer <= 0 then
        Player.visual_state = "idle"
      end
    end

    -- 攻击输入
    if app.get_key_down(KEY_J) then
      Player.ex_press_time = G.time
      PlayerAttackInput()
    end

    -- 蓄力攻击 (C# Exstart: 长按 J 0.3s → PowerCharge; 松开 ResetPower)
    if not Player.excharging and app.get_key(KEY_J)
       and Player.ex_press_time and G.time - Player.ex_press_time > 0.3
       and Player.attacking <= 0 and Player.dodge_timer <= 0
       and Player.grab_timer <= 0 and Player.skill_timer <= 0
       and not Player.casting then
      Player.excharging = true
      UISystem.power_charge()
      if S.skill then dse.audio.play_sfx(S.skill, 0.6, 0) end
    end
    if Player.excharging and app.get_key_up(KEY_J) then
      Player.excharging = false
      UISystem.reset_power()
    end

    -- 追击 QTE (C# attackex1: Eximpact 后 0.5-0.7s 窗口内按键追击)
    if Player.qte_timer > 0 then
      Player.qte_timer = Player.qte_timer + dt
      if Player.qte_timer >= 0.5 and Player.qte_timer <= 0.7 then
        Player.qte_active = true
      end
      if Player.qte_timer > 0.7 then
        Player.qte_timer = 0
        Player.qte_active = false
      end
      if Player.qte_active and app.get_key_down(KEY_J) then
        Player.qte_timer = 0
        Player.qte_active = false
        PlayerAttackEx1()
      end
    end

    -- 技能输入 (委托给 SkillSystem)
    if app.get_key_down(KEY_K) then
      SkillSystem.player_skill()
    end

    -- 闪避 (C# 双击闪避: dubbleclick < 0.3s 内第二次按 L)
    if app.get_key_down(KEY_L) then
      local now = G.time
      if Player._dodge_click_t and now - Player._dodge_click_t < 0.3 then
        PlayerDodge()
        Player._dodge_click_t = nil
      else
        Player._dodge_click_t = now
      end
    end

    -- 格挡 (按住)
    if app.get_key(KEY_O) then
      PlayerBlock(dt)
    end

    -- 抓取
    if app.get_key_down(KEY_P) then
      PlayerGrab()
    end

    -- 切换武器
    if app.get_key_down(KEY_U) then
      local next_weapon = (Player.weapon_kind + 1) % 6
      ChangeCharacter(next_weapon)
      -- 随机赋予武器特殊属性
      Player.special_kind = math.random(-2, 6)
      if Player.special_kind >= 0 then
        local sp_name = DB.DB_WeaponSpecial[Player.special_kind] and DB.DB_WeaponSpecial[Player.special_kind].name or ""
        spawn_damage_text(Player.x, 3.0, Player.z, "武器: " .. DB.DB_Weapon[next_weapon].name .. " [" .. sp_name .. "]", 0.8, 0.8, 1.0, 20)
      else
        spawn_damage_text(Player.x, 3.0, Player.z, "武器: " .. DB.DB_Weapon[next_weapon].name, 0.8, 0.8, 1.0, 20)
      end
    end

    -- 召唤武将
    if app.get_key_down(KEY_I) then
      if Player.general then
        GeneralOff()
      else
        CallGeneral()
      end
    end

    -- Q 键切换技能槽
    if app.get_key_down(KEY_Q) then
      Player.current_skill_slot = (Player.current_skill_slot % 6) + 1
      local skill, set, grade = GetCurrentSkill()
      if skill then
        local sname = DB.SkillNames[skill.name] or ("技能" .. tostring(set))
        spawn_damage_text(Player.x, 3.0, Player.z, "技能: " .. sname .. " Lv" .. (grade + 1), 0.6, 0.8, 1.0, 20)
      end
    end

    -- 数字键切换武器
    for i = 0, 5 do
      if app.get_key_down(KEY_1 + i) then
        ChangeCharacter(i)
        spawn_damage_text(Player.x, 3.0, Player.z, DB.DB_Weapon[i].name, 0.8, 0.8, 1.0, 20)
      end
    end
  end

  -- 攻击结束后恢复
  if Player.attacking <= 0 and Player.chamovestat > 10 and Player.chamovestat < 50 then
    Player.chamovestat = 0
    Player.visual_state = "idle"
    Player.curruntattack = Player.attackkind_factor
  end

  -- 抓取结束
  if Player.grab_timer <= 0 and Player.chamovestat == 112 then
    Player.chamovestat = 0
    Player.visual_state = "idle"
  end

  -- 技能/施法结束
  if (Player.skill_timer <= 0 or not Player.casting) and Player.chamovestat == 180 and not Player.casting then
    Player.chamovestat = 0
    Player.visual_state = "idle"
  end

  -- 换装结束
  if Player.visual_timer <= 0 and (Player.chamovestat == 100) then
    Player.chamovestat = 0
    Player.visual_state = "idle"
  end

  -- 边界限制
  Player.x = clamp(Player.x, -30, 30)
  Player.z = clamp(Player.z, -30, 30)

  -- 更新玩家实体
  if G.player_e then
    dse.ecs.set_transform_position(G.player_e, Player.x, Player.y, Player.z)
    dse.ecs.set_transform_rotation(G.player_e, 0, Player.yaw, 0)

    -- 程序化动画：根据状态调整缩放和颜色
    local sx, sy, sz = 1.0, 1.0, 1.0
    local r, g, b, a = 1.0, 1.0, 1.0, 1.0

    if Player.visual_state == "attack1" or Player.visual_state == "attack2" then
      -- 攻击时前倾
      sy = 0.9
      sx = 1.1
    elseif Player.visual_state == "attack3" then
      -- 重击时放大
      sx, sy, sz = 1.2, 1.2, 1.2
    elseif Player.visual_state == "attack4" then
      -- 突刺时拉长
      sz = 1.3
    elseif Player.visual_state == "attack5" then
      -- 终结技时更大
      sx, sy, sz = 1.3, 1.3, 1.3
    elseif Player.visual_state == "dash_attack" then
      sx, sz = 1.4, 1.4
    elseif Player.visual_state == "sprint" then
      -- 冲刺姿态: 前冲
      sx, sz = 1.25, 1.15
    elseif Player.visual_state == "dodge" or Player.visual_state == "evade" then
      -- 闪避时压扁
      sy = 0.5
      sx = 1.3
    elseif Player.visual_state == "block" then
      -- 格挡时缩小
      sx, sz = 0.9, 0.9
    elseif Player.visual_state == "grab" then
      sx, sy, sz = 1.1, 1.1, 1.1
    elseif Player.visual_state == "behit" then
      -- 受击时后退倾斜
      sy = 0.8
    elseif Player.visual_state == "behitdown" then
      -- 倒地时压扁
      sy = 0.3
      sx = 1.5
    elseif Player.visual_state == "dead" then
      sy = 0.2
      sx = 1.5
    elseif Player.visual_state == "skill" or Player.visual_state == "cast1" then
      -- 技能时发光放大
      sx, sy, sz = 1.3, 1.3, 1.3
      r, g, b = 0.8, 0.6, 1.0
    elseif Player.visual_state == "cast2" then
      -- 自身范围施法
      sx, sy, sz = 1.4, 0.9, 1.4
      r, g, b = 0.6, 0.8, 1.0
    elseif Player.visual_state == "cast3" then
      -- 蓄力施法
      local pulse = 1.0 + 0.2 * math.sin(G.time * 20)
      sx, sy, sz = pulse, pulse, pulse
      r, g, b = 1.0, 0.8, 0.2
    elseif Player.visual_state == "cast4" then
      -- 终极施法
      local pulse = 1.0 + 0.3 * math.sin(G.time * 25)
      sx, sy, sz = pulse * 1.5, pulse * 1.5, pulse * 1.5
      r, g, b = 1.0, 0.4, 0.2
    elseif Player.visual_state == "charge" then
      -- 蓄力斩
      local pulse = 1.0 + 0.15 * math.sin(G.time * 30)
      sx, sz = 1.5 * pulse, 1.5 * pulse
      r, g, b = 0.6, 0.4, 1.0
    elseif Player.visual_state == "change_out" or Player.visual_state == "change_in" then
      sx, sy, sz = 1.2, 1.2, 1.2
      r, g, b = 1.0, 0.8, 0.2
    end

    -- 攻击中的脉动效果
    if Player.attacking > 0 then
      local pulse = 1.0 + 0.1 * math.sin(Player.attacking * 30)
      sx = sx * pulse
    end

    -- 无敌闪烁
    if Player.isinvincibility then
      if math.floor(G.time * 15) % 2 == 0 then
        a = 0.5
        r, g, b = 0.5, 0.5, 1.0
      end
    end

    -- 受击闪烁
    if Player.hit_flash > 0 then
      r, g, b = 1.0, 0.3, 0.3
    end

    -- 超级模式
    if Player.superMode > 0 then
      r, g, b = 1.0, 0.8, 0.2
      local pulse = 1.0 + 0.15 * math.sin(G.time * 20)
      sx, sy, sz = pulse, pulse, pulse
    end

    dse.ecs.set_transform_scale(G.player_e, sx, sy, sz)
    dse.ecs.set_mesh_color(G.player_e, r, g, b, a)

    -- 武将时放大
    if Player.general then
      local gdata = DB.DB_General[Player.general_kind]
      if gdata and gdata.scale then
        -- 已经在 ChangeCharacter 时处理
      end
    end
  end

  -- 宝物拾取 (C# Itemdrop 碰撞; 用掉落物池回收实体)
  for _, t in ipairs(Entities.treasures) do
    if not t.collected and t.dropped then
      if dist2d(Player.x, Player.z, t.x, t.z) < 1.5 then
        t.collected = true
        GetItem(t.kind or 0, t.level or 1)
        if t.e then FreeItemBoxEntity(t.e) end
      end
    end
  end
end

-- ============================================================================
-- 敌人 AI (AI_Enemy01 完整移植)
-- 100% 忠实还原 C# AI_Enemy01.cs 的所有逻辑
-- 16 层伤害系统 / 状态机 / 抓取 / 升龙 / 属性异常 / 波次等级缩放
-- ============================================================================

-- attack_type 字符串到 C# Physics Layer 映射
local function attack_type_to_layer(at)
  local m = {
    normal=20, strong=21, arrow=22, poison=23, petrify=24, grab=25,
    pierce=26, external=27, skill=28, rising=29, fire=30, ice=31,
    paralyze=16, weak_rise=17, electric=18, darken=19,
  }
  return m[at] or 20
end

-- 创建敌人 (Awake + Start 合并)
local function CreateEnemy(enemykind, x, z)
  local data = DB.DB_Monster[enemykind]
  if not data then return nil end
  local model = MON_MODELS[enemykind + 1] or MON_MODELS[1]
  local mon_tex = TEX.mon and TEX.mon[enemykind + 1]
  local e = spawn_model(model_path(model), x, 0, z, 1, 1, 1, mon_tex)
  local scale = 1.0
  if data.sizekind == 20 then scale = 1.2
  elseif data.sizekind == 24 then scale = 1.5 end
  dse.ecs.set_transform_scale(e, scale, scale, scale)
  local enemy = {
    e = e, enemykind = enemykind,
    x = x, z = z, y = 0, yaw = 0,
    hp = data.maxhp, maxhp = data.maxhp,
    power = data.power, haveExp = data.haveExp, block = data.block,
    firerange = data.firerange, runspeed = data.runspeed,
    backspeed = data.backspeed, dash = data.dash,
    moving_atk = data.moving_atk, attach_ef = data.attach_ef,
    speed_move = data.speed_move,
    speed_m_attack1 = data.speed_m_attack1,
    speed_m_attack1_i = data.speed_m_attack1_i,
    speed_idle = data.speed_idle,
    sizekind = data.sizekind, kind = data.kind, scale = scale,
    level = 1, playkind = 0, restrict_area = 625,
    monmovestat = 0, life = true,
    movespeed = 0, direction_x = 0, direction_z = -1, look_yaw = 0,
    behaviour = 0, behaviour_delay = 2.0,
    attackstart = false, attack_impact = false,
    attackstart_x = 0, attackstart_z = 0, attack_cd = 0,
    hit_flash = 0, downhigh = false, magnitude_behitdir = 0,
    attackdir_x = 0, attackdir_z = 0, attackforce = 0, damage = 0, accuracy = 90,
    att_status = 0, burn_timer = 0, burn_damage_timer = 0,
    freeze_timer = 0, shock_timer = 0, shock_damage_timer = 0,
    darken_timer = 0, petrify_timer = 0, petrify_rate = 0,
    pierce = false, pierce_timer = 0,
    poison = false, poison_delay = 0, poison_damage = 0, old_delay = 0,
    risedrop = false, f_risefactor = 0,
    grabed = false, grabstyle = 0, grab_phase = 0, grab_timer = 0,
    spawn_ing = false, spawn_timer = 0,
    target_is_player = true, target_fix = false, target_reset = false,
    lastmon = false, showme = false, arrow_e = nil,
    knockback_x = 0, knockback_z = 0, knockback_timer = 0,
    dead = false, death_timer = 0, death_kind = 0,
    base_x = x, base_z = z, t = math.random() * 10,
    setdir_timer = 0.1 + math.random() * 0.3,
    visual_state = "idle",
    -- 血条/影子 (C# script_monEf.CreatHpbar + CreatShadow)
    hp_bar_e = CreateHpBarEntity(2.4),
    shadow = GetShadow(),
  }
  -- 初始化影子位置
  if enemy.shadow then
    dse.ecs.set_transform_position(enemy.shadow.e, x, 0.03, z)
    dse.ecs.set_transform_scale(enemy.shadow.e, scale * 1.4, 1, scale * 1.4)
  end
  return enemy
end

-- 敌人设置等级 (SetLevel)
local function EnemySetLevel(en, level, playkind, issummon, restrictArea)
  en.level = level + 1
  en.restrict_area = restrictArea or 625
  en.maxhp = en.maxhp + math.floor(0.1445 * en.level * en.level + 6.3873 * en.level - 5)
  en.power = en.power + math.floor(0.0058 * en.level * en.level + 1.008 * en.level - 5)
  en.block = en.block + math.floor(en.level * 0.4)
  en.hp = en.maxhp
  en.haveExp = en.haveExp + en.level * 0.1
  en.playkind = playkind or 0
  -- 特殊关 (运粮车/守城): 敌人攻击目标而非玩家 (C# target_fix)
  if en.playkind == 6 or en.playkind == 7 then
    en.target_fix = true
    en.target_is_player = false
  end
  if issummon then
    en.spawn_ing = true
    en.spawn_timer = (en.kind == 2) and 1.5 or 0.4
    en.yaw = math.random(0, 360)
  end
end

-- 敌人死亡 (Dead) — 4 种死亡类型
local function EnemyDead(en, dead_kind)
  en.monmovestat = -4
  en.life = false
  en.dead = true
  en.hp = 0
  en.lastmon = false
  en.death_kind = dead_kind or 0
  if en.arrow_e then kill_entity(en.arrow_e); en.arrow_e = nil end
  -- 血条/影子释放 (C# script_hpbar.FreeSelect + DestroyShadow)
  if en.hp_bar_e then dse.ecs.set_mesh_visible(en.hp_bar_e, false) end
  if en.shadow then FreeShadow(en.shadow); en.shadow = nil end
  GainExp(en.haveExp or 1)
  local drop_rate = math.random(0, 100)
  if drop_rate < 30 then
    local te = GetItemBoxEntity()
    dse.ecs.set_transform_position(te, en.x, 0.5, en.z)
    dse.ecs.set_transform_scale(te, 0.4, 0.4, 0.4)
    -- giftbox_n 贴图自带颜色, 保持白色即可 (C# 由 prefab 材质决定)
    dse.ecs.set_mesh_color(te, 1.0, 1.0, 1.0, 1.0)
    table.insert(Entities.treasures, {
      x = en.x, z = en.z, kind = 0, level = en.level or 1,
      collected = false, dropped = true, pooled = true,
      e = te,
    })
  elseif drop_rate < 50 then
    local te = GetItemBoxEntity()
    dse.ecs.set_transform_position(te, en.x, 0.5, en.z)
    dse.ecs.set_transform_scale(te, 0.3, 0.3, 0.3)
    dse.ecs.set_mesh_color(te, 1.0, 1.0, 1.0, 1.0)
    table.insert(Entities.treasures, {
      x = en.x, z = en.z, kind = 1, level = 1,
      collected = false, dropped = true, pooled = true,
      e = te,
    })
  end
  if S.mon_die then dse.audio.play_sfx(S.mon_die, 0.7, 0) end
  en.death_timer = (dead_kind == 1) and 2.0 or 0.8
  G.enemykill = G.enemykill + 1
  G.totalkill = G.totalkill + 1
  G.combo = G.combo + 1
  G.combo_timer = 2.0
end

-- 敌人受击 (OnTriggerEnter — 16 层伤害系统)
local function EnemyDamaged(en, damage, from_x, from_z, attack_type, mass)
  if en.dead or not en.life then return end
  if en.grabed then return end
  local layer = attack_type_to_layer(attack_type)
  if layer < 16 then return end
  en.accuracy = Player.hitrate
  en.downhigh = Player.attack_rising

  -- 计算击退方向 (attackdir)
  if layer == 28 then
    en.attackdir_x = en.x - Player.x
    en.attackdir_z = en.z - Player.z
  else
    en.attackdir_x = en.x - from_x
    en.attackdir_z = en.z - from_z
  end
  local mag = math.sqrt(en.attackdir_x * en.attackdir_x + en.attackdir_z * en.attackdir_z)
  en.magnitude_behitdir = mag
  if mag > 0.001 then
    if mag < 0.08 then
      en.attackdir_x = en.attackdir_x / mag * 1.6
      en.attackdir_z = en.attackdir_z / mag * 1.6
    else
      en.attackdir_x = en.attackdir_x / mag
      en.attackdir_z = en.attackdir_z / mag
    end
  end

  en.attackforce = 40
  en.damage = damage or 0

  -- ===== 16 层伤害判定 =====
  if layer == 20 then
    en.attackforce = 40
    local rnd = math.random(0, 100)
    if rnd < (en.block - en.accuracy) and en.monmovestat >= 0 then
      spawn_damage_text(en.x, 2.0, en.z, "block!", 0.5, 0.8, 1.0, 16)
      if S.block then dse.audio.play_sfx(S.block, 0.5, 0) end
      en.knockback_x = en.attackdir_x * 10
      en.knockback_z = en.attackdir_z * 10
      en.knockback_timer = 0.1
      return
    end
    en.damage = Player.atk
    CameraHitcam()
    en.target_is_player = true
    spawn_hit_effect(en.x, 1.0, en.z, 5, "normal")

  elseif layer == 21 then
    en.attackforce = 30
    en.damage = damage or Player.atk
    en.downhigh = true
    CameraHitcam2(1.0)
    en.target_is_player = true
    spawn_hit_effect(en.x, 1.0, en.z, 5, "normal")

  elseif layer == 22 then
    en.attackforce = 10
    en.damage = mass or damage or 10
    en.downhigh = true
    CameraHitcam()
    en.target_is_player = true
    spawn_hit_effect(en.x, 1.0, en.z, 5, "normal")

  elseif layer == 23 then
    en.attackforce = 10
    local m = mass or 0
    if m == 0.1 then
      en.damage = 0
      en.poison = true
      en.poison_damage = Player.atk * 0.6
      en.poison_delay = 4.0
      en.old_delay = math.floor(en.poison_delay)
      en.downhigh = false
      en.target_is_player = true
      return
    end
    en.poison = true
    en.poison_damage = m
    en.damage = en.poison_damage * 50
    CameraHitcam()
    en.downhigh = true
    en.poison_delay = 5.0
    en.old_delay = math.floor(en.poison_delay)
    en.target_is_player = true
    spawn_hit_effect(en.x, 1.0, en.z, 5, "poison")

  elseif layer == 24 then
    en.attackforce = 0
    CameraHitcam()
    en.life = false
    en.petrify_rate = math.floor(mass or 50)
    en.petrify_timer = 2.5
    en.att_status = 2
    en.target_is_player = true
    spawn_hit_effect(en.x, 1.0, en.z, 5, "ice")
    if en.e then dse.ecs.set_mesh_color(en.e, 0.6, 0.6, 0.6, 1.0) end

  elseif layer == 25 then
    en.attackforce = 60
    en.damage = damage or Player.atk
    en.downhigh = false
    en.target_is_player = true
    spawn_hit_effect(en.x, 1.0, en.z, 5, "normal")

  elseif layer == 26 then
    en.attackforce = 0
    en.pierce = true
    en.life = false
    en.pierce_timer = 4.0
    en.damage = mass or damage or 50
    en.hp = en.hp - math.floor(en.damage)
    en.target_is_player = true
    spawn_hit_effect(en.x, 1.0, en.z, 5, "normal")
    if en.e then dse.ecs.set_mesh_color(en.e, 0.5, 0.3, 0.3, 0.7) end

  elseif layer == 27 then
    en.attackforce = 10
    en.damage = mass or damage or 10
    en.target_reset = true
    en.downhigh = false
    spawn_hit_effect(en.x, 1.0, en.z, 5, "normal")

  elseif layer == 28 then
    en.attackforce = 40
    en.damage = damage or Player.atk
    CameraHitcam()
    en.target_is_player = true
    spawn_hit_effect(en.x, 1.0, en.z, 5, "skill")

  elseif layer == 29 then
    en.attackforce = 0
    en.damage = (damage or Player.atk) * 0.4
    en.downhigh = false
    if en.risedrop then
      en.f_risefactor = 0.6
    else
      en.yaw = math.random(0, 360)
      en.f_risefactor = 3.4
    end
    en.risedrop = true
    en.target_is_player = true
    spawn_hit_effect(en.x, 1.0, en.z, 5, "skill")

  elseif layer == 30 then
    en.attackforce = 40
    en.damage = damage or Player.atk
    CameraHitcam()
    en.target_is_player = true
    if en.att_status ~= 1 then
      en.att_status = 1
      en.burn_timer = 3.0
      en.burn_damage_timer = 0.5
      spawn_hit_effect(en.x, 1.0, en.z, 5, "fire")
    end

  elseif layer == 31 then
    en.attackforce = 40
    en.downhigh = false
    en.damage = damage or Player.atk
    CameraHitcam()
    en.target_is_player = true
    if en.att_status ~= 1 then
      en.att_status = 2
      en.freeze_timer = 2.0
      spawn_hit_effect(en.x, 1.0, en.z, 5, "ice")
    end

  elseif layer == 16 then
    en.attackforce = -10
    en.downhigh = false
    en.damage = 0
    en.target_is_player = true
    if en.att_status ~= 1 then
      en.att_status = 2
      en.freeze_timer = 2.0
    end

  elseif layer == 17 then
    en.attackforce = 0
    en.damage = (damage or Player.atk) * 0.2
    CameraHitcam2(0.2)
    en.downhigh = false
    if en.risedrop then
      en.f_risefactor = 0.6
    else
      en.yaw = math.random(0, 360)
      en.f_risefactor = 1.2
    end
    en.risedrop = true
    en.target_is_player = true
    spawn_hit_effect(en.x, 1.0, en.z, 3, "normal")

  elseif layer == 18 then
    en.attackforce = 40
    en.damage = damage or Player.atk
    CameraHitcam()
    en.target_is_player = true
    if en.att_status ~= 3 then
      en.att_status = 3
      en.shock_timer = 1.2
      en.shock_damage_timer = 0.5
      spawn_hit_effect(en.x, 1.0, en.z, 5, "electric")
    end

  elseif layer == 19 then
    en.attackforce = 40
    en.damage = damage or Player.atk
    CameraHitcam()
    en.target_is_player = true
    if en.att_status ~= 4 then
      en.att_status = 4
      en.darken_timer = 2.0
      spawn_hit_effect(en.x, 1.0, en.z, 5, "normal")
    end
  end

  -- ===== 通用受击处理 =====
  en.movespeed = 0
  if S.slash1 then dse.audio.play_sfx(S.slash1, 0.4, 0) end
  -- 血溅特效 (C# script_monEf.CreatBlood)
  if en.attackdir_x ~= 0 or en.attackdir_z ~= 0 then
    spawn_blood_effect(en.x, 1.0, en.z, en.attackdir_x, en.attackdir_z, 0.5)
  end

  if not en.life then
    if en.pierce then en.visual_state = "pierced" end
  else
    en.hit_flash = 0.2
    if en.downhigh then
      en.yaw = math.random(0, 360)
      en.visual_state = "down_high"
      en.monmovestat = -1
    else
      en.visual_state = "down"
      en.monmovestat = -1
    end
    en.knockback_x = en.attackdir_x * en.attackforce
    en.knockback_z = en.attackdir_z * en.attackforce
    en.knockback_timer = 0.15
    if en.damage > 0 then
      en.hp = en.hp - math.floor(en.damage)
      local r2, g2, b2 = 1.0, 0.9, 0.3
      if attack_type == "fire" then r2, g2, b2 = 1.0, 0.4, 0.1
      elseif attack_type == "ice" then r2, g2, b2 = 0.3, 0.7, 1.0
      elseif attack_type == "electric" then r2, g2, b2 = 1.0, 1.0, 0.2
      elseif attack_type == "poison" then r2, g2, b2 = 0.5, 1.0, 0.2
      elseif attack_type == "strong" or attack_type == "rising" then r2, g2, b2 = 1.0, 0.2, 0.2
      elseif attack_type == "skill" then r2, g2, b2 = 0.5, 0.5, 1.0
      end
      spawn_damage_text(en.x, 2.0, en.z, tostring(math.floor(en.damage)), r2, g2, b2, 20)
    end
    if math.random(0, 4) == 0 then
      if S.hurt then dse.audio.play_sfx(S.hurt, 0.5, 0) end
    end
  end

  if en.hp <= 0 and en.life and not en.risedrop then
    EnemyDead(en, 2)
  end
  if en.target_reset then
    en.target_is_player = true
    en.target_reset = false
  end
end

-- 特殊关卡类型 (C# play_kind; 关卡映射存在于 Unity 场景配置, 此处按节奏自定义):
--   0=普通战斗  5=Boss(由 DB_Stage.bosscount 决定)  6=运粮车护送  7=守城
local function GetPlayKind(stage_idx)
  local stg = DB.DB_Stage[stage_idx % 90]
  if not stg or (stg.bosscount or 0) > 0 then return 0 end
  local m = stage_idx % 10
  if m == 1 then return 6 end
  if m == 4 then return 7 end
  return 0
end

-- 特殊关目标实体: 运粮车(6) / 大本营(7)
local function BuildObjective()
  Entities.objective = nil
  if G.play_kind == 6 then
    -- 运粮车: 从后方护送前进, 到达终点通关 (C# Cart.cs)
    local e = spawn_model(model_path(STRUCT_MODELS.cart), 0, 0.2, -8, 1, 1, 1, TEX.cart)
    Entities.objective = {
      e = e, kind = "cart",
      x = 0, z = -8, y = 0.2,
      hp = 400, maxhp = 400,
      speed = 0.14, goal_z = 24,
    }
  elseif G.play_kind == 7 then
    -- 大本营: 敌人向它推进, 被毁失败 (C# Tower.cs + Tank.cs)
    local e = spawn_model(model_path(STRUCT_MODELS.basecamp), 0, 0.2, 18, 1, 1, 1, TEX.basecamp)
    Entities.objective = {
      e = e, kind = "basecamp",
      x = 0, z = 18, y = 0.2,
      hp = 800, maxhp = 800,
    }
    -- 前方两座防御塔 (视觉)
    for i = 1, 2 do
      local tx = (i == 1) and -2.5 or 2.5
      local te = spawn_model(model_path(STRUCT_MODELS.tower), tx, 0, 10, 1, 1, 1, TEX.tower)
      table.insert(Entities.structures, { e = te, x = tx, z = 10, type = "tower" })
    end
  end
end

-- 特殊关目标受伤 (敌人攻击目标而非玩家)
local function DamageObjective(dmg)
  local obj = Entities.objective
  if not obj then return end
  obj.hp = obj.hp - (dmg or 1)
  print(string.format("[test] objective %s hit %d hp=%.0f", obj.kind, dmg or 1, obj.hp))
  spawn_damage_text(obj.x, obj.y + 3.2, obj.z, "-" .. math.floor(dmg or 1), 1.0, 0.2, 0.2, 18)
  if obj.e then
    dse.ecs.set_mesh_color(obj.e, 1.0, 0.6, 0.6, 1.0)
    obj._flash = 0.2
  end
  if obj.hp <= 0 then
    -- 目标被毁 → 失败 (C# Cart/Tower 被摧毁)
    if S.boom then dse.audio.play_sfx(S.boom, 0.8, 0) end
    spawn_hit_effect(obj.x, 1.0, obj.z, 12, "explode")
    G.mode = "game_over"
    if S.bgm_fail then dse.audio.play_bgm(S.bgm_fail, 0.6, false) end
    UISystem.show_chance()
  end
end

-- 特殊关目标更新 (运粮车前进)
local function UpdateObjective(dt)
  local obj = Entities.objective
  if not obj then return end
  if obj._flash and obj._flash > 0 then
    obj._flash = obj._flash - dt
    if obj._flash <= 0 and obj.e then dse.ecs.set_mesh_color(obj.e, 1.0, 1.0, 1.0, 1.0) end
  end
  if obj.kind == "cart" and G.mode == "play" then
    obj.z = obj.z + obj.speed * dt
    if obj.e then dse.ecs.set_transform_position(obj.e, obj.x, obj.y, obj.z) end
    if obj.z >= obj.goal_z then
      -- 护送到达 → 通关
      G.mode = "level_complete"
      G.level_complete_timer = 0
    end
  end
end

-- 敌人目标位置: 特殊关 (运粮车/大本营) 时非玩家目标优先 (C# target_fix)
local function EnemyTargetPos(en)
  if en.target_is_player or not Entities.objective then
    return Player.x, Player.z
  end
  return Entities.objective.x, Entities.objective.z
end

-- 敌人方向设定 (SetDir — C# InvokeRepeating 0.5s)
local function EnemySetDir(en)
  if not en.life or en.spawn_ing then return end
  local chamovestat = Player.chamovestat
  local tx, tz = EnemyTargetPos(en)
  local attackrange
  if en.att_status == 4 then
    en.direction_x = en.x - tx
    en.direction_z = en.z - tz
    local mag = math.sqrt(en.direction_x * en.direction_x + en.direction_z * en.direction_z)
    if mag > 0.001 then en.direction_x = en.direction_x / mag; en.direction_z = en.direction_z / mag end
    attackrange = 2.0
  elseif en.att_status == 3 then
    en.direction_x = -math.sin(math.rad(en.yaw))
    en.direction_z = math.cos(math.rad(en.yaw))
    attackrange = 2.0
  elseif en.att_status == 2 then
    en.direction_x = 0
    en.direction_z = 0
    attackrange = 2.0
  else
    attackrange = dist2d(en.x, en.z, tx, tz)
    en.direction_x = tx - en.x
    en.direction_z = tz - en.z
    local mag = math.sqrt(en.direction_x * en.direction_x + en.direction_z * en.direction_z)
    if mag > 0.001 then en.direction_x = en.direction_x / mag; en.direction_z = en.direction_z / mag end
  end
  if en.direction_x ~= 0 or en.direction_z ~= 0 then
    en.look_yaw = math.deg(atan2(en.direction_x, -en.direction_z))
  end
  if en.behaviour_delay < 0 then
    en.behaviour = math.random(0, 5)
    if en.behaviour == 0 then
      if S.footstep then dse.audio.play_sfx(S.footstep, 0.3, 0) end
    end
    en.behaviour_delay = 2.0
  end
  if attackrange < en.firerange then
    if en.attackstart then return end
    if chamovestat < 110 and en.monmovestat >= 0 then
      en.monmovestat = 11
      en.visual_state = "m_attack1"
      en.attackstart = true
      en.attack_impact = false
      en.attackstart_x = en.direction_x
      en.attackstart_z = en.direction_z
      en.attack_cd = 0.6 / (1 + en.speed_m_attack1)
      if en.target_reset then
        en.target_is_player = true
        en.target_reset = false
      end
    else
      en.movespeed = en.backspeed
      en.behaviour = -1
      en.behaviour_delay = 1.0
    end
  else
    local visible = math.abs(en.x - tx) < 20 and math.abs(en.z - tz) < 20
    if visible then
      if en.behaviour ~= -1 then
        if en.behaviour >= 3 then
          en.movespeed = en.runspeed * 0.4
          if en.monmovestat >= 0 then
            en.monmovestat = 0
            en.visual_state = "idle"
          end
        else
          en.movespeed = en.runspeed
          if en.monmovestat >= 0 then
            en.monmovestat = 1
            en.visual_state = "move"
          end
        end
      end
      if en.lastmon and en.showme then en.showme = false end
    else
      en.movespeed = en.runspeed * (0.7 + math.random() * 0.3)
      if en.monmovestat >= 0 then
        en.monmovestat = 1
        en.visual_state = "move"
      end
      if en.direction_x ~= 0 or en.direction_z ~= 0 then
        en.yaw = math.deg(atan2(en.direction_x, -en.direction_z))
      end
      if en.lastmon and not en.showme then en.showme = true end
    end
  end
end

-- 最后一只怪物指示 (CountDown)
local function EnemyCountDown(en)
  if en.life then
    en.lastmon = true
    en.showme = true
  end
end

-- 敌人更新 (Update)
local function UpdateEnemies(dt)
  for _, en in ipairs(Entities.enemies) do
    if en.dead then
      en.death_timer = en.death_timer - dt
      if en.death_timer <= 0 and en.e then
        dse.ecs.set_transform_scale(en.e, 0.1, 0.1, 0.1)
        dse.ecs.set_mesh_visible(en.e, false)
        en.e_removed = true
      end
      if en.e and en.death_timer > 0.3 then
        local prog = 1.0 - en.death_timer / 0.8
        local s = en.scale * (1.0 - prog * 0.7)
        dse.ecs.set_transform_scale(en.e, s * 1.5, s * 0.1, s)
        if en.death_kind == 1 then
          dse.ecs.set_transform_rotation(en.e, 0, en.yaw, 90)
        end
      end
      goto continue
    end

    if not en.life then
      if en.petrify_timer > 0 then
        en.petrify_timer = en.petrify_timer - dt
        if en.petrify_timer <= 0 then
          if en.petrify_rate > math.random(0, 100) then
            EnemyDead(en, 0)
          else
            en.life = true
            en.att_status = 0
            if en.e then dse.ecs.set_mesh_color(en.e, 1.0, 1.0, 1.0, 1.0) end
          end
        end
      end
      if en.pierce and en.pierce_timer > 0 then
        en.pierce_timer = en.pierce_timer - dt
        if en.pierce_timer <= 0 then
          if en.hp <= 0 then
            EnemyDead(en, 0)
          else
            en.life = true
            en.pierce = false
            en.monmovestat = -1
            en.visual_state = "down"
            if en.e then dse.ecs.set_mesh_color(en.e, 1.0, 1.0, 1.0, 1.0) end
          end
        end
      end
      goto update_entity
    end

    en.t = en.t + dt
    if en.hit_flash > 0 then en.hit_flash = en.hit_flash - dt end
    if en.knockback_timer > 0 then en.knockback_timer = en.knockback_timer - dt end
    if en.attack_cd > 0 then en.attack_cd = en.attack_cd - dt end
    if en.behaviour_delay > 0 then en.behaviour_delay = en.behaviour_delay - dt end
    if en.grab_timer > 0 then en.grab_timer = en.grab_timer - dt end

    en.setdir_timer = en.setdir_timer - dt
    if en.setdir_timer <= 0 then
      en.setdir_timer = 0.5
      EnemySetDir(en)
    end

    -- 升龙击飞物理
    if en.risedrop then
      en.y = en.y + en.f_risefactor * dt
      en.f_risefactor = en.f_risefactor - dt * 5.0
      en.yaw = en.yaw + dt * 360
      if en.y < 0 then
        en.y = 0
        en.risedrop = false
        en.f_risefactor = 0
        en.monmovestat = -1
        en.visual_state = "down"
        en.damage = Player.atk
        en.hp = en.hp - math.floor(en.damage)
        spawn_damage_text(en.x, 2.0, en.z, tostring(math.floor(en.damage)), 1.0, 0.2, 0.2, 20)
        spawn_hit_effect(en.x, 0.5, en.z, 5, "normal")
        CameraHitcam2(0.5)
        if en.hp <= 0 then
          EnemyDead(en, 3)
          goto update_entity
        else
          if en.attackdir_x ~= 0 or en.attackdir_z ~= 0 then
            en.yaw = math.deg(atan2(en.attackdir_x, -en.attackdir_z))
          end
        end
      end
      goto update_entity
    end

    -- 异常状态计时器
    if en.burn_timer > 0 then
      en.burn_timer = en.burn_timer - dt
      en.burn_damage_timer = en.burn_damage_timer - dt
      if en.burn_damage_timer <= 0 then
        en.burn_damage_timer = 1.0
        local burn_dmg = math.max(1, math.floor(Player.atk / 10))
        en.hp = en.hp - burn_dmg
        spawn_damage_text(en.x, 2.0, en.z, tostring(burn_dmg), 1.0, 0.4, 0.1, 16)
        if en.hp <= 0 then EnemyDead(en, 0); goto update_entity end
      end
      if en.burn_timer <= 0 then
        en.att_status = 0
        if en.e then dse.ecs.set_mesh_color(en.e, 1.0, 1.0, 1.0, 1.0) end
      end
    end
    if en.freeze_timer > 0 then
      en.freeze_timer = en.freeze_timer - dt
      if en.freeze_timer <= 0 then
        en.att_status = 0
        if en.e then dse.ecs.set_mesh_color(en.e, 1.0, 1.0, 1.0, 1.0) end
      end
    end
    if en.shock_timer > 0 then
      en.shock_timer = en.shock_timer - dt
      en.shock_damage_timer = en.shock_damage_timer - dt
      if en.shock_damage_timer <= 0 then
        en.shock_damage_timer = 0.1
        en.monmovestat = -1
        en.visual_state = "down"
      end
      if en.shock_timer <= 0 then
        en.att_status = 0
        if en.e then dse.ecs.set_mesh_color(en.e, 1.0, 1.0, 1.0, 1.0) end
      end
    end
    if en.darken_timer > 0 then
      en.darken_timer = en.darken_timer - dt
      if en.darken_timer <= 0 then
        en.att_status = 0
        if en.e then dse.ecs.set_mesh_color(en.e, 1.0, 1.0, 1.0, 1.0) end
      end
    end

    -- 中毒
    if en.poison then
      en.poison_delay = en.poison_delay - dt
      if en.poison_delay < 0 then
        en.poison = false
      else
        local cur_delay = math.floor(en.poison_delay)
        if cur_delay ~= en.old_delay then
          en.hp = en.hp - math.floor(en.poison_damage)
          spawn_damage_text(en.x, 2.0, en.z, tostring(math.floor(en.poison_damage)), 0.5, 1.0, 0.2, 16)
          en.old_delay = cur_delay
          if en.hp <= 0 then
            EnemyDead(en, 0)
            goto update_entity
          end
        end
      end
    end

    -- 被抓取状态
    if en.grabed then
      if en.e then
        dse.ecs.set_transform_position(en.e, Player.x, en.y, Player.z)
      end
      en.x, en.z = Player.x, Player.z
      if en.grab_timer < 1.2 and en.grab_phase == 1 then
        en.grab_phase = 2
        en.monmovestat = -3
        en.visual_state = "bethrust"
        en.hp = 0
        CameraHitcam2(1.0)
        if S.slash1 then dse.audio.play_sfx(S.slash1, 0.8, 0) end
        spawn_hit_effect(en.x, 1.5, en.z, 10, "skill")
        spawn_damage_text(en.x, 2.5, en.z, "FINISH!", 1.0, 0.2, 0.2, 24)
      elseif en.grab_timer < 0.6 and en.grab_phase == 2 then
        en.grab_phase = 3
        en.monmovestat = -4
        en.visual_state = "bekicked"
        en.knockback_x = en.attackdir_x * 100
        en.knockback_z = en.attackdir_z * 100
        en.knockback_timer = 0.3
        CameraHitcam2(1.0)
        if S.boom then dse.audio.play_sfx(S.boom, 0.7, 0) end
        if en.hp <= 0 then
          EnemyDead(en, 1)
          goto update_entity
        end
      elseif en.grab_timer <= 0 and en.grab_phase == 3 then
        en.grab_phase = 4
        en.grabed = false
        en.monmovestat = 0
        en.visual_state = "idle"
      end
      goto update_entity
    end

    -- 召唤动画
    if en.spawn_ing then
      en.spawn_timer = en.spawn_timer - dt
      if en.spawn_timer <= 0 then
        en.spawn_ing = false
        en.monmovestat = 0
        en.visual_state = "idle"
      end
      goto update_entity
    end

    -- 击退
    if en.knockback_timer > 0 then
      en.x = en.x + en.knockback_x * dt * 0.5
      en.z = en.z + en.knockback_z * dt * 0.5
    end

    -- 状态机 (do 块限定局部作用域, 避免 goto update_entity 跳入局部变量作用域)
    do
      local speed_mod = 1.0
      if en.att_status == 2 and en.freeze_timer > 0 then speed_mod = 0.0
      elseif en.att_status == 3 and en.shock_timer > 0 then speed_mod = 0.1
      end

      if en.monmovestat == 11 then
        en.look_yaw = math.deg(atan2(en.attackstart_x, -en.attackstart_z))
        en.yaw = lerp(en.yaw, en.look_yaw, dt * 6.0)
        en.x = en.x + en.attackstart_x * dt * en.moving_atk * 10
        en.z = en.z + en.attackstart_z * dt * en.moving_atk * 10
        if en.attack_cd <= 0 then
          en.monmovestat = 12
          en.visual_state = "m_attack1_i"
          en.attack_impact = false
        end
      elseif en.monmovestat == 12 then
        if not en.attack_impact then
          en.attack_impact = true
          if en.target_is_player or not Entities.objective then
            local actual_dmg = math.max(1, en.power - Player.defence)
            PlayerDamaged(en.power, en.x, en.z)
          else
            -- 特殊关: 攻击目标 (运粮车/大本营)
            DamageObjective(en.power)
          end
          if en.dash > 0 then
            en.x = en.x + en.direction_x * en.dash * 0.01 * dt
            en.z = en.z + en.direction_z * en.dash * 0.01 * dt
          end
          if S.slash then dse.audio.play_sfx(S.slash, 0.5, 0) end
          spawn_swing_ef(en.x, 1.0, en.z, en.yaw, 1.5, 0.3)
          CameraHitcam()
        end
        if en.attack_cd <= -0.3 then
          en.monmovestat = 1
          en.visual_state = "idle"
          en.attackstart = false
        end
      elseif en.monmovestat == -1 then
        if en.hit_flash <= 0 and en.behaviour_delay < 0 then
          en.monmovestat = 0
          en.visual_state = "idle"
        end
      elseif en.monmovestat == 1 or en.monmovestat == 0 then
        if (en.att_status == 0 or speed_mod > 0) and en.movespeed ~= 0 then
          en.x = en.x + en.direction_x * dt * en.movespeed * speed_mod * 10
          en.z = en.z + en.direction_z * dt * en.movespeed * speed_mod * 10
          en.yaw = lerp(en.yaw, en.look_yaw, dt * 3.0)
        end
      end

      -- 边界限制 (restrictArea)
      local sqr_mag = en.x * en.x + en.z * en.z
      if sqr_mag > en.restrict_area then
        local mag2 = math.sqrt(sqr_mag)
        en.x = en.x - en.x / mag2 * 5.0 * dt
        en.z = en.z - en.z / mag2 * 5.0 * dt
      end
    end

    -- 最后一只怪物指示箭头
    if en.lastmon and en.showme then
      local dx, dz = en.x - Player.x, en.z - Player.z
      local mag2 = math.sqrt(dx * dx + dz * dz)
      if mag2 > 0.001 then
        dx, dz = dx / mag2, dz / mag2
        if not en.arrow_e then
          en.arrow_e = dse.ecs.create_entity()
          dse.ecs.add_transform(en.arrow_e, Player.x + dx * 0.3, 0.02, Player.z + dz * 0.3, 0.3, 0.3, 0.3)
          local v = {0, 0, 0.5, -0.3, 0, -0.3, 0.3, 0, -0.3}
          local idx = {0, 1, 2}
          dse.ecs.add_mesh_renderer(en.arrow_e, 1.0, 0.3, 0.3, 0.8, v, idx)
          dse.ecs.set_mesh_shader_variant(en.arrow_e, "MESH_UNLIT")
        end
        dse.ecs.set_transform_position(en.arrow_e, Player.x + dx * 0.3, 0.02, Player.z + dz * 0.3)
        dse.ecs.set_transform_rotation(en.arrow_e, 0, math.deg(atan2(dx, -dz)), 0)
      end
    elseif en.arrow_e then
      dse.ecs.set_mesh_visible(en.arrow_e, false)
    end

    ::update_entity::
    -- 血条跟随 (C# Hp_bar.Update: parentmon.position + (0, posY, -0.02))
    if en.hp_bar_e then
      UpdateHpBar(en.hp_bar_e, en.x, 2.4, en.z, en.maxhp > 0 and (en.hp / en.maxhp) or 0)
    end
    if en.e then
      dse.ecs.set_transform_position(en.e, en.x, en.y, en.z)
      dse.ecs.set_transform_rotation(en.e, 0, en.yaw, 0)
      local sx, sy, sz = en.scale, en.scale, en.scale
      local r, g, b, a = 1.0, 1.0, 1.0, 1.0

      if en.spawn_ing then
        local grow = 1.0 - (en.spawn_timer / 1.5)
        sx = en.scale * grow; sy = en.scale * grow; sz = en.scale * grow
        r = 0.5 + 0.5 * grow; g = 0.8; b = 1.0
      elseif en.risedrop then
        en.yaw = en.yaw + dt * 720
        sx = en.scale * 0.8; sy = en.scale * 0.8; sz = en.scale * 0.8
        r = 1.0; g = 0.6; b = 0.3
      elseif en.monmovestat == 11 then
        sy = en.scale * 1.1; sx = en.scale * 0.9
        sx = sx * (1.0 + 0.08 * math.sin(en.t * 25))
      elseif en.monmovestat == 12 then
        sy = en.scale * 0.85; sx = en.scale * 1.2
        sx = sx * (1.0 + 0.15 * math.sin(en.t * 30))
      elseif en.monmovestat == -1 then
        if en.downhigh then
          sy = en.scale * 0.3; sx = en.scale * 1.6; sz = en.scale * 1.6
        else
          sy = en.scale * 0.5; sx = en.scale * 1.3
        end
      elseif en.monmovestat == -2 then
        sy = en.scale * 0.4; sx = en.scale * 1.5
      elseif en.monmovestat == -3 then
        sy = en.scale * 0.3; sx = en.scale * 1.3
        r = 1.0; g = 0.2; b = 0.2
      elseif en.monmovestat == -4 then
        sy = en.scale * 0.3; sx = en.scale * 1.8
      elseif en.visual_state == "move" then
        sx = en.scale * (1.0 + 0.06 * math.sin(en.t * 12))
        sy = en.scale * (1.0 - 0.03 * math.sin(en.t * 12))
      elseif en.visual_state == "idle" then
        sy = en.scale * (1.0 + 0.02 * math.sin(en.t * 4))
      end

      if en.hit_flash > 0 then
        r = 1.0; g = 0.3; b = 0.3
      end
      if en.att_status == 1 then
        r = 1.0; g = 0.4 * (0.8 + 0.2 * math.sin(en.t * 20)); b = 0.1
      elseif en.att_status == 2 and en.freeze_timer > 0 then
        r = 0.5; g = 0.7; b = 1.0
      elseif en.att_status == 3 then
        r = 1.0; g = 1.0; b = 0.2
        en.yaw = en.yaw + (math.random() - 0.5) * 2
      elseif en.att_status == 4 then
        r = 0.4; g = 0.2; b = 0.5
      end
      if en.poison then
        g = math.min(1.0, g + 0.3)
        r = r * 0.7
      end
      if en.hp < en.maxhp * 0.3 then
        r = r * 0.8; g = g * 0.8; b = b * 0.8
      end

      dse.ecs.set_transform_scale(en.e, sx, sy, sz)
      dse.ecs.set_mesh_color(en.e, r, g, b, a)
    end

    ::continue::
  end

  -- 检测最后一只怪物
  local alive_count = 0
  local last_alive = nil
  for _, en in ipairs(Entities.enemies) do
    if not en.dead and en.life then
      alive_count = alive_count + 1
      last_alive = en
    end
  end
  if alive_count == 1 and last_alive and not last_alive.lastmon then
    EnemyCountDown(last_alive)
  end
  if alive_count > 1 then
    for _, en in ipairs(Entities.enemies) do
      if en.lastmon and en ~= last_alive then
        en.lastmon = false
        if en.arrow_e then kill_entity(en.arrow_e); en.arrow_e = nil end
      end
    end
  end
end
-- ============================================================================
-- Boss AI (AI_Boss01 完整移植)
-- 100% 忠实还原 C# AI_Boss01.cs
-- 16 层伤害系统 / 3 种攻击模式 / HP 阈段转换 / 入场特写 / 属性异常
-- 武器系统见 weapon_damage.lua (clone_weapon / ef_secondweapon)
-- ============================================================================

-- 创建 Boss (Awake + Start 合并)
local function CreateBoss(bosskind, x, z)
  local data = DB.DB_Boss[bosskind]
  if not data then return nil end

  local model = BOSS_MODELS[bosskind + 1] or MON_MODELS[1]
  -- Boss 贴图: 0-9 → boss01~10; 10 → mon0285; 11 → bosssp/finalboss
  local boss_tex
  if bosskind <= 9 then
    boss_tex = TEX.boss and TEX.boss[bosskind + 1]
  elseif bosskind == 10 then
    boss_tex = TEX.mon0285
  else
    boss_tex = TEX.bosssp or TEX.finalboss
  end
  local e = spawn_model(model_path(model), x, 0, z, 2.0, 2.0, 2.0, boss_tex)

  local m1 = data.matk1 or {0,0}
  local m2 = data.matk2 or {0,0}
  local m3 = data.matk3 or {0,0}

  local boss = {
    e = e, bosskind = bosskind,
    x = x, z = z, y = 0, yaw = 0,
    -- 原始数据 (用于 SetLevel 缩放)
    _raw_maxhp = data.maxhp,
    _raw_power = {data.power1, data.power2, data.power3},
    -- 缩放后属性
    maxhp = data.maxhp, hp = data.maxhp,
    power = {data.power1, data.power2, data.power3},
    haveExp = data.haveExp, block = data.block,
    -- 攻击范围 (3 种攻击模式)
    firerange = {data.firerange1, data.firerange2, data.firerange3},
    turnspeed = data.turnspeed, runspeed = data.runspeed,
    dash = {data.dash1, data.dash2, data.dash3},
    -- 攻击位移 {delay, speed}
    moving_atk = {{x=m1[1] or 0,y=m1[2] or 0},{x=m2[1] or 0,y=m2[2] or 0},{x=m3[1] or 0,y=m3[2] or 0}},
    -- 武器特效附件 (0=无 1=跟随 2=第二武器)
    attach_ef = {data.aef1 or 0, data.aef2 or 0, data.aef3 or 0},
    -- 碰撞体开关
    collideroff = {data.coff1 or false, data.coff2 or false, data.coff3 or false},
    -- 动画速度
    speed_move = data.speed_move,
    speed_b_attack = {data.speed_b_attack1, data.speed_b_attack2, data.speed_b_attack3},
    speed_b_attack_i = {data.speed_b_attack1_i, data.speed_b_attack2_i, data.speed_b_attack3_i},
    speed_idle = data.speed_idle, speed_down = data.speed_down,
    sizekind = data.sizekind,
    -- 等级缩放
    level = 1, plusmaxhp = 1.0, pluspower = 1.0, restrict_area = 625,
    -- AI 状态机 (monmovestat: 0=idle 1=move 11=attack 12=impact -1=down -4=dead)
    monmovestat = 0, life = true, invince = false,
    -- 攻击系统
    attackstart = false, impact = false,
    currentAtk = 0, atkkind = 0, setattackkind = true,
    jump = false, m_atk_delay = 0, attack_duration = 0.5,
    -- 动画阶段 (代替 Unity Animation)
    anim_phase = "idle", anim_timer = 0,
    -- 行为
    behaviour = 0, behaviour_delay = 2.0,
    -- 方向
    direction_x = 0, direction_z = -1, look_yaw = 0,
    -- 受击
    hit_flash = 0, attackdir_x = 0, attackdir_z = 0,
    attackforce = 0, damage = 0, accuracy = 90,
    knockback_x = 0, knockback_z = 0, knockback_timer = 0,
    -- 异常状态 (att_status: 0=normal 1=fire 2=freeze 3=shock 4=darken)
    att_status = 1,
    burn_timer = 0, burn_damage_timer = 0,
    freeze_timer = 0, shock_timer = 0, shock_damage_timer = 0,
    darken_timer = 0, petrify_timer = 0, petrify_rate = 0,
    poison = false, poison_delay = 0, poison_damage = 0, old_delay = 0,
    -- 特写
    bosscutin = false, intro_timer = 0,
    -- 指示箭头
    showme = false, arrow_e = nil,
    -- 武器实体 (C# clone_weapon[3] / ef_secondweapon)
    clone_weapons = {}, ef_secondweapon = nil,
    _impact_hit_player = false,
    -- 死亡
    dead = false, death_timer = 0, death_kind = 0,
    -- 杂项
    base_x = x, base_z = z, t = 0,
    visual_state = "idle", setdir_timer = 0.1,
  }

  -- 血条 (C# script_monEf.CreatHpbar + script_hpbar.Damaged)
  boss.hp_bar_e = CreateHpBarEntity(4.2)
  -- 影子 (C# script_monEf.CreatShadow)
  boss.shadow = GetShadow()

  -- 创建指示箭头实体
  do
    local ae = dse.ecs.create_entity()
    dse.ecs.add_transform(ae, 0, 0.02, 0, 0.3, 0.3, 0.3)
    local v = {0,0,0.5, -0.3,0,-0.3, 0.3,0,-0.3}
    dse.ecs.add_mesh_renderer(ae, 1.0, 0.3, 0.3, 1.0, v, {0,1,2})
    dse.ecs.set_mesh_shader_variant(ae, "MESH_UNLIT")
    dse.ecs.set_mesh_visible(ae, false)
    boss.arrow_e = ae
  end

  -- enemykind == 0: 入场特写 (C# Start 中 enemykind==0 分支)
  if bosskind == 0 then
    boss.intro_timer = 3.0
    boss.invince = true
    boss.bosscutin = true
    boss.anim_phase = "trans"
    G.boss_intro_timer = 3.0
    -- 相机聚焦 Boss (C# script_cam.LookTarget(mytransform, 30, 4f))
    CameraLookTarget(e, 30, 4.0)
    -- 玩家被推后 (C# cha1.rigidbody.AddForce(cha1.forward * -240f))
    local pdx = math.sin(math.rad(Player.yaw))
    local pdz = -math.cos(math.rad(Player.yaw))
    Player.knockback_x = -pdx * 240
    Player.knockback_z = -pdz * 240
    Player.knockback_timer = 0.15
  end

  return boss
end

-- Boss 等级缩放 (C# SetLevel)
local function BossSetLevel(boss, level, restrictArea)
  boss.level = level + 1
  boss.restrict_area = restrictArea or 625
  boss.plusmaxhp = 0.55 * (boss.level / 3.0) + 0.5
  if boss.level >= 61 then
    boss.pluspower = 0.05 * (boss.level / 3.0) + 3.5
  elseif boss.level >= 31 then
    boss.pluspower = 2.5
  else
    boss.pluspower = 0.05 * (boss.level / 3.0) + 0.8
  end
  boss.block = boss.block + math.floor(boss.level * 0.5)
  boss.haveExp = boss.haveExp + boss.level
  boss.maxhp = math.floor(boss._raw_maxhp * boss.plusmaxhp)
  boss.power[1] = math.floor(boss._raw_power[1] * boss.pluspower)
  boss.power[2] = math.floor(boss._raw_power[2] * boss.pluspower)
  boss.power[3] = math.floor(boss._raw_power[3] * boss.pluspower)
  boss.hp = boss.maxhp
end

-- Boss 死亡 (C# Dead)
local function BossDead(boss, dead_kind)
  boss.monmovestat = -4
  boss.life = false
  boss.dead = true
  boss.hp = 0
  boss.death_kind = dead_kind or 0
  boss.death_timer = 2.0
  if boss.arrow_e then kill_entity(boss.arrow_e); boss.arrow_e = nil end
  -- 血条释放 (C# script_hpbar.FreeSelect)
  if boss.hp_bar_e then dse.ecs.set_mesh_visible(boss.hp_bar_e, false) end
  -- 影子释放 (C# script_monEf.DestroyShadow)
  if boss.shadow then FreeShadow(boss.shadow); boss.shadow = nil end
  -- 武器实体释放 (C# clone_weapon / ef_secondweapon 销毁)
  for _, w in ipairs(boss.clone_weapons) do
    if w then
      w.active = false
      if w.e then dse.ecs.set_mesh_visible(w.e, false) end
    end
  end
  BossSecondWeaponHide(boss)
  -- 武器掉落 (C# weapon.GetComponent<WeaponDrop>().Drop(true))
  spawn_weapon_drop(boss.x, boss.z, boss.yaw)
  if boss.bosskind == 9 then
    -- C# ChangeBoss: 变身为下一个 Boss
    local new_boss = CreateBoss(10, boss.x, boss.z)
    if new_boss then
      BossSetLevel(new_boss, boss.level - 1, boss.restrict_area)
      Entities.boss = new_boss
      G.boss_intro_timer = 1.0
      spawn_damage_text(boss.x, 4.0, boss.z, "变身!", 1.0, 0.2, 0.2, 28)
    end
  else
    GainExp(boss.haveExp or 100)
    G.enemykill = G.enemykill + 1
    G.totalkill = G.totalkill + 1
    G.combo = G.combo + 1
    G.combo_timer = 2.0
    -- BGM 切换回普通音乐 (C# ChangeBGM(false))
    ChangeBGM(false)
    -- 掉落宝箱 (C# script_monEf.SetItemBox)
    local te = GetItemBoxEntity()
    dse.ecs.set_transform_position(te, boss.x, 0.5, boss.z)
    dse.ecs.set_transform_scale(te, 0.5, 0.5, 0.5)
    dse.ecs.set_mesh_color(te, 1.0, 0.2, 0.2, 1.0)
    table.insert(Entities.treasures, {
      x = boss.x, z = boss.z, kind = 2, level = boss.level or 1,
      collected = false, dropped = true, pooled = true,
      e = te,
    })
    if S.bgm_victory then dse.audio.play_bgm(S.bgm_victory, 0.6, false) end
  end
  if S.boom then dse.audio.play_sfx(S.boom, 1.0, 0) end
  CameraHitcam2(2.0)
end

-- Boss 受击 (C# OnTriggerEnter — 16 层伤害系统)
local function BossDamaged(boss, damage, from_x, from_z, attack_type, mass)
  if boss.dead or not boss.life then return end
  if boss.invince then return end

  local layer = attack_type_to_layer(attack_type)
  if layer < 16 then return end

  boss.setattackkind = true
  boss.accuracy = Player.hitrate
  local atk = Player.atk

  -- 计算击退方向 (attackdir)
  if layer == 28 then
    boss.attackdir_x = boss.x - Player.x
    boss.attackdir_z = boss.z - Player.z
  else
    boss.attackdir_x = boss.x - from_x
    boss.attackdir_z = boss.z - from_z
  end
  local mag = math.sqrt(boss.attackdir_x * boss.attackdir_x + boss.attackdir_z * boss.attackdir_z)
  if mag > 0.001 then
    boss.attackdir_x = boss.attackdir_x / mag
    boss.attackdir_z = boss.attackdir_z / mag
  else
    boss.attackdir_x = 0; boss.attackdir_z = -1
  end

  boss.attackforce = 40
  boss.damage = damage or 0

  -- ===== 16 层伤害判定 (忠实还原 C# switch(layer)) =====
  if layer == 20 then
    -- 普通攻击: 格挡判定
    if math.random(0, 100) < (boss.block - boss.accuracy) and boss.monmovestat >= 0 then
      boss.knockback_x = boss.attackdir_x * 10
      boss.knockback_z = boss.attackdir_z * 10
      boss.knockback_timer = 0.1
      spawn_damage_text(boss.x, 3.0, boss.z, "block!", 0.5, 0.8, 1.0, 20)
      if S.block then dse.audio.play_sfx(S.block, 0.6, 0) end
      return
    end
    boss.attackforce = 60; boss.damage = atk
    CameraHitcam()
    spawn_hit_effect(boss.x, 2.0, boss.z, 8, "normal")

  elseif layer == 21 then
    boss.attackforce = 100; boss.damage = atk
    CameraHitcam2(1.0)
    spawn_hit_effect(boss.x, 2.0, boss.z, 8, "normal")

  elseif layer == 22 then
    boss.attackforce = 50; boss.damage = mass or damage or 10
    CameraHitcam()
    spawn_hit_effect(boss.x, 2.0, boss.z, 8, "normal")

  elseif layer == 23 then
    -- 毒击
    local m = mass or 0
    if m == 0.1 then
      boss.damage = 0
      boss.poison = true
      boss.poison_damage = atk * 0.6
      boss.poison_delay = 4.0
      boss.old_delay = math.floor(boss.poison_delay)
      return
    end
    boss.attackforce = 100
    boss.poison_damage = m
    boss.damage = boss.poison_damage * 2
    boss.poison = true
    boss.poison_delay = 12.0
    boss.old_delay = math.floor(boss.poison_delay)
    CameraHitcam()
    spawn_hit_effect(boss.x, 2.0, boss.z, 8, "poison")

  elseif layer == 24 then
    -- 石化
    boss.attackforce = 100; boss.life = false
    boss.petrify_rate = math.floor((mass or 50)) * 0.01
    boss.petrify_timer = 2.5; boss.att_status = 2
    CameraHitcam()
    if boss.e then dse.ecs.set_mesh_color(boss.e, 0.6, 0.6, 0.6, 1.0) end
    spawn_hit_effect(boss.x, 2.0, boss.z, 8, "ice")

  elseif layer == 25 then
    boss.attackforce = 60; boss.damage = atk
    spawn_hit_effect(boss.x, 2.0, boss.z, 8, "normal")

  elseif layer == 26 then
    -- 穿透
    boss.anim_phase = "down"; boss.anim_timer = 0
    boss.attackforce = 10; boss.monmovestat = -1
    boss.damage = mass or damage or 50
    boss.hp = boss.hp - math.floor(boss.damage)
    spawn_hit_effect(boss.x, 2.0, boss.z, 8, "normal")

  elseif layer == 28 then
    boss.attackforce = 100; boss.damage = atk
    CameraHitcam()
    spawn_hit_effect(boss.x, 2.0, boss.z, 10, "skill")

  elseif layer == 29 then
    boss.attackforce = 50; boss.damage = atk * 0.4
    spawn_hit_effect(boss.x, 2.0, boss.z, 8, "skill")

  elseif layer == 30 then
    -- 火
    boss.attackforce = 100; boss.damage = atk
    CameraHitcam()
    if boss.att_status ~= 1 then
      boss.att_status = 1; boss.burn_timer = 3.0; boss.burn_damage_timer = 0.5
    end
    spawn_hit_effect(boss.x, 2.0, boss.z, 8, "fire")

  elseif layer == 31 then
    -- 冰
    boss.attackforce = 100; boss.damage = atk
    CameraHitcam()
    if boss.att_status ~= 1 then
      boss.att_status = 2; boss.freeze_timer = 2.0
    end
    spawn_hit_effect(boss.x, 2.0, boss.z, 8, "ice")

  elseif layer == 16 then
    -- 雷麻痹
    boss.attackforce = -50; boss.damage = 0
    if boss.att_status ~= 1 then
      boss.att_status = 2; boss.freeze_timer = 2.0
    end

  elseif layer == 17 then
    boss.attackforce = 40; boss.damage = mass or damage or 10
    CameraHitcam2(0.2)
    spawn_hit_effect(boss.x, 2.0, boss.z, 5, "normal")

  elseif layer == 18 then
    boss.attackforce = 100; boss.damage = atk
    CameraHitcam()
    spawn_hit_effect(boss.x, 2.0, boss.z, 8, "electric")

  elseif layer == 19 then
    boss.attackforce = 100; boss.damage = atk
    CameraHitcam()
    spawn_hit_effect(boss.x, 2.0, boss.z, 8, "normal")
  end

  -- ===== 通用受击处理 (C# OnTriggerEnter 后段) =====
  if not boss.life then
    if S.mon_die then dse.audio.play_sfx(S.mon_die, 0.5, 0) end
    return
  end

  -- 血溅特效 (C# script_monEf.CreatBlood)
  if boss.attackdir_x ~= 0 or boss.attackdir_z ~= 0 then
    spawn_blood_effect(boss.x, 2.0, boss.z, boss.attackdir_x, boss.attackdir_z, 0.7)
  end

  if S.slash1 then dse.audio.play_sfx(S.slash1, 0.5, 0) end

  -- HP 阶段转换 (C# num = maxhp * 0.3, num2 = hp / num)
  local num = math.floor(boss.maxhp * 0.3)
  local num2 = math.floor(boss.hp / num)
  if boss.hp == boss.maxhp then num2 = num2 - 1 end

  boss.hp = boss.hp - math.floor(boss.damage)
  if boss.hp < 0 then boss.hp = 0 end

  -- 伤害数字
  local r2, g2, b2 = 1.0, 0.9, 0.3
  if attack_type == "fire" then r2, g2, b2 = 1.0, 0.4, 0.1
  elseif attack_type == "ice" then r2, g2, b2 = 0.3, 0.7, 1.0
  elseif attack_type == "electric" then r2, g2, b2 = 1.0, 1.0, 0.2
  elseif attack_type == "poison" then r2, g2, b2 = 0.5, 1.0, 0.2
  elseif attack_type == "strong" or attack_type == "rising" then r2, g2, b2 = 1.0, 0.2, 0.2
  elseif attack_type == "skill" then r2, g2, b2 = 0.5, 0.5, 1.0
  end
  spawn_damage_text(boss.x, 3.0, boss.z, tostring(math.floor(boss.damage)), r2, g2, b2, 24)

  G.combo = G.combo + 1
  G.combo_timer = 2.0
  boss.hit_flash = 0.2

  -- 死亡判定
  if boss.hp <= 0 and boss.life then
    BossDead(boss, 2)
    return
  end

  -- HP 阶段转换 (C# hp < num * num2 → 召唤 + 倒地)
  if num > 0 and boss.hp < num * num2 then
    boss.attackforce = 300
    boss.anim_phase = "down"
    boss.anim_timer = 0
    boss.monmovestat = -1
    -- 召唤 4 只小怪 (C# script_spawn.Summon(4, position))
    BossSummon(4, {x = boss.x, z = boss.z})
    spawn_damage_text(boss.x, 4.0, boss.z, "召唤!", 1.0, 0.5, 0.2, 24)
    if S.boom then dse.audio.play_sfx(S.boom, 0.8, 0) end
    CameraHitcam2(1.5)
  end

  -- 应用击退 (C# rigidbody.AddForce(attackdir * attackforce))
  boss.knockback_x = boss.attackdir_x * boss.attackforce * 0.01
  boss.knockback_z = boss.attackdir_z * boss.attackforce * 0.01
  boss.knockback_timer = 0.15
end

-- Boss 方向设定 (C# SetDir — InvokeRepeating 0.5s)
local function BossSetDir(boss)
  if not boss.life then return end

  -- 行为随机化
  if boss.behaviour_delay < 0 and boss.bosscutin then
    boss.behaviour = math.random(0, 5)
    if boss.behaviour == 0 and S.footstep then
      dse.audio.play_sfx(S.footstep, 0.4, 0)
    end
    boss.behaviour_delay = 1.0
  end

  local attackrange = dist2d(boss.x, boss.z, Player.x, Player.z)

  -- Boss 近身特写触发 (C# !bosscutin && attackrange < 0.7)
  if not boss.bosscutin and attackrange < 0.7 then
    BossCutin(boss.bosskind)
  end

  -- 方向计算 (C# directionVector 朝向玩家)
  boss.direction_x = Player.x - boss.x
  boss.direction_z = Player.z - boss.z
  local mag = math.sqrt(boss.direction_x * boss.direction_x + boss.direction_z * boss.direction_z)
  if mag > 0.001 then
    boss.direction_x = boss.direction_x / mag
    boss.direction_z = boss.direction_z / mag
  end
  if boss.direction_x ~= 0 or boss.direction_z ~= 0 then
    boss.look_yaw = math.deg(atan2(boss.direction_x, -boss.direction_z))
  end

  local chamovestat = Player.chamovestat

  if attackrange < boss.firerange[3] then
    -- 在攻击范围内
    boss.showme = false
    if boss.arrow_e then dse.ecs.set_mesh_visible(boss.arrow_e, false) end
    if boss.attackstart then return end

    if chamovestat < 50 and boss.monmovestat >= 0 then
      -- 选择攻击类型 (C# setattackkind)
      if boss.setattackkind then
        boss.atkkind = math.random(0, 2)
        boss.setattackkind = false
      end

      local fr = boss.firerange
      local can_attack = false
      if boss.atkkind == 2 then
        if attackrange < fr[3] then boss.currentAtk = 2; boss.setattackkind = true; can_attack = true
        else can_attack = false end
      elseif boss.atkkind == 1 then
        if attackrange < fr[2] then boss.currentAtk = 1; boss.setattackkind = true; can_attack = true
        else can_attack = false end
      else
        if attackrange < fr[1] then boss.currentAtk = 0; boss.setattackkind = true; can_attack = true
        else can_attack = false end
      end

      if can_attack then
        boss.anim_phase = "attack"
        boss.anim_timer = 0
        boss.attackstart = true
        boss.impact = false
        boss.m_atk_delay = 0
        boss.monmovestat = 11
        local spd = boss.speed_b_attack[boss.currentAtk + 1] or 0.2
        boss.attack_duration = 0.6 / (1 + spd)
      else
        boss.anim_phase = "move"
      end
    else
      if not boss.attackstart then
        boss.anim_phase = "idle"
        boss.behaviour = 4
        boss.behaviour_delay = 1.0
      end
    end
  else
    -- 太远, 显示箭头
    if not boss.attackstart then
      if boss.behaviour >= 5 then
        boss.anim_phase = "idle"
      else
        boss.anim_phase = "move"
      end
      boss.showme = true
      if boss.arrow_e then dse.ecs.set_mesh_visible(boss.arrow_e, true) end
    end
  end
end

-- Boss 特写触发 (C# Spawn.BossCutin + ChangeBGM)
-- 特写动画 Cutin01 见 cutin01.lua
local function BossCutin(enemykind)
  if not Entities.boss then return end
  local boss = Entities.boss
  boss.bosscutin = true
  -- BGM 切换到 Boss 音乐 (C# ChangeBGM(true))
  if S.bgm_boss then dse.audio.play_bgm(S.bgm_boss, 0.8, true) end
  -- Cutin01 特写 (C# cut_boss.CutinOn(...) — 原版 Boss 名图)
  local name = DB.BossNames and DB.BossNames[boss.bosskind] or "BOSS"
  local ctex = TEX.boss_name and TEX.boss_name[boss.bosskind + 1]
  CutinOn(ctex, name)
  if S.mon_die then dse.audio.play_sfx(S.mon_die, 0.8, 0) end
end

-- BGM 切换 (C# ChangeBGM)
local function ChangeBGM(is_boss)
  if is_boss then
    if S.bgm_boss then dse.audio.play_bgm(S.bgm_boss, 0.8, true) end
  else
    local bgm = {S.bgm_stage1, S.bgm_stage2, S.bgm_stage3}
    local bgm_idx = (G.stage_index % 3) + 1
    if bgm[bgm_idx] then dse.audio.play_bgm(bgm[bgm_idx], 0.6, true) end
  end
end

-- Boss 更新 (C# Update — 完整状态机)
local function UpdateBoss(dt)
  local boss = Entities.boss
  if not boss then return end

  -- 武器生命周期 (C# WeaponDamage.Update + ef_secondweapon)
  BossWeaponUpdate(dt)
  BossSecondWeaponTick(boss, dt)

  -- ===== 死亡处理 =====
  if boss.dead then
    boss.death_timer = boss.death_timer - dt
    if boss.death_timer <= 0 and boss.e then
      dse.ecs.set_mesh_visible(boss.e, false)
    end
    if boss.death_timer > 1.0 then
      if boss.e then
        local prog = 1.0 - boss.death_timer / 2.0
        local s = 2.0 * (1.0 - prog * 0.8)
        dse.ecs.set_transform_scale(boss.e, s, s * 0.1, s)
        dse.ecs.set_mesh_color(boss.e, 1.0, 0.5, 0.3, 1.0 - prog)
      end
    end
    return
  end

  -- ===== 石化/冻结恢复 =====
  if not boss.life then
    if boss.petrify_timer > 0 then
      boss.petrify_timer = boss.petrify_timer - dt
      if boss.petrify_timer <= 0 then
        if boss.petrify_rate > math.random(0, 100) then
          BossDead(boss, 0)
        else
          boss.life = true
          boss.att_status = 1
          if boss.e then dse.ecs.set_mesh_color(boss.e, 1.0, 1.0, 1.0, 1.0) end
          boss.monmovestat = 0
          boss.anim_phase = "idle"
        end
      end
    end
    if boss.freeze_timer > 0 then
      boss.freeze_timer = boss.freeze_timer - dt
      if boss.freeze_timer <= 0 then
        boss.att_status = 1
        boss.monmovestat = 0
        boss.anim_phase = "idle"
      end
    end
    if boss.e then
      dse.ecs.set_transform_position(boss.e, boss.x, boss.y, boss.z)
      dse.ecs.set_transform_rotation(boss.e, 0, boss.yaw, 0)
    end
    return
  end

  -- ===== 更新计时器 =====
  boss.t = boss.t + dt
  boss.behaviour_delay = boss.behaviour_delay - dt
  if boss.hit_flash > 0 then boss.hit_flash = boss.hit_flash - dt end
  if boss.knockback_timer > 0 then boss.knockback_timer = boss.knockback_timer - dt end
  if boss.intro_timer > 0 then boss.intro_timer = boss.intro_timer - dt end
  if boss.burn_timer > 0 then boss.burn_timer = boss.burn_timer - dt end
  boss.anim_timer = boss.anim_timer + dt

  -- ===== 入场特写 (C# myanimation.IsPlaying("trans")) =====
  if boss.anim_phase == "trans" then
    if boss.intro_timer <= 0 then
      boss.invince = false
      boss.anim_phase = "idle"
    else
      if boss.e then
        local s = 2.0 * (1.0 + 0.15 * math.sin(boss.t * 8))
        dse.ecs.set_transform_scale(boss.e, s, s, s)
        dse.ecs.set_mesh_color(boss.e, 1.0, 0.8, 0.2, 1.0)
        dse.ecs.set_transform_position(boss.e, boss.x, boss.y, boss.z)
      end
      return
    end
  end

  -- ===== 倒地状态 (C# b_down) =====
  if boss.anim_phase == "down" then
    boss.monmovestat = -1
    local down_dur = 0.6 / (1 + (boss.speed_down or 0.22))
    if boss.anim_timer > down_dur then
      boss.anim_phase = "idle"
      boss.anim_timer = 0
      boss.monmovestat = 0
      boss.attackstart = false
    end
  else
    -- ===== 正常状态机 =====
    if boss.anim_phase == "attack" then
      -- 攻击阶段 (C# b_attack1/2/3)
      boss.monmovestat = 11
      boss.impact = false

      -- 第二武器激活 (C# attach_weaponEf==2 → ef_secondweapon.active = true)
      if boss.attach_ef[boss.currentAtk + 1] == 2 then
        BossSecondWeaponShow(boss)
      end

      -- 转向
      if boss.direction_x ~= 0 or boss.direction_z ~= 0 then
        local target_yaw = math.deg(atan2(boss.direction_x, -boss.direction_z))
        boss.yaw = lerp(boss.yaw, target_yaw, clamp(dt * boss.turnspeed * 10, 0, 1))
      end

      -- 碰撞体关闭
      if boss.collideroff[boss.currentAtk + 1] and not boss.jump then
        boss.jump = true
      end

      -- 攻击位移 (C# moving_atk)
      local matk = boss.moving_atk[boss.currentAtk + 1]
      if matk and (matk.x ~= 0 or matk.y ~= 0) then
        if boss.m_atk_delay > matk.x then
          boss.x = boss.x + boss.direction_x * dt * matk.y
          boss.z = boss.z + boss.direction_z * dt * matk.y
        else
          boss.m_atk_delay = boss.m_atk_delay + dt
        end
      end

      -- 攻击动画结束 → 进入冲击阶段
      if boss.anim_timer > boss.attack_duration then
        boss.anim_phase = "attack_i"
        boss.anim_timer = 0
        boss.monmovestat = 12
      end

    elseif boss.anim_phase == "attack_i" then
      -- 冲击阶段 (C# b_attack1_i/2_i/3_i)
      boss.monmovestat = 12

      if not boss.impact then
        -- 第一帧冲击
        if boss.jump then boss.jump = false end

        -- 冲刺 (C# rigidbody.AddForce(forward * dash))
        local dash_val = boss.dash[boss.currentAtk + 1] or 0
        if dash_val > 0 then
          local fx = math.sin(math.rad(boss.yaw))
          local fz = -math.cos(math.rad(boss.yaw))
          boss.x = boss.x + fx * dash_val * 0.001 * dt
          boss.z = boss.z + fz * dash_val * 0.001 * dt
        end
        boss.impact = true
        boss.m_atk_delay = 0

        -- 对玩家造成伤害 (C# 武器碰撞体 OnTriggerEnter)
        local dmg_dist = dist2d(boss.x, boss.z, Player.x, Player.z)
        boss._impact_hit_player = (dmg_dist < 3.0)
        if boss._impact_hit_player then
          PlayerDamaged(boss.power[boss.currentAtk + 1] or 30, boss.x, boss.z)
        end

        -- 武器实体生成 (C# clone_weapon + WeaponDamage.PressDamage)
        BossWeaponActivate(boss, boss.currentAtk)

        -- 武器特效
        local aef = boss.attach_ef[boss.currentAtk + 1] or 0
        if aef >= 1 then
          spawn_hit_effect(boss.x + boss.direction_x * 1.5, 2.0, boss.z + boss.direction_z * 1.5, 10, "skill")
          if aef == 2 then
            -- C# Impact2: 第二武器 PressDamage + Hitcam
            BossSecondWeaponImpact(boss)
            spawn_swing_ef(boss.x, 0.1, boss.z, boss.yaw, 2.0, 0.3, "skill")
          end
        end
        if S.slash then dse.audio.play_sfx(S.slash, 0.6, 0) end
        CameraHitcam()
      end

      -- 冲击阶段持续时间
      local spd_i = boss.speed_b_attack_i[boss.currentAtk + 1] or 0.2
      local impact_dur = 0.3 / (1 + spd_i)
      if boss.anim_timer > impact_dur then
        boss.anim_phase = "idle"
        boss.anim_timer = 0
        boss.attackstart = false
        boss.monmovestat = 0
        BossSecondWeaponHide(boss)
      end

    elseif boss.anim_phase == "move" then
      -- 移动 (C# b_move)
      boss.attackstart = false
      boss.monmovestat = 1
      boss.x = boss.x + boss.direction_x * dt * boss.runspeed * 8
      boss.z = boss.z + boss.direction_z * dt * boss.runspeed * 8

      -- 转向
      if boss.direction_x ~= 0 or boss.direction_z ~= 0 then
        local target_yaw = math.deg(atan2(boss.direction_x, -boss.direction_z))
        boss.yaw = lerp(boss.yaw, target_yaw, clamp(dt * 30, 0, 1))
      end

    else
      -- 待机
      boss.monmovestat = 0
      boss.attackstart = false
      boss.anim_phase = "idle"
    end
  end

  -- ===== 定期调用 SetDir (C# InvokeRepeating 0.5s) =====
  boss.setdir_timer = boss.setdir_timer - dt
  if boss.setdir_timer <= 0 then
    boss.setdir_timer = 0.5
    BossSetDir(boss)
  end

  -- ===== 指示箭头 (C# showme) =====
  if boss.showme and boss.arrow_e then
    local avx, avz = boss.x - Player.x, boss.z - Player.z
    local avm = math.sqrt(avx * avx + avz * avz)
    if avm > 0.001 then avx, avz = avx / avm, avz / avm end
    local arrow_yaw = math.deg(atan2(avx, -avz))
    local target_x = Player.x + avx * 0.3
    local target_z = Player.z + avz * 0.3
    dse.ecs.set_transform_position(boss.arrow_e, target_x, 0.02, target_z)
    dse.ecs.set_transform_rotation(boss.arrow_e, 0, arrow_yaw, 0)
    dse.ecs.set_mesh_visible(boss.arrow_e, true)
  elseif boss.arrow_e then
    dse.ecs.set_mesh_visible(boss.arrow_e, false)
  end

  -- ===== 毒伤害 (C# poison) =====
  if boss.poison then
    boss.poison_delay = boss.poison_delay - dt
    if boss.poison_delay < 0 then
      boss.poison = false
      if boss.e then dse.ecs.set_mesh_color(boss.e, 1.0, 1.0, 1.0, 1.0) end
    else
      local cur_delay = math.floor(boss.poison_delay)
      if cur_delay ~= boss.old_delay then
        boss.hp = boss.hp - math.floor(boss.poison_damage)
        boss.old_delay = cur_delay
        spawn_damage_text(boss.x, 3.0, boss.z, tostring(math.floor(boss.poison_damage)), 0.5, 1.0, 0.2, 20)
        if boss.e then dse.ecs.set_mesh_color(boss.e, 0.5, 1.0, 0.2, 1.0) end
        if boss.hp <= 0 then BossDead(boss, 1); return end
      end
    end
  end

  -- ===== 燃烧伤害 (att_status == 1) =====
  if boss.burn_timer > 0 then
    boss.burn_damage_timer = boss.burn_damage_timer - dt
    if boss.burn_damage_timer <= 0 then
      boss.burn_damage_timer = 1.0
      local burn_dmg = math.max(1, math.floor(Player.atk / 10))
      boss.hp = boss.hp - burn_dmg
      spawn_damage_text(boss.x, 3.0, boss.z, tostring(burn_dmg), 1.0, 0.4, 0.1, 18)
      if boss.hp <= 0 then BossDead(boss, 0); return end
    end
    if boss.burn_timer <= 0 and boss.att_status == 1 then
      boss.att_status = 1
      if boss.e then dse.ecs.set_mesh_color(boss.e, 1.0, 1.0, 1.0, 1.0) end
    end
  end

  -- ===== 区域限制 (C# SqrMagnitude > restrictArea) =====
  local sqmag = boss.x * boss.x + boss.z * boss.z
  if sqmag > boss.restrict_area then
    boss.x = boss.x * (1 - 0.02 * dt)
    boss.z = boss.z * (1 - 0.02 * dt)
  end

  -- 边界
  boss.x = clamp(boss.x, -28, 28)
  boss.z = clamp(boss.z, -28, 28)

  -- ===== 击退 =====
  if boss.knockback_timer > 0 then
    boss.x = boss.x + boss.knockback_x * dt
    boss.z = boss.z + boss.knockback_z * dt
  end

  -- ===== 更新实体 =====
  -- 血条跟随 (C# Hp_bar.Update: parentmon.position + (0, posY, -0.02))
  UpdateHpBar(boss.hp_bar_e, boss.x, 4.2, boss.z, boss.maxhp > 0 and (boss.hp / boss.maxhp) or 0)
  -- 影子跟随
  if boss.shadow then
    dse.ecs.set_transform_position(boss.shadow.e, boss.x, 0.03, boss.z)
    local ss = 2.0 * 1.4
    dse.ecs.set_transform_scale(boss.shadow.e, ss, 1, ss)
  end
  if boss.e then
    dse.ecs.set_transform_position(boss.e, boss.x, boss.y, boss.z)
    dse.ecs.set_transform_rotation(boss.e, 0, boss.yaw, 0)

    -- 程序化动画
    local base_scale = 2.0
    if boss.sizekind == 24 then base_scale = 2.5 end
    local sx, sy, sz = base_scale, base_scale, base_scale
    local r, g, b, a = 1.0, 1.0, 1.0, 1.0

    -- 异常状态颜色
    if boss.att_status == 2 and boss.freeze_timer > 0 then
      r = 0.6; g = 0.6; b = 0.6
    elseif boss.burn_timer > 0 then
      r = 1.0; g = 0.5; b = 0.2
    elseif boss.poison then
      r = 0.5; g = 1.0; b = 0.2
    end

    -- 动画变形
    if boss.anim_phase == "attack" then
      local pulse = 1.0 + 0.15 * math.sin(boss.t * 20)
      sx = sx * pulse
    elseif boss.anim_phase == "attack_i" then
      local pulse = 1.0 + 0.25 * math.sin(boss.t * 30)
      sx = sx * pulse; sz = sz * pulse
    elseif boss.anim_phase == "move" then
      local pulse = 1.0 + 0.08 * math.sin(boss.t * 12)
      sx = sx * pulse
    elseif boss.anim_phase == "down" then
      local prog = boss.anim_timer / 0.5
      sy = sy * (1.0 - prog * 0.5)
      sx = sx * (1.0 + prog * 0.3)
    end

    -- 受击闪烁
    if boss.hit_flash > 0 then
      r = 1.0; g = 0.3; b = 0.3
    end

    -- 入场特写
    if boss.anim_phase == "trans" then
      local pulse = 1.0 + 0.15 * math.sin(boss.t * 8)
      sx, sy, sz = base_scale * pulse, base_scale * pulse, base_scale * pulse
      r = 1.0; g = 0.8; b = 0.2
    end

    dse.ecs.set_transform_scale(boss.e, sx, sy, sz)
    dse.ecs.set_mesh_color(boss.e, r, g, b, a)
  end
end

-- ── 子弹系统回调注入 ──────────────────────────────────────────────
BulletSystem.on_hit_enemy = function(en, b)
  EnemyDamaged(en, b.damage, b.x, b.z, b.attack_type)
end
BulletSystem.on_hit_boss = function(boss, b)
  BossDamaged(boss, b.damage, b.x, b.z, b.attack_type)
end
BulletSystem.on_hit_player = function(b)
  -- 玩家受伤 (简化)
  if Player.life and Player.invuln <= 0 then
    Player.hp = Player.hp - (b.damage or 10)
    Player.invuln = 1.0
    if Player.hp <= 0 then
      Player.hp = 0; Player.life = false
    end
  end
end
BulletSystem.on_splash = function(x, z)
  spawn_hit_effect(x, 0.1, z, 0.8, "boom")
end
BulletSystem.on_angel_finish = function()
  -- 天使攻击完成回调 (AI_Asist)
end

-- ── 技能系统回调注入 ──────────────────────────────────────────────
SkillSystem.on_hit_enemy = function(en, s)
  EnemyDamaged(en, s.damage, s.x, s.z, s.attack_type)
end
SkillSystem.on_hit_boss = function(boss, s)
  BossDamaged(boss, s.damage, s.x, s.z, s.attack_type)
end
SkillSystem.on_invincibility = function(t)
  Invincibility(t)
end
SkillSystem.on_attack_up = function(factor)
  AttakUp(factor)
end
SkillSystem.on_defence_up = function()
  Player.defence_up_factor = 1.5
end
SkillSystem.on_reset_atk = function()
  ResetAtk()
end
SkillSystem.on_reset_def = function()
  Player.defence_up_factor = 1.0
end
SkillSystem.on_stop_control = function()
  Player.chamovestat = 200
end
SkillSystem.on_start_control = function()
  Player.chamovestat = 0
end
SkillSystem.on_disappear = function()
  if G.player_e then dse.ecs.set_mesh_visible(G.player_e, false) end
end
SkillSystem.on_appear = function()
  if G.player_e then dse.ecs.set_mesh_visible(G.player_e, true) end
end
SkillSystem.on_skill_start = function()
  -- 施法开始 hook (预留)
end
SkillSystem.on_camera_zoom = function(zoomspeed, fov, delay)
  CameraZoomIn(zoomspeed, fov, delay)
end
SkillSystem.on_camera_hitcam = function(intensity)
  CameraHitcam2(intensity)
end
SkillSystem.on_camera_look = function(entity, angle, time)
  CameraLookTarget(entity, angle, time)
end
SkillSystem.on_spawn_effect = function(x, y, z, count, kind)
  spawn_hit_effect(x, y, z, count, kind)
end
SkillSystem.on_spawn_swing = function(x, y, z, yaw, scale, dur, kind)
  spawn_swing_ef(x, y, z, yaw, scale, dur, kind)
end
SkillSystem.on_play_anim = function(name)
  Player.visual_state = name
  Player.visual_duration = 0.6
end
SkillSystem.on_stop_anim = function()
  Player.visual_state = "idle"
end
SkillSystem.on_play_audio = function(name)
  local snd = S[name]
  if snd then dse.audio.play_sfx(snd, 0.8, 0) end
end
SkillSystem.on_boom = function(boom_type, x, z, collider)
  spawn_hit_effect(x, 0.1, z, 1, "boom")
  if S.boom then dse.audio.play_sfx(S.boom, 0.8, 0) end
end
SkillSystem.on_sp_charge = function(amount)
  Spcharge(amount)
end

-- ── AI 系统回调注入 (武将 + 天使) ──────────────────────────────
AISystem.on_general_dead = function()
  -- 武将死亡 → 延迟恢复玩家属性
  Player._general_off_timer = 2.0
end
AISystem.on_general_attack = function(x, z, yaw, dmg)
  -- 武将普通攻击命中附近敌人
  for _, en in ipairs(Entities.enemies) do
    if not en.dead and en.monmovestat and en.monmovestat > 0 then
      if dist2d(x, z, en.x, en.z) < 2.0 then
        EnemyDamaged(en, dmg, x, z, "normal")
      end
    end
  end
  if Entities.boss and not Entities.boss.dead then
    if dist2d(x, z, Entities.boss.x, Entities.boss.z) < 2.5 then
      BossDamaged(Entities.boss, dmg, x, z, "normal")
    end
  end
  spawn_swing_ef(x, 1.0, z, yaw, 1.5, 0.3)
end
AISystem.on_general_special = function(x, z, yaw, dmg)
  -- 武将特殊攻击 (范围更大, 伤害更高)
  spawn_hit_effect(x, 0.5, z, 10, "skill")
  for _, en in ipairs(Entities.enemies) do
    if not en.dead and en.monmovestat and en.monmovestat > 0 then
      if dist2d(x, z, en.x, en.z) < 3.0 then
        EnemyDamaged(en, dmg, x, z, "strong")
      end
    end
  end
  if Entities.boss and not Entities.boss.dead then
    if dist2d(x, z, Entities.boss.x, Entities.boss.z) < 3.5 then
      BossDamaged(Entities.boss, dmg, x, z, "strong")
    end
  end
  spawn_swing_ef(x, 0.1, z, yaw, 2.5, 0.4, "skill")
  CameraHitcam2(1.5)
end
AISystem.on_general_hitcam = function()
  CameraHitcam()
end
AISystem.on_angel_shoot = function(ax, ay, az, tx, tz)
  -- 天使发射箭矢 (委托给 BulletSystem)
  local dx, dz = tx - ax, tz - az
  local yaw = (dx == 0 and dz == 0) and 0 or math.deg(atan2(dx, -dz))
  BulletSystem.spawn({
    type = "arrow", x = ax, y = ay, z = az, yaw = yaw,
    speed = 12, damage = Player.atk * 0.5, attack_type = "arrow",
    life = 2.0, r = 0.6, g = 0.8, b = 1.0,
  })
  if S.slash then dse.audio.play_sfx(S.slash, 0.4, 0) end
end
AISystem.on_angel_splash = function(tx, tz)
  -- 天使箭矢溅射特效
  spawn_hit_effect(tx, 0.3, tz, 5, "skill")
end

-- 子弹/投射物更新 (委托给 BulletSystem)
local function UpdateBullets(dt)
  BulletSystem.update(dt)
end

-- ============================================================================
-- 刷怪系统 (Spawn 完整移植)
-- ============================================================================

local function GetStageData()
  return DB.DB_Stage[G.stage_index % 90]
end

local function BuildStage()
  ClearLevel()

  local stg = GetStageData()
  if not stg then return end

  -- 特殊关卡类型 + 目标实体 (C# Spawn.play_kind 6/7)
  G.play_kind = GetPlayKind(G.stage_index)
  BuildObjective()

  -- 地面
  local map_idx = math.floor(G.stage_index / 10) % 5
  local map_model = MAP_MODELS[map_idx + 1] or MAP_MODELS[1]
  Entities.ground = spawn_ground_plane(2.0, 0.3, 0.35, 0.25)

  -- 地图装饰 (原版 map + 天空地面贴图)
  local map_tex = TEX.map and TEX.map[map_idx + 1]
  local map_e = spawn_model(model_path(map_model), 0, 0, 0, 2.0, 2.0, 2.0, map_tex)
  table.insert(Entities.decor, map_e)

  -- 建筑 (原版贴图)
  local struct_tex_map = {
    barrack = TEX.barrack, tower = TEX.tower, barricade = nil,
    basecamp = TEX.basecamp, tank = TEX.tank, cart = TEX.cart,
  }
  for i = 1, 3 do
    local angle = (i / 3) * math.pi * 2
    local bx = math.cos(angle) * 15
    local bz = math.sin(angle) * 15
    local struct_type = (i == 1) and "barrack" or (i == 2) and "tower" or "barricade"
    local se = spawn_model(model_path(STRUCT_MODELS[struct_type]), bx, 0, bz, 1, 1, 1, struct_tex_map[struct_type])
    table.insert(Entities.structures, {e = se, x = bx, z = bz, type = struct_type})
  end

  -- 玩家 (原版 cha01_01 + costume01)
  local pe = spawn_model(model_path(CHA_MODELS[1]), 0, 0, 0, 1, 1, 1, TEX.costume and TEX.costume[1])
  G.player_e = pe

  -- 重置玩家
  Player.x = 0; Player.z = 0; Player.y = 0; Player.yaw = 0
  Player.hp = Player.maxhp
  Player.sp = Player.maxsp
  Player.life = true
  Player.isplaycha = true
  Player.chamovestat = 0
  Player.visual_state = "idle"
  Player.attacking = 0
  Player.combotime = 0
  Player.curruntattack = Player.attackkind_factor

  -- 初始怪物
  local basemon1 = stg.basemon1 or 0
  local basemon2 = stg.basemon2 or 1
  local mainmon = stg.mainmon or 2
  G.wave = 1
  G.enemykill = 0
  G.enemies_alive = 0

  -- 首批怪物由 UpdateSpawn regen==0 生成 (C# Spawn.Update 首批 3 只, 此处不再预刷避免重复)

  -- Boss
  if stg.boss1 >= 0 then
    local boss = CreateBoss(stg.boss1, 0, -15)
    if boss then
      Entities.boss = boss
      G.boss_active = true
      G.boss_intro_timer = 2.0
      if S.bgm_boss then dse.audio.play_bgm(S.bgm_boss, 0.7, true) end
    end
  end

  -- 刷怪点
  for i = 1, 4 do
    local angle = (i / 4) * math.pi * 2
    table.insert(Entities.spawn_points, {
      x = math.cos(angle) * 12,
      z = math.sin(angle) * 12,
    })
  end

  G.spawn_timer = 3.0
  G.spawn_interval = 2.0

  -- Spawn 系统状态初始化 (C# Spawn Update)
  G.spawn_regen = 0
  G.spawn_total = stg and (stg.stagenum * 5) or 40
  G.spawn_max = 15
  G.wave = 1
  G.finalstage = stg and stg.stagenum or 3
  G.countdown = false
  G.stagefinish = false
  G.boss_index = 0

  print(string.format("[topdown_3d] Stage %d built — enemies: %d, boss: %s",
    G.stage_index, #Entities.enemies, Entities.boss and "yes" or "no"))
end

-- Spawn 系统辅助函数 (C# Spawn.cs)

-- 随机刷怪点 (C# SetRndPoint)
local function SetRndPoint()
  if #Entities.spawn_points == 0 then return {x=10, z=0} end
  local rndoldpoint = G.spawn_rndoldpoint or -1
  local rndpoint = math.random(1, #Entities.spawn_points)
  if rndpoint == rndoldpoint then
    rndpoint = rndpoint % #Entities.spawn_points + 1
  end
  G.spawn_rndoldpoint = rndpoint
  return Entities.spawn_points[rndpoint]
end

-- Boss 出现 (C# BossAppear)
local function BossAppear(boss_kind)
  local sp = SetRndPoint()
  local boss = CreateBoss(boss_kind, sp.x, sp.z)
  if boss then
    BossSetLevel(boss, G.stage_index, 625)
    Entities.boss = boss
    G.boss_active = true
    G.boss_intro_timer = 3.0
    if S.bgm_boss then dse.audio.play_bgm(S.bgm_boss, 0.8, true) end
    spawn_damage_text(boss.x, 5.0, boss.z, "BOSS!", 1.0, 0.2, 0.2, 32)
  end
end

-- Boss 召唤小怪 (C# Summon)
local function BossSummon(amount, summonpos)
  G.summon_amount = amount
  G.summonpos = summonpos
  G.summon_timer = 0.1
end

local function UpdateBossSummon(dt)
  if not G.summon_amount or G.summon_amount <= 0 then return end
  G.summon_timer = G.summon_timer - dt
  if G.summon_timer <= 0 then
    G.summon_timer = 0.5
    local alive_count = 0
    for _, en in ipairs(Entities.enemies) do
      if not en.dead then alive_count = alive_count + 1 end
    end
    if alive_count > 0 then
      local angle = math.random() * math.pi * 2
      local sx = G.summonpos.x + math.cos(angle) * 3
      local sz = G.summonpos.z + math.sin(angle) * 3
      local stg = GetStageData()
      local basemon1 = stg.basemon1 or 0
      local basemon2 = stg.basemon2 or 1
      local mainmon = stg.mainmon or 2
      local ekind = math.random(0, 2)
      if ekind == 0 then ekind = basemon1
      elseif ekind == 1 then ekind = basemon2
      else ekind = mainmon end
      local en = CreateEnemy(ekind, sx, sz)
      if en then
        EnemySetLevel(en, G.stage_index, G.play_kind, true, 625)
        en.behaviour_delay = 0.5
        table.insert(Entities.enemies, en)
        spawn_hit_effect(sx, 0.5, sz, 5, "fire")
      end
      G.summon_amount = G.summon_amount - 1
    end
    if G.summon_amount <= 0 then
      G.summon_amount = 0
    end
  end
end

local function UpdateSpawn(dt)
  if G.mode ~= "play" then return end

  -- Boss 召唤更新
  UpdateBossSummon(dt)

  -- 计算存活敌人数量
  local alive_count = 0
  for _, en in ipairs(Entities.enemies) do
    if not en.dead then alive_count = alive_count + 1 end
  end

  -- Boss 存在时不刷怪
  if Entities.boss and not Entities.boss.dead then
    -- Boss 战中的小怪刷新 (C# Update regen > 0 && totalEnemyNum > 0)
    if G.spawn_regen == 1 and G.spawn_total > 0 then
      G.spawn_timer = G.spawn_timer - dt
      if G.spawn_timer <= 0 then
        if alive_count < G.spawn_max and G.spawn_total > 0 then
          G.spawn_timer = G.spawn_interval
          local stg = GetStageData()
          local basemon1 = stg.basemon1 or 0
          local basemon2 = stg.basemon2 or 1
          local mainmon = stg.mainmon or 2
          local ekind = math.random(0, 100)
          if ekind < 45 then ekind = basemon1
          elseif ekind < 80 then ekind = basemon2
          else ekind = mainmon end
          local sp = SetRndPoint()
          local en = CreateEnemy(ekind, sp.x, sp.z)
          if en then
            EnemySetLevel(en, G.stage_index, G.play_kind, false, 625)
            en.behaviour_delay = 0.5
            table.insert(Entities.enemies, en)
            G.spawn_total = G.spawn_total - 1
          end
        else
          if G.spawn_total <= 0 then
            G.spawn_regen = -2
          end
        end
      end
    end
    return
  end

  -- 波次刷怪状态机 (C# Update regen)
  if G.spawn_regen == 0 then
    -- 初始刷怪 (C# regen == 0)
    local stg = GetStageData()
    local basemon1 = stg.basemon1 or 0
    local basemon2 = stg.basemon2 or 1
    local mainmon = stg.mainmon or 2
    local enemyset = {basemon1, basemon2, mainmon}
    for i = 1, 3 do
      local sp = SetRndPoint()
      local en = CreateEnemy(enemyset[i], sp.x, sp.z)
      if en then
        EnemySetLevel(en, G.stage_index, G.play_kind, false, 625)
        en.behaviour_delay = 0.5
        table.insert(Entities.enemies, en)
        G.spawn_total = G.spawn_total - 1
      end
    end
    G.spawn_regen = 1
    G.spawn_timer = G.spawn_interval
    G.countdown = false

  elseif G.spawn_regen > 0 then
    -- 持续刷怪 (C# regen > 0)
    G.spawn_timer = G.spawn_timer - dt
    if G.spawn_timer <= 0 then
      if G.spawn_total > 0 then
        if alive_count < G.spawn_max then
          local stg = GetStageData()
          local basemon1 = stg.basemon1 or 0
          local basemon2 = stg.basemon2 or 1
          local mainmon = stg.mainmon or 2
          local ekind = math.random(0, 100)
          if ekind < 45 then ekind = basemon1
          elseif ekind < 80 then ekind = basemon2
          else ekind = mainmon end
          local sp = SetRndPoint()
          local en = CreateEnemy(ekind, sp.x, sp.z)
          if en then
            EnemySetLevel(en, G.stage_index, G.play_kind, false, 625)
            en.behaviour_delay = 0.5
            table.insert(Entities.enemies, en)
            G.spawn_total = G.spawn_total - 1
          end
          G.spawn_timer = G.spawn_interval
        end
      else
        -- G.spawn_total <= 0
        if G.wave >= G.finalstage then
          -- 最后一波，生成 Boss
          local stg = GetStageData()
          if stg and stg.bosscount > 0 and G.boss_index < 3 then
            local bosskind = stg.boss1
            if G.boss_index == 1 then bosskind = stg.boss2 or stg.boss1
            elseif G.boss_index == 2 then bosskind = stg.boss3 or stg.boss1 end
            if bosskind and bosskind >= 0 then
              BossAppear(bosskind)
              G.boss_index = G.boss_index + 1
              if G.boss_index >= (stg.bosscount or 1) then
                G.spawn_regen = -2
              end
            else
              G.spawn_regen = -2
            end
          else
            G.spawn_regen = -2
          end
        else
          -- 非最后一波，等待所有敌人死亡
          if G.spawn_total <= 0 then
            G.spawn_regen = -2
          end
        end
      end
    end

  elseif G.spawn_regen == -2 then
    -- 等待所有敌人死亡
    if alive_count <= 0 then
      G.countdown = false
      if G.spawn_total <= 0 and not G.stagefinish then
        G.stagefinish = true
        G.wave = G.wave + 1
        if G.wave >= G.finalstage then
          G.mode = "level_complete"
          G.level_complete_timer = 0
        else
          -- 进入下一波
          G.spawn_regen = 0
          local stg = GetStageData()
          G.spawn_total = stg and (stg.stagenum * 5) or 40
          G.spawn_max = 15
          G.spawn_interval = math.max(0.4, 1.0 - G.wave * 0.05)
          G.spawn_timer = 3.0
        end
      end
    end
  end

  -- 倒计时提示 (C# monnum <= 5 && !countdown)
  if G.spawn_regen == -2 and alive_count <= 5 and not G.countdown then
    for _, en in ipairs(Entities.enemies) do
      if not en.dead then
        spawn_damage_text(en.x, 2.5, en.z, "!", 1.0, 1.0, 0.3, 20)
      end
    end
    G.countdown = true
  end
end

-- ============================================================================
-- UI 系统 (UI_Ingame 完整移植)
-- ============================================================================
-- 纯文本 HUD 已废弃：由 ui_system.lua 完整 HUD（gauge 条 + 文本）取代

-- ============================================================================
-- 攻击碰撞检测 (代替 OnTriggerEnter)
-- ============================================================================
local function UpdateCombat(dt)
  -- 玩家攻击命中检测
  if Player.attacking > 0 and Player.attacking > Player.visual_duration * 0.3
     and Player.attacking < Player.visual_duration * 0.8 then
    -- 攻击有效窗口
    if not Player._hit_done then
      local attack_range = 3.0
      -- 武器范围修正
      if Player.weapon_kind == 2 then attack_range = 4.0 -- 长枪
      elseif Player.weapon_kind == 3 then attack_range = 6.0 -- 弓
      elseif Player.weapon_kind == 4 then attack_range = 3.5 -- 法杖
      end

      local hit_count = 0
      for _, en in ipairs(Entities.enemies) do
        if not en.dead and not en._hit_this_attack then
          local d = dist2d(Player.x, Player.z, en.x, en.z)
          if d <= attack_range then
            -- 检查角度
            local dx, dz = en.x - Player.x, en.z - Player.z
            local angle = math.deg(atan2(dx, -dz))
            local diff = ((angle - Player.yaw + 180) % 360) - 180
            if math.abs(diff) < 90 then
              -- 命中
              local attack_type = "normal"
              if Player.special_kind == 0 then attack_type = "fire"
              elseif Player.special_kind == 1 then attack_type = "ice"
              elseif Player.special_kind == 2 then attack_type = "electric"
              elseif Player.special_kind == 3 then attack_type = "poison"
              end

              local dmg = Player.atk
              -- C# downhigh = script_cha.attack_rising (在 EnemyDamaged 内部处理)
              EnemyDamaged(en, dmg, Player.x, Player.z, attack_type)
              en._hit_this_attack = true
              hit_count = hit_count + 1

              -- 摄像机震动 (C# script_cam.Hitcam)
              CameraHitcam()
            end
          end
        end
      end

      -- Boss 命中
      if Entities.boss and not Entities.boss.dead and not Entities.boss._hit_this_attack then
        local d = dist2d(Player.x, Player.z, Entities.boss.x, Entities.boss.z)
        if d <= attack_range + 1.0 then
          local dx, dz = Entities.boss.x - Player.x, Entities.boss.z - Player.z
          local angle = math.deg(atan2(dx, -dz))
          local diff = ((angle - Player.yaw + 180) % 360) - 180
          if math.abs(diff) < 100 then
            BossDamaged(Entities.boss, Player.atk, Player.x, Player.z, attack_type)
            Entities.boss._hit_this_attack = true
          end
        end
      end

      if hit_count > 0 then
        -- 命中音效
        if S.slash1 then dse.audio.play_sfx(S.slash1, 0.6, 0) end
      end
      Player._hit_done = true
    end
  else
    -- 攻击窗口外重置命中标记
    if Player.attacking <= 0 then
      Player._hit_done = false
      for _, en in ipairs(Entities.enemies) do
        en._hit_this_attack = false
      end
      if Entities.boss then
        Entities.boss._hit_this_attack = false
      end
    end
  end

  -- 玩家攻击时的连续命中 (技能/范围攻击)
  if Player.skill_timer > 0 then
    -- 技能持续期间持续检测
  end
end

-- ============================================================================
-- 关卡流程
-- ============================================================================
-- 通关结算: 记录星级/奖励/解锁, 返回地图 (C# UI_map + UI_result)
local function OnStageCleared()
  local stars, getcoin, getexp =
    MenuSystem.record_stage_clear(G.stage_index, Player.maxhp > 0 and Player.hp / Player.maxhp or 1.0)
  GainExp(getexp)
  SaveSystem.save_all()
  if G.stage_index + 1 >= 90 then
    -- 全部通关 (C# UI_map: max_stage_index >= 90 → Ending)
    G.mode = "win"
    if S.bgm_victory then dse.audio.play_bgm(S.bgm_victory, 0.6, false) end
  else
    UISystem.clear()
    MenuSystem.show_map()
    G.mode = "map"
  end
end

-- 开始新游戏 (C# UI_intro.InitStat + jumpSence): 清空存档从第 0 关开始
local function RestartGame()
  MenuSystem.clear_all()
  UISystem.build()
  G.stage_index = 0
  G.lives = 3
  G.score = 0
  G.coin = 0
  G.soul = 1
  G.jade = 0
  G.combo = 0
  G.combo_max = 0
  G.totalkill = 0
  G.stage_clear = {}
  G.max_stage_index = 0
  Player.level = 1
  Player.exp = 0
  Player.maxhp = 100
  Player.hp = 100
  Player.maxsp = 100
  Player.sp = 100
  Player.weapon_kind = 0
  Player.skill_grades = {}
  ChangeCharacter(0)
  ResetPower()
  PetSystem.clear()
  -- 开始新游戏: 覆盖为新档 (C# ConvertSaveData.ConvertData)
  SaveSystem.save_all()
  -- 新游戏先播第 0 关剧情 (C# UI_intro → Story)
  if ScenarioSystem.has_scene(0) then
    UISystem.clear()
    ScenarioSystem.on_finish = function()
      UISystem.build()
      BuildStage()
      G.mode = "play"
      if S.bgm_stage1 then dse.audio.play_bgm(S.bgm_stage1, 0.6, true) end
    end
    ScenarioSystem.start(0)
    G.mode = "story"
  else
    BuildStage()
    G.mode = "play"
    if S.bgm_stage1 then dse.audio.play_bgm(S.bgm_stage1, 0.6, true) end
  end
end

-- 从地图进入指定关卡 (C# UI_map 选关 → Loading)
local function StartStageInternal(stage_idx)
  MenuSystem.clear_all()
  UISystem.build()
  G.stage_index = stage_idx
  ResetPower()
  Player.maxhp = 95 + Player.level * 5
  Player.hp = Player.maxhp
  Player.sp = Player.maxsp
  Player.life = true
  PetSystem.clear()
  BuildStage()
  G.mode = "play"
  local bgm = {S.bgm_stage1, S.bgm_stage2, S.bgm_stage3}
  local bgm_idx = (G.stage_index % 3) + 1
  if bgm[bgm_idx] then dse.audio.play_bgm(bgm[bgm_idx], 0.6, true) end
end

-- 从地图进入关卡: 首次进入先播剧情 (C# UI_map → Story 场景)
local function StartStage(stage_idx)
  local cleared = (G.stage_clear and G.stage_clear[stage_idx]) or 0
  if cleared <= 0 and ScenarioSystem.has_scene(stage_idx) then
    ScenarioSystem.on_finish = function()
      StartStageInternal(stage_idx)
    end
    ScenarioSystem.start(stage_idx)
    G.mode = "story"
  else
    StartStageInternal(stage_idx)
  end
end

-- ============================================================================
-- 主流程
-- ============================================================================
function Awake()
  -- 依赖注入 (WeaponDamage 需要 main.lua 的 PlayerDamaged)
  WeaponDamage.set_damage_player(PlayerDamaged)

  LoadAudio()
  LoadTextures()

  -- 加载字体纹理
  G._font_tex = dse.assets.load_texture(resolve_path("assets/font/bitmap_font.png"))

  -- 3D 相机
  local cam = dse.ecs.create_entity()
  dse.ecs.add_transform(cam, 0, 18, 8, 1, 1, 1)
  dse.ecs.add_camera_3d(cam, 55, 0)
  G.cam = cam

  -- 方向光
  local light = dse.ecs.create_entity()
  dse.ecs.add_transform(light, 0, 20, 0, 1, 1, 1)
  dse.ecs.add_directional_light_3d(light, 0.5, -0.8, 0.3, 1.0, 0.95, 0.85, 1.5, 0.3, 0.0)

  -- 环境光
  local ambient = dse.ecs.create_entity()
  dse.ecs.add_transform(ambient, 0, 15, 0, 1, 1, 1)
  dse.ecs.add_point_light_3d(ambient, 0.4, 0.4, 0.5, 0.5, 30.0)

  -- HUD (ui_system 完整 HUD: gauge 条 + 文本)
  UISystem.build()

  -- UI 回调注入
  UISystem.on_pause = function() if S.click then dse.audio.play_sfx(S.click, 0.5, 0) end end
  UISystem.on_resume = function() if S.click then dse.audio.play_sfx(S.click, 0.5, 0) end end
  -- 暂停菜单退出 → 回主菜单 (C# UI_intro)
  UISystem.on_quit = function()
    UISystem.clear()
    MenuSystem.show_intro()
    G.mode = "menu"
    if S.bgm_intro then dse.audio.play_bgm(S.bgm_intro, 0.6, true) end
  end
  -- 结算界面 R 键 (game_over → 回地图)
  UISystem.on_restart = function()
    UISystem.clear()
    MenuSystem.show_map()
    G.mode = "map"
  end
  UISystem.on_stage_continue = function() OnStageCleared() end
  UISystem.on_revive = function()
    Player.hp = Player.maxhp
    Player.life = true
    G.mode = "play"
    Player.invuln = 3.0
  end
  -- 放弃复活/超时 → 回地图
  UISystem.on_chance_fail = function()
    UISystem.clear()
    MenuSystem.show_map()
    G.mode = "map"
    if S.bgm_intro then dse.audio.play_bgm(S.bgm_intro, 0.6, true) end
  end
  UISystem.on_time_up = function()
    UISystem.clear()
    MenuSystem.show_map()
    G.mode = "map"
    if S.bgm_fail then dse.audio.play_sfx(S.bgm_fail, 0.6, 0) end
    if S.bgm_intro then dse.audio.play_bgm(S.bgm_intro, 0.6, true) end
  end
  UISystem.on_sp_charge = function(amount)
    Player.sp = math.min(Player.maxsp, Player.sp + amount)
  end
  UISystem.on_super_mode = function()
    Player.isinvincibility = true
    Player.target_invincibility = 5.0
  end
  UISystem.on_power_release = function()
    -- 蓄力释放: 触发 Eximpact
    Player.attack_rising = true
    Player.attacking = 0.01
    Player.visual_state = "exattack"
    Player.visual_duration = 0.6
    Player.excharging = false
    -- 追击 QTE 窗口 (C# attackex1: Eximpact 后 0.5-0.7s 按键追击)
    Player.qte_timer = 0.0001
    Player.qte_active = false
  end
  UISystem.on_fov_change = function(fov)
    if G.cam then
      local new_fov = 55 - fov * 2
      dse.ecs.set_camera_fov(G.cam, new_fov)
    end
  end

  -- ── 宠物系统回调注入 ──────────────────────────────────────────────
  -- 猎鹰攻击敌人 (C# Pet_eagle → EnemyDamaged)
  PetSystem.on_eagle_attack = function(en, dmg)
    EnemyDamaged(en, dmg, Player.x, Player.z, "normal")
    spawn_hit_effect(en.x, 1.5, en.z, 3, "skill")
  end
  -- 骑乘攻击音效
  PetSystem.on_ride_attack = function(is_right)
    if S.slash1 then dse.audio.play_sfx(S.slash1, 0.6, 0) end
  end
  -- 骑乘击杀敌人
  PetSystem.on_ride_kill_enemy = function(x, z)
    spawn_hit_effect(x, 0.5, z, 3, "skill")
    if S.mon_die then dse.audio.play_sfx(S.mon_die, 0.6, 0) end
  end
  -- 骑乘拾取魂石
  PetSystem.on_ride_getcoin = function(x, z)
    spawn_hit_effect(x, 1.0, z, 2, "coin")
    if S.coin then dse.audio.play_sfx(S.coin, 0.5, 0) end
  end
  -- 骑乘受伤
  PetSystem.on_ride_behit = function()
    CameraHitcam2(1.5)
    if S.hurt then dse.audio.play_sfx(S.hurt, 0.5, 0) end
  end
  -- 骑乘跳跃
  PetSystem.on_ride_jump = function()
    -- 预留音效
  end
  -- 骑乘完成
  PetSystem.on_ride_finish = function()
    G.mode = "level_complete"
    G.level_complete_timer = 0
  end
  -- 战马骑乘触发
  PetSystem.on_horse_ride = function()
    if S.horse then dse.audio.play_sfx(S.horse, 0.8, 0) end
  end
  -- 天使命中
  PetSystem.on_angel_hit = function(en, dmg, splashEF)
    if en == Entities.boss then
      BossDamaged(en, dmg, en.x, en.z, "arrow")
    else
      EnemyDamaged(en, dmg, en.x, en.z, "arrow")
    end
    spawn_hit_effect(en.x, 1.0, en.z, 3, "skill")
  end
  -- 天使射击音效
  PetSystem.on_angel_fire = function()
    if S.slash2 then dse.audio.play_sfx(S.slash2, 0.4, 0) end
  end
  -- 宠物技能开始
  PetSystem.on_pet_skill_start = function()
    spawn_hit_effect(Player.x, 0.5, Player.z, 10, "skill")
  end
  -- 攻击力提升 (C# Cha_Control.AttakUp)
  PetSystem.on_attack_up = function(factor)
    AttakUp(factor)
  end
  -- 重置攻击力 (C# Cha_Control.ResetAtk)
  PetSystem.on_reset_atk = function()
    ResetAtk()
  end
  -- 播放音效
  PetSystem.on_play_audio = function(name)
    local snd = S[name]
    if snd then dse.audio.play_sfx(snd, 0.8, 0) end
  end

  -- 初始化宠物系统
  PetSystem.init()

  -- 读取存档 (恢复金币/玉石/技能/武器/关卡进度; 无存档则为新游戏)
  local loaded = SaveSystem.load_all()
  if not loaded then
    G.stage_index = 0
  end

  -- 初始化玩家 (沿用存档的武器与等级)
  ChangeCharacter(Player.weapon_kind or 0)
  ResetPower()
  Player.maxhp = 95 + Player.level * 5
  Player.hp = Player.maxhp
  Player.sp = Player.maxsp

  -- ── 游戏外 UI 回调注入 (主菜单/地图/技能商店) ──────────────────────
  MenuSystem.on_new_game = function() RestartGame() end
  MenuSystem.on_start_stage = function(idx) StartStage(idx) end
  MenuSystem.on_quit = function()
    -- 退出游戏 (引擎无强制退出 API, 置模式由 DSE_MAX_FRAMES/窗口关闭兜底)
    G.mode = "quit"
    if S.click then dse.audio.play_sfx(S.click, 0.5, 0) end
  end

  -- 进入主菜单 (C# UI_intro)
  UISystem.clear()
  MenuSystem.show_intro()
  G.mode = "menu"
  if S.bgm_intro then dse.audio.play_bgm(S.bgm_intro, 0.6, true) end

  print("[topdown_3d] Game initialized — full port from C# source")
end

function Update(dt)
  -- 原始帧时间 (Cutin01 用, C# Time.realtimeSinceStartup 不受 timeScale 影响)
  local raw_dt = dt or 0.0
  dt = raw_dt * G.time_scale
  if dt > 0.05 then dt = 0.05 end

  G.time = G.time + dt

  -- 时间减速恢复
  if G.time_scale_timer > 0 then
    G.time_scale_timer = G.time_scale_timer - dt
    if G.time_scale_timer <= 0 then
      G.time_scale = 1.0
    end
  end

  -- ESC 暂停切换 (C# UI_Ingame_GUI PauseOn/PauseOff, 仅战斗中)
  if (G.mode == "play" or (G.boss_intro_timer and G.boss_intro_timer > 0))
     and app.get_key_down(KEY_ESCAPE) then
    UISystem.toggle_pause()
  end

  -- Cutin01 特写更新 (使用原始帧时间)
  if Cutin01.Cutin.active then
    UpdateCutin(raw_dt)
  end

  -- Boss 入场 (相机聚焦 Boss, C# LookTarget)
  if G.boss_intro_timer > 0 then
    G.boss_intro_timer = G.boss_intro_timer - dt
    UpdateCamera(dt)
    UpdateEffects(dt)
    EfSystem.update(dt)
    UISystem.update(dt)
    return
  end

  -- 剧情演出 (Phase 4)
  if G.mode == "story" then
    ScenarioSystem.update(dt)
    UpdateCamera(dt)
    return
  end

  -- 游戏外界面 (主菜单/世界地图/技能商店)
  if G.mode == "menu" or G.mode == "map" or G.mode == "shop" then
    G.time_scale = 1.0  -- 菜单模式不受暂停 timeScale 影响
    MenuSystem.update(dt)
    UpdateCamera(dt)
    UpdateEffects(dt)
    EfSystem.update(dt)
    return
  end

  if G.mode == "play" then
    UpdatePlayer(dt)
    UpdateEnemies(dt)
    if Entities.boss then UpdateBoss(dt) end
    UpdateBullets(dt)
    SkillSystem.update(dt)
    AISystem.update(dt)
    PetSystem.update(dt)
    UpdateCombat(dt)
    UpdateSpawn(dt)
    UpdateObjective(dt)
    UpdateCamera(dt)
    UpdateEffects(dt)
    EfSystem.update(dt)
    UISystem.update(dt)
  elseif G.mode == "level_complete" then
    G.level_complete_timer = G.level_complete_timer + dt
    UpdateEffects(dt)
    EfSystem.update(dt)
    UpdateCamera(dt)
    UISystem.update(dt)
    if G.level_complete_timer > 2.0 then
      G.level_complete_timer = 0
      OnStageCleared()
    end
  else
    -- game_over / win
    if app.get_key_down(KEY_R) then
      if G.mode == "win" then
        -- 通关后回主菜单
        UISystem.clear()
        MenuSystem.show_intro()
        G.mode = "menu"
      else
        -- 死亡后回地图
        UISystem.clear()
        MenuSystem.show_map()
        G.mode = "map"
      end
      if S.bgm_intro then dse.audio.play_bgm(S.bgm_intro, 0.6, true) end
    end
    UpdateEffects(dt)
    EfSystem.update(dt)
    UpdateCamera(dt)
    UISystem.update(dt)
  end
end
