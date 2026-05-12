-- Ragdoll 布娃娃 Demo —— 简易版（无骨骼动画模型）
-- 场景：用多个刚体模拟人体部件，验证 Ragdoll Lua 接口
-- 注：完整 Ragdoll 需要骨骼动画模型，这里用立方体串联演示接口可用性
local RagdollDemo = {}

local camera_entity = nil
local light_entity = nil
local ground = nil
local ragdoll_entity = nil
local body_parts = {}

local cube_verts = {-0.5,-0.5,0.5, 0.5,-0.5,0.5, 0.5,0.5,0.5, -0.5,0.5,0.5,
                    -0.5,-0.5,-0.5, 0.5,-0.5,-0.5, 0.5,0.5,-0.5, -0.5,0.5,-0.5}
local cube_inds  = {0,1,2,2,3,0, 1,5,6,6,2,1, 5,4,7,7,6,5, 4,0,3,3,7,4, 3,2,6,6,7,3, 4,5,1,1,0,4}

-- 创建一个刚体方块
local function create_body_part(name, x, y, z, sx, sy, sz, mass, color, dynamic)
    local e = dse.ecs.create_entity()
    dse.ecs.add_transform(e, x, y, z, sx, sy, sz)
    dse.ecs.add_mesh_renderer(e, color[1], color[2], color[3], 1.0, cube_verts, cube_inds)
    dse.ecs.set_mesh_shader_variant(e, "MESH_LIT")
    dse.ecs.add_rigidbody_3d(e, dynamic and 2 or 0, mass)
    dse.ecs.add_box_collider_3d(e, sx, sy, sz)
    table.insert(body_parts, { name = name, entity = e })
    return e
end

function RagdollDemo.Setup(config)
    print("[Ragdoll Demo] 初始化场景...")

    -- 相机
    camera_entity = dse.ecs.create_entity()
    dse.ecs.add_transform(camera_entity, 0.0, 4.0, 12.0, 1.0, 1.0, 1.0)
    dse.ecs.set_transform_rotation(camera_entity, -15.0, 0.0, 0.0)
    dse.ecs.add_camera_3d(camera_entity, 60.0, 100)
    dse.ecs.add_free_camera_controller(camera_entity)

    -- 灯光
    light_entity = dse.ecs.create_entity()
    dse.ecs.add_directional_light_3d(light_entity, -0.4, -1.0, -0.5, 1.0, 0.98, 0.92, 1.2, 0.3, 0.3)

    -- 地面
    ground = dse.ecs.create_entity()
    dse.ecs.add_transform(ground, 0.0, -0.5, 0.0, 20.0, 1.0, 20.0)
    dse.ecs.add_mesh_renderer(ground, 0.3, 0.3, 0.3, 1.0, cube_verts, cube_inds)
    dse.ecs.set_mesh_shader_variant(ground, "MESH_LIT")
    dse.ecs.add_rigidbody_3d(ground, 0, 0.0)
    dse.ecs.add_box_collider_3d(ground, 20.0, 1.0, 20.0)

    -- 简易人形刚体（从上到下：头、躯干、左臂、右臂、左腿、右腿）
    local torso  = create_body_part("torso",     0.0,  5.0, 0.0, 0.8, 1.2, 0.4, 5.0, {0.7, 0.5, 0.3}, true)
    local head   = create_body_part("head",      0.0,  6.2, 0.0, 0.4, 0.4, 0.4, 1.0, {0.9, 0.75, 0.6}, true)
    local l_arm  = create_body_part("left_arm", -0.8,  5.0, 0.0, 0.3, 1.0, 0.3, 1.5, {0.7, 0.5, 0.3}, true)
    local r_arm  = create_body_part("right_arm", 0.8,  5.0, 0.0, 0.3, 1.0, 0.3, 1.5, {0.7, 0.5, 0.3}, true)
    local l_leg  = create_body_part("left_leg", -0.3,  3.4, 0.0, 0.3, 1.2, 0.3, 2.0, {0.3, 0.3, 0.6}, true)
    local r_leg  = create_body_part("right_leg", 0.3,  3.4, 0.0, 0.3, 1.2, 0.3, 2.0, {0.3, 0.3, 0.6}, true)

    -- 连接关节
    -- 头-躯干
    dse.ecs.add_joint_3d(head, torso, 0, 0, 0, 0, 5000.0) -- type=0 Fixed
    -- 左臂-躯干
    dse.ecs.add_joint_3d(l_arm, torso, 1, 0, 0, 0, 3000.0) -- type=1 Hinge
    -- 右臂-躯干
    dse.ecs.add_joint_3d(r_arm, torso, 1, 0, 0, 0, 3000.0)
    -- 左腿-躯干
    dse.ecs.add_joint_3d(l_leg, torso, 1, 0, 0, 0, 4000.0)
    -- 右腿-躯干
    dse.ecs.add_joint_3d(r_leg, torso, 1, 0, 0, 0, 4000.0)

    -- 创建一个用于标记的 Ragdoll 组件（演示 Lua 接口可用性）
    ragdoll_entity = torso
    dse.ecs.add_ragdoll(ragdoll_entity, 12.0, false, 0.0, 50.0)
    dse.ecs.ragdoll_activate(ragdoll_entity)

    print("[Ragdoll Demo] 就绪！简易人形模型自由落体，WASD 移动相机")
    print(string.format("[Ragdoll Demo] ragdoll active = %s",
        tostring(dse.ecs.ragdoll_is_active(ragdoll_entity))))
end

local log_timer = 0.0

function RagdollDemo.Update(delta_time)
    log_timer = log_timer + delta_time

    -- 2秒后输出所有部件位置
    if log_timer > 2.0 and #body_parts > 0 then
        for _, part in ipairs(body_parts) do
            local x, y, z = dse.ecs.get_transform_position(part.entity)
            print(string.format("[Ragdoll] %s pos=(%.2f,%.2f,%.2f)",
                part.name, x or 0, y or 0, z or 0))
        end
        body_parts = {} -- 只输出一次
    end
end

return RagdollDemo
