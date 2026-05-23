-- 3D P2 sample: scene load showcase
-- 目标：验证 dse.ecs.load_scene 从 .dscene 文件反序列化场景的能力，
--       并展示 load_scene + find_entities_by_mesh_path 的配套查询。
-- 覆盖 API: dse.ecs.load_scene, find_entities_by_mesh_path
local SceneLoad3D = {}


SceneLoad3D._meta = {
    name     = "scene load showcase",
    category = "scene",
    config   = { camera_distance=12.0,
    scene_path="" },
}

local state = {
    camera = nil,
    scene_loaded = false,
    scene_entities = {},
    fallback_objects = {},
    time = 0.0,
    logged = false
}

-- ============================================================
-- 几何数据
-- ============================================================

local function cube_vertices()
    return { -0.5,-0.5,0.5, 0.5,-0.5,0.5, 0.5,0.5,0.5, -0.5,0.5,0.5, -0.5,-0.5,-0.5, 0.5,-0.5,-0.5, 0.5,0.5,-0.5, -0.5,0.5,-0.5 }
end

local function cube_indices()
    return { 0,1,2,2,3,0, 1,5,6,6,2,1, 5,4,7,7,6,5, 4,0,3,3,7,4, 3,2,6,6,7,3, 4,5,1,1,0,4 }
end

-- ============================================================
-- 场景搭建
-- ============================================================

local function add_cube(name, x, y, z, sx, sy, sz, color, emissive)
    local e = dse.ecs.create_entity()
    dse.ecs.add_transform(e, x, y, z, sx, sy, sz)
    dse.ecs.add_mesh_renderer(e, color[1], color[2], color[3], color[4] or 1.0, cube_vertices(), cube_indices())
    dse.ecs.set_mesh_shader_variant(e, "MESH_LIT")
    dse.ecs.set_mesh_material(e, 0.0, 0.55, 1.0,
        emissive and emissive[1] or 0.0,
        emissive and emissive[2] or 0.0,
        emissive and emissive[3] or 0.0,
        1.0, true, true)
    table.insert(state.fallback_objects, { name = name, entity = e })
    return e
end

local function setup_camera(config)
    local camera = dse.ecs.create_entity()
    local distance = (type(config) == "table" and type(config.camera_distance) == "number") and config.camera_distance or 12.0
    dse.ecs.add_transform(camera, 0.0, 5.5, distance, 1.0, 1.0, 1.0)
    dse.ecs.set_transform_rotation(camera, -24.0, 0.0, 0.0)
    dse.ecs.add_camera_3d(camera, 60.0, 100)
    if dse.ecs.add_free_camera_controller then
        dse.ecs.add_free_camera_controller(camera, 5.5, 0.12)
    end
    state.camera = camera
end

local function setup_scene(config)
    -- 光照
    local light = dse.ecs.create_entity()
    dse.ecs.add_directional_light_3d(light, -0.45, -1.0, -0.30, 1.0, 0.96, 0.88, 1.15, 0.20, 0.32)

    -- 场景路径（可通过 config 覆盖）
    local scene_path = (type(config) == "table" and type(config.scene_path) == "string") and config.scene_path or ""

    -- 尝试加载场景
    if scene_path ~= "" and dse.ecs.load_scene then
        local ok, err = dse.ecs.load_scene(scene_path)
        if ok then
            state.scene_loaded = true
            print(string.format("[3D][SceneLoad] load_scene: path='%s' ok=true", scene_path))

            -- 查询加载的实体
            if dse.ecs.find_entities_by_mesh_path then
                local entities = dse.ecs.find_entities_by_mesh_path("")
                print(string.format("[3D][SceneLoad] find_entities_by_mesh_path(''): count=%d", #entities))
                for i, entity in ipairs(entities) do
                    if i <= 8 then
                        table.insert(state.scene_entities, entity)
                    end
                end
            end
        else
            print(string.format("[3D][SceneLoad] load_scene: path='%s' ok=false err='%s'", scene_path, err or "unknown"))
            print("[3D][SceneLoad] falling back to procedural scene")
        end
    else
        if scene_path == "" then
            print("[3D][SceneLoad] no scene_path configured; demonstrating load_scene API with fallback scene")
        end
        if not dse.ecs.load_scene then
            print("[3D][SceneLoad] load_scene API not available")
        end
    end

    -- 如果场景加载失败或未指定路径，搭建 fallback 场景
    if not state.scene_loaded then
        add_cube("ground", 0.0, -0.55, 0.0, 8.0, 0.12, 6.0, {0.30, 0.34, 0.38, 1.0})
        add_cube("center", 0.0, 0.3, 0.0, 1.2, 1.2, 1.2, {0.25, 0.65, 1.0, 1.0}, {0.04, 0.15, 0.35})
        add_cube("left", -2.5, 0.2, -0.5, 0.7, 0.7, 0.7, {1.0, 0.35, 0.20, 1.0})
        add_cube("right", 2.5, 0.2, -0.5, 0.7, 0.7, 0.7, {0.20, 1.0, 0.40, 1.0})
        add_cube("back", 0.0, 0.2, -2.0, 0.7, 0.7, 0.7, {1.0, 0.85, 0.18, 1.0})

        -- 演示 find_entities_by_mesh_path（在 fallback 场景中查找空路径实体）
        if dse.ecs.find_entities_by_mesh_path then
            local entities = dse.ecs.find_entities_by_mesh_path("")
            print(string.format("[3D][SceneLoad] find_entities_by_mesh_path(''): fallback_count=%d", #entities))
        end
    end

    -- API 可用性报告
    print(string.format("[3D][SceneLoad] api_summary: load_scene=%s find_entities_by_mesh_path=%s scene_loaded=%s path='%s'",
        tostring(dse.ecs.load_scene ~= nil),
        tostring(dse.ecs.find_entities_by_mesh_path ~= nil),
        tostring(state.scene_loaded),
        scene_path
    ))
end

-- ============================================================
-- 生命周期
-- ============================================================

function SceneLoad3D.Setup(config)
    print("[3D][SceneLoad] setup: load_scene + find_entities_by_mesh_path demo. Set config.scene_path to a .dscene file to test loading.")
    setup_camera(config or {})
    setup_scene(config or {})
end

function SceneLoad3D.Update(delta_time)
    local dt = delta_time or 0.0
    if dt > 0.1 then dt = 0.1 end
    state.time = state.time + dt

    -- fallback 物体动画
    for i, obj in ipairs(state.fallback_objects) do
        if i > 1 then
            dse.ecs.set_transform_rotation(obj.entity, math.sin(state.time + i) * 7.0, state.time * (14.0 + i * 5.0), 0.0)
        end
    end

    -- 查询场景实体的位置（验证场景加载后实体可查询）
    if not state.logged and state.time > 0.8 then
        state.logged = true

        if state.scene_loaded and #state.scene_entities > 0 then
            -- 报告场景实体位置
            for i, entity in ipairs(state.scene_entities) do
                if i <= 4 then
                    local x, y, z = dse.ecs.get_transform_position(entity)
                    print(string.format("[3D][SceneLoad] scene_entity[%d]: id=%s pos=(%.2f,%.2f,%.2f)", i, tostring(entity), x or 0.0, y or 0.0, z or 0.0))
                end
            end
        end

        print(string.format("[3D][SceneLoad] runtime: scene_loaded=%s fallback_objects=%d scene_entities=%d",
            tostring(state.scene_loaded), #state.fallback_objects, #state.scene_entities))
    end
end

return SceneLoad3D
