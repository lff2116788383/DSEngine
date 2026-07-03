local M = {}
M._meta = { name = "2D Lighting and Shadows", category = "2d" }

local state = { elapsed = 0 }

function M.Setup(config)
    local cam = dse.ecs.create_entity()
    dse.ecs.add_transform(cam, 0, 0, 0, 1, 1, 1)
    dse.ecs.add_camera(cam, config.camera_ortho_size or 7.0)

    local tex = dse.assets.load_texture("data/textures/white.png")

    -- Set ambient light (dim)
    dse.ecs.set_ambient_2d(0.08, 0.08, 0.12)

    -- Red point light on left
    local light1 = dse.ecs.create_entity()
    dse.ecs.add_transform(light1, -3.0, 1.0, 0, 1, 1, 1)
    dse.ecs.add_light_2d(light1)
    dse.ecs.set_light_2d_color(light1, 1.0, 0.3, 0.1)
    dse.ecs.set_light_2d_intensity(light1, 2.5)
    dse.ecs.set_light_2d_range(light1, 5.0)
    dse.ecs.set_light_2d_shadow(light1, true)
    state.light1 = light1

    -- Blue point light on right
    local light2 = dse.ecs.create_entity()
    dse.ecs.add_transform(light2, 3.0, -1.0, 0, 1, 1, 1)
    dse.ecs.add_light_2d(light2)
    dse.ecs.set_light_2d_color(light2, 0.1, 0.4, 1.0)
    dse.ecs.set_light_2d_intensity(light2, 2.0)
    dse.ecs.set_light_2d_range(light2, 6.0)
    dse.ecs.set_light_2d_shadow(light2, true)
    state.light2 = light2

    -- Shadow casters (walls)
    local wall1 = dse.ecs.create_entity()
    dse.ecs.add_transform(wall1, -1.0, 0, 0, 0.3, 3.0, 1)
    dse.ecs.add_sprite(wall1, 0.4, 0.4, 0.4, 1.0, 0, tex)

    local wall2 = dse.ecs.create_entity()
    dse.ecs.add_transform(wall2, 1.5, 1.5, 0, 2.0, 0.3, 1)
    dse.ecs.add_sprite(wall2, 0.4, 0.4, 0.4, 1.0, 0, tex)

    -- Floor
    local floor = dse.ecs.create_entity()
    dse.ecs.add_transform(floor, 0, 0, 0, 14.0, 10.0, 1)
    dse.ecs.add_sprite(floor, 0.2, 0.2, 0.2, 1.0, -5, tex)
end

function M.Update(dt)
    state.elapsed = state.elapsed + dt
end

return M