local M = {}
M._meta = { name = "3D World Partition", category = "openworld" }

local ecs = dse.ecs
local state = { elapsed = 0 }

function M.Setup(config)
    -- Camera
    local cam = ecs.create_entity()
    ecs.add_transform(cam, 0, 20, -30, 1, 1, 1)
    ecs.add_camera3d(cam, 60.0, 0.1, 500.0)

    -- World partition setup
    local world = ecs.create_entity()
    ecs.add_transform(world, 0, 0, 0, 1, 1, 1)
    ecs.add_world_partition(world, 64.0, 4, 4) -- cell_size, grid_x, grid_z

    -- Streaming origin (player position drives loading)
    local origin = ecs.create_entity()
    ecs.add_transform(origin, 0, 1, 0, 1, 2, 1)
    ecs.add_mesh_renderer(origin, "capsule")
    ecs.add_streaming_origin(origin, 128.0) -- load radius
    state.origin = origin

    -- Generate terrain chunks per partition cell
    for gx = 0, 3 do
        for gz = 0, 3 do
            local cx = (gx - 1.5) * 64.0
            local cz = (gz - 1.5) * 64.0
            local chunk = ecs.create_entity()
            ecs.add_transform(chunk, cx, 0, cz, 64, 0.5, 64)
            ecs.add_mesh_renderer(chunk, "cube")
            -- Color code by cell
            local r = 0.2 + gx * 0.15
            local g = 0.3 + gz * 0.1
        end
    end

    -- Directional light
    local light = ecs.create_entity()
    ecs.add_transform(light, 0, 50, 0, 1, 1, 1)
    ecs.add_dir_light(light, 1.0, 0.95, 0.85, 1.2)
end

function M.Update(dt)
    state.elapsed = state.elapsed + dt
    -- Move streaming origin to test partition loading
    local x = math.sin(state.elapsed * 0.3) * 60.0
    local z = math.cos(state.elapsed * 0.2) * 60.0
    ecs.add_transform(state.origin, x, 1, z, 1, 2, 1)
end

return M