-- 3D P1 sample: input showcase
-- 目标：展示 3D 场景中的键盘/鼠标输入交互 — WASD 移动物体、鼠标点击拾取、
--       方向键旋转、空格键跳跃、R 重置位置。
-- 覆盖 API: dse.app.get_key, get_key_down, get_key_up,
--           get_mouse_x/y, get_mouse_left, get_mouse_left_down, get_mouse_left_up,
--           get_mouse_right, get_mouse_left_double_click, get_mouse_left_long_press,
--           get_mouse_swipe_dx/dy, get_screen_width/height
local InputShowcase3D = {}

-- 键码常量（与 engine/input/key_code.h 对齐）
local KEY_W = 87
local KEY_A = 65
local KEY_S = 83
local KEY_D = 68
local KEY_R = 82
local KEY_Q = 81
local KEY_E = 69
local KEY_SPACE = 32
local KEY_UP = 265
local KEY_DOWN = 264
local KEY_LEFT = 263
local KEY_RIGHT = 262
local KEY_ESCAPE = 256
local KEY_LEFT_SHIFT = 340
local KEY_F1 = 290
local KEY_F2 = 291

local state = {
    camera = nil,
    player = nil,
    ground = nil,
    objects = {},
    selected = nil,
    selected_color = {},
    time = 0.0,
    move_speed = 3.5,
    rotate_speed = 90.0,
    jump_speed = 5.0,
    player_vy = 0.0,
    player_y = 0.3,
    player_grounded = true,
    gravity = -12.0,
    input_logged = false,
    click_count = 0,
    last_click_pos = { 0.0, 0.0 },
    swipe_logged = false
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
    table.insert(state.objects, { name = name, entity = e, color = color, x = x, y = y, z = z })
    return e
end

local function setup_camera(config)
    local camera = dse.ecs.create_entity()
    local distance = (type(config) == "table" and type(config.camera_distance) == "number") and config.camera_distance or 12.0
    dse.ecs.add_transform(camera, 0.0, 5.5, distance, 1.0, 1.0, 1.0)
    dse.ecs.set_transform_rotation(camera, -25.0, 0.0, 0.0)
    dse.ecs.add_camera_3d(camera, 60.0, 100)
    if dse.ecs.add_free_camera_controller then
        dse.ecs.add_free_camera_controller(camera, 5.5, 0.12)
    end
    state.camera = camera
end

local function setup_scene(config)
    state.move_speed = (type(config) == "table" and type(config.move_speed) == "number") and config.move_speed or 3.5

    -- 光照
    local light = dse.ecs.create_entity()
    dse.ecs.add_directional_light_3d(light, -0.45, -1.0, -0.30, 1.0, 0.96, 0.88, 1.15, 0.20, 0.32)

    -- 地面
    state.ground = add_cube("ground", 0.0, -0.55, 0.0, 10.0, 0.12, 8.0, {0.30, 0.34, 0.38, 1.0})

    -- 玩家方块（蓝色，可移动）
    state.player = add_cube("player", 0.0, 0.3, 0.0, 0.75, 0.75, 0.75, {0.22, 0.62, 1.0, 1.0}, {0.03, 0.10, 0.30})
    state.player_y = 0.3

    -- 可拾取的目标方块
    add_cube("target_red", -3.0, 0.3, -1.5, 0.65, 0.65, 0.65, {1.0, 0.30, 0.18, 1.0}, {0.25, 0.05, 0.01})
    add_cube("target_green", 3.0, 0.3, -1.5, 0.65, 0.65, 0.65, {0.18, 1.0, 0.35, 1.0}, {0.02, 0.25, 0.05})
    add_cube("target_yellow", 0.0, 0.3, -3.0, 0.65, 0.65, 0.65, {1.0, 0.90, 0.15, 1.0}, {0.25, 0.20, 0.01})
    add_cube("target_purple", -2.0, 0.3, 2.0, 0.65, 0.65, 0.65, {0.72, 0.28, 1.0, 1.0}, {0.15, 0.04, 0.25})
    add_cube("target_orange", 2.0, 0.3, 2.0, 0.65, 0.65, 0.65, {1.0, 0.58, 0.12, 1.0}, {0.25, 0.12, 0.01})

    -- 操作提示地面标记
    add_cube("marker_w", -1.5, -0.46, 1.5, 0.45, 0.04, 0.45, {0.22, 0.62, 1.0, 1.0})
    add_cube("marker_a", -2.5, -0.46, 2.5, 0.45, 0.04, 0.45, {0.85, 0.85, 0.22, 1.0})
    add_cube("marker_s", -1.5, -0.46, 3.5, 0.45, 0.04, 0.45, {0.85, 0.85, 0.22, 1.0})
    add_cube("marker_d", -0.5, -0.46, 2.5, 0.45, 0.04, 0.45, {0.85, 0.85, 0.22, 1.0})

    print("[3D][Input] setup: WASD=move, Arrow=rotate, Space=jump, R=reset, Click=select, DblClick=highlight")
    print(string.format("[3D][Input] input_api: get_key=%s get_key_down=%s get_key_up=%s get_mouse_left=%s get_mouse_left_down=%s",
        tostring(dse.app.get_key ~= nil),
        tostring(dse.app.get_key_down ~= nil),
        tostring(dse.app.get_key_up ~= nil),
        tostring(dse.app.get_mouse_left ~= nil),
        tostring(dse.app.get_mouse_left_down ~= nil)
    ))
end

-- ============================================================
-- 输入处理
-- ============================================================

local function handle_keyboard_input(dt)
    if not state.player then return end

    local dx, dz = 0.0, 0.0
    local speed = state.move_speed
    if dse.app.get_key then
        -- Shift 加速
        if dse.app.get_key(KEY_LEFT_SHIFT) then
            speed = speed * 2.0
        end
        -- WASD 移动
        if dse.app.get_key(KEY_W) then dz = dz - 1.0 end
        if dse.app.get_key(KEY_S) then dz = dz + 1.0 end
        if dse.app.get_key(KEY_A) then dx = dx - 1.0 end
        if dse.app.get_key(KEY_D) then dx = dx + 1.0 end
    end

    -- 归一化对角移动
    local len = math.sqrt(dx * dx + dz * dz)
    if len > 0.001 then
        dx = dx / len * speed * dt
        dz = dz / len * speed * dt
    end

    -- 方向键旋转
    local rot_delta = 0.0
    if dse.app.get_key then
        if dse.app.get_key(KEY_LEFT) then rot_delta = rot_delta - state.rotate_speed * dt end
        if dse.app.get_key(KEY_RIGHT) then rot_delta = rot_delta + state.rotate_speed * dt end
    end

    -- Q/E 上下移动（调试用）
    if dse.app.get_key and dse.app.get_key(KEY_Q) then
        state.player_y = state.player_y + speed * dt
    end
    if dse.app.get_key and dse.app.get_key(KEY_E) then
        state.player_y = math.max(0.3, state.player_y - speed * dt)
    end

    -- 空格跳跃
    if dse.app.get_key_down and dse.app.get_key_down(KEY_SPACE) then
        if state.player_grounded then
            state.player_vy = state.jump_speed
            state.player_grounded = false
        end
    end

    -- R 重置位置
    if dse.app.get_key_down and dse.app.get_key_down(KEY_R) then
        dx, dz = 0.0, 0.0
        state.player_y = 0.3
        state.player_vy = 0.0
        state.player_grounded = true
        dse.ecs.set_transform_position(state.player, 0.0, 0.3, 0.0)
        dse.ecs.set_transform_rotation(state.player, 0.0, 0.0, 0.0)
        print("[3D][Input] key_down: R -> reset position")
    end

    -- 应用移动
    if len > 0.001 then
        local px, py, pz = dse.ecs.get_transform_position(state.player)
        dse.ecs.set_transform_position(state.player, (px or 0.0) + dx, py or state.player_y, (pz or 0.0) + dz)
    end

    -- 应用旋转
    if rot_delta ~= 0.0 then
        local rx, ry, rz = dse.ecs.get_transform_rotation(state.player)
        dse.ecs.set_transform_rotation(state.player, rx or 0.0, (ry or 0.0) + rot_delta, rz or 0.0)
    end

    -- 应用跳跃/重力
    if not state.player_grounded then
        state.player_vy = state.player_vy + state.gravity * dt
        state.player_y = state.player_y + state.player_vy * dt
        if state.player_y <= 0.3 then
            state.player_y = 0.3
            state.player_vy = 0.0
            state.player_grounded = true
        end
    end

    -- 同步 y 坐标
    local px, py, pz = dse.ecs.get_transform_position(state.player)
    dse.ecs.set_transform_position(state.player, px or 0.0, state.player_y, pz or 0.0)
end

local function handle_mouse_input()
    -- 鼠标左键点击拾取
    if dse.app.get_mouse_left_down and dse.app.get_mouse_left_down() then
        local mx = dse.app.get_mouse_x and dse.app.get_mouse_x() or 0.0
        local my = dse.app.get_mouse_y and dse.app.get_mouse_y() or 0.0
        state.click_count = state.click_count + 1
        state.last_click_pos = { mx, my }

        -- Raycast 从鼠标位置拾取物体
        if dse.ecs.physics_3d_raycast then
            local hit, entity, hx, hy, hz, nx, ny, nz, distance =
                dse.ecs.physics_3d_raycast(0.0, 8.0, 0.0, 0.0, -1.0, 0.0, 20.0)
            if hit and entity then
                -- 恢复之前选中物体颜色
                if state.selected and state.selected_color then
                    dse.ecs.add_mesh_renderer(state.selected,
                        state.selected_color[1], state.selected_color[2], state.selected_color[3],
                        state.selected_color[4] or 1.0, cube_vertices(), cube_indices())
                    dse.ecs.set_mesh_shader_variant(state.selected, "MESH_LIT")
                    dse.ecs.set_mesh_material(state.selected, 0.0, 0.55, 1.0, 0.0, 0.0, 0.0, 1.0, true, true)
                end
                -- 高亮新选中物体
                state.selected = entity
                -- 保存原始颜色（简化：标记为白色高亮）
                state.selected_color = { 1.0, 1.0, 1.0, 1.0 }
                dse.ecs.set_mesh_emissive(entity, 0.4, 0.4, 0.4)
                print(string.format("[3D][Input] mouse_left_down: click=%d mouse=(%.0f,%.0f) raycast_hit=%s entity=%s",
                    state.click_count, mx, my, tostring(hit), tostring(entity)))
            else
                print(string.format("[3D][Input] mouse_left_down: click=%d mouse=(%.0f,%.0f) raycast_hit=false",
                    state.click_count, mx, my))
            end
        else
            print(string.format("[3D][Input] mouse_left_down: click=%d mouse=(%.0f,%.0f) raycast_unavailable",
                state.click_count, mx, my))
        end
    end

    -- 鼠标双击
    if dse.app.get_mouse_left_double_click and dse.app.get_mouse_left_double_click() then
        print("[3D][Input] mouse_left_double_click detected")
    end

    -- 鼠标长按
    if dse.app.get_mouse_left_long_press and dse.app.get_mouse_left_long_press(0.8) then
        print("[3D][Input] mouse_left_long_press (>0.8s) detected")
    end

    -- 鼠标滑动增量
    if dse.app.get_mouse_swipe_dx and dse.app.get_mouse_swipe_dy then
        local sdx = dse.app.get_mouse_swipe_dx()
        local sdy = dse.app.get_mouse_swipe_dy()
        if (math.abs(sdx) > 2.0 or math.abs(sdy) > 2.0) and not state.swipe_logged then
            state.swipe_logged = true
            print(string.format("[3D][Input] mouse_swipe: dx=%.1f dy=%.1f", sdx, sdy))
        elseif math.abs(sdx) < 0.5 and math.abs(sdy) < 0.5 then
            state.swipe_logged = false
        end
    end

    -- 右键取消选中
    if dse.app.get_mouse_right_down and dse.app.get_mouse_right_down() then
        if state.selected then
            dse.ecs.set_mesh_emissive(state.selected, 0.0, 0.0, 0.0)
            state.selected = nil
            print("[3D][Input] mouse_right_down: deselected")
        end
    end
end

-- ============================================================
-- 生命周期
-- ============================================================

function InputShowcase3D.Setup(config)
    print("[3D][Input] setup: keyboard+mouse input in 3D scene. WASD=move, Arrow=rotate, Space=jump, R=reset, LeftClick=select, RightClick=deselect")
    setup_camera(config or {})
    setup_scene(config or {})

    -- 验证屏幕尺寸 API
    local screen_w = dse.app.get_screen_width and dse.app.get_screen_width() or 0
    local screen_h = dse.app.get_screen_height and dse.app.get_screen_height() or 0
    print(string.format("[3D][Input] screen: %dx%d", screen_w, screen_h))
end

function InputShowcase3D.Update(delta_time)
    local dt = delta_time or 0.0
    if dt > 0.1 then dt = 0.1 end
    state.time = state.time + dt

    handle_keyboard_input(dt)
    handle_mouse_input()

    -- 目标方块微浮动动画
    for i, obj in ipairs(state.objects) do
        if obj.name and obj.name:find("target_") then
            local y = obj.y + math.sin(state.time * 1.5 + i * 1.2) * 0.08
            local px, py, pz = dse.ecs.get_transform_position(obj.entity)
            if px then
                dse.ecs.set_transform_position(obj.entity, px, y, pz)
            end
            dse.ecs.set_transform_rotation(obj.entity, state.time * 18.0, state.time * 28.0 + i * 30.0, 0.0)
        end
    end

    -- 延迟输出输入 API 完整报告
    if not state.input_logged and state.time > 0.5 then
        state.input_logged = true
        print(string.format("[3D][Input] input_api_summary: get_key=%s get_key_down=%s get_key_up=%s get_mouse_x=%s get_mouse_y=%s get_mouse_left=%s get_mouse_left_down=%s get_mouse_left_up=%s get_mouse_right=%s get_mouse_right_down=%s get_mouse_right_up=%s get_mouse_left_double_click=%s get_mouse_left_long_press=%s get_mouse_swipe_dx=%s get_mouse_swipe_dy=%s get_screen_width=%s get_screen_height=%s time_since_startup=%s",
            tostring(dse.app.get_key ~= nil),
            tostring(dse.app.get_key_down ~= nil),
            tostring(dse.app.get_key_up ~= nil),
            tostring(dse.app.get_mouse_x ~= nil),
            tostring(dse.app.get_mouse_y ~= nil),
            tostring(dse.app.get_mouse_left ~= nil),
            tostring(dse.app.get_mouse_left_down ~= nil),
            tostring(dse.app.get_mouse_left_up ~= nil),
            tostring(dse.app.get_mouse_right ~= nil),
            tostring(dse.app.get_mouse_right_down ~= nil),
            tostring(dse.app.get_mouse_right_up ~= nil),
            tostring(dse.app.get_mouse_left_double_click ~= nil),
            tostring(dse.app.get_mouse_left_long_press ~= nil),
            tostring(dse.app.get_mouse_swipe_dx ~= nil),
            tostring(dse.app.get_mouse_swipe_dy ~= nil),
            tostring(dse.app.get_screen_width ~= nil),
            tostring(dse.app.get_screen_height ~= nil),
            tostring(dse.app.time_since_startup ~= nil)
        ))
    end
end

return InputShowcase3D
