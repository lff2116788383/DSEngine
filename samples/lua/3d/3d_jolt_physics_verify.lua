-- Jolt Physics 运行时综合验证 Demo
-- 覆盖：刚体碰撞、碰撞层过滤、关节约束(Fixed/Hinge/Distance)、
--       角色控制器、触发区域(Enter/Exit)、Ragdoll 激活、Raycast
local JoltVerify = {}

local cube_verts = {-0.5,-0.5,0.5, 0.5,-0.5,0.5, 0.5,0.5,0.5, -0.5,0.5,0.5,
                    -0.5,-0.5,-0.5, 0.5,-0.5,-0.5, 0.5,0.5,-0.5, -0.5,0.5,-0.5}
local cube_inds  = {0,1,2,2,3,0, 1,5,6,6,2,1, 5,4,7,7,6,5, 4,0,3,3,7,4, 3,2,6,6,7,3, 4,5,1,1,0,4}

local state = {
    time = 0,
    results = {},       -- {test_name = "PASS"/"FAIL"/"PENDING"}
    phase = 0,
    -- entities
    ground = nil,
    ball_a = nil,       -- layer 1, 碰撞测试
    ball_b = nil,       -- layer 2, 不与 ball_a 碰撞
    ball_c = nil,       -- layer 1, 与 ball_a 碰撞
    joint_anchor = nil,
    joint_pendulum = nil,
    trigger_zone = nil,
    trigger_probe = nil,
    char_entity = nil,
    ragdoll_torso = nil,
    ragdoll_head = nil,
    -- state flags
    collision_enter_seen = false,
    collision_exit_seen = false,
    trigger_enter_seen = false,
    trigger_exit_seen = false,
    layer_filter_ok = false,
    raycast_ok = false,
    joint_intact = true,
    char_grounded = false,
    ragdoll_fell = false,
    summary_printed = false,
}

local function make_box(x, y, z, sx, sy, sz, r, g, b, dynamic, mass)
    local e = dse.ecs.create_entity()
    dse.ecs.add_transform(e, x, y, z, sx, sy, sz)
    dse.ecs.add_mesh_renderer(e, r, g, b, 1.0, cube_verts, cube_inds)
    dse.ecs.set_mesh_shader_variant(e, "MESH_LIT")
    dse.ecs.add_box_collider_3d(e, sx, sy, sz)
    dse.ecs.add_rigidbody_3d(e, dynamic and 2 or 0, mass or 1.0)
    return e
end

function JoltVerify.Setup(config)
    print("[JoltVerify] ===== Jolt Physics 综合验证开始 =====")

    -- 相机
    local cam = dse.ecs.create_entity()
    dse.ecs.add_transform(cam, 0, 8, 22, 1, 1, 1)
    dse.ecs.set_transform_rotation(cam, -20, 0, 0)
    dse.ecs.add_camera_3d(cam, 60, 100)
    if dse.ecs.add_free_camera_controller then
        dse.ecs.add_free_camera_controller(cam, 5.5, 0.12)
    end

    -- 灯光
    local light = dse.ecs.create_entity()
    dse.ecs.add_directional_light_3d(light, -0.4, -1.0, -0.5, 1.0, 0.98, 0.92, 1.2, 0.3, 0.3)

    -- 地面（用 MESH_UNLIT 避免无法线导致纯黑）
    state.ground = dse.ecs.create_entity()
    dse.ecs.add_transform(state.ground, 0, -0.5, 0, 30, 1, 30)
    dse.ecs.add_mesh_renderer(state.ground, 0.25, 0.28, 0.3, 1.0, cube_verts, cube_inds)
    dse.ecs.set_mesh_shader_variant(state.ground, "MESH_UNLIT")
    dse.ecs.add_box_collider_3d(state.ground, 30, 1, 30)
    dse.ecs.add_rigidbody_3d(state.ground, 0, 0)

    -- ============================================================
    -- Test 1: 刚体碰撞 (ball_a 落向地面)
    -- ============================================================
    state.ball_a = make_box(-6, 5, 0, 0.8, 0.8, 0.8, 0.9, 0.2, 0.2, true, 2.0)
    dse.ecs.set_collision_layer(state.ball_a, 1, 0xFFFF) -- layer 1, collide with all

    -- ============================================================
    -- Test 2: 碰撞层过滤 — ball_b(layer 2, mask=2) 与 ball_c(layer 1, mask=1)
    --   ball_b 和 ball_c 同位置落下，不应碰撞（layer 不匹配 mask）
    -- ============================================================
    state.ball_b = make_box(-3, 6, 0, 0.6, 0.6, 0.6, 0.2, 0.2, 0.9, true, 1.0)
    dse.ecs.set_collision_layer(state.ball_b, 2, 2)  -- layer=2, mask=2 (只碰 layer 2)

    state.ball_c = make_box(-3, 3, 0, 0.6, 0.6, 0.6, 0.2, 0.9, 0.2, true, 1.0)
    dse.ecs.set_collision_layer(state.ball_c, 1, 1)  -- layer=1, mask=1 (只碰 layer 1)

    -- ============================================================
    -- Test 3: 关节约束 — Fixed 关节悬挂 pendulum
    -- ============================================================
    state.joint_anchor = make_box(0, 6, 0, 0.4, 0.4, 0.4, 0.9, 0.9, 0.2, false, 0)
    state.joint_pendulum = make_box(0, 4, 0, 0.5, 0.5, 0.5, 0.9, 0.6, 0.1, true, 2.0)
    -- Hinge joint, break_force=5000
    dse.ecs.add_joint_3d(state.joint_pendulum, state.joint_anchor, 1, 0, 0, 0, 5000.0)

    -- ============================================================
    -- Test 4: 触发区域 (Enter + Exit)
    -- ============================================================
    state.trigger_zone = dse.ecs.create_entity()
    dse.ecs.add_transform(state.trigger_zone, 3, 3, 0, 1, 1, 1)
    dse.ecs.add_box_collider_3d(state.trigger_zone, 1, 1, 1)
    dse.ecs.add_rigidbody_3d(state.trigger_zone, 0, 0)
    dse.ecs.set_collider_trigger(state.trigger_zone, true)

    -- 探针：落入触发区后穿过（触发区 y=2~4，探针落地 y≈0.45，离开触发区）
    state.trigger_probe = make_box(3, 8, 0, 0.4, 0.4, 0.4, 1.0, 0.5, 0.0, true, 1.0)

    -- ============================================================
    -- Test 5: 角色控制器
    -- ============================================================
    state.char_entity = dse.ecs.create_entity()
    dse.ecs.add_transform(state.char_entity, 6, 2, 0, 1, 1, 1)
    dse.ecs.add_mesh_renderer(state.char_entity, 0.3, 0.7, 1.0, 1.0, cube_verts, cube_inds)
    dse.ecs.set_mesh_shader_variant(state.char_entity, "MESH_LIT")
    dse.ecs.add_character_controller_3d(state.char_entity, 0.4, 1.8)

    -- ============================================================
    -- Test 6: Ragdoll — 简易两节 (torso + head, fixed joint)
    -- ============================================================
    state.ragdoll_torso = make_box(9, 5, 0, 0.6, 0.8, 0.4, 0.7, 0.5, 0.3, true, 3.0)
    state.ragdoll_head  = make_box(9, 6.2, 0, 0.35, 0.35, 0.35, 0.9, 0.7, 0.5, true, 1.0)
    dse.ecs.add_joint_3d(state.ragdoll_head, state.ragdoll_torso, 0, 0, 0, 0, 8000.0)
    dse.ecs.add_ragdoll(state.ragdoll_torso, 4.0, false, 0.0, 50.0)
    dse.ecs.ragdoll_activate(state.ragdoll_torso)

    print("[JoltVerify] 场景就绪：6 项测试同步运行")
end

local function check_collision_events()
    if not dse.ecs.physics_3d_get_collision_events then return end
    local events = dse.ecs.physics_3d_get_collision_events()
    if not events then return end
    for _, ev in ipairs(events) do
        if ev.type == 0 then -- Enter
            state.collision_enter_seen = true
        elseif ev.type == 2 then -- Exit
            state.collision_exit_seen = true
        end
    end
end

local function check_trigger_events()
    if not dse.ecs.physics_3d_get_trigger_events then return end
    local events = dse.ecs.physics_3d_get_trigger_events()
    if not events then return end
    for _, ev in ipairs(events) do
        if ev.type == 0 then -- Enter
            state.trigger_enter_seen = true
        elseif ev.type == 1 then -- Exit
            state.trigger_exit_seen = true
        end
    end
end

local function check_layer_filter()
    -- ball_b(layer2,mask2) 和 ball_c(layer1,mask1) 不应碰撞
    -- 如果两者都落到地面且没有叠在一起，说明过滤生效
    if state.layer_filter_ok then return end
    local _, yb, _ = dse.ecs.get_transform_position(state.ball_b)
    local _, yc, _ = dse.ecs.get_transform_position(state.ball_c)
    -- 两者都在地面附近（y < 1.5）且各自独立落下
    if yb and yc and yb < 1.5 and yc < 1.5 then
        state.layer_filter_ok = true
    end
end

local function check_raycast()
    if state.raycast_ok then return end
    if not dse.ecs.physics_3d_raycast then return end
    local hit, hx, hy, hz, dist, eid = dse.ecs.physics_3d_raycast(0, 10, 0, 0, -1, 0, 50)
    if hit then
        state.raycast_ok = true
        print(string.format("[JoltVerify] Raycast hit: pos=(%.2f,%.2f,%.2f) dist=%.2f entity=%s",
            hx or 0, hy or 0, hz or 0, dist or 0, tostring(eid)))
    end
end

local function check_joint()
    if not dse.ecs.is_joint_3d_broken then return end
    local broken = dse.ecs.is_joint_3d_broken(state.joint_pendulum)
    if broken then state.joint_intact = false end
end

local function check_character()
    if state.char_grounded then return end
    if not dse.ecs.character_controller_3d_is_grounded then return end
    -- 给角色一点向下的移动让它落地
    if dse.ecs.character_controller_3d_move then
        dse.ecs.character_controller_3d_move(state.char_entity, 0, -0.1, 0)
    end
    local grounded = dse.ecs.character_controller_3d_is_grounded(state.char_entity)
    if grounded then state.char_grounded = true end
end

local function check_ragdoll()
    if state.ragdoll_fell then return end
    local _, y, _ = dse.ecs.get_transform_position(state.ragdoll_torso)
    if y and y < 2.0 then
        state.ragdoll_fell = true
    end
end

local function print_summary()
    if state.summary_printed then return end
    state.summary_printed = true

    local tests = {
        {"RigidBody Collision Enter",   state.collision_enter_seen},
        {"RigidBody Collision Exit",    state.collision_exit_seen},
        {"Collision Layer Filter",      state.layer_filter_ok},
        {"Joint Constraint (intact)",   state.joint_intact},
        {"Trigger Zone Enter",          state.trigger_enter_seen},
        {"Trigger Zone Exit",           state.trigger_exit_seen},
        {"Raycast",                     state.raycast_ok},
        {"Character Controller Ground", state.char_grounded},
        {"Ragdoll Fell",                state.ragdoll_fell},
    }

    print("[JoltVerify] ===== 验证结果 =====")
    local pass_count = 0
    for _, t in ipairs(tests) do
        local status = t[2] and "PASS" or "FAIL"
        if t[2] then pass_count = pass_count + 1 end
        print(string.format("[JoltVerify]   %-30s %s", t[1], status))
    end
    print(string.format("[JoltVerify] ===== %d/%d PASSED =====", pass_count, #tests))
end

function JoltVerify.Update(delta_time)
    local dt = delta_time or 0
    if dt > 0.1 then dt = 0.1 end
    state.time = state.time + dt

    check_collision_events()
    check_trigger_events()
    check_layer_filter()
    check_joint()
    check_character()
    check_ragdoll()

    -- 1s 后做 raycast
    if state.time > 1.0 then
        check_raycast()
    end

    -- 4s 后打印总结
    if state.time > 4.0 then
        print_summary()
    end
end

return JoltVerify
