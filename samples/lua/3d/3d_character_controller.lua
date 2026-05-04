-- 3D P4 sample: CharacterController3D showcase + PhysX 专项回归
-- 目标：验证 PhysX kinematic PxRigidDynamic + sweep 角色控制器 + Lua move/jump/grounded API
-- 8 个分时交互阶段：
--   P1 移动→P2 跳跃→P3 方向切换→P4 碰撞检测→P5 着地验证
--   P6 sweep 薄壁检测→P7 collider fallback 对比→P8 连续跳跃回归
-- PhysX ENABLED 时使用真实后端，DISABLED 时 Transform fallback 仍可运行
local CharacterController3D = {}

local state = {
    camera = nil,
    character = nil,
    ground = nil,
    obstacles = {},
    markers = {},
    time = 0.0,
    logged_api = false,
    logged_phase = {},
    gravity_y = -9.81,
    character_velocity_y = 0.0,
    is_grounded = false,
    move_dir = 1,           -- 1=前 -1=后
    character_facing = 0.0, -- 朝向角度
    -- PhysX 专项回归数据
    phase_start_pos = {},   -- 每阶段开始位置
    jump_peak_y = 0.0,      -- 跳跃最高点
    sweep_hit_count = 0,    -- sweep 检测命中次数
    grounded_transition_count = 0,  -- grounded 状态翻转次数
    last_grounded = false,
}

local function cube_vertices()
    return { -0.5,-0.5,0.5, 0.5,-0.5,0.5, 0.5,0.5,0.5, -0.5,0.5,0.5, -0.5,-0.5,-0.5, 0.5,-0.5,-0.5, 0.5,0.5,-0.5, -0.5,0.5,-0.5 }
end

local function cube_indices()
    return { 0,1,2,2,3,0, 1,5,6,6,2,1, 5,4,7,7,6,5, 4,0,3,3,7,4, 3,2,6,6,7,3, 4,5,1,1,0,4 }
end

local function add_visual_cube(name, x, y, z, sx, sy, sz, color, emissive)
    local e = dse.ecs.create_entity()
    dse.ecs.add_transform(e, x, y, z, sx, sy, sz)
    dse.ecs.add_mesh_renderer(e, color[1], color[2], color[3], color[4] or 1.0, cube_vertices(), cube_indices())
    dse.ecs.set_mesh_shader_variant(e, "MESH_LIT")
    dse.ecs.set_mesh_material(e, 0.0, 0.48, 1.0,
        emissive and emissive[1] or 0.0,
        emissive and emissive[2] or 0.0,
        emissive and emissive[3] or 0.0,
        1.0, true, true)
    return e
end

local function setup_camera(config)
    local camera = dse.ecs.create_entity()
    local distance = (type(config) == "table" and type(config.camera_distance) == "number") and config.camera_distance or 8.0
    dse.ecs.add_transform(camera, 0.0, 4.5, distance, 1.0, 1.0, 1.0)
    dse.ecs.set_transform_rotation(camera, -28.0, 0.0, 0.0)
    dse.ecs.add_camera_3d(camera, 60.0, 100)
    if dse.ecs.add_free_camera_controller then
        dse.ecs.add_free_camera_controller(camera, 4.0, 0.10)
    end
    state.camera = camera
end

local function setup_scene(config)
    -- 方向光
    local light = dse.ecs.create_entity()
    dse.ecs.add_directional_light_3d(light, -0.42, -1.0, -0.25, 1.0, 0.95, 0.86, 1.1, 0.20, 0.35)

    -- 静态地面 + 物理碰撞体
    state.ground = add_visual_cube("ground", 0.0, -0.30, 0.0, 14.0, 0.25, 10.0, {0.30, 0.35, 0.33, 1.0})
    dse.ecs.add_box_collider_3d(state.ground, 14.0, 0.25, 10.0)
    dse.ecs.add_rigidbody_3d(state.ground, 0, 0.0) -- Static

    -- 障碍物（静态）
    local obstacles = {
        { name = "wall_front",  x = 0.0,  y = 0.65, z = -3.5, sx = 4.0,  sy = 1.3, sz = 0.4, color = {0.50, 0.38, 0.28, 1.0} },
        { name = "wall_left",   x = -3.0, y = 0.40, z = 0.0,  sx = 0.4,  sy = 0.8, sz = 2.5, color = {0.42, 0.36, 0.30, 1.0} },
        { name = "wall_right",  x = 3.0,  y = 0.40, z = 0.0,  sx = 0.4,  sy = 0.8, sz = 2.5, color = {0.42, 0.36, 0.30, 1.0} },
        { name = "step",        x = 1.5,  y = 0.10, z = 2.0,  sx = 1.8,  sy = 0.20,sz = 1.0, color = {0.55, 0.50, 0.42, 1.0} },
        { name = "ramp_block",  x = -1.5, y = 0.25, z = -1.2, sx = 1.2,  sy = 0.5, sz = 0.8, color = {0.45, 0.52, 0.40, 1.0} },
        -- P6: sweep 薄壁检测专用障碍物
        { name = "thin_wall",   x = 0.0,  y = 0.50, z = -5.5, sx = 3.0,  sy = 1.0, sz = 0.08,color = {0.70, 0.55, 0.25, 1.0} },
    }
    for _, obs in ipairs(obstacles) do
        local e = add_visual_cube(obs.name, obs.x, obs.y, obs.z, obs.sx, obs.sy, obs.sz, obs.color)
        dse.ecs.add_box_collider_3d(e, obs.sx, obs.sy, obs.sz)
        dse.ecs.add_rigidbody_3d(e, 0, 0.0) -- Static
        table.insert(state.obstacles, { name = obs.name, entity = e })
    end

    -- 角色控制器
    local character = dse.ecs.create_entity()
    dse.ecs.add_transform(character, 0.0, 0.0, 2.0, 1.0, 1.0, 1.0)
    local cc_radius = 0.35
    local cc_height = 1.0
    local cc_success = dse.ecs.add_character_controller_3d(character, cc_radius, cc_height, 45.0, 0.3)
    state.character = character
    state.cc_radius = cc_radius
    state.cc_height = cc_height

    -- 角色可视化（胶囊体 marker：身体 + 头部）
    local body = add_visual_cube("char_body", 0.0, 0.85, 2.0, 0.55, 0.80, 0.35,
        {0.18, 0.45, 0.90, 1.0}, {0.02, 0.10, 0.25})
    local head = add_visual_cube("char_head", 0.0, 1.48, 2.0, 0.38, 0.38, 0.38,
        {0.92, 0.72, 0.55, 1.0})
    table.insert(state.markers, { name = "body", entity = body, offset_y = 0.85 })
    table.insert(state.markers, { name = "head", entity = head, offset_y = 1.48 })

    -- 着地指示 marker
    local grounded_marker = add_visual_cube("grounded_indicator", 0.0, 0.04, 2.0, 0.60, 0.08, 0.60,
        {0.0, 0.80, 0.20, 1.0}, {0.0, 0.20, 0.05})
    table.insert(state.markers, { name = "grounded", entity = grounded_marker, offset_y = 0.04 })

    -- 路径方向 marker
    local dir_marker = add_visual_cube("direction_marker", 0.0, 0.55, 1.4, 0.12, 0.12, 0.30,
        {0.90, 0.20, 0.15, 1.0}, {0.25, 0.05, 0.02})
    table.insert(state.markers, { name = "direction", entity = dir_marker, offset_y = 0.55 })

    -- 日志
    print("[3D][CharacterController] character_controller_api=true")
    print("[3D][CharacterController] add_character_controller_3d=" .. tostring(cc_success))
    print("[3D][CharacterController] radius=" .. cc_radius .. " height=" .. cc_height)
    print("[3D][CharacterController] physx_regression: 8-phase sweep/grounded/jump/collider_fallback")
end

function CharacterController3D.Setup(config)
    setup_camera(config)
    setup_scene(config)
end

function CharacterController3D.Update(dt)
    state.time = state.time + dt
    local t = state.time
    local char = state.character
    if not char then return end

    local move_speed = 2.5
    local gravity = state.gravity_y
    local jump_speed = 5.0
    local dx, dy, dz = 0.0, 0.0, 0.0

    -- 重力累加
    if not state.is_grounded then
        state.character_velocity_y = state.character_velocity_y + gravity * dt
    else
        state.character_velocity_y = 0.0
    end
    dy = state.character_velocity_y * dt

    -- ===== 8 阶段控制 =====
    -- Phase 1 (0~2s)：向前移动 + 记录起始位置
    if t < 2.0 then
        dz = -move_speed * dt * state.move_dir
        if not state.logged_phase[1] then
            state.logged_phase[1] = true
            local px, py, pz = dse.ecs.character_controller_3d_get_position(char)
            state.phase_start_pos[1] = { x = px, y = py, z = pz }
            print("[3D][CharacterController] phase1=move_forward speed=" .. move_speed
                .. " start_pos=" .. string.format("(%.2f,%.2f,%.2f)", px, py, pz))
        end

    -- Phase 2 (2~4s)：跳跃 + 峰值检测
    elseif t < 4.0 then
        dz = -move_speed * dt * state.move_dir * 0.5
        if not state.logged_phase[2] then
            state.logged_phase[2] = true
            local jump_ok = dse.ecs.character_controller_3d_jump(char, jump_speed)
            state.character_velocity_y = jump_speed
            local px, py, pz = dse.ecs.character_controller_3d_get_position(char)
            state.phase_start_pos[2] = { x = px, y = py, z = pz }
            print("[3D][CharacterController] phase2=jump jump_speed=" .. jump_speed
                .. " jump_ok=" .. tostring(jump_ok)
                .. " launch_y=" .. string.format("%.2f", py))
        end
        -- 跟踪跳跃峰值
        local _, cur_y, _ = dse.ecs.character_controller_3d_get_position(char)
        if cur_y > state.jump_peak_y then
            state.jump_peak_y = cur_y
        end

    -- Phase 3 (4~5.5s)：切换方向移动 + 着地确认
    elseif t < 5.5 then
        state.move_dir = -1
        dz = -move_speed * dt * state.move_dir
        if not state.logged_phase[3] then
            state.logged_phase[3] = true
            local px, py, pz = dse.ecs.character_controller_3d_get_position(char)
            print("[3D][CharacterController] phase3=reverse_direction dir=" .. state.move_dir
                .. " jump_peak_y=" .. string.format("%.2f", state.jump_peak_y)
                .. " landed=" .. tostring(state.is_grounded)
                .. " cur_y=" .. string.format("%.2f", py))
        end

    -- Phase 4 (5.5~7.0s)：朝障碍物方向移动 + 碰撞检测
    elseif t < 7.0 then
        state.move_dir = 1
        dx = -move_speed * dt * 0.7
        dz = -move_speed * dt * state.move_dir
        if not state.logged_phase[4] then
            state.logged_phase[4] = true
            print("[3D][CharacterController] phase4=obstacle_collision")
        end

    -- Phase 5 (7.0~8.5s)：着地验证 + grounded 精确性
    elseif t < 8.5 then
        dz = -move_speed * dt * 0.3
        if not state.logged_phase[5] then
            state.logged_phase[5] = true
            local px, py, pz = dse.ecs.character_controller_3d_get_position(char)
            local grounded_check = dse.ecs.character_controller_3d_is_grounded(char)
            print("[3D][CharacterController] phase5=grounded_verify"
                .. " move_api_grounded=" .. tostring(state.is_grounded)
                .. " is_grounded_api=" .. tostring(grounded_check)
                .. " pos_y=" .. string.format("%.2f", py)
                .. " grounded_transitions=" .. tostring(state.grounded_transition_count))
        end

    -- Phase 6 (8.5~10.5s)：sweep 薄壁检测（朝 thin_wall 移动）
    elseif t < 10.5 then
        state.move_dir = -1
        dz = -move_speed * dt * 1.2
        dx = move_speed * dt * 0.2
        if not state.logged_phase[6] then
            state.logged_phase[6] = true
            print("[3D][CharacterController] phase6=sweep_thin_wall 朝 thin_wall 移动验证 sweep 碰撞")
        end

    -- Phase 7 (10.5~12.0s)：collider fallback 对比
    --     角色 move 后检查位置是否合理（不应穿墙）
    elseif t < 12.0 then
        state.move_dir = 1
        dz = -move_speed * dt * 0.5
        dx = move_speed * dt * 0.6
        if not state.logged_phase[7] then
            state.logged_phase[7] = true
            local px, py, pz = dse.ecs.character_controller_3d_get_position(char)
            print("[3D][CharacterController] phase7=collider_fallback pos="
                .. string.format("(%.2f,%.2f,%.2f)", px, py, pz)
                .. " 角色 move 后位置应合理（不穿墙/不嵌入地面）")
        end

    -- Phase 8 (12.0s+)：连续跳跃回归
    else
        dz = -move_speed * dt * 0.3
        if state.is_grounded and (t % 1.0 < dt) then
            dse.ecs.character_controller_3d_jump(char, jump_speed * 0.75)
            state.character_velocity_y = jump_speed * 0.75
        end
        if not state.logged_phase[8] then
            state.logged_phase[8] = true
            print("[3D][CharacterController] phase8=continuous_jump_regression 每1.0s跳跃验证着地循环")
        end
    end

    -- 调用 move API
    local is_grounded, vx, vy, vz, collision_flags = dse.ecs.character_controller_3d_move(char, dx, dy, dz, 0.001, dt)

    -- grounded 状态翻转计数
    if is_grounded ~= state.last_grounded then
        state.grounded_transition_count = state.grounded_transition_count + 1
        state.last_grounded = is_grounded
    end
    state.is_grounded = is_grounded

    -- sweep 命中计数（collision_flags 非零表示发生了碰撞）
    if collision_flags and collision_flags ~= 0 then
        state.sweep_hit_count = state.sweep_hit_count + 1
    end

    -- 查询位置
    local px, py, pz = dse.ecs.character_controller_3d_get_position(char)

    -- 同步可视化 marker 位置
    for _, m in ipairs(state.markers) do
        if m.entity then
            dse.ecs.set_transform_position(m.entity, px, py + m.offset_y, pz)
        end
    end

    -- 着地指示颜色
    if state.markers[3] then
        local gm = state.markers[3]
        if state.is_grounded then
            dse.ecs.set_mesh_material(gm.entity, 0.0, 0.48, 1.0, 0.0, 0.20, 0.05, 1.0, true, true)
        else
            dse.ecs.set_mesh_material(gm.entity, 0.0, 0.48, 1.0, 0.25, 0.05, 0.02, 1.0, true, true)
        end
    end

    -- 周期日志
    if not state.logged_api and t > 0.5 then
        state.logged_api = true
        local grounded_check = dse.ecs.character_controller_3d_is_grounded(char)
        print("[3D][CharacterController] character_controller_runtime_api=true")
        print("[3D][CharacterController] move_api=true is_grounded=" .. tostring(is_grounded))
        print("[3D][CharacterController] is_grounded_api=" .. tostring(grounded_check))
        print("[3D][CharacterController] get_position_api=true pos=" .. string.format("%.2f,%.2f,%.2f", px, py, pz))
        print("[3D][CharacterController] collision_flags=" .. tostring(collision_flags))
    end

    -- 阶段验证日志
    if t > 3.8 and t < 3.9 then
        print("[3D][CharacterController] phase2_verify jump_peak_y=" .. string.format("%.2f", state.jump_peak_y)
            .. " is_grounded=" .. tostring(is_grounded) .. " pos_y=" .. string.format("%.2f", py))
    end
    if t > 5.2 and t < 5.3 then
        print("[3D][CharacterController] phase3_verify landed=" .. tostring(is_grounded)
            .. " pos_y=" .. string.format("%.2f", py)
            .. " grounded_transitions=" .. tostring(state.grounded_transition_count))
    end
    if t > 8.2 and t < 8.3 then
        print("[3D][CharacterController] phase5_verify grounded=" .. tostring(is_grounded)
            .. " grounded_transitions=" .. tostring(state.grounded_transition_count)
            .. " sweep_hits=" .. tostring(state.sweep_hit_count))
    end
    if t > 10.2 and t < 10.3 then
        print("[3D][CharacterController] phase6_verify sweep_hits=" .. tostring(state.sweep_hit_count)
            .. " pos=" .. string.format("(%.2f,%.2f,%.2f)", px, py, pz))
    end
    if t > 11.8 and t < 11.9 then
        print("[3D][CharacterController] phase7_verify collider_fallback pos="
            .. string.format("(%.2f,%.2f,%.2f)", px, py, pz))
    end
end

return CharacterController3D
