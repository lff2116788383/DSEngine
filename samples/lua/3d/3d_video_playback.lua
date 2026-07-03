local M = {}
M._meta = { name = "3D Video Playback", category = "media" }

local ecs = dse.ecs
local state = { elapsed = 0 }

function M.Setup(config)
    -- Camera
    local cam = ecs.create_entity()
    ecs.add_transform(cam, 0, 3, -6, 1, 1, 1)
    ecs.add_camera3d(cam, 60.0, 0.1, 100.0)

    -- Video screen (large plane facing camera)
    local screen = ecs.create_entity()
    ecs.add_transform(screen, 0, 3, 0, 8, 4.5, 0.1)
    ecs.add_mesh_renderer(screen, "cube")

    -- Try to attach video texture
    local ok = pcall(function()
        dse.video.create(screen, "data/video/test_clip.mp4")
        dse.video.play(screen)
    end)

    if not ok then
        -- Fallback: just show a colored plane as placeholder
    end

    state.screen = screen

    -- Room environment
    local floor = ecs.create_entity()
    ecs.add_transform(floor, 0, 0, 0, 12, 0.1, 10)
    ecs.add_mesh_renderer(floor, "cube")

    -- Seats
    for row = 0, 1 do
        for col = 0, 3 do
            local seat = ecs.create_entity()
            ecs.add_transform(seat, -3 + col * 2, 0.5, -3 - row * 2, 0.6, 1, 0.6)
            ecs.add_mesh_renderer(seat, "cube")
        end
    end

    -- Dim ambient light (cinema)
    local light = ecs.create_entity()
    ecs.add_transform(light, 0, 6, -3, 1, 1, 1)
    ecs.add_point_light(light, 0.3, 0.3, 0.4, 0.5, 12.0)
end

function M.Update(dt)
    state.elapsed = state.elapsed + dt
end

return M