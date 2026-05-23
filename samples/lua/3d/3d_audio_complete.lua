-- 3D P3 sample: audio complete showcase
-- 目标：展示 dse.audio 完整 API — 包括 3D 空间音频和完整播放控制
--       (add_listener, add_source, set_3d_mode, set_3d_distance, set_playing, set_volume,
--        set_pitch, set_loop, restart, get_source_state)。
--       相比 3d_audio_spatial.lua，本范例额外覆盖 set_loop / restart，
--       并提供多音源 + 3D 距离衰减 + 循环控制的完整组合。
-- 覆盖 API: dse.audio.add_listener, add_source, set_3d_mode, set_3d_distance,
--           set_playing, set_volume, set_pitch, set_loop, restart, get_source_state
local AudioComplete3D = {}


AudioComplete3D._meta = {
    name     = "audio complete showcase",
    category = "audio",
    config   = { camera_distance=12.0,
    audio_path_a="audio/spatial/spatial_ping.wav",
    audio_path_b="",
    audio_path_c="" },
}

local state = {
    camera = nil,
    listener = nil,
    sources = {},
    markers = {},
    rings = {},
    time = 0.0,
    logged_api = false,
    logged_runtime = false,
    loop_toggled = false,
    restart_triggered = false
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

    -- 地面
    add_cube("ground", 0.0, -0.55, 0.0, 10.0, 0.12, 8.0, {0.30, 0.34, 0.38, 1.0})

    -- Listener 标记（中央蓝色柱体）
    state.listener = add_cube("listener", 0.0, 0.55, 0.0, 0.40, 1.10, 0.40, {0.22, 0.62, 1.0, 1.0}, {0.04, 0.15, 0.35})

    -- 音频源配置：3 个空间音源，各有不同路径/距离/颜色
    local source_configs = {
        { name = "source_orbit",   audio_path = config.audio_path_a or "", color = {1.0, 0.50, 0.18}, emissive = {0.45, 0.12, 0.02}, radius = 3.0, speed = 0.7,  min_dist = 1.0, max_dist = 6.0, rolloff = 1.1 },
        { name = "source_far",     audio_path = config.audio_path_b or "", color = {0.25, 1.0, 0.48},  emissive = {0.04, 0.30, 0.08}, radius = 4.5, speed = 0.45, min_dist = 1.5, max_dist = 8.0, rolloff = 1.3 },
        { name = "source_close",   audio_path = config.audio_path_c or "", color = {0.72, 0.35, 1.0},  emissive = {0.18, 0.05, 0.35}, radius = 1.8, speed = 1.1,  min_dist = 0.5, max_dist = 4.0, rolloff = 0.9 },
    }

    for i, sc in ipairs(source_configs) do
        -- 音源标记方块
        local marker = add_cube(sc.name .. "_marker", sc.radius, 0.45, 0.0, 0.42, 0.42, 0.42, sc.color, sc.emissive)
        table.insert(state.markers, { entity = marker, radius = sc.radius, speed = sc.speed, phase = i * 1.2 })

        -- 音源实体（挂载音频组件）
        local source = dse.ecs.create_entity()
        dse.ecs.add_transform(source, sc.radius, 0.45, 0.0, 1.0, 1.0, 1.0)
        table.insert(state.sources, { entity = source, config = sc, audio_enabled = false })

        -- 添加音频源
        if sc.audio_path ~= "" and dse.audio.add_source then
            dse.audio.add_source(source, sc.audio_path, true, false, 0.65)
            state.sources[#state.sources].audio_enabled = true
        end

        -- 3D 空间模式
        if dse.audio.set_3d_mode then
            dse.audio.set_3d_mode(source, true)
        end

        -- 距离衰减
        if dse.audio.set_3d_distance then
            dse.audio.set_3d_distance(source, sc.min_dist, sc.max_dist, sc.rolloff)
        end

        -- 距离环标记
        local ring_count = 12
        for r = 1, ring_count do
            local angle = (r / ring_count) * math.pi * 2.0
            local ring = add_cube(sc.name .. "_ring_" .. r,
                math.cos(angle) * sc.radius, -0.46, math.sin(angle) * sc.radius,
                0.10, 0.04, 0.10,
                { sc.color[1] * 0.6, sc.color[2] * 0.6, sc.color[3] * 0.6, 1.0 })
            table.insert(state.rings, { entity = ring, angle = angle, radius = sc.radius, base_y = -0.46, speed = sc.speed })
        end
    end

    -- Listener 挂载
    if dse.audio.add_listener then
        dse.audio.add_listener(state.listener)
    end

    -- 开始播放所有音源
    for _, src in ipairs(state.sources) do
        if src.audio_enabled and dse.audio.set_playing then
            dse.audio.set_playing(src.entity, true)
        end
    end

    -- API 可用性报告
    print(string.format("[3D][AudioComplete] api_summary: add_listener=%s add_source=%s set_3d_mode=%s set_3d_distance=%s set_playing=%s set_volume=%s set_pitch=%s set_loop=%s restart=%s get_source_state=%s",
        tostring(dse.audio.add_listener ~= nil),
        tostring(dse.audio.add_source ~= nil),
        tostring(dse.audio.set_3d_mode ~= nil),
        tostring(dse.audio.set_3d_distance ~= nil),
        tostring(dse.audio.set_playing ~= nil),
        tostring(dse.audio.set_volume ~= nil),
        tostring(dse.audio.set_pitch ~= nil),
        tostring(dse.audio.set_loop ~= nil),
        tostring(dse.audio.restart ~= nil),
        tostring(dse.audio.get_source_state ~= nil)
    ))
end

-- ============================================================
-- 生命周期
-- ============================================================

function AudioComplete3D.Setup(config)
    print("[3D][AudioComplete] setup: 3 spatial audio sources with loop/restart control. Config keys: audio_path_a/b/c")
    setup_camera(config or {})
    setup_scene(config or {})
end

function AudioComplete3D.Update(delta_time)
    local dt = delta_time or 0.0
    if dt > 0.1 then dt = 0.1 end
    state.time = state.time + dt

    -- 更新音源位置和标记
    for i, src in ipairs(state.sources) do
        local sc = src.config
        local x = math.cos(state.time * sc.speed + i * 2.1) * sc.radius
        local z = math.sin(state.time * sc.speed + i * 2.1) * sc.radius
        dse.ecs.set_transform_position(src.entity, x, 0.45, z)

        -- 同步标记位置
        if state.markers[i] then
            dse.ecs.set_transform_position(state.markers[i].entity, x, 0.45, z)
            dse.ecs.set_transform_rotation(state.markers[i].entity, state.time * 75.0, state.time * 120.0, 0.0)
        end

        -- 动态音量/变调（距离衰减模拟）
        if src.audio_enabled then
            local distance = math.sqrt(x * x + z * z)
            local volume = math.max(0.08, math.min(0.85, 1.0 - distance / sc.max_dist))
            local pitch = 0.85 + math.sin(state.time * sc.speed) * 0.15
            if dse.audio.set_volume then
                dse.audio.set_volume(src.entity, volume)
            end
            if dse.audio.set_pitch then
                dse.audio.set_pitch(src.entity, pitch)
            end
        end
    end

    -- 距离环脉冲
    for i, ring in ipairs(state.rings) do
        local pulse = 1.0 + math.sin(state.time * 2.5 - i * 0.3) * 0.30
        dse.ecs.set_transform_position(ring.entity,
            math.cos(ring.angle) * ring.radius, ring.base_y,
            math.sin(ring.angle) * ring.radius)
    end

    -- 阶段 1：2.0s 后设置循环播放
    if state.time > 2.0 and not state.loop_toggled then
        state.loop_toggled = true
        for _, src in ipairs(state.sources) do
            if src.audio_enabled and dse.audio.set_loop then
                dse.audio.set_loop(src.entity, true)
                print(string.format("[3D][AudioComplete] phase1: set_loop(true) on source %s", src.config.name))
            end
        end
    end

    -- 阶段 2：4.0s 后重启第一个音源
    if state.time > 4.0 and not state.restart_triggered then
        state.restart_triggered = true
        if #state.sources >= 1 and state.sources[1].audio_enabled and dse.audio.restart then
            dse.audio.restart(state.sources[1].entity)
            print(string.format("[3D][AudioComplete] phase2: restart() on source %s", state.sources[1].config.name))
        end
    end

    -- 延迟运行时报告
    if not state.logged_runtime and state.time > 1.0 then
        state.logged_runtime = true
        for _, src in ipairs(state.sources) do
            if src.audio_enabled and dse.audio.get_source_state then
                local has_state, clip_loaded, is_playing, spatial_enabled, min_d, max_d, rolloff, source_volume, source_pitch, runtime_handle, clip_bytes, clip_path =
                    dse.audio.get_source_state(src.entity)
                print(string.format("[3D][AudioComplete] runtime: %s state=%s playing=%s spatial=%s vol=%.2f pitch=%.2f loop_toggled=%s",
                    src.config.name, tostring(has_state), tostring(is_playing), tostring(spatial_enabled),
                    source_volume or 0.0, source_pitch or 0.0, tostring(state.loop_toggled)))
            end
        end
    end
end

return AudioComplete3D
