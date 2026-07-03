local M = {}
M._meta = { name = "2D Physics Joints", category = "2d" }

local state = {}

function M.Setup(config)
    local cam = dse.ecs.create_entity()
    dse.ecs.add_transform(cam, 0, 0, 0, 1, 1, 1)
    dse.ecs.add_camera(cam, config.camera_ortho_size or 10.0)

    local tex = dse.assets.load_texture("data/textures/white.png")

    -- === Revolute Joint (pendulum) ===
    local anchor1 = dse.ecs.create_entity()
    dse.ecs.add_transform(anchor1, -4.0, 4.0, 0, 0.4, 0.4, 1)
    dse.ecs.add_sprite(anchor1, 0.9, 0.9, 0.9, 1.0, 2, tex)
    dse.ecs.add_rigid_body(anchor1, 0, 1.0, 0) -- static

    local pendulum = dse.ecs.create_entity()
    dse.ecs.add_transform(pendulum, -4.0, 1.0, 0, 0.6, 0.6, 1)
    dse.ecs.add_sprite(pendulum, 1.0, 0.4, 0.2, 1.0, 1, tex)
    dse.ecs.add_rigid_body(pendulum, 2, 1.0, 0)
    dse.ecs.add_box_collider(pendulum, 0.6, 0.6, 1.0, 0.3, 0.5)
    dse.ecs.add_joint_2d(pendulum, anchor1)
    dse.ecs.set_joint_2d_revolute(pendulum, 0.0, 3.0)

    -- === Distance Joint (spring) ===
    local anchor2 = dse.ecs.create_entity()
    dse.ecs.add_transform(anchor2, 0.0, 4.0, 0, 0.4, 0.4, 1)
    dse.ecs.add_sprite(anchor2, 0.9, 0.9, 0.9, 1.0, 2, tex)
    dse.ecs.add_rigid_body(anchor2, 0, 1.0, 0)

    local bob = dse.ecs.create_entity()
    dse.ecs.add_transform(bob, 0.0, 0.0, 0, 0.8, 0.8, 1)
    dse.ecs.add_sprite(bob, 0.2, 0.7, 0.4, 1.0, 1, tex)
    dse.ecs.add_rigid_body(bob, 2, 1.0, 0)
    dse.ecs.add_box_collider(bob, 0.8, 0.8, 1.0, 0.3, 0.5)
    dse.ecs.add_joint_2d(bob, anchor2)
    dse.ecs.set_joint_2d_distance(bob, 4.0, 0.5, 5.0)

    -- === Prismatic Joint (slider) ===
    local rail = dse.ecs.create_entity()
    dse.ecs.add_transform(rail, 4.0, 2.0, 0, 0.3, 6.0, 1)
    dse.ecs.add_sprite(rail, 0.3, 0.3, 0.5, 0.5, -1, tex)
    dse.ecs.add_rigid_body(rail, 0, 1.0, 0)

    local slider = dse.ecs.create_entity()
    dse.ecs.add_transform(slider, 4.0, 3.0, 0, 0.7, 0.7, 1)
    dse.ecs.add_sprite(slider, 0.6, 0.3, 0.8, 1.0, 1, tex)
    dse.ecs.add_rigid_body(slider, 2, 1.0, 0)
    dse.ecs.add_box_collider(slider, 0.7, 0.7, 1.0, 0.2, 0.3)
    dse.ecs.add_joint_2d(slider, rail)
    dse.ecs.set_joint_2d_prismatic(slider, 0.0, 1.0, -3.0, 3.0)

    -- Ground
    local ground = dse.ecs.create_entity()
    dse.ecs.add_transform(ground, 0, -6.0, 0, 16.0, 1.0, 1)
    dse.ecs.add_sprite(ground, 0.25, 0.25, 0.3, 1.0, 0, tex)
    dse.ecs.add_rigid_body(ground, 0, 1.0, 0)
    dse.ecs.add_box_collider(ground, 16.0, 1.0, 1.0, 0.4, 0.1)
end

function M.Update(dt)
end

return M