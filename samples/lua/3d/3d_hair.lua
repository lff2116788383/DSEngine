-- Hair 毛发渲染 Demo
-- 验证: HairComponent 的物理/渲染/风力/LOD 参数
-- 场景: 多个实体附带不同参数的毛发组件
local HairDemo = {}


HairDemo._meta = {
    name     = "Hair 毛发渲染 Demo",
    category = "rendering",
    config   = { camera_distance=10.0 },
}

local state = {
    camera = nil,
    light  = nil,
    hair_entities = {},
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
    dse.ecs.add_transform(e, 0, 3.5, dist, 1, 1, 1)
    dse.ecs.set_transform_rotation(e, -10, 0, 0)
    dse.ecs.add_camera_3d(e, 60, 80)
    if dse.ecs.add_free_camera_controller then
        dse.ecs.add_free_camera_controller(e, 5.0, 0.12)
    end
    state.camera = e
end

local function setup_scene()
    local light = dse.ecs.create_entity()
    dse.ecs.add_directional_light_3d(light, -0.35, -1.0, -0.4, 1.0, 0.95, 0.88, 1.1, 0.18, 0.28)
    state.light = light

    -- 地面
    make_box(0, -0.15, 0, 18, 0.1, 12, 0.25, 0.28, 0.30)

    -- 毛发参数配置
    local hair_configs = {
        {name="Short Stiff",  color={0.15, 0.10, 0.05}, stiffness=0.8, damping=0.5, length_scale=0.5},
        {name="Medium Soft",  color={0.45, 0.25, 0.10}, stiffness=0.3, damping=0.3, length_scale=1.0},
        {name="Long Flowing", color={0.60, 0.40, 0.15}, stiffness=0.1, damping=0.2, length_scale=1.5},
        {name="Curly Dense",  color={0.08, 0.06, 0.04}, stiffness=0.5, damping=0.4, length_scale=0.8},
    }

    for i, hc in ipairs(hair_configs) do
        local x = (i - 2.5) * 4.0

        -- 头部基座
        local head = make_box(x, 2.5, 0, 1.2, 1.6, 1.2, 0.75, 0.60, 0.50)

        -- 毛发实体
        local hair = dse.ecs.create_entity()
        dse.ecs.add_transform(hair, x, 3.5, 0, 1, 1, 1)
        dse.ecs.add_mesh_renderer(hair, hc.color[1], hc.color[2], hc.color[3], 1.0, cube_v, cube_i)
        dse.ecs.set_mesh_shader_variant(hair, "MESH_LIT")

        -- 添加 Hair 组件
        dse.ecs.add_hair(hair, "models/cube.dmesh", 4)
        -- set_hair_physics(e, stiffness, damping, gravity_factor, strand_length, substeps)
        dse.ecs.set_hair_physics(hair, hc.stiffness, hc.damping, 1.0, hc.length_scale, 4)
        -- set_hair_render(e, thickness, tip_thickness, opacity, ao_strength, scatter)
        dse.ecs.set_hair_render(hair, 0.02, 0.005, 0.9, 0.3, 0.5)
        -- set_hair_wind(e, wind_x, wind_y, wind_z, turbulence, frequency)
        dse.ecs.set_hair_wind(hair, 1.0, 0.0, 0.3, 0.4, 2.0)
        dse.ecs.set_hair_enabled(hair, true)
        -- set_hair_lod(e, lod0_dist, lod1_dist, lod2_dist, cull_dist)
        dse.ecs.set_hair_lod(hair, 5.0, 15.0, 30.0, 50.0)

        table.insert(state.hair_entities, {
            entity = hair,
            name = hc.name,
            base_x = x,
        })

        -- 名称标记
        make_box(x, 0.1, 2.5, 0.15, 0.2, 0.15, 0.9, 0.9, 0.9)
    end

    print(string.format("[Hair] 场景: %d 个毛发实体, 参数各异", #state.hair_entities))
    print("[Hair] Short Stiff / Medium Soft / Long Flowing / Curly Dense")
    print("[Hair] WASD 移动相机, 观察毛发物理和风力效果")
end

function HairDemo.Setup(config)
    setup_camera(config)
    setup_scene()
end

function HairDemo.Update(delta_time)
    local dt = delta_time or 0
    if dt > 0.1 then dt = 0.1 end
    state.time = state.time + dt

    -- 动态风向变化
    for _, he in ipairs(state.hair_entities) do
        local wind_x = math.sin(state.time * 0.4) * 2.0
        local wind_z = math.cos(state.time * 0.3) * 1.0
        local turb = 0.3 + math.sin(state.time * 0.8) * 0.2
        dse.ecs.set_hair_wind(he.entity, wind_x, 0.0, wind_z, turb, 2.0)
    end
end

return HairDemo
