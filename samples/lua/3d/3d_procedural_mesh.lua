-- 3D P2 sample: procedural mesh showcase
-- 目标：展示程序化生成网格 — 球体、圆柱体、圆锥体，覆盖完整顶点属性
--       （position + UV + normal + tangent），验证 set_mesh_normals / set_mesh_tangents API。
-- 覆盖 API: set_mesh_uvs, set_mesh_normals, set_mesh_tangents, set_mesh_texture,
--           set_mesh_material_scalar, set_mesh_emissive
local ProceduralMesh3D = {}


ProceduralMesh3D._meta = {
    name     = "procedural mesh showcase",
    category = "procedural",
    config   = { camera_distance=12.0 },
}

local state = {
    camera = nil,
    meshes = {},
    time = 0.0,
    logged = false
}

-- ============================================================
-- 程序化网格生成器
-- ============================================================

--- 生成 UV 球体的顶点、索引、UV、法线和切线数据
--- @param radius number 球体半径
--- @param segments number 经线分段数
--- @param rings number 纬线分段数
--- @return table vertices, indices, uvs, normals, tangents
local function generate_sphere(radius, segments, rings)
    radius = radius or 0.5
    segments = segments or 16
    rings = rings or 12

    local vertices = {}
    local uvs = {}
    local normals = {}
    local tangents = {}

    -- 顶点：从北极到南极
    for ring = 0, rings do
        local phi = math.pi * ring / rings
        local sin_phi = math.sin(phi)
        local cos_phi = math.cos(phi)

        for seg = 0, segments do
            local theta = 2.0 * math.pi * seg / segments
            local sin_theta = math.sin(theta)
            local cos_theta = math.cos(theta)

            local x = sin_phi * cos_theta
            local y = cos_phi
            local z = sin_phi * sin_theta

            table.insert(vertices, x * radius)
            table.insert(vertices, y * radius)
            table.insert(vertices, z * radius)

            -- UV：u 沿经线 [0,1]，v 沿纬线 [0,1]（北极=1, 南极=0）
            table.insert(uvs, seg / segments)
            table.insert(uvs, 1.0 - ring / rings)

            -- 法线 = 归一化位置（单位球面上与位置同向）
            table.insert(normals, x)
            table.insert(normals, y)
            table.insert(normals, z)

            -- 切线：沿 theta 增大方向（经线方向），与法线正交
            table.insert(tangents, -sin_theta)
            table.insert(tangents, 0.0)
            table.insert(tangents, cos_theta)
        end
    end

    -- 索引
    local indices = {}
    for ring = 0, rings - 1 do
        for seg = 0, segments - 1 do
            local current = ring * (segments + 1) + seg
            local next = current + segments + 1

            table.insert(indices, current)
            table.insert(indices, next)
            table.insert(indices, current + 1)

            table.insert(indices, current + 1)
            table.insert(indices, next)
            table.insert(indices, next + 1)
        end
    end

    return vertices, indices, uvs, normals, tangents
end

--- 生成圆柱体的顶点、索引、UV、法线和切线数据
--- @param radius number 底面半径
--- @param height number 高度
--- @param segments number 圆周分段数
--- @return table vertices, indices, uvs, normals, tangents
local function generate_cylinder(radius, height, segments)
    radius = radius or 0.4
    height = height or 1.0
    segments = segments or 16

    local vertices = {}
    local uvs = {}
    local normals = {}
    local tangents = {}
    local half_h = height * 0.5

    -- 侧面顶点（两层：顶面边 + 底面边）
    for row = 0, 1 do
        local y = (row == 0) and -half_h or half_h
        for seg = 0, segments do
            local theta = 2.0 * math.pi * seg / segments
            local cos_t = math.cos(theta)
            local sin_t = math.sin(theta)

            table.insert(vertices, cos_t * radius)
            table.insert(vertices, y)
            table.insert(vertices, sin_t * radius)

            table.insert(uvs, seg / segments)
            table.insert(uvs, row * 1.0)

            table.insert(normals, cos_t)
            table.insert(normals, 0.0)
            table.insert(normals, sin_t)

            table.insert(tangents, -sin_t)
            table.insert(tangents, 0.0)
            table.insert(tangents, cos_t)
        end
    end

    -- 侧面索引
    local indices = {}
    for seg = 0, segments - 1 do
        local bottom = seg
        local top = seg + segments + 1
        table.insert(indices, bottom)
        table.insert(indices, top)
        table.insert(indices, bottom + 1)
        table.insert(indices, bottom + 1)
        table.insert(indices, top)
        table.insert(indices, top + 1)
    end

    -- 顶盖中心点
    local top_center_idx = #vertices / 3
    table.insert(vertices, 0.0)
    table.insert(vertices, half_h)
    table.insert(vertices, 0.0)
    table.insert(uvs, 0.5)
    table.insert(uvs, 0.5)
    table.insert(normals, 0.0)
    table.insert(normals, 1.0)
    table.insert(normals, 0.0)
    table.insert(tangents, 1.0)
    table.insert(tangents, 0.0)
    table.insert(tangents, 0.0)

    -- 顶盖边顶点
    local top_rim_start = top_center_idx + 1
    for seg = 0, segments do
        local theta = 2.0 * math.pi * seg / segments
        table.insert(vertices, math.cos(theta) * radius)
        table.insert(vertices, half_h)
        table.insert(vertices, math.sin(theta) * radius)
        table.insert(uvs, 0.5 + math.cos(theta) * 0.5)
        table.insert(uvs, 0.5 + math.sin(theta) * 0.5)
        table.insert(normals, 0.0)
        table.insert(normals, 1.0)
        table.insert(normals, 0.0)
        table.insert(tangents, 1.0)
        table.insert(tangents, 0.0)
        table.insert(tangents, 0.0)
    end

    for seg = 0, segments - 1 do
        table.insert(indices, top_center_idx)
        table.insert(indices, top_rim_start + seg + 1)
        table.insert(indices, top_rim_start + seg)
    end

    -- 底盖中心点
    local bottom_center_idx = #vertices / 3
    table.insert(vertices, 0.0)
    table.insert(vertices, -half_h)
    table.insert(vertices, 0.0)
    table.insert(uvs, 0.5)
    table.insert(uvs, 0.5)
    table.insert(normals, 0.0)
    table.insert(normals, -1.0)
    table.insert(normals, 0.0)
    table.insert(tangents, 1.0)
    table.insert(tangents, 0.0)
    table.insert(tangents, 0.0)

    -- 底盖边顶点
    local bottom_rim_start = bottom_center_idx + 1
    for seg = 0, segments do
        local theta = 2.0 * math.pi * seg / segments
        table.insert(vertices, math.cos(theta) * radius)
        table.insert(vertices, -half_h)
        table.insert(vertices, math.sin(theta) * radius)
        table.insert(uvs, 0.5 + math.cos(theta) * 0.5)
        table.insert(uvs, 0.5 - math.sin(theta) * 0.5)
        table.insert(normals, 0.0)
        table.insert(normals, -1.0)
        table.insert(normals, 0.0)
        table.insert(tangents, 1.0)
        table.insert(tangents, 0.0)
        table.insert(tangents, 0.0)
    end

    for seg = 0, segments - 1 do
        table.insert(indices, bottom_center_idx)
        table.insert(indices, bottom_rim_start + seg)
        table.insert(indices, bottom_rim_start + seg + 1)
    end

    return vertices, indices, uvs, normals, tangents
end

--- 生成圆锥体的顶点、索引、UV、法线和切线数据
--- @param radius number 底面半径
--- @param height number 高度
--- @param segments number 圆周分段数
--- @return table vertices, indices, uvs, normals, tangents
local function generate_cone(radius, height, segments)
    radius = radius or 0.4
    height = height or 1.0
    segments = segments or 16

    local vertices = {}
    local uvs = {}
    local normals = {}
    local tangents = {}
    local half_h = height * 0.5

    -- 侧面顶点（底边 + 顶点）
    -- 底边
    for seg = 0, segments do
        local theta = 2.0 * math.pi * seg / segments
        local cos_t = math.cos(theta)
        local sin_t = math.sin(theta)
        local slope_len = math.sqrt(radius * radius + height * height)
        local nx = cos_t * height / slope_len
        local nz = sin_t * height / slope_len
        local ny = radius / slope_len

        table.insert(vertices, cos_t * radius)
        table.insert(vertices, -half_h)
        table.insert(vertices, sin_t * radius)
        table.insert(uvs, seg / segments)
        table.insert(uvs, 0.0)
        table.insert(normals, nx)
        table.insert(normals, ny)
        table.insert(normals, nz)
        table.insert(tangents, -sin_t)
        table.insert(tangents, 0.0)
        table.insert(tangents, cos_t)
    end

    -- 侧面顶点（尖端，每个 seg 一份以获得平滑法线）
    local tip_start = #vertices / 3
    for seg = 0, segments do
        local theta = 2.0 * math.pi * seg / segments
        local cos_t = math.cos(theta)
        local sin_t = math.sin(theta)
        local slope_len = math.sqrt(radius * radius + height * height)
        local nx = cos_t * height / slope_len
        local nz = sin_t * height / slope_len
        local ny = radius / slope_len

        table.insert(vertices, 0.0)
        table.insert(vertices, half_h)
        table.insert(vertices, 0.0)
        table.insert(uvs, (seg + 0.5) / segments)
        table.insert(uvs, 1.0)
        table.insert(normals, nx)
        table.insert(normals, ny)
        table.insert(normals, nz)
        table.insert(tangents, -sin_t)
        table.insert(tangents, 0.0)
        table.insert(tangents, cos_t)
    end

    -- 侧面索引
    local indices = {}
    for seg = 0, segments - 1 do
        local bottom = seg
        local top = tip_start + seg
        table.insert(indices, bottom)
        table.insert(indices, top)
        table.insert(indices, bottom + 1)
    end

    -- 底盖
    local bottom_center_idx = #vertices / 3
    table.insert(vertices, 0.0)
    table.insert(vertices, -half_h)
    table.insert(vertices, 0.0)
    table.insert(uvs, 0.5)
    table.insert(uvs, 0.5)
    table.insert(normals, 0.0)
    table.insert(normals, -1.0)
    table.insert(normals, 0.0)
    table.insert(tangents, 1.0)
    table.insert(tangents, 0.0)
    table.insert(tangents, 0.0)

    local bottom_rim_start = bottom_center_idx + 1
    for seg = 0, segments do
        local theta = 2.0 * math.pi * seg / segments
        table.insert(vertices, math.cos(theta) * radius)
        table.insert(vertices, -half_h)
        table.insert(vertices, math.sin(theta) * radius)
        table.insert(uvs, 0.5 + math.cos(theta) * 0.5)
        table.insert(uvs, 0.5 - math.sin(theta) * 0.5)
        table.insert(normals, 0.0)
        table.insert(normals, -1.0)
        table.insert(normals, 0.0)
        table.insert(tangents, 1.0)
        table.insert(tangents, 0.0)
        table.insert(tangents, 0.0)
    end

    for seg = 0, segments - 1 do
        table.insert(indices, bottom_center_idx)
        table.insert(indices, bottom_rim_start + seg)
        table.insert(indices, bottom_rim_start + seg + 1)
    end

    return vertices, indices, uvs, normals, tangents
end

-- ============================================================
-- 场景搭建
-- ============================================================

local function cube_vertices()
    return { -0.5,-0.5,0.5, 0.5,-0.5,0.5, 0.5,0.5,0.5, -0.5,0.5,0.5, -0.5,-0.5,-0.5, 0.5,-0.5,-0.5, 0.5,0.5,-0.5, -0.5,0.5,-0.5 }
end

local function cube_indices()
    return { 0,1,2,2,3,0, 1,5,6,6,2,1, 5,4,7,7,6,5, 4,0,3,3,7,4, 3,2,6,6,7,3, 4,5,1,1,0,4 }
end

local function create_procedural_mesh(name, x, y, z, verts, idx, uv_data, normal_data, tangent_data, color, metallic, roughness, emissive)
    local e = dse.ecs.create_entity()
    dse.ecs.add_transform(e, x, y, z, 1.0, 1.0, 1.0)
    dse.ecs.add_mesh_renderer(e, color[1], color[2], color[3], color[4] or 1.0, verts, idx)
    dse.ecs.set_mesh_shader_variant(e, "MESH_LIT")
    dse.ecs.set_mesh_material(e, metallic or 0.0, roughness or 0.55, 1.0,
        emissive and emissive[1] or 0.0,
        emissive and emissive[2] or 0.0,
        emissive and emissive[3] or 0.0,
        1.0, true, true)

    -- 设置 UV
    local uv_ok, uv_count, uv_vertices = false, 0, 0
    if dse.ecs.set_mesh_uvs then
        uv_ok, uv_count, uv_vertices = dse.ecs.set_mesh_uvs(e, uv_data)
    end

    -- 设置法线
    local normals_ok, normal_count = false, 0
    if dse.ecs.set_mesh_normals then
        normals_ok, normal_count = dse.ecs.set_mesh_normals(e, normal_data)
    end

    -- 设置切线
    local tangents_ok, tangent_count = false, 0
    if dse.ecs.set_mesh_tangents then
        tangents_ok, tangent_count = dse.ecs.set_mesh_tangents(e, tangent_data)
    end

    table.insert(state.meshes, {
        name = name, entity = e,
        uv_ok = uv_ok, normals_ok = normals_ok, tangents_ok = tangents_ok,
        vertex_count = #verts / 3, index_count = #idx
    })

    return e
end

local function setup_camera(config)
    local camera = dse.ecs.create_entity()
    local distance = (type(config) == "table" and type(config.camera_distance) == "number") and config.camera_distance or 12.0
    dse.ecs.add_transform(camera, 0.0, 4.5, distance, 1.0, 1.0, 1.0)
    dse.ecs.set_transform_rotation(camera, -22.0, 0.0, 0.0)
    dse.ecs.add_camera_3d(camera, 60.0, 100)
    if dse.ecs.add_free_camera_controller then
        dse.ecs.add_free_camera_controller(camera, 5.5, 0.12)
    end
    state.camera = camera
end

local function setup_scene(config)
    -- 光照
    local light = dse.ecs.create_entity()
    dse.ecs.add_directional_light_3d(light, -0.45, -1.0, -0.30, 1.0, 0.96, 0.88, 1.20, 0.18, 0.30)

    -- 补光
    local fill = dse.ecs.create_entity()
    dse.ecs.add_point_light_3d(fill, 0.0, 3.5, 2.5, 0.30, 0.55, 1.0, 1.0, 8.0)

    -- 地面
    local ground = dse.ecs.create_entity()
    dse.ecs.add_transform(ground, 0.0, -0.55, 0.0, 10.0, 0.12, 6.0)
    dse.ecs.add_mesh_renderer(ground, 0.30, 0.34, 0.38, 1.0, cube_vertices(), cube_indices())
    dse.ecs.set_mesh_shader_variant(ground, "MESH_LIT")
    dse.ecs.set_mesh_material(ground, 0.0, 0.85, 1.0, 0.0, 0.0, 0.0, 1.0, true, true)

    -- === 程序化球体 ===
    local s_verts, s_idx, s_uvs, s_normals, s_tangents = generate_sphere(0.6, 20, 14)
    create_procedural_mesh("sphere", -3.5, 0.6, 0.0,
        s_verts, s_idx, s_uvs, s_normals, s_tangents,
        {0.90, 0.92, 0.96, 1.0}, 0.85, 0.15, {0.05, 0.05, 0.06})

    -- === 程序化圆柱体 ===
    local c_verts, c_idx, c_uvs, c_normals, c_tangents = generate_cylinder(0.4, 1.2, 20)
    create_procedural_mesh("cylinder", 0.0, 0.6, 0.0,
        c_verts, c_idx, c_uvs, c_normals, c_tangents,
        {0.25, 0.70, 0.95, 1.0}, 0.05, 0.45)

    -- === 程序化圆锥体 ===
    local co_verts, co_idx, co_uvs, co_normals, co_tangents = generate_cone(0.45, 1.2, 20)
    create_procedural_mesh("cone", 3.5, 0.6, 0.0,
        co_verts, co_idx, co_uvs, co_normals, co_tangents,
        {1.0, 0.45, 0.15, 1.0}, 0.0, 0.55, {0.15, 0.05, 0.0})

    -- === 标签标记 ===
    local labels = {
        { "sphere_marker", -3.5, -0.46, -1.2, {0.90, 0.92, 0.96} },
        { "cylinder_marker", 0.0, -0.46, -1.2, {0.25, 0.70, 0.95} },
        { "cone_marker", 3.5, -0.46, -1.2, {1.0, 0.45, 0.15} },
    }
    for _, lbl in ipairs(labels) do
        local e = dse.ecs.create_entity()
        dse.ecs.add_transform(e, lbl[2], lbl[3], lbl[4], 0.85, 0.04, 0.35)
        dse.ecs.add_mesh_renderer(e, lbl[5][1], lbl[5][2], lbl[5][3], 1.0, cube_vertices(), cube_indices())
        dse.ecs.set_mesh_shader_variant(e, "MESH_LIT")
        dse.ecs.set_mesh_material(e, 0.0, 0.50, 1.0, lbl[5][1] * 0.08, lbl[5][2] * 0.08, lbl[5][3] * 0.08, 1.0, true, true)
    end

    -- 输出网格生成统计
    for _, mesh in ipairs(state.meshes) do
        print(string.format("[3D][ProceduralMesh] %s: vertices=%d indices=%d uv=%s(%s/%s) normals=%s(%s) tangents=%s(%s)",
            mesh.name, mesh.vertex_count, mesh.index_count,
            tostring(mesh.uv_ok), "ok", tostring(mesh.vertex_count),
            tostring(mesh.normals_ok), tostring(mesh.normal_count),
            tostring(mesh.tangents_ok), tostring(mesh.tangent_count)))
    end
end

-- ============================================================
-- 生命周期
-- ============================================================

function ProceduralMesh3D.Setup(config)
    print("[3D][ProceduralMesh] setup: procedural sphere + cylinder + cone with full vertex attributes (UV/normal/tangent)")
    setup_camera(config or {})
    setup_scene(config or {})
end

function ProceduralMesh3D.Update(delta_time)
    local dt = delta_time or 0.0
    if dt > 0.1 then dt = 0.1 end
    state.time = state.time + dt

    -- 旋转和浮动动画
    for i, mesh in ipairs(state.meshes) do
        local y_offset = math.sin(state.time * 1.2 + i * 1.5) * 0.12
        local base_y = 0.6 + y_offset
        dse.ecs.set_transform_rotation(mesh.entity, state.time * (15.0 + i * 8.0), state.time * (25.0 + i * 5.0), 0.0)
        local px, py, pz = dse.ecs.get_transform_position(mesh.entity)
        if px then
            dse.ecs.set_transform_position(mesh.entity, px, base_y, pz)
        end
    end

    -- 球体材质动画：metallic 闪烁
    if #state.meshes >= 1 and state.meshes[1].entity then
        local sphere = state.meshes[1].entity
        local m = 0.5 + math.sin(state.time * 2.0) * 0.35
        if dse.ecs.set_mesh_material_scalar then
            dse.ecs.set_mesh_material_scalar(sphere, "metallic", m)
        end
    end

    -- 圆柱体 roughness 动画
    if #state.meshes >= 2 and state.meshes[2].entity then
        local cyl = state.meshes[2].entity
        local r = 0.25 + math.sin(state.time * 1.5) * 0.20
        if dse.ecs.set_mesh_material_scalar then
            dse.ecs.set_mesh_material_scalar(cyl, "roughness", r)
        end
    end

    -- 圆锥体 emissive 动画
    if #state.meshes >= 3 and state.meshes[3].entity then
        local cone = state.meshes[3].entity
        local e_val = 0.3 + math.sin(state.time * 2.5) * 0.25
        if dse.ecs.set_mesh_emissive then
            dse.ecs.set_mesh_emissive(cone, e_val, e_val * 0.3, 0.0)
        end
    end

    -- 延迟报告
    if not state.logged and state.time > 0.5 then
        state.logged = true
        local has_normals = dse.ecs.set_mesh_normals ~= nil
        local has_tangents = dse.ecs.set_mesh_tangents ~= nil
        local has_uvs = dse.ecs.set_mesh_uvs ~= nil
        print(string.format("[3D][ProceduralMesh] runtime: mesh_authoring_api set_mesh_uvs=%s set_mesh_normals=%s set_mesh_tangents=%s",
            tostring(has_uvs), tostring(has_normals), tostring(has_tangents)))
    end
end

return ProceduralMesh3D
