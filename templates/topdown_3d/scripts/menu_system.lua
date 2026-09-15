-- ============================================================================
-- menu_system.lua — 游戏外 UI (Phase 3)
-- 对应 C#: UI_intro(主菜单) / UI_map(世界地图 90 关) / UI_skill(技能商店)
-- 复用 ui_system.lua 导出的 make_quad/make_text/make_button 工具
-- main.lua 通过 mode=menu/map/shop 驱动本模块 update
-- ============================================================================

local State = require("state")
local G, Player = State.G, State.Player
local DB = require("database")
local UISystem = require("ui_system")

local make_quad = UISystem.make_quad
local make_text = UISystem.make_text
local make_button = UISystem.make_button
local ui_set_visible = UISystem.ui_set_visible
local kill_ui = UISystem.kill_ui

local M = {}

-- 回调 (由 main.lua 注入)
M.on_new_game = nil     -- 主菜单「开始新游戏」
M.on_start_stage = nil  -- 地图选择关卡 (idx)
M.on_quit = nil         -- 退出游戏

-- ── 内部状态 ────────────────────────────────────────────────────────────
local screen = nil      -- nil / "intro" / "map" / "shop"
local ui = {}           -- 当前屏幕实体句柄
local ui_buttons = {}   -- 可点击按钮表: {e=实体, data=任意数据}

local function clear()
  for _, e in ipairs(ui.all or {}) do kill_ui(e) end
  ui = { all = {} }
  ui_buttons = {}
end

-- 按钮防抖: 屏幕构建后 0.5s 内不响应点击
-- (is_pressed 是持续按下状态, 新建按钮可能被启动瞬间的鼠标位置误判)
local function buttons_ready()
  return (G.time or 0) >= (ui.ready_at or 0)
end

-- is_pressed/is_hovered 返回 Lua number (0/1) 而非 boolean; 必须显式比较
local function ui_pressed(e)
  return e and dse.ui.is_pressed(e) == 1
end

local function add(e)
  ui.all[#ui.all + 1] = e
end

-- 通用返回按钮
local function make_back(x, y, label)
  local btn = make_button(x, y, 160, 40, 0.3, 0.2, 0.15, 0.9, 970)
  add(btn)
  add(make_text(label, x, y, 1.0, 1.0, 1.0, 16, 20))
  return btn
end

-- 顶部资源条 (金币/玉石/等级)
local function build_status_bar()
  local y = 330
  add(make_text("--- 资源 ---", -620, y, 0.6, 0.6, 0.6, 12, 16))
  ui.coin = make_text("金币 0", -500, y, 1.0, 0.85, 0.2, 16, 20); add(ui.coin)
  ui.jade = make_text("玉石 0", -340, y, 0.3, 1.0, 0.8, 16, 20); add(ui.jade)
  ui.soul = make_text("灵魂 0", -180, y, 0.8, 0.3, 1.0, 16, 20); add(ui.soul)
  ui.level = make_text("LV 1", -20, y, 1.0, 0.85, 0.3, 16, 20); add(ui.level)
  -- 进度: 已通关数
  local cleared = 0
  for _, s in pairs(G.stage_clear or {}) do if s and s > 0 then cleared = cleared + 1 end end
  ui.progress = make_text("进度 0/90", 200, y, 0.7, 0.7, 0.7, 16, 20); add(ui.progress)
  ui.progress_v = make_text(tostring(cleared), 290, y, 0.9, 0.9, 0.4, 16, 20); add(ui.progress_v)
end

local function refresh_status()
  if ui.coin then dse.ui.set_label_text(ui.coin, string.format("金币 %d", G.coin or 0)) end
  if ui.jade then dse.ui.set_label_text(ui.jade, string.format("玉石 %d", G.jade or 0)) end
  if ui.soul then dse.ui.set_label_text(ui.soul, string.format("灵魂 %d", G.soul or 1)) end
  if ui.level then dse.ui.set_label_text(ui.level, string.format("LV %d", Player.level or 1)) end
  if ui.progress_v then
    local cleared = 0
    for _, s in pairs(G.stage_clear or {}) do if s and s > 0 then cleared = cleared + 1 end end
    dse.ui.set_label_text(ui.progress_v, tostring(cleared))
  end
end

-- ============================================================================
-- 主菜单 (UI_intro)
-- ============================================================================
local function build_intro()
  clear()
  ui.ready_at = (G.time or 0) + 0.5
  add(make_quad(0, 0, 1280, 720, 0.05, 0.05, 0.1, 1.0, 960))
  add(make_text("亡灵杀手", 0, 200, 1.0, 0.9, 0.3, 48, 64))
  add(make_text("Topdown 3D Port", 0, 130, 0.6, 0.6, 0.6, 18, 24))
  add(make_text("WASD 移动  J 攻击  K 技能  L 闪避  P 抓取  ESC 暂停", 0, -360, 0.5, 0.5, 0.5, 12, 16))

  -- 开始新游戏
  local new_btn = make_button(0, 40, 260, 48, 0.15, 0.3, 0.15, 0.9, 970)
  add(new_btn)
  add(make_text("开始新游戏", 0, 40, 1.0, 1.0, 1.0, 20, 26))
  ui.new_btn = new_btn

  -- 继续游戏 (有存档进度时)
  local has_save = (G.max_stage_index or 0) > 0
  if not has_save then
    for _, s in pairs(G.stage_clear or {}) do if s and s > 0 then has_save = true break end end
  end
  if has_save then
    local cont_btn = make_button(0, -30, 260, 48, 0.15, 0.2, 0.35, 0.9, 970)
    add(cont_btn)
    add(make_text("继续游戏", 0, -30, 1.0, 1.0, 1.0, 20, 26))
    ui.cont_btn = cont_btn
  end

  -- 退出
  local quit_btn = make_button(0, -100, 260, 48, 0.3, 0.15, 0.15, 0.9, 970)
  add(quit_btn)
  add(make_text("退出", 0, -100, 1.0, 1.0, 1.0, 20, 26))
  ui.quit_btn = quit_btn

  build_status_bar()
  refresh_status()
end

-- ============================================================================
-- 世界地图 (UI_map): 90 关 10列x9行
-- ============================================================================
local function build_map()
  clear()
  ui.ready_at = (G.time or 0) + 0.5
  add(make_quad(0, 0, 1280, 720, 0.06, 0.05, 0.08, 1.0, 960))
  add(make_text("世界地图", 0, 315, 1.0, 0.9, 0.4, 30, 40))
  build_status_bar()

  local unlocked_max = G.max_stage_index or 0
  for idx = 0, 89 do
    local col = idx % 10
    local row = math.floor(idx / 10)
    local x = -470 + col * 104
    local y = 250 - row * 60
    local unlocked = idx <= unlocked_max
    local stars = G.stage_clear[idx] or 0

    local r, g, b = 0.25, 0.3, 0.45
    if not unlocked then
      r, g, b = 0.12, 0.12, 0.14
    elseif stars >= 3 then
      r, g, b = 0.2, 0.5, 0.22
    elseif stars >= 1 then
      r, g, b = 0.3, 0.42, 0.2
    end

    local btn = make_button(x, y, 60, 44, r, g, b, 0.95, 970)
    add(btn)
    local txt = make_text(tostring(idx + 1), x, y + 4, unlocked and 1.0 or 0.5, unlocked and 1.0 or 0.5, unlocked and 1.0 or 0.5, 10, 14)
    add(txt)
    -- 星级标记
    if stars > 0 then
      local star_txt = ""
      for i = 1, stars do star_txt = star_txt .. "*" end
      add(make_text(star_txt, x, y - 16, 1.0, 0.85, 0.2, 8, 12))
    end
    table.insert(ui_buttons, { e = btn, data = { kind = "stage", idx = idx } })
  end

  -- 底部: 技能商店 / 返回主菜单
  local shop_btn = make_button(-200, -320, 180, 40, 0.15, 0.2, 0.3, 0.9, 970)
  add(shop_btn)
  add(make_text("技能商店", -200, -320, 1.0, 1.0, 1.0, 16, 20))
  ui.shop_btn = shop_btn
  local back_btn = make_back(200, -320, "返回")
  ui.back_btn = back_btn

  refresh_status()
end

-- ============================================================================
-- 技能商店 (UI_skill)
-- ============================================================================
local function build_shop()
  clear()
  ui.ready_at = (G.time or 0) + 0.5
  add(make_quad(0, 0, 1280, 720, 0.06, 0.05, 0.08, 1.0, 960))
  add(make_text("技能商店", 0, 315, 1.0, 0.9, 0.4, 30, 40))
  build_status_bar()

  -- 6 个已装备技能槽
  for slot = 1, 6 do
    local set = Player.skill_slots[slot]
    local grade = Player.skill_grades[set] or 0
    local skill = DB.DB_Skill[set] and DB.DB_Skill[set][grade]
    local y = 230 - (slot - 1) * 92

    local name = "技能 " .. tostring(set)
    if skill and skill.name then name = DB.SkillNames[skill.name] or name end
    add(make_text(string.format("%d. %s", slot, name), -420, y, 1.0, 1.0, 0.9, 18, 24))
    add(make_text(string.format("Lv.%d", grade + 1), -120, y, 1.0, 0.85, 0.3, 16, 20))

    if grade < 4 then
      local next_skill = DB.DB_Skill[set] and DB.DB_Skill[set][grade + 1]
      local price = next_skill and next_skill.price or 0
      local pricekind = next_skill and next_skill.pricekind or 0
      local price_tag = (pricekind == 1) and (price .. " 玉") or (price .. " 金")
      local btn = make_button(120, y, 140, 40, 0.15, 0.3, 0.15, 0.9, 970)
      add(btn)
      add(make_text("升级 " .. price_tag, 120, y, 1.0, 1.0, 1.0, 12, 16))
      table.insert(ui_buttons, { e = btn, data = { kind = "upgrade", set = set } })
    else
      add(make_text("MAX", 120, y, 0.7, 0.7, 0.7, 14, 18))
    end

    -- 技能描述
    if skill and skill.info then
      local info_name = DB.SkillNames[skill.info] or ""
      add(make_text("   " .. info_name, -420, y - 22, 0.6, 0.6, 0.6, 11, 14))
    end
  end

  local back_btn = make_back(0, -320, "返回")
  ui.back_btn = back_btn
  refresh_status()
end

-- ============================================================================
-- 公开接口
-- ============================================================================
function M.show_intro()
  screen = "intro"
  build_intro()
end

function M.show_map()
  screen = "map"
  build_map()
end

function M.show_shop()
  screen = "shop"
  build_shop()
end

function M.get_screen() return screen end

-- 清理菜单实体 (进入战斗时调用)
function M.clear_all()
  clear()
  screen = nil
end

-- 升级技能 (C# UI_skill: cur_skill_grade[i]++, 扣费)
function M.upgrade_skill(set)
  local grade = Player.skill_grades[set] or 0
  if grade >= 4 then return false end
  local next_skill = DB.DB_Skill[set] and DB.DB_Skill[set][grade + 1]
  if not next_skill then return false end
  -- 等级要求 (C# ss[i,0]._requireLV)
  if Player.level < (next_skill.requireLV or 0) then
    print(string.format("[shop] 等级不足: 需要 Lv.%d", next_skill.requireLV))
    return false
  end
  local price = next_skill.price or 0
  local pricekind = next_skill.pricekind or 0
  if pricekind == 1 then
    if (G.jade or 0) < price then
      print("[shop] 玉石不足")
      return false
    end
    G.jade = G.jade - price
  else
    if (G.coin or 0) < price then
      print("[shop] 金币不足")
      return false
    end
    G.coin = G.coin - price
  end
  Player.skill_grades[set] = grade + 1
  print(string.format("[shop] 技能集 %d 升至 Lv.%d", set, grade + 2))
  build_shop()  -- 刷新显示
  return true
end

-- 通关记录 (C# UI_map 通关 + UI_result 奖励)
-- 返回: stars, getcoin, getexp
function M.record_stage_clear(stage_idx, hp_ratio)
  stage_idx = stage_idx or 0
  hp_ratio = hp_ratio or 1.0
  -- 星级: 通关=1, 剩余HP>30%=2, >60%=3 (3 个 mission 的简化)
  local stars = 1
  if hp_ratio > 0.3 then stars = 2 end
  if hp_ratio > 0.6 then stars = 3 end
  local old = G.stage_clear[stage_idx] or 0
  if stars > old then G.stage_clear[stage_idx] = stars end
  -- 解锁下一关 (C# max_stage_index++)
  if (G.max_stage_index or 0) < stage_idx + 1 then
    G.max_stage_index = stage_idx + 1
  end
  -- 奖励 (C# UI_result.Start: getcoin = idx*2+100, getexp = (idx+5)*20)
  local getcoin = stage_idx * 2 + 100
  local getexp = (stage_idx + 5) * 20
  G.coin = (G.coin or 0) + getcoin
  print(string.format("[stage] 通关 %d: %d星 金币+%d 经验+%d", stage_idx, stars, getcoin, getexp))
  return stars, getcoin, getexp
end

-- ============================================================================
-- 更新 (main.lua 在 menu/map/shop 模式调用)
-- ============================================================================
function M.update(dt)
  -- 按钮防抖 (新建屏幕 0.5s 内不响应)
  if not buttons_ready() then return end

  -- ESC 返回上级 (地图→主菜单, 商店→地图)
  if dse.app.get_key_down(256) then
    if screen == "map" then
      M.show_intro()
    elseif screen == "shop" then
      M.show_map()
    end
  end

  if screen == "intro" then
    if ui_pressed(ui.new_btn) then
      if M.on_new_game then M.on_new_game() end
    elseif ui_pressed(ui.cont_btn) then
      M.show_map()
    elseif ui_pressed(ui.quit_btn) then
      if M.on_quit then M.on_quit() end
    end
  elseif screen == "map" then
    for _, b in ipairs(ui_buttons) do
      if b.data.kind == "stage" and ui_pressed(b.e) then
        local idx = b.data.idx
        if idx <= (G.max_stage_index or 0) then
          G.current_stage = idx
          if M.on_start_stage then M.on_start_stage(idx) end
        end
        break
      end
    end
    if ui_pressed(ui.shop_btn) then
      M.show_shop()
    elseif ui_pressed(ui.back_btn) then
      M.show_intro()
    end
  elseif screen == "shop" then
    for _, b in ipairs(ui_buttons) do
      if b.data.kind == "upgrade" and ui_pressed(b.e) then
        M.upgrade_skill(b.data.set)
        break
      end
    end
    if ui_pressed(ui.back_btn) then
      M.show_map()
    end
  end
end

return M
