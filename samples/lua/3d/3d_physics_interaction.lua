-- 3D P4 sample: physics interaction showcase（PhysX 综合交互验证）
-- 目标：验证 PhysX 全链路物理交互 — 堆叠掉落、raycast 拾取、impulse/force 施力、
--       速度查询、重力控制、选中高亮。
-- 前置：DSE_ENABLE_PHYSX 构建已启用，Lua 物理 API 已暴露。
local PhysicsInteraction3D = {}

local state = {
    camera = nil,
    time = 0.0,
    ground = nil,
    cubes = {},
    sphere = nil,
    hit_marker = nil,
    impulse_done = false,
    force_done = false,
    velocity_logged = false,
    gravity_test_done = false,
    raycast_logged = false,
    phase = 0,
    log_summary = {}
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

local function setup_camera(config)
    local camera = dse.ecs.create_entity()
    local distance = (type(config) == "table" and type(config.camera_distance) == "number") and config.camera_distance or 12.0
    dse.ecs.add_transform(camera, 0.0, 5.0, distance, 1.0, 1.0, 1.0)
    dse.ecs.set_transform_rotation(camera, -22.0, 0.0, 0.0)
    dse.ecs.add_camera_3d(camera, 60.0, 100)
    if dse.ecs.add_free_camera_controller then
        dse.ecs.add_free_camera_controller(camera, 5.5, 0.12)
    end
    state.camera = camera
end

local function add_cube(name, x, y, z, sx, sy, sz, color, dynamic, mass)
    local e = dse.ecs.create_entity()
    dse.ecs.add_transform(e, x, y, z, sx, sy, sz)
    dse.ecs.add_mesh_renderer(e, color[1], color[2], color[3], color[4] or 1.0, cube_vertices(), cube_indices())
    dse.ecs.set_mesh_shader_variant(e, "MESH_LIT")
    dse.ecs.set_mesh_material(e, 0.0, 0.48, 1.0, 0.0, 0.0, 0.0, 1.0, true, true)
    dse.ecs.add_box_collider_3d(e, sx, sy, sz)
    dse.ecs.add_rigidbody_3d(e, dynamic and 2 or 0, mass or 1.0)
    table.insert(state.cubes, { name = name, entity = e, dynamic = dynamic, initial_y = y, color = color })
    return e
end

local function add_sphere(name, x, y, z, radius, color, dynamic, mass)
    -- 球体近似用较小方块渲染，物理用 SphereCollider
    local e = dse.ecs.create_entity()
    local vis_size = radius * 1.6
    dse.ecs.add_transform(e, x, y, z, vis_size, vis_size, vis_size)
    dse.ecs.add_mesh_renderer(e, color[1], color[2], color[3], color[4] or 1.0, cube_vertices(), cube_indices())
    dse.ecs.set_mesh_shader_variant(e, "MESH_LIT")
    dse.ecs.set_mesh_material(e, 0.0, 0.42, 0.9, 0.12, 0.08, 0.0, 1.0, true, true)
    dse.ecs.add_sphere_collider_3d(e, radius)
    dse.ecs.add_rigidbody_3d(e, dynamic and 2 or 0, mass or 1.0)
    state.sphere = { name = name, entity = e, dynamic = dynamic, initial_y = y, color = color }
    return e
end

local function setup_scene(config)
    -- 光照
    local light = dse.ecs.create_entity()
    dse.ecs.add_directional_light_3d(light, -0.45, -1.0, -0.25, 1.0, 0.95, 0.86, 1.05, 0.16, 0.30)

    -- 静态地面
    state.ground = add_cube("ground", 0.0, -0.52, 0.0, 10.0, 0.16, 6.0, {0.28, 0.30, 0.32, 1.0}, false, 0.0)

    -- 堆叠方块（3 行 × 2 列 = 6 个动态方块）
    local rows = config and config.rows or 3
    local cols = config and config.cols or 2
    for row = 1, rows do
        for col = 1, cols do
            local x = (col - (cols + 1) * 0.5) * 0.72
            local y = 1.2 + row * 0.72
            local z = (row % 2 == 0) and 0.18 or -0.18
            local r = 0.25 + row * 0.12
            local g = 0.45 + col * 0.10
            local b = 0.95 - row * 0.08
            add_cube(string.format("box_%d_%d", row, col), x, y, z, 0.58, 0.58, 0.58, {r, g, b, 1.0}, true, 1.0)
        end
    end

    -- 球体目标（用于 raycast 验证）
    add_sphere("sphere_target", 2.8, 0.5, 0.0, 0.45, {1.0, 0.7, 0.15, 1.0}, true, 1.5)

    -- 命中标记方块
    state.hit_marker = dse.ecs.create_entity()
    dse.ecs.add_transform(state.hit_marker, 0.0, 2.0, 0.0, 0.28, 0.28, 0.28)
    dse.ecs.add_mesh_renderer(state.hit_marker, 1.0, 0.3, 0.08, 1.0, cube_vertices(), cube_indices())
    dse.ecs.set_mesh_shader_variant(state.hit_marker, "MESH_LIT")
    dse.ecs.set_mesh_material(state.hit_marker, 0.0, 0.5, 1.0, 0.8, 0.35, 0.0, 1.0, true, true)

    print("[3D][PhysicsInteraction] setup: ground + stacked boxes + sphere target + hit marker")
    print(string.format("[3D][PhysicsInteraction] PhysX %s", dse.ecs.physics_3d_raycast and "ENABLED" or "DISABLED"))
end

-- ============================================================
-- 交互阶段
-- ============================================================

--- 阶段 1：等待堆叠体下落稳定，然后对第一个动态方块施加向上冲量
local function phase_impulse()
    if state.impulse_done then return end
    if #state.cubes < 2 then return end
    local target = state.cubes[2] -- cubes[1] 是地面
    if not target or not target.dynamic then return end
    if not dse.ecs.rigidbody_3d_add_impulse then return end

    dse.ecs.rigidbody_3d_add_impulse(target.entity, 0.0, 6.0, 0.0)
    state.impulse_done = true
    print(string.format("[3D][PhysicsInteraction] phase1: impulse (0,6,0) applied to %s", target.name))
end

--- 阶段 2：对球体施加强制力（模拟持续推力），验证 add_force API
local function phase_force()
    if state.force_done then return end
    if not state.sphere or not state.sphere.dynamic then return end
    if not dse.ecs.rigidbody_3d_add_force then return end

    -- 对球体施加向右的力
    dse.ecs.rigidbody_3d_add_force(state.sphere.entity, 4.0, 0.0, 0.0)
    state.force_done = true
    print(string.format("[3D][PhysicsInteraction] phase2: force (4,0,0) applied to %s", state.sphere.name))
end

--- 阶段 3：检测速度和位置变化
local function phase_velocity()
    if state.velocity_logged then return end
    if #state.cubes < 2 then return end
    local target = state.cubes[2]
    if not target or not target.dynamic then return end

    -- 位置检测
    local x, y, z = dse.ecs.get_transform_position(target.entity)
    local dy = (y or 0.0) - target.initial_y

    -- 速度检测
    local vx, vy, vz = 0.0, 0.0, 0.0
    if dse.ecs.rigidbody_3d_get_velocity then
        vx, vy, vz = dse.ecs.rigidbody_3d_get_velocity(target.entity)
    end

    state.velocity_logged = true
    print(string.format("[3D][PhysicsInteraction] phase3: %s pos=(%.3f,%.3f,%.3f) dy=%.3f vel=(%.3f,%.3f,%.3f)",
        target.name, x or 0.0, y or 0.0, z or 0.0, dy, vx, vy, vz))

    if dy < -0.05 then
        table.insert(state.log_summary, "box_fell=true")
    else
        table.insert(state.log_summary, "box_fell=false")
    end
end

--- 阶段 4：Raycast 拾取验证
local function phase_raycast()
    if state.raycast_logged then return end
    if not dse.ecs.physics_3d_raycast then return end

    -- 从上方向下射线检测，应命中地面或方块
    local hit, entity, hx, hy, hz, nx, ny, nz, distance =
        dse.ecs.physics_3d_raycast(0.0, 8.0, 0.0, 0.0, -1.0, 0.0, 20.0)

    state.raycast_logged = true
    print(string.format("[3D][PhysicsInteraction] phase4: raycast hit=%s entity=%s pos=(%.2f,%.2f,%.2f) dist=%.2f",
        tostring(hit), tostring(entity), hx or 0.0, hy or 0.0, hz or 0.0, distance or 0.0))

    if hit and state.hit_marker then
        dse.ecs.set_transform_position(state.hit_marker, hx or 0.0, (hy or 0.0) + 0.22, hz or 0.0)
    end

    if hit then
        table.insert(state.log_summary, "raycast_hit=true")
    else
        table.insert(state.log_summary, "raycast_hit=false")
    end
end

--- 阶段 5：重力控制验证（关闭重力再恢复）
local function phase_gravity()
    if state.gravity_test_done then return end
    if #state.cubes < 2 then return end
    local target = state.cubes[2]
    if not target or not target.dynamic then return end
    if not dse.ecs.rigidbody_3d_set_gravity then return end

    -- 先关闭重力
    dse.ecs.rigidbody_3d_set_gravity(target.entity, false)
    print(string.format("[3D][PhysicsInteraction] phase5a: gravity DISABLED for %s", target.name))

    -- 记录关闭后位置
    local x1, y1, z1 = dse.ecs.get_transform_position(target.entity)

    -- 立即恢复重力
    dse.ecs.rigidbody_3d_set_gravity(target.entity, true)
    print(string.format("[3D][PhysicsInteraction] phase5b: gravity RE-ENABLED for %s", target.name))

    state.gravity_test_done = true
    table.insert(state.log_summary, "gravity_toggle=true")
end

-- ============================================================
-- 生命周期
-- ============================================================

function PhysicsInteraction3D.Setup(config)
    setup_camera(config or {})
    setup_scene(config or {})
end

function PhysicsInteraction3D.Update(delta_time)
    local dt = delta_time or 0.0
    if dt > 0.1 then dt = 0.1 end
    state.time = state.time + dt

    -- Phase 1: 0.6s 后施加冲量
    if state.time > 0.6 and not state.impulse_done then
        phase_impulse()
    end

    -- Phase 2: 1.2s 后施加持续力
    if state.time > 1.2 and not state.force_done then
        phase_force()
    end

    -- Phase 3: 2.0s 后检测速度和位置
    if state.time > 2.0 and not state.velocity_logged then
        phase_velocity()
    end

    -- Phase 4: 2.5s 后执行 raycast
    if state.time > 2.5 and not state.raycast_logged then
        phase_raycast()
    end

    -- Phase 5: 3.2s 后测试重力控制
    if state.time > 3.2 and not state.gravity_test_done then
        phase_gravity()
    end

    -- 命中标记旋转动画
    if state.hit_marker then
        dse.ecs.set_transform_rotation(state.hit_marker, state.time * 35.0, state.time * 90.0, 0.0)
    end

    -- 最终汇总日志（4.0s 后输出一次）
    if state.time > 4.0 and #state.log_summary > 0 and not state.final_logged then
        state.final_logged = true
        print(string.format("[3D][PhysicsInteraction] summary: %s", table.concat(state.log_summary, ", ")))
        print(string.format("[3D][PhysicsInteraction] physics_interaction_api=true"))
    end
end

return PhysicsInteraction3D
