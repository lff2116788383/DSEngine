-- 3D P2 sample: metrics debug showcase
-- 目标：在 3D 场景中验证 dse.metrics.* / dse.get_memory_usage_kb / dse.app.* 性能查询 API，
--       展示如何在运行时获取 draw_calls、sprite_count、内存占用和屏幕尺寸等调试信息。
-- 覆盖 API: dse.metrics.get_draw_calls, get_max_batch_sprites, get_sprite_count,
--           dse.get_memory_usage_kb, dse.app.get_screen_width/height, time_since_startup
local MetricsDebug3D = {}

local state = {
    camera = nil,
    objects = {},
    time = 0.0,
    metrics_timer = 0.0,
    metrics_interval = 1.0,
    logged_api = false,
    logged_metrics = false,
    peak_draw_calls = 0,
    peak_memory_kb = 0
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
    table.insert(state.objects, { name = name, entity = e })
    return e
end

local function setup_camera(config)
    local camera = dse.ecs.create_entity()
    local distance = (type(config) == "table" and type(config.camera_distance) == "number") and config.camera_distance or 14.0
    dse.ecs.add_transform(camera, 0.0, 6.0, distance, 1.0, 1.0, 1.0)
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

    -- 点光源增加 draw calls
    local pl1 = dse.ecs.create_entity()
    dse.ecs.add_point_light_3d(pl1, -3.0, 2.5, 1.0, 0.35, 0.55, 1.0, 0.9, 6.0)
    local pl2 = dse.ecs.create_entity()
    dse.ecs.add_point_light_3d(pl2, 3.0, 2.5, 1.0, 1.0, 0.50, 0.20, 0.9, 6.0)

    -- 地面
    add_cube("ground", 0.0, -0.55, 0.0, 10.0, 0.12, 8.0, {0.30, 0.34, 0.38, 1.0})

    -- 多排方块（增加渲染负载以便观察 draw calls 变化）
    local rows = (type(config) == "table" and type(config.rows) == "number") and config.rows or 3
    local cols = (type(config) == "table" and type(config.cols) == "number") and config.cols or 5
    for row = 1, rows do
        for col = 1, cols do
            local x = (col - (cols + 1) * 0.5) * 1.5
            local z = (row - 1) * 1.5 - 2.0
            local r = 0.3 + col * 0.08
            local g = 0.4 + row * 0.10
            local b = 0.7 + (row + col) * 0.03
            add_cube(string.format("box_%d_%d", row, col), x, 0.3, z, 0.85, 0.85, 0.85,
                { r, g, b, 1.0 })
        end
    end

    -- emissive 参考方块
    add_cube("emissive_ref", 0.0, 0.3, 2.5, 1.0, 1.0, 1.0, {1.0, 0.50, 0.15, 1.0}, {0.60, 0.20, 0.02})

    print(string.format("[3D][MetricsDebug] setup: %d objects, 1 directional + 2 point lights", #state.objects))
end

-- ============================================================
-- 指标采集
-- ============================================================

local function collect_metrics()
    local draw_calls = 0
    local max_batch = 0
    local sprite_count = 0
    local memory_kb = 0
    local screen_w = 0
    local screen_h = 0
    local uptime = 0.0

    if dse.metrics.get_draw_calls then
        draw_calls = dse.metrics.get_draw_calls()
    end
    if dse.metrics.get_max_batch_sprites then
        max_batch = dse.metrics.get_max_batch_sprites()
    end
    if dse.metrics.get_sprite_count then
        sprite_count = dse.metrics.get_sprite_count()
    end
    if dse.get_memory_usage_kb then
        memory_kb = dse.get_memory_usage_kb()
    end
    if dse.app.get_screen_width then
        screen_w = dse.app.get_screen_width()
    end
    if dse.app.get_screen_height then
        screen_h = dse.app.get_screen_height()
    end
    if dse.app.time_since_startup then
        uptime = dse.app.time_since_startup()
    end

    -- 跟踪峰值
    if draw_calls > state.peak_draw_calls then
        state.peak_draw_calls = draw_calls
    end
    if memory_kb > state.peak_memory_kb then
        state.peak_memory_kb = memory_kb
    end

    return draw_calls, max_batch, sprite_count, memory_kb, screen_w, screen_h, uptime
end

-- ============================================================
-- 生命周期
-- ============================================================

function MetricsDebug3D.Setup(config)
    print("[3D][MetricsDebug] setup: runtime metrics collection (draw_calls, memory, screen, uptime)")
    setup_camera(config or {})
    setup_scene(config or {})

    -- API 可用性报告
    print(string.format("[3D][MetricsDebug] api_summary: get_draw_calls=%s get_max_batch_sprites=%s get_sprite_count=%s get_memory_usage_kb=%s get_screen_width=%s get_screen_height=%s time_since_startup=%s",
        tostring(dse.metrics.get_draw_calls ~= nil),
        tostring(dse.metrics.get_max_batch_sprites ~= nil),
        tostring(dse.metrics.get_sprite_count ~= nil),
        tostring(dse.get_memory_usage_kb ~= nil),
        tostring(dse.app.get_screen_width ~= nil),
        tostring(dse.app.get_screen_height ~= nil),
        tostring(dse.app.time_since_startup ~= nil)
    ))
end

function MetricsDebug3D.Update(delta_time)
    local dt = delta_time or 0.0
    if dt > 0.1 then dt = 0.1 end
    state.time = state.time + dt
    state.metrics_timer = state.metrics_timer + dt

    -- 物体缓慢旋转
    for i, obj in ipairs(state.objects) do
        if i > 1 then
            dse.ecs.set_transform_rotation(obj.entity, state.time * (8.0 + i * 2.0), state.time * (12.0 + i * 3.0), 0.0)
        end
    end

    -- 定期采集指标
    if state.metrics_timer >= state.metrics_interval then
        state.metrics_timer = 0.0
        local draw_calls, max_batch, sprite_count, memory_kb, screen_w, screen_h, uptime = collect_metrics()

        print(string.format("[3D][MetricsDebug] metrics: uptime=%.1f draw_calls=%d max_batch=%d sprite_count=%d memory_kb=%d screen=%dx%d peak_dc=%d peak_mem=%d",
            uptime, math.floor(draw_calls), math.floor(max_batch), math.floor(sprite_count), math.floor(memory_kb), math.floor(screen_w), math.floor(screen_h),
            math.floor(state.peak_draw_calls), math.floor(state.peak_memory_kb)))
    end

    -- 首次指标报告
    if not state.logged_metrics and state.time > 0.3 then
        state.logged_metrics = true
        local draw_calls, max_batch, sprite_count, memory_kb, screen_w, screen_h, uptime = collect_metrics()
        print(string.format("[3D][MetricsDebug] first_sample: draw_calls=%d max_batch=%d sprite_count=%d memory_kb=%d screen=%dx%d uptime=%.2f",
            math.floor(draw_calls), math.floor(max_batch), math.floor(sprite_count), math.floor(memory_kb), math.floor(screen_w), math.floor(screen_h), uptime))
    end
end

return MetricsDebug3D
