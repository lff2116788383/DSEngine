-- ============================================================================
-- DSEngine 3D Top-Down Action Template — 三国题材俯视角动作游戏
-- ----------------------------------------------------------------------------
-- 操作：
--   W A S D（或方向键）移动武将
--   J / 鼠标左键  普通攻击（挥砍）
--   K / 鼠标右键  技能（范围攻击）
--   R 重开当前关卡
-- 玩法：控制武将在地图上战斗，消灭所有怪物即可过关；
--       怪物会掉落宝物（拾取加分）；血条 HUD、伤害飘字、BGM/SFX 完整闭环。
--
-- 模型来自 Unity 逆向解包资源（glb 格式），运行时通过引擎新增的 .glb 加载
-- 管线直接载入（无需 AssetBuilder 预转换 .dmesh）。
-- ============================================================================

local app = dse.app

-- ── 键码（GLFW）─────────────────────────────────────────────────────────────
local KEY_A, KEY_D, KEY_W, KEY_S = 65, 68, 87, 83
local KEY_LEFT, KEY_RIGHT, KEY_UP, KEY_DOWN = 263, 262, 265, 264
local KEY_J, KEY_K, KEY_R = 74, 75, 82
local KEY_SPACE = 32

-- Lua 5.3+ 移除了 math.atan2，用 math.atan(y, x) 替代
local atan2 = math.atan2 or math.atan

-- ── 游戏参数 ────────────────────────────────────────────────────────────────
local MOVE_SPEED = 7.0
local ATTACK_RANGE = 3.0
local ATTACK_DAMAGE = 35
local SKILL_DAMAGE = 60
local SKILL_RANGE = 6.0
local SKILL_COOLDOWN = 4.0
local PLAYER_MAX_HP = 200
local ENEMY_DETECT_RANGE = 12.0
local ENEMY_ATTACK_RANGE = 2.5
local ENEMY_ATTACK_DAMAGE = 15
local ENEMY_ATTACK_COOLDOWN = 1.5
local ENEMY_PATROL_SPEED = 2.0
local ENEMY_CHASE_SPEED = 4.5
local START_LIVES = 3

-- ── 资产路径解析 ─────────────────────────────────────────────────────────────
local function file_exists(path)
    local f = io.open(path, "rb")
    if f then f:close(); return true end
    return false
end
local function resolve_path(path)
    if file_exists(path) then return path end
    for _, p in ipairs({ "data/" .. path, "assets/" .. path }) do
        if file_exists(p) then return p end
    end
    return path
end
local function audio_path(path) return resolve_path(path) end

-- ── 关卡数据 ────────────────────────────────────────────────────────────────
-- enemies: { x, z, model, hp, dmg, patrol_range }
-- structures: { x, z, model, scale }
-- treasures: { x, z }
-- bounds: 相机不越出此范围 { min_x, min_z, max_x, max_z }
local LEVELS = {
    {
        name = "Stage 1 - Village Outpost",
        start = { x = 0.0, z = 0.0 },
        bounds = { -25.0, -25.0, 25.0, 25.0 },
        ground_model = "assets/models/map01.glb",
        ground_scale = 2.0,
        enemies = {
            { x = 8.0, z = 5.0, model = "assets/models/mon_0.glb", hp = 80, dmg = 12, patrol = 4.0 },
            { x = -8.0, z = -3.0, model = "assets/models/mon_1.glb", hp = 100, dmg = 15, patrol = 3.0 },
            { x = 5.0, z = -8.0, model = "assets/models/mon_2.glb", hp = 60, dmg = 10, patrol = 5.0 },
            { x = -5.0, z = 10.0, model = "assets/models/mon_2.glb", hp = 90, dmg = 14, patrol = 3.5 },
        },
        structures = {
            { x = -12.0, z = 8.0, model = "assets/models/barrack.glb", scale = 1.0 },
            { x = 12.0, z = -6.0, model = "assets/models/tower.glb", scale = 1.0 },
            { x = 0.0, z = 12.0, model = "assets/models/barricade.glb", scale = 1.0 },
            { x = -3.0, z = 12.0, model = "assets/models/barricade.glb", scale = 1.0 },
            { x = 3.0, z = 12.0, model = "assets/models/barricade.glb", scale = 1.0 },
        },
        treasures = {
            { x = 8.0, z = 5.0 }, { x = -8.0, z = -3.0 },
            { x = 5.0, z = -8.0 }, { x = -5.0, z = 10.0 },
        },
    },
    {
        name = "Stage 2 - Forest Ambush",
        start = { x = 0.0, z = 15.0 },
        bounds = { -30.0, -30.0, 30.0, 30.0 },
        ground_model = "assets/models/map02.glb",
        ground_scale = 2.5,
        enemies = {
            { x = 10.0, z = 0.0, model = "assets/models/mon_0.glb", hp = 90, dmg = 14, patrol = 5.0 },
            { x = -10.0, z = 5.0, model = "assets/models/mon_1.glb", hp = 120, dmg = 18, patrol = 4.0 },
            { x = 5.0, z = -10.0, model = "assets/models/mon_2.glb", hp = 70, dmg = 12, patrol = 6.0 },
            { x = -5.0, z = -5.0, model = "assets/models/mon_2.glb", hp = 100, dmg = 16, patrol = 4.0 },
            { x = 0.0, z = -15.0, model = "assets/models/mon_0.glb", hp = 85, dmg = 13, patrol = 5.0 },
            { x = 15.0, z = 10.0, model = "assets/models/mon_1.glb", hp = 110, dmg = 17, patrol = 3.5 },
        },
        structures = {
            { x = -15.0, z = 10.0, model = "assets/models/tower.glb", scale = 1.0 },
            { x = 15.0, z = -10.0, model = "assets/models/tower.glb", scale = 1.0 },
            { x = -8.0, z = -8.0, model = "assets/models/barrack.glb", scale = 1.0 },
            { x = 8.0, z = 8.0, model = "assets/models/barricade.glb", scale = 1.0 },
            { x = -20.0, z = -15.0, model = "assets/models/horse.glb", scale = 1.0 },
        },
        treasures = {
            { x = 10.0, z = 0.0 }, { x = -10.0, z = 5.0 },
            { x = 5.0, z = -10.0 }, { x = -5.0, z = -5.0 },
            { x = 0.0, z = -15.0 }, { x = 15.0, z = 10.0 },
        },
    },
}

-- ── 运行时状态 ──────────────────────────────────────────────────────────────
local state = {
    mode = "play",        -- play / level_complete / game_over / win
    level_index = 1,
    lives = START_LIVES,
    score = 0,
    time = 0.0,
    cam = nil,
    player_e = nil,
    hud = {},
    skill_cd = 0.0,
}

local level_entities = {
    enemies = {}, structures = {}, treasures = {}, ground = nil, decor = {},
    damage_texts = {}, particles = {},
}

-- ── 音频句柄 ────────────────────────────────────────────────────────────────
local S = {}

local function LoadAudio()
    S.bgm = audio_path("assets/audio/bgm_intro.mp3")
    S.slash = audio_path("assets/audio/slash0.mp3")
    S.coin = audio_path("assets/audio/coin.mp3")
    S.hurt = audio_path("assets/audio/breath_pain.mp3")
    S.mon_die = audio_path("assets/audio/mon_scream1.mp3")
    S.skill = audio_path("assets/audio/skillstart.mp3")
    S.getitem = audio_path("assets/audio/getitem.mp3")
    S.horse = audio_path("assets/audio/horse_cry.mp3")
end

-- ── 工具函数 ────────────────────────────────────────────────────────────────
local function clamp(v, lo, hi)
    if v < lo then return lo end
    if v > hi then return hi end
    return v
end

local function dist3(x1, z1, x2, z2)
    local dx, dz = x1 - x2, z1 - z2
    return math.sqrt(dx * dx + dz * dz)
end

local function CurrentLevel() return LEVELS[state.level_index] end

-- ── 实体创建辅助 ─────────────────────────────────────────────────────────────

-- 加载 GLB 模型实体（mesh + transform + shader + 可选纹理）
local function spawn_model(mesh_path, x, y, z, sx, sy, sz, tex_path)
    local e = dse.ecs.create_entity()
    dse.ecs.add_transform(e, x, y, z, sx, sy, sz)
    dse.ecs.mesh_renderer_add(e, mesh_path)
    dse.ecs.set_mesh_shader_variant(e, "MESH_LIT")
    dse.ecs.set_mesh_material(e, 0.0, 0.7, 1.0, 0, 0, 0, 1.0, true, false)
    if tex_path then
        dse.ecs.set_mesh_texture(e, "albedo", tex_path)
    end
    return e
end

-- 纯色地面（大平面，作为背景）
local function spawn_ground_plane(level)
    local e = dse.ecs.create_entity()
    local s = level.ground_scale or 2.0
    dse.ecs.add_transform(e, 0, -0.05, 0, 40 * s, 0.1, 40 * s)
    -- 用程序化平面代替 map glb（map 模型可能坐标系不匹配，作为装饰额外放置）
    local verts = {
        -0.5, 0,  0.5,  0.5, 0,  0.5,  0.5, 0, -0.5, -0.5, 0, -0.5,
    }
    local indices = { 0, 1, 2, 2, 3, 0 }
    dse.ecs.add_mesh_renderer(e, 0.35, 0.40, 0.30, 1.0, verts, indices)
    dse.ecs.set_mesh_shader_variant(e, "MESH_LIT")
    dse.ecs.set_mesh_material(e, 0.0, 0.85, 1.0, 0, 0, 0, 1.0, true, true)
    return e
end

-- ── 伤害飘字 ────────────────────────────────────────────────────────────────
local function spawn_damage_text(x, y, z, text, r, g, b)
    local e = dse.ecs.create_entity()
    dse.ecs.add_transform(e, x, y, z, 1.0, 1.0, 1.0)
    dse.ui.add_label(e, text, 0, r, g, b, 1.0, 24.0, 32.0, 1.0, 12, 6, 32, 0, 0)
    table.insert(level_entities.damage_texts, {
        e = e, x = x, y = y, z = z, vy = 3.0, life = 0.8, t = 0.0,
    })
end

-- ── 攻击粒子特效 ────────────────────────────────────────────────────────────
local function spawn_hit_effect(x, y, z)
    for _ = 1, 5 do
        local e = dse.ecs.create_entity()
        dse.ecs.add_transform(e, x, y, z, 0.15, 0.15, 0.15)
        local verts = { -0.5, 0, 0, 0.5, 0, 0, 0, 0.5, 0, -0.5, 0, 0 }
        local indices = { 0, 1, 2, 2, 3, 0 }
        dse.ecs.add_mesh_renderer(e, 1.0, 0.2, 0.1, 0.9, verts, indices)
        dse.ecs.set_mesh_shader_variant(e, "MESH_UNLIT")
        table.insert(level_entities.particles, {
            e = e, x = x, y = y, z = z,
            vx = (math.random() - 0.5) * 4.0,
            vy = math.random() * 3.0 + 1.0,
            vz = (math.random() - 0.5) * 4.0,
            life = 0.3 + math.random() * 0.2, t = 0.0,
        })
    end
end

-- ── 关卡清理 ────────────────────────────────────────────────────────────────
local function ClearLevelEntities()
    local function kill(e)
        if e and e ~= 0 then pcall(dse.ecs.destroy_entity, e) end
    end
    for _, en in ipairs(level_entities.enemies) do kill(en.e) end
    for _, s in ipairs(level_entities.structures) do kill(s.e) end
    for _, t in ipairs(level_entities.treasures) do kill(t.e) end
    for _, d in ipairs(level_entities.decor) do kill(d) end
    for _, dt in ipairs(level_entities.damage_texts) do kill(dt.e) end
    for _, p in ipairs(level_entities.particles) do kill(p.e) end
    kill(level_entities.ground)
    kill(state.player_e)
    level_entities = {
        enemies = {}, structures = {}, treasures = {}, ground = nil, decor = {},
        damage_texts = {}, particles = {},
    }
    state.player_e = nil
end

-- ── 玩家状态 ────────────────────────────────────────────────────────────────
local player = {
    x = 0, z = 0, yaw = 0,
    hp = PLAYER_MAX_HP, max_hp = PLAYER_MAX_HP,
    attacking = 0.0,     -- 攻击动画计时
    skill_active = 0.0,  -- 技能动画计时
    invuln = 0.0,
    anim = "idle",
}

local function ResetPlayer()
    local level = CurrentLevel()
    player.x, player.z = level.start.x, level.start.z
    player.yaw = 0.0
    player.hp = player.max_hp
    player.attacking = 0.0
    player.skill_active = 0.0
    player.invuln = 0.0
    player.anim = "idle"
end

-- ── 构建关卡 ────────────────────────────────────────────────────────────────
local function BuildLevel(level)
    -- 地面
    level_entities.ground = spawn_ground_plane(level)

    -- 装饰：地图模型放置在地面上
    local map_e = spawn_model(level.ground_model, 0, 0, 0,
        level.ground_scale or 2.0, level.ground_scale or 2.0, level.ground_scale or 2.0)
    table.insert(level_entities.decor, map_e)

    -- 建筑结构
    for _, s in ipairs(level.structures) do
        local e = spawn_model(s.model, s.x, 0, s.z,
            s.scale, s.scale, s.scale)
        table.insert(level_entities.structures, {
            e = e, x = s.x, z = s.z, model = s.model, scale = s.scale,
        })
    end

    -- 怪物
    for _, en in ipairs(level.enemies) do
        local e = spawn_model(en.model, en.x, 0, en.z, 1.0, 1.0, 1.0)
        table.insert(level_entities.enemies, {
            e = e, x = en.x, z = en.z, base_x = en.x, base_z = en.z,
            model = en.model, hp = en.hp, max_hp = en.hp,
            dmg = en.dmg, patrol = en.patrol or 3.0,
            dead = false, dir = 1.0, t = math.random() * 10.0,
            attack_cd = 0.0, state = "patrol", -- patrol / chase / attack
            hit_flash = 0.0,
        })
    end

    -- 宝物（先隐藏，怪物死后掉落）
    for _, t in ipairs(level.treasures) do
        local e = dse.ecs.create_entity()
        dse.ecs.add_transform(e, t.x, 0.5, t.z, 0.5, 0.5, 0.5)
        -- 宝箱用小立方体表示
        local verts = {
            -0.5,-0.5, 0.5,  0.5,-0.5, 0.5,  0.5, 0.5, 0.5, -0.5, 0.5, 0.5,
            -0.5,-0.5,-0.5,  0.5,-0.5,-0.5,  0.5, 0.5,-0.5, -0.5, 0.5,-0.5,
        }
        local indices = { 0,1,2,2,3,0, 1,5,6,6,2,1, 5,4,7,7,6,5, 4,0,3,3,7,4, 3,2,6,6,7,3, 4,5,1,1,0,4 }
        dse.ecs.add_mesh_renderer(e, 1.0, 0.85, 0.2, 1.0, verts, indices)
        dse.ecs.set_mesh_shader_variant(e, "MESH_UNLIT")
        dse.ecs.set_mesh_visible(e, false)  -- 初始隐藏，怪物死亡后显示
        table.insert(level_entities.treasures, {
            e = e, x = t.x, z = t.z, collected = false,
        })
    end

    -- 玩家
    local pe = spawn_model("assets/models/cha01_01.glb",
        level.start.x, 0, level.start.z, 1.0, 1.0, 1.0)
    state.player_e = pe
end

-- ============================================================================
-- 战斗系统
-- ============================================================================

local function PlayerAttack()
    if player.attacking > 0.0 then return end
    player.attacking = 0.35
    if S.slash then dse.audio.play_sfx(S.slash, 0.8, 0) end

    -- 检测攻击范围内的敌人
    local hit_count = 0
    for _, en in ipairs(level_entities.enemies) do
        if not en.dead then
            local d = dist3(player.x, player.z, en.x, en.z)
            if d <= ATTACK_RANGE then
                -- 检查是否在玩家朝向的扇形范围内
                local dx, dz = en.x - player.x, en.z - player.z
                local angle_to_enemy = math.deg(atan2(dx, -dz))
                local diff = ((angle_to_enemy - player.yaw + 180) % 360) - 180
                if math.abs(diff) < 90 then
                    en.hp = en.hp - ATTACK_DAMAGE
                    en.hit_flash = 0.2
                    en.state = "chase"
                    spawn_damage_text(en.x, 2.0, en.z, tostring(ATTACK_DAMAGE), 1.0, 0.9, 0.3)
                    spawn_hit_effect(en.x, 1.0, en.z)
                    hit_count = hit_count + 1
                    if en.hp <= 0 then
                        en.dead = true
                        en.death_timer = 0.8
                        if S.mon_die then dse.audio.play_sfx(S.mon_die, 0.7, 0) end
                        -- 掉落宝物
                        for _, t in ipairs(level_entities.treasures) do
                            if not t.collected and dist3(t.x, t.z, en.x, en.z) < 1.0 then
                                dse.ecs.set_transform_position(t.e, en.x, 0.5, en.z)
                                dse.ecs.set_mesh_visible(t.e, true)
                                t.x, t.z = en.x, en.z
                                t.dropped = true
                                break
                            end
                        end
                    end
                end
            end
        end
    end
end

local function PlayerSkill()
    if state.skill_cd > 0.0 then return end
    state.skill_cd = SKILL_COOLDOWN
    player.skill_active = 0.6
    if S.skill then dse.audio.play_sfx(S.skill, 0.9, 0) end

    -- 范围攻击：所有 SKILL_RANGE 内的敌人受到伤害
    for _, en in ipairs(level_entities.enemies) do
        if not en.dead then
            local d = dist3(player.x, player.z, en.x, en.z)
            if d <= SKILL_RANGE then
                en.hp = en.hp - SKILL_DAMAGE
                en.hit_flash = 0.3
                en.state = "chase"
                spawn_damage_text(en.x, 2.5, en.z, tostring(SKILL_DAMAGE), 1.0, 0.5, 1.0)
                spawn_hit_effect(en.x, 1.0, en.z)
                if en.hp <= 0 then
                    en.dead = true
                    en.death_timer = 0.8
                    if S.mon_die then dse.audio.play_sfx(S.mon_die, 0.7, 0) end
                    for _, t in ipairs(level_entities.treasures) do
                        if not t.collected and dist3(t.x, t.z, en.x, en.z) < 1.0 then
                            dse.ecs.set_transform_position(t.e, en.x, 0.5, en.z)
                            dse.ecs.set_mesh_visible(t.e, true)
                            t.x, t.z = en.x, en.z
                            t.dropped = true
                            break
                        end
                    end
                end
            end
        end
    end
end

local function DamagePlayer(dmg, from_x, from_z)
    if player.invuln > 0.0 or state.mode ~= "play" then return end
    player.hp = player.hp - dmg
    player.invuln = 0.6
    if S.hurt then dse.audio.play_sfx(S.hurt, 0.7, 0) end
    spawn_damage_text(player.x, 2.5, player.z, tostring(dmg), 1.0, 0.3, 0.3)
    -- 击退
    local dx, dz = player.x - (from_x or 0), player.z - (from_z or 0)
    local d = math.sqrt(dx * dx + dz * dz)
    if d > 0.01 then
        player.x = player.x + (dx / d) * 1.5
        player.z = player.z + (dz / d) * 1.5
    end
    if player.hp <= 0 then
        state.lives = state.lives - 1
        if state.lives <= 0 then
            state.mode = "game_over"
        else
            -- 重置玩家位置和血量
            local level = CurrentLevel()
            player.x, player.z = level.start.x, level.start.z
            player.hp = player.max_hp
            player.invuln = 2.0
        end
    end
end

-- ============================================================================
-- 敌人 AI
-- ============================================================================
local function UpdateEnemies(dt)
    for _, en in ipairs(level_entities.enemies) do
        if en.dead then
            en.death_timer = (en.death_timer or 0) - dt
            if en.death_timer <= 0 and en.e then
                -- 倒地消失（缩小）
                local s = 0.1
                dse.ecs.set_transform_scale(en.e, s, s, s)
                dse.ecs.set_mesh_visible(en.e, false)
                en.e_removed = true
            end
            goto continue
        end

        en.t = en.t + dt
        if en.attack_cd > 0 then en.attack_cd = en.attack_cd - dt end
        if en.hit_flash > 0 then en.hit_flash = en.hit_flash - dt end

        local d = dist3(player.x, player.z, en.x, en.z)

        -- AI 状态机
        if d <= ENEMY_ATTACK_RANGE then
            en.state = "attack"
            if en.attack_cd <= 0 then
                en.attack_cd = ENEMY_ATTACK_COOLDOWN
                DamagePlayer(en.dmg, en.x, en.z)
            end
        elseif d <= ENEMY_DETECT_RANGE then
            en.state = "chase"
        else
            en.state = "patrol"
        end

        -- 移动
        if en.state == "chase" then
            local dx, dz = player.x - en.x, player.z - en.z
            local d2 = math.sqrt(dx * dx + dz * dz)
            if d2 > 0.01 then
                en.x = en.x + (dx / d2) * ENEMY_CHASE_SPEED * dt
                en.z = en.z + (dz / d2) * ENEMY_CHASE_SPEED * dt
                en.yaw = math.deg(atan2(dx, -dz))
            end
        elseif en.state == "patrol" then
            en.x = en.x + en.dir * ENEMY_PATROL_SPEED * dt
            if en.x > en.base_x + en.patrol then en.dir = -1.0 end
            if en.x < en.base_x - en.patrol then en.dir = 1.0 end
            en.yaw = math.deg(atan2(en.dir, 0))
        end

        -- 边界限制
        local b = CurrentLevel().bounds
        en.x = clamp(en.x, b[1], b[3])
        en.z = clamp(en.z, b[2], b[4])

        -- 更新实体
        if en.e then
            dse.ecs.set_transform_position(en.e, en.x, 0, en.z)
            dse.ecs.set_transform_rotation(en.e, 0, en.yaw or 0, 0)
            -- 受击闪烁
            if en.hit_flash > 0 then
                dse.ecs.set_mesh_color(en.e, 1.0, 0.3, 0.3, 1.0)
            else
                dse.ecs.set_mesh_color(en.e, 1.0, 1.0, 1.0, 1.0)
            end
        end

        ::continue::
    end
end

-- ============================================================================
-- 玩家更新
-- ============================================================================
local function UpdatePlayer(dt)
    if state.mode ~= "play" then return end

    -- 计时器
    if player.attacking > 0 then player.attacking = player.attacking - dt end
    if player.skill_active > 0 then player.skill_active = player.skill_active - dt end
    if player.invuln > 0 then player.invuln = player.invuln - dt end
    if state.skill_cd > 0 then state.skill_cd = state.skill_cd - dt end

    -- 输入：移动
    local dx, dz = 0.0, 0.0
    if app.get_key(KEY_A) or app.get_key(KEY_LEFT)  then dx = dx - 1.0 end
    if app.get_key(KEY_D) or app.get_key(KEY_RIGHT) then dx = dx + 1.0 end
    if app.get_key(KEY_W) or app.get_key(KEY_UP)    then dz = dz - 1.0 end
    if app.get_key(KEY_S) or app.get_key(KEY_DOWN)  then dz = dz + 1.0 end
    if dx ~= 0.0 and dz ~= 0.0 then
        dx = dx * 0.70710678
        dz = dz * 0.70710678
    end

    player.x = player.x + dx * MOVE_SPEED * dt
    player.z = player.z + dz * MOVE_SPEED * dt

    -- 朝向
    if dx ~= 0.0 or dz ~= 0.0 then
        player.yaw = math.deg(atan2(dx, -dz))
    end

    -- 边界
    local b = CurrentLevel().bounds
    player.x = clamp(player.x, b[1], b[3])
    player.z = clamp(player.z, b[2], b[4])

    -- 攻击输入
    if app.get_key_down(KEY_J) then
        PlayerAttack()
    end
    if app.get_key_down(KEY_K) then
        PlayerSkill()
    end

    -- 更新玩家实体
    if state.player_e then
        dse.ecs.set_transform_position(state.player_e, player.x, 0, player.z)
        dse.ecs.set_transform_rotation(state.player_e, 0, player.yaw, 0)
        -- 攻击时换模型（挥砍姿态）
        local target_model = "assets/models/cha01_01.glb"
        if player.attacking > 0 or player.skill_active > 0 then
            target_model = "assets/models/cha01_02.glb"
        end
        if player._last_model ~= target_model then
            dse.ecs.set_mesh_path(state.player_e, target_model)
            dse.ecs.set_mesh_shader_variant(state.player_e, "MESH_LIT")
            player._last_model = target_model
        end
        -- 无敌闪烁
        if player.invuln > 0 and math.floor(player.invuln * 10) % 2 == 0 then
            dse.ecs.set_mesh_color(state.player_e, 0.5, 0.5, 1.0, 0.7)
        else
            dse.ecs.set_mesh_color(state.player_e, 1.0, 1.0, 1.0, 1.0)
        end
    end

    -- 宝物拾取
    for _, t in ipairs(level_entities.treasures) do
        if not t.collected and t.dropped then
            if dist3(player.x, player.z, t.x, t.z) < 1.5 then
                t.collected = true
                state.score = state.score + 100
                if S.getitem then dse.audio.play_sfx(S.getitem, 0.8, 0) end
                if t.e then dse.ecs.set_mesh_visible(t.e, false) end
            end
        end
    end

    -- 检查过关条件（所有怪物死亡）
    local all_dead = true
    for _, en in ipairs(level_entities.enemies) do
        if not en.dead then all_dead = false; break end
    end
    if all_dead and #level_entities.enemies > 0 then
        state.mode = "level_complete"
    end
end

-- ============================================================================
-- 相机更新（俯视角跟随）
-- ============================================================================
local function UpdateCamera(dt)
    if not state.cam then return end
    -- 俯视角：相机在玩家上方斜后方
    local cam_height = 18.0
    local cam_back = 8.0
    local target_x = player.x
    local target_y = cam_height
    local target_z = player.z + cam_back

    -- 边界约束
    local b = CurrentLevel().bounds
    target_x = clamp(target_x, b[1] + 2, b[3] - 2)
    target_z = clamp(target_z, b[2] + 5, b[4] - 5)

    dse.ecs.set_transform_position(state.cam, target_x, target_y, target_z)
    -- 相机俯视角度（绕 X 轴旋转，约 55 度俯视）
    dse.ecs.set_transform_rotation(state.cam, -55.0, 0.0, 0.0)
end

-- ============================================================================
-- HUD
-- ============================================================================
local function UpdateHUD()
    local hud = state.hud
    if hud.hp then
        dse.ui.set_label_text(hud.hp, string.format("HP %d/%d", math.max(0, player.hp), player.max_hp))
    end
    if hud.score then
        dse.ui.set_label_text(hud.score, string.format("Score %d", state.score))
    end
    if hud.level then
        dse.ui.set_label_text(hud.level, CurrentLevel().name)
    end
    if hud.lives then
        dse.ui.set_label_text(hud.lives, string.format("Lives %d", state.lives))
    end
    if hud.skill then
        if state.skill_cd > 0 then
            dse.ui.set_label_text(hud.skill, string.format("Skill CD %.1f", state.skill_cd))
        else
            dse.ui.set_label_text(hud.skill, "Skill READY [K]")
        end
    end
    if hud.status then
        local msg = ""
        if state.mode == "level_complete" then
            msg = (state.level_index >= #LEVELS) and "VICTORY!" or ("STAGE CLEAR! -> " .. LEVELS[state.level_index + 1].name)
        elseif state.mode == "game_over" then
            msg = "GAME OVER - Press R to restart"
        elseif state.mode == "win" then
            msg = "YOU WIN!  Press R to play again"
        end
        dse.ui.set_label_text(hud.status, msg)
    end
    if hud.tip then
        dse.ui.set_label_text(hud.tip, "WASD move  J attack  K skill  R restart")
    end
end

-- ============================================================================
-- 关卡流程
-- ============================================================================
local function RespawnLevel()
    ClearLevelEntities()
    state.time = 0.0
    state.mode = "play"
    state.skill_cd = 0.0
    local level = CurrentLevel()
    BuildLevel(level)
    ResetPlayer()
end

local function AdvanceLevel()
    if state.level_index >= #LEVELS then
        state.mode = "win"
        return
    end
    state.level_index = state.level_index + 1
    RespawnLevel()
    if S.bgm then dse.audio.play_bgm(S.bgm, 0.6, true) end
end

local level_complete_timer = 0.0

-- ============================================================================
-- 主流程
-- ============================================================================
function Awake()
    LoadAudio()
    state.time = 0.0
    state.lives = START_LIVES
    state.score = 0
    state.level_index = 1
    state.mode = "play"

    -- 3D 相机（俯视角）
    local cam = dse.ecs.create_entity()
    dse.ecs.add_transform(cam, 0.0, 18.0, 8.0, 1.0, 1.0, 1.0)
    dse.ecs.add_camera_3d(cam, 55.0, 0)  -- fov=55, priority=0
    state.cam = cam

    -- 方向光（从上方斜照）
    local light = dse.ecs.create_entity()
    dse.ecs.add_transform(light, 0.0, 20.0, 0.0, 1.0, 1.0, 1.0)
    dse.ecs.add_directional_light_3d(light, 0.5, -0.8, 0.3, 1.0, 0.95, 0.85, 1.5, 0.3, 0.0)

    -- 环境光（点光源，补充暗部）
    local ambient = dse.ecs.create_entity()
    dse.ecs.add_transform(ambient, 0.0, 15.0, 0.0, 1.0, 1.0, 1.0)
    dse.ecs.add_point_light_3d(ambient, 0.4, 0.4, 0.5, 0.5, 30.0)

    -- HUD：用标签绘制（坐标是相对屏幕中心的像素偏移）
    local function make_label(text, ox, oy, r, g, b, gw, gh)
        local e = dse.ecs.create_entity()
        dse.ecs.add_transform(e, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0)
        dse.ui.add_label(e, text, 0, r, g, b, 1.0, gw, gh, 1.0, 16, 6, 32, ox, oy)
        return e
    end

    -- 加载位图字体纹理
    local font_tex = dse.assets.load_texture(resolve_path("assets/font/bitmap_font.png"))
    -- 重新创建 HUD 标签（带字体纹理）
    local function make_label_f(text, ox, oy, r, g, b, gw, gh)
        local e = dse.ecs.create_entity()
        dse.ecs.add_transform(e, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0)
        dse.ui.add_label(e, text, font_tex, r, g, b, 1.0, gw, gh, 1.0, 16, 6, 32, ox, oy)
        return e
    end

    state.hud.hp     = make_label_f("HP 200/200",    -560.0, 320.0, 1.0, 0.3, 0.3, 18.0, 24.0)
    state.hud.score  = make_label_f("Score 0",       -200.0, 320.0, 1.0, 0.85, 0.3, 18.0, 24.0)
    state.hud.level  = make_label_f("Stage 1",         60.0, 320.0, 0.9, 0.9, 1.0, 18.0, 24.0)
    state.hud.lives  = make_label_f("Lives 3",        380.0, 320.0, 1.0, 1.0, 1.0, 18.0, 24.0)
    state.hud.skill  = make_label_f("Skill READY [K]", 380.0, 270.0, 0.6, 0.8, 1.0, 16.0, 22.0)
    state.hud.status = make_label_f("",                 0.0,  60.0, 1.0, 1.0, 0.4, 30.0, 40.0)
    state.hud.tip    = make_label_f("",                 0.0, -330.0, 0.7, 0.7, 0.7, 16.0, 22.0)

    local level = CurrentLevel()
    BuildLevel(level)
    ResetPlayer()

    if S.bgm then dse.audio.play_bgm(S.bgm, 0.6, true) end
    print("[topdown_3d] ready -- " .. #level.enemies .. " enemies, " .. #level.structures .. " structures")
end

function Update(dt)
    dt = dt or 0.0
    if dt > 0.05 then dt = 0.05 end

    -- 伤害飘字更新
    for i = #level_entities.damage_texts, 1, -1 do
        local dt2 = level_entities.damage_texts[i]
        dt2.t = dt2.t + dt
        dt2.y = dt2.y + dt2.vy * dt
        dt2.vy = dt2.vy - 5.0 * dt
        if dt2.e then
            dse.ecs.set_transform_position(dt2.e, dt2.x, dt2.y, dt2.z)
        end
        if dt2.t >= dt2.life then
            if dt2.e then pcall(dse.ecs.destroy_entity, dt2.e) end
            table.remove(level_entities.damage_texts, i)
        end
    end

    -- 粒子更新
    for i = #level_entities.particles, 1, -1 do
        local p = level_entities.particles[i]
        p.t = p.t + dt
        p.x = p.x + p.vx * dt
        p.y = p.y + p.vy * dt
        p.z = p.z + p.vz * dt
        p.vy = p.vy - 10.0 * dt
        if p.e then
            dse.ecs.set_transform_position(p.e, p.x, p.y, p.z)
            local s = 0.15 * (1.0 - p.t / p.life)
            if s < 0.01 then s = 0.01 end
            dse.ecs.set_transform_scale(p.e, s, s, s)
        end
        if p.t >= p.life then
            if p.e then pcall(dse.ecs.destroy_entity, p.e) end
            table.remove(level_entities.particles, i)
        end
    end

    if state.mode == "play" then
        state.time = state.time + dt
        UpdatePlayer(dt)
        UpdateEnemies(dt)
        UpdateCamera(dt)
        UpdateHUD()
    elseif state.mode == "level_complete" then
        level_complete_timer = level_complete_timer + dt
        UpdateCamera(dt)
        UpdateHUD()
        if level_complete_timer > 1.8 then
            level_complete_timer = 0.0
            AdvanceLevel()
        end
    else
        -- game_over / win：按 R 重开
        if app.get_key_down(KEY_R) then
            state.level_index = 1
            state.lives = START_LIVES
            state.score = 0
            RespawnLevel()
            if S.bgm then dse.audio.play_bgm(S.bgm, 0.6, true) end
        end
        UpdateHUD()
    end
end
