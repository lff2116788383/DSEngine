-- Streaming 异步加载 Demo
-- 验证 streaming zone 的创建、资源注册、距离触发加载/卸载
-- 场景：3 个区域分布在不同位置，相机靠近时自动加载
local StreamingDemo = {}

local state = {
    camera = nil,
    light  = nil,
    zones  = {},   -- {id, name, cx, cz, entities, state_text}
    time   = 0.0,
    hud_entity = nil,
}

local cube_v = {
    -0.5,-0.5, 0.5,  0.5,-0.5, 0.5,  0.5, 0.5, 0.5, -0.5, 0.5, 0.5,
    -0.5,-0.5,-0.5,  0.5,-0.5,-0.5,  0.5, 0.5,-0.5, -0.5, 0.5,-0.5,
}
local cube_i = {0,1,2,2,3,0, 1,5,6,6,2,1, 5,4,7,7,6,5, 4,0,3,3,7,4, 3,2,6,6,7,3, 4,5,1,1,0,4}

local function make_box(x, y, z, sx, sy, sz, r, g, b)
    local e = dse.ecs.create_entity()
    dse.ecs.add_transform(e, x, y, z, sx, sy, sz)
    dse.ecs.add_mesh_renderer(e, r, g, b, 1.0, cube_v, cube_i)
    dse.ecs.set_mesh_shader_variant(e, "MESH_LIT")
    dse.ecs.set_mesh_material(e, 0.0, 0.5, 1.0, 0.0, 0.0, 0.0, 0.0, true, false)
    return e
end

local function make_zone_marker(cx, cz, r, g, b)
    -- 区域中心标记柱
    return make_box(cx, 1.5, cz, 0.6, 3.0, 0.6, r, g, b)
end

local zone_configs = {
    {name = "Zone_A", cx = -15, cz = 0,   color = {0.9, 0.3, 0.2}, load_r = 12, unload_r = 18},
    {name = "Zone_B", cx =  15, cz = 0,   color = {0.2, 0.7, 0.3}, load_r = 12, unload_r = 18},
    {name = "Zone_C", cx =   0, cz = -20, color = {0.3, 0.4, 0.9}, load_r = 12, unload_r = 18},
}

local function setup_camera(config)
    local e = dse.ecs.create_entity()
    local dist = (config and config.camera_distance) or 8.0
    dse.ecs.add_transform(e, 0, 6, dist, 1, 1, 1)
    dse.ecs.set_transform_rotation(e, -20, 0, 0)
    dse.ecs.add_camera_3d(e, 60, 120)
    if dse.ecs.add_free_camera_controller then
        dse.ecs.add_free_camera_controller(e, 10.0, 0.12)
    end
    state.camera = e
end

local function setup_scene()
    local light = dse.ecs.create_entity()
    dse.ecs.add_directional_light_3d(light, -0.4, -1.0, -0.3, 1.0, 0.95, 0.88, 1.0, 0.2, 0.30)
    state.light = light

    -- 地面
    make_box(0, -0.2, -5, 60, 0.1, 60, 0.22, 0.25, 0.27)

    -- 创建 streaming zones
    if not streaming or not streaming.create_zone then
        print("[Streaming] streaming API 不可用, 仅展示静态场景")
        -- 仍然创建静态物体展示布局
        for _, zc in ipairs(zone_configs) do
            make_zone_marker(zc.cx, zc.cz, zc.color[1], zc.color[2], zc.color[3])
            for i = 1, 4 do
                local angle = (i - 1) * math.pi * 0.5
                local ox = math.cos(angle) * 3.5
                local oz = math.sin(angle) * 3.5
                make_box(zc.cx + ox, 0.6, zc.cz + oz, 1.0, 1.2, 1.0,
                    zc.color[1] * 0.7, zc.color[2] * 0.7, zc.color[3] * 0.7)
            end
        end
        return
    end

    for _, zc in ipairs(zone_configs) do
        local zone_id = streaming.create_zone(zc.name, zc.cx, 0, zc.cz, zc.load_r, zc.unload_r)

        -- 注册一些资源到 zone（实际项目中是 texture/mesh/audio）
        streaming.add_asset(zone_id, "models/cube.dmesh", "mesh")

        -- 标记柱（始终可见，不受 streaming 控制）
        local marker = make_zone_marker(zc.cx, zc.cz, zc.color[1], zc.color[2], zc.color[3])

        -- 区域内物体
        local zone_entities = {}
        for i = 1, 6 do
            local angle = (i - 1) * math.pi * 2 / 6
            local ox = math.cos(angle) * 4.0
            local oz = math.sin(angle) * 4.0
            local e = make_box(zc.cx + ox, 0.6, zc.cz + oz, 1.0, 1.2, 1.0,
                zc.color[1] * 0.8, zc.color[2] * 0.8, zc.color[3] * 0.8)
            table.insert(zone_entities, e)
        end

        table.insert(state.zones, {
            id = zone_id,
            name = zc.name,
            cx = zc.cx, cz = zc.cz,
            marker = marker,
            entities = zone_entities,
            last_state = "",
        })

        print(string.format("[Streaming] Zone '%s' id=%d center=(%.0f,%.0f) load_r=%.0f unload_r=%.0f",
            zc.name, zone_id, zc.cx, zc.cz, zc.load_r, zc.unload_r))
    end

    print("[Streaming] WASD 移动相机接近各区域, 观察 zone 状态变化")
end

function StreamingDemo.Setup(config)
    setup_camera(config)
    setup_scene()
end

function StreamingDemo.Update(delta_time)
    local dt = delta_time or 0
    if dt > 0.1 then dt = 0.1 end
    state.time = state.time + dt

    if not streaming or not streaming.get_zone_state then return end

    -- 每秒打印一次 zone 状态
    for _, zone in ipairs(state.zones) do
        local zone_state = streaming.get_zone_state(zone.id)
        local progress = streaming.get_zone_progress(zone.id)
        if zone_state ~= zone.last_state then
            print(string.format("[Streaming] %s: %s → %s (progress=%.0f%%)",
                zone.name, zone.last_state == "" and "init" or zone.last_state,
                zone_state, progress * 100))
            zone.last_state = zone_state
        end

        -- 已加载的区域物体做旋转动画
        if zone_state == "loaded" then
            for i, e in ipairs(zone.entities) do
                local angle = state.time * 25 + i * 60
                dse.ecs.set_transform_rotation(e, 0, angle, 0)
            end
        end
    end
end

return StreamingDemo
