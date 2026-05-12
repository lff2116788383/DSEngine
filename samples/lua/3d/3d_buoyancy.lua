-- 浮力模拟 Demo —— 水面浮沉效果
-- 场景：几个动态方块从空中落入水面，产生浮力效果
local BuoyancyDemo = {}

local camera_entity = nil
local light_entity = nil
local ground = nil
local water_plane = nil
local floating_boxes = {}
local WATER_LEVEL = 2.0

local cube_verts = {-0.5,-0.5,0.5, 0.5,-0.5,0.5, 0.5,0.5,0.5, -0.5,0.5,0.5,
                    -0.5,-0.5,-0.5, 0.5,-0.5,-0.5, 0.5,0.5,-0.5, -0.5,0.5,-0.5}
local cube_inds  = {0,1,2,2,3,0, 1,5,6,6,2,1, 5,4,7,7,6,5, 4,0,3,3,7,4, 3,2,6,6,7,3, 4,5,1,1,0,4}

function BuoyancyDemo.Setup(config)
    print("[浮力Demo] 初始化场景...")

    -- 相机
    camera_entity = dse.ecs.create_entity()
    dse.ecs.add_transform(camera_entity, 0.0, 6.0, 16.0, 1.0, 1.0, 1.0)
    dse.ecs.set_transform_rotation(camera_entity, -18.0, 0.0, 0.0)
    dse.ecs.add_camera_3d(camera_entity, 60.0, 100)
    dse.ecs.add_free_camera_controller(camera_entity)

    -- 灯光
    light_entity = dse.ecs.create_entity()
    dse.ecs.add_directional_light_3d(light_entity, -0.4, -1.0, -0.5, 1.0, 0.98, 0.92, 1.2, 0.3, 0.3)

    -- 地面
    ground = dse.ecs.create_entity()
    dse.ecs.add_transform(ground, 0.0, -1.0, 0.0, 30.0, 1.0, 30.0)
    dse.ecs.add_mesh_renderer(ground, 0.3, 0.3, 0.3, 1.0, cube_verts, cube_inds)
    dse.ecs.set_mesh_shader_variant(ground, "MESH_LIT")
    dse.ecs.add_rigidbody_3d(ground, 0, 0.0)
    dse.ecs.add_box_collider_3d(ground, 30.0, 1.0, 30.0)

    -- 水面可视化（半透明蓝色平面）
    water_plane = dse.ecs.create_entity()
    dse.ecs.add_transform(water_plane, 0.0, WATER_LEVEL, 0.0, 20.0, 0.05, 20.0)
    dse.ecs.add_mesh_renderer(water_plane, 0.1, 0.3, 0.7, 0.5, cube_verts, cube_inds)
    dse.ecs.set_mesh_shader_variant(water_plane, "MESH_LIT")

    -- 浮力方块（不同质量和起始高度）
    local box_configs = {
        { x = -3.0, y = 8.0, z = 0.0, mass = 5.0,  force = 15.0, color = {0.9, 0.3, 0.2} },
        { x =  0.0, y = 10.0, z = 0.0, mass = 2.0, force = 10.0, color = {0.3, 0.9, 0.2} },
        { x =  3.0, y = 12.0, z = 0.0, mass = 8.0, force = 20.0, color = {0.2, 0.3, 0.9} },
        { x = -1.5, y = 9.0,  z = 2.0, mass = 3.0, force = 12.0, color = {0.9, 0.9, 0.2} },
        { x =  1.5, y = 11.0, z = -2.0, mass = 6.0, force = 18.0, color = {0.9, 0.2, 0.9} },
    }

    for i, bc in ipairs(box_configs) do
        local e = dse.ecs.create_entity()
        dse.ecs.add_transform(e, bc.x, bc.y, bc.z, 1.0, 1.0, 1.0)
        dse.ecs.add_mesh_renderer(e, bc.color[1], bc.color[2], bc.color[3], 1.0, cube_verts, cube_inds)
        dse.ecs.set_mesh_shader_variant(e, "MESH_LIT")
        dse.ecs.add_rigidbody_3d(e, 2, bc.mass)
        dse.ecs.add_box_collider_3d(e, 1.0, 1.0, 1.0)

        -- 添加浮力组件
        dse.ecs.add_buoyancy(e, WATER_LEVEL, bc.force, 3.0, 1.0, 1.0)

        -- 添加多个采样点（方块的上下四角）
        dse.ecs.buoyancy_add_sample_point(e, -0.4, -0.4, -0.4, 1.0)
        dse.ecs.buoyancy_add_sample_point(e,  0.4, -0.4, -0.4, 1.0)
        dse.ecs.buoyancy_add_sample_point(e, -0.4, -0.4,  0.4, 1.0)
        dse.ecs.buoyancy_add_sample_point(e,  0.4, -0.4,  0.4, 1.0)

        table.insert(floating_boxes, { entity = e, name = "box_" .. i, logged = false })
    end

    print(string.format("[浮力Demo] 就绪！%d个方块落入水面（Y=%.1f），WASD 移动相机",
        #floating_boxes, WATER_LEVEL))
end

local log_timer = 0.0

function BuoyancyDemo.Update(delta_time)
    log_timer = log_timer + delta_time

    -- 每2秒输出一次浮力状态
    if log_timer > 2.0 then
        log_timer = 0.0
        for _, box in ipairs(floating_boxes) do
            local x, y, z = dse.ecs.get_transform_position(box.entity)
            local ratio = dse.ecs.buoyancy_get_submerge_ratio(box.entity)
            if x and not box.logged then
                print(string.format("[浮力Demo] %s pos=(%.2f,%.2f,%.2f) 淹没=%.0f%%",
                    box.name, x, y or 0, z or 0, (ratio or 0) * 100))
                if ratio and ratio > 0 then
                    box.logged = true
                end
            end
        end
    end
end

return BuoyancyDemo
