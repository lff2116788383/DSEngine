-- 3D P2 sample: spatial audio showcase
-- 目标：展示 3D audio source/listener 概念；当前音频绑定仍是 2D source，因此用环绕音源 marker + volume/pitch 动态 fallback。
local AudioSpatial3D = {}

local state = { camera = nil, listener = nil, source = nil, source_marker = nil, rings = {}, time = 0.0, logged = false }

local function cube_vertices()
    return { -0.5,-0.5,0.5, 0.5,-0.5,0.5, 0.5,0.5,0.5, -0.5,0.5,0.5, -0.5,-0.5,-0.5, 0.5,-0.5,-0.5, 0.5,0.5,-0.5, -0.5,0.5,-0.5 }
end

local function cube_indices()
    return { 0,1,2,2,3,0, 1,5,6,6,2,1, 5,4,7,7,6,5, 4,0,3,3,7,4, 3,2,6,6,7,3, 4,5,1,1,0,4 }
end

local function add_cube(name, x, y, z, sx, sy, sz, color, emissive)
    local e = dse.ecs.create_entity()
    dse.ecs.add_transform(e, x, y, z, sx, sy, sz)
    dse.ecs.add_mesh_renderer(e, color[1], color[2], color[3], color[4] or 1.0, cube_vertices(), cube_indices())
    dse.ecs.set_mesh_shader_variant(e, "MESH_LIT")
    dse.ecs.set_mesh_material(e, 0.0, 0.55, 1.0, emissive and emissive[1] or 0.0, emissive and emissive[2] or 0.0, emissive and emissive[3] or 0.0, 1.0, true, true)
    return e
end

local function setup_camera(config)
    local camera = dse.ecs.create_entity()
    local distance = (type(config) == "table" and type(config.camera_distance) == "number") and config.camera_distance or 9.5
    dse.ecs.add_transform(camera, 0.0, 3.1, distance, 1.0, 1.0, 1.0)
    dse.ecs.set_transform_rotation(camera, -20.0, 0.0, 0.0)
    dse.ecs.add_camera_3d(camera, 60.0, 100)
    if Ecs and Ecs.add_free_camera_controller then Ecs.add_free_camera_controller(camera, 5.5, 0.12) elseif dse.ecs.add_free_camera_controller then dse.ecs.add_free_camera_controller(camera, 5.5, 0.12) end
    state.camera = camera
end

local function setup_scene(config)
    local light = dse.ecs.create_entity()
    dse.ecs.add_directional_light_3d(light, -0.35, -1.0, -0.28, 1.0, 0.95, 0.88, 1.15, 0.20, 0.34)
    add_cube("ground", 0.0, -0.58, 0.0, 8.5, 0.12, 6.0, {0.30, 0.34, 0.38, 1.0})
    state.listener = add_cube("listener_camera_anchor", 0.0, 0.35, 0.0, 0.40, 0.80, 0.40, {0.25, 0.72, 1.0, 1.0}, {0.04, 0.18, 0.35})
    state.source_marker = add_cube("moving_audio_source", 2.4, 0.55, 0.0, 0.46, 0.46, 0.46, {1.0, 0.50, 0.18, 1.0}, {0.45, 0.12, 0.02})
    for i = 1, 16 do
        local angle = (i / 16.0) * math.pi * 2.0
        local e = add_cube("distance_ring", math.cos(angle) * 2.4, -0.43, math.sin(angle) * 2.4, 0.14, 0.04, 0.14, {0.88, 0.72, 0.32, 1.0}, {0.12, 0.08, 0.01})
        table.insert(state.rings, { entity = e, angle = angle })
    end

    local audio_path = (type(config) == "table" and type(config.audio_path) == "string") and config.audio_path or ""
    state.source = dse.ecs.create_entity()
    dse.ecs.add_transform(state.source, 2.4, 0.55, 0.0, 1.0, 1.0, 1.0)
    if audio_path ~= "" then
        dse.audio.add_source(state.source, audio_path, true, true, 0.35)
        dse.audio.set_playing(state.source, true)
        state.audio_enabled = true
        print(string.format("[3D][AudioSpatial] setup: source orbits listener. audio_path='%s' 3D audio API missing, using volume/pitch fallback.", audio_path))
    else
        state.audio_enabled = false
        print("[3D][AudioSpatial] setup: source orbits listener. audio_path empty; visual-only spatial audio fallback active.")
    end
end

function AudioSpatial3D.Setup(config)
    setup_camera(config or {})
    setup_scene(config or {})
end

function AudioSpatial3D.Update(delta_time)
    local dt = delta_time or 0.0
    if dt > 0.1 then dt = 0.1 end
    state.time = state.time + dt
    local radius = 2.4 + math.sin(state.time * 0.65) * 0.75
    local x = math.cos(state.time * 0.85) * radius
    local z = math.sin(state.time * 0.85) * radius
    local distance = math.sqrt(x * x + z * z)
    local volume = math.max(0.08, math.min(0.85, 1.0 - distance / 5.0))
    local pitch = 0.82 + (math.sin(state.time * 0.85) * 0.5 + 0.5) * 0.36
    if state.source ~= nil then
        dse.ecs.set_transform_position(state.source, x, 0.55, z)
        if state.audio_enabled then
            dse.audio.set_volume(state.source, volume)
            dse.audio.set_pitch(state.source, pitch)
        end
    end
    if state.source_marker ~= nil then
        dse.ecs.set_transform_position(state.source_marker, x, 0.55, z)
        dse.ecs.set_transform_rotation(state.source_marker, state.time * 80.0, state.time * 130.0, 0.0)
    end
    for i, ring in ipairs(state.rings) do
        local pulse = 1.0 + math.sin(state.time * 3.0 - i * 0.35) * 0.35
        dse.ecs.add_transform(ring.entity, math.cos(ring.angle) * radius, -0.43, math.sin(ring.angle) * radius, 0.10 * pulse, 0.04, 0.10 * pulse)
    end
    if not state.logged and state.time > 1.0 then
        state.logged = true
        print(string.format("[3D][AudioSpatial] runtime: source=(%.2f,%.2f) listener=(0,0) distance=%.2f volume=%.2f pitch=%.2f", x, z, distance, volume, pitch))
    end
end

return AudioSpatial3D
