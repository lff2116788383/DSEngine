local Bootstrap = require("vse_demo.common.bootstrap")

local Demo158 = {}

local state = {
    using_reference_scene = false,
    missing_resource_count = 0
}

local function report_reference_scene_result(scene_path, diagnostics)
    state.missing_resource_count = 0
    if type(diagnostics) == "string" and diagnostics ~= "" then
        for item in string.gmatch(diagnostics, "[^|]+") do
            state.missing_resource_count = state.missing_resource_count + 1
            print("[VSE-Demo][15.8] mvp_resource_missing " .. item)
        end
    end
    print("[VSE-Demo][15.8] observer_hint free_camera=W/A/S/D + mouse")
    print("[VSE-Demo][15.8] missing_resource_count=" .. tostring(state.missing_resource_count))
end

local function try_load_reference_scene(scene_path)
    local ok, diagnostics = dse.ecs.load_scene(scene_path)
    if ok then
        state.using_reference_scene = true
        print("[VSE-Demo][15.8] startup_scene_loaded path=" .. scene_path)
        report_reference_scene_result(scene_path, diagnostics)
        return true
    end
    state.using_reference_scene = false
    print("[VSE-Demo][15.8] startup_scene_failed path=" .. scene_path .. " reason=" .. tostring(diagnostics))
    return false
end


local demo_config = {

    title = "DSEngine Lua Demo 15.8",
    intro_lines = {
        "[VSE-Demo][15.8] 启动 Lua 对齐版 demo。",
        "[VSE-Demo][15.8] 当前阶段优先加载 reference scene，并保留自由相机观察。",
        "[VSE-Demo][15.8] 参考 scene: assets/scenes/reference_demo_15_8.scene.json",
        "[VSE-Demo][15.8] 观察建议：使用 W/A/S/D 与鼠标查看当前构图和灯光基线。",
        "[VSE-Demo][15.8] 当前已接入 Monster / OceanPlane cooked 资产。",
        "[VSE-Demo][15.8] 天空盒资源仍待补齐，现阶段使用 SkyLight 作为最小环境光替代。"
    },

    camera = {
        x = 0.0,
        y = 6.0,
        z = 18.0,
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
    ground = {
        x = 0.0,
        y = -2.0,
        z = 0.0,
        sx = 40.0,
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
            x = 0.0,
            y = 0.0,
            z = 0.0,
            sx = 1.2,
            sy = 3.0,
            sz = 1.0,
            color = { 0.92, 0.94, 1.0, 1.0 },
            material = {
                metallic = 0.05,
                roughness = 0.65,
                ao = 1.0,
                emissive_strength = 0.03,
                emissive_b = 0.18
            }
        },
        {
            x = -4.0,
            y = 0.6,
            z = 2.5,
            sx = 0.8,
            sy = 0.8,
            sz = 0.8,
            color = { 0.55, 0.62, 0.78, 1.0 },
            material = {
                metallic = 0.02,
                roughness = 0.72,
                ao = 1.0,
                emissive_strength = 0.02,
                emissive_b = 0.08
            }
        },
        {
            x = 4.0,
            y = 0.4,
            z = -2.0,
            sx = 0.7,
            sy = 0.7,
            sz = 0.7,
            color = { 0.78, 0.62, 0.55, 1.0 },
            material = {
                metallic = 0.03,
                roughness = 0.78,
                ao = 1.0,
                emissive_strength = 0.01,
                emissive_r = 0.05
            }
        }
    }
}

function Demo158.Setup(config)
    local merged = demo_config
    if type(config) == "table" then
        if type(config.title) == "string" and config.title ~= "" then
            merged.title = config.title
        end
    end
    if not try_load_reference_scene("assets/scenes/reference_demo_15_8.scene.json") then
        Bootstrap.SetupScene(merged)
        print("[VSE-Demo][15.8] fallback_scene_active programmatic_preview")
        print("[VSE-Demo][15.8] observer_hint free_camera=W/A/S/D + mouse")
    end
end



function Demo158.Update(delta_time)
end

return Demo158
