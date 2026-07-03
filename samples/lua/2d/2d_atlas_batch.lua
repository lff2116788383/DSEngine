local M = {}
M._meta = { name = "2D Atlas Batch Rendering", category = "2d" }

local state = { entities = {} }

function M.Setup(config)
    local cam = dse.ecs.create_entity()
    dse.ecs.add_transform(cam, 0, 0, 0, 1, 1, 1)
    dse.ecs.add_camera(cam, config.camera_ortho_size or 8.0)

    local tex = dse.assets.load_texture("data/textures/white.png")

    -- Simulate batch rendering: create many sprites in a grid
    local cols, rows = 8, 6
    local spacing = 1.2
    local ox = -(cols - 1) * spacing * 0.5
    local oy = -(rows - 1) * spacing * 0.5

    for row = 0, rows - 1 do
        for col = 0, cols - 1 do
            local e = dse.ecs.create_entity()
            local x = ox + col * spacing
            local y = oy + row * spacing
            dse.ecs.add_transform(e, x, y, 0, 0.8, 0.8, 1)
            -- Vary colors based on position
            local r = (col + 1) / cols
            local g = (row + 1) / rows
            local b = 1.0 - r * 0.5
            dse.ecs.add_sprite(e, r, g, b, 1.0, 0, tex)
            table.insert(state.entities, e)
        end
    end
end

function M.Update(dt)
end

return M