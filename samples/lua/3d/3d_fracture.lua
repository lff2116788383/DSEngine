-- 破碎系统 Demo —— 运行时 Voronoi 碎裂展示
-- 场景：6 种材质的方块自动依次碎裂，碎片落到地面
-- 材质：冰块、石头、玻璃、木头、水晶、金属
local FractureDemo = {}

local camera_entity = nil
local light_entity = nil
local destructibles = {}   -- {entity, delay, triggered, name}
local ground = nil
local elapsed = 0.0

-- ============================================================
-- 细分立方体生成器（Voronoi 需要足够多的顶点才能正确切分）
-- subdivisions: 每条边的细分数
-- ============================================================
local function generate_subdivided_cube(subdivisions)
    local n = subdivisions or 4
    local verts = {}
    local indices = {}
    local vert_map = {} -- 去重: "x_y_z" -> index

    local function add_vert(x, y, z)
        -- 四舍五入避免浮点误差导致的重复
        local kx = math.floor(x * 10000 + 0.5)
        local ky = math.floor(y * 10000 + 0.5)
        local kz = math.floor(z * 10000 + 0.5)
        local key = kx .. "_" .. ky .. "_" .. kz
        if vert_map[key] then
            return vert_map[key]
        end
        local idx = #verts / 3
        verts[#verts + 1] = x
        verts[#verts + 1] = y
        verts[#verts + 1] = z
        vert_map[key] = idx
        return idx
    end

    -- 生成一个面的细分网格
    -- origin, u_axis, v_axis 定义面的平面
    local function add_face(ox, oy, oz, ux, uy, uz, vx, vy, vz)
        local grid = {}
        for j = 0, n do
            grid[j] = {}
            for i = 0, n do
                local t_u = i / n
                local t_v = j / n
                local px = ox + ux * t_u + vx * t_v
                local py = oy + uy * t_u + vy * t_v
                local pz = oz + uz * t_u + vz * t_v
                grid[j][i] = add_vert(px, py, pz)
            end
        end
        for j = 0, n - 1 do
            for i = 0, n - 1 do
                local a = grid[j][i]
                local b = grid[j][i + 1]
                local c = grid[j + 1][i + 1]
                local d = grid[j + 1][i]
                indices[#indices + 1] = a
                indices[#indices + 1] = b
                indices[#indices + 1] = c
                indices[#indices + 1] = c
                indices[#indices + 1] = d
                indices[#indices + 1] = a
            end
        end
    end

    local s = 0.5 -- half-size
    -- +Z face
    add_face(-s, -s,  s,  1, 0, 0,  0, 1, 0)
    -- -Z face
    add_face( s, -s, -s, -1, 0, 0,  0, 1, 0)
    -- +X face
    add_face( s, -s,  s,  0, 0,-1,  0, 1, 0)
    -- -X face
    add_face(-s, -s, -s,  0, 0, 1,  0, 1, 0)
    -- +Y face
    add_face(-s,  s,  s,  1, 0, 0,  0, 0,-1)
    -- -Y face
    add_face(-s, -s, -s,  1, 0, 0,  0, 0, 1)

    return verts, indices
end

-- 简单低面数立方体（用于地面等不需要破碎的物体）
local simple_cube_v = {
    -0.5,-0.5, 0.5,  0.5,-0.5, 0.5,  0.5, 0.5, 0.5, -0.5, 0.5, 0.5,
    -0.5,-0.5,-0.5,  0.5,-0.5,-0.5,  0.5, 0.5,-0.5, -0.5, 0.5,-0.5,
}
local simple_cube_i = {
    0,1,2, 2,3,0,  1,5,6, 6,2,1,  5,4,7, 7,6,5,
    4,0,3, 3,7,4,  3,2,6, 6,7,3,  4,5,1, 1,0,4,
}

-- 细分立方体（用于破碎对象，6 细分 ≈ 218 顶点）
local subdiv_cube_v, subdiv_cube_i = generate_subdivided_cube(6)

-- ============================================================
-- 材质预设
-- ============================================================
local materials = {
    { -- 冰块: 浅蓝半透明，碎成多片，散开较快
        name = "冰块",
        color = {0.7, 0.85, 1.0, 0.9},
        metallic = 0.1, roughness = 0.15, ao = 1.0,
        fragments = 12, explosion = 8.0,
        lifetime = 60.0, fade = 3.0, mass_scale = 0.5,
    },
    { -- 石头: 灰褐色，碎成中等块，沉重坠落
        name = "石头",
        color = {0.55, 0.5, 0.45, 1.0},
        metallic = 0.0, roughness = 0.9, ao = 1.0,
        fragments = 8, explosion = 5.0,
        lifetime = 60.0, fade = 3.0, mass_scale = 1.5,
    },
    { -- 玻璃: 浅绿半透明，碎成很多小片，飞溅
        name = "玻璃",
        color = {0.8, 0.95, 0.85, 0.7},
        metallic = 0.2, roughness = 0.05, ao = 1.0,
        fragments = 16, explosion = 10.0,
        lifetime = 60.0, fade = 3.0, mass_scale = 0.3,
    },
    { -- 木头: 暖棕色，碎成较少大块，缓慢散开
        name = "木头",
        color = {0.6, 0.4, 0.2, 1.0},
        metallic = 0.0, roughness = 0.8, ao = 0.9,
        fragments = 5, explosion = 3.0,
        lifetime = 60.0, fade = 3.0, mass_scale = 0.8,
    },
    { -- 水晶: 紫色微透明，碎成中等片，爆裂飞散
        name = "水晶",
        color = {0.7, 0.3, 0.9, 0.85},
        metallic = 0.4, roughness = 0.1, ao = 1.0,
        fragments = 10, explosion = 12.0,
        lifetime = 60.0, fade = 3.0, mass_scale = 0.6,
    },
    { -- 金属: 银灰色，碎成少量大块，质量重
        name = "金属",
        color = {0.75, 0.75, 0.78, 1.0},
        metallic = 0.95, roughness = 0.3, ao = 1.0,
        fragments = 6, explosion = 6.0,
        lifetime = 60.0, fade = 3.0, mass_scale = 2.0,
    },
}

local function create_destructible(x, y, z, mat, delay)
    local e = dse.ecs.create_entity()
    dse.ecs.add_transform(e, x, y, z, 1.8, 1.8, 1.8)
    dse.ecs.add_mesh_renderer(e, mat.color[1], mat.color[2], mat.color[3], mat.color[4],
                              subdiv_cube_v, subdiv_cube_i)
    dse.ecs.set_mesh_shader_variant(e, "MESH_LIT")
    dse.ecs.set_mesh_material(e, mat.metallic, mat.roughness, mat.ao)

    -- 运行时 Voronoi 破碎 (source=1)
    dse.ecs.add_fracture(e, 1, mat.fragments, 500.0, 100.0)
    dse.ecs.set_fracture_params(e, mat.explosion, mat.lifetime, mat.fade, mat.mass_scale)

    return {entity = e, delay = delay, triggered = false, name = mat.name}
end

function FractureDemo.Setup(config)
    print("[破碎Demo] 初始化场景...")
    print("[破碎Demo] 细分立方体: " .. (#subdiv_cube_v / 3) .. " 顶点, " .. (#subdiv_cube_i / 3) .. " 三角形")

    -- 相机（稍远，俯瞰全景）
    camera_entity = dse.ecs.create_entity()
    dse.ecs.add_transform(camera_entity, 0.0, 6.0, 18.0, 1.0, 1.0, 1.0)
    dse.ecs.set_transform_rotation(camera_entity, -12.0, 0.0, 0.0)
    dse.ecs.add_camera_3d(camera_entity, 60.0, 100)
    dse.ecs.add_free_camera_controller(camera_entity)

    -- 灯光
    light_entity = dse.ecs.create_entity()
    dse.ecs.add_directional_light_3d(light_entity, -0.5, -1.0, -0.3, 1.0, 0.95, 0.9, 1.8, 0.3, 0.35)

    -- 地面（宽大，用简单立方体 + 物理碰撞体）
    ground = dse.ecs.create_entity()
    dse.ecs.add_transform(ground, 0.0, -0.5, 0.0, 40.0, 1.0, 40.0)
    dse.ecs.add_mesh_renderer(ground, 0.3, 0.3, 0.32, 1.0, simple_cube_v, simple_cube_i)
    dse.ecs.set_mesh_shader_variant(ground, "MESH_LIT")
    dse.ecs.set_mesh_material(ground, 0.0, 0.8, 1.0)
    dse.ecs.add_box_collider_3d(ground, 40.0, 1.0, 40.0)
    dse.ecs.add_rigidbody_3d(ground, 0, 0.0) -- Static

    -- 创建 6 个方块，从左到右排列，每个延迟 2 秒依次碎裂
    for i, mat in ipairs(materials) do
        local x = (i - 3.5) * 4.5
        local y = 3.0
        local delay = 1.5 + (i - 1) * 2.0  -- 1.5s, 3.5s, 5.5s, 7.5s, 9.5s, 11.5s
        local d = create_destructible(x, y, 0.0, mat, delay)
        table.insert(destructibles, d)
        print(string.format("[破碎Demo] %s: 位置(%.1f, %.1f), %d 碎片, %.0f 爆炸力, 延迟 %.1fs",
            mat.name, x, y, mat.fragments, mat.explosion, delay))
    end

    print("[破碎Demo] 场景就绪！WASD 移动相机，方块将自动依次碎裂")
end

function FractureDemo.Update(delta_time)
    elapsed = elapsed + delta_time

    -- 自动依次触发碎裂
    for _, d in ipairs(destructibles) do
        if not d.triggered and elapsed >= d.delay then
            if not dse.ecs.fracture_is_fractured(d.entity) then
                -- 从随机方向施加冲击
                local ix = (math.random() - 0.5) * 2.0
                local iy = math.random() * 0.5
                local iz = (math.random() - 0.5) * 2.0
                dse.ecs.fracture_trigger(d.entity, ix, iy, iz)
                print(string.format("[破碎Demo] %.1fs — %s 碎裂！", elapsed, d.name))
            end
            d.triggered = true
        end
    end
end

return FractureDemo
