local Bootstrap = {}

--- @class VseDemoBootstrapConfig
--- @field title string|nil 窗口标题
--- @field intro_lines string[]|nil 启动时输出的提示信息
--- @field camera table|nil 相机参数
--- @field light table|nil 主方向光参数
--- @field ground table|nil 地面参数
--- @field actors table[]|nil 演示实体列表
--- @field material_controls table[]|nil 材质调节说明

local created_entities = {}

local function remember(entity)
    table.insert(created_entities, entity)
    return entity
end

local function create_box_mesh(x, y, z, sx, sy, sz, color, material)
    local entity = remember(dse.ecs.create_entity())
    dse.ecs.add_transform(entity, x, y, z, sx, sy, sz)

    local vertices = {
        -0.5, -0.5,  0.5,
         0.5, -0.5,  0.5,
         0.5,  0.5,  0.5,
        -0.5,  0.5,  0.5,
        -0.5, -0.5, -0.5,
         0.5, -0.5, -0.5,
         0.5,  0.5, -0.5,
        -0.5,  0.5, -0.5
    }
    local indices = {
        0, 1, 2, 2, 3, 0,
        1, 5, 6, 6, 2, 1,
        5, 4, 7, 7, 6, 5,
        4, 0, 3, 3, 7, 4,
        3, 2, 6, 6, 7, 3,
        4, 5, 1, 1, 0, 4
    }

    local base = color or { 1.0, 1.0, 1.0, 1.0 }
    local scalar = material or {}
    dse.ecs.add_mesh_renderer(entity, base[1], base[2], base[3], base[4] or 1.0, vertices, indices)
    dse.ecs.set_mesh_shader_variant(entity, scalar.shader_variant or "MESH_LIT")
    dse.ecs.set_mesh_material(
        entity,
        scalar.metallic or 0.05,
        scalar.roughness or 0.6,
        scalar.ao or 1.0,
        scalar.emissive_strength or 0.02,
        scalar.emissive_r or 0.0,
        scalar.emissive_g or 0.0,
        scalar.emissive_b or 0.0,
        scalar.receive_shadow ~= false
    )
    return entity
end

local function setup_camera(camera)
    local cfg = camera or {}
    local entity = remember(dse.ecs.create_entity())
    dse.ecs.add_transform(
        entity,
        cfg.x or 0.0,
        cfg.y or 5.0,
        cfg.z or 15.0,
        1.0,
        1.0,
        1.0
    )
    dse.ecs.set_transform_rotation(entity, cfg.pitch or -15.0, cfg.yaw or 0.0, cfg.roll or 0.0)
    dse.ecs.add_camera_3d(entity, cfg.fov or 60.0, cfg.priority or 100)
    if cfg.free_camera ~= false and Ecs and Ecs.add_free_camera_controller then
        Ecs.add_free_camera_controller(entity)
    end
    if cfg.post_process and Ecs and Ecs.add_post_process then
        Ecs.add_post_process(
            entity,
            cfg.post_process.enabled ~= false,
            cfg.post_process.threshold or 1.0,
            cfg.post_process.intensity or 1.5
        )
    end
    return entity
end

local function setup_directional_light(light)
    local cfg = light or {}
    local entity = remember(dse.ecs.create_entity())
    dse.ecs.add_directional_light_3d(
        entity,
        cfg.dir_x or -0.45,
        cfg.dir_y or -1.0,
        cfg.dir_z or -0.45,
        cfg.color_r or 1.0,
        cfg.color_g or 0.97,
        cfg.color_b or 0.92,
        cfg.intensity or 1.8,
        cfg.ambient or 0.12,
        cfg.shadow or 0.45
    )
    return entity
end

local function setup_ground(ground)
    local cfg = ground or {}
    return create_box_mesh(
        cfg.x or 0.0,
        cfg.y or -2.0,
        cfg.z or 0.0,
        cfg.sx or 40.0,
        cfg.sy or 1.0,
        cfg.sz or 40.0,
        cfg.color or { 0.8, 0.8, 0.85, 1.0 },
        cfg.material or { metallic = 0.0, roughness = 0.9, ao = 1.0, emissive_strength = 0.0 }
    )
end

local function print_lines(lines)
    if type(lines) ~= "table" then
        return
    end
    for _, line in ipairs(lines) do
        print(line)
    end
end

function Bootstrap.SetupScene(config)
    created_entities = {}
    local runtime = config or {}
    if type(runtime.title) == "string" and runtime.title ~= "" then
        dse.app.set_window_title(runtime.title)
    end

    print_lines(runtime.intro_lines)
    setup_camera(runtime.camera)
    setup_directional_light(runtime.light)
    setup_ground(runtime.ground)

    if type(runtime.actors) == "table" then
        for _, actor in ipairs(runtime.actors) do
            create_box_mesh(
                actor.x or 0.0,
                actor.y or 0.0,
                actor.z or 0.0,
                actor.sx or 1.0,
                actor.sy or 1.0,
                actor.sz or 1.0,
                actor.color,
                actor.material
            )
        end
    end

    if type(runtime.material_controls) == "table" then
        for _, line in ipairs(runtime.material_controls) do
            print(line)
        end
    end
end

function Bootstrap.UpdateMaterialPreview(actors, channel, delta)
    if type(actors) ~= "table" then
        return
    end
    for _, actor in ipairs(actors) do
        if type(actor.material) == "table" then
            local next_value = (actor.material[channel] or 0.0) + delta
            if channel == "roughness" then
                next_value = math.max(0.02, math.min(1.0, next_value))
            elseif channel == "metallic" then
                next_value = math.max(0.0, math.min(1.0, next_value))
            elseif channel == "emissive_strength" then
                next_value = math.max(0.0, math.min(1.0, next_value))
            end
            actor.material[channel] = next_value
        end
    end
end

return Bootstrap
