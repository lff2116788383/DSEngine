-- ============================================================================
-- main.lua  青溪问剑（HD-2D 武侠模板）
--   WASD/方向键 移动    J 三段连招    K 闪避(无敌帧)    U 分花拂柳    I 紫霞真气
--   1/2 用药    E 交互    Tab 背包    Esc 暂停    R 重开
-- ============================================================================
local core = require("core")
local A = require("assets")
local D = require("data")
local W = require("world")
local F = require("fx")
local UI = require("ui")
local P = require("player")
local E = require("enemy")
local AU = require("audio")
local Save = require("save")
local B = require("bplus")

local MAPS = require("mapdata")
local G = {
    state = "title", map_id = "village", quest_index = 1, kills = {},
    alive_prev = 0, fade_t = 0, fade_dir = 0, title_sel = 1, pause_sel = 1,
}
local npcs, pickups = {}, {}
local ENTRY = {
    village = { default = nil, north = { x = 20.5, y = 2.6 }, south = { x = 20.5, y = 25.4 } },
    stronghold = { default = nil, north = { x = 21.5, y = 4.2 }, south = { x = 21.5, y = 26.4 } },
}

local function find_map(id)
    for _, m in ipairs(MAPS) do if m.id == id then return m end end
    return MAPS[1]
end

local function quest()
    return D.quests[G.quest_index]
end

local function update_quest_progress()
    local q = quest()
    if not q then return end
    if q.target == "kill_stronghold" then
        local n = G.kills.stronghold or 0
        G.quest = { title = q.title, progress = string.format("%d/%d", math.min(n, q.count), q.count) }
        if n >= q.count then
            G.quest_index = 3
            F.toast("任务完成：救出村民", 0.6, 1.0, 0.6)
            AU.sfx("levelup", 0.5)
        end
    else
        G.quest = { title = q.title, progress = "" }
    end
end

local function clear_scene()
    for _, n in ipairs(npcs) do pcall(dse.ecs.destroy_entity, n.e) end
    for _, p in ipairs(pickups) do pcall(dse.ecs.destroy_entity, p.e) end
    npcs, pickups = {}, {}
    E.clear()
end

local function spawn_npc(n)
    local id = n.id
    local w, h = A.size_of(id)
    local e = dse.ecs.create_entity()
    dse.ecs.add_transform(e, n.x, n.y + h / 64.0, 0, w / 32, h / 32, 1)
    if B.enabled then
        B.register_actor(e, id, n.dir or "d", "idle", w, h)
    else
        dse.ecs.add_sprite(e, 1, 1, 1, 1, 300, A.frames_for(id, n.dir or "d", "idle")[1])
        dse.ecs.add_animator(e)
        dse.ecs.add_animation_state(e, "idle", 4, true, A.frames_for(id, n.dir or "d", "idle"))
        dse.ecs.play_animation(e, "idle")
    end
    local marker = nil
    if n.dialog == "elder" or n.dialog == "villager" then
        marker = dse.ecs.create_entity()
        dse.ecs.add_transform(marker, n.x, n.y + h / 32 + 0.9, 0, 0.16, 0.16, 1)
        if B.enabled then
            B.add_marker(marker, A.tex.quest_marker, 0.16 * 32, 0.16 * 32)
        else
            dse.ecs.add_sprite(marker, 1, 1, 1, 1, 400, A.tex.quest_marker)
        end
    end
    local rec = { e = e, marker = marker, x = n.x, y = n.y, id = id, dialog = n.dialog }
    npcs[#npcs + 1] = rec
    return rec
end

local function spawn_pickup(it)
    local icon = (it.kind == "coin") and A.tex.pickup_coin or
                 (it.kind == "hp_potion") and A.tex.pickup_hp or A.tex.pickup_mp
    local e = dse.ecs.create_entity()
    dse.ecs.add_transform(e, it.x, it.y + 0.35, 0, 0.42, 0.42, 1)
    if B.enabled then
        B.add_pickup(e, icon, 0.42 * 32, 0.42 * 32)
    else
        dse.ecs.add_sprite(e, 1, 1, 1, 1, 350, icon)
    end
    local rec = { e = e, x = it.x, y = it.y, kind = it.kind, t = math.random() * 6.28 }
    pickups[#pickups + 1] = rec
    return rec
end

local function spawn_level(map_id, entry)
    local map = find_map(map_id)
    G.map_id = map_id
    clear_scene()
    W.load_map(map)
    for _, n in ipairs(map.npcs or {}) do spawn_npc(n) end
    for _, it in ipairs(map.items or {}) do spawn_pickup(it) end
    for _, en in ipairs(map.enemies or {}) do E.spawn(en.kind, en.x, en.y, en.patrol, map_id) end
    if map.boss then E.spawn(map.boss.kind, map.boss.x, map.boss.y, map.boss.patrol, map_id) end
    P.spawn(map)
    local ep = entry and ENTRY[map_id] and ENTRY[map_id][entry]
    if ep then P.x, P.y = ep.x, ep.y P.ent = P.ent end
    W.follow(P.ent)
    G.alive_prev = E.count(map_id)
    UI.chapter_banner(map_id == "village" and D.story.map1_title or D.story.map2_title, 3.0)
    AU.bgm(map.music == "boss" and "bgm_boss" or "bgm_field", 0.5)
    F.set_weather(map_id == "village" and "leaf" or "firefly")
    update_quest_progress()
end

local function start_new_game()
    G.quest_index = 1
    G.kills = {}
    P.hp_max, P.mp_max = 130, 60
    P.hp, P.mp, P.stam = 130, 60, 100
    P.level, P.exp, P.exp_next = 1, 0, 45
    P.atk, P.def, P.gold = 12, 5, 0
    P.inv = { hp_potion = 3, mp_potion = 2 }
    P.equip = {}
    P.cd = { fenhua = 0, zixia = 0 }
    G.state = "explore"
    spawn_level("village", "south")
    UI.set_title(false)
    UI.fade_to(0)
end

local function transition_to(map_id, entry)
    G.state = "transition"
    G.fade_dir = 1
    G.fade_t = 0
    G.pending_map, G.pending_entry = map_id, entry
end

--  生命周期 
function Awake()
    A.load(MAPS)
    W.setup_camera()
    UI.init(MAPS[1])
    UI.fade_to(1)
    UI.set_title(true)
    G.title_sel = 1
    UI.hud.title_menu:set("开始新游戏\n读取存档\n退出游戏")
    AU.bgm("bgm_title", 0.5)
    G.state = "title"
    local auto = os and os.getenv and os.getenv("DSE_HD2D_AUTOSTART")
    if auto and auto ~= "" then
        start_new_game()
        G.demo = (os.getenv("DSE_HD2D_DEMO") or "") ~= ""
        G.demo_t = 0
        print("[hd2d] autostart=1 demo=" .. tostring(G.demo))
    end
    print("[hd2d] 青溪问剑 ready. maps=" .. #MAPS)
end

local function title_update(dt)
    local h = UI.hud
    local sel = G.title_sel
    local items = { "开始新游戏", "读取存档", "退出游戏" }
    local txt = ""
    for i, s in ipairs(items) do
        txt = txt .. ((i == sel) and " " or "　 ") .. s .. ((i < #items) and "\n" or "")
    end
    h.title_menu:set(txt)
    if core.key_down(core.KEY.W) or core.key_down(core.KEY.UP) then
        G.title_sel = (sel - 2) % #items + 1
        AU.sfx("ui", 0.4)
    end
    if core.key_down(core.KEY.S) or core.key_down(core.KEY.DOWN) then
        G.title_sel = sel % #items + 1
        AU.sfx("ui", 0.4)
    end
    if core.key_down(core.KEY.J) or core.key_down(core.KEY.ENTER) or core.key_down(core.KEY.SPACE) then
        AU.sfx("ui", 0.6)
        if sel == 1 then
            start_new_game()
        elseif sel == 2 then
            local ok, err = Save.load(G, P)
            if ok then
                start_new_game_loaded()
            else
                F.toast(err or "读取失败", 1.0, 0.6, 0.5)
            end
        else
            dse.app.quit()
        end
    end
end

function start_new_game_loaded()
    G.state = "explore"
    spawn_level(G.map_id, nil)
    UI.set_title(false)
    UI.fade_to(0)
    F.toast("读档成功", 0.7, 1.0, 0.7)
end

local function interact_update(dt)
    -- NPC
    local best, bd = nil, 1.9
    for _, n in ipairs(npcs) do
        local d = core.dist(P.x, P.y, n.x, n.y)
        if d < bd then best, bd = n, d end
    end
    if best then
        UI.prompt("按 E 与 " .. (D.dialogues[best.dialog] and D.dialogues[best.dialog][1].speaker or best.id) .. " 交谈")
        if core.key_down(core.KEY.E) then
            local dlg = D.dialogues[best.dialog]
            UI.open_dialogue(dlg, function()
                if best.dialog == "elder" and G.quest_index == 1 then
                    G.quest_index = 2
                    F.toast("新任务：救出村民", 1.0, 0.9, 0.5)
                    AU.sfx("levelup", 0.5)
                elseif best.dialog == "villager" and G.quest_index < 2 then
                    G.quest_index = 2
                end
                update_quest_progress()
            end)
            G.state = "dialog"
        end
    else
        UI.prompt("")
    end

    -- 拾取
    for i = #pickups, 1, -1 do
        local it = pickups[i]
        if core.dist(P.x, P.y, it.x, it.y) < 0.85 then
            if it.kind == "coin" then
                P.gain_gold(1)
                F.popup("+1 铜钱", it.x, it.y + 0.6, "exp")
            else
                P.add_item(it.kind, 1)
                F.toast("获得 " .. (D.items[it.kind] and D.items[it.kind].name or it.kind) .. " x1", 1.0, 0.92, 0.6)
            end
            AU.random("pickup", 0.6)
            pcall(dse.ecs.destroy_entity, it.e)
            table.remove(pickups, i)
        end
    end

    -- 过关口
    for _, ex in ipairs(W.map.exits or {}) do
        if P.x > ex.x - ex.w / 2 and P.x < ex.x + ex.w / 2 and
           P.y > ex.y - ex.h / 2 and P.y < ex.y + ex.h / 2 then
            transition_to(ex.to, ex.entry)
            AU.sfx("gate", 0.5)
            return
        end
    end
end

local function pause_update(dt)
    local items = { "继续游戏", "保存进度", "读取进度", "退出到标题" }
    local txt = "暂停\n\n"
    for i, s in ipairs(items) do
        txt = txt .. ((i == G.pause_sel) and " " or "　 ") .. s .. ((i < #items) and "\n" or "")
    end
    UI.set_center(txt, "Esc 返回")
    if core.key_down(core.KEY.W) or core.key_down(core.KEY.UP) then
        G.pause_sel = (G.pause_sel - 2) % #items + 1
        AU.sfx("ui", 0.4)
    end
    if core.key_down(core.KEY.S) or core.key_down(core.KEY.DOWN) then
        G.pause_sel = G.pause_sel % #items + 1
        AU.sfx("ui", 0.4)
    end
    if core.key_down(core.KEY.ESC) then
        G.state = "explore"
        UI.set_center("", "")
        AU.sfx("ui", 0.5)
    elseif core.key_down(core.KEY.J) or core.key_down(core.KEY.ENTER) then
        AU.sfx("ui", 0.5)
        local sel = G.pause_sel
        if sel == 1 then
            G.state = "explore"
            UI.set_center("", "")
        elseif sel == 2 then
            local ok, err = Save.save(G, P)
            F.toast(ok and "存档成功" or (err or "存档失败"), 0.7, 1.0, 0.7)
        elseif sel == 3 then
            local ok, err = Save.load(G, P)
            if ok then
                UI.set_center("", "")
                start_new_game_loaded()
            else
                F.toast(err or "读取失败", 1.0, 0.6, 0.5)
            end
        else
            G.state = "title"
            UI.set_center("", "")
            UI.set_title(true)
            AU.bgm("bgm_title", 0.5)
        end
    end
end

local function explore_update(dt)
    if G.demo then
        G.demo_last = G.demo_last or 0
        G.demo_t = G.demo_t + dt
        local t = G.demo_t
        core.demo_attack = false
        core.demo_dodge = false
        if t < 1.6 then
            core.demo = { x = 1, y = 0 }
        elseif t < 2.8 then
            core.demo = nil
            if (t > 1.7 and G.demo_last <= 1.7) or (t > 2.3 and G.demo_last <= 2.3) then
                core.demo_attack = true
            end
        elseif t < 5.0 then
            core.demo = { x = 0.6, y = 0.5 }
            if t > 4.4 and G.demo_last <= 4.4 then core.demo_attack = true end
        else
            core.demo = nil
            if t > 5.6 and G.demo_last <= 5.6 then core.demo_dodge = true end
        end
        G.demo_last = t
    end
    P.update(dt, "play")
    -- 命中结算：把主角这一帧的判定交给敌人
    if P.pending_hit then
        local hit = P.pending_hit
        P.pending_hit = nil
        for _, en in ipairs(E.list) do
            if not en.dead then
                local d = core.dist(P.x, P.y + 0.5, en.x, en.y + 0.4)
                if d <= hit.range + (en.w / 64) then
                    local dir_ok = true
                    if hit.arc < 360 and hit.dir then
                        local dx, dy = en.x - P.x, en.y - P.y
                        local fx_ = (hit.dir == "r" and 1) or (hit.dir == "l" and -1) or 0
                        local fy_ = (hit.dir == "u" and 1) or (hit.dir == "d" and -1) or 0
                        dir_ok = (dx * fx_ + dy * fy_) > -0.2
                    end
                    if dir_ok then
                        E.damage(en, hit.dmg, P.x, P.y, hit.crit)
                        if hit.hitstop then
                            dse.app.set_time_scale(0.08)
                            G.hitstop = hit.hitstop
                        end
                    end
                end
            end
        end
    end
    E.update(dt, P, "play", G.map_id)

    -- 击杀计数  任务
    local alive = E.count(G.map_id)
    if alive < G.alive_prev then
        G.kills[G.map_id] = (G.kills[G.map_id] or 0) + (G.alive_prev - alive)
        update_quest_progress()
    end
    G.alive_prev = alive

    interact_update(dt)
    -- 暂停/背包
    if core.key_down(core.KEY.ESC) then
        G.state = "pause"
        G.pause_sel = 1
        AU.sfx("ui", 0.5)
    end
    if core.key_down(core.KEY.TAB) then
        G.tab_open = not G.tab_open
        if G.tab_open then
            local lines = UI.menu_lines()
            UI.set_center("背　包\n\n" .. table.concat(lines, "\n"), "Tab 关闭")
        else
            UI.set_center("", "")
        end
        AU.sfx("ui", 0.5)
    end
    if P.dead then
        G.state = "dead"
        UI.set_center("游戏结束", "按 R 重新开始")
        AU.sfx("die", 0.8)
    end
end

local function check_boss_victory()
    local boss = nil
    for _, en in ipairs(E.list) do
        if en.kind == "boss" then boss = en end
    end
    if boss and boss.dead and not G.boss_done then
        G.boss_done = true
        UI.open_dialogue(D.dialogues.boss_dead, function()
            G.state = "win"
            UI.set_center("通关！", D.story and "青溪问剑  完" or "")
            AU.bgm("bgm_title", 0.5)
            F.toast("感谢游玩 HD-2D 武侠模板", 1.0, 0.95, 0.7)
        end)
        G.state = "dialog"
    end
end

function Update(dt)
    dt = core.clamp(dt or 0.016, 0.0, 0.05)
    if G.hitstop and G.hitstop > 0 then
        G.hitstop = G.hitstop - dt
        if G.hitstop <= 0 then dse.app.set_time_scale(1.0) end
    end
    -- 过渡淡入淡出
    if G.state == "transition" then
        G.fade_t = G.fade_t + dt
        if G.fade_dir == 1 then
            UI.fade_to(core.clamp(G.fade_t / 0.35, 0, 1))
            if G.fade_t >= 0.35 then
                spawn_level(G.pending_map, G.pending_entry)
                G.state = "transition_in"
                G.fade_t = 0
            end
        end
    elseif G.state == "transition_in" then
        G.fade_t = G.fade_t + dt
        UI.fade_to(core.clamp(1 - G.fade_t / 0.45, 0, 1))
        explore_update(dt)
        if G.fade_t >= 0.45 then
            UI.fade_to(0)
            G.state = "explore"
        end
    elseif G.state == "title" then
        title_update(dt)
    elseif G.state == "explore" then
        explore_update(dt)
        check_boss_victory()
    elseif G.state == "dialog" then
        P.update(dt, "none")
        E.update(dt, P, "none", G.map_id)
        if core.key_down(core.KEY.J) or core.key_down(core.KEY.E) or core.key_down(core.KEY.SPACE)
           or core.key_down(core.KEY.ENTER) then
            UI.dialogue_advance()
            AU.sfx("ui", 0.3)
        end
        if not UI.dialog.active and G.state == "dialog" then
            G.state = "explore"
        end
    elseif G.state == "pause" then
        pause_update(dt)
    elseif G.state == "dead" then
        if core.key_down(core.KEY.R) then
            P.hp = P.hp_max
            P.dead = false
            P.anim = ""
            spawn_level(G.map_id, nil)
            UI.set_center("", "")
            G.state = "explore"
        end
    elseif G.state == "win" then
        if core.key_down(core.KEY.R) then
            G.boss_done = false
            G.state = "title"
            UI.set_title(true)
            UI.set_center("", "")
        end
    end
    -- 全局
    W.update(dt)
    F.update(dt)
    if G.state ~= "title" then UI.update(dt, { map_id = G.map_id, quest = G.quest }) end
end