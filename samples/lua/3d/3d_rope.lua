-- 绳索模拟 Demo —— Verlet 积分绳索/链条
-- 场景：两个立方体之间悬挂一根绳索，在重力下自然下垂
local RopeDemo = {}


RopeDemo._meta = {
    name     = "绳索模拟 Demo —— Verlet 积分绳索/链条",
    category = "physics",
    config   = { camera_distance=15.0 },
}

local camera_entity = nil
local light_entity = nil
local ground = nil
local anchor_a = nil
local anchor_b = nil
local rope_entity = nil
local rope_visual_entities = {}

local cube_verts = {-0.5,-0.5,0.5, 0.5,-0.5,0.5, 0.5,0.5,0.5, -0.5,0.5,0.5,
                    -0.5,-0.5,-0.5, 0.5,-0.5,-0.5, 0.5,0.5,-0.5, -0.5,0.5,-0.5}
local cube_inds  = {0,1,2,2,3,0, 1,5,6,6,2,1, 5,4,7,7,6,5, 4,0,3,3,7,4, 3,2,6,6,7,3, 4,5,1,1,0,4}

function RopeDemo.Setup(config)
    print("[绳索Demo] 初始化场景...")

    -- 相机
    camera_entity = dse.ecs.create_entity()
    dse.ecs.add_transform(camera_entity, 0.0, 5.0, 15.0, 1.0, 1.0, 1.0)
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

    -- 左锚点
    anchor_a = dse.ecs.create_entity()
    dse.ecs.add_transform(anchor_a, -4.0, 6.0, 0.0, 0.6, 0.6, 0.6)
    dse.ecs.add_mesh_renderer(anchor_a, 0.9, 0.2, 0.2, 1.0, cube_verts, cube_inds)
    dse.ecs.set_mesh_shader_variant(anchor_a, "MESH_LIT")

    -- 右锚点
    anchor_b = dse.ecs.create_entity()
    dse.ecs.add_transform(anchor_b, 4.0, 6.0, 0.0, 0.6, 0.6, 0.6)
    dse.ecs.add_mesh_renderer(anchor_b, 0.2, 0.2, 0.9, 1.0, cube_verts, cube_inds)
    dse.ecs.set_mesh_shader_variant(anchor_b, "MESH_LIT")

    -- 绳索实体
    local segment_count = 20
    local segment_length = 0.5
    rope_entity = dse.ecs.create_entity()
    dse.ecs.add_transform(rope_entity, 0.0, 6.0, 0.0, 1.0, 1.0, 1.0)
    dse.ecs.add_rope(rope_entity, segment_count, segment_length, 0.98, 10)
    dse.ecs.rope_set_anchors(rope_entity, anchor_a, anchor_b, 0,0,0, 0,0,0)
    dse.ecs.rope_set_gravity(rope_entity, true, 1.0)

    -- 为每个绳索段创建可视化小方块
    for i = 1, segment_count + 1 do
        local ve = dse.ecs.create_entity()
        dse.ecs.add_transform(ve, 0, 0, 0, 0.08, 0.08, 0.08)
        dse.ecs.add_mesh_renderer(ve, 1.0, 0.8, 0.2, 1.0, cube_verts, cube_inds)
        dse.ecs.set_mesh_shader_variant(ve, "MESH_LIT")
        table.insert(rope_visual_entities, ve)
    end

    print(string.format("[绳索Demo] 就绪！%d段绳索连接左右锚点，WASD 移动相机", segment_count))
end

function RopeDemo.Update(delta_time)
    if not rope_entity then return end

    -- 获取绳索粒子位置，同步到可视化实体
    local positions = dse.ecs.rope_get_positions(rope_entity)
    if positions then
        for i, pos in ipairs(positions) do
            if rope_visual_entities[i] then
                dse.ecs.set_transform_position(rope_visual_entities[i], pos[1], pos[2], pos[3])
            end
        end
    end
end

return RopeDemo
