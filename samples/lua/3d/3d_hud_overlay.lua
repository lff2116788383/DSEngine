-- 3D P1 sample: HUD overlay showcase
-- 目标：展示 3D 场景中 UI 叠加层（HUD）— 面板、标签、按钮、摇杆的混合使用。
-- 覆盖 API: dse.ui.add_renderer, add_label, set_label_text, set_label_number,
--           add_panel, add_button, set_button_scale, add_joystick, get_joystick_x/y
local HudOverlay3D = {}


HudOverlay3D._meta = {
    name     = "HUD overlay showcase",
    category = "ui",
    config   = { camera_distance=14.0 },
}

local state = {
    camera = nil,
    player = nil,
    ui = {},
    score = 0,
    time = 0.0,
    player_x = 0.0,
    player_z = 0.0,
    logged = false
}

-- ============================================================
-- 几何数据
-- ============================================================

local function cube_vertices()
    return { -0.5,-0.5,0.5, 0.5,-0.5,0.5, 0.5,0.5,0.5, -0.5,0.5,0.5, -0.5,-0.5,-0.5, 0.5,-0.5,-0.5, 0.5,0.5,-0.5, -0.5,0.5,-0.5 }
end

local function cube_indices()
    return { 0,1,2,2,3,0, 1,5,6,6,2,1, 5,4,7,7,6,5, 4,0,3,3,7,4, 3,2,6,6,7,3, 4,5,1,1,0,4 }
end

-- ============================================================
-- 场景搭建
-- ============================================================

local function add_cube(name, x, y, z, sx, sy, sz, color, emissive)
    local e = dse.ecs.create_entity()
    dse.ecs.add_transform(e, x, y, z, sx, sy, sz)
    dse.ecs.add_mesh_renderer(e, color[1], color[2], color[3], color[4] or 1.0, cube_vertices(), cube_indices())
    dse.ecs.set_mesh_shader_variant(e, "MESH_LIT")
    dse.ecs.set_mesh_material(e, 0.0, 0.55, 1.0,
        emissive and emissive[1] or 0.0,
        emissive and emissive[2] or 0.0,
        emissive and emissive[3] or 0.0,
        1.0, true, true)
    return e
end

local function setup_camera(config)
    local camera = dse.ecs.create_entity()
    local distance = (type(config) == "table" and type(config.camera_distance) == "number") and config.camera_distance or 14.0
    dse.ecs.add_transform(camera, 0.0, 8.0, distance, 1.0, 1.0, 1.0)
    dse.ecs.set_transform_rotation(camera, -30.0, 0.0, 0.0)
    dse.ecs.add_camera_3d(camera, 60.0, 100)
    if dse.ecs.add_free_camera_controller then
        dse.ecs.add_free_camera_controller(camera, 5.5, 0.12)
    end
    state.camera = camera
end

local function setup_scene(config)
    -- 光照
    local light = dse.ecs.create_entity()
    dse.ecs.add_directional_light_3d(light, -0.45, -1.0, -0.30, 1.0, 0.96, 0.88, 1.15, 0.20, 0.32)

    -- 地面
    add_cube("ground", 0.0, -0.55, 0.0, 10.0, 0.12, 8.0, {0.30, 0.34, 0.38, 1.0})

    -- 玩家方块
    state.player = add_cube("player", 0.0, 0.3, 0.0, 0.8, 0.8, 0.8, {0.22, 0.62, 1.0, 1.0}, {0.03, 0.10, 0.30})

    -- 散布的目标方块
    add_cube("target_1", -3.5, 0.25, -2.0, 0.6, 0.6, 0.6, {1.0, 0.35, 0.18, 1.0})
    add_cube("target_2", 3.5, 0.25, -2.0, 0.6, 0.6, 0.6, {0.18, 1.0, 0.40, 1.0})
    add_cube("target_3", 0.0, 0.25, -3.5, 0.6, 0.6, 0.6, {1.0, 0.88, 0.15, 1.0})
    add_cube("target_4", -2.0, 0.25, 2.5, 0.6, 0.6, 0.6, {0.72, 0.28, 1.0, 1.0})
    add_cube("target_5", 2.0, 0.25, 2.5, 0.6, 0.6, 0.6, {1.0, 0.55, 0.12, 1.0})
end

local function setup_ui(config)
    local font_tex = (type(config) == "table" and type(config.font_tex) == "number") and config.font_tex or 0
    local ui_tex = (type(config) == "table" and type(config.ui_tex) == "number") and config.ui_tex or 0

    -- === 顶部面板：标题 + 分数 ===
    local top_panel = dse.ecs.create_entity()
    dse.ecs.add_transform(top_panel, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0)
    dse.ui.add_renderer(top_panel, 0, 0.06, 0.10, 0.16, 0.78, 900, 640.0, 80.0)
    dse.ui.add_panel(top_panel, true)
    state.ui.top_panel = top_panel

    local title = dse.ecs.create_entity()
    dse.ecs.add_transform(title, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0)
    dse.ui.add_label(title, "3D HUD Overlay", font_tex, 1.0, 0.95, 0.70, 1.0, 20.0, 28.0, 2.0, 16, 6, 32, -280.0, -22.0)
    state.ui.title = title

    local score_label = dse.ecs.create_entity()
    dse.ecs.add_transform(score_label, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0)
    dse.ui.add_label(score_label, "Score", font_tex, 0.85, 0.90, 1.0, 1.0, 14.0, 20.0, 1.0, 16, 6, 32, 200.0, -22.0)
    state.ui.score_label = score_label

    local score_value = dse.ecs.create_entity()
    dse.ecs.add_transform(score_value, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0)
    dse.ui.add_label(score_value, "0", font_tex, 1.0, 0.85, 0.35, 1.0, 18.0, 24.0, 1.0, 16, 6, 32, 200.0, 8.0)
    state.ui.score_value = score_value

    -- === 底部状态栏 ===
    local bottom_panel = dse.ecs.create_entity()
    dse.ecs.add_transform(bottom_panel, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0)
    dse.ui.add_renderer(bottom_panel, 0, 0.08, 0.06, 0.12, 0.70, 850, 640.0, 60.0)
    dse.ui.add_panel(bottom_panel, false)
    state.ui.bottom_panel = bottom_panel

    local status = dse.ecs.create_entity()
    dse.ecs.add_transform(status, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0)
    dse.ui.add_label(status, "Joystick -> move | FPS camera: right mouse + WASD", font_tex, 0.72, 0.80, 0.90, 1.0, 12.0, 16.0, 1.0, 16, 6, 32, -290.0, 16.0)
    state.ui.status = status

    -- === 右下角按钮 ===
    local action_btn = dse.ecs.create_entity()
    dse.ecs.add_transform(action_btn, 4.8, -3.2, 0.0, 1.8, 0.7, 1.0)
    dse.ecs.add_sprite(action_btn, 0.20, 0.48, 0.75, 0.92, 10, ui_tex)
    dse.ui.add_button(action_btn, 0.20, 0.48, 0.75, 0.92)
    dse.ui.set_button_scale(action_btn, 1.06, 0.94, 12.0)
    state.ui.action_btn = action_btn

    local btn_label = dse.ecs.create_entity()
    dse.ecs.add_transform(btn_label, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0)
    dse.ui.add_label(btn_label, "Action", font_tex, 1.0, 1.0, 1.0, 1.0, 14.0, 20.0, 1.0, 16, 6, 32, 232.0, -132.0)
    state.ui.btn_label = btn_label

    -- === 左下角虚拟摇杆 ===
    local joystick_bg = dse.ecs.create_entity()
    dse.ecs.add_transform(joystick_bg, -4.5, -2.8, 0.0, 2.0, 2.0, 1.0)
    dse.ecs.add_sprite(joystick_bg, 0.15, 0.20, 0.30, 0.50, 5, ui_tex)
    state.ui.joystick_bg = joystick_bg

    local joystick = dse.ecs.create_entity()
    dse.ecs.add_transform(joystick, -4.5, -2.8, 0.0, 1.0, 1.0, 1.0)
    dse.ecs.add_sprite(joystick, 0.40, 0.55, 0.85, 0.70, 6, ui_tex)
    dse.ui.add_joystick(joystick, 64.0, true, true)
    state.ui.joystick = joystick

    -- === 右上角信息面板 ===
    local info_panel = dse.ecs.create_entity()
    dse.ecs.add_transform(info_panel, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0)
    dse.ui.add_renderer(info_panel, 0, 0.10, 0.08, 0.15, 0.65, 800, 180.0, 120.0)
    dse.ui.add_panel(info_panel, false)
    state.ui.info_panel = info_panel

    local pos_label = dse.ecs.create_entity()
    dse.ecs.add_transform(pos_label, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0)
    dse.ui.add_label(pos_label, "Pos: (0.0, 0.0)", font_tex, 0.80, 0.88, 1.0, 1.0, 10.0, 14.0, 1.0, 16, 6, 32, 232.0, -80.0)
    state.ui.pos_label = pos_label

    print(string.format("[3D][HudOverlay] setup: top_panel + bottom_status + action_button + joystick + info_panel"))
    print(string.format("[3D][HudOverlay] ui_api: add_renderer=%s add_label=%s set_label_text=%s set_label_number=%s add_panel=%s add_button=%s set_button_scale=%s add_joystick=%s get_joystick_x/y=%s",
        tostring(dse.ui.add_renderer ~= nil),
        tostring(dse.ui.add_label ~= nil),
        tostring(dse.ui.set_label_text ~= nil),
        tostring(dse.ui.set_label_number ~= nil),
        tostring(dse.ui.add_panel ~= nil),
        tostring(dse.ui.add_button ~= nil),
        tostring(dse.ui.set_button_scale ~= nil),
        tostring(dse.ui.add_joystick ~= nil),
        tostring(dse.ui.get_joystick_x ~= nil)
    ))
end

-- ============================================================
-- 生命周期
-- ============================================================

function HudOverlay3D.Setup(config)
    print("[3D][HudOverlay] setup: 3D scene with UI overlay (panel, label, button, joystick)")
    setup_camera(config or {})
    setup_scene(config or {})
    setup_ui(config or {})
end

function HudOverlay3D.Update(delta_time)
    local dt = delta_time or 0.0
    if dt > 0.1 then dt = 0.1 end
    state.time = state.time + dt

    -- 摇杆驱动玩家移动
    local jx, jy = 0.0, 0.0
    if dse.ui.get_joystick_x and dse.ui.get_joystick_y and state.ui.joystick then
        jx = dse.ui.get_joystick_x(state.ui.joystick)
        jy = dse.ui.get_joystick_y(state.ui.joystick)
    end
    local move_speed = 3.5
    state.player_x = state.player_x + jx * move_speed * dt
    state.player_z = state.player_z - jy * move_speed * dt
    -- 限制范围
    state.player_x = math.max(-4.5, math.min(4.5, state.player_x))
    state.player_z = math.max(-3.5, math.min(3.5, state.player_z))

    if state.player then
        dse.ecs.set_transform_position(state.player, state.player_x, 0.3, state.player_z)
        dse.ecs.set_transform_rotation(state.player, state.time * 22.0, state.time * 35.0, 0.0)
    end

    -- 更新分数（基于移动距离）
    local joystick_active = math.abs(jx) > 0.05 or math.abs(jy) > 0.05
    if joystick_active then
        state.score = state.score + math.floor(math.sqrt(jx * jx + jy * jy) * 10 * dt + 0.5)
    end

    -- 更新 UI 文本
    if dse.ui.set_label_number and state.ui.score_value then
        dse.ui.set_label_number(state.ui.score_value, state.score)
    end

    if dse.ui.set_label_text and state.ui.pos_label then
        dse.ui.set_label_text(state.ui.pos_label, string.format("Pos: (%.1f, %.1f)", state.player_x, state.player_z))
    end

    -- 状态栏动态切换
    if dse.ui.set_label_text and state.ui.status then
        if joystick_active then
            dse.ui.set_label_text(state.ui.status, string.format("Moving... jx=%.2f jy=%.2f", jx, jy))
        else
            dse.ui.set_label_text(state.ui.status, "Joystick -> move | FPS camera: right mouse + WASD")
        end
    end

    -- 延迟报告
    if not state.logged and state.time > 0.5 then
        state.logged = true
        print(string.format("[3D][HudOverlay] runtime: score=%d player=(%.1f, %.1f) joystick=(%.2f, %.2f)",
            state.score, state.player_x, state.player_z, jx, jy))
    end
end

return HudOverlay3D
