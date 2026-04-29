-- 3D P3 sample: terrain LOD zones showcase
-- 目标：专项展示 TerrainComponent 程序化 height data 与动态 LOD；tile 分区作为 near/mid/far LOD 可视化标尺。
local TerrainLodZones3D = {}

local state = { camera = nil, terrain = nil, tiles = {}, time = 0.0, logged = false, lod_logged = false }

local function cube_vertices()
    return { -0.5,-0.5,0.5, 0.5,-0.5,0.5, 0.5,0.5,0.5, -0.5,0.5,0.5, -0.5,-0.5,-0.5, 0.5,-0.5,-0.5, 0.5,0.5,-0.5, -0.5,0.5,-0.5 }
end

local function cube_indices()
    return { 0,1,2,2,3,0, 1,5,6,6,2,1, 5,4,7,7,6,5, 4,0,3,3,7,4, 3,2,6,6,7,3, 4,5,1,1,0,4 }
end

local function setup_camera(config)
    local camera = dse.ecs.create_entity()
    local distance = (type(config) == "table" and type(config.camera_distance) == "number") and config.camera_distance or 15.0
    dse.ecs.add_transform(camera, 0.0, 6.0, distance, 1.0, 1.0, 1.0)
    dse.ecs.set_transform_rotation(camera, -30.0, 0.0, 0.0)
    dse.ecs.add_camera_3d(camera, 60.0, 100)
    if Ecs and Ecs.add_free_camera_controller then Ecs.add_free_camera_controller(camera, 7.0, 0.12) elseif dse.ecs.add_free_camera_controller then dse.ecs.add_free_camera_controller(camera, 7.0, 0.12) end
    state.camera = camera
end

local function sample_height(x, z)
    return math.sin(x * 0.9) * 0.34 + math.cos(z * 1.2) * 0.28 + math.sin((x - z) * 0.45) * 0.20
end

local function add_tile(zone, x, z, size, color)
    local h = sample_height(x, z)
    local e = dse.ecs.create_entity()
    dse.ecs.add_transform(e, x, -0.48 + h * 0.55, z, size * 0.88, 0.07 + math.max(0.02, h + 0.7) * 0.18, size * 0.88)
    dse.ecs.add_mesh_renderer(e, color[1] + h * 0.06, color[2] + h * 0.08, color[3] + h * 0.04, 1.0, cube_vertices(), cube_indices())
    dse.ecs.set_mesh_shader_variant(e, "MESH_LIT")
    dse.ecs.set_mesh_material(e, 0.0, 0.62, 1.0, 0.0, 0.0, 0.0, 1.0, true, true)
    table.insert(state.tiles, { zone = zone, entity = e, x = x, z = z, h = h })
end

local function add_zone_marker(name, x, z, color)
    local e = dse.ecs.create_entity()
    dse.ecs.add_transform(e, x, 0.92, z, 0.42, 0.42, 0.42)
    dse.ecs.add_mesh_renderer(e, color[1], color[2], color[3], 1.0, cube_vertices(), cube_indices())
    dse.ecs.set_mesh_shader_variant(e, "MESH_LIT")
    dse.ecs.set_mesh_material(e, 0.0, 0.42, 1.0, color[1] * 0.25, color[2] * 0.25, color[3] * 0.25, 1.0, true, true)
    table.insert(state.tiles, { zone = name, entity = e, x = x, z = z, h = 1.0, marker = true })
end

local function setup_terrain(config)
    local terrain = dse.ecs.create_entity()
    dse.ecs.add_transform(terrain, 0.0, -0.9, 0.0, 1.0, 1.0, 1.0)
    local heightmap = (type(config) == "table" and type(config.heightmap_path) == "string") and config.heightmap_path or ""
    local width = (type(config) == "table" and type(config.width) == "number") and config.width or 14.0
    local depth = (type(config) == "table" and type(config.depth) == "number") and config.depth or 10.0
    local max_height = (type(config) == "table" and type(config.max_height) == "number") and config.max_height or 3.0
    dse.ecs.add_terrain(terrain, heightmap, width, depth, max_height)
    state.terrain = terrain

    local resolution_x = (type(config) == "table" and type(config.resolution_x) == "number") and config.resolution_x or 33
    local resolution_z = (type(config) == "table" and type(config.resolution_z) == "number") and config.resolution_z or 33
    local max_lod_levels = (type(config) == "table" and type(config.max_lod_levels) == "number") and config.max_lod_levels or 4
    local lod_distance_factor = (type(config) == "table" and type(config.lod_distance_factor) == "number") and config.lod_distance_factor or 4.0
    local terrain_api_ok = dse.ecs.set_terrain_params and dse.ecs.set_terrain_height and dse.ecs.get_terrain_lod
    if terrain_api_ok then
        dse.ecs.set_terrain_params(terrain, resolution_x, resolution_z, max_lod_levels, lod_distance_factor, true)
        for z = 0, resolution_z - 1 do
            local nz = (z / (resolution_z - 1) - 0.5) * depth
            for x = 0, resolution_x - 1 do
                local nx = (x / (resolution_x - 1) - 0.5) * width
                local h = sample_height(nx, nz) * max_height * 0.45
                dse.ecs.set_terrain_height(terrain, x, z, h)
            end
        end
        local lod, rx, rz, lod_levels, lod_factor = dse.ecs.get_terrain_lod(terrain)
        print(string.format("[3D][TerrainLodZones] terrain_api: set_terrain_params=%dx%d max_lod=%d lod_factor=%.2f current_lod=%s", rx or resolution_x, rz or resolution_z, lod_levels or max_lod_levels, lod_factor or lod_distance_factor, tostring(lod)))
    else
        print("[3D][TerrainLodZones] terrain_api unavailable; using TerrainComponent + visible tile LOD markers only.")
    end

    local light = dse.ecs.create_entity()
    dse.ecs.add_directional_light_3d(light, -0.38, -1.0, -0.28, 0.92, 0.96, 1.0, 1.18, 0.16, 0.32)

    for ix = -3, 3 do
        for iz = -2, 2 do
            add_tile("near_dense", ix * 0.55, iz * 0.55 - 2.5, 0.48, {0.16, 0.58, 0.26})
        end
    end
    for ix = -3, 3 do
        add_tile("mid", ix * 0.95, 0.6, 0.78, {0.28, 0.48, 0.25})
    end
    for ix = -2, 2 do
        add_tile("far_coarse", ix * 1.35, 3.1, 1.10, {0.46, 0.42, 0.22})
    end

    add_zone_marker("near_lod_dense_marker", -4.9, -2.5, {0.2, 1.0, 0.35})
    add_zone_marker("mid_lod_marker", -4.9, 0.6, {0.4, 0.8, 1.0})
    add_zone_marker("far_lod_coarse_marker", -4.9, 3.1, {1.0, 0.68, 0.18})

    print(string.format("[3D][TerrainLodZones] setup: TerrainComponent heightmap='%s' width=%.1f depth=%.1f max_height=%.1f", heightmap, width, depth, max_height))
    print("[3D][TerrainLodZones] visual markers show near dense / mid / far coarse LOD zones while TerrainSystem renders procedural height data.")
end

function TerrainLodZones3D.Setup(config)
    setup_camera(config or {})
    setup_terrain(config or {})
end

function TerrainLodZones3D.Update(delta_time)
    local dt = delta_time or 0.0
    if dt > 0.1 then dt = 0.1 end
    state.time = state.time + dt
    for i, item in ipairs(state.tiles) do
        if item.marker then
            dse.ecs.set_transform_rotation(item.entity, state.time * 24.0, state.time * (42.0 + i), 0.0)
        elseif item.zone == "near_dense" then
            local pulse = math.sin(state.time * 2.0 + i * 0.17) * 0.025
            dse.ecs.set_transform_position(item.entity, item.x, -0.48 + item.h * 0.55 + pulse, item.z)
        end
    end
    if state.terrain and dse.ecs.get_terrain_lod and not state.lod_logged and state.time > 0.2 then
        state.lod_logged = true
        local lod, rx, rz, lod_levels, lod_factor = dse.ecs.get_terrain_lod(state.terrain)
        print(string.format("[3D][TerrainLodZones] runtime_lod: current_lod=%s resolution=%sx%s max_lod=%s lod_factor=%s", tostring(lod), tostring(rx), tostring(rz), tostring(lod_levels), tostring(lod_factor)))
    end
    if not state.logged and state.time > 0.2 then
        state.logged = true
        print("[3D][TerrainLodZones] runtime: TerrainComponent height grid is active; near tiles animate subtly and far zone uses larger tiles for LOD contrast.")
    end
end

return TerrainLodZones3D
