-- Water 水面渲染 Demo
-- 验证: WaterComponent 的波浪、折射、反射、焦散、泡沫、水下雾效
-- 场景: 地形岛 + 水面 + 浮在水面的立方体
local WaterDemo = {}

local state = {
    camera = nil,
    light  = nil,
    water  = nil,
    floats = {},
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
    local dist = (config and config.camera_distance) or 22.0
    dse.ecs.add_transform(e, 0, 8, dist, 1, 1, 1)
    dse.ecs.set_transform_rotation(e, -20, 0, 0)
    dse.ecs.add_camera_3d(e, 60, 200)
    if dse.ecs.add_free_camera_controller then
        dse.ecs.add_free_camera_controller(e, 8.0, 0.12)
    end
    state.camera = e
end

local function setup_scene()
    -- 方向光
    local light = dse.ecs.create_entity()
    dse.ecs.add_directional_light_3d(light, -0.35, -1.0, -0.5, 1.0, 0.95, 0.88, 1.2, 0.18, 0.30)
    state.light = light

    -- 地形岛（中央隆起）
    make_box(0, -1.0, 0, 8, 2.0, 8, 0.45, 0.40, 0.30)   -- 岛体
    make_box(0, 0.15, 0, 6, 0.3, 6, 0.35, 0.50, 0.25)    -- 草皮
    -- 远处海底
    make_box(0, -3.0, 0, 60, 0.2, 60, 0.20, 0.22, 0.25)

    -- 岛上装饰物
    make_box(-1.5, 1.2, 1.0, 0.5, 2.0, 0.5, 0.55, 0.35, 0.20)
    make_box( 1.5, 0.8, -1.0, 0.6, 1.2, 0.6, 0.50, 0.30, 0.18)

    -- 水面
    local water = dse.ecs.create_entity()
    dse.ecs.add_transform(water, 0, 0, 0, 1, 1, 1)
    dse.ecs.add_water(water)
    -- set_water 参数:
    -- enabled, water_level, deep_r/g/b, shallow_r/g/b, max_depth, transparency,
    -- wave_amplitude, wave_frequency, wave_speed, wave_dir_x, wave_dir_y,
    -- refraction_strength, reflection_strength, specular_power,
    -- caustic_intensity, caustic_scale, foam_intensity, foam_depth_threshold,
    -- underwater_fog_density, underwater_fog_r/g/b
    dse.ecs.set_water(water,
        true,           -- enabled
        -0.3,           -- water_level
        0.02, 0.08, 0.18,  -- deep color (deep blue)
        0.10, 0.30, 0.35,  -- shallow color (turquoise)
        5.0,            -- max_depth
        0.6,            -- transparency
        0.15,           -- wave_amplitude
        2.5,            -- wave_frequency
        1.2,            -- wave_speed
        0.7, 0.3,       -- wave direction
        0.08,           -- refraction_strength
        0.5,            -- reflection_strength
        64.0,           -- specular_power
        0.3,            -- caustic_intensity
        8.0,            -- caustic_scale
        0.4,            -- foam_intensity
        0.8,            -- foam_depth_threshold
        0.15,           -- underwater_fog_density
        0.03, 0.08, 0.15  -- underwater_fog_color
    )
    state.water = water
    print("[Water] 水面: level=-0.3, wave_amp=0.15, freq=2.5, speed=1.2")

    -- 浮在水面的箱子
    local float_colors = {
        {0.8, 0.5, 0.2}, {0.6, 0.3, 0.15}, {0.7, 0.4, 0.1},
        {0.5, 0.35, 0.2}, {0.65, 0.45, 0.15},
    }
    for i, c in ipairs(float_colors) do
        local angle = (i - 1) * math.pi * 2 / #float_colors
        local x = math.cos(angle) * 8.0
        local z = math.sin(angle) * 8.0
        local e = make_box(x, -0.1, z, 0.8, 0.6, 0.8, c[1], c[2], c[3])
        table.insert(state.floats, {entity = e, base_x = x, base_z = z, phase = i * 0.8})
    end

    print(string.format("[Water] 场景: 岛 + 水面 + %d 浮箱", #state.floats))
    print("[Water] WASD 移动相机，观察波浪/反射/折射/焦散效果")
end

function WaterDemo.Setup(config)
    setup_camera(config)
    setup_scene()
end

function WaterDemo.Update(delta_time)
    local dt = delta_time or 0
    if dt > 0.1 then dt = 0.1 end
    state.time = state.time + dt

    -- 浮箱随波浮动
    for _, f in ipairs(state.floats) do
        local bob = math.sin(state.time * 1.5 + f.phase) * 0.12
        local sway_x = math.sin(state.time * 0.4 + f.phase) * 0.3
        dse.ecs.set_transform_position(f.entity, f.base_x + sway_x, -0.1 + bob, f.base_z)
        local roll = math.sin(state.time * 1.2 + f.phase) * 5
        dse.ecs.set_transform_rotation(f.entity, roll, state.time * 8, 0)
    end
end

return WaterDemo
