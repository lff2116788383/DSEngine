-- 3D P1 sample: particles showcase
-- 目标：验证 3D 粒子发射器、Lua 参数 setter 与可视化 emitter marker。
local ParticlesShowcase3D = {}

local state = { camera = nil, emitter = nil, marker = nil, objects = {}, fallback_particles = {}, time = 0.0 }

local function cube_vertices()
    return { -0.5,-0.5,0.5, 0.5,-0.5,0.5, 0.5,0.5,0.5, -0.5,0.5,0.5, -0.5,-0.5,-0.5, 0.5,-0.5,-0.5, 0.5,0.5,-0.5, -0.5,0.5,-0.5 }
end

local function cube_indices()
    return { 0,1,2,2,3,0, 1,5,6,6,2,1, 5,4,7,7,6,5, 4,0,3,3,7,4, 3,2,6,6,7,3, 4,5,1,1,0,4 }
end

local function setup_camera(config)
    local camera = dse.ecs.create_entity()
    local distance = (type(config) == "table" and type(config.camera_distance) == "number") and config.camera_distance or 8.5
    dse.ecs.add_transform(camera, 0.0, 3.0, distance, 1.0, 1.0, 1.0)
    dse.ecs.set_transform_rotation(camera, -22.0, 0.0, 0.0)
    dse.ecs.add_camera_3d(camera, 60.0, 100)
    if Ecs and Ecs.add_free_camera_controller then Ecs.add_free_camera_controller(camera, 5.5, 0.12) elseif dse.ecs.add_free_camera_controller then dse.ecs.add_free_camera_controller(camera, 5.5, 0.12) end
    state.camera = camera
end

local function add_cube(name, x, y, z, sx, sy, sz, color, emissive)
    local e = dse.ecs.create_entity()
    dse.ecs.add_transform(e, x, y, z, sx, sy, sz)
    dse.ecs.add_mesh_renderer(e, color[1], color[2], color[3], color[4] or 1.0, cube_vertices(), cube_indices())
    dse.ecs.set_mesh_shader_variant(e, "MESH_LIT")
    dse.ecs.set_mesh_material(e, 0.0, 0.45, 1.0, emissive and emissive[1] or 0.0, emissive and emissive[2] or 0.0, emissive and emissive[3] or 0.0, 1.0, true, true)
    table.insert(state.objects, { name = name, entity = e })
    return e
end

local function setup_scene(config)
    local light = dse.ecs.create_entity()
    dse.ecs.add_directional_light_3d(light, -0.4, -1.0, -0.2, 1.0, 0.94, 0.84, 1.0, 0.16, 0.25)
    add_cube("ground", 0.0, -0.55, 0.0, 7.5, 0.12, 5.0, {0.28, 0.30, 0.32, 1.0})
    state.marker = add_cube("emitter_marker", 0.0, 0.0, 0.0, 0.36, 0.36, 0.36, {1.0, 0.58, 0.10, 1.0}, {0.55, 0.20, 0.02})
    add_cube("left_reference", -2.4, 0.0, -1.2, 0.75, 0.75, 0.75, {0.15, 0.62, 1.0, 1.0})
    add_cube("right_reference", 2.4, 0.0, -1.2, 0.75, 0.75, 0.75, {0.25, 1.0, 0.52, 1.0})

    local emitter = dse.ecs.create_entity()
    dse.ecs.add_transform(emitter, 0.0, 0.35, 0.0, 1.0, 1.0, 1.0)
    dse.ecs.add_particle_system_3d(emitter, config.max_particles or 420, config.emission_rate or 120.0)
    dse.ecs.set_particle_system_3d_params(emitter, 1.0, 2.2, 0.06, 0.18, 1.2, 3.4, 1.0, 0.56, 0.12, 0.92, 0.0, -2.2, 0.0, "")
    state.emitter = emitter

    for i = 1, 18 do
        local angle = i * 0.72
        local radius = 0.12 + (i % 6) * 0.08
        local y = 0.45 + (i % 5) * 0.24
        local p = add_cube("visible_particle_fallback_" .. tostring(i), math.cos(angle) * radius, y, math.sin(angle) * radius, 0.08, 0.08, 0.08, {1.0, 0.56, 0.12, 1.0}, {0.75, 0.24, 0.04})
        table.insert(state.fallback_particles, { entity = p, angle = angle, radius = radius, base_y = y, speed = 0.8 + (i % 4) * 0.15 })
    end
    print(string.format("[3D][Particles] emitter max_particles=%d emission_rate=%.1f life=1.0..2.2 size=0.06..0.18 visible_markers=%d", config.max_particles or 420, config.emission_rate or 120.0, #state.fallback_particles))
end

function ParticlesShowcase3D.Setup(config)
    print("[3D][Particles] setup: particle fountain with animated emitter marker.")
    setup_camera(config or {})
    setup_scene(config or {})
end

function ParticlesShowcase3D.Update(delta_time)
    local dt = delta_time or 0.0
    if dt > 0.1 then dt = 0.1 end
    state.time = state.time + dt
    local x = math.cos(state.time * 0.7) * 0.8
    local z = math.sin(state.time * 0.7) * 0.45
    if state.emitter ~= nil then dse.ecs.set_transform_position(state.emitter, x, 0.35, z) end
    if state.marker ~= nil then
        dse.ecs.set_transform_position(state.marker, x, 0.05, z)
        dse.ecs.set_transform_rotation(state.marker, state.time * 70.0, state.time * 110.0, 0.0)
    end
    for i, p in ipairs(state.fallback_particles) do
        local angle = p.angle + state.time * p.speed
        local rise = (math.sin(state.time * 1.8 + i) + 1.0) * 0.5
        local radius = p.radius + rise * 0.35
        dse.ecs.set_transform_position(p.entity, x + math.cos(angle) * radius, p.base_y + rise * 1.1, z + math.sin(angle) * radius)
        dse.ecs.set_transform_rotation(p.entity, state.time * 80.0, state.time * (100.0 + i), 0.0)
    end
end

return ParticlesShowcase3D
