-- 3D MVP Test Scene
local Runtime3DMVP = {}

local camera_entity = nil
local light_entity = nil
local cubes = {}

-- Cube vertices (pos, normal, uv)
-- To keep it simple, we just define a helper function to create a physical cube
local function create_cube(x, y, z, scale_x, scale_y, scale_z, is_dynamic, r, g, b)
    local entity = dse.ecs.create_entity()
    dse.ecs.add_transform(entity, x, y, z, scale_x, scale_y, scale_z)
    
    -- Very simple cube vertices
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
    
    dse.ecs.add_mesh_renderer(entity, r, g, b, 1.0, vertices, indices)
    dse.ecs.set_mesh_shader_variant(entity, "MESH_LIT")
    dse.ecs.set_mesh_material(entity, 0.05, 0.55, 1.0, 0.05, 0.0, 0.0, 1.0, true)
    
    -- Physics
    if is_dynamic then
        Ecs.add_rigidbody_3d(entity, 2, 1.0) -- Dynamic, mass 1.0
    else
        Ecs.add_rigidbody_3d(entity, 0, 0.0) -- Static
    end
    Ecs.add_box_collider_3d(entity, scale_x, scale_y, scale_z)
    
    return entity
end

function Runtime3DMVP.Setup(config)
    print("[3D-MVP] Setting up 3D MVP Scene with Physics and Post-Processing...")
    
    -- 1. Camera 3D with Post Process
    camera_entity = dse.ecs.create_entity()
    dse.ecs.add_transform(camera_entity, 0.0, 5.0, 15.0, 1.0, 1.0, 1.0)
    dse.ecs.set_transform_rotation(camera_entity, -15.0, 0.0, 0.0)
    dse.ecs.add_camera_3d(camera_entity, 60.0, 100)
    Ecs.add_free_camera_controller(camera_entity)
    Ecs.add_post_process(camera_entity, true, 1.0, 2.5) -- Bloom enabled, threshold 1.0, intensity 2.5
    
    -- 2. Lighting
    light_entity = dse.ecs.create_entity()
    dse.ecs.add_directional_light_3d(light_entity, -0.5, -1.0, -0.3, 1.0, 1.0, 0.95, 1.2, 0.2, 0.35)
    
    -- 3. Ground Plane (Static)
    create_cube(0.0, -0.5, 0.0, 20.0, 1.0, 20.0, false, 0.3, 0.3, 0.3)
    
    -- 4. Falling Cubes (Dynamic)
    for i = 1, 10 do
        local x = math.random() * 4.0 - 2.0
        local y = 5.0 + i * 2.0
        local z = math.random() * 4.0 - 2.0
        
        -- Make them glow a bit so bloom picks them up
        local r = math.random() * 0.5 + 0.5
        local g = math.random() * 0.5 + 0.5
        local b = math.random() * 0.5 + 0.5
        
        local cube = create_cube(x, y, z, 1.0, 1.0, 1.0, true, r, g, b)
        -- Give them random initial rotation
        dse.ecs.set_transform_rotation(cube, math.random() * 360, math.random() * 360, math.random() * 360)
        table.insert(cubes, cube)
    end
    
    print("[3D-MVP] Setup completed successfully. Press W/A/S/D to fly around!")
end

function Runtime3DMVP.Update(delta_time)
    -- Physics engine runs in the background automatically!
end

return Runtime3DMVP
