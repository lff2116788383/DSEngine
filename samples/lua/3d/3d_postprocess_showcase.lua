-- 3D P1 sample: postprocess showcase
-- 目标：验证 bloom 后处理参数与 emissive 物体的视觉差异。
local PostprocessShowcase3D = {}

local state = { camera = nil, post_process = nil, cubes = {}, time = 0.0 }

local function cube_vertices()
    return { -0.5,-0.5,0.5, 0.5,-0.5,0.5, 0.5,0.5,0.5, -0.5,0.5,0.5, -0.5,-0.5,-0.5, 0.5,-0.5,-0.5, 0.5,0.5,-0.5, -0.5,0.5,-0.5 }
end

local function cube_indices()
    return { 0,1,2,2,3,0, 1,5,6,6,2,1, 5,4,7,7,6,5, 4,0,3,3,7,4, 3,2,6,6,7,3, 4,5,1,1,0,4 }
end

local function setup_camera(config)
    local camera = dse.ecs.create_entity()
    local distance = (type(config) == "table" and type(config.camera_distance) == "number") and config.camera_distance or 8.5
    dse.ecs.add_transform(camera, 0.0, 2.8, distance, 1.0, 1.0, 1.0)
    dse.ecs.set_transform_rotation(camera, -20.0, 0.0, 0.0)
    dse.ecs.add_camera_3d(camera, 60.0, 100)
    if Ecs and Ecs.add_free_camera_controller then Ecs.add_free_camera_controller(camera, 5.0, 0.12) elseif dse.ecs.add_free_camera_controller then dse.ecs.add_free_camera_controller(camera, 5.0, 0.12) end
    state.camera = camera
end

local function add_cube(name, x, y, z, sx, sy, sz, color, emissive)
    local e = dse.ecs.create_entity()
    dse.ecs.add_transform(e, x, y, z, sx, sy, sz)
    dse.ecs.add_mesh_renderer(e, color[1], color[2], color[3], color[4] or 1.0, cube_vertices(), cube_indices())
    dse.ecs.set_mesh_shader_variant(e, "MESH_LIT")
    dse.ecs.set_mesh_material(e, 0.0, 0.35, 1.0, emissive[1], emissive[2], emissive[3], 1.0, true, true)
    table.insert(state.cubes, { name = name, entity = e })
    return e
end

local function setup_scene(config)
    local light = dse.ecs.create_entity()
    dse.ecs.add_directional_light_3d(light, -0.4, -1.0, -0.3, 1.0, 0.95, 0.86, 0.85, 0.12, 0.30)
    add_cube("ground", 0.0, -0.55, 0.0, 7.5, 0.12, 5.0, {0.20, 0.22, 0.25, 1.0}, {0.0, 0.0, 0.0})
    add_cube("normal_reference", -2.2, 0.1, 0.0, 1.05, 1.05, 1.05, {0.35, 0.62, 1.0, 1.0}, {0.0, 0.0, 0.0})
    add_cube("emissive_orange", 0.0, 0.2, 0.0, 1.25, 1.25, 1.25, {1.0, 0.42, 0.10, 1.0}, {1.4, 0.42, 0.05})
    add_cube("emissive_blue", 2.2, 0.1, 0.0, 1.05, 1.05, 1.05, {0.25, 0.62, 1.0, 1.0}, {0.12, 0.35, 1.1})

    local pp = dse.ecs.create_entity()
    local threshold = config.bloom_threshold or 0.8
    local intensity = config.bloom_intensity or 1.25
    dse.ecs.add_post_process(pp, true, threshold, intensity, config.exposure or 1.0)
    state.post_process = pp
    print(string.format("[3D][PostProcess] bloom threshold=%.2f intensity=%.2f exposure=%.2f", threshold, intensity, config.exposure or 1.0))
end

function PostprocessShowcase3D.Setup(config)
    print("[3D][PostProcess] setup: emissive cubes plus animated bloom intensity/threshold.")
    setup_camera(config or {})
    setup_scene(config or {})
end

function PostprocessShowcase3D.Update(delta_time)
    local dt = delta_time or 0.0
    if dt > 0.1 then dt = 0.1 end
    state.time = state.time + dt
    local pulse = (math.sin(state.time * 1.15) + 1.0) * 0.5
    if state.post_process ~= nil then
        dse.ecs.set_post_process_bloom(state.post_process, true, true, 0.65 + pulse * 0.55, 0.75 + pulse * 1.25, 1.0)
    end
    for i, item in ipairs(state.cubes) do
        if i > 1 then dse.ecs.set_transform_rotation(item.entity, math.sin(state.time + i) * 7.0, state.time * (12.0 + i * 4.0), 0.0) end
    end
end

return PostprocessShowcase3D
