local M = {}
M._meta = { name = "3D Tree Rendering", category = "vegetation" }

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
    ecs.add_transform(cam, 0, 8, -15, 1, 1, 1)
    ecs.add_camera_3d(cam, 60.0, 300.0)
    state.cam = cam

    -- Ground
    local ground = ecs.create_entity()
    ecs.add_transform(ground, 0, 0, 0, 80, 0.1, 80)
    ecs.add_mesh_renderer(ground, 0.6, 0.6, 0.6, 1.0, cube_vertices(), cube_indices())

    -- Forest of trees at various distances (LOD + wind)
    for i = 1, 30 do
        local tree = ecs.create_entity()
        local x = (math.random() - 0.5) * 50.0
        local z = (math.random() - 0.5) * 50.0
        local h = 4.0 + math.random() * 4.0
        ecs.add_transform(tree, x, h * 0.5, z, 1.5, h, 1.5)
        ecs.add_mesh_renderer(tree, 0.7, 0.5, 0.3, 1.0, cube_vertices(), cube_indices())
        ecs.add_tree(tree)

        -- Canopy
        local canopy = ecs.create_entity()
        ecs.add_transform(canopy, x, h * 0.8, z, 3, 2, 3)
        ecs.add_mesh_renderer(canopy, 0.7, 0.7, 0.7, 1.0, cube_vertices(), cube_indices())
        ecs.add_foliage(canopy, "tree_canopy")
    end

    -- Skybox
    local sky = ecs.create_entity()
    ecs.add_transform(sky, 0, 0, 0, 1, 1, 1)
    ecs.add_atmosphere(sky)

    -- Light
    local light = ecs.create_entity()
    ecs.add_transform(light, 0, 30, 0, 1, 1, 1)
    ecs.add_directional_light_3d(light, -0.45, -1.0, -0.25, 1.0, 0.95, 0.85, 1.5, 0.15, 0.25)
end

function M.Update(dt)
    state.elapsed = state.elapsed + dt
end

return M