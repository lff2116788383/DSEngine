-- Fog + Light Shaft 雾效与体积光 Demo
-- 验证: PostProcessComponent 的 fog/light_shaft 参数
-- 场景: 多层建筑 + 高度雾 + 太阳光轴
local FogDemo = {}

local state = {
    camera = nil,
    light  = nil,
    pp     = nil,
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
    dse.ecs.set_mesh_material(e, 0.0, 0.6, 1.0, 0.0, 0.0, 0.0, 0.0, true, false)
    return e
end

local function setup_camera(config)
    local e = dse.ecs.create_entity()
    local dist = (config and config.camera_distance) or 25.0
    dse.ecs.add_transform(e, 0, 6, dist, 1, 1, 1)
    dse.ecs.set_transform_rotation(e, -10, 0, 0)
    dse.ecs.add_camera_3d(e, 60, 200)
    if dse.ecs.add_free_camera_controller then
        dse.ecs.add_free_camera_controller(e, 8.0, 0.12)
    end

    -- PostProcess 组件挂在相机上
    dse.ecs.add_post_process(e, true, 0.9, 0.5, 1.2)

    -- Fog 参数
    -- set_post_process_fog(e, enabled, density, height_falloff, height_offset,
    --                      start, end, steps, sun_scatter, fog_r, fog_g, fog_b)
    dse.ecs.set_post_process_fog(e,
        true,   -- enabled
        0.025,  -- density
        0.8,    -- height_falloff
        0.0,    -- height_offset
        5.0,    -- start
        80.0,   -- end
        32,     -- steps
        0.6,    -- sun_scatter
        0.65, 0.72, 0.80  -- fog color (偏蓝灰)
    )

    -- Light Shaft 参数
    -- set_post_process_light_shaft(e, enabled, density, weight, decay, exposure,
    --                              intensity, samples, color_r, color_g, color_b)
    dse.ecs.set_post_process_light_shaft(e,
        true,   -- enabled
        0.8,    -- density
        0.6,    -- weight
        0.96,   -- decay
        0.15,   -- exposure
        0.8,    -- intensity
        64,     -- samples
        1.0, 0.95, 0.85  -- shaft color (暖白)
    )

    -- SSAO 增强深度感
    dse.ecs.set_post_process_ssao(e, true, 0.5, 0.025)

    state.camera = e
    state.pp = e
end

local function setup_scene()
    -- 低角度方向光（制造长光轴）
    local light = dse.ecs.create_entity()
    dse.ecs.add_directional_light_3d(light, -0.2, -0.4, -0.8, 1.0, 0.95, 0.85, 1.3, 0.12, 0.25)
    state.light = light

    -- 地面
    make_box(0, -0.15, -20, 50, 0.1, 80, 0.28, 0.30, 0.32)

    -- 远处建筑群（逐渐被雾淡化）
    local building_data = {
        -- 近处
        {-4,  2.5, -2,  2, 5,  2, 0.55, 0.50, 0.45},
        { 4,  3.5, -3,  2, 7,  2, 0.50, 0.48, 0.42},
        { 0,  1.5,  2,  3, 3,  2, 0.52, 0.47, 0.40},
        -- 中景
        {-8,  4.0, -15, 3, 8,  3, 0.48, 0.45, 0.40},
        { 6,  3.0, -18, 2, 6,  2, 0.50, 0.47, 0.42},
        { 0,  5.0, -20, 2, 10, 2, 0.45, 0.42, 0.38},
        {-3,  2.0, -22, 4, 4,  3, 0.47, 0.44, 0.40},
        -- 远景
        {-10, 6.0, -35, 3, 12, 3, 0.42, 0.40, 0.36},
        { 10, 4.0, -40, 4, 8,  3, 0.44, 0.42, 0.38},
        { 0,  7.0, -45, 3, 14, 3, 0.40, 0.38, 0.35},
        { 5,  3.5, -50, 5, 7,  4, 0.43, 0.41, 0.37},
        {-7,  5.0, -55, 3, 10, 3, 0.41, 0.39, 0.36},
    }

    for _, b in ipairs(building_data) do
        make_box(b[1], b[2], b[3], b[4], b[5], b[6], b[7], b[8], b[9])
    end

    -- 柱子（遮挡光线产生 light shaft）
    for i = 1, 6 do
        local x = (i - 3.5) * 5
        make_box(x, 4.0, -8, 0.6, 8.0, 0.6, 0.50, 0.48, 0.44)
    end

    print(string.format("[Fog] 场景: %d 建筑 + 6 柱子 + 高度雾 + 体积光轴",
        #building_data))
    print("[Fog] WASD 移动相机，远处建筑逐渐淡入雾中")
end

function FogDemo.Setup(config)
    setup_camera(config)
    setup_scene()
end

function FogDemo.Update(delta_time)
    local dt = delta_time or 0
    if dt > 0.1 then dt = 0.1 end
    state.time = state.time + dt

    -- 动态调整雾密度（模拟天气变化）
    if state.pp then
        local density = 0.025 + math.sin(state.time * 0.15) * 0.01
        dse.ecs.set_post_process_fog(state.pp, true, density)
    end
end

return FogDemo
