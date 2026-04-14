local Bootstrap = require("vse_demo.common.bootstrap")

local Demo159 = {}

local KEY_MINUS = 45
local KEY_EQUAL = 61
local KEY_LEFT_BRACKET = 91
local KEY_RIGHT_BRACKET = 93

local state = {
    initialized = false,
    using_reference_scene = false,
    frame_counter = 0,
    left_entity = nil,
    right_entity = nil,
    left_material = {
        metallic = 0.03,
        roughness = 0.78,
        emissive = { 0.0, 0.0, 0.08 }
    },
    right_material = {
        metallic = 0.38,
        roughness = 0.18,
        emissive = { 0.08, 0.01, 0.0 }
    }
}

local function apply_material_state()
    if state.left_entity ~= nil then
        dse.ecs.set_mesh_material_scalar(state.left_entity, "metallic", state.left_material.metallic)
        dse.ecs.set_mesh_material_scalar(state.left_entity, "roughness", state.left_material.roughness)
        dse.ecs.set_mesh_emissive(state.left_entity, state.left_material.emissive[1], state.left_material.emissive[2], state.left_material.emissive[3])
    end
    if state.right_entity ~= nil then
        dse.ecs.set_mesh_material_scalar(state.right_entity, "metallic", state.right_material.metallic)
        dse.ecs.set_mesh_material_scalar(state.right_entity, "roughness", state.right_material.roughness)
        dse.ecs.set_mesh_emissive(state.right_entity, state.right_material.emissive[1], state.right_material.emissive[2], state.right_material.emissive[3])
    end
end

local function log_material_state(tag)
    print(string.format(
        "[VSE-Demo][15.9] %s left(m=%.2f r=%.2f e=%.2f,%.2f,%.2f) right(m=%.2f r=%.2f e=%.2f,%.2f,%.2f)",
        tag,
        state.left_material.metallic,
        state.left_material.roughness,
        state.left_material.emissive[1],
        state.left_material.emissive[2],
        state.left_material.emissive[3],
        state.right_material.metallic,
        state.right_material.roughness,
        state.right_material.emissive[1],
        state.right_material.emissive[2],
        state.right_material.emissive[3]
    ))
end

local function try_bind_reference_mesh_entities_by_position()
    local left_candidate = nil
    local right_candidate = nil
    local candidates = {}
    local monster_entities = dse.ecs.find_entities_by_mesh_path("assets/cooked/reference_demo/shared/monster/Monster.dmesh")
    local lod_entities = dse.ecs.find_entities_by_mesh_path("assets/cooked/reference_demo/shared/monster_lod0/MonsterLOD0.dmesh")
    if type(monster_entities) == "table" then
        for _, entity in ipairs(monster_entities) do
            table.insert(candidates, entity)
        end
    end
    if type(lod_entities) == "table" then
        for _, entity in ipairs(lod_entities) do
            table.insert(candidates, entity)
        end
    end
    for _, entity in ipairs(candidates) do
        local x, y, z = dse.ecs.get_transform_position(entity)
        if x ~= nil and y ~= nil and y > -1.0 then
            if x < -1.0 then
                left_candidate = entity
            elseif x > 1.0 then
                right_candidate = entity
            end
        end
    end
    if left_candidate ~= nil and right_candidate ~= nil then
        state.left_entity = left_candidate
        state.right_entity = right_candidate
        apply_material_state()
        print(string.format("[VSE-Demo][15.9] runtime_entity_binding left=%d right=%d", left_candidate, right_candidate))
        return true
    end
    return false
end


local function bind_reference_mesh_entities()
    state.left_entity = nil
    state.right_entity = nil
    if try_bind_reference_mesh_entities_by_position() then
        return
    end
    state.left_entity = 1
    state.right_entity = 2
    dse.ecs.set_mesh_material(state.left_entity, "assets/cooked/reference_demo/shared/monster/Monster.dmat", 0)
    dse.ecs.set_mesh_material(state.right_entity, "assets/cooked/reference_demo/shared/monster/Monster.dmat", 1)
    print("[VSE-Demo][15.9] material_instance_bound slot=left source=assets/cooked/reference_demo/shared/monster/Monster.dmat index=0")
    print("[VSE-Demo][15.9] material_instance_bound slot=right source=assets/cooked/reference_demo/shared/monster/Monster.dmat index=1")
    apply_material_state()
    print("[VSE-Demo][15.9] runtime_entity_binding fallback_ids=1,2")
end


local function try_load_reference_scene(scene_path)
    local ok, diagnostics = dse.ecs.load_scene(scene_path)
    if ok then
        local missing_count = 0
        print("[VSE-Demo][15.9] startup_scene_loaded path=" .. scene_path)
        if type(diagnostics) == "string" and diagnostics ~= "" then
            for item in string.gmatch(diagnostics, "[^|]+") do
                missing_count = missing_count + 1
                print("[VSE-Demo][15.9] mvp_resource_missing " .. item)
            end
        end
        print("[VSE-Demo][15.9] missing_resource_count=" .. tostring(missing_count))
        state.using_reference_scene = true
        bind_reference_mesh_entities()
        return true
    end
    print("[VSE-Demo][15.9] startup_scene_failed path=" .. scene_path .. " reason=" .. tostring(diagnostics))
    state.using_reference_scene = false
    return false
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
        state.left_material.emissive[3] = 0.10 + state.left_material.roughness * 0.25
        state.right_material.emissive[1] = 0.10 + state.right_material.metallic * 0.25
        apply_material_state()
        log_material_state("material_input")
    end
end

local demo_config = {
    title = "DSEngine Lua Demo 15.9",
    intro_lines = {
        "[VSE-Demo][15.9] 启动 Lua 对齐版材质演示。",
        "[VSE-Demo][15.9] 当前阶段优先加载 reference scene，并用 Lua 覆盖材质参数。",
        "[VSE-Demo][15.9] 键盘调参：'-' / '=' 调左侧 roughness，'[' / ']' 调右侧 metallic。",
        "[VSE-Demo][15.9] 参考 scene: assets/scenes/reference_demo_15_9.scene.json",
        "[VSE-Demo][15.9] 当前已接入 Monster / MonsterLOD0 / OceanPlane cooked 资产。",
        "[VSE-Demo][15.9] 左右展示位已分别绑定 Monster.dmat 的第 0 / 1 个材质槽，并继续叠加 Lua 标量调参。",
        "[VSE-Demo][15.9] 当前已接入最小目录式天空盒，并继续保留 SkyLight 作为环境光近似。",
        "[VSE-Demo][15.9] 启动日志会输出 runtime_entity_binding，便于确认左右材质作用到了正确实体。"
    },
    camera = {
        x = 0.0,
        y = 7.5,
        z = 20.0,
        pitch = -16.0,
        yaw = 0.0,
        roll = 0.0,
        fov = 60.0,
        priority = 100,
        free_camera = true,
        post_process = {
            enabled = true,
            threshold = 0.95,
            intensity = 1.8
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
    ground = {
        x = 0.0,
        y = -2.0,
        z = 0.0,
        sx = 48.0,
        sy = 1.0,
        sz = 48.0,
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
            x = -2.8,
            y = 0.0,
            z = 0.0,
            sx = 1.1,
            sy = 2.8,
            sz = 1.0,
            color = { 0.95, 0.95, 1.0, 1.0 },
            material = {
                metallic = 0.08,
                roughness = 0.55,
                ao = 1.0,
                emissive_strength = 0.05,
                emissive_b = 0.20
            }
        },
        {
            x = 2.8,
            y = 0.0,
            z = 0.0,
            sx = 1.1,
            sy = 2.8,
            sz = 1.0,
            color = { 1.0, 0.9, 0.85, 1.0 },
            material = {
                metallic = 0.12,
                roughness = 0.30,
                ao = 1.0,
                emissive_strength = 0.08,
                emissive_r = 0.24
            }
        }
    }
}

function Demo159.Setup(config)
    local merged = demo_config
    if type(config) == "table" then
        if type(config.title) == "string" and config.title ~= "" then
            merged.title = config.title
        end
    end
    if not try_load_reference_scene("assets/scenes/reference_demo_15_9.scene.json") then
        Bootstrap.SetupScene(merged)
        print("[VSE-Demo][15.9] fallback_scene_active programmatic_preview")
        state.left_entity = 1
        state.right_entity = 2
        apply_material_state()
        print_visual_baseline_summary()

    end
    log_material_state("material_bootstrap")

    state.initialized = true
end

function Demo159.Update(delta_time)
    if not state.initialized then
        return
    end
    handle_keyboard_interaction()
end

return Demo159
