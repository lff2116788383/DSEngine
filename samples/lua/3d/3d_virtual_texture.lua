local M = {}
M._meta = { name = "3D Virtual Texture", category = "openworld" }

local ecs = dse.ecs
local state = { elapsed = 0 }

function M.Setup(config)
    -- Camera
    local cam = ecs.create_entity()
    ecs.add_transform(cam, 0, 10, -15, 1, 1, 1)
    ecs.add_camera3d(cam, 60.0, 0.1, 200.0)
    state.cam = cam

    -- Virtual texture terrain
    local terrain = ecs.create_entity()
    ecs.add_transform(terrain, 0, 0, 0, 100, 1, 100)
    ecs.add_mesh_renderer(terrain, "cube")
    ecs.add_virtual_texture(terrain, 8192, 8192) -- VT resolution
    state.terrain = terrain

    -- Objects on terrain to show VT detail levels
    for i = 1, 15 do
        local x = math.cos(i * 0.42) * (5.0 + i * 2.0)
        local z = math.sin(i * 0.42) * (5.0 + i * 2.0)
        local e = ecs.create_entity()
        ecs.add_transform(e, x, 0.5, z, 1, 1, 1)
        ecs.add_mesh_renderer(e, "cube")
    end

    -- Light
    local light = ecs.create_entity()
    ecs.add_transform(light, 0, 20, 0, 1, 1, 1)
    ecs.add_dir_light(light, 1.0, 0.95, 0.88, 1.4)
end

function M.Update(dt)
    state.elapsed = state.elapsed + dt
    -- Fly camera closer/farther to trigger VT mip transitions
    local dist = 15.0 + math.sin(state.elapsed * 0.3) * 10.0
    local y = 5.0 + math.cos(state.elapsed * 0.2) * 5.0
    ecs.add_transform(state.cam, 0, y, -dist, 1, 1, 1)
end

return M