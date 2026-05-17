-- Morph Target (Blend Shape) 验证 Demo
-- 验证: MorphComponent 的目标权重插值
-- 场景: 多个实体配置不同 morph target，权重随时间变化
local MorphDemo = {}

local state = {
    camera = nil,
    light  = nil,
    morph_entities = {},
    time = 0.0,
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
    local dist = (config and config.camera_distance) or 12.0
    dse.ecs.add_transform(e, 0, 3, dist, 1, 1, 1)
    dse.ecs.set_transform_rotation(e, -10, 0, 0)
    dse.ecs.add_camera_3d(e, 60, 100)
    if dse.ecs.add_free_camera_controller then
        dse.ecs.add_free_camera_controller(e, 6.0, 0.12)
    end
    state.camera = e
end

local function setup_scene()
    local light = dse.ecs.create_entity()
    dse.ecs.add_directional_light_3d(light, -0.4, -1.0, -0.3, 1.0, 0.95, 0.88, 1.1, 0.18, 0.30)
    state.light = light

    -- 地面
    make_box(0, -0.15, 0, 20, 0.1, 14, 0.25, 0.28, 0.30)

    -- Morph target 配置
    local configs = {
        {name="Stretch",  targets={"stretch_x","stretch_y"},       color={0.8, 0.4, 0.2}},
        {name="Bulge",    targets={"bulge_top","bulge_side"},      color={0.3, 0.7, 0.4}},
        {name="Squash",   targets={"squash","flatten"},            color={0.4, 0.3, 0.8}},
        {name="Twist",    targets={"twist_cw","twist_ccw","lean"}, color={0.8, 0.7, 0.2}},
        {name="Blend",    targets={"smile","frown","blink"},       color={0.7, 0.3, 0.6}},
    }

    for i, cfg in ipairs(configs) do
        local x = (i - 3) * 3.5
        local e = make_box(x, 1.2, 0, 1.5, 1.5, 1.5, cfg.color[1], cfg.color[2], cfg.color[3])

        -- 添加 morph component
        dse.ecs.add_morph(e)
        for _, target_name in ipairs(cfg.targets) do
            dse.ecs.morph_add_target(e, target_name, 0.0)
        end
        dse.ecs.set_morph_enabled(e, true)

        table.insert(state.morph_entities, {
            entity = e,
            name = cfg.name,
            targets = cfg.targets,
            phase = i * 0.6,
        })

        -- 标签柱
        make_box(x, 0.1, 2.5, 0.15, 0.2, 0.15, 0.9, 0.9, 0.9)
    end

    print(string.format("[Morph] 场景: %d 个 morph 物体, 各含 2~3 个 blend shape", #state.morph_entities))
    print("[Morph] 权重随时间正弦波变化, WASD 移动相机观察")
end

function MorphDemo.Setup(config)
    setup_camera(config)
    setup_scene()
end

function MorphDemo.Update(delta_time)
    local dt = delta_time or 0
    if dt > 0.1 then dt = 0.1 end
    state.time = state.time + dt

    -- 动态调整每个 morph entity 的 target 权重
    for _, me in ipairs(state.morph_entities) do
        for j, target_name in ipairs(me.targets) do
            local w = (math.sin(state.time * 1.2 + me.phase + j * 0.9) + 1.0) * 0.5
            dse.ecs.morph_set_weight(me.entity, target_name, w)
        end
        -- 用缩放模拟形变效果（morph 需要实际 mesh 顶点偏移才有视觉变化）
        local sx = 1.5 + math.sin(state.time * 1.2 + me.phase) * 0.3
        local sy = 1.5 + math.sin(state.time * 1.2 + me.phase + 1.0) * 0.3
        local sz = 1.5 + math.sin(state.time * 1.2 + me.phase + 2.0) * 0.3
        dse.ecs.set_transform_scale(me.entity, sx, sy, sz)
    end
end

return MorphDemo
