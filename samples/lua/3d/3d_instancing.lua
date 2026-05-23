-- GPU Instancing 验证 Demo
-- 创建大量同 mesh + 同材质的实体，引擎自动合批为 instanced draw
-- 对比: 关闭 instancing 前后的 draw_calls 与 instanced_draw_calls
local InstancingDemo = {}


InstancingDemo._meta = {
    name     = "GPU Instancing 验证 Demo",
    category = "rendering",
    config   = { camera_distance=28.0 },
}

local state = {
    camera = nil,
    light  = nil,
    entities = {},
    time = 0.0,
    grid_size = 12,     -- 12×12×3 = 432 个立方体
    layers   = 3,
    logged = false,
}

local function setup_camera(config)
    local e = dse.ecs.create_entity()
    local dist = (config and config.camera_distance) or 28.0
    dse.ecs.add_transform(e, 0, 8, dist, 1, 1, 1)
    dse.ecs.set_transform_rotation(e, -18, 0, 0)
    dse.ecs.add_camera_3d(e, 60, 200)
    if dse.ecs.add_free_camera_controller then
        dse.ecs.add_free_camera_controller(e, 8.0, 0.12)
    end
    state.camera = e
end

local function setup_lights()
    local e = dse.ecs.create_entity()
    dse.ecs.add_directional_light_3d(e, -0.4, -1.0, -0.3, 1.0, 0.95, 0.88, 1.1, 0.18, 0.32)
    state.light = e
end

-- 手写 cube 顶点（pos*3 只，stride=3）
local cube_v = {
    -0.5,-0.5, 0.5,  0.5,-0.5, 0.5,  0.5, 0.5, 0.5, -0.5, 0.5, 0.5,
    -0.5,-0.5,-0.5,  0.5,-0.5,-0.5,  0.5, 0.5,-0.5, -0.5, 0.5,-0.5,
}
local cube_i = {0,1,2,2,3,0, 1,5,6,6,2,1, 5,4,7,7,6,5, 4,0,3,3,7,4, 3,2,6,6,7,3, 4,5,1,1,0,4}

local function create_instanced_grid()
    local n = state.grid_size
    local spacing = 1.6
    local offset = (n - 1) * spacing * 0.5

    for layer = 0, state.layers - 1 do
        for row = 0, n - 1 do
            for col = 0, n - 1 do
                local e = dse.ecs.create_entity()
                local x = col * spacing - offset
                local y = layer * spacing + 0.5
                local z = row * spacing - offset
                dse.ecs.add_transform(e, x, y, z, 0.7, 0.7, 0.7)
                -- 所有实体使用相同 mesh + 相同材质参数 → 触发 GPU instancing
                dse.ecs.add_mesh_renderer(e, 0.3, 0.55, 0.9, 1.0, cube_v, cube_i)
                dse.ecs.set_mesh_shader_variant(e, "MESH_LIT")
                dse.ecs.set_mesh_material(e, 0.1, 0.45, 1.0, 0.0, 0.0, 0.0, 0.0, true, false)
                table.insert(state.entities, e)
            end
        end
    end

    -- 地面
    local ground = dse.ecs.create_entity()
    dse.ecs.add_transform(ground, 0, -0.2, 0, 30, 0.1, 30)
    dse.ecs.add_mesh_renderer(ground, 0.25, 0.28, 0.3, 1.0, cube_v, cube_i)
    dse.ecs.set_mesh_shader_variant(ground, "MESH_LIT")
    dse.ecs.set_mesh_material(ground, 0.0, 0.7, 1.0, 0.0, 0.0, 0.0, 0.0, true, false)

    local total = n * n * state.layers
    print(string.format("[Instancing] 创建 %d 个同 mesh 同材质立方体 (grid %dx%dx%d)",
        total, n, n, state.layers))
    print("[Instancing] 引擎自动检测同 mesh_path+材质 → GPU instanced draw")
    print("[Instancing] WASD 移动相机观察")
end

function InstancingDemo.Setup(config)
    setup_camera(config)
    setup_lights()
    create_instanced_grid()
end

function InstancingDemo.Update(delta_time)
    local dt = delta_time or 0
    if dt > 0.1 then dt = 0.1 end
    state.time = state.time + dt

    -- 让立方体缓慢波动，展示 instancing 仍支持独立 transform
    local n = state.grid_size
    local spacing = 1.6
    local offset = (n - 1) * spacing * 0.5
    local idx = 1
    for layer = 0, state.layers - 1 do
        for row = 0, n - 1 do
            for col = 0, n - 1 do
                local e = state.entities[idx]
                if e then
                    local base_y = layer * spacing + 0.5
                    local wave = math.sin(state.time * 1.5 + col * 0.4 + row * 0.3) * 0.3
                    local x = col * spacing - offset
                    local z = row * spacing - offset
                    dse.ecs.set_transform_position(e, x, base_y + wave, z)
                end
                idx = idx + 1
            end
        end
    end
end

return InstancingDemo
