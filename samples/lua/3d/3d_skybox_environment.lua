-- 3D P1 sample: skybox/environment showcase
-- 目标：验证 SkyLight Lua 绑定，并用环境色变化展示模型暗部受环境光影响。
local SkyboxEnvironment3D = {}


SkyboxEnvironment3D._meta = {
    name     = "skybox/environment showcase",
    category = "rendering",
    config   = { camera_distance=9.0,
    intensity=1.0,
    up_color={0.20,0.30,0.50 },
}

local state = {
    camera = nil,
    sky_light = nil,
    objects = {},
    time = 0.0
}

local function cube_vertices()
    return {
        -0.5, -0.5,  0.5,  0.5, -0.5,  0.5,  0.5,  0.5,  0.5, -0.5,  0.5,  0.5,
        -0.5, -0.5, -0.5,  0.5, -0.5, -0.5,  0.5,  0.5, -0.5, -0.5,  0.5, -0.5
    }
end

local function cube_indices()
    return {
        0, 1, 2, 2, 3, 0, 1, 5, 6, 6, 2, 1, 5, 4, 7, 7, 6, 5,
        4, 0, 3, 3, 7, 4, 3, 2, 6, 6, 7, 3, 4, 5, 1, 1, 0, 4
    }
end

local function setup_camera(config)
    local camera = dse.ecs.create_entity()
    local distance = (type(config) == "table" and type(config.camera_distance) == "number") and config.camera_distance or 9.0
    dse.ecs.add_transform(camera, 0.0, 2.8, distance, 1.0, 1.0, 1.0)
    dse.ecs.set_transform_rotation(camera, -20.0, 0.0, 0.0)
    dse.ecs.add_camera_3d(camera, 60.0, 100)
    if Ecs and Ecs.add_free_camera_controller then
        Ecs.add_free_camera_controller(camera, 5.5, 0.12)
    elseif dse.ecs.add_free_camera_controller then
        dse.ecs.add_free_camera_controller(camera, 5.5, 0.12)
    end
    state.camera = camera
end

local function add_cube(name, x, y, z, sx, sy, sz, color, emissive)
    local e = dse.ecs.create_entity()
    dse.ecs.add_transform(e, x, y, z, sx, sy, sz)
    dse.ecs.add_mesh_renderer(e, color[1], color[2], color[3], color[4] or 1.0, cube_vertices(), cube_indices())
    dse.ecs.set_mesh_shader_variant(e, "MESH_LIT")
    dse.ecs.set_mesh_material(e, 0.0, 0.62, 1.0, emissive and emissive[1] or 0.0, emissive and emissive[2] or 0.0, emissive and emissive[3] or 0.0, 1.0, true, true)
    table.insert(state.objects, { name = name, entity = e })
    return e
end

local function setup_environment(config)
    local sky = dse.ecs.create_entity()
    local up = config.up_color or {0.20, 0.30, 0.50}
    local down = config.down_color or {0.03, 0.05, 0.08}
    dse.ecs.add_sky_light(sky, up[1], up[2], up[3], down[1], down[2], down[3], config.intensity or 1.0)
    state.sky_light = sky

    local skybox_path = (type(config.skybox_path) == "string") and config.skybox_path or ""
    if skybox_path ~= "" then
        local skybox = dse.ecs.create_entity()
        dse.ecs.add_skybox(skybox, skybox_path)
        print(string.format("[3D][Skybox] skybox path=%s", skybox_path))
    else
        print("[3D][Skybox] no cubemap configured; validating SkyLight ambient environment only")
    end
    print(string.format("[3D][Skybox] SkyLight up=(%.2f,%.2f,%.2f) down=(%.2f,%.2f,%.2f) intensity=%.2f", up[1], up[2], up[3], down[1], down[2], down[3], config.intensity or 1.0))
end

local function setup_scene()
    local light = dse.ecs.create_entity()
    dse.ecs.add_directional_light_3d(light, -0.25, -1.0, -0.25, 0.85, 0.90, 1.0, 0.35, 0.02, 0.20)
    add_cube("ground", 0.0, -0.55, 0.0, 8.0, 0.12, 5.2, {0.30, 0.34, 0.38, 1.0})
    add_cube("center_dark_receiver", 0.0, 0.2, 0.0, 1.25, 1.25, 1.25, {0.82, 0.82, 0.78, 1.0})
    add_cube("warm_side", -2.2, 0.0, -1.2, 0.8, 0.8, 0.8, {1.0, 0.55, 0.22, 1.0})
    add_cube("cool_side", 2.2, 0.0, -1.2, 0.8, 0.8, 0.8, {0.20, 0.62, 1.0, 1.0})
    add_cube("environment_marker", 0.0, 1.8, 1.8, 0.32, 0.32, 0.32, {0.45, 0.75, 1.0, 1.0}, {0.05, 0.10, 0.18})
end

function SkyboxEnvironment3D.Setup(config)
    print("[3D][Skybox] setup: SkyLight up/down colors animate environment brightness; optional skybox cubemap path supported.")
    setup_camera(config or {})
    setup_environment(config or {})
    setup_scene()
end

function SkyboxEnvironment3D.Update(delta_time)
    local dt = delta_time or 0.0
    if dt > 0.1 then dt = 0.1 end
    state.time = state.time + dt
    local pulse = (math.sin(state.time * 0.9) + 1.0) * 0.5
    if state.sky_light ~= nil then
        dse.ecs.set_sky_light(state.sky_light, 0.12 + pulse * 0.28, 0.18 + pulse * 0.28, 0.32 + pulse * 0.30, 0.02 + pulse * 0.06, 0.03 + pulse * 0.06, 0.06 + pulse * 0.08, 0.65 + pulse * 0.75, true)
    end
    for i, item in ipairs(state.objects) do
        if i > 1 then
            dse.ecs.set_transform_rotation(item.entity, math.sin(state.time + i) * 5.0, state.time * (8.0 + i), 0.0)
        end
    end
end

return SkyboxEnvironment3D
