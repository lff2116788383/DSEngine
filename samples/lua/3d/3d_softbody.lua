-- 软体模拟 Demo —— PBD 可变形网格
-- 场景：一个软体网格从空中落下并变形
local SoftBodyDemo = {}

local camera_entity = nil
local light_entity = nil
local ground = nil
local softbody_entity = nil

local cube_verts = {-0.5,-0.5,0.5, 0.5,-0.5,0.5, 0.5,0.5,0.5, -0.5,0.5,0.5,
                    -0.5,-0.5,-0.5, 0.5,-0.5,-0.5, 0.5,0.5,-0.5, -0.5,0.5,-0.5}
local cube_inds  = {0,1,2,2,3,0, 1,5,6,6,2,1, 5,4,7,7,6,5, 4,0,3,3,7,4, 3,2,6,6,7,3, 4,5,1,1,0,4}

-- 生成 NxN 网格
local function generate_grid_mesh(cols, rows, spacing)
    local vertices = {}
    local indices = {}
    for r = 0, rows - 1 do
        for c = 0, cols - 1 do
            table.insert(vertices, c * spacing)
            table.insert(vertices, 0.0)
            table.insert(vertices, r * spacing)
        end
    end
    for r = 0, rows - 2 do
        for c = 0, cols - 2 do
            local tl = r * cols + c
            local tr = tl + 1
            local bl = tl + cols
            local br = bl + 1
            table.insert(indices, tl)
            table.insert(indices, bl)
            table.insert(indices, tr)
            table.insert(indices, tr)
            table.insert(indices, bl)
            table.insert(indices, br)
        end
    end
    return vertices, indices
end

function SoftBodyDemo.Setup(config)
    print("[软体Demo] 初始化场景...")

    -- 相机
    camera_entity = dse.ecs.create_entity()
    dse.ecs.add_transform(camera_entity, 2.0, 6.0, 12.0, 1.0, 1.0, 1.0)
    dse.ecs.set_transform_rotation(camera_entity, -20.0, 0.0, 0.0)
    dse.ecs.add_camera_3d(camera_entity, 60.0, 100)
    dse.ecs.add_free_camera_controller(camera_entity)

    -- 灯光
    light_entity = dse.ecs.create_entity()
    dse.ecs.add_directional_light_3d(light_entity, -0.4, -1.0, -0.5, 1.0, 0.98, 0.92, 1.2, 0.3, 0.3)

    -- 地面
    ground = dse.ecs.create_entity()
    dse.ecs.add_transform(ground, 0.0, -0.5, 0.0, 20.0, 1.0, 20.0)
    dse.ecs.add_mesh_renderer(ground, 0.3, 0.35, 0.3, 1.0, cube_verts, cube_inds)
    dse.ecs.set_mesh_shader_variant(ground, "MESH_LIT")
    dse.ecs.add_rigidbody_3d(ground, 0, 0.0)
    dse.ecs.add_box_collider_3d(ground, 20.0, 1.0, 20.0)

    -- 软体网格 (8x8 grid)
    local cols, rows, spacing = 8, 8, 0.5
    local verts, inds = generate_grid_mesh(cols, rows, spacing)

    softbody_entity = dse.ecs.create_entity()
    dse.ecs.add_transform(softbody_entity, 0.0, 5.0, 0.0, 1.0, 1.0, 1.0)
    dse.ecs.add_mesh_renderer(softbody_entity, 0.2, 0.8, 0.4, 1.0, verts, inds)
    dse.ecs.set_mesh_shader_variant(softbody_entity, "MESH_LIT")

    -- 添加软体组件
    dse.ecs.add_softbody(softbody_entity, 0.6, 6, 0.98, 0.3)
    dse.ecs.softbody_set_gravity(softbody_entity, true, 1.0)

    -- 固定左上角几个顶点（作为悬挂点）
    dse.ecs.softbody_pin_vertex(softbody_entity, 0)
    dse.ecs.softbody_pin_vertex(softbody_entity, 1)
    dse.ecs.softbody_pin_vertex(softbody_entity, cols)

    print(string.format("[软体Demo] 就绪！%dx%d 软体网格悬挂模拟，WASD 移动相机", cols, rows))
end

local log_timer = 0.0

function SoftBodyDemo.Update(delta_time)
    log_timer = log_timer + delta_time

    -- 3秒后输出粒子数
    if log_timer > 3.0 and softbody_entity then
        local count = dse.ecs.softbody_get_particle_count(softbody_entity)
        if count and count > 0 then
            print(string.format("[软体Demo] 粒子数: %d", count))
            softbody_entity = nil -- 只输出一次
        end
    end
end

return SoftBodyDemo
