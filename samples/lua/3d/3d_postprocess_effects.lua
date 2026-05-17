-- PostProcess 全效果展示 Demo
-- 验证: Vignette / Film Grain / Color LUT / Outline / Auto Exposure
-- 与 3d_postprocess_showcase 互补（该 demo 侧重 Bloom + Color Grading）
local PPEffects = {}

local state = {
    camera = nil,
    pp     = nil,
    cubes  = {},
    time   = 0.0,
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

local function setup_camera(config)
    local e = dse.ecs.create_entity()
    local dist = (config and config.camera_distance) or 14.0
    dse.ecs.add_transform(e, 0, 4, dist, 1, 1, 1)
    dse.ecs.set_transform_rotation(e, -12, 0, 0)
    dse.ecs.add_camera_3d(e, 60, 100)
    if dse.ecs.add_free_camera_controller then
        dse.ecs.add_free_camera_controller(e, 6.0, 0.12)
    end

    -- PostProcess 组件
    dse.ecs.add_post_process(e, true, 0.9, 0.6, 1.0)

    -- Vignette
    -- set_post_process_vignette(e, enabled, intensity, radius, softness)
    dse.ecs.set_post_process_vignette(e, true, 0.45, 0.75, 0.35)

    -- Film Grain
    -- set_post_process_film_grain(e, enabled, intensity, time_scale)
    dse.ecs.set_post_process_film_grain(e, true, 0.12, 1.0)

    -- Auto Exposure
    -- set_post_process_auto_exposure(e, enabled, min, max, speed_up, speed_down, compensation)
    dse.ecs.set_post_process_auto_exposure(e, true, 0.5, 4.0, 2.0, 1.0, 0.0)

    -- Outline (edge detect)
    -- set_post_process_outline(e, enabled, r, g, b, thickness, depth_threshold, normal_threshold)
    dse.ecs.set_post_process_outline(e, true, 0.1, 0.1, 0.1, 1.5, 0.3, 0.7)

    -- Color LUT (路径可选，无 LUT 文件时仍可验证 API 不崩溃)
    -- set_post_process_color_lut(e, lut_path_or_nil, intensity)
    dse.ecs.set_post_process_color_lut(e, nil, 0.8)

    -- SSAO 补充深度感
    dse.ecs.set_post_process_ssao(e, true, 0.4, 0.02)

    state.camera = e
    state.pp = e
end

local function setup_scene()
    local light = dse.ecs.create_entity()
    dse.ecs.add_directional_light_3d(light, -0.4, -1.0, -0.3, 1.0, 0.95, 0.88, 1.2, 0.15, 0.30)

    -- 地面
    make_box(0, -0.15, 0, 20, 0.1, 16, 0.30, 0.32, 0.35)

    -- 多彩物体（让 outline 和 vignette 效果更明显）
    local colors = {
        {0.85, 0.25, 0.20}, {0.20, 0.75, 0.30}, {0.25, 0.40, 0.90},
        {0.90, 0.80, 0.15}, {0.75, 0.30, 0.75}, {0.90, 0.55, 0.20},
    }
    for i, c in ipairs(colors) do
        local angle = (i - 1) * (math.pi * 2 / #colors)
        local x = math.cos(angle) * 4.0
        local z = math.sin(angle) * 4.0
        local h = 0.8 + (i % 3) * 0.5
        local e = make_box(x, h / 2, z, 1.2, h, 1.2, c[1], c[2], c[3])
        table.insert(state.cubes, e)
    end

    -- 中央高柱
    local pillar = make_box(0, 1.5, 0, 1.0, 3.0, 1.0, 0.85, 0.85, 0.88)
    dse.ecs.set_mesh_material(pillar, 0.8, 0.15, 1.0, 0.0, 0.0, 0.0, 0.0, true, false)
    table.insert(state.cubes, pillar)

    -- 发光体（测试 auto exposure 反应）
    local emissive = dse.ecs.create_entity()
    dse.ecs.add_transform(emissive, 3, 2.5, -3, 0.8, 0.8, 0.8)
    dse.ecs.add_mesh_renderer(emissive, 1.0, 0.9, 0.5, 1.0, cube_v, cube_i)
    dse.ecs.set_mesh_shader_variant(emissive, "MESH_LIT")
    dse.ecs.set_mesh_material(emissive, 0.0, 0.3, 1.0, 2.0, 1.8, 0.6, 1.0, true, false)
    table.insert(state.cubes, emissive)

    print("[PPEffects] 后处理全效果: Vignette + Film Grain + Auto Exposure + Outline + Color LUT")
    print("[PPEffects] WASD 移动相机, 看向发光体观察自动曝光响应")
end

function PPEffects.Setup(config)
    setup_camera(config)
    setup_scene()
end

function PPEffects.Update(delta_time)
    local dt = delta_time or 0
    if dt > 0.1 then dt = 0.1 end
    state.time = state.time + dt

    -- 旋转物体
    for i, e in ipairs(state.cubes) do
        dse.ecs.set_transform_rotation(e, 0, state.time * 10 + i * 60, 0)
    end

    -- 动态调整 vignette 强度（模拟受伤闪烁效果）
    if state.pp then
        local vi = 0.35 + math.sin(state.time * 0.6) * 0.15
        dse.ecs.set_post_process_vignette(state.pp, true, vi, 0.75, 0.35)
    end

    -- 动态调整 film grain（夜间模式加重）
    if state.pp then
        local gi = 0.08 + math.sin(state.time * 0.3) * 0.06
        dse.ecs.set_post_process_film_grain(state.pp, true, gi, 1.0)
    end
end

return PPEffects
