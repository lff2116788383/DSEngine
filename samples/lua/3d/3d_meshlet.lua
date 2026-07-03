local M = {}
M._meta = { name = "3D Meshlet Rendering", category = "rendering_advanced" }

local ecs = dse.ecs
local state = { elapsed = 0 }

function M.Setup(config)
    -- Camera
    local cam = ecs.create_entity()
    ecs.add_transform(cam, 0, 5, -10, 1, 1, 1)
    ecs.add_camera3d(cam, 60.0, 0.1, 200.0)
    state.cam = cam

    -- High-poly mesh rendered via meshlet pipeline
    local mesh_entity = ecs.create_entity()
    ecs.add_transform(mesh_entity, 0, 2, 0, 3, 3, 3)
    ecs.add_mesh_renderer(mesh_entity, "sphere")
    dse.meshlet.enable(mesh_entity)
    state.mesh = mesh_entity

    -- Additional meshlet objects at various distances
    for i = 1, 12 do
        local e = ecs.create_entity()
        local angle = (i / 12) * math.pi * 2
        local dist = 6.0 + (i % 3) * 3.0
        ecs.add_transform(e, math.cos(angle) * dist, 1, math.sin(angle) * dist, 1.5, 1.5, 1.5)
        ecs.add_mesh_renderer(e, "sphere")
        dse.meshlet.enable(e)
    end

    -- Ground
    local ground = ecs.create_entity()
    ecs.add_transform(ground, 0, 0, 0, 40, 0.1, 40)
    ecs.add_mesh_renderer(ground, "cube")

    -- Light
    local light = ecs.create_entity()
    ecs.add_transform(light, 0, 15, 0, 1, 1, 1)
    ecs.add_dir_light(light, 1.0, 0.95, 0.9, 1.5)
end

function M.Update(dt)
    state.elapsed = state.elapsed + dt
end

return M