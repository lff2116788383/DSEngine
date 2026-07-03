local M = {}
M._meta = { name = "3D Cutscene Playback", category = "cutscene" }

local ecs = dse.ecs
local state = { elapsed = 0 }

function M.Setup(config)
    -- Camera (will be controlled by cutscene)
    local cam = ecs.create_entity()
    ecs.add_transform(cam, 0, 3, -8, 1, 1, 1)
    ecs.add_camera3d(cam, 60.0, 0.1, 100.0)
    state.cam = cam

    -- Cutscene controller
    local cs = ecs.create_entity()
    ecs.add_transform(cs, 0, 0, 0, 1, 1, 1)
    dse.cutscene.create(cs, "demo_cutscene")
    dse.cutscene.add_track(cs, "camera", cam)
    dse.cutscene.set_duration(cs, 10.0)
    dse.cutscene.play(cs)
    state.cutscene = cs

    -- Scene: characters and environment
    local actor1 = ecs.create_entity()
    ecs.add_transform(actor1, -2, 1, 0, 1, 2, 1)
    ecs.add_mesh_renderer(actor1, "capsule")

    local actor2 = ecs.create_entity()
    ecs.add_transform(actor2, 2, 1, 0, 1, 2, 1)
    ecs.add_mesh_renderer(actor2, "capsule")

    -- Ground
    local ground = ecs.create_entity()
    ecs.add_transform(ground, 0, 0, 0, 20, 0.1, 20)
    ecs.add_mesh_renderer(ground, "cube")

    -- Props
    local prop1 = ecs.create_entity()
    ecs.add_transform(prop1, 0, 0.5, 3, 2, 1, 2)
    ecs.add_mesh_renderer(prop1, "cube")

    -- Light
    local light = ecs.create_entity()
    ecs.add_transform(light, 0, 10, 0, 1, 1, 1)
    ecs.add_dir_light(light, 1.0, 0.95, 0.9, 1.5)

    -- Spot light for dramatic effect
    local spot = ecs.create_entity()
    ecs.add_transform(spot, 0, 8, -3, 1, 1, 1)
    ecs.add_spot_light(spot, 1.0, 0.9, 0.7, 3.0, 15.0, 30.0)
end

function M.Update(dt)
    state.elapsed = state.elapsed + dt
end

return M