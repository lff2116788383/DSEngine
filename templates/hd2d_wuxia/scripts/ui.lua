-- ============================================================================
-- ui.lua  HUD / 对话（打字机）/ 背包菜单 / 暂停 / 标题 / 淡入淡出 / 死亡与通关
-- ============================================================================
local core = require("core")
local A = require("assets")
local D = require("data")
local AU = require("audio")

local UI = {}
local ecs = dse.ecs
local W, H = core.SCREEN_W, core.SCREEN_H

UI.fs_tex, UI.fs, UI.fb_tex, UI.fb = nil, nil, nil, nil
UI.hud = {}
UI.dialog = { active = false, lines = nil, idx = 0, t = 0, full = "" }
UI.paused = false
UI.menu_tab = 1
UI.menu_open = false
UI.ents = {}
UI.cooldown_texts = {}

local function q(tex, x, y, w, h, r, g, b, a, order)
    local e = ecs.create_entity()
    ecs.add_transform(e, 0, 0, 0, 1, 1, 1)
    dse.ui.add_renderer(e, tex or 0, r or 1, g or 1, b or 1, a or 1, order or 100000, w or 16, h or 16)
    dse.ui.set_anchor(e, 0, 0)
    dse.ui.set_position(e, x, y)
    UI.ents[#UI.ents + 1] = e
    return e
end
UI.quad = q

local function mk_text(font_data, tex, x, y, r, g, b, order)
    local t = core.Text.new(font_data, tex, x, y, r, g, b, order)
    UI.ents[#UI.ents + 1] = t
    return t
end

function UI.init(map)
    UI.fs = require("font_small")
    UI.fb = require("font_big")
    UI.fs_tex = A.get("ui/font_small.png")
    UI.fb_tex = A.get("ui/font_big.png")
    local h = UI.hud
    -- 左上：生命/内力/体力
    q(A.get("ui/panel_hud.png"), 16 + 160, H - 12 - 42, 320, 84, 1, 1, 1, 0.9, 100)
    q(A.get("ui/bar_frame.png"), 34 + 126, H - 46, 252, 22, 1, 1, 1, 1, 102)
    h.hp = q(A.get("ui/bar_hp.png"), 40, H - 46, 240, 12, 1, 1, 1, 1, 103)
    q(A.get("ui/bar_frame.png"), 34 + 126, H - 70, 252, 22, 1, 1, 1, 1, 102)
    h.mp = q(A.get("ui/bar_mp.png"), 40, H - 70, 240, 12, 1, 1, 1, 1, 103)
    q(A.get("ui/bar_frame.png"), 34 + 106, H - 92, 212, 18, 1, 1, 1, 1, 102)
    h.stam = q(A.get("ui/bar_stam.png"), 40, H - 92, 200, 8, 1, 1, 1, 1, 103)
    h.hp_t = mk_text(UI.fs, UI.fs_tex, 40, H - 68, 1.0, 0.9, 0.9, 104)
    h.mp_t = mk_text(UI.fs, UI.fs_tex, 40, H - 92, 0.85, 0.92, 1.0, 104)
    h.title_t = mk_text(UI.fb, UI.fb_tex, 24, H - 126, 1.0, 0.95, 0.8, 104)
    h.quest_t = mk_text(UI.fs, UI.fs_tex, 24, H - 158, 0.95, 0.9, 0.62, 104)
    h.gold_t = mk_text(UI.fs, UI.fs_tex, W - 250, H - 48, 1.0, 0.88, 0.5, 104)
    h.stat_t = mk_text(UI.fs, UI.fs_tex, W - 250, H - 74, 0.9, 0.94, 1.0, 104)
    h.tip_t = mk_text(UI.fs, UI.fs_tex, 24, 24, 0.85, 0.88, 0.92, 104)
    -- 技能栏
    h.slots = {}
    local sx = W * 0.5 - 2 * 64 + 4
    for i, sk in ipairs(D.skill_bar) do
        local x = sx + (i - 1) * 64
        q(A.get("ui/skill_slot.png"), x + 28, 60, 56, 56, 1, 1, 1, 0.95, 105)
        local icon_path = D.skill_icon[sk.icon] or D.skill_icon[1]
        q(A.get(icon_path), x + 28, 62, 40, 40, 1, 1, 1, 1, 106)
        local key = mk_text(UI.fs, UI.fs_tex, x + 6, 30, 0.95, 0.92, 0.7, 107)
        key:set(sk.key)
        h.slots[i] = { cd = mk_text(UI.fs, UI.fs_tex, x + 10, 56, 1.0, 0.6, 0.5, 107), x = x }
    end
    -- 对话
    h.dlg = { }
    h.dlg.box = q(A.get("ui/panel_dialog.png"), W * 0.5, 118, 768, 176, 1, 1, 1, 0.0, 200)
    h.dlg.name = mk_text(UI.fs, UI.fs_tex, W * 0.5 - 330, 186, 1.0, 0.88, 0.6, 201)
    h.dlg.line1 = mk_text(UI.fs, UI.fs_tex, W * 0.5 - 330, 132, 0.97, 0.97, 0.94, 201)
    h.dlg.line2 = mk_text(UI.fs, UI.fs_tex, W * 0.5 - 330, 100, 0.97, 0.97, 0.94, 201)
    h.dlg.hint = mk_text(UI.fs, UI.fs_tex, W * 0.5 + 240, 44, 0.8, 0.85, 0.9, 201)
    h.dlg.hint:set("J/E 继续")
    UI.dlg_visible(false)
    -- 交互提示
    h.prompt = mk_text(UI.fs, UI.fs_tex, W * 0.5 - 60, 220, 1.0, 0.95, 0.6, 150)
    h.prompt:set("")
    -- 提示行（左上第 1 章标题）
    h.chapter = mk_text(UI.fb, UI.fb_tex, W * 0.5 - 150, H * 0.62, 1.0, 0.94, 0.72, 300)
    h.chapter:set("")
    -- 全屏淡入淡出
    UI.fade = q(0, W * 0.5, H * 0.5, W, H, 0, 0, 0, 0, 999999)
    -- 标题界面
    h.title_plate = q(A.get("ui/title_plate.png"), W * 0.5, H * 0.62, 560, 140, 1, 1, 1, 0.0, 400)
    h.title_big = mk_text(UI.fb, UI.fb_tex, W * 0.5 - 200, H * 0.66, 1.0, 0.93, 0.72, 401)
    h.title_big:set("青溪问剑")
    h.title_sub = mk_text(UI.fs, UI.fs_tex, W * 0.5 - 110, H * 0.58, 0.85, 0.9, 0.95, 401)
    h.title_sub:set("HD-2D 武侠  DSEngine 模板")
    h.title_menu = mk_text(UI.fs, UI.fs_tex, W * 0.5 - 70, H * 0.44, 0.95, 0.95, 0.95, 401)
    h.title_tip = mk_text(UI.fs, UI.fs_tex, W * 0.5 - 260, H * 0.30, 0.8, 0.85, 0.9, 401)
    h.title_tip:set("W/S 选择　J/Enter 确认")
    h.center = mk_text(UI.fb, UI.fb_tex, W * 0.5 - 160, H * 0.55, 1.0, 0.9, 0.6, 600)
    h.center_sub = mk_text(UI.fs, UI.fs_tex, W * 0.5 - 160, H * 0.46, 0.9, 0.92, 0.95, 600)
    UI.hud = h
    UI.set_title(false)
    UI.set_center("", "")
end

local function vis(t, v) if t then t:visible(v) end end

function UI.dlg_visible(v)
    local d = UI.hud.dlg
    dse.ui.set_color(UI.hud.dlg.box, 1, 1, 1, v and 0.96 or 0.0)
    vis(d.name, v); vis(d.line1, v); vis(d.line2, v); vis(d.hint, v)
end

function UI.set_title(v)
    local h = UI.hud
    dse.ui.set_color(h.title_plate, 1, 1, 1, v and 0.92 or 0.0)
    vis(h.title_big, v); vis(h.title_sub, v); vis(h.title_menu, v); vis(h.title_tip, v)
end

function UI.set_center(main, sub)
    local h = UI.hud
    h.center:set(main or "")
    h.center_sub:set(sub or "")
    vis(h.center, (main or "") ~= "")
    vis(h.center_sub, (sub or "") ~= "")
end

function UI.fade_to(alpha) dse.ui.set_color(UI.fade, 0, 0, 0, core.clamp(alpha, 0, 1)) end

function UI.chapter_banner(text, t)
    UI.hud.chapter:set(text or "")
    UI.hud.chapter_until = t or 2.6
end

function UI.set_bar(e, x0, w, ratio)
    ratio = core.clamp(ratio or 0, 0, 1)
    dse.ui.set_size(e, math.max(1, w * ratio), (e == UI.hud.stam) and 8 or 12)
    dse.ui.set_position(e, x0 + w * ratio * 0.5, (e == UI.hud.hp) and (H - 46) or ((e == UI.hud.mp) and (H - 70) or (H - 92)))
end

function UI.open_dialogue(lines, on_done)
    UI.dialog.active = true
    UI.dialog.lines = lines
    UI.dialog.idx = 0
    UI.dialog.t = 0
    UI.dialog.on_done = on_done
    UI.dlg_visible(true)
    UI.next_line()
end

function UI.next_line()
    local d = UI.dialog
    d.idx = d.idx + 1
    local line = d.lines[d.idx]
    if not line then
        UI.close_dialogue()
        return
    end
    d.full = line.text or ""
    d.shown = 0
    d.t = 0
    d.speaker = line.speaker or ""
    UI.hud.dlg.name:set(d.speaker)
end

function UI.close_dialogue()
    UI.dialog.active = false
    UI.dlg_visible(false)
    local cb = UI.dialog.on_done
    UI.dialog.on_done = nil
    if cb then cb() end
end

function UI.update_dialogue(dt)
    local d = UI.dialog
    if not d.active then return end
    local n = utf8.len(d.full) or 0
    d.t = d.t + dt
    local target = math.min(n, math.floor(d.t * 34))
    if target ~= d.shown then
        d.shown = target
        UI.render_dialogue_text(target)
        if target % 3 == 0 then AU.sfx("talk", 0.25) end
    end
end

function UI.render_dialogue_text(shown)
    local d = UI.dialog
    local n = utf8.len(d.full) or 0
    shown = math.min(shown or n, n)
    local per = 21
    local cut = math.min(shown, per)
    UI.hud.dlg.line1:set(core.utf8_sub(d.full, 1, cut))
    if shown > per then
        UI.hud.dlg.line2:set(core.utf8_sub(d.full, per + 1, shown))
    else
        UI.hud.dlg.line2:set("")
    end
end

function UI.dialogue_advance()
    local d = UI.dialog
    local n = utf8.len(d.full) or 0
    if d.shown < n then
        d.shown = n
        d.t = 1000
        UI.render_dialogue_text(n)
    else
        UI.next_line()
    end
end

function UI.update(dt, state)
    local h = UI.hud
    local P = require("player")
    UI.set_bar(h.hp, 28, 240, P.hp / P.hp_max)
    UI.set_bar(h.mp, 28, 240, P.mp / P.mp_max)
    UI.set_bar(h.stam, 34, 200, P.stam / P.stam_max)
    h.hp_t:set(string.format("生命 %d/%d", math.floor(P.hp), P.hp_max))
    h.mp_t:set(string.format("内力 %d/%d", math.floor(P.mp), P.mp_max))
    h.gold_t:set("铜钱 " .. P.gold)
    local atk, def = P.stats()
    h.stat_t:set(string.format("等级 %d　攻击 %d　防御 %d", P.level, atk, def))
    h.title_t:set(D.story and (state.map_id == "village" and D.story.map1_title or D.story.map2_title) or "")
    if state.quest then
        h.quest_t:set("任务：" .. state.quest.title .. "（" .. (state.quest.progress or "") .. "）")
    end
    h.tip_t:set(D.tips[1] or "")
    -- 技能冷却
    for i, sk in ipairs(D.skill_bar) do
        local slot = h.slots[i]
        local id = sk.id
        local cd = (id == "fenhua" or id == "zixia") and (P.cd[id] or 0) or 0
        if id == "dodge" then cd = (P.stam < 20) and 1 or 0 end
        slot.cd:set(cd > 0 and string.format("%.1f", cd) or "")
    end
    if h.chapter_until then
        h.chapter_until = h.chapter_until - dt
        if h.chapter_until <= 0 then
            h.chapter:set("")
            h.chapter_until = nil
        end
    end
    UI.update_dialogue(dt)
end

function UI.prompt(text)
    UI.hud.prompt:set(text or "")
end

-- 暂停 / 背包面板（Tab）
function UI.menu_lines()
    local P = require("player")
    local lines = {}
    for _, k in ipairs({ "hp_potion", "mp_potion", "iron_sword", "leather_armor", "manual", "boss_token" }) do
        local n = P.inv[k] or 0
        if n > 0 then
            local it = D.items[k]
            lines[#lines + 1] = string.format("%s x%d  %s", it.name, n, it.desc or "")
        end
    end
    if #lines == 0 then lines[1] = "（背包是空的）" end
    return lines
end

return UI