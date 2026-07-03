local M = {}
M._meta = { name = "3D Video Playback", category = "media" }

local ecs = dse.ecs
local state = { elapsed = 0 }

local function cube_vertices()
    return {
        -0.5,-0.5, 0.5,  0.5,-0.5, 0.5,  0.5, 0.5, 0.5, -0.5, 0.5, 0.5,
        -0.5,-0.5,-0.5,  0.5,-0.5,-0.5,  0.5, 0.5,-0.5, -0.5, 0.5,-0.5,
    }
end
local function cube_indices()
    return {
        0,1,2, 2,3,0, 1,5,6, 6,2,1, 5,4,7, 7,6,5, 4,0,3, 3,7,4, 3,2,6, 6,7,3, 4,5,1, 1,0,4
    }
end

function M.Setup(config)
    -- Camera
    local cam = ecs.create_entity()
    ecs.add_transform(cam, 0, 3, -6, 1, 1, 1)
    ecs.add_camera_3d(cam, 60.0, 100.0)

    -- Video screen (large plane facing camera)
    local screen = ecs.create_entity()
    ecs.add_transform(screen, 0, 3, 0, 8, 4.5, 0.1)
    ecs.add_mesh_renderer(screen, 0.6, 0.6, 0.6, 1.0, cube_vertices(), cube_indices())

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
    ecs.add_mesh_renderer(floor, 0.6, 0.6, 0.6, 1.0, cube_vertices(), cube_indices())

    -- Seats
    for row = 0, 1 do
        for col = 0, 3 do
            local seat = ecs.create_entity()
            ecs.add_transform(seat, -3 + col * 2, 0.5, -3 - row * 2, 0.6, 1, 0.6)
            ecs.add_mesh_renderer(seat, 0.6, 0.6, 0.6, 1.0, cube_vertices(), cube_indices())
        end
    end

    -- Dim ambient light (cinema)
    local light = ecs.create_entity()
    ecs.add_transform(light, 0, 6, -3, 1, 1, 1)
    ecs.add_point_light_3d(light, 0.3, 0.3, 0.4, 0.5, 12.0)
end

function M.Update(dt)
    state.elapsed = state.elapsed + dt
end

return M