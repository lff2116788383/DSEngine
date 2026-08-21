-- ============================================================================
-- cutin01.lua — Boss 特写动画 (Cutin01.cs 完整移植)
-- 时间暂停 → 大图弹出(10x) → 缩放恢复 → 复位相机 (CameraReset 由 main 注入)
-- ============================================================================
local State = require("state")

-- dse.ui.set_visible 只接受 number, 这里包装布尔转换
local function ui_set_visible(e, v) dse.ui.set_visible(e, v and 1 or 0) end
local G = State.G
local Player = State.Player
local clamp = State.clamp
local lerp = State.lerp

-- main.lua 注入的相机复位 (CamMove.CameraReset)
local camera_reset = function()
end
local function set_camera(fn)
  camera_reset = fn
end

local Cutin = {
  panel_e = nil, label_e = nil,
  starttime = 0, active = false,
  startlerp = 2.4, endlerp = 2.5, prevtimescale = 1.0,
}

local function CutinOn(boss_tex_path, bossname)
  -- 创建横幅面板 + 大标题 (仅首次, C# bg_black + cut_boss)
  if not Cutin.panel_e then
    Cutin.panel_e = dse.ecs.create_entity()
    dse.ecs.add_transform(Cutin.panel_e, 0, 0, 0, 1, 1, 1)
    -- 原版 Boss 名称大图 (C# Cutin_BossTexture.SetCutinTexture)
    local h = boss_tex_path and dse.assets.load_texture(boss_tex_path) or 0
    dse.ui.add_renderer(Cutin.panel_e, h, 1, 1, 1, 1.0, 200, 1, 1)
  end
  if not Cutin.label_e then
    Cutin.label_e = dse.ecs.create_entity()
    dse.ecs.add_transform(Cutin.label_e, 0, 0, 0, 1, 1, 1)
    dse.ui.add_label(Cutin.label_e, bossname or "", G._font_tex or 0, 1.0, 0.85, 0.3, 1.0, 56, 64, 2.0, 16, 6, 32, 0, 0)
  end
  -- 初始化 (C# CutinOn)
  Cutin.active = true
  Cutin.starttime = 0
  Cutin.prevtimescale = G.time_scale
  G.time_scale = 0.1               -- C# Time.timeScale = timescale
  G.time_scale_timer = 0
  Player.control_lock = 3.0        -- C# script_cha.StopControl()
  ui_set_visible(Cutin.panel_e, true)
  ui_set_visible(Cutin.label_e, true)
  dse.ui.set_position(Cutin.panel_e, 0, 0)
  dse.ui.set_position(Cutin.label_e, 0, 40)
  dse.ui.set_size(Cutin.panel_e, 3000, 900)   -- C# 初始 scale = originscale * 10
  dse.ui.set_color(Cutin.label_e, 1.0, 0.85, 0.3, 1.0)
end

-- dt 必须是未受 time_scale 影响的原始帧时间 (C# Time.realtimeSinceStartup)
local function UpdateCutin(dt)
  if not Cutin.active then return end
  Cutin.starttime = Cutin.starttime + dt
  if Cutin.starttime < Cutin.startlerp then
    -- 大图缩小到正常尺寸 (C# scale Lerp * 50)
    local w = lerp(3000, 420, clamp(dt * 200, 0, 1))
    local h = lerp(900, 140, clamp(dt * 200, 0, 1))
    dse.ui.set_size(Cutin.panel_e, w, h)
  elseif Cutin.starttime < Cutin.endlerp then
    -- 时间恢复 + 缩回 10 倍 (C# timeScale = prev; scale Lerp * 5.6)
    G.time_scale = Cutin.prevtimescale
    local w = lerp(420, 3000, clamp(dt * 22, 0, 1))
    local h = lerp(140, 900, clamp(dt * 22, 0, 1))
    dse.ui.set_size(Cutin.panel_e, w, h)
    local fade = 1.0 - clamp(Cutin.starttime - Cutin.startlerp, 0, 1)
    dse.ui.set_color(Cutin.label_e, 1.0, 0.85, 0.3, fade)
  else
    -- 结束 (C# ResetCam + StartControl + 隐藏)
    G.time_scale = 1.0
    Cutin.active = false
    Player.control_lock = 0
    ui_set_visible(Cutin.panel_e, false)
    ui_set_visible(Cutin.label_e, false)
    camera_reset()
  end
end

local M = {
  Cutin = Cutin,
  CutinOn = CutinOn,
  UpdateCutin = UpdateCutin,
  set_camera = set_camera,
}

return M
