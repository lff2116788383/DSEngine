-- 车辆物理 Demo —— Raycast 车辆模型
-- 场景：一辆简易四轮车在平地上行驶，IJKL 控制油门/刹车/转向
local VehicleDemo = {}


VehicleDemo._meta = {
    name     = "车辆物理 Demo —— Raycast 车辆模型",
    category = "physics",
    config   = { camera_distance=18.0 },
}

local KEY_I = 73  -- 前进
local KEY_K = 75  -- 刹车/倒车
local KEY_J = 74  -- 左转
local KEY_L = 76  -- 右转
local KEY_SPACE = 32 -- 手刹

local camera_entity = nil
local light_entity = nil
local ground = nil
local car_body = nil
local wheel_entities = {}
local hud_logged = false

local cube_verts = {-0.5,-0.5,0.5, 0.5,-0.5,0.5, 0.5,0.5,0.5, -0.5,0.5,0.5,
                    -0.5,-0.5,-0.5, 0.5,-0.5,-0.5, 0.5,0.5,-0.5, -0.5,0.5,-0.5}
local cube_inds  = {0,1,2,2,3,0, 1,5,6,6,2,1, 5,4,7,7,6,5, 4,0,3,3,7,4, 3,2,6,6,7,3, 4,5,1,1,0,4}

function VehicleDemo.Setup(config)
    print("[车辆Demo] 初始化场景...")

    -- 相机
    camera_entity = dse.ecs.create_entity()
    dse.ecs.add_transform(camera_entity, 0.0, 8.0, 18.0, 1.0, 1.0, 1.0)
    dse.ecs.set_transform_rotation(camera_entity, -20.0, 0.0, 0.0)
    dse.ecs.add_camera_3d(camera_entity, 60.0, 100)
    dse.ecs.add_free_camera_controller(camera_entity)

    -- 灯光
    light_entity = dse.ecs.create_entity()
    dse.ecs.add_directional_light_3d(light_entity, -0.4, -1.0, -0.3, 1.0, 0.98, 0.92, 1.2, 0.25, 0.3)

    -- 地面（大平面）
    ground = dse.ecs.create_entity()
    dse.ecs.add_transform(ground, 0.0, -0.25, 0.0, 100.0, 0.5, 100.0)
    dse.ecs.add_mesh_renderer(ground, 0.35, 0.38, 0.35, 1.0, cube_verts, cube_inds)
    dse.ecs.set_mesh_shader_variant(ground, "MESH_LIT")
    dse.ecs.add_rigidbody_3d(ground, 0, 0.0)
    dse.ecs.add_box_collider_3d(ground, 100.0, 0.5, 100.0)

    -- 车身
    car_body = dse.ecs.create_entity()
    dse.ecs.add_transform(car_body, 0.0, 2.0, 0.0, 2.0, 0.6, 4.0)
    dse.ecs.add_mesh_renderer(car_body, 0.15, 0.45, 0.85, 1.0, cube_verts, cube_inds)
    dse.ecs.set_mesh_shader_variant(car_body, "MESH_LIT")
    dse.ecs.add_rigidbody_3d(car_body, 2, 1200.0)
    dse.ecs.add_box_collider_3d(car_body, 2.0, 0.6, 4.0)

    -- 添加车辆组件
    dse.ecs.add_vehicle(car_body, 6000.0, 4000.0, 35.0)

    -- 四个车轮 (前左、前右、后左、后右)
    local wheel_configs = {
        { x = -0.9, y = -0.2, z =  1.4, steer = true,  drive = false, name = "FL" },
        { x =  0.9, y = -0.2, z =  1.4, steer = true,  drive = false, name = "FR" },
        { x = -0.9, y = -0.2, z = -1.4, steer = false, drive = true,  name = "RL" },
        { x =  0.9, y = -0.2, z = -1.4, steer = false, drive = true,  name = "RR" },
    }

    for _, w in ipairs(wheel_configs) do
        dse.ecs.vehicle_add_wheel(car_body, w.x, w.y, w.z, 0.35, w.drive, w.steer, 35000.0, 4500.0)

        -- 车轮可视化
        local we = dse.ecs.create_entity()
        dse.ecs.add_transform(we, 0.0, 0.0, 0.0, 0.7, 0.7, 0.15)
        dse.ecs.add_mesh_renderer(we, 0.2, 0.2, 0.2, 1.0, cube_verts, cube_inds)
        dse.ecs.set_mesh_shader_variant(we, "MESH_LIT")
        table.insert(wheel_entities, { entity = we, offset_x = w.x, offset_y = w.y, offset_z = w.z })
    end

    print("[车辆Demo] 就绪！I=前进 K=刹车 J=左转 L=右转，WASD 移动相机")
end

function VehicleDemo.Update(delta_time)
    if not car_body then return end

    -- 读取键盘输入
    local throttle = 0.0
    local brake = 0.0
    local steering = 0.0

    if dse.app.get_key(KEY_I) then throttle = 1.0 end
    if dse.app.get_key(KEY_K) then brake = 1.0 end
    if dse.app.get_key(KEY_J) then steering = -1.0 end
    if dse.app.get_key(KEY_L) then steering = 1.0 end
    if dse.app.get_key(KEY_SPACE) then brake = 1.0 end

    dse.ecs.vehicle_set_input(car_body, throttle, brake, steering)

    -- 更新车轮可视化位置（跟随车身）
    local cx, cy, cz = dse.ecs.get_transform_position(car_body)
    if cx then
        for _, w in ipairs(wheel_entities) do
            dse.ecs.set_transform_position(w.entity,
                cx + w.offset_x,
                cy + w.offset_y,
                cz + w.offset_z)
        end
    end

    -- 定期输出速度
    if not hud_logged then
        local speed = dse.ecs.vehicle_get_speed(car_body)
        if speed and speed > 0.1 then
            print(string.format("[车辆Demo] 当前速度: %.1f m/s", speed))
            hud_logged = true
        end
    end
end

return VehicleDemo
