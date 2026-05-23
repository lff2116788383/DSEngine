-- 流体模拟 Demo —— SPH 粒子喷泉
-- 场景：多个流体发射器从不同位置喷射水流，粒子落地反弹
local FluidDemo = {}


FluidDemo._meta = {
    name     = "流体模拟 Demo —— SPH 粒子喷泉",
    category = "physics",
    config   = { camera_distance=18.0,
    emission_rate=800.0 },
}

local camera_entity = nil
local light_entity = nil
local emitters = {}
local ground = nil

local cube_vertices = {
    -0.5,-0.5,0.5, 0.5,-0.5,0.5, 0.5,0.5,0.5, -0.5,0.5,0.5,
    -0.5,-0.5,-0.5, 0.5,-0.5,-0.5, 0.5,0.5,-0.5, -0.5,0.5,-0.5,
}
local cube_indices = {
    0,1,2,2,3,0, 1,5,6,6,2,1, 5,4,7,7,6,5, 4,0,3,3,7,4, 3,2,6,6,7,3, 4,5,1,1,0,4,
}

local function create_fountain(x, y, z, dir_x, dir_y, dir_z, r, g, b, rate)
    local e = dse.ecs.create_entity()
    dse.ecs.add_transform(e, x, y, z, 1.0, 1.0, 1.0)

    -- 点状发射器
    dse.ecs.add_fluid_emitter(e, 0, rate, 3.0, 4.0) -- Point, rate, lifetime, speed
    dse.ecs.set_fluid_emit_direction(e, dir_x, dir_y, dir_z, 0.15) -- 方向 + 扩散
    dse.ecs.set_fluid_physics(e, 0.02, 0.05, 1000.0, 50.0) -- viscosity, tension, density, stiffness
    dse.ecs.set_fluid_rendering(e, r, g, b, 0.85, 0.3, 2.0, 0.8) -- color + refraction/fresnel/specular
    dse.ecs.set_fluid_floor(e, 0.0, 0.4) -- floor_y, restitution

    return e
end

function FluidDemo.Setup(config)
    print("[流体Demo] 初始化场景...")

    -- 相机
    camera_entity = dse.ecs.create_entity()
    dse.ecs.add_transform(camera_entity, 0.0, 8.0, 18.0, 1.0, 1.0, 1.0)
    dse.ecs.set_transform_rotation(camera_entity, -20.0, 0.0, 0.0)
    dse.ecs.add_camera_3d(camera_entity, 60.0, 100)
    dse.ecs.add_free_camera_controller(camera_entity)

    -- 灯光
    light_entity = dse.ecs.create_entity()
    dse.ecs.add_directional_light_3d(light_entity, -0.4, -1.0, -0.3, 0.95, 0.95, 1.0, 1.3, 0.25, 0.3)

    -- 地面
    ground = dse.ecs.create_entity()
    dse.ecs.add_transform(ground, 0.0, -0.5, 0.0, 30.0, 1.0, 30.0)
    dse.ecs.add_mesh_renderer(ground, 0.25, 0.25, 0.3, 1.0, cube_vertices, cube_indices)
    dse.ecs.set_mesh_shader_variant(ground, "MESH_LIT")
    dse.ecs.set_mesh_material(ground, 0.0, 0.2, 1.0) -- 光滑地面

    -- 中央喷泉（蓝色，向上）
    local e1 = create_fountain(0.0, 0.5, 0.0, 0.0, 1.0, 0.0, 0.15, 0.4, 0.9, 800)
    table.insert(emitters, e1)

    -- 左侧喷泉（绿色，斜向右上）
    local e2 = create_fountain(-6.0, 0.5, 0.0, 0.5, 0.8, 0.0, 0.1, 0.8, 0.3, 500)
    table.insert(emitters, e2)

    -- 右侧喷泉（红色，斜向左上）
    local e3 = create_fountain(6.0, 0.5, 0.0, -0.5, 0.8, 0.0, 0.9, 0.2, 0.15, 500)
    table.insert(emitters, e3)

    -- 后方瀑布（青色，向下倾斜）
    local e4 = create_fountain(0.0, 8.0, -5.0, 0.0, -0.3, 0.8, 0.1, 0.7, 0.8, 600)
    table.insert(emitters, e4)

    print("[流体Demo] 场景就绪！WASD 移动相机，观察 4 个流体发射器")
end

local hud_timer = 0.0

function FluidDemo.Update(delta_time)
    -- 每秒打印一次粒子统计
    hud_timer = hud_timer + delta_time
    if hud_timer >= 2.0 then
        hud_timer = 0.0
        local total = 0
        for _, e in ipairs(emitters) do
            total = total + dse.ecs.get_fluid_particle_count(e)
        end
        print(string.format("[流体Demo] 活跃粒子总数: %d", total))
    end
end

return FluidDemo
