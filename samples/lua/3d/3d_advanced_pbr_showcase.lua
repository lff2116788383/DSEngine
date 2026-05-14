-- 3D 高级 PBR 材质演示：Clear Coat、Anisotropy、POM、SSS
-- 使用 set_mesh_advanced_material / set_mesh_material_scalar 新接口
local AdvancedPbrShowcase = {}

local state = {
    camera = nil,
    light  = nil,
    spheres = {},
    time   = 0.0,
}

local function make_sphere_mesh(e, r, g, b)
    ecs.add_mesh_renderer(e, r, g, b, 1.0)
    ecs.set_mesh_path(e, "assets/meshes/sphere.glb")
    ecs.set_mesh_shader_variant(e, "MESH_PBR")
end

function AdvancedPbrShowcase.init()
    local cam = ecs.create_entity()
    ecs.add_transform_3d(cam, 0, 1.5, 6)
    ecs.add_camera_3d(cam, 60, 0, 0.1, 100)
    state.camera = cam

    local sun = ecs.create_entity()
    ecs.add_transform_3d(sun, 0, 5, 2)
    ecs.add_directional_light_3d(sun, 1, 0.95, 0.85, 1, 0.5, -1, -0.3)
    state.light = sun

    -- 列排布：x = -4..4, 间距 2
    local configs = {
        {label="baseline",     cc=0,    cc_r=0.1, aniso=0,   pom=0,    sss=0,    r=0.9, g=0.1, b=0.1},
        {label="clear_coat",   cc=0.9,  cc_r=0.05,aniso=0,   pom=0,    sss=0,    r=0.2, g=0.5, b=0.9},
        {label="cc_matte",     cc=0.8,  cc_r=0.4, aniso=0,   pom=0,    sss=0,    r=0.1, g=0.8, b=0.3},
        {label="anisotropy",   cc=0,    cc_r=0.1, aniso=0.8, pom=0,    sss=0,    r=0.8, g=0.7, b=0.1},
        {label="sss_skin",     cc=0,    cc_r=0.1, aniso=0,   pom=0,    sss=0.6,  r=1.0, g=0.8, b=0.7},
    }

    for i, cfg in ipairs(configs) do
        local e = ecs.create_entity()
        local x = (i - 3) * 2.2
        ecs.add_transform_3d(e, x, 0, 0)
        make_sphere_mesh(e, cfg.r, cfg.g, cfg.b)
        ecs.set_mesh_material_scalar(e, "metallic",  0.0)
        ecs.set_mesh_material_scalar(e, "roughness", 0.3)
        -- clear_coat, clear_coat_roughness, anisotropy, pom_height_scale, sss_strength
        ecs.set_mesh_advanced_material(e, cfg.cc, cfg.cc_r, cfg.aniso, cfg.pom, cfg.sss)
        table.insert(state.spheres, e)
    end
end

function AdvancedPbrShowcase.update(dt)
    state.time = state.time + dt
    -- 光源绕 Y 轴旋转，便于观察高光形状差异
    local angle = state.time * 0.4
    ecs.set_transform_position(state.light, math.sin(angle) * 5, 5, math.cos(angle) * 3)
end

function AdvancedPbrShowcase.shutdown() end

return AdvancedPbrShowcase
