-- 3D P2 sample: terrain heightmap showcase
-- 目标：验证 Terrain 组件入口；当前无 heightmap 资源时以程序化地形 marker 展示 LOD/起伏主题。
local TerrainHeightmap3D = {}

local state = { camera = nil, terrain = nil, markers = {}, time = 0.0, logged = false }

local function cube_vertices()
    return { -0.5,-0.5,0.5, 0.5,-0.5,0.5, 0.5,0.5,0.5, -0.5,0.5,0.5, -0.5,-0.5,-0.5, 0.5,-0.5,-0.5, 0.5,0.5,-0.5, -0.5,0.5,-0.5 }
end

local function cube_indices()
    return { 0,1,2,2,3,0, 1,5,6,6,2,1, 5,4,7,7,6,5, 4,0,3,3,7,4, 3,2,6,6,7,3, 4,5,1,1,0,4 }
end

local function setup_camera(config)
    local camera = dse.ecs.create_entity()
    local distance = (type(config) == "table" and type(config.camera_distance) == "number") and config.camera_distance or 14.0
    dse.ecs.add_transform(camera, 0.0, 5.2, distance, 1.0, 1.0, 1.0)
    dse.ecs.set_transform_rotation(camera, -28.0, 0.0, 0.0)
    dse.ecs.add_camera_3d(camera, 60.0, 100)
    if Ecs and Ecs.add_free_camera_controller then Ecs.add_free_camera_controller(camera, 7.0, 0.12) elseif dse.ecs.add_free_camera_controller then dse.ecs.add_free_camera_controller(camera, 7.0, 0.12) end
    state.camera = camera
end

local function add_marker(name, x, y, z, sx, sy, sz, color, emissive)
    local e = dse.ecs.create_entity()
    dse.ecs.add_transform(e, x, y, z, sx, sy, sz)
    dse.ecs.add_mesh_renderer(e, color[1], color[2], color[3], color[4] or 1.0, cube_vertices(), cube_indices())
    dse.ecs.set_mesh_shader_variant(e, "MESH_LIT")
    dse.ecs.set_mesh_material(e, 0.0, 0.58, 1.0, emissive and emissive[1] or 0.0, emissive and emissive[2] or 0.0, emissive and emissive[3] or 0.0, 1.0, true, true)
    table.insert(state.markers, { name = name, entity = e, x = x, z = z, base_y = y })
    return e
end

local function sample_height(x, z)
    return math.sin(x * 1.15) * 0.42 + math.cos(z * 0.9) * 0.34 + math.sin((x + z) * 0.55) * 0.28
end

local function setup_terrain(config)
    local terrain = dse.ecs.create_entity()
    dse.ecs.add_transform(terrain, 0.0, -0.75, 0.0, 1.0, 1.0, 1.0)
    local heightmap = (type(config) == "table" and type(config.heightmap_path) == "string") and config.heightmap_path or ""
    local width = (type(config) == "table" and type(config.width) == "number") and config.width or 12.0
    local depth = (type(config) == "table" and type(config.depth) == "number") and config.depth or 9.0
    local max_height = (type(config) == "table" and type(config.max_height) == "number") and config.max_height or 2.8
    dse.ecs.add_terrain(terrain, heightmap, width, depth, max_height)
    state.terrain = terrain

    local light = dse.ecs.create_entity()
    dse.ecs.add_directional_light_3d(light, -0.42, -1.0, -0.25, 0.95, 0.96, 1.0, 1.15, 0.18, 0.32)

    for ix = -5, 5 do
        for iz = -4, 4 do
            local x = ix * 0.82
            local z = iz * 0.82
            local h = sample_height(x, z)
            local color = {0.18 + h * 0.08, 0.46 + h * 0.10, 0.22 + h * 0.05, 1.0}
            add_marker("height_tile", x, -0.45 + h * 0.45, z, 0.72, 0.08 + math.max(0.02, h + 0.8) * 0.18, 0.72, color)
        end
    end

    add_marker("near_lod_marker", -4.8, 1.2, -3.8, 0.24, 0.24, 0.24, {0.2, 0.9, 0.35, 1.0}, {0.05, 0.35, 0.08})
    add_marker("far_lod_marker", 4.8, 1.2, 3.8, 0.46, 0.46, 0.46, {1.0, 0.72, 0.2, 1.0}, {0.4, 0.18, 0.03})
    print(string.format("[3D][Terrain] setup: add_terrain heightmap='%s' width=%.1f depth=%.1f max_height=%.1f fallback_grid=99", heightmap, width, depth, max_height))
    print("[3D][Terrain] LOD markers: small green near / large amber far. Use free camera to inspect terrain relief.")
end

function TerrainHeightmap3D.Setup(config)
    setup_camera(config or {})
    setup_terrain(config or {})
end

function TerrainHeightmap3D.Update(delta_time)
    local dt = delta_time or 0.0
    if dt > 0.1 then dt = 0.1 end
    state.time = state.time + dt
    for i, item in ipairs(state.markers) do
        if item.name == "near_lod_marker" or item.name == "far_lod_marker" then
            dse.ecs.set_transform_rotation(item.entity, state.time * 22.0, state.time * (36.0 + i), 0.0)
        end
    end
    if not state.logged and state.time > 1.0 then
        state.logged = true
        print("[3D][Terrain] runtime: terrain component created; procedural fallback makes non-flat heightfield visible until real heightmap sampling/LOD assets land.")
    end
end

return TerrainHeightmap3D
