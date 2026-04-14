local Bootstrap = require("vse_demo.common.bootstrap")

local Demo157 = {}

local KEY_MINUS = 45
local KEY_EQUAL = 61
local KEY_LEFT_BRACKET = 91
local KEY_RIGHT_BRACKET = 93

local state = {
    initialized = false,
    frame_counter = 0,
    left_far_entity = nil,
    left_entity = nil,
    center_entity = nil,
    right_entity = nil,
    right_far_entity = nil,
    left_far_material = {
        metallic = 0.08,
        roughness = 0.55,
        emissive = { 0.0, 0.0, 0.16 }
    },
    left_material = {
        metallic = 0.18,
        roughness = 0.35,
        emissive = { 0.10, 0.06, 0.0 }
    },
    center_material = {
        metallic = 0.04,
        roughness = 0.82,
        emissive = { 0.0, 0.12, 0.0 }
    },
    right_material = {
        metallic = 0.28,
        roughness = 0.24,
        emissive = { 0.10, 0.0, 0.18 }
    },
    right_far_material = {
        metallic = 0.42,
        roughness = 0.14,
        emissive = { 0.18, 0.08, 0.0 }
    }
}


local function apply_material_state()
    if state.left_entity ~= nil then
        dse.ecs.set_mesh_material_scalar(state.left_entity, "metallic", state.left_material.metallic)
        dse.ecs.set_mesh_material_scalar(state.left_entity, "roughness", state.left_material.roughness)
        dse.ecs.set_mesh_emissive(state.left_entity, state.left_material.emissive[1], state.left_material.emissive[2], state.left_material.emissive[3])
    end
    if state.center_entity ~= nil then
        dse.ecs.set_mesh_material_scalar(state.center_entity, "metallic", state.center_material.metallic)
        dse.ecs.set_mesh_material_scalar(state.center_entity, "roughness", state.center_material.roughness)
        dse.ecs.set_mesh_emissive(state.center_entity, state.center_material.emissive[1], state.center_material.emissive[2], state.center_material.emissive[3])
    end
    if state.right_entity ~= nil then
        dse.ecs.set_mesh_material_scalar(state.right_entity, "metallic", state.right_material.metallic)
        dse.ecs.set_mesh_material_scalar(state.right_entity, "roughness", state.right_material.roughness)
        dse.ecs.set_mesh_emissive(state.right_entity, state.right_material.emissive[1], state.right_material.emissive[2], state.right_material.emissive[3])
    end
end

local function log_material_state(tag)
    print(string.format(
        "[VSE-Demo][15.7] %s lf(r=%.2f) l(m=%.2f r=%.2f) c(r=%.2f) r(m=%.2f r=%.2f) rf(m=%.2f r=%.2f)",
        tag,
        state.left_far_material.roughness,
        state.left_material.metallic,
        state.left_material.roughness,
        state.center_material.roughness,
        state.right_material.metallic,
        state.right_material.roughness,
        state.right_far_material.metallic,
        state.right_far_material.roughness
    ))
end


local function bind_preview_entities()
    local left_far = dse.ecs.create_entity()
    local left = dse.ecs.create_entity()
    local center = dse.ecs.create_entity()
    local right = dse.ecs.create_entity()
    local right_far = dse.ecs.create_entity()
    state.left_far_entity = left_far
    state.left_entity = left
    state.center_entity = center
    state.right_entity = right
    state.right_far_entity = right_far

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

    dse.ecs.add_transform(left_far, -11.0, 0.0, 0.0, 1.0, 2.8, 1.0)
    dse.ecs.add_transform(left, -5.5, 0.0, 0.0, 1.0, 2.8, 1.0)
    dse.ecs.add_transform(center, 0.0, 0.0, 0.0, 1.2, 3.0, 1.0)
    dse.ecs.add_transform(right, 5.5, 0.0, 0.0, 1.0, 2.8, 1.0)
    dse.ecs.add_transform(right_far, 11.0, 0.0, 0.0, 1.0, 2.8, 1.0)

    dse.ecs.add_mesh_renderer(left_far, 0.95, 0.95, 1.0, 1.0, vertices, indices)
    dse.ecs.add_mesh_renderer(left, 1.0, 0.94, 0.88, 1.0, vertices, indices)
    dse.ecs.add_mesh_renderer(center, 0.85, 0.96, 0.90, 1.0, vertices, indices)
    dse.ecs.add_mesh_renderer(right, 0.92, 0.90, 1.0, 1.0, vertices, indices)
    dse.ecs.add_mesh_renderer(right_far, 0.95, 0.88, 0.82, 1.0, vertices, indices)
    dse.ecs.set_mesh_shader_variant(left_far, "MESH_PBR")
    dse.ecs.set_mesh_shader_variant(left, "MESH_PBR")
    dse.ecs.set_mesh_shader_variant(center, "MESH_PBR")
    dse.ecs.set_mesh_shader_variant(right, "MESH_PBR")
    dse.ecs.set_mesh_shader_variant(right_far, "MESH_PBR")
    apply_material_state()
end


local function handle_keyboard_interaction()
    local changed = false
    if dse.app.get_key_down(KEY_MINUS) then
        state.left_material.roughness = math.max(0.04, state.left_material.roughness - 0.05)
        changed = true
    end
    if dse.app.get_key_down(KEY_EQUAL) then
        state.left_material.roughness = math.min(1.0, state.left_material.roughness + 0.05)
        changed = true
    end
    if dse.app.get_key_down(KEY_LEFT_BRACKET) then
        state.right_material.metallic = math.max(0.0, state.right_material.metallic - 0.05)
        changed = true
    end
    if dse.app.get_key_down(KEY_RIGHT_BRACKET) then
        state.right_material.metallic = math.min(1.0, state.right_material.metallic + 0.05)
        changed = true
    end
    if changed then
        state.left_material.emissive[1] = 0.04 + state.left_material.roughness * 0.18
        state.right_material.roughness = math.max(0.08, 0.8 - state.right_material.metallic * 0.5)
        state.right_material.emissive[3] = 0.08 + state.right_material.metallic * 0.28
        apply_material_state()
        log_material_state("material_input")
    end
end


local function try_load_reference_scene(scene_path)
    local ok, diagnostics = dse.ecs.load_scene(scene_path)
    if ok then
        local missing_count = 0
        print("[VSE-Demo][15.7] startup_scene_loaded path=" .. scene_path)
        if type(diagnostics) == "string" and diagnostics ~= "" then
            for item in string.gmatch(diagnostics, "[^|]+") do
                missing_count = missing_count + 1
                print("[VSE-Demo][15.7] mvp_resource_missing " .. item)
            end
        end
        print("[VSE-Demo][15.7] missing_resource_count=" .. tostring(missing_count))
        return true
    end
    print("[VSE-Demo][15.7] startup_scene_failed path=" .. scene_path .. " reason=" .. tostring(diagnostics))
    return false
end

local demo_config = {
    title = "DSEngine Lua Demo 15.7",
    intro_lines = {
        "[VSE-Demo][15.7] 启动 Lua 对齐版材质模型展示。",
        "[VSE-Demo][15.7] 当前以 reference scene 承载五类材质观察位：PhoneTwoPass / Phone / BlinnPhone / OrenNayar / Custom。",
        "[VSE-Demo][15.7] 交互：'-' / '=' 调左侧 Phone roughness，'[' / ']' 调右侧 OrenNayar metallic。",
        "[VSE-Demo][15.7] 参考 scene: assets/scenes/reference_demo_15_7.scene.json",
        "[VSE-Demo][15.7] 当前已接入 Monster / OceanPlane cooked 资产与最小目录式天空盒。",
        "[VSE-Demo][15.7] 左侧两个展示位已显式接入 Monster.dmat 的第 0 / 1 个材质槽，其余展示位继续保留标量实例语义。"
    },
    camera = {
        x = 0.0,
        y = 6.5,
        z = 20.0,
        pitch = -14.0,
        yaw = 0.0,
        roll = 0.0,
        fov = 60.0,
        priority = 100,
        free_camera = true,
        post_process = {
            enabled = true,
            threshold = 1.0,
            intensity = 1.6
        }
    },
    light = {
        dir_x = -0.45,
        dir_y = -1.0,
        dir_z = -0.45,
        color_r = 1.0,
        color_g = 0.97,
        color_b = 0.92,
        intensity = 1.8,
        ambient = 0.12,
        shadow = 0.45
    },
    skybox = {
        cubemap_path = "assets/source/reference_demo/shared/skybox/default_sky"
    },
    ground = {
        x = 0.0,
        y = -2.0,
        z = 0.0,
        sx = 52.0,
        sy = 1.0,
        sz = 40.0,
        color = { 0.8, 0.8, 0.85, 1.0 },
        material = {
            metallic = 0.0,
            roughness = 0.9,
            ao = 1.0,
            emissive_strength = 0.0
        }
    },
    actors = {
        {
            x = -11.0,
            y = 0.0,
            z = 0.0,
            sx = 1.0,
            sy = 2.8,
            sz = 1.0,
            color = { 0.95, 0.95, 1.0, 1.0 },
            material = {
                metallic = 0.08,
                roughness = 0.55,
                ao = 1.0,
                emissive_strength = 0.03,
                emissive_b = 0.16
            }
        },
        {
            x = -5.5,
            y = 0.0,
            z = 0.0,
            sx = 1.0,
            sy = 2.8,
            sz = 1.0,
            color = { 1.0, 0.94, 0.88, 1.0 },
            material = {
                metallic = 0.18,
                roughness = 0.35,
                ao = 1.0,
                emissive_strength = 0.04,
                emissive_r = 0.10,
                emissive_g = 0.06
            }
        },
        {
            x = 0.0,
            y = 0.0,
            z = 0.0,
            sx = 1.2,
            sy = 3.0,
            sz = 1.0,
            color = { 0.85, 0.96, 0.90, 1.0 },
            material = {
                metallic = 0.04,
                roughness = 0.82,
                ao = 1.0,
                emissive_strength = 0.03,
                emissive_g = 0.12
            }
        },
        {
            x = 5.5,
            y = 0.0,
            z = 0.0,
            sx = 1.0,
            sy = 2.8,
            sz = 1.0,
            color = { 0.92, 0.90, 1.0, 1.0 },
            material = {
                metallic = 0.28,
                roughness = 0.24,
                ao = 1.0,
                emissive_strength = 0.05,
                emissive_r = 0.10,
                emissive_b = 0.18
            }
        },
        {
            x = 11.0,
            y = 0.0,
            z = 0.0,
            sx = 1.0,
            sy = 2.8,
            sz = 1.0,
            color = { 0.95, 0.88, 0.82, 1.0 },
            material = {
                metallic = 0.42,
                roughness = 0.14,
                ao = 1.0,
                emissive_strength = 0.05,
                emissive_r = 0.18,
                emissive_g = 0.08
            }
        }
    }
}

function Demo157.Setup(config)
    local merged = demo_config
    if type(config) == "table" then
        if type(config.title) == "string" and config.title ~= "" then
            merged.title = config.title
        end
    end
    if not try_load_reference_scene("assets/scenes/reference_demo_15_7.scene.json") then
        Bootstrap.SetupScene(merged)
        state.left_far_entity = 1
        state.left_entity = 2
        state.center_entity = 3
        state.right_entity = 4
        state.right_far_entity = 5
        dse.ecs.set_mesh_material(state.left_far_entity, "assets/cooked/reference_demo/shared/monster/Monster.dmat", 0)
        dse.ecs.set_mesh_material(state.left_entity, "assets/cooked/reference_demo/shared/monster/Monster.dmat", 1)
        apply_material_state()
        print("[VSE-Demo][15.7] material_instance_bound slot=PhoneTwoPass source=assets/cooked/reference_demo/shared/monster/Monster.dmat index=0")
        print("[VSE-Demo][15.7] material_instance_bound slot=Phone source=assets/cooked/reference_demo/shared/monster/Monster.dmat index=1")
        print("[VSE-Demo][15.7] skybox_loaded path=assets/source/reference_demo/shared/skybox/default_sky")
        print("[VSE-Demo][15.7] fallback_scene_active material_showcase_preview")
    else
        state.left_far_entity = 1
        state.left_entity = 2
        state.center_entity = 3
        state.right_entity = 4
        state.right_far_entity = 5
        dse.ecs.set_mesh_material(state.left_far_entity, "assets/cooked/reference_demo/shared/monster/Monster.dmat", 0)
        dse.ecs.set_mesh_material(state.left_entity, "assets/cooked/reference_demo/shared/monster/Monster.dmat", 1)
        apply_material_state()
        print("[VSE-Demo][15.7] material_instance_bound slot=PhoneTwoPass source=assets/cooked/reference_demo/shared/monster/Monster.dmat index=0")
        print("[VSE-Demo][15.7] material_instance_bound slot=Phone source=assets/cooked/reference_demo/shared/monster/Monster.dmat index=1")
    end
    log_material_state("material_bootstrap")
    state.initialized = true
end



function Demo157.Update(delta_time)
    if not state.initialized then
        return
    end
    handle_keyboard_interaction()
end

return Demo157
