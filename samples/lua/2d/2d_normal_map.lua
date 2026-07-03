local M = {}
M._meta = { name = "2D Normal Map Lighting", category = "2d" }

local state = { elapsed = 0 }

function M.Setup(config)
    local cam = dse.ecs.create_entity()
    dse.ecs.add_transform(cam, 0, 0, 0, 1, 1, 1)
    dse.ecs.add_camera(cam, config.camera_ortho_size or 5.0)

    local tex = (dse.assets and dse.assets.load_texture) and dse.assets.load_texture("data/textures/white.png") or 0

    -- Set dim ambient
    pcall(dse.ecs.set_ambient_2d, 0.05, 0.05, 0.08)

    -- Point light that will orbit
    local light = dse.ecs.create_entity()
    dse.ecs.add_transform(light, 2.0, 2.0, 0, 1, 1, 1)
    pcall(dse.ecs.add_light_2d, light)
    pcall(dse.ecs.set_light_2d_color, light, 1.0, 0.9, 0.7)
    pcall(dse.ecs.set_light_2d_intensity, light, 3.0)
    pcall(dse.ecs.set_light_2d_range, light, 8.0)
    state.light = light

    -- Sprite with normal map for bumpy surface effect
    local surface = dse.ecs.create_entity()
    dse.ecs.add_transform(surface, 0, 0, 0, 4.0, 4.0, 1)
    dse.ecs.add_sprite(surface, 0.7, 0.6, 0.5, 1.0, 0, tex)
    pcall(dse.ecs.add_normal_map_2d, surface, tex) -- uses white as placeholder normal

    -- Smaller objects with normal maps
    local obj1 = dse.ecs.create_entity()
    dse.ecs.add_transform(obj1, -1.5, 1.5, 0, 1.2, 1.2, 1)
    dse.ecs.add_sprite(obj1, 0.8, 0.3, 0.3, 1.0, 1, tex)
    pcall(dse.ecs.add_normal_map_2d, obj1, tex)

    local obj2 = dse.ecs.create_entity()
    dse.ecs.add_transform(obj2, 1.5, -1.0, 0, 1.5, 1.5, 1)
    dse.ecs.add_sprite(obj2, 0.3, 0.6, 0.8, 1.0, 1, tex)
    pcall(dse.ecs.add_normal_map_2d, obj2, tex)
end

function M.Update(dt)
    state.elapsed = state.elapsed + dt
    -- Light orbits around center
    local radius = 3.0
    local x = math.cos(state.elapsed * 1.5) * radius
    local y = math.sin(state.elapsed * 1.5) * radius
    dse.ecs.add_transform(state.light, x, y, 0, 1, 1, 1)
end

return M