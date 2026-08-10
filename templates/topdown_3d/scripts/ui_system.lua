-- ============================================================================
-- ui_system.lua — Phase 5: 完整 UI 系统
-- 1:1 移植自 Unity 逆向 C# 源码:
--   UI_Ingame.cs      → 血条/蓝条/经验条/魂力条/连击条/蓄力条/关卡进度/Boss血条
--   UI_Ingame_GUI.cs  → 暂停菜单/选项/复活界面/教程引导/天使通知
--   Icon_Skill.cs     → 技能图标/魂力图标/冷却填充/宠物图标
--   Gauge_UV.cs       → UV 血条 (DSEngine 用 set_size 替代)
--   MakeUI.cs         → CreatCustomPlane (DSEngine 用 add_renderer 替代)
--   Txt_result.cs     → 结算文字
--   Txt_star.cs       → 星级评价
-- ============================================================================

local State = require("state")
local G, Player, Entities = State.G, State.Player, State.Entities
local clamp, lerp = State.clamp, State.lerp

local M = {}

-- dse.ui.set_visible 只接受 number, 这里包装布尔转换
local function ui_set_visible(e, v) dse.ui.set_visible(e, v and 1 or 0) end

-- ── 内部状态 ────────────────────────────────────────────────────────────
local ui = {}         -- 所有 UI 实体句柄
local gauges = {}     -- 血条等 gauge 对象
local pause_menu = {} -- 暂停菜单实体
local result_ui = {}  -- 结算界面实体
local chance_ui = {}  -- 复活界面实体
local skill_icons = {}-- 技能图标实体

-- UI 状态
local paused = false
local option_mode = false
local chance_mode = false
local result_mode = nil  -- nil / "clear" / "game_over" / "wave_clear"
local result_timer = 0
local chance_count = 10
local chance_timer = 0
local level_up_timer = 0
local level_up_text = nil
local charge_visible = false
local charge_value = 0
local combo_super = false
local combo_fill = 0
local gauge_scale_change = false
local gauge_scale_target = 1.0
local gauge_scale_current = 1.0
local play_time = 0
local countdown_text = ""
local finish_delay = 0
local return_map = false
local score_anim_cur = 0
local score_anim_target = 0
local score_anim_timer = 0

-- ── 辅助函数 ────────────────────────────────────────────────────────────

-- 创建有色 quad (替代 MakeUI.CreatCustomPlane)
local function make_quad(x, y, w, h, r, g, b, a, depth)
  local e = dse.ecs.create_entity()
  dse.ecs.add_transform(e, 0, 0, 0, 1, 1, 1)
  dse.ui.add_renderer(e, 0, r or 0.5, g or 0.5, b or 0.5, a or 1.0, depth or 900, w, h)
  dse.ui.add_panel(e, false)
  dse.ui.set_position(e, x, y)
  dse.ui.set_size(e, w, h)
  ui_set_visible(e, true)
  return e
end

-- 创建文本标签 (替代 TextMesh)
local function make_text(text, x, y, r, g, b, gw, gh, scale)
  local e = dse.ecs.create_entity()
  dse.ecs.add_transform(e, 0, 0, 0, 1, 1, 1)
  local font_tex = G._font_tex or 0
  dse.ui.add_label(e, text or "", font_tex, r or 1, g or 1, b or 1, 1.0,
                   gw or 16, gh or 20, scale or 1.0, 16, 6, 32, x or 0, y or 0)
  return e
end

-- 创建交互按钮
local function make_button(x, y, w, h, r, g, b, a, depth)
  local e = dse.ecs.create_entity()
  dse.ecs.add_transform(e, 0, 0, 0, 1, 1, 1)
  dse.ui.add_renderer(e, 0, r or 0.3, g or 0.3, b or 0.3, a or 0.9, depth or 950, w, h)
  dse.ui.add_button(e, 0.25, 0.25, 0.25, 0.95)
  dse.ui.set_position(e, x, y)
  dse.ui.set_size(e, w, h)
  ui_set_visible(e, true)
  return e
end

-- 创建血条 gauge (替代 Gauge_UV + CreatCustomPlane)
-- 返回 {bg, fg, x, y, w, h, r, g, b, depth}
local function create_gauge(x, y, w, h, r, g, b, depth)
  depth = depth or 900
  -- 背景 (暗色底)
  local bg = make_quad(x, y, w, h, 0.1, 0.1, 0.1, 0.85, depth + 1)
  -- 前景 (填充色)
  local fg = make_quad(x - w * 0.5, y, w, h, r, g, b, 1.0, depth)
  return { bg = bg, fg = fg, x = x, y = y, w = w, h = h, r = r, g = g, b = b, depth = depth, cur_fill = 1.0 }
end

-- 设置 gauge 填充量 (0.0~1.0) — 替代 Gauge_UV.UvMove
-- C# 原理: UV 偏移使血条从右向左减少
-- DSEngine: 调整 fg 的 size 和 position 实现相同视觉效果
local function set_gauge_fill(gauge, fill)
  if not gauge or not gauge.fg then return end
  fill = clamp(fill, 0, 1)
  gauge.cur_fill = fill
  local fw = gauge.w * fill
  if fw < 1 then
    ui_set_visible(gauge.fg, false)
  else
    ui_set_visible(gauge.fg, true)
    dse.ui.set_size(gauge.fg, fw, gauge.h)
    -- 左对齐: position.x = x - w/2 + fw/2
    dse.ui.set_position(gauge.fg, gauge.x - gauge.w * 0.5 + fw * 0.5, gauge.y)
  end
end

-- 设置 gauge 可见性
local function set_gauge_visible(gauge, vis)
  if not gauge then return end
  ui_set_visible(gauge.bg, vis)
  ui_set_visible(gauge.fg, vis)
end

-- 设置 gauge 颜色 (用于超级模式变色等)
local function set_gauge_color(gauge, r, g, b)
  if not gauge or not gauge.fg then return end
  dse.ui.set_color(gauge.fg, r, g, b, 1.0)
end

-- 安全销毁实体
local function kill_ui(e)
  if e and e ~= 0 then pcall(dse.ecs.destroy_entity, e) end
end

-- ============================================================================
-- 主 HUD 构建 (UI_Ingame.cs → Start / BuildHUD)
-- ============================================================================
function M.build()
  M.clear()

  -- ── HP 血条 (左上) ──────────────────────────────────────────────────
  -- C# g_hp: gauge_hp, 位置 (-0.757, 2.92, 1.8), size (0.9, 0.1)
  -- 困难模式 cur_difficulty==2: 更短
  gauges.hp = create_gauge(-380, 310, 220, 14, 1.0, 0.25, 0.25, 905)
  ui.hp_text = make_text("HP 100/100", -380, 328, 1.0, 0.4, 0.4, 16, 20)

  -- HP 图标背景 (C# bg_hp at -1.35, 2.85, 2f)
  ui.hp_icon_bg = make_quad(-520, 310, 28, 28, 0.3, 0.15, 0.15, 0.9, 906)

  -- ── SP 蓝条 ──────────────────────────────────────────────────────────
  gauges.sp = create_gauge(-380, 285, 220, 10, 0.3, 0.5, 1.0, 905)
  ui.sp_text = make_text("SP 100/100", -380, 298, 0.3, 0.6, 1.0, 14, 18)

  -- ── EXP 经验条 (竖直, 右侧) ─────────────────────────────────────────
  -- C# gauge_exp: 位置 (-1.352, 2.85, 2.1), size 0.186, 方向竖直
  -- DSEngine: 竖直条用 height 填充
  gauges.exp = create_gauge(-545, 270, 10, 70, 0.3, 1.0, 0.3, 905)
  ui.exp_text = make_text("LV 1", -545, 330, 1.0, 0.85, 0.3, 12, 16)
  ui.exp_text2 = make_text("Exp 0", -545, 200, 0.5, 0.8, 0.5, 10, 14)

  -- ── Soul/MP 魂力条 (水平, HP下方) ───────────────────────────────────
  -- C# g_mp: gauge_mp, 位置 (-0.6775, 2.78, 1.8), size (1.06, 0.1)
  gauges.soul = create_gauge(-380, 260, 220, 8, 0.8, 0.3, 1.0, 905)
  ui.soul_text = make_text("Soul 1/8", -380, 272, 0.8, 0.3, 1.0, 12, 16)

  -- ── Combo 连击条 (中上) ─────────────────────────────────────────────
  -- C# g_combo: gauge_combo, 位置 (0, 2.74, 2.1), size (0.6, 0.05)
  gauges.combo = create_gauge(0, 280, 180, 8, 1.0, 0.85, 0.3, 903)
  ui.combo_text = make_text("", 0, 295, 1.0, 0.85, 0.3, 18, 24)
  ui.combo_label = make_text("COMBO", 0, 268, 0.7, 0.7, 0.7, 10, 14)
  set_gauge_visible(gauges.combo, false)

  -- ── Power 蓄力条 (中央, 默认隐藏) ───────────────────────────────────
  -- C# g_power: gauge_power, 位置 (0, 2.4, 2.1), size (0.4, 0.05)
  gauges.power = create_gauge(0, 235, 120, 8, 1.0, 1.0, 0.3, 895)
  set_gauge_visible(gauges.power, false)

  -- ── Stage 关卡信息 (右上) ───────────────────────────────────────────
  ui.stage_text = make_text("Stage 1", 420, 320, 0.9, 0.9, 1.0, 18, 24)
  ui.weapon_text = make_text("Weapon: 刀", 420, 295, 0.9, 0.9, 0.9, 14, 18)
  ui.skill_text = make_text("Skill READY [K]", 420, 270, 0.6, 0.8, 1.0, 14, 18)
  ui.coin_text = make_text("Coin 0", 420, 245, 1.0, 0.85, 0.2, 14, 18)
  ui.kill_text = make_text("Kill 0", 420, 222, 0.8, 0.8, 0.8, 12, 16)

  -- ── 关卡进度条 (右上, C# gauge_stage) ───────────────────────────────
  gauges.stage_progress = create_gauge(420, 200, 100, 6, 0.3, 1.0, 0.3, 903)
  ui.progress_text = make_text("", -200, 200, 0.6, 1.0, 0.6, 12, 16)

  -- ── Boss 血条 (顶部中央) ────────────────────────────────────────────
  -- C# 在 Boss 出现时显示
  gauges.boss_hp = create_gauge(0, 300, 360, 18, 1.0, 0.15, 0.15, 915)
  ui.boss_text = make_text("", 0, 318, 1.0, 0.3, 0.3, 20, 28)
  ui.boss_name = make_text("", 0, 335, 1.0, 0.9, 0.4, 16, 20)
  set_gauge_visible(gauges.boss_hp, false)
  ui_set_visible(ui.boss_text, false)
  ui_set_visible(ui.boss_name, false)

  -- ── 中央状态文字 (结算/提示) ────────────────────────────────────────
  ui.status_text = make_text("", 0, 50, 1.0, 1.0, 0.4, 30, 40)

  -- ── 底部操作提示 ────────────────────────────────────────────────────
  ui.tip_text = make_text("", 0, -330, 0.6, 0.6, 0.6, 12, 16)

  -- ── 暂停按钮 (右上角) ───────────────────────────────────────────────
  -- C# icon_pause at (1.38, 2.9, 2f)
  ui.pause_btn = make_button(560, 320, 36, 36, 0.25, 0.25, 0.25, 0.85, 960)
  ui.pause_btn_label = make_text("II", 560, 320, 1.0, 1.0, 1.0, 14, 18)

  -- ── 倒计时显示 ──────────────────────────────────────────────────────
  ui.countdown_text = make_text("", 200, 320, 1.0, 0.9, 0.5, 16, 20)

  -- ── 分数显示 (无限模式) ─────────────────────────────────────────────
  ui.score_text = make_text("", 200, 295, 1.0, 0.9, 0.3, 14, 18)
  ui.wave_text = make_text("", 200, 270, 0.8, 0.8, 1.0, 14, 18)

  -- ── 武将 HP 条 (武将召唤时显示) ─────────────────────────────────────
  gauges.general_hp = create_gauge(-380, 240, 180, 10, 0.9, 0.7, 0.3, 904)
  ui.general_text = make_text("", -380, 252, 0.9, 0.7, 0.3, 12, 16)
  set_gauge_visible(gauges.general_hp, false)
  ui_set_visible(ui.general_text, false)

  -- 构建暂停菜单
  M._build_pause_menu()
  -- 构建结算界面
  M._build_result_screen()
  -- 构建复活界面
  M._build_chance_screen()
  -- 构建技能图标
  M._build_skill_icons()
end

-- ============================================================================
-- 暂停菜单 (UI_Ingame_GUI.cs → OnGUI pause/option)
-- ============================================================================
function M._build_pause_menu()
  -- 半透明背景遮罩
  pause_menu.bg = make_quad(0, 0, 1280, 720, 0, 0, 0, 0.7, 970)
  ui_set_visible(pause_menu.bg, false)

  -- 三个按钮: 继续游戏 / 选项 / 退出
  pause_menu.resume_btn = make_button(0, 50, 200, 48, 0.15, 0.3, 0.15, 0.9, 975)
  pause_menu.resume_label = make_text("RESUME", 0, 50, 1.0, 1.0, 1.0, 18, 24)
  ui_set_visible(pause_menu.resume_btn, false)
  ui_set_visible(pause_menu.resume_label, false)

  pause_menu.option_btn = make_button(0, -10, 200, 48, 0.15, 0.2, 0.3, 0.9, 975)
  pause_menu.option_label = make_text("OPTIONS", 0, -10, 1.0, 1.0, 1.0, 18, 24)
  ui_set_visible(pause_menu.option_btn, false)
  ui_set_visible(pause_menu.option_label, false)

  pause_menu.quit_btn = make_button(0, -70, 200, 48, 0.3, 0.15, 0.15, 0.9, 975)
  pause_menu.quit_label = make_text("QUIT", 0, -70, 1.0, 1.0, 1.0, 18, 24)
  ui_set_visible(pause_menu.quit_btn, false)
  ui_set_visible(pause_menu.quit_label, false)

  -- 选项面板
  pause_menu.option_bg = make_quad(0, 0, 400, 300, 0.1, 0.1, 0.15, 0.95, 978)
  ui_set_visible(pause_menu.option_bg, false)

  pause_menu.option_title = make_text("OPTIONS", 0, 120, 1.0, 0.9, 0.4, 20, 28)
  ui_set_visible(pause_menu.option_title, false)

  -- BGM 音量
  pause_menu.bgm_label = make_text("BGM Volume", -120, 60, 0.8, 0.8, 0.8, 14, 18)
  pause_menu.bgm_bar_bg = make_quad(0, 60, 160, 12, 0.2, 0.2, 0.2, 0.9, 979)
  pause_menu.bgm_bar_fg = make_quad(-80, 60, 160, 12, 0.3, 0.6, 1.0, 1.0, 980)
  ui_set_visible(pause_menu.bgm_label, false)
  ui_set_visible(pause_menu.bgm_bar_bg, false)
  ui_set_visible(pause_menu.bgm_bar_fg, false)

  -- Master 音量
  pause_menu.master_label = make_text("Master Volume", -120, 20, 0.8, 0.8, 0.8, 14, 18)
  pause_menu.master_bar_bg = make_quad(0, 20, 160, 12, 0.2, 0.2, 0.2, 0.9, 979)
  pause_menu.master_bar_fg = make_quad(-80, 20, 160, 12, 0.6, 0.8, 0.3, 1.0, 980)
  ui_set_visible(pause_menu.master_label, false)
  ui_set_visible(pause_menu.master_bar_bg, false)
  ui_set_visible(pause_menu.master_bar_fg, false)

  -- FOV 缩放
  pause_menu.fov_label = make_text("Camera Zoom", -120, -20, 0.8, 0.8, 0.8, 14, 18)
  pause_menu.fov_minus = make_button(-30, -20, 32, 32, 0.2, 0.2, 0.2, 0.9, 981)
  pause_menu.fov_plus = make_button(30, -20, 32, 32, 0.2, 0.2, 0.2, 0.9, 981)
  pause_menu.fov_value = make_text("x 0", 0, -20, 1.0, 1.0, 1.0, 14, 18)
  ui_set_visible(pause_menu.fov_label, false)
  ui_set_visible(pause_menu.fov_minus, false)
  ui_set_visible(pause_menu.fov_plus, false)
  ui_set_visible(pause_menu.fov_value, false)

  -- 返回按钮
  pause_menu.back_btn = make_button(0, -100, 120, 40, 0.15, 0.25, 0.15, 0.9, 981)
  pause_menu.back_label = make_text("BACK", 0, -100, 1.0, 1.0, 1.0, 16, 20)
  ui_set_visible(pause_menu.back_btn, false)
  ui_set_visible(pause_menu.back_label, false)
end

-- ============================================================================
-- 结算界面 (Txt_result.cs + UI_Ingame.ShowTxt)
-- ============================================================================
function M._build_result_screen()
  -- 半透明背景
  result_ui.bg = make_quad(0, 0, 1280, 720, 0, 0, 0, 0.5, 970)
  ui_set_visible(result_ui.bg, false)

  -- 结果标题
  result_ui.title = make_text("", 0, 80, 1.0, 1.0, 0.4, 36, 48)
  ui_set_visible(result_ui.title, false)

  -- 星级评价 (无限模式)
  result_ui.stars = {}
  for i = 1, 3 do
    result_ui.stars[i] = make_text("*", (i - 2) * 40, 30, 0.6, 0.6, 0.2, 28, 36)
    ui_set_visible(result_ui.stars[i], false)
  end

  -- 统计信息
  result_ui.kill_label = make_text("Kills:", -120, -10, 0.8, 0.8, 0.8, 14, 18)
  result_ui.kill_value = make_text("0", 80, -10, 1.0, 0.9, 0.5, 14, 18)
  ui_set_visible(result_ui.kill_label, false)
  ui_set_visible(result_ui.kill_value, false)

  result_ui.combo_label = make_text("Max Combo:", -120, -40, 0.8, 0.8, 0.8, 14, 18)
  result_ui.combo_value = make_text("0", 80, -40, 1.0, 0.9, 0.5, 14, 18)
  ui_set_visible(result_ui.combo_label, false)
  ui_set_visible(result_ui.combo_value, false)

  result_ui.time_label = make_text("Time:", -120, -70, 0.8, 0.8, 0.8, 14, 18)
  result_ui.time_value = make_text("0:00", 80, -70, 1.0, 0.9, 0.5, 14, 18)
  ui_set_visible(result_ui.time_label, false)
  ui_set_visible(result_ui.time_value, false)

  result_ui.score_label = make_text("Score:", -120, -100, 0.8, 0.8, 0.8, 14, 18)
  result_ui.score_value = make_text("0", 80, -100, 1.0, 0.9, 0.5, 16, 20)
  ui_set_visible(result_ui.score_label, false)
  ui_set_visible(result_ui.score_value, false)

  -- 继续按钮
  result_ui.continue_btn = make_button(0, -160, 180, 44, 0.15, 0.3, 0.15, 0.9, 980)
  result_ui.continue_label = make_text("Press R to Continue", 0, -160, 1.0, 1.0, 1.0, 16, 20)
  ui_set_visible(result_ui.continue_btn, false)
  ui_set_visible(result_ui.continue_label, false)
end

-- ============================================================================
-- 复活界面 (UI_Ingame_GUI.cs → chance)
-- ============================================================================
function M._build_chance_screen()
  -- 半透明黑色背景 (渐变)
  chance_ui.bg = make_quad(0, 0, 1280, 720, 0, 0, 0, 0.8, 975)
  ui_set_visible(chance_ui.bg, false)

  -- 倒计时文字
  chance_ui.count_text = make_text("10", 0, 50, 1.0, 1.0, 1.0, 40, 52)
  ui_set_visible(chance_ui.count_text, false)

  -- 提示文字
  chance_ui.prompt_text = make_text("Continue?", 0, 100, 1.0, 0.9, 0.4, 24, 32)
  ui_set_visible(chance_ui.prompt_text, false)

  -- 需要的玉石数量
  chance_ui.jade_cost_text = make_text("Cost: 1 Jade", 0, 0, 0.9, 0.9, 0.3, 18, 24)
  ui_set_visible(chance_ui.jade_cost_text, false)

  chance_ui.jade_have_text = make_text("(Have: 0)", 0, -30, 0.7, 0.7, 0.7, 14, 18)
  ui_set_visible(chance_ui.jade_have_text, false)

  -- 复活按钮
  chance_ui.revive_btn = make_button(-80, -80, 120, 40, 0.15, 0.3, 0.15, 0.9, 980)
  chance_ui.revive_label = make_text("REVIVE", -80, -80, 1.0, 1.0, 1.0, 16, 20)
  ui_set_visible(chance_ui.revive_btn, false)
  ui_set_visible(chance_ui.revive_label, false)

  -- 放弃按钮
  chance_ui.quit_btn = make_button(80, -80, 120, 40, 0.3, 0.15, 0.15, 0.9, 980)
  chance_ui.quit_label = make_text("GIVE UP", 80, -80, 1.0, 1.0, 1.0, 16, 20)
  ui_set_visible(chance_ui.quit_btn, false)
  ui_set_visible(chance_ui.quit_label, false)
end

-- ============================================================================
-- 技能图标 (Icon_Skill.cs)
-- ============================================================================
function M._build_skill_icons()
  -- 技能图标在右上角竖直排列 (C# skill_iconpos = (1.4, 2.6, 2f))
  -- 每个技能槽一个图标 + 冷却填充条
  for i = 1, 6 do
    local y = 200 - (i - 1) * 36
    -- 技能图标背景
    skill_icons[i] = {
      bg = make_quad(540, y, 28, 28, 0.2, 0.2, 0.25, 0.9, 940),
      icon = make_quad(540, y, 28, 28, 0.4, 0.5, 0.7, 1.0, 941),
      cd_bg = make_quad(540, y, 28, 28, 0.8, 0.8, 0.8, 0.7, 942),
      cd_fg = make_quad(540, y + 14, 28, 0, 0.2, 0.2, 0.2, 0.85, 943),
      label = make_text("", 540, y, 1.0, 1.0, 1.0, 10, 14),
      cost = make_text("", 530, y + 12, 1.0, 0.9, 0.3, 8, 12),
      y = y,
    }
    ui_set_visible(skill_icons[i].cd_fg, false)
  end

  -- 暂停图标已在主 HUD 中创建

  -- SP 不足警告图标 (C# icon_shortsp)
  ui.sp_warn = make_quad(-540, 250, 20, 20, 1.0, 0.5, 0.2, 0.9, 944)
  ui_set_visible(ui.sp_warn, false)
end

-- ============================================================================
-- 更新主 HUD (UI_Ingame.cs → Update)
-- ============================================================================
function M.update(dt)
  if not ui.hp_text then return end  -- 未初始化

  -- 暂停模式下不更新游戏 HUD
  if paused then
    M._update_pause_menu(dt)
    return
  end

  -- ── HP 血条 ─────────────────────────────────────────────────────────
  local hp_ratio = Player.maxhp > 0 and math.max(0, Player.hp) / Player.maxhp or 0
  set_gauge_fill(gauges.hp, hp_ratio)
  dse.ui.set_label_text(ui.hp_text, string.format("HP %d/%d", math.max(0, Player.hp), Player.maxhp))
  -- 低血量警告变色
  if hp_ratio < 0.25 then
    set_gauge_color(gauges.hp, 1.0, 0.1, 0.1)
  else
    set_gauge_color(gauges.hp, 1.0, 0.25, 0.25)
  end

  -- ── SP 蓝条 ─────────────────────────────────────────────────────────
  local sp_ratio = Player.maxsp > 0 and math.max(0, Player.sp) / Player.maxsp or 0
  set_gauge_fill(gauges.sp, sp_ratio)
  dse.ui.set_label_text(ui.sp_text, string.format("SP %d/%d", math.floor(Player.sp), Player.maxsp))
  -- SP 不足警告
  ui_set_visible(ui.sp_warn, Player.sp < 10 and Player.sp >= 0)

  -- ── EXP 经验条 ──────────────────────────────────────────────────────
  local exp_max = Player.level * 100
  local exp_ratio = exp_max > 0 and Player.exp / exp_max or 0
  -- 竖直条: 从下往上填充
  if gauges.exp then
    local fh = gauges.exp.h * exp_ratio
    dse.ui.set_size(gauges.exp.fg, gauges.exp.w, fh)
    dse.ui.set_position(gauges.exp.fg, gauges.exp.x, gauges.exp.y - gauges.exp.h * 0.5 + fh * 0.5)
    ui_set_visible(gauges.exp.fg, fh > 1)
  end
  dse.ui.set_label_text(ui.exp_text, string.format("LV %d", Player.level))
  dse.ui.set_label_text(ui.exp_text2, string.format("Exp %d/%d", math.floor(Player.exp), exp_max))

  -- ── Soul 魂力条 ─────────────────────────────────────────────────────
  -- C# f_soul 0~8, mp_length = (1 - soul/8) * 0.5
  local soul_ratio = G.soul and G.soul / 8 or 0
  set_gauge_fill(gauges.soul, soul_ratio)
  dse.ui.set_label_text(ui.soul_text, string.format("Soul %d/8", G.soul or 0))

  -- ── Combo 连击条 ────────────────────────────────────────────────────
  -- C# combo_length: 0~0.5, 满 0.5 时触发超级模式
  if G.combo > 0 then
    set_gauge_visible(gauges.combo, true)
    dse.ui.set_label_text(ui.combo_text, string.format("%d", G.combo))
    -- 连击填充量
    combo_fill = clamp(G.combo / 50, 0, 0.5) * 2  -- 0~1
    set_gauge_fill(gauges.combo, combo_fill)

    -- 超级模式 (combo_length >= 0.5)
    if combo_fill >= 1.0 and not combo_super then
      combo_super = true
      set_gauge_color(gauges.combo, 1.0, 0.5, 0.1)
      dse.ui.set_label_text(ui.status_text, "MAX COMBO! SUPER MODE!")
    elseif combo_fill < 1.0 and combo_super then
      combo_super = false
      set_gauge_color(gauges.combo, 1.0, 0.85, 0.3)
    end
  else
    set_gauge_visible(gauges.combo, false)
    dse.ui.set_label_text(ui.combo_text, "")
    combo_super = false
    set_gauge_color(gauges.combo, 1.0, 0.85, 0.3)
  end

  -- ── Power 蓄力条 ────────────────────────────────────────────────────
  -- C# chargeon: f_charge 0~0.5, 到 0.5 自动释放
  if charge_visible then
    set_gauge_visible(gauges.power, true)
    set_gauge_fill(gauges.power, charge_value * 2)  -- 0~1
    charge_value = charge_value + dt * 0.6
    if charge_value >= 0.5 then
      -- 自动释放
      charge_value = 0
      charge_visible = false
      set_gauge_visible(gauges.power, false)
      if M.on_power_release then M.on_power_release() end
    end
  end

  -- ── Stage 关卡信息 ──────────────────────────────────────────────────
  dse.ui.set_label_text(ui.stage_text, string.format("Stage %d", G.stage_index + 1))

  -- 武器/武将名称
  local DB = require("database")
  if Player.general then
    local gname = DB.DB_General and DB.DB_General[Player.general_kind] and DB.DB_General[Player.general_kind].name or "General"
    dse.ui.set_label_text(ui.weapon_text, "General: " .. gname)
  else
    local wname = DB.DB_Weapon and DB.DB_Weapon[Player.weapon_kind] and DB.DB_Weapon[Player.weapon_kind].name or "?"
    dse.ui.set_label_text(ui.weapon_text, "Weapon: " .. wname)
  end

  -- 技能信息
  local skill_set = Player.skill_slots and Player.skill_slots[Player.current_skill_slot] or 0
  local skill_grade = (Player.skill_grades and Player.skill_grades[skill_set]) or 0
  local skill_data = DB.DB_Skill and DB.DB_Skill[skill_set]
  if skill_data then
    local sname = DB.SkillNames and DB.SkillNames[skill_data.name] or ("Skill" .. tostring(skill_set))
    local cd = Player.skill_cd and Player.skill_cd[skill_set] or 0
    if Player.casting then
      dse.ui.set_label_text(ui.skill_text, string.format("Casting: %s...", sname))
    elseif cd > 0 then
      dse.ui.set_label_text(ui.skill_text, string.format("%s Lv%d CD %.1f", sname, skill_grade + 1, cd))
    elseif skill_data.soulprice and skill_data.soulprice > 0 then
      dse.ui.set_label_text(ui.skill_text, string.format("%s Lv%d [Soul%d] [K]", sname, skill_grade + 1, skill_data.soulprice))
    else
      local cost = 20 + skill_grade * 10
      dse.ui.set_label_text(ui.skill_text, string.format("%s Lv%d [SP%d] [K]", sname, skill_grade + 1, cost))
    end
  else
    dse.ui.set_label_text(ui.skill_text, "No Skill")
  end

  -- 金币/击杀
  dse.ui.set_label_text(ui.coin_text, string.format("Coin %d  Jade %d", G.coin or 0, G.jade or 0))
  dse.ui.set_label_text(ui.kill_text, string.format("Kill %d", G.totalkill or 0))

  -- ── 关卡进度 ────────────────────────────────────────────────────────
  if Entities.boss and not Entities.boss.dead then
    dse.ui.set_label_text(ui.progress_text, "BOSS BATTLE!")
  else
    local stg = M._get_stage_data()
    local total = stg and stg.stagenum * 5 or 15
    dse.ui.set_label_text(ui.progress_text, string.format("Progress %d/%d", G.enemykill or 0, total))
  end

  -- ── Boss 血条 ───────────────────────────────────────────────────────
  if Entities.boss and not Entities.boss.dead then
    set_gauge_visible(gauges.boss_hp, true)
    ui_set_visible(ui.boss_text, true)
    ui_set_visible(ui.boss_name, true)
    local boss_hp_ratio = Entities.boss.maxhp > 0 and math.max(0, Entities.boss.hp) / Entities.boss.maxhp or 0
    set_gauge_fill(gauges.boss_hp, boss_hp_ratio)
    local bname = DB.BossNames and DB.BossNames[Entities.boss.bosskind] or "Boss"
    dse.ui.set_label_text(ui.boss_name, bname)
    dse.ui.set_label_text(ui.boss_text, string.format("HP %d/%d", math.max(0, Entities.boss.hp), Entities.boss.maxhp))
  else
    set_gauge_visible(gauges.boss_hp, false)
    ui_set_visible(ui.boss_text, false)
    ui_set_visible(ui.boss_name, false)
  end

  -- ── 武将 HP 条 ──────────────────────────────────────────────────────
  if Player.general and Player.general_hp >= 0 then
    set_gauge_visible(gauges.general_hp, true)
    ui_set_visible(ui.general_text, true)
    local gh_ratio = Player.general_maxhp > 0 and Player.general_hp / Player.general_maxhp or 0
    set_gauge_fill(gauges.general_hp, gh_ratio)
    dse.ui.set_label_text(ui.general_text, string.format("General HP %d/%d", Player.general_hp, Player.general_maxhp))
  else
    set_gauge_visible(gauges.general_hp, false)
    ui_set_visible(ui.general_text, false)
  end

  -- ── 状态文字 ────────────────────────────────────────────────────────
  local status_msg = ""
  if G.mode == "level_complete" then
    status_msg = "STAGE CLEAR!"
  elseif G.mode == "game_over" then
    status_msg = "GAME OVER"
  elseif G.mode == "win" then
    status_msg = "YOU WIN!"
  elseif G.boss_intro_timer and G.boss_intro_timer > 0 then
    status_msg = "WARNING! BOSS APPROACHING!"
  end
  if combo_super then
    status_msg = "MAX COMBO! SUPER MODE!"
  end
  dse.ui.set_label_text(ui.status_text, status_msg)

  -- ── 操作提示 ────────────────────────────────────────────────────────
  dse.ui.set_label_text(ui.tip_text,
    "WASD move  J atk  K skill  Q skill+  L dodge  O block  P grab  U weapon  I general  ESC pause")

  -- ── 倒计时 (困难模式) ───────────────────────────────────────────────
  if G.difficulty == 1 and play_time > 0 then
    play_time = play_time - dt
    if play_time < 0 then play_time = 0 end
    local mins = math.floor(play_time / 60)
    local secs = math.floor(play_time) % 60
    dse.ui.set_label_text(ui.countdown_text, string.format("%d:%02d", mins, secs))
    if play_time <= 0 and M.on_time_up then M.on_time_up() end
  elseif G.infinitymode then
    play_time = play_time + dt
    local mins = math.floor(play_time / 60)
    local secs = math.floor(play_time) % 60
    dse.ui.set_label_text(ui.countdown_text, string.format("%d:%02d", mins, secs))
  else
    dse.ui.set_label_text(ui.countdown_text, "")
  end

  -- ── 无限模式分数/波数 ──────────────────────────────────────────────
  if G.infinitymode then
    dse.ui.set_label_text(ui.score_text, string.format("Score: %d", G.score or 0))
    dse.ui.set_label_text(ui.wave_text, string.format("Wave: %d", G.wave or 1))
  else
    dse.ui.set_label_text(ui.score_text, "")
    dse.ui.set_label_text(ui.wave_text, "")
  end

  -- ── 技能图标更新 ────────────────────────────────────────────────────
  M._update_skill_icons()

  -- ── 暂停按钮检测 ────────────────────────────────────────────────────
  if dse.ui.is_pressed(ui.pause_btn) and not paused then
    M.pause_on()
  end

  -- ── 结算界面更新 ────────────────────────────────────────────────────
  if result_mode then
    M._update_result(dt)
  end

  -- ── 复活界面更新 ────────────────────────────────────────────────────
  if chance_mode then
    M._update_chance(dt)
  end

  -- ── 升级动画 ────────────────────────────────────────────────────────
  if level_up_timer > 0 then
    level_up_timer = level_up_timer - dt
    if level_up_timer <= 0 then
      if level_up_text then
        ui_set_visible(level_up_text, false)
      end
    end
  end

  -- ── 退出关卡延迟 ────────────────────────────────────────────────────
  if return_map then
    finish_delay = finish_delay + dt
    if finish_delay > 2.0 then
      finish_delay = 0
      return_map = false
      if M.on_finish then M.on_finish() end
    end
  end

  -- ── 分数动画 (无限模式星级结算) ────────────────────────────────────
  if score_anim_timer > 0 then
    score_anim_timer = score_anim_timer - dt
    local diff = score_anim_target - score_anim_cur
    if diff > 1 then
      score_anim_cur = lerp(score_anim_cur, score_anim_target, dt * 5)
      dse.ui.set_label_text(result_ui.score_value, string.format("%d", math.floor(score_anim_cur)))
    else
      score_anim_cur = score_anim_target
      dse.ui.set_label_text(result_ui.score_value, string.format("%d", score_anim_target))
      score_anim_timer = 0
    end
  end

  -- ── HP gauge 缩放动画 (武将切换时) ──────────────────────────────────
  if gauge_scale_change then
    gauge_scale_current = lerp(gauge_scale_current, gauge_scale_target, dt * 8)
    if math.abs(gauge_scale_current - gauge_scale_target) < 0.01 then
      gauge_scale_current = gauge_scale_target
      gauge_scale_change = false
    end
    -- 设置 HP 条宽度缩放
    if gauges.hp then
      local w = 220 * math.abs(gauge_scale_current)
      dse.ui.set_size(gauges.hp.bg, w, gauges.hp.h)
      dse.ui.set_position(gauges.hp.bg, gauges.hp.x, gauges.hp.y)
    end
  end
end

-- ============================================================================
-- 技能图标更新 (Icon_Skill.cs → Update + SoulMeasure)
-- ============================================================================
function M._update_skill_icons()
  local DB = require("database")
  for i = 1, 6 do
    local icon = skill_icons[i]
    if not icon then break end

    local skill_set = Player.skill_slots and Player.skill_slots[i] or -1
    if skill_set and skill_set >= 0 then
      local skill_data = DB.DB_Skill and DB.DB_Skill[skill_set]
      if skill_data then
        local sname = DB.SkillNames and DB.SkillNames[skill_data.name] or ("S" .. tostring(skill_set))
        local grade = (Player.skill_grades and Player.skill_grades[skill_set]) or 0
        local cd = Player.skill_cd and Player.skill_cd[skill_set] or 0
        local cd_max = skill_data.cooltime or 5.0
        local soul_cost = skill_data.soulprice or 0
        local sp_cost = 20 + grade * 10

        -- 图标颜色: 可用=亮色, 冷却中=暗色, 魂力不足=红色
        if cd > 0 then
          -- 冷却中: 从上往下填充灰色
          local cd_ratio = cd / cd_max
          ui_set_visible(icon.cd_fg, true)
          dse.ui.set_size(icon.cd_fg, 28, 28 * cd_ratio)
          dse.ui.set_position(icon.cd_fg, 540, icon.y + 14 - 28 * cd_ratio * 0.5)
          dse.ui.set_color(icon.icon, 0.3, 0.3, 0.3, 1.0)
        else
          ui_set_visible(icon.cd_fg, false)
          -- 检查是否可用
          if soul_cost > 0 then
            if (G.soul or 0) >= soul_cost then
              dse.ui.set_color(icon.icon, 0.4, 0.6, 1.0, 1.0)
            else
              dse.ui.set_color(icon.icon, 0.5, 0.2, 0.2, 1.0)
            end
          else
            if Player.sp >= sp_cost then
              dse.ui.set_color(icon.icon, 0.4, 0.6, 1.0, 1.0)
            else
              dse.ui.set_color(icon.icon, 0.5, 0.2, 0.2, 1.0)
            end
          end
        end

        -- 技能名和消耗
        dse.ui.set_label_text(icon.label, string.format("%s L%d", sname, grade + 1))
        if soul_cost > 0 then
          dse.ui.set_label_text(icon.cost, string.format("S%d", soul_cost))
        else
          dse.ui.set_label_text(icon.cost, string.format("%d", sp_cost))
        end
      else
        dse.ui.set_color(icon.icon, 0.2, 0.2, 0.2, 0.5)
        dse.ui.set_label_text(icon.label, "Locked")
        dse.ui.set_label_text(icon.cost, "")
      end
    else
      dse.ui.set_color(icon.icon, 0.2, 0.2, 0.2, 0.5)
      dse.ui.set_label_text(icon.label, "-")
      dse.ui.set_label_text(icon.cost, "")
      ui_set_visible(icon.cd_fg, false)
    end
  end
end

-- ============================================================================
-- 暂停菜单更新 (UI_Ingame_GUI.cs → OnGUI pause/option)
-- ============================================================================
function M._update_pause_menu(dt)
  -- ESC 键退出暂停
  local KEY_ESCAPE = 256
  if dse.app.get_key_down(KEY_ESCAPE) then
    M.pause_off()
    return
  end

  if option_mode then
    -- 选项面板
    -- BGM 音量滑块 (左右键调整)
    if dse.app.get_key(263) then  -- Left
      M._vol_bgm = clamp((M._vol_bgm or 0.7) - dt * 0.5, 0, 1)
    elseif dse.app.get_key(262) then  -- Right
      M._vol_bgm = clamp((M._vol_bgm or 0.7) + dt * 0.5, 0, 1)
    end
    -- 更新滑块条
    if pause_menu.bgm_bar_fg then
      dse.ui.set_size(pause_menu.bgm_bar_fg, 160 * (M._vol_bgm or 0.7), 12)
      dse.ui.set_position(pause_menu.bgm_bar_fg, -80 + 80 * (M._vol_bgm or 0.7), 60)
    end

    -- FOV +/- 按钮
    if dse.ui.is_pressed(pause_menu.fov_minus) then
      M._cam_fov = math.max(0, (M._cam_fov or 0) - 1)
      dse.ui.set_label_text(pause_menu.fov_value, string.format("x %d", M._cam_fov))
      if M.on_fov_change then M.on_fov_change(M._cam_fov) end
    elseif dse.ui.is_pressed(pause_menu.fov_plus) then
      M._cam_fov = math.min(5, (M._cam_fov or 0) + 1)
      dse.ui.set_label_text(pause_menu.fov_value, string.format("x %d", M._cam_fov))
      if M.on_fov_change then M.on_fov_change(M._cam_fov) end
    end

    -- 返回按钮
    if dse.ui.is_pressed(pause_menu.back_btn) then
      M._option_off()
    end
  else
    -- 暂停菜单按钮
    if dse.ui.is_pressed(pause_menu.resume_btn) then
      M.pause_off()
    elseif dse.ui.is_pressed(pause_menu.option_btn) then
      M._option_on()
    elseif dse.ui.is_pressed(pause_menu.quit_btn) then
      M.pause_off()
      if M.on_quit then M.on_quit() end
    end
  end
end

-- ============================================================================
-- 结算界面更新 (UI_Ingame.cs → ShowTxt + Stagefinish)
-- ============================================================================
function M._update_result(dt)
  result_timer = result_timer + dt

  -- R 键继续
  local KEY_R = 82
  if dse.app.get_key_down(KEY_R) and result_timer > 1.0 then
    M.hide_result()
    if result_mode == "clear" or result_mode == "win" then
      if M.on_stage_continue then M.on_stage_continue() end
    elseif result_mode == "game_over" then
      if M.on_restart then M.on_restart() end
    elseif result_mode == "wave_clear" then
      if M.on_wave_continue then M.on_wave_continue() end
    end
  end
end

-- ============================================================================
-- 复活界面更新 (UI_Ingame_GUI.cs → chance)
-- ============================================================================
function M._update_chance(dt)
  chance_timer = chance_timer + dt
  if chance_timer >= 0.5 then
    chance_timer = 0
    chance_count = chance_count - 1
    dse.ui.set_label_text(chance_ui.count_text, tostring(chance_count))

    if chance_count <= 0 then
      -- 超时, 放弃
      M.hide_chance()
      if M.on_chance_fail then M.on_chance_fail() end
    end
  end

  -- 更新玉石数量
  dse.ui.set_label_text(chance_ui.jade_cost_text,
    string.format("Cost: %d Jade", M._require_jade or 1))
  dse.ui.set_label_text(chance_ui.jade_have_text,
    string.format("(Have: %d)", G.jade or 0))

  -- 按钮检测
  if dse.ui.is_pressed(chance_ui.revive_btn) then
    if (G.jade or 0) >= (M._require_jade or 1) then
      -- 复活成功
      G.jade = (G.jade or 0) - (M._require_jade or 1)
      M._require_jade = (M._require_jade or 1) * 2  -- 下次更贵
      M.hide_chance()
      if M.on_revive then M.on_revive() end
    end
  elseif dse.ui.is_pressed(chance_ui.quit_btn) then
    M.hide_chance()
    if M.on_chance_fail then M.on_chance_fail() end
  end
end

-- ============================================================================
-- 公开接口 (供 main.lua 调用)
-- ============================================================================

-- 显示暂停菜单
function M.pause_on()
  if paused then return end
  paused = true
  G.time_scale = 0  -- C# Time.timeScale = 0
  -- 显示遮罩和按钮
  ui_set_visible(pause_menu.bg, true)
  ui_set_visible(pause_menu.resume_btn, true)
  ui_set_visible(pause_menu.resume_label, true)
  ui_set_visible(pause_menu.option_btn, true)
  ui_set_visible(pause_menu.option_label, true)
  ui_set_visible(pause_menu.quit_btn, true)
  ui_set_visible(pause_menu.quit_label, true)
  if M.on_pause then M.on_pause() end
end

-- 关闭暂停菜单
function M.pause_off()
  if not paused then return end
  paused = false
  G.time_scale = 1.0
  -- 隐藏所有暂停菜单元素
  ui_set_visible(pause_menu.bg, false)
  ui_set_visible(pause_menu.resume_btn, false)
  ui_set_visible(pause_menu.resume_label, false)
  ui_set_visible(pause_menu.option_btn, false)
  ui_set_visible(pause_menu.option_label, false)
  ui_set_visible(pause_menu.quit_btn, false)
  ui_set_visible(pause_menu.quit_label, false)
  M._option_off()
  if M.on_resume then M.on_resume() end
end

-- ESC 键切换暂停 (C# UI_Ingame_GUI 暂停开关)
function M.toggle_pause()
  if paused then
    M.pause_off()
  else
    M.pause_on()
  end
end

-- 开启选项面板
function M._option_on()
  option_mode = true
  -- 隐藏暂停按钮, 显示选项
  ui_set_visible(pause_menu.resume_btn, false)
  ui_set_visible(pause_menu.resume_label, false)
  ui_set_visible(pause_menu.option_btn, false)
  ui_set_visible(pause_menu.option_label, false)
  ui_set_visible(pause_menu.quit_btn, false)
  ui_set_visible(pause_menu.quit_label, false)
  -- 显示选项面板
  ui_set_visible(pause_menu.option_bg, true)
  ui_set_visible(pause_menu.option_title, true)
  ui_set_visible(pause_menu.bgm_label, true)
  ui_set_visible(pause_menu.bgm_bar_bg, true)
  ui_set_visible(pause_menu.bgm_bar_fg, true)
  ui_set_visible(pause_menu.master_label, true)
  ui_set_visible(pause_menu.master_bar_bg, true)
  ui_set_visible(pause_menu.master_bar_fg, true)
  ui_set_visible(pause_menu.fov_label, true)
  ui_set_visible(pause_menu.fov_minus, true)
  ui_set_visible(pause_menu.fov_plus, true)
  ui_set_visible(pause_menu.fov_value, true)
  ui_set_visible(pause_menu.back_btn, true)
  ui_set_visible(pause_menu.back_label, true)
  -- 初始化值
  dse.ui.set_label_text(pause_menu.fov_value, string.format("x %d", M._cam_fov or 0))
end

-- 关闭选项面板
function M._option_off()
  option_mode = false
  ui_set_visible(pause_menu.option_bg, false)
  ui_set_visible(pause_menu.option_title, false)
  ui_set_visible(pause_menu.bgm_label, false)
  ui_set_visible(pause_menu.bgm_bar_bg, false)
  ui_set_visible(pause_menu.bgm_bar_fg, false)
  ui_set_visible(pause_menu.master_label, false)
  ui_set_visible(pause_menu.master_bar_bg, false)
  ui_set_visible(pause_menu.master_bar_fg, false)
  ui_set_visible(pause_menu.fov_label, false)
  ui_set_visible(pause_menu.fov_minus, false)
  ui_set_visible(pause_menu.fov_plus, false)
  ui_set_visible(pause_menu.fov_value, false)
  ui_set_visible(pause_menu.back_btn, false)
  ui_set_visible(pause_menu.back_label, false)
  -- 恢复暂停按钮
  ui_set_visible(pause_menu.resume_btn, true)
  ui_set_visible(pause_menu.resume_label, true)
  ui_set_visible(pause_menu.option_btn, true)
  ui_set_visible(pause_menu.option_label, true)
  ui_set_visible(pause_menu.quit_btn, true)
  ui_set_visible(pause_menu.quit_label, true)
end

-- 显示结算界面
-- type: "clear" | "game_over" | "wave_clear" | "win"
-- data: { kills=, max_combo=, time=, score=, stars= }
function M.show_result(rtype, data)
  result_mode = rtype
  result_timer = 0
  data = data or {}

  -- 显示背景和标题
  ui_set_visible(result_ui.bg, true)
  ui_set_visible(result_ui.title, true)

  local title_text = ""
  if rtype == "clear" then
    title_text = "STAGE CLEAR!"
  elseif rtype == "win" then
    title_text = "VICTORY!"
  elseif rtype == "game_over" then
    title_text = "GAME OVER"
  elseif rtype == "wave_clear" then
    title_text = "WAVE CLEAR!"
  end
  dse.ui.set_label_text(result_ui.title, title_text)

  -- 星级评价 (无限模式)
  if rtype == "wave_clear" and data.stars then
    for i = 1, 3 do
      ui_set_visible(result_ui.stars[i], true)
      if i <= data.stars then
        dse.ui.set_color(result_ui.stars[i], 1.0, 0.85, 0.3, 1.0)
      else
        dse.ui.set_color(result_ui.stars[i], 0.3, 0.3, 0.3, 0.5)
      end
    end
  else
    for i = 1, 3 do
      ui_set_visible(result_ui.stars[i], false)
    end
  end

  -- 统计信息
  ui_set_visible(result_ui.kill_label, true)
  ui_set_visible(result_ui.kill_value, true)
  dse.ui.set_label_text(result_ui.kill_value, tostring(data.kills or G.totalkill or 0))

  ui_set_visible(result_ui.combo_label, true)
  ui_set_visible(result_ui.combo_value, true)
  dse.ui.set_label_text(result_ui.combo_value, tostring(data.max_combo or G.combo_max or 0))

  ui_set_visible(result_ui.time_label, true)
  ui_set_visible(result_ui.time_value, true)
  local t = data.time or play_time
  local mins = math.floor(t / 60)
  local secs = math.floor(t) % 60
  dse.ui.set_label_text(result_ui.time_value, string.format("%d:%02d", mins, secs))

  ui_set_visible(result_ui.score_label, true)
  ui_set_visible(result_ui.score_value, true)
  if data.score then
    -- 分数动画
    score_anim_cur = 0
    score_anim_target = data.score
    score_anim_timer = 2.0
  else
    dse.ui.set_label_text(result_ui.score_value, tostring(G.score or 0))
  end

  -- 继续按钮
  ui_set_visible(result_ui.continue_btn, true)
  ui_set_visible(result_ui.continue_label, true)
  if rtype == "game_over" then
    dse.ui.set_label_text(result_ui.continue_label, "Press R to Restart")
  else
    dse.ui.set_label_text(result_ui.continue_label, "Press R to Continue")
  end
end

-- 隐藏结算界面
function M.hide_result()
  result_mode = nil
  ui_set_visible(result_ui.bg, false)
  ui_set_visible(result_ui.title, false)
  for i = 1, 3 do ui_set_visible(result_ui.stars[i], false) end
  ui_set_visible(result_ui.kill_label, false)
  ui_set_visible(result_ui.kill_value, false)
  ui_set_visible(result_ui.combo_label, false)
  ui_set_visible(result_ui.combo_value, false)
  ui_set_visible(result_ui.time_label, false)
  ui_set_visible(result_ui.time_value, false)
  ui_set_visible(result_ui.score_label, false)
  ui_set_visible(result_ui.score_value, false)
  ui_set_visible(result_ui.continue_btn, false)
  ui_set_visible(result_ui.continue_label, false)
end

-- 显示复活界面
function M.show_chance()
  chance_mode = true
  chance_count = 10
  chance_timer = 0
  G.time_scale = 0.5  -- C# Time.timeScale = 0.5
  ui_set_visible(chance_ui.bg, true)
  ui_set_visible(chance_ui.count_text, true)
  ui_set_visible(chance_ui.prompt_text, true)
  ui_set_visible(chance_ui.jade_cost_text, true)
  ui_set_visible(chance_ui.jade_have_text, true)
  ui_set_visible(chance_ui.revive_btn, true)
  ui_set_visible(chance_ui.revive_label, true)
  ui_set_visible(chance_ui.quit_btn, true)
  ui_set_visible(chance_ui.quit_label, true)
  dse.ui.set_label_text(chance_ui.count_text, "10")
end

-- 隐藏复活界面
function M.hide_chance()
  chance_mode = false
  G.time_scale = 1.0
  ui_set_visible(chance_ui.bg, false)
  ui_set_visible(chance_ui.count_text, false)
  ui_set_visible(chance_ui.prompt_text, false)
  ui_set_visible(chance_ui.jade_cost_text, false)
  ui_set_visible(chance_ui.jade_have_text, false)
  ui_set_visible(chance_ui.revive_btn, false)
  ui_set_visible(chance_ui.revive_label, false)
  ui_set_visible(chance_ui.quit_btn, false)
  ui_set_visible(chance_ui.quit_label, false)
end

-- ── HP/SP 状态更新接口 (供 main.lua 直接调用) ──────────────────────────
-- C# UI_Ingame.StatUpdate_hp / StatUpdate_sp
function M.stat_update_hp(hp, maxhp)
  if not gauges.hp then return end
  local ratio = maxhp > 0 and math.max(0, hp) / maxhp or 0
  set_gauge_fill(gauges.hp, ratio)
  if ui.hp_text then
    dse.ui.set_label_text(ui.hp_text, string.format("HP %d/%d", math.max(0, hp), maxhp))
  end
end

function M.stat_update_sp(sp, maxsp)
  if not gauges.sp then return end
  local ratio = maxsp > 0 and math.max(0, sp) / maxsp or 0
  set_gauge_fill(gauges.sp, ratio)
  if ui.sp_text then
    dse.ui.set_label_text(ui.sp_text, string.format("SP %d/%d", math.floor(sp), maxsp))
  end
  -- SP 不足图标 (C# icon_shortsp)
  if ui.sp_warn then
    ui_set_visible(ui.sp_warn, sp < 10)
  end
end

-- ── 连击更新 (C# UI_Ingame.ComboPlus) ──────────────────────────────────
function M.combo_plus(amount)
  combo_fill = combo_fill + amount
  if combo_fill >= 0.5 and not combo_super then
    combo_super = true
    set_gauge_color(gauges.combo, 1.0, 0.5, 0.1)
    if M.on_super_mode then M.on_super_mode() end
  end
end

-- ── 超级模式触发 (C# UI_Ingame.SuperModeOn) ────────────────────────────
function M.super_mode_on()
  combo_super = false
  combo_fill = 0
  set_gauge_fill(gauges.combo, 0)
  set_gauge_color(gauges.combo, 1.0, 0.85, 0.3)
end

-- ── 蓄力开始 (C# UI_Ingame.PowerCharge) ────────────────────────────────
function M.power_charge()
  charge_visible = true
  charge_value = 0
  set_gauge_visible(gauges.power, true)
end

-- ── 蓄力更新 (C# UI_Ingame.GrabCharge) ─────────────────────────────────
function M.grab_charge()
  charge_visible = true
  charge_value = charge_value + 0.02
  set_gauge_visible(gauges.power, true)
end

-- ── 蓄力取消 (C# UI_Ingame.ResetPower) ─────────────────────────────────
function M.reset_power()
  charge_visible = false
  charge_value = 0
  set_gauge_visible(gauges.power, false)
end

-- ── 获取经验 (C# UI_Ingame.GetExp) ─────────────────────────────────────
function M.get_exp()
  -- 更新经验条
  if gauges.exp then
    local exp_max = Player.level * 100
    local exp_ratio = exp_max > 0 and Player.exp / exp_max or 0
    local fh = gauges.exp.h * exp_ratio
    dse.ui.set_size(gauges.exp.fg, gauges.exp.w, fh)
    dse.ui.set_position(gauges.exp.fg, gauges.exp.x, gauges.exp.y - gauges.exp.h * 0.5 + fh * 0.5)
  end
end

-- ── 升级特效 (C# pt_levelup 实例化) ────────────────────────────────────
function M.show_level_up(lv)
  if not level_up_text then
    level_up_text = make_text("LEVEL UP!", 0, 150, 1.0, 1.0, 0.3, 28, 36)
  end
  dse.ui.set_label_text(level_up_text, string.format("LEVEL %d!", lv))
  ui_set_visible(level_up_text, true)
  level_up_timer = 2.0
  -- 更新等级显示
  if ui.exp_text then
    dse.ui.set_label_text(ui.exp_text, string.format("LV %d", lv))
  end
end

-- ── 魂力获取 (C# UI_Ingame.GainSoul) ───────────────────────────────────
function M.gain_soul(amount)
  -- 更新魂力条
  if gauges.soul then
    local soul_ratio = (G.soul or 0) / 8
    set_gauge_fill(gauges.soul, soul_ratio)
  end
  if ui.soul_text then
    dse.ui.set_label_text(ui.soul_text, string.format("Soul %d/8", G.soul or 0))
  end
end

-- ── 金币获取 (C# UI_Ingame.GainCoin) ───────────────────────────────────
function M.gain_coin(amount)
  G.coin = (G.coin or 0) + amount
  if ui.coin_text then
    dse.ui.set_label_text(ui.coin_text, string.format("Coin %d  Jade %d", G.coin or 0, G.jade or 0))
  end
end

-- ── 玉石获取 (C# UI_Ingame.GainJade) ───────────────────────────────────
function M.gain_jade(amount)
  G.jade = (G.jade or 0) + amount
  if ui.coin_text then
    dse.ui.set_label_text(ui.coin_text, string.format("Coin %d  Jade %d", G.coin or 0, G.jade or 0))
  end
end

-- ── 受伤闪烁 (C# UI_Ingame.Damaged) ────────────────────────────────────
function M.damaged()
  -- HP 条闪烁效果
  if gauges.hp then
    set_gauge_color(gauges.hp, 1.0, 0.05, 0.05)
    -- 延迟恢复颜色 (通过 update 中的低血量检测自动恢复)
  end
end

-- ── 武将切换 HP 条缩放 (C# gauge_scalechange) ──────────────────────────
function M.general_switch(on)
  gauge_scale_change = true
  gauge_scale_target = on and -1.0 or 1.0
  gauge_scale_current = on and 1.0 or -1.0
end

-- ── 关卡完成 (C# UI_Ingame.Stagefinish) ────────────────────────────────
function M.stage_finish()
  return_map = true
  finish_delay = 0
end

-- ── 波次完成 (C# UI_Ingame.WaveClear) ──────────────────────────────────
function M.wave_clear()
  -- SP 恢复满
  if M.on_sp_charge then M.on_sp_charge(100) end
end

-- ── 设置倒计时 (C# playtime) ───────────────────────────────────────────
function M.set_playtime(seconds)
  play_time = seconds
end

-- ── 获取关卡数据 ────────────────────────────────────────────────────────
function M._get_stage_data()
  local DB = require("database")
  if DB.DB_Stage and DB.DB_Stage[G.stage_index] then
    return DB.DB_Stage[G.stage_index]
  end
  return nil
end

-- ── 天使通知 (C# UI_Ingame.GetAngel) ────────────────────────────────────
function M.show_angel_notice(index)
  if not ui.angel_notice then
    ui.angel_notice = make_text("", 0, 100, 1.0, 0.9, 0.4, 24, 32)
  end
  dse.ui.set_label_text(ui.angel_notice, string.format("Angel %d Acquired!", index))
  ui_set_visible(ui.angel_notice, true)
  -- 2.4 秒后隐藏 (通过计时器)
  ui.angel_notice_timer = 2.4
end

-- ── 暂停状态查询 ────────────────────────────────────────────────────────
function M.is_paused() return paused end
function M.is_chance() return chance_mode end
function M.is_result() return result_mode ~= nil end

-- ── 技能图标移动 (C# Icon_Skill.SkillIcon_Move) ─────────────────────────
function M.skill_icon_move(move)
  -- 武将召唤时技能图标移动
  for i = 1, 6 do
    if skill_icons[i] then
      local x = move and 480 or 540
      dse.ui.set_position(skill_icons[i].bg, x, skill_icons[i].y)
      dse.ui.set_position(skill_icons[i].icon, x, skill_icons[i].y)
      dse.ui.set_position(skill_icons[i].cd_bg, x, skill_icons[i].y)
      dse.ui.set_position(skill_icons[i].cd_fg, x, skill_icons[i].y)
      dse.ui.set_position(skill_icons[i].label, x, skill_icons[i].y)
      dse.ui.set_position(skill_icons[i].cost, x - 10, skill_icons[i].y + 12)
    end
  end
end

-- ── 清空所有 UI 实体 ────────────────────────────────────────────────────
function M.clear()
  -- 销毁所有 gauge
  for _, g in pairs(gauges) do
    if type(g) == "table" then
      kill_ui(g.bg)
      kill_ui(g.fg)
    end
  end
  gauges = {}

  -- 销毁所有 UI 文本/按钮
  for _, e in pairs(ui) do
    if type(e) == "number" then kill_ui(e)
    elseif type(e) == "table" then
      for _, ee in pairs(e) do
        if type(ee) == "number" then kill_ui(ee) end
      end
    end
  end
  ui = {}

  -- 销毁暂停菜单
  for _, e in pairs(pause_menu) do
    if type(e) == "number" then kill_ui(e) end
  end
  pause_menu = {}

  -- 销毁结算界面
  for _, e in pairs(result_ui) do
    if type(e) == "number" then kill_ui(e)
    elseif type(e) == "table" then
      for _, ee in pairs(e) do kill_ui(ee) end
    end
  end
  result_ui = {}

  -- 销毁复活界面
  for _, e in pairs(chance_ui) do
    if type(e) == "number" then kill_ui(e) end
  end
  chance_ui = {}

  -- 销毁技能图标
  for _, icon in pairs(skill_icons) do
    if type(icon) == "table" then
      kill_ui(icon.bg)
      kill_ui(icon.icon)
      kill_ui(icon.cd_bg)
      kill_ui(icon.cd_fg)
      kill_ui(icon.label)
      kill_ui(icon.cost)
    end
  end
  skill_icons = {}

  -- 销毁升级文字
  if level_up_text then kill_ui(level_up_text) end
  level_up_text = nil

  -- 重置状态
  paused = false
  option_mode = false
  chance_mode = false
  result_mode = nil
  combo_super = false
  combo_fill = 0
  charge_visible = false
  charge_value = 0
  return_map = false
  finish_delay = 0
  play_time = 0
  level_up_timer = 0
end

-- ── ESC 键检测 (在 main.lua 的 Update 中调用) ───────────────────────────
function M.handle_esc()
  local KEY_ESCAPE = 256
  if dse.app.get_key_down(KEY_ESCAPE) then
    if paused then
      M.pause_off()
    elseif chance_mode then
      -- 复活界面不响应 ESC
    elseif not result_mode then
      M.pause_on()
    end
  end
end

-- ============================================================================
-- 模块导出
-- ============================================================================
M.paused = false
return M
