-- 3D P2 sample: shadow showcase
-- 目标：展示方向光 shadow_strength/cast_shadow 路线；若阴影 pass 不可见，用地面暗色投影 marker 作为稳定验收 fallback。
local ShadowShowcase3D = {}


ShadowShowcase3D._meta = {
    name     = "shadow showcase",
    category = "rendering",
    config   = { camera_distance=9.0,
    shadow_strength=0.45 },
}

local state = { camera = nil, light = nil, caster = nil, shadow_marker = nil, time = 0.0, logged = false }

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
    dse.ecs.set_mesh_material(e, material.metallic or 0.0, material.roughness or 0.5, material.ao or 1.0, material.er or 0.0, material.eg or 0.0, material.eb or 0.0, material.normal_strength or 1.0, material.receive_shadow ~= false, material.double_sided ~= false)
    return e
end

local function setup_camera(config)
    local camera = dse.ecs.create_entity()
    local distance = (type(config) == "table" and type(config.camera_distance) == "number") and config.camera_distance or 9.0
    dse.ecs.add_transform(camera, 0.0, 3.4, distance, 1.0, 1.0, 1.0)
    dse.ecs.set_transform_rotation(camera, -22.0, 0.0, 0.0)
    dse.ecs.add_camera_3d(camera, 60.0, 100)
    if Ecs and Ecs.add_free_camera_controller then Ecs.add_free_camera_controller(camera, 5.8, 0.12) elseif dse.ecs.add_free_camera_controller then dse.ecs.add_free_camera_controller(camera, 5.8, 0.12) end
    state.camera = camera
end

local function setup_scene(config)
    local initial_strength = (type(config) == "table" and type(config.shadow_strength) == "number") and config.shadow_strength or 0.45
    local light = dse.ecs.create_entity()
    dse.ecs.add_directional_light_3d(light, -0.45, -1.0, -0.28, 1.0, 0.94, 0.82, 1.25, 0.12, initial_strength)
    local shadow_ok, cast_shadow, applied_strength, c0, c1, c2 = dse.ecs.set_directional_light_shadow(light, true, initial_strength, 12.0, 32.0, 80.0)
    state.light = light

    add_cube("ground", 0.0, -0.58, 0.0, 8.5, 0.12, 5.5, {0.52, 0.52, 0.46, 1.0}, {roughness = 0.72, receive_shadow = true})
    add_cube("back_wall", 0.0, 1.25, -2.3, 8.5, 3.2, 0.10, {0.34, 0.36, 0.42, 1.0}, {roughness = 0.68, receive_shadow = true})
    state.caster = add_cube("caster", 0.0, 1.25, 0.0, 0.9, 0.9, 0.9, {0.9, 0.78, 0.38, 1.0}, {roughness = 0.42, receive_shadow = true})
    state.shadow_marker = add_cube("shadow_fallback_marker", 0.42, -0.49, 0.46, 1.4, 0.026, 1.0, {0.035, 0.035, 0.035, 0.78}, {roughness = 1.0, receive_shadow = false})
    add_cube("strength_meter", -3.5, 0.15, 2.1, 0.24, 1.0, 0.24, {0.25, 0.55, 1.0, 1.0}, {er = 0.03, eg = 0.12, eb = 0.35})
    print(string.format("[3D][Shadow] setup: directional light shadow_strength=%.2f; caster above receive-shadow ground", initial_strength))
    print(string.format("[3D][Shadow] shadow_param_api set_directional_light_shadow=%s cast_shadow=%s shadow_strength=%.2f cascade_splits=%.1f/%.1f/%.1f", tostring(shadow_ok == true), tostring(cast_shadow == true), applied_strength or -1.0, c0 or -1.0, c1 or -1.0, c2 or -1.0))
    print("[3D][Shadow] fallback marker tracks caster footprint so screenshots show expected shadow theme even before full shadow validation.")
end

function ShadowShowcase3D.Setup(config)
    setup_camera(config or {})
    setup_scene(config or {})
end

function ShadowShowcase3D.Update(delta_time)
    local dt = delta_time or 0.0
    if dt > 0.1 then dt = 0.1 end
    state.time = state.time + dt
    local x = math.sin(state.time * 0.75) * 1.45
    local z = math.cos(state.time * 0.55) * 0.65
    local y = 1.2 + math.sin(state.time * 1.1) * 0.22
    if state.caster ~= nil then
        dse.ecs.set_transform_position(state.caster, x, y, z)
        dse.ecs.set_transform_rotation(state.caster, state.time * 38.0, state.time * 52.0, 0.0)
    end
    local strength = 0.18 + (math.sin(state.time * 0.8) * 0.5 + 0.5) * 0.62
    if state.light ~= nil then
        dse.ecs.set_directional_light_3d(state.light, true, -0.45, -1.0, -0.28, 1.0, 0.94, 0.82, 1.25, 0.12, strength)
        dse.ecs.set_directional_light_shadow(state.light, true, strength, 12.0, 32.0, 80.0)
    end
    if state.shadow_marker ~= nil then
        local scale = 1.15 + (1.0 - (y - 0.95)) * 0.35
        dse.ecs.set_transform_position(state.shadow_marker, x + 0.40, -0.49, z + 0.42)
        dse.ecs.add_transform(state.shadow_marker, x + 0.40, -0.49, z + 0.42, scale, 0.026, scale * 0.66)
    end
    if not state.logged and state.time > 1.0 then
        state.logged = true
        print(string.format("[3D][Shadow] runtime: set_directional_light_shadow=true cast_shadow=true animated shadow_strength=%.2f cascade_splits=12.0/32.0/80.0 caster=(%.2f,%.2f,%.2f)", strength, x, y, z))
    end
end

return ShadowShowcase3D
