-- 3D P4 sample: Render Quality Showcase
-- 目标：专项压测 Shadow/CSM、Bloom、Exposure/Gamma、后处理模式组合
-- 覆盖 API: add_directional_light_3d, set_directional_light_shadow, add_post_process,
--            set_post_process_bloom, set_post_process_color, get_post_process_state,
--            set_mesh_material (emissive 对比), add_point_light_3d
local RenderQualityShowcase3D = {}

local state = {
    camera = nil,
    light = nil,
    post_process = nil,
    objects = {},
    time = 0.0,
    logged_quality = false,
    -- 压测阶段控制
    phase = 0,       -- 0:shadow  1:bloom  2:exposure_gamma  3:full_postprocess
    phase_time = 0.0,
    logged_phase = {},
}

local PHASE_DURATION = 4.0  -- 每个阶段持续秒数

local function cube_vertices()
    return { -0.5,-0.5,0.5, 0.5,-0.5,0.5, 0.5,0.5,0.5, -0.5,0.5,0.5, -0.5,-0.5,-0.5, 0.5,-0.5,-0.5, 0.5,0.5,-0.5, -0.5,0.5,-0.5 }
end

local function cube_indices()
    return { 0,1,2,2,3,0, 1,5,6,6,2,1, 5,4,7,7,6,5, 4,0,3,3,7,4, 3,2,6,6,7,3, 4,5,1,1,0,4 }
end

local function add_cube(name, x, y, z, sx, sy, sz, color, material)
    local e = dse.ecs.create_entity()
    dse.ecs.add_transform(e, x, y, z, sx, sy, sz)
    dse.ecs.add_mesh_renderer(e, color[1], color[2], color[3], color[4] or 1.0, cube_vertices(), cube_indices())
    dse.ecs.set_mesh_shader_variant(e, "MESH_LIT")
    material = material or {}
    dse.ecs.set_mesh_material(e,
        material.metallic or 0.0,
        material.roughness or 0.50,
        material.ao or 1.0,
        material.er or 0.0,
        material.eg or 0.0,
        material.eb or 0.0,
        material.normal_strength or 1.0,
        material.receive_shadow ~= false,
        material.double_sided ~= false)
    return e
end

local function setup_camera(config)
    local camera = dse.ecs.create_entity()
    local distance = (type(config) == "table" and type(config.camera_distance) == "number") and config.camera_distance or 11.0
    dse.ecs.add_transform(camera, 0.0, 4.0, distance, 1.0, 1.0, 1.0)
    dse.ecs.set_transform_rotation(camera, -24.0, 0.0, 0.0)
    dse.ecs.add_camera_3d(camera, 60.0, 100)
    if dse.ecs.add_free_camera_controller then
        dse.ecs.add_free_camera_controller(camera, 5.5, 0.12)
    end
    state.camera = camera
end

local function setup_scene(config)
    -- ========== 光照 ==========
    local shadow_strength = (type(config) == "table" and type(config.shadow_strength) == "number") and config.shadow_strength or 0.50
    local light = dse.ecs.create_entity()
    dse.ecs.add_directional_light_3d(light, -0.42, -1.0, -0.28, 1.0, 0.95, 0.86, 1.20, 0.15, shadow_strength)
    -- CSM 参数
    local shadow_ok, cast_shadow, applied_strength, c0, c1, c2 = false, false, 0.0, 0.0, 0.0, 0.0
    if dse.ecs.set_directional_light_shadow then
        shadow_ok, cast_shadow, applied_strength, c0, c1, c2 = dse.ecs.set_directional_light_shadow(light, true, shadow_strength, 10.0, 30.0, 75.0)
    end
    state.light = light

    -- 点光源（bloom 压测辅助）
    if dse.ecs.add_point_light_3d then
        local point = dse.ecs.create_entity()
        dse.ecs.add_transform(point, 3.0, 2.0, 1.0, 1.0, 1.0, 1.0)
        dse.ecs.add_point_light_3d(point, 1.0, 0.70, 0.30, 8.0, 0.85, 1.0)
        table.insert(state.objects, { name = "point_light", entity = point })
    end

    -- ========== 场景物体 ==========
    -- 地面（receive_shadow）
    add_cube("ground", 0.0, -0.55, 0.0, 10.0, 0.12, 6.5,
        {0.42, 0.42, 0.38, 1.0}, {roughness = 0.72, receive_shadow = true})

    -- 阴影投射体
    local caster = add_cube("caster", 0.0, 1.30, 0.0, 1.1, 1.1, 1.1,
        {0.88, 0.72, 0.32, 1.0}, {roughness = 0.40, receive_shadow = true})
    table.insert(state.objects, { name = "caster", entity = caster })

    -- Emissive 物体（bloom 压测核心）
    add_cube("emissive_red", -2.8, 0.45, 0.0, 0.8, 0.8, 0.8,
        {1.0, 0.22, 0.10, 1.0}, {er = 1.8, eg = 0.22, eb = 0.05})
    add_cube("emissive_green", 2.8, 0.45, 0.0, 0.8, 0.8, 0.8,
        {0.10, 1.0, 0.22, 1.0}, {er = 0.05, eg = 1.5, eb = 0.22})
    add_cube("emissive_blue", 0.0, 0.45, -2.5, 0.8, 0.8, 0.8,
        {0.15, 0.35, 1.0, 1.0}, {er = 0.12, eg = 0.40, eb = 2.0})

    -- 金属/粗糙对比
    add_cube("metallic", -1.5, 0.25, 2.0, 0.7, 0.7, 0.7,
        {0.90, 0.88, 0.82, 1.0}, {metallic = 0.95, roughness = 0.12, receive_shadow = true})
    add_cube("rough", 1.5, 0.25, 2.0, 0.7, 0.7, 0.7,
        {0.72, 0.45, 0.28, 1.0}, {metallic = 0.05, roughness = 0.95, receive_shadow = true})

    -- ========== 后处理 ==========
    local pp = dse.ecs.create_entity()
    local bloom_threshold = (type(config) == "table" and type(config.bloom_threshold) == "number") and config.bloom_threshold or 0.75
    local bloom_intensity = (type(config) == "table" and type(config.bloom_intensity) == "number") and config.bloom_intensity or 1.35
    local exposure = (type(config) == "table" and type(config.exposure) == "number") and config.exposure or 1.0
    local gamma = (type(config) == "table" and type(config.gamma) == "number") and config.gamma or 2.2
    dse.ecs.add_post_process(pp, true, bloom_threshold, bloom_intensity, exposure)
    if dse.ecs.set_post_process_color then
        dse.ecs.set_post_process_color(pp, true, exposure, gamma)
    end
    state.post_process = pp

    -- 初始日志
    print(string.format("[3D][RenderQuality] setup: shadow_strength=%.2f CSM=(%.0f/%.0f/%.0f) cast_shadow=%s set_directional_light_shadow=%s",
        applied_strength, c0, c1, c2, tostring(cast_shadow), tostring(shadow_ok == true)))
    print(string.format("[3D][RenderQuality] setup: bloom threshold=%.2f intensity=%.2f exposure=%.2f gamma=%.2f",
        bloom_threshold, bloom_intensity, exposure, gamma))
    print(string.format("[3D][RenderQuality] api: set_directional_light_shadow=%s set_post_process_bloom=%s set_post_process_color=%s get_post_process_state=%s",
        tostring(dse.ecs.set_directional_light_shadow ~= nil),
        tostring(dse.ecs.set_post_process_bloom ~= nil),
        tostring(dse.ecs.set_post_process_color ~= nil),
        tostring(dse.ecs.get_post_process_state ~= nil)))
end

function RenderQualityShowcase3D.Setup(config)
    print("[3D][RenderQuality] setup: Shadow/CSM + Bloom + Exposure/Gamma + 后处理组合压测")
    setup_camera(config or {})
    setup_scene(config or {})
end

function RenderQualityShowcase3D.Update(delta_time)
    local dt = delta_time or 0.0
    if dt > 0.1 then dt = 0.1 end
    state.time = state.time + dt
    state.phase_time = state.phase_time + dt

    -- 阶段推进
    if state.phase_time >= PHASE_DURATION then
        state.phase = state.phase + 1
        state.phase_time = 0.0
        if state.phase > 3 then state.phase = 0 end
    end

    local t = state.time
    local pt = state.phase_time

    -- 阴影投射体动画
    local caster_obj = nil
    for _, obj in ipairs(state.objects) do
        if obj.name == "caster" then caster_obj = obj.entity end
    end
    if caster_obj then
        local cx = math.sin(t * 0.65) * 1.8
        local cz = math.cos(t * 0.45) * 0.8
        local cy = 1.3 + math.sin(t * 1.0) * 0.30
        dse.ecs.set_transform_position(caster_obj, cx, cy, cz)
        dse.ecs.set_transform_rotation(caster_obj, t * 35.0, t * 50.0, 0.0)
    end

    -- ===== 阶段化参数 =====
    if state.phase == 0 then
        -- 阶段 0: Shadow/CSM 变化
        local strength = 0.15 + (math.sin(pt / PHASE_DURATION * math.pi) * 0.5 + 0.5) * 0.65
        if state.light and dse.ecs.set_directional_light_shadow then
            dse.ecs.set_directional_light_shadow(state.light, true, strength, 10.0, 30.0, 75.0)
        end
        if not state.logged_phase[0] then
            state.logged_phase[0] = true
            print("[3D][RenderQuality] phase0=shadow_csm 动态 shadow_strength")
        end

    elseif state.phase == 1 then
        -- 阶段 1: Bloom 变化
        local bloom_pulse = (math.sin(pt / PHASE_DURATION * math.pi * 2.0) * 0.5 + 0.5)
        local threshold = 0.35 + bloom_pulse * 0.55
        local intensity = 0.60 + bloom_pulse * 2.0
        if state.post_process and dse.ecs.set_post_process_bloom then
            dse.ecs.set_post_process_bloom(state.post_process, true, true, threshold, intensity, 1.0)
        end
        if not state.logged_phase[1] then
            state.logged_phase[1] = true
            print("[3D][RenderQuality] phase1=bloom 动态 threshold/intensity")
        end

    elseif state.phase == 2 then
        -- 阶段 2: Exposure/Gamma 变化
        local exp_pulse = (math.sin(pt / PHASE_DURATION * math.pi) * 0.5 + 0.5)
        local exposure = 0.60 + exp_pulse * 0.80
        local gamma = 1.80 + exp_pulse * 0.60
        if state.post_process and dse.ecs.set_post_process_color then
            dse.ecs.set_post_process_color(state.post_process, true, exposure, gamma)
        end
        if not state.logged_phase[2] then
            state.logged_phase[2] = true
            print("[3D][RenderQuality] phase2=exposure_gamma 动态 tone mapping")
        end

    elseif state.phase == 3 then
        -- 阶段 3: 全后处理联动
        local pulse = (math.sin(pt / PHASE_DURATION * math.pi * 3.0) * 0.5 + 0.5)
        if state.post_process then
            if dse.ecs.set_post_process_bloom then
                dse.ecs.set_post_process_bloom(state.post_process, true, true, 0.45 + pulse * 0.40, 0.80 + pulse * 1.50, 0.85 + pulse * 0.35)
            end
            if dse.ecs.set_post_process_color then
                dse.ecs.set_post_process_color(state.post_process, true, 0.85 + pulse * 0.30, 2.0 + pulse * 0.40)
            end
        end
        if state.light and dse.ecs.set_directional_light_shadow then
            dse.ecs.set_directional_light_shadow(state.light, true, 0.20 + pulse * 0.55, 10.0, 30.0, 75.0)
        end
        if not state.logged_phase[3] then
            state.logged_phase[3] = true
            print("[3D][RenderQuality] phase3=full_postprocess shadow+bloom+exposure 联动")
        end
    end

    -- 运行时状态日志
    if not state.logged_quality and state.time > 1.0 then
        state.logged_quality = true
        if state.post_process and dse.ecs.get_post_process_state then
            local ok, enabled, bloom_en, threshold, intensity, color_en, exposure, gamma, ssao_en, ssao_r, ssao_b =
                dse.ecs.get_post_process_state(state.post_process)
            print(string.format("[3D][RenderQuality] runtime: get_post_process_state=%s enabled=%s bloom=%s(%.2f/%.2f) color=%s(%.2f/%.2f) ssao=%s(%.2f/%.2f)",
                tostring(ok), tostring(enabled), tostring(bloom_en), threshold or 0, intensity or 0,
                tostring(color_en), exposure or 0, gamma or 0,
                tostring(ssao_en), ssao_r or 0, ssao_b or 0))
        end
        print(string.format("[3D][RenderQuality] runtime: phase=%d phase_time=%.1f", state.phase, state.phase_time))
    end
end

return RenderQualityShowcase3D
