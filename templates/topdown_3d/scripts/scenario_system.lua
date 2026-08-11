-- ============================================================================
-- scenario_system.lua — 剧情演出 (Phase 4)
-- 对应 C# scenario.cs 简化版: 对话框文本推进
-- 不做头像移动/运镜/特效, 保留镜头序列与中文台词
-- main.lua 在 mode="story" 时驱动 update, 播完回调 on_finish 进入战斗
-- ============================================================================

local State = require("state")
local G = State.G
local UISystem = require("ui_system")
local ScenarioData = require("scenario_data")

local make_quad = UISystem.make_quad
local make_text = UISystem.make_text
local kill_ui = UISystem.kill_ui

local M = {}

-- 回调 (main.lua 注入): 剧情播放完毕
M.on_finish = nil

local active = false
local stage_idx = 0
local scene_idx = 0
local scenes = nil
local ui = {}

local function clear()
  for _, e in ipairs(ui.all or {}) do kill_ui(e) end
  ui = { all = {} }
end

-- 该关是否有剧情
function M.has_scene(idx)
  local sc = ScenarioData.DB_Scenario[idx]
  return sc and sc.n and sc.n > 0
end

local function finish()
  active = false
  clear()
  if M.on_finish then M.on_finish() end
end

local function show_scene(i)
  local sc = scenes.s[i + 1]
  if not sc then
    finish()
    return
  end
  local txt = ScenarioData.ScenarioTxt[sc[4]]
  if txt == nil then txt = "" end
  dse.ui.set_label_text(ui.text, txt)
end

-- 开始播放某关剧情; 无剧情则立即回调结束并返回 false
function M.start(idx)
  stage_idx = idx or 0
  local sc = ScenarioData.DB_Scenario[stage_idx]
  if not sc or not sc.n or sc.n <= 0 then
    if M.on_finish then M.on_finish() end
    return false
  end
  scenes = sc
  scene_idx = 0
  active = true
  clear()
  -- 全屏暗底 + 底部对话框
  ui.bg = make_quad(0, 0, 1280, 720, 0.02, 0.02, 0.05, 1.0, 980)
  ui.all[#ui.all + 1] = ui.bg
  ui.box = make_quad(0, -190, 1120, 180, 0.1, 0.08, 0.15, 0.96, 981)
  ui.all[#ui.all + 1] = ui.box
  ui.text = make_text("", 0, -185, 1.0, 1.0, 1.0, 16, 22)
  ui.all[#ui.all + 1] = ui.text
  ui.hint = make_text("按 J / 空格 / Enter 继续", 0, -265, 0.6, 0.6, 0.6, 12, 16)
  ui.all[#ui.all + 1] = ui.hint
  ui.title = make_text(string.format("— 第 %d 幕 —", stage_idx + 1), 0, 260, 0.8, 0.8, 0.5, 20, 26)
  ui.all[#ui.all + 1] = ui.title
  show_scene(0)
  return true
end

function M.update(dt)
  if not active then return end
  -- 按键推进 (J/空格/Enter)
  if dse.app.get_key_down(74) or dse.app.get_key_down(32) or dse.app.get_key_down(257) then
    scene_idx = scene_idx + 1
    show_scene(scene_idx)
  end
end

function M.is_active() return active end

-- 强制结束 (如进入战斗前清理)
function M.cancel()
  if active then
    active = false
    clear()
  end
end

return M
