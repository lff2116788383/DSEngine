-- Compute Shader 间接验证 Demo（通过 Grass System 展示 GPU Compute 驱动）
-- 引擎的 Grass System 内部使用 compute shader 进行:
--   1. 草叶 instance 生成与空间分配
--   2. 风力动画计算
--   3. LOD 距离剔除
-- 本 demo 验证 compute 管线完整性
local ComputeDemo = {}


ComputeDemo._meta = {
    name     = "Compute Shader 间接验证 Demo（通过 Grass System 展示 GPU Compute 驱动）",
    category = "compute",
    config   = { camera_distance=18.0 },
}

local state = {
    camera = nil,
    light  = nil,
    terrain_entity = nil,
    grass_entity   = nil,
    time = 0.0,
    logged_stats = false,
}

local cube_v = {
    -0.5,-0.5, 0.5,  0.5,-0.5, 0.5,  0.5, 0.5, 0.5, -0.5, 0.5, 0.5,
    -0.5,-0.5,-0.5,  0.5,-0.5,-0.5,  0.5, 0.5,-0.5, -0.5, 0.5,-0.5,
}
local cube_i = {0,1,2,2,3,0, 1,5,6,6,2,1, 5,4,7,7,6,5, 4,0,3,3,7,4, 3,2,6,6,7,3, 4,5,1,1,0,4}

local function setup_camera(config)
    local e = dse.ecs.create_entity()
    local dist = (config and config.camera_distance) or 18.0
    dse.ecs.add_transform(e, 0, 8, dist, 1, 1, 1)
    dse.ecs.set_transform_rotation(e, -25, 0, 0)
    dse.ecs.add_camera_3d(e, 60, 150)
    if dse.ecs.add_free_camera_controller then
        dse.ecs.add_free_camera_controller(e, 6.0, 0.12)
    end
    state.camera = e
end

local function setup_scene()
    -- 方向光
    local light = dse.ecs.create_entity()
    dse.ecs.add_directional_light_3d(light, -0.35, -1.0, -0.4, 1.0, 0.97, 0.90, 1.15, 0.20, 0.35)
    state.light = light

    -- 地形（草地附着面）
    local terrain = dse.ecs.create_entity()
    dse.ecs.add_transform(terrain, 0, 0, 0, 1, 1, 1)
    dse.ecs.add_terrain(terrain)
    dse.ecs.set_terrain_params(terrain, 30, 30, 4, 128, 0.8)
    state.terrain_entity = terrain

    -- Grass system（内部走 compute shader）
    if dse.ecs.add_grass then
        local grass = dse.ecs.create_entity()
        dse.ecs.add_transform(grass, 0, 0, 0, 1, 1, 1)
        dse.ecs.add_grass(grass)
        dse.ecs.set_grass_params(grass, 2000, 0.6, 1.2, 0.08, 0.03)
        dse.ecs.set_grass_color(grass, 0.22, 0.55, 0.18, 0.35, 0.70, 0.25, 0.15, 0.40, 0.12)
        dse.ecs.set_grass_wind(grass, 1.0, 0.5, 1.5, 0.7)
        dse.ecs.set_grass_lod(grass, 40, 80, 120)
        dse.ecs.set_grass_enabled(grass, true)
        state.grass_entity = grass
        print("[Compute] Grass system 已创建 (内部 compute shader 驱动)")
    else
        print("[Compute] dse.ecs.add_grass 不可用, 降级为静态场景展示")
    end

    -- 几个装饰立方体
    local colors = {
        {0.6, 0.3, 0.15}, {0.5, 0.25, 0.1}, {0.45, 0.35, 0.2},
        {0.55, 0.28, 0.12}, {0.5, 0.32, 0.18},
    }
    for i = 1, 5 do
        local angle = (i - 1) * math.pi * 2 / 5
        local x = math.cos(angle) * 8
        local z = math.sin(angle) * 8
        local e = dse.ecs.create_entity()
        dse.ecs.add_transform(e, x, 0.8, z, 1.2, 1.6, 1.2)
        dse.ecs.add_mesh_renderer(e, colors[i][1], colors[i][2], colors[i][3], 1.0, cube_v, cube_i)
        dse.ecs.set_mesh_shader_variant(e, "MESH_LIT")
        dse.ecs.set_mesh_material(e, 0.0, 0.65, 1.0, 0.0, 0.0, 0.0, 0.0, true, false)
    end

    print("[Compute] 场景: 地形 + grass(compute) + 5 装饰物")
    print("[Compute] WASD 移动相机, 观察草地 LOD 与风力动画")
end

function ComputeDemo.Setup(config)
    setup_camera(config)
    setup_scene()
end

function ComputeDemo.Update(delta_time)
    local dt = delta_time or 0
    if dt > 0.1 then dt = 0.1 end
    state.time = state.time + dt

    -- 动态风向
    if state.grass_entity and dse.ecs.set_grass_wind then
        local wind_x = math.sin(state.time * 0.3) * 1.2
        local wind_z = math.cos(state.time * 0.2) * 0.8
        dse.ecs.set_grass_wind(state.grass_entity, wind_x, 0.5, wind_z, 0.7 + math.sin(state.time * 0.5) * 0.3)
    end

    -- 每 3 秒打印一次 grass 统计
    if state.grass_entity and dse.ecs.get_grass_stats and not state.logged_stats then
        if state.time > 2.0 then
            local stats = dse.ecs.get_grass_stats(state.grass_entity)
            if stats then
                print(string.format("[Compute] Grass instances: %d", stats))
            end
            state.logged_stats = true
        end
    end
end

return ComputeDemo
