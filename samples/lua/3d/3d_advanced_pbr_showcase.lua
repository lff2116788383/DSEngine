-- 3D 高级 PBR 材质演示：Clear Coat、Anisotropy、POM、SSS
-- 使用 set_mesh_advanced_material / set_mesh_material_scalar 新接口
local AdvancedPbrShowcase = {}

local state = {
    camera = nil,
    light  = nil,
    spheres = {},
    time   = 0.0,
}

local ecs = dse.ecs

local cube_v = {
    -0.5,-0.5, 0.5,  0.5,-0.5, 0.5,  0.5, 0.5, 0.5, -0.5, 0.5, 0.5,
    -0.5,-0.5,-0.5,  0.5,-0.5,-0.5,  0.5, 0.5,-0.5, -0.5, 0.5,-0.5,
}
local cube_i = {0,1,2,2,3,0, 1,5,6,6,2,1, 5,4,7,7,6,5, 4,0,3,3,7,4, 3,2,6,6,7,3, 4,5,1,1,0,4}

local function make_sphere(x, y, z, r, g, b)
    local e = ecs.create_entity()
    ecs.add_transform(e, x, y, z, 1.2, 1.2, 1.2)
    ecs.add_mesh_renderer(e, r, g, b, 1.0, cube_v, cube_i)
    ecs.set_mesh_shader_variant(e, "MESH_LIT")
    return e
end

function AdvancedPbrShowcase.Setup(config)
    local cam = ecs.create_entity()
    local dist = (config and config.camera_distance) or 10.0
    ecs.add_transform(cam, 0, 1.5, dist, 1, 1, 1)
    ecs.set_transform_rotation(cam, -5, 0, 0)
    ecs.add_camera_3d(cam, 60, 100)
    if ecs.add_free_camera_controller then
        ecs.add_free_camera_controller(cam, 5.0, 0.12)
    end
    state.camera = cam

    local sun = ecs.create_entity()
    ecs.add_directional_light_3d(sun, -0.4, -1.0, -0.3, 1.0, 0.95, 0.85, 1.2, 0.15, 0.30)
    state.light = sun

    -- 地面
    local ground = ecs.create_entity()
    ecs.add_transform(ground, 0, -0.8, 0, 16, 0.1, 10)
    ecs.add_mesh_renderer(ground, 0.3, 0.3, 0.32, 1.0, cube_v, cube_i)
    ecs.set_mesh_shader_variant(ground, "MESH_LIT")
    ecs.set_mesh_material(ground, 0.0, 0.8, 1.0, 0.0, 0.0, 0.0, 0.0, true, false)

    -- 列排布：x = -4..4, 间距 2
    local configs = {
        {label="baseline",     cc=0,    cc_r=0.1, aniso=0,   pom=0,    sss=0,    r=0.9, g=0.1, b=0.1},
        {label="clear_coat",   cc=0.9,  cc_r=0.05,aniso=0,   pom=0,    sss=0,    r=0.2, g=0.5, b=0.9},
        {label="cc_matte",     cc=0.8,  cc_r=0.4, aniso=0,   pom=0,    sss=0,    r=0.1, g=0.8, b=0.3},
        {label="anisotropy",   cc=0,    cc_r=0.1, aniso=0.8, pom=0,    sss=0,    r=0.8, g=0.7, b=0.1},
        {label="sss_skin",     cc=0,    cc_r=0.1, aniso=0,   pom=0,    sss=0.6,  r=1.0, g=0.8, b=0.7},
    }

    for i, cfg in ipairs(configs) do
        local x = (i - 3) * 2.5
        local e = make_sphere(x, 0, 0, cfg.r, cfg.g, cfg.b)
        ecs.set_mesh_material(e, 0.0, 0.3, 1.0, 0.0, 0.0, 0.0, 0.0, true, false)
        ecs.set_mesh_material_scalar(e, "metallic",  0.0)
        ecs.set_mesh_material_scalar(e, "roughness", 0.3)
        ecs.set_mesh_advanced_material(e, cfg.cc, cfg.cc_r, cfg.aniso, cfg.pom, cfg.sss)
        table.insert(state.spheres, e)
    end

    print("[AdvPBR] 5 材质球: baseline / ClearCoat / CC_Matte / Anisotropy / SSS")
    print("[AdvPBR] WASD 移动相机，观察高光形状差异")
end

function AdvancedPbrShowcase.Update(dt)
    state.time = state.time + dt
    -- 球体缓慢旋转
    for i, e in ipairs(state.spheres) do
        ecs.set_transform_rotation(e, 0, state.time * 12 + i * 72, 0)
    end
end

return AdvancedPbrShowcase
