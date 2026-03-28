-- 3D MVP Test Scene
local Runtime3DMVP = {}

local camera_entity = nil
local cube_entity = nil
local light_entity = nil
local time_acc = 0.0

function Runtime3DMVP.Setup(config)
    print("[3D-MVP] Setting up 3D MVP Scene...")
    
    -- 1. Camera 3D
    camera_entity = dse.ecs.create_entity()
    dse.ecs.add_transform(camera_entity, 0.0, 0.0, 5.0, 1.0, 1.0, 1.0)
    dse.ecs.add_camera_3d(camera_entity, 60.0, 100)
    
    light_entity = dse.ecs.create_entity()
    dse.ecs.add_directional_light_3d(light_entity, -0.5, -1.0, -0.3, 1.0, 1.0, 0.95, 1.0, 0.2, 0.35)
    
    -- 2. Mesh Renderer (A simple Cube)
    cube_entity = dse.ecs.create_entity()
    dse.ecs.add_transform(cube_entity, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0)
    
    -- Cube vertices (pos only for now)
    local vertices = {
        -- Front face
        -0.5, -0.5,  0.5,
         0.5, -0.5,  0.5,
         0.5,  0.5,  0.5,
        -0.5,  0.5,  0.5,
        -- Back face
        -0.5, -0.5, -0.5,
         0.5, -0.5, -0.5,
         0.5,  0.5, -0.5,
        -0.5,  0.5, -0.5
    }
    
    local indices = {
        0, 1, 2, 2, 3, 0, -- Front
        1, 5, 6, 6, 2, 1, -- Right
        5, 4, 7, 7, 6, 5, -- Back
        4, 0, 3, 3, 7, 4, -- Left
        3, 2, 6, 6, 7, 3, -- Top
        4, 5, 1, 1, 0, 4  -- Bottom
    }
    
    dse.ecs.add_mesh_renderer(cube_entity, 1.0, 0.0, 0.0, 1.0, vertices, indices)
    dse.ecs.set_mesh_shader_variant(cube_entity, "MESH_LIT")
    dse.ecs.set_mesh_material(cube_entity, 0.05, 0.55, 1.0, 0.05, 0.0, 0.0, 1.0, true)
    
    print("[3D-MVP] Setup completed successfully.")
end

function Runtime3DMVP.Update(delta_time)
    if cube_entity == nil then
        return
    end
    time_acc = time_acc + delta_time
    
    -- Spin the cube
    local rot_x = time_acc * 30.0
    local rot_y = time_acc * 45.0
    -- print("[3D-MVP] Update rot_x: " .. rot_x .. " rot_y: " .. rot_y)
    dse.ecs.set_transform_rotation(cube_entity, rot_x, rot_y, 0.0)
end

return Runtime3DMVP
