-- GPU Culling 验证 Demo（Hi-Z Occlusion Culling + Frustum Culling）
-- 场景：大量物体分布在遮挡墙后面，Hi-Z 自动剔除不可见物体
-- 观察：绕墙观察时 draw_calls 和 gpu_culled_count 会动态变化
local GPUCullingDemo = {}


GPUCullingDemo._meta = {
    name     = "GPU Culling 验证 Demo（Hi-Z Occlusion Culling + Frustum Cull...",
    category = "compute",
    config   = { camera_distance=20.0 },
}

local state = {
    camera = nil,
    light  = nil,
    occluders = {},
    hidden_objects = {},
    visible_objects = {},
    time = 0.0,
    logged = false,
}

local cube_v = {
    -0.5,-0.5, 0.5,  0.5,-0.5, 0.5,  0.5, 0.5, 0.5, -0.5, 0.5, 0.5,
    -0.5,-0.5,-0.5,  0.5,-0.5,-0.5,  0.5, 0.5,-0.5, -0.5, 0.5,-0.5,
}
local cube_i = {0,1,2,2,3,0, 1,5,6,6,2,1, 5,4,7,7,6,5, 4,0,3,3,7,4, 3,2,6,6,7,3, 4,5,1,1,0,4}

local function make_box(x, y, z, sx, sy, sz, r, g, b)
    local e = dse.ecs.create_entity()
    dse.ecs.add_transform(e, x, y, z, sx, sy, sz)
    dse.ecs.add_mesh_renderer(e, r, g, b, 1.0, cube_v, cube_i)
    dse.ecs.set_mesh_shader_variant(e, "MESH_LIT")
    dse.ecs.set_mesh_material(e, 0.0, 0.5, 1.0, 0.0, 0.0, 0.0, 0.0, true, false)
    return e
end

local function setup_camera(config)
    local e = dse.ecs.create_entity()
    local dist = (config and config.camera_distance) or 20.0
    dse.ecs.add_transform(e, 0, 5, dist, 1, 1, 1)
    dse.ecs.set_transform_rotation(e, -15, 0, 0)
    dse.ecs.add_camera_3d(e, 60, 150)
    if dse.ecs.add_free_camera_controller then
        dse.ecs.add_free_camera_controller(e, 8.0, 0.12)
    end
    state.camera = e
end

local function setup_scene()
    -- 方向光
    local light = dse.ecs.create_entity()
    dse.ecs.add_directional_light_3d(light, -0.35, -1.0, -0.5, 1.0, 0.95, 0.88, 1.1, 0.18, 0.30)
    state.light = light

    -- 地面
    make_box(0, -0.2, 0, 50, 0.1, 50, 0.22, 0.25, 0.27)

    -- 3 面遮挡墙（高墙，挡住后方物体）
    local wall_h = 8.0
    local wall_w = 12.0
    local wall_d = 0.5
    table.insert(state.occluders, make_box(0, wall_h/2, -5, wall_w, wall_h, wall_d, 0.55, 0.55, 0.58))
    table.insert(state.occluders, make_box(-8, wall_h/2, -12, wall_d, wall_h, wall_w, 0.55, 0.55, 0.58))
    table.insert(state.occluders, make_box( 8, wall_h/2, -12, wall_d, wall_h, wall_w, 0.55, 0.55, 0.58))

    -- 墙后方：大量隐藏物体（应被 Hi-Z 剔除）
    local count = 0
    for row = 0, 7 do
        for col = 0, 7 do
            local x = (col - 3.5) * 2.0
            local z = -12 - row * 2.0
            local e = make_box(x, 0.8, z, 0.8, 0.8, 0.8, 0.85, 0.35, 0.25)
            table.insert(state.hidden_objects, e)
            count = count + 1
        end
    end

    -- 墙前方：可见物体
    for i = 1, 6 do
        local x = (i - 3.5) * 3.0
        local e = make_box(x, 0.8, 5, 0.9, 0.9, 0.9, 0.25, 0.7, 0.4)
        table.insert(state.visible_objects, e)
    end

    print(string.format("[GPUCulling] 场景: %d 个遮挡墙, %d 个被遮挡物体, %d 个可见物体",
        #state.occluders, #state.hidden_objects, #state.visible_objects))
    print("[GPUCulling] Hi-Z 遮挡剔除 + Frustum Culling 自动生效")
    print("[GPUCulling] WASD 移动相机，绕到墙后观察 draw_calls 变化")
end

function GPUCullingDemo.Setup(config)
    setup_camera(config)
    setup_scene()
end

function GPUCullingDemo.Update(delta_time)
    local dt = delta_time or 0
    if dt > 0.1 then dt = 0.1 end
    state.time = state.time + dt

    -- 让被遮挡物体缓慢旋转，证明它们确实存在
    for i, e in ipairs(state.hidden_objects) do
        local angle = state.time * 30 + i * 15
        dse.ecs.set_transform_rotation(e, 0, angle, 0)
    end
end

return GPUCullingDemo
