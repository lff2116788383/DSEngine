local M = {}
M._meta = { name = "3D Open World Complete", category = "comprehensive" }

local ecs = dse.ecs
local state = { elapsed = 0, character = 0, clouds = {} }

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
    local cam = ecs.create_entity()
    ecs.add_transform(cam, 0, 15, -25, 1, 1, 1)
    ecs.add_camera_3d(cam, 60.0, 200.0)
    ecs.add_free_camera_controller(cam)

    local ground = ecs.create_entity()
    ecs.add_transform(ground, 0, 0, 0, 60, 0.2, 60)
    ecs.add_mesh_renderer(ground, 0.3, 0.45, 0.2, 1.0, cube_vertices(), cube_indices())

    local char = ecs.create_entity()
    ecs.add_transform(char, 0, 1.0, 0, 0.6, 1.8, 0.6)
    ecs.add_mesh_renderer(char, 0.2, 0.5, 0.8, 1.0, cube_vertices(), cube_indices())
    state.character = char

    for i = 1, 20 do
        local x = math.cos(i * 0.7) * (8 + i * 0.8)
        local z = math.sin(i * 1.1) * (6 + i * 0.6)
        local trunk = ecs.create_entity()
        ecs.add_transform(trunk, x, 2.0, z, 0.3, 4.0, 0.3)
        ecs.add_mesh_renderer(trunk, 0.4, 0.25, 0.15, 1.0, cube_vertices(), cube_indices())
        local canopy = ecs.create_entity()
        ecs.add_transform(canopy, x, 4.5, z, 2.0, 2.0, 2.0)
        ecs.add_mesh_renderer(canopy, 0.15, 0.5, 0.15, 1.0, cube_vertices(), cube_indices())
    end

    local water = ecs.create_entity()
    ecs.add_transform(water, 20, 0.3, 10, 15, 0.05, 15)
    ecs.add_mesh_renderer(water, 0.1, 0.3, 0.6, 0.7, cube_vertices(), cube_indices())

    for i = 1, 5 do
        local cloud = ecs.create_entity()
        local cx = -20 + i * 10
        ecs.add_transform(cloud, cx, 20, math.sin(i) * 10, 4+i, 1, 3+i)
        ecs.add_mesh_renderer(cloud, 0.9, 0.9, 0.95, 0.6, cube_vertices(), cube_indices())
        table.insert(state.clouds, cloud)
    end

    for i = 1, 4 do
        local bldg = ecs.create_entity()
        local h = 3 + i * 2
        ecs.add_transform(bldg, -15 + i * 5, h/2, -10, 3, h, 3)
        ecs.add_mesh_renderer(bldg, 0.6, 0.55, 0.5, 1.0, cube_vertices(), cube_indices())
    end

    local light = ecs.create_entity()
    ecs.add_transform(light, 0, 30, 0, 1, 1, 1)
    ecs.add_directional_light_3d(light, -0.3, -0.8, -0.2, 1.0, 0.95, 0.85, 1.5, 0.15, 0.3)
end

function M.Update(dt)
    state.elapsed = state.elapsed + dt
    local t = state.elapsed * 0.4
    local x = math.sin(t) * 8
    local z = math.cos(t * 0.7) * 6
    ecs.add_transform(state.character, x, 1.0, z, 0.6, 1.8, 0.6)
    for i, c in ipairs(state.clouds) do
        local cx = -20 + i * 10 + state.elapsed * 0.5
        if cx > 30 then cx = cx - 60 end
        ecs.add_transform(c, cx, 20, math.sin(i) * 10, 4+i, 1, 3+i)
    end
end

return M
