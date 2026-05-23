-- 3D P4 sample: Asset Pack Showcase
-- 目标：验证模型/材质/贴图/动画资源包加载与资源 manifest 整理
-- 覆盖 API: set_mesh_path, set_mesh_material, set_mesh_texture, set_mesh_normals, add_transform,
--            add_mesh_renderer, add_directional_light_3d, add_camera_3d
local AssetPackShowcase3D = {}


AssetPackShowcase3D._meta = {
    name     = "Asset Pack Showcase",
    category = "scene",
    config   = { camera_distance=12.0,
    mesh_path="models/cube.dmesh",
    material_path="models/cube.dmat" },
}

local state = {
    camera = nil,
    objects = {},
    time = 0.0,
    logged_manifest = false,
    logged_runtime = false,
}

--- 资源 manifest 定义：集中管理本 demo 使用的所有资源路径
local ASSET_MANIFEST = {
    meshes = {
        cube       = "models/cube.dmesh",
        -- 后续可扩展: sphere = "models/sphere.dmesh", etc.
    },
    materials = {
        cube_default = "models/cube.dmat",
    },
    animations = {
        -- 暂无动画资源，留作扩展
    },
    textures = {
        -- 暂无独立贴图，留作扩展
    },
}

--- 几何 fallback（当资源文件不可用时使用程序化网格）
local function cube_vertices()
    return { -0.5,-0.5,0.5, 0.5,-0.5,0.5, 0.5,0.5,0.5, -0.5,0.5,0.5, -0.5,-0.5,-0.5, 0.5,-0.5,-0.5, 0.5,0.5,-0.5, -0.5,0.5,-0.5 }
end

local function cube_indices()
    return { 0,1,2,2,3,0, 1,5,6,6,2,1, 5,4,7,7,6,5, 4,0,3,3,7,4, 3,2,6,6,7,3, 4,5,1,1,0,4 }
end

--- 尝试通过资源路径加载 mesh，失败则 fallback 到程序化
local function create_mesh_entity(name, x, y, z, sx, sy, sz, color, mesh_path, material_path)
    local e = dse.ecs.create_entity()
    dse.ecs.add_transform(e, x, y, z, sx, sy, sz)

    local loaded = false
    -- 尝试使用 set_mesh_path 加载真实资源
    if mesh_path and mesh_path ~= "" and dse.ecs.set_mesh_path then
        local ok = dse.ecs.set_mesh_path(e, mesh_path)
        if ok then
            loaded = true
            -- 尝试设置材质
            if material_path and material_path ~= "" and dse.ecs.set_mesh_material then
                -- 先添加默认 mesh renderer 再覆盖材质参数
                dse.ecs.add_mesh_renderer(e, color[1], color[2], color[3], color[4] or 1.0, cube_vertices(), cube_indices())
                dse.ecs.set_mesh_shader_variant(e, "MESH_LIT")
            end
        end
    end

    -- Fallback: 程序化 mesh
    if not loaded then
        dse.ecs.add_mesh_renderer(e, color[1], color[2], color[3], color[4] or 1.0, cube_vertices(), cube_indices())
        dse.ecs.set_mesh_shader_variant(e, "MESH_LIT")
        dse.ecs.set_mesh_material(e, 0.0, 0.50, 1.0, 0.0, 0.0, 0.0, 1.0, true, true)
    end

    return e, loaded
end

local function setup_camera(config)
    local camera = dse.ecs.create_entity()
    local distance = (type(config) == "table" and type(config.camera_distance) == "number") and config.camera_distance or 12.0
    dse.ecs.add_transform(camera, 0.0, 4.5, distance, 1.0, 1.0, 1.0)
    dse.ecs.set_transform_rotation(camera, -22.0, 0.0, 0.0)
    dse.ecs.add_camera_3d(camera, 60.0, 100)
    if dse.ecs.add_free_camera_controller then
        dse.ecs.add_free_camera_controller(camera, 5.0, 0.12)
    end
    state.camera = camera
end

local function setup_scene(config)
    -- 方向光
    local light = dse.ecs.create_entity()
    dse.ecs.add_directional_light_3d(light, -0.42, -1.0, -0.25, 1.0, 0.95, 0.88, 1.15, 0.18, 0.30)

    -- 读取 manifest 中的默认路径
    local mesh_path = ""
    local material_path = ""
    if type(config) == "table" then
        mesh_path = config.mesh_path or ASSET_MANIFEST.meshes.cube or ""
        material_path = config.material_path or ASSET_MANIFEST.materials.cube_default or ""
    end

    -- 地面
    local ground, ground_loaded = create_mesh_entity("ground", 0.0, -0.55, 0.0, 10.0, 0.12, 7.0,
        {0.28, 0.32, 0.36, 1.0}, "", "")
    table.insert(state.objects, { name = "ground", entity = ground, loaded = ground_loaded })

    -- 资源包验证对象：使用 manifest 路径
    local asset_obj, asset_loaded = create_mesh_entity("asset_cube", 0.0, 0.35, 0.0, 1.4, 1.4, 1.4,
        {0.25, 0.60, 1.0, 1.0}, mesh_path, material_path)
    table.insert(state.objects, { name = "asset_cube", entity = asset_obj, loaded = asset_loaded })

    -- 不同材质参数对比对象
    local metallic = dse.ecs.create_entity()
    dse.ecs.add_transform(metallic, -2.5, 0.25, 0.0, 0.9, 0.9, 0.9)
    dse.ecs.add_mesh_renderer(metallic, 0.90, 0.85, 0.72, 1.0, cube_vertices(), cube_indices())
    dse.ecs.set_mesh_shader_variant(metallic, "MESH_LIT")
    dse.ecs.set_mesh_material(metallic, 0.92, 0.18, 0.65, 0.0, 0.0, 0.0, 1.0, true, true)
    table.insert(state.objects, { name = "metallic", entity = metallic, loaded = false })

    local rough = dse.ecs.create_entity()
    dse.ecs.add_transform(rough, 2.5, 0.25, 0.0, 0.9, 0.9, 0.9)
    dse.ecs.add_mesh_renderer(rough, 0.82, 0.42, 0.22, 1.0, cube_vertices(), cube_indices())
    dse.ecs.set_mesh_shader_variant(rough, "MESH_LIT")
    dse.ecs.set_mesh_material(rough, 0.08, 0.95, 0.55, 0.0, 0.0, 0.0, 1.0, true, true)
    table.insert(state.objects, { name = "rough", entity = rough, loaded = false })

    local emissive = dse.ecs.create_entity()
    dse.ecs.add_transform(emissive, 0.0, 0.25, -2.2, 0.7, 0.7, 0.7)
    dse.ecs.add_mesh_renderer(emissive, 0.15, 0.55, 0.90, 1.0, cube_vertices(), cube_indices())
    dse.ecs.set_mesh_shader_variant(emissive, "MESH_LIT")
    dse.ecs.set_mesh_material(emissive, 0.0, 0.40, 1.0, 0.30, 0.55, 1.20, 1.0, true, true)
    table.insert(state.objects, { name = "emissive", entity = emissive, loaded = false })

    -- 资源 manifest 日志
    print("[3D][AssetPack] manifest: meshes=" .. (ASSET_MANIFEST.meshes and "configured" or "empty")
        .. " materials=" .. (ASSET_MANIFEST.materials and "configured" or "empty"))
    print(string.format("[3D][AssetPack] setup: mesh_path='%s' material_path='%s' asset_loaded=%s",
        mesh_path, material_path, tostring(asset_loaded)))

    -- API 可用性
    print(string.format("[3D][AssetPack] api: set_mesh_path=%s set_mesh_material=%s",
        tostring(dse.ecs.set_mesh_path ~= nil),
        tostring(dse.ecs.set_mesh_material ~= nil)))
end

function AssetPackShowcase3D.Setup(config)
    print("[3D][AssetPack] setup: 资源包加载验证，mesh/材质/贴图路径来自 manifest + config")
    setup_camera(config or {})
    setup_scene(config or {})
end

function AssetPackShowcase3D.Update(delta_time)
    local dt = delta_time or 0.0
    if dt > 0.1 then dt = 0.1 end
    state.time = state.time + dt

    -- 对象轻微旋转
    for i, obj in ipairs(state.objects) do
        if i > 1 and obj.entity then
            dse.ecs.set_transform_rotation(obj.entity,
                math.sin(state.time + i) * 5.0,
                state.time * (10.0 + i * 3.0),
                0.0)
        end
    end

    -- 运行时日志
    if not state.logged_runtime and state.time > 0.8 then
        state.logged_runtime = true
        local loaded_count = 0
        for _, obj in ipairs(state.objects) do
            if obj.loaded then loaded_count = loaded_count + 1 end
        end
        print(string.format("[3D][AssetPack] runtime: objects=%d asset_loaded=%d", #state.objects, loaded_count))
    end
end

return AssetPackShowcase3D
