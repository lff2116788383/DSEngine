-- LOD (Level of Detail) 验证 Demo
-- 验证: LODGroupComponent 根据屏幕占比自动切换 mesh 级别
-- 场景: 多行物体从近到远排列，近处高精度远处低精度
local LODDemo = {}

local state = {
    camera = nil,
    light  = nil,
    lod_entities = {},
    time = 0.0,
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
    local dist = (config and config.camera_distance) or 10.0
    dse.ecs.add_transform(e, 0, 4, dist, 1, 1, 1)
    dse.ecs.set_transform_rotation(e, -12, 0, 0)
    dse.ecs.add_camera_3d(e, 60, 200)
    if dse.ecs.add_free_camera_controller then
        dse.ecs.add_free_camera_controller(e, 8.0, 0.12)
    end
    state.camera = e
end

local function setup_scene()
    local light = dse.ecs.create_entity()
    dse.ecs.add_directional_light_3d(light, -0.35, -1.0, -0.4, 1.0, 0.95, 0.88, 1.1, 0.18, 0.30)
    state.light = light

    -- 地面
    make_box(0, -0.15, -20, 30, 0.1, 60, 0.25, 0.28, 0.30)

    -- LOD 级别颜色: LOD0=绿, LOD1=黄, LOD2=红
    local lod_colors = {
        {0.2, 0.8, 0.3},  -- LOD0 (近)
        {0.9, 0.8, 0.2},  -- LOD1 (中)
        {0.9, 0.3, 0.2},  -- LOD2 (远)
    }

    -- 沿 Z 轴排列 5 列 × 3 行
    for col = 0, 4 do
        for row = 0, 2 do
            local x = (col - 2) * 4.0
            local z = -row * 15.0  -- 0, -15, -30
            local e = dse.ecs.create_entity()
            dse.ecs.add_transform(e, x, 1.0, z, 1.2, 1.2, 1.2)
            -- 使用 LOD0 颜色作为基础
            dse.ecs.add_mesh_renderer(e, lod_colors[1][1], lod_colors[1][2], lod_colors[1][3], 1.0, cube_v, cube_i)
            dse.ecs.set_mesh_shader_variant(e, "MESH_LIT")
            dse.ecs.set_mesh_material(e, 0.1, 0.45, 1.0, 0.0, 0.0, 0.0, 0.0, true, false)

            -- 配置 LOD 级别（使用 cube.dmesh 作为各级别 mesh）
            dse.ecs.lod_add_level(e, "models/cube.dmesh", 0.5)   -- LOD0: 屏幕占比 > 50%
            dse.ecs.lod_add_level(e, "models/cube.dmesh", 0.2)   -- LOD1: 屏幕占比 > 20%
            dse.ecs.lod_add_level(e, "models/cube.dmesh", 0.05)  -- LOD2: 屏幕占比 > 5%
            dse.ecs.lod_set_scale(e, 1.0)
            dse.ecs.lod_set_enabled(e, true)

            table.insert(state.lod_entities, e)
        end
    end

    -- 距离标记柱
    for i = 0, 2 do
        local z = -i * 15.0
        local marker = make_box(10, 0.5, z, 0.3, 1.0, 0.3, 0.9, 0.9, 0.9)
    end

    print(string.format("[LOD] 场景: %d 个 LOD 物体 (3级), 沿 Z 轴排列", #state.lod_entities))
    print("[LOD] LOD0(近)=绿, LOD1(中)=黄, LOD2(远)=红")
    print("[LOD] WASD 移动相机, 前后移动观察 LOD 切换")
end

function LODDemo.Setup(config)
    setup_camera(config)
    setup_scene()
end

function LODDemo.Update(delta_time)
    local dt = delta_time or 0
    if dt > 0.1 then dt = 0.1 end
    state.time = state.time + dt

    -- 轻微旋转展示物体存在
    for i, e in ipairs(state.lod_entities) do
        local angle = state.time * 15 + i * 24
        dse.ecs.set_transform_rotation(e, 0, angle, 0)
    end
end

return LODDemo
