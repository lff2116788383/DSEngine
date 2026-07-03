local M = {}
M._meta = { name = "3D Behavior Tree AI", category = "ai" }

local ecs = dse.ecs
local state = { elapsed = 0, agents = {} }

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
    ecs.add_transform(cam, 0, 12, -15, 1, 1, 1)
    ecs.add_camera_3d(cam, 60.0, 100.0)
    ecs.add_free_camera_controller(cam)

    local ground = ecs.create_entity()
    ecs.add_transform(ground, 0, 0, 0, 30, 0.1, 30)
    ecs.add_mesh_renderer(ground, 0.35, 0.4, 0.35, 1.0, cube_vertices(), cube_indices())

    for i = 1, 4 do
        local agent = ecs.create_entity()
        local x = math.cos(i * 1.57) * 5.0
        local z = math.sin(i * 1.57) * 5.0
        ecs.add_transform(agent, x, 0.8, z, 0.8, 1.6, 0.8)
        ecs.add_mesh_renderer(agent, 0.2 + i*0.15, 0.7, 0.3, 1.0, cube_vertices(), cube_indices())
        table.insert(state.agents, {entity = agent, angle = i * 1.57, speed = 2.0 + i * 0.5})
    end

    local target = ecs.create_entity()
    ecs.add_transform(target, 0, 0.5, 0, 0.6, 0.6, 0.6)
    ecs.add_mesh_renderer(target, 1.0, 0.3, 0.2, 1.0, cube_vertices(), cube_indices())
    state.target = target

    for i = 1, 8 do
        local wp = ecs.create_entity()
        local angle = (i / 8) * math.pi * 2
        ecs.add_transform(wp, math.cos(angle) * 10, 0.2, math.sin(angle) * 10, 0.3, 0.3, 0.3)
        ecs.add_mesh_renderer(wp, 0.9, 0.8, 0.2, 1.0, cube_vertices(), cube_indices())
    end

    local light = ecs.create_entity()
    ecs.add_transform(light, 0, 15, 0, 1, 1, 1)
    ecs.add_directional_light_3d(light, -0.45, -1.0, -0.25, 1.0, 0.95, 0.9, 1.5, 0.15, 0.25)
end

function M.Update(dt)
    state.elapsed = state.elapsed + dt
    for _, a in ipairs(state.agents) do
        a.angle = a.angle + dt * a.speed * 0.2
        local r = 5.0 + math.sin(state.elapsed * 0.5) * 2.0
        local x = math.cos(a.angle) * r
        local z = math.sin(a.angle) * r
        ecs.add_transform(a.entity, x, 0.8, z, 0.8, 1.6, 0.8)
    end
end

return M
