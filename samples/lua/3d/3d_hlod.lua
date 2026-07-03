local M = {}
M._meta = { name = "3D Hierarchical LOD", category = "openworld" }

local ecs = dse.ecs
local state = { elapsed = 0 }

function M.Setup(config)
    -- Camera (will fly through scene)
    local cam = ecs.create_entity()
    ecs.add_transform(cam, 0, 5, -20, 1, 1, 1)
    ecs.add_camera3d(cam, 60.0, 0.1, 500.0)
    state.cam = cam

    -- Configure HLOD system
    local hlod_root = ecs.create_entity()
    ecs.add_transform(hlod_root, 0, 0, 0, 1, 1, 1)
    ecs.add_hlod_config(hlod_root, 3) -- 3 LOD levels

    -- Create clusters of objects at different distances
    -- Near cluster (LOD 0 - full detail)
    for i = 1, 10 do
        local e = ecs.create_entity()
        local x = math.cos(i * 0.63) * 5.0
        local z = math.sin(i * 0.63) * 5.0
        ecs.add_transform(e, x, 0.5, z, 1, 1, 1)
        ecs.add_mesh_renderer(e, "sphere")
    end

    -- Mid cluster (LOD 1 - medium)
    for i = 1, 20 do
        local e = ecs.create_entity()
        local x = math.cos(i * 0.31) * 30.0
        local z = math.sin(i * 0.31) * 30.0
        ecs.add_transform(e, x, 0.5, z, 1, 1, 1)
        ecs.add_mesh_renderer(e, "cube")
    end

    -- Far cluster (LOD 2 - simplified)
    for i = 1, 30 do
        local e = ecs.create_entity()
        local x = math.cos(i * 0.21) * 80.0
        local z = math.sin(i * 0.21) * 80.0
        ecs.add_transform(e, x, 1.0, z, 2, 2, 2)
        ecs.add_mesh_renderer(e, "cube")
    end

    -- Ground
    local ground = ecs.create_entity()
    ecs.add_transform(ground, 0, 0, 0, 200, 0.1, 200)
    ecs.add_mesh_renderer(ground, "cube")

    -- Light
    local light = ecs.create_entity()
    ecs.add_transform(light, 0, 30, 0, 1, 1, 1)
    ecs.add_dir_light(light, 1.0, 0.95, 0.9, 1.3)
end

function M.Update(dt)
    state.elapsed = state.elapsed + dt
    -- Camera flies forward and back to trigger LOD transitions
    local z = math.sin(state.elapsed * 0.2) * 60.0 - 20.0
    local y = 5.0 + math.abs(math.sin(state.elapsed * 0.1)) * 15.0
    ecs.add_transform(state.cam, 0, y, z, 1, 1, 1)
end

return M