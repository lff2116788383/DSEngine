local M = {}
M._meta = { name = "3D Lightmap Baking", category = "rendering_advanced" }

local ecs = dse.ecs
local state = { elapsed = 0 }

function M.Setup(config)
    -- Camera
    local cam = ecs.create_entity()
    ecs.add_transform(cam, 0, 5, -10, 1, 1, 1)
    ecs.add_camera3d(cam, 60.0, 0.1, 100.0)

    -- Indoor scene (lightmapped)
    -- Floor
    local floor = ecs.create_entity()
    ecs.add_transform(floor, 0, 0, 0, 12, 0.2, 12)
    ecs.add_mesh_renderer(floor, "cube")
    ecs.add_lightmap(floor, 256, 256) -- lightmap UV resolution

    -- Walls
    local walls = {
        {-6, 3, 0, 0.3, 6, 12},
        {6, 3, 0, 0.3, 6, 12},
        {0, 3, 6, 12, 6, 0.3},
        {0, 3, -6, 12, 6, 0.3},
    }
    for _, w in ipairs(walls) do
        local e = ecs.create_entity()
        ecs.add_transform(e, w[1], w[2], w[3], w[4], w[5], w[6])
        ecs.add_mesh_renderer(e, "cube")
        ecs.add_lightmap(e, 128, 128)
    end

    -- Ceiling
    local ceil = ecs.create_entity()
    ecs.add_transform(ceil, 0, 6, 0, 12, 0.2, 12)
    ecs.add_mesh_renderer(ceil, "cube")
    ecs.add_lightmap(ceil, 256, 256)

    -- Interior objects (receive baked light)
    local table_e = ecs.create_entity()
    ecs.add_transform(table_e, 0, 0.8, 0, 3, 0.1, 2)
    ecs.add_mesh_renderer(table_e, "cube")
    ecs.add_lightmap(table_e, 64, 64)

    -- Light sources for baking
    local light1 = ecs.create_entity()
    ecs.add_transform(light1, -2, 5, 0, 1, 1, 1)
    ecs.add_point_light(light1, 1.0, 0.9, 0.7, 3.0, 10.0)

    local light2 = ecs.create_entity()
    ecs.add_transform(light2, 2, 5, 0, 1, 1, 1)
    ecs.add_point_light(light2, 0.7, 0.9, 1.0, 2.0, 10.0)
end

function M.Update(dt)
    state.elapsed = state.elapsed + dt
end

return M