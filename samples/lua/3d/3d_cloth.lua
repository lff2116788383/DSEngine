-- 布料模拟 Demo —— XPBD 悬挂布料
-- 场景：一块布料顶部固定，在重力和风力下自然下垂飘动
local ClothDemo = {}

local camera_entity = nil
local light_entity = nil
local cloth_entity = nil
local ground = nil
local sphere_entity = nil

-- 生成 NxM 网格的顶点和三角形
local function generate_grid_mesh(cols, rows, spacing)
    local vertices = {}
    local indices = {}
    local uvs = {}
    local normals = {}

    for r = 0, rows - 1 do
        for c = 0, cols - 1 do
            -- 位置
            table.insert(vertices, c * spacing)
            table.insert(vertices, 0.0)
            table.insert(vertices, r * spacing)
            -- UV
            table.insert(uvs, c / (cols - 1))
            table.insert(uvs, r / (rows - 1))
            -- 法线（初始朝上）
            table.insert(normals, 0.0)
            table.insert(normals, 1.0)
            table.insert(normals, 0.0)
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

    return vertices, indices, uvs, normals
end

function ClothDemo.Setup(config)
    print("[布料Demo] 初始化场景...")

    -- 相机
    camera_entity = dse.ecs.create_entity()
    dse.ecs.add_transform(camera_entity, 3.0, 6.0, 14.0, 1.0, 1.0, 1.0)
    dse.ecs.set_transform_rotation(camera_entity, -15.0, 0.0, 0.0)
    dse.ecs.add_camera_3d(camera_entity, 60.0, 100)
    dse.ecs.add_free_camera_controller(camera_entity)

    -- 灯光
    light_entity = dse.ecs.create_entity()
    dse.ecs.add_directional_light_3d(light_entity, -0.4, -1.0, -0.5, 1.0, 0.98, 0.92, 1.2, 0.3, 0.3)

    -- 地面
    local gv = {-0.5,-0.5,0.5, 0.5,-0.5,0.5, 0.5,0.5,0.5, -0.5,0.5,0.5,
                -0.5,-0.5,-0.5, 0.5,-0.5,-0.5, 0.5,0.5,-0.5, -0.5,0.5,-0.5}
    local gi = {0,1,2,2,3,0, 1,5,6,6,2,1, 5,4,7,7,6,5, 4,0,3,3,7,4, 3,2,6,6,7,3, 4,5,1,1,0,4}
    ground = dse.ecs.create_entity()
    dse.ecs.add_transform(ground, 0.0, -0.5, 0.0, 20.0, 1.0, 20.0)
    dse.ecs.add_mesh_renderer(ground, 0.3, 0.3, 0.3, 1.0, gv, gi)
    dse.ecs.set_mesh_shader_variant(ground, "MESH_LIT")
    dse.ecs.add_rigidbody_3d(ground, 0, 0.0)
    dse.ecs.add_box_collider_3d(ground, 20.0, 1.0, 20.0)

    -- 碰撞球体
    sphere_entity = dse.ecs.create_entity()
    dse.ecs.add_transform(sphere_entity, 3.0, 3.0, 3.0, 2.0, 2.0, 2.0)
    -- 用立方体代替球体可视化（引擎没有球体 mesh 生成器）
    dse.ecs.add_mesh_renderer(sphere_entity, 0.8, 0.3, 0.3, 1.0, gv, gi)
    dse.ecs.set_mesh_shader_variant(sphere_entity, "MESH_LIT")

    -- 布料网格 (12x12)
    local cols, rows, spacing = 12, 12, 0.5
    local verts, inds, uvs, norms = generate_grid_mesh(cols, rows, spacing)

    cloth_entity = dse.ecs.create_entity()
    dse.ecs.add_transform(cloth_entity, 0.0, 8.0, 0.0, 1.0, 1.0, 1.0)
    dse.ecs.add_mesh_renderer(cloth_entity, 0.85, 0.15, 0.15, 1.0, verts, inds)
    dse.ecs.set_mesh_shader_variant(cloth_entity, "MESH_LIT")
    dse.ecs.set_mesh_material(cloth_entity, 0.0, 0.8, 1.0) -- 非金属、粗糙

    -- 添加布料组件
    dse.ecs.add_cloth(cloth_entity, 10, 0.9, 0.02, 0.3) -- iterations, stiffness, damping, bend
    dse.ecs.set_cloth_gravity(cloth_entity, 0.0, -9.81, 0.0)
    dse.ecs.set_cloth_wind(cloth_entity, 2.0, 0.0, 1.0, 0.3)

    -- 固定顶部一行
    local pinned = {}
    for c = 0, cols - 1 do
        table.insert(pinned, c)
    end
    dse.ecs.cloth_pin_vertices(cloth_entity, pinned)

    -- 添加球体碰撞
    dse.ecs.cloth_add_sphere_collider(cloth_entity, sphere_entity, 1.2)

    print("[布料Demo] 场景就绪！WASD 移动相机观察布料飘动")
end

local wind_time = 0.0

function ClothDemo.Update(delta_time)
    -- 动态风力变化
    wind_time = wind_time + delta_time
    local wx = 2.0 + math.sin(wind_time * 0.5) * 1.5
    local wz = 1.0 + math.cos(wind_time * 0.7) * 1.0
    if cloth_entity then
        dse.ecs.set_cloth_wind(cloth_entity, wx, 0.0, wz, 0.3)
    end
end

return ClothDemo
