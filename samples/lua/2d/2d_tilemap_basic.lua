local M = {}
M._meta = { name = "2D Tilemap Basic", category = "2d" }

local state = {}

function M.Setup(config)
    local cam = dse.ecs.create_entity()
    dse.ecs.add_transform(cam, 0, 0, 0, 1, 1, 1)
    dse.ecs.add_camera(cam, config.camera_ortho_size or 8.0)

    local tex = dse.assets.load_texture("data/textures/white.png")

    -- Create tilemap: 16x10 grid
    local tilemap = dse.ecs.create_entity()
    dse.ecs.add_transform(tilemap, -8.0, -5.0, 0, 1, 1, 1)
    dse.ecs.add_tilemap(tilemap, 16, 10, 1.0, tex)

    -- Fill ground layer (bottom 2 rows)
    for x = 0, 15 do
        for y = 0, 1 do
            dse.ecs.set_tile(tilemap, x, y, 1)
        end
    end

    -- Platforms
    for x = 3, 6 do
        dse.ecs.set_tile(tilemap, x, 4, 2)
    end
    for x = 9, 13 do
        dse.ecs.set_tile(tilemap, x, 3, 2)
    end
    for x = 5, 8 do
        dse.ecs.set_tile(tilemap, x, 7, 3)
    end

    -- Walls
    for y = 2, 5 do
        dse.ecs.set_tile(tilemap, 0, y, 4)
        dse.ecs.set_tile(tilemap, 15, y, 4)
    end

    state.tilemap = tilemap

    -- Background
    local bg = dse.ecs.create_entity()
    dse.ecs.add_transform(bg, 0, 0, 0, 20.0, 14.0, 1)
    dse.ecs.add_sprite(bg, 0.05, 0.08, 0.15, 1.0, -10, tex)
end

function M.Update(dt)
end

return M