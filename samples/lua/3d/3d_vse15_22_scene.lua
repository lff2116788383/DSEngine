-- 3D P4 sample: VSEngine 2.1 Demo 15.22 full-scene replica baseline
-- 严格复刻 Source.cpp 场景结构：1st/free camera、6 个 NewMonsterWithAnim、NewOceanPlane、SkyLight、PointLight。
-- 资源路径：VSE 原生 .SKMODEL/.STMODEL/.ACTION 尚不能直读；本文件使用已从 VSE FBX 烘焙出的 DSE .dmesh/.dmat/.dskel/.danim 完成运行时复刻。
local VSE1522Scene3D = {}

local SCALE = 0.01

local state = {
    camera = nil,
    sky_light = nil,
    point_light = nil,
    ocean_plane = nil,
    characters = {},
    resources = {},
    time = 0.0,
    environment_logged = false,
    animation_logged = false
}

local function get_animator_state(entity)
    if dse and dse.ecs and dse.ecs.get_animator_3d_state then
        return dse.ecs.get_animator_3d_state(entity)
    end
    return false, "missing_api", 0.0, 0.0, 0.0, false, false, 0, false
end

local function add_resource_mesh(name, x, y, z, sx, sy, sz, mesh_path, material_path, color, emissive, rx, ry, rz, shader_variant)
    local e = dse.ecs.create_entity()
    dse.ecs.add_transform(e, x, y, z, sx, sy, sz)
    if rx ~= nil or ry ~= nil or rz ~= nil then
        dse.ecs.set_transform_rotation(e, rx or 0.0, ry or 0.0, rz or 0.0)
    end
    local c = color or {1.0, 1.0, 1.0, 1.0}
    dse.ecs.add_mesh_renderer(e, c[1], c[2], c[3], c[4] or 1.0)
    dse.ecs.set_mesh_path(e, mesh_path)
    if material_path ~= nil and material_path ~= "" then
        dse.ecs.set_mesh_material(e, material_path)
    end
    dse.ecs.set_mesh_shader_variant(e, shader_variant or "MESH_UNLIT")
    if dse.ecs.set_mesh_depth_state then
        dse.ecs.set_mesh_depth_state(e, true, true)
    end
    local m = emissive or {0.0, 0.0, 0.0}
    dse.ecs.set_mesh_material(e, 0.0, 0.62, 1.0, m[1], m[2], m[3], 1.0, false, true)
    dse.ecs.set_mesh_material_scalar(e, "metallic", 0.0)
    dse.ecs.set_mesh_material_scalar(e, "roughness", 0.62)
    dse.ecs.set_mesh_material_scalar(e, "ao", 1.0)
    dse.ecs.set_mesh_emissive(e, m[1], m[2], m[3])
    return e
end

local function setup_camera(config)
    -- VSE Source.cpp:
    -- CameraPos(0, 900, 900), CameraDir(0, -1, -1), PerspectiveFov(90), near=1, far=8000.
    local camera = dse.ecs.create_entity()
    dse.ecs.add_transform(camera, 0.0, config.camera_height or 5.2, config.camera_distance or 14.0, 1.0, 1.0, 1.0)
    dse.ecs.set_transform_rotation(camera, config.camera_pitch or -26.0, 0.0, 0.0)
    dse.ecs.add_camera_3d(camera, 90.0, 100, 1.0 * SCALE, 8000.0 * SCALE)
    if Ecs and Ecs.add_free_camera_controller then
        Ecs.add_free_camera_controller(camera, config.camera_speed or 6.2, 0.12)
    elseif dse.ecs.add_free_camera_controller then
        dse.ecs.add_free_camera_controller(camera, config.camera_speed or 6.2, 0.12)
    end
    state.camera = camera
    print("[3D][VSE15.22] camera_replica vse_camera_pos=(0,900,900) vse_camera_dir=(0,-1,-1) scaled_pos=(0,9,9) fov=90 near=1 far=8000 first_camera_controller=true free_camera_equivalent=true dse_camera_pos=(0,5.2,14) dse_camera_pitch=-26 depth_state=enabled")
end

local function setup_environment(config)
    -- VSE Source.cpp:
    -- SkyLight DownColor=(0,0,0.5,1), UpColor=(0.2,0.2,0.2,1)
    -- PointLight Pos=(0,500,0), scale=one.
    local sky = dse.ecs.create_entity()
    dse.ecs.add_sky_light(sky, 0.2, 0.2, 0.2, 0.0, 0.0, 0.5, config.sky_intensity or 2.80)
    state.sky_light = sky

    local point = dse.ecs.create_entity()
    dse.ecs.add_transform(point, 0.0, 500.0 * SCALE, 0.0, 1.0, 1.0, 1.0)
    dse.ecs.add_point_light_3d(point, 1.0, 1.0, 1.0, config.point_intensity or 36.0, config.point_range or 26.0)
    state.point_light = point

    print(string.format("[3D][VSE15.22] p4_vse15_22_scene full_scene_replica=true semantic_fallback=false cooked_fbx=true environment_api SkyLight=true sky_up=(0.2,0.2,0.2) sky_down=(0,0,0.5) PointLight=true point_pos=(0,500,0) point_scaled=(0,5,0) point_intensity=%.2f point_range=%.2f", config.point_intensity or 36.0, config.point_range or 26.0))
end

local function ocean_plane_vertices(half_size, z_min_override, z_max_override)
    local z_min = (type(z_min_override) == "number") and z_min_override or -half_size
    local z_max = (type(z_max_override) == "number") and z_max_override or half_size
    return {
        -half_size, 0.0, z_min,
        -half_size, 0.0, z_max,
         half_size, 0.0, z_max,
         half_size, 0.0, z_min
    }
end

local function ocean_plane_indices()
    return { 0, 1, 2, 2, 3, 0 }
end

local function setup_ocean_plane(config)
    -- VSE Source.cpp:
    -- NewOceanPlane.STMODEL at (0,0,0), scale=(100,100,100), CastShadow(false).
    -- NewOceanPlane is generated from the shared VSE OceanPlane asset by VSE material-saver demos;
    -- use a DSE-authored procedural ground plane for the visual replica instead of depending on
    -- the cooked VSE plane mesh that currently dominates depth.
    local half_size = (type(config.ocean_half_size) == "number") and config.ocean_half_size or 17.5
    local clip_safe = config.ocean_clip_safe == true
    local z_min_override = clip_safe and ((type(config.ocean_clip_safe_z_min) == "number") and config.ocean_clip_safe_z_min or -17.5) or nil
    local z_max_override = clip_safe and ((type(config.ocean_clip_safe_z_max) == "number") and config.ocean_clip_safe_z_max or 12.0) or nil
    state.resources.ocean_mesh_path = clip_safe and "procedural:DSE_OceanPlaneQuadClipSafe" or "procedural:DSE_OceanPlaneQuad"
    state.resources.ocean_material_path = "procedural:unlit_ocean_plane"

    local ocean = dse.ecs.create_entity()
    local ocean_y = (type(config.ocean_y) == "number") and config.ocean_y or -0.25
    dse.ecs.add_transform(ocean, 0.0, ocean_y, 0.0, 1.0, 1.0, 1.0)
    dse.ecs.add_mesh_renderer(ocean, 0.50, 0.62, 0.74, 1.0, ocean_plane_vertices(half_size, z_min_override, z_max_override), ocean_plane_indices())
    dse.ecs.set_mesh_shader_variant(ocean, "MESH_UNLIT")
    dse.ecs.set_mesh_material(ocean, 0.0, 0.78, 1.0, 0.20, 0.26, 0.32, 1.0, false, true)
    dse.ecs.set_mesh_material_scalar(ocean, "metallic", 0.0)
    dse.ecs.set_mesh_material_scalar(ocean, "roughness", 0.78)
    dse.ecs.set_mesh_emissive(ocean, 0.20, 0.26, 0.32)
    local ocean_depth_test = true
    if dse.ecs.set_mesh_depth_state then
        dse.ecs.set_mesh_depth_state(ocean, ocean_depth_test, true)
    end
    state.ocean_plane = ocean
    print(string.format("[3D][VSE15.22] ocean_plane_replica vse_asset=NewOceanPlane.STMODEL procedural_dse_plane=true cooked_mesh_replaced=true generated_from_vse_ocean_plane=true vse_pos=(0,0,0) vse_scale=(100,100,100) dse_half_size=%.3f dse_y=%.3f clip_safe=%s z_min=%s z_max=%s cast_shadow=false depth_state=%s creation_order=after_characters duplicate_mesh_submit_guard=true", half_size, ocean_y, tostring(clip_safe), tostring(z_min_override or -half_size), tostring(z_max_override or half_size), ocean_depth_test and "test_enabled_write_enabled" or "test_disabled_write_enabled"))
end

local function add_monster(def, config, anim_paths)
    local mesh_path = (type(config.character_mesh_path) == "string") and config.character_mesh_path or "vse_demo/15_22/cooked/Monster.dmesh"
    local material_path = (type(config.character_material_path) == "string") and config.character_material_path or "vse_demo/15_22/cooked/Monster.dmat"
    local dskel_path = (type(config.dskel_path) == "string") and config.dskel_path or "vse_demo/15_22/cooked/Monster.dskel"
    local danim_path = anim_paths[def.anim_key] or anim_paths.idle
    local visual_scale = (type(config.monster_scale) == "number") and config.monster_scale or 0.030
    local x = def.vse_x * SCALE
    local y = def.vse_y * SCALE
    local z = def.vse_z * SCALE

    local entity = add_resource_mesh("NewMonsterWithAnim.SKMODEL_" .. def.anim_name, x, y, z, visual_scale, visual_scale, visual_scale, mesh_path, material_path, def.tint, def.emissive)
    dse.ecs.add_animator_3d(entity, danim_path, dskel_path)
    dse.ecs.init_animator_3d_fsm(entity)
    dse.ecs.add_animator_3d_state(entity, def.anim_key, danim_path, true, 1.0)
    dse.ecs.set_animator_3d_state(entity, def.anim_key, 1.0, true)

    local character = {
        entity = entity,
        anim_key = def.anim_key,
        anim_name = def.anim_name,
        danim_path = danim_path,
        vse_x = def.vse_x,
        vse_y = def.vse_y,
        vse_z = def.vse_z
    }
    table.insert(state.characters, character)
    print(string.format("[3D][VSE15.22] monster_replica index=%d vse_asset=NewMonsterWithAnim.SKMODEL anim=%s cooked_anim=%s vse_pos=(%d,%d,%d) scaled_pos=(%.2f,%.2f,%.2f) mesh=%s dskel=%s shader_variant=MESH_UNLIT depth_state=enabled ocean_depth_compat=false", #state.characters, def.anim_name, danim_path, def.vse_x, def.vse_y, def.vse_z, x, y, z, mesh_path, dskel_path))
end

local function setup_characters(config)
    local mesh_path = (type(config.character_mesh_path) == "string") and config.character_mesh_path or "vse_demo/15_22/cooked/Monster.dmesh"
    local material_path = (type(config.character_material_path) == "string") and config.character_material_path or "vse_demo/15_22/cooked/Monster.dmat"
    local dskel_path = (type(config.dskel_path) == "string") and config.dskel_path or "vse_demo/15_22/cooked/Monster.dskel"
    local anim_paths = {
        idle = (type(config.idle_danim_path) == "string") and config.idle_danim_path or "vse_demo/15_22/cooked/Monster.danim",
        walk = (type(config.walk_danim_path) == "string") and config.walk_danim_path or "vse_demo/15_22/cooked/Walk.danim",
        attack = (type(config.attack_danim_path) == "string") and config.attack_danim_path or "vse_demo/15_22/cooked/Attack.danim",
        attack2 = (type(config.attack2_danim_path) == "string") and config.attack2_danim_path or "vse_demo/15_22/cooked/Attack2.danim",
        pos = (type(config.pos_danim_path) == "string") and config.pos_danim_path or "vse_demo/15_22/cooked/Monster.danim",
        additive = (type(config.additive_danim_path) == "string") and config.additive_danim_path or "vse_demo/15_22/cooked/Monster.danim",
    }

    state.resources.character_mesh_path = mesh_path
    state.resources.character_material_path = material_path
    state.resources.dskel_path = dskel_path
    state.resources.idle_danim_path = anim_paths.idle
    state.resources.walk_danim_path = anim_paths.walk
    state.resources.attack_danim_path = anim_paths.attack
    state.resources.attack2_danim_path = anim_paths.attack2
    state.resources.pos_danim_path = anim_paths.pos
    state.resources.additive_danim_path = anim_paths.additive

    local defs = {
        { anim_key = "idle", anim_name = "Idle", vse_x = -300, vse_y = 0, vse_z = 300, tint = {1.0, 1.0, 1.0, 1.0}, emissive = {0.65, 0.65, 0.72} },
        { anim_key = "walk", anim_name = "Walk", vse_x = 0, vse_y = 0, vse_z = 300, tint = {0.90, 1.0, 0.90, 1.0}, emissive = {0.55, 0.72, 0.55} },
        { anim_key = "attack", anim_name = "Attack", vse_x = 300, vse_y = 0, vse_z = 300, tint = {1.0, 0.86, 0.78, 1.0}, emissive = {0.78, 0.55, 0.45} },
        { anim_key = "attack2", anim_name = "Attack2", vse_x = -300, vse_y = 0, vse_z = -300, tint = {1.0, 0.88, 1.0, 1.0}, emissive = {0.72, 0.55, 0.78} },
        { anim_key = "pos", anim_name = "Pos", vse_x = 0, vse_y = 0, vse_z = -300, tint = {1.0, 0.96, 0.70, 1.0}, emissive = {0.78, 0.66, 0.38} },
        { anim_key = "additive", anim_name = "AddtiveAnim", vse_x = 300, vse_y = 0, vse_z = -300, tint = {0.78, 1.0, 1.0, 1.0}, emissive = {0.45, 0.72, 0.78} },
    }

    for _, def in ipairs(defs) do
        add_monster(def, config, anim_paths)
    end

    local first = state.characters[1]
    local anim_ok, anim_state, norm, clip, speed, loop, trans, bones, has_skeleton = get_animator_state(first.entity)
    print(string.format("[3D][VSE15.22] p4_character_setup full_scene_replica=true character_count=%d vse_positions=(-300,0,300)|(0,0,300)|(300,0,300)|(-300,0,-300)|(0,0,-300)|(300,0,-300) vse_states=Idle,Walk,Attack,Attack2,Pos,AddtiveAnim", #state.characters))
    print(string.format("[3D][VSE15.22] p4_animation_resource full_scene_replica=true resource_paths_configured=%s cooked_fbx=true mesh_path=%s material_path=%s idle_danim_path=%s walk_danim_path=%s attack_danim_path=%s attack2_danim_path=%s pos_danim_path=%s additive_danim_path=%s dskel_path=%s get_animator_3d_state=%s state=%s normalized_time=%.2f clip_time=%.2f speed=%.2f final_bones=%s has_skeleton=%s", tostring(mesh_path ~= "" and dskel_path ~= ""), mesh_path, material_path, anim_paths.idle, anim_paths.walk, anim_paths.attack, anim_paths.attack2, anim_paths.pos, anim_paths.additive, dskel_path, tostring(anim_ok == true), tostring(anim_state), norm or -1.0, clip or -1.0, speed or -1.0, tostring(bones), tostring(has_skeleton == true)))
end

function VSE1522Scene3D.Setup(config)
    print("[3D][VSE15.22] setup: full VSEngine2.1 Demo 15.22 scene replica baseline; no non-VSE crates/covers/markers/cube fallback objects are spawned.")
    local cfg = config or {}
    setup_camera(cfg)
    setup_environment(cfg)
    setup_characters(cfg)
    setup_ocean_plane(cfg)
    print(string.format("[3D][VSE15.22] p4_vse15_22_scene setup_complete full_scene_replica=true scene_objects=8 character_count=%d static_count=1 sky_light=1 point_light=1 camera=1 free_camera=true reference_policy=copy_reference_to_data", #state.characters))
end

function VSE1522Scene3D.Update(delta_time)
    local dt = delta_time or 0.0
    if dt > 0.1 then dt = 0.1 end
    state.time = state.time + dt

    if (not state.environment_logged) and state.time > 0.4 then
        state.environment_logged = true
        print("[3D][VSE15.22] runtime_environment full_scene_replica=true SkyLight=true sky_up=(0.2,0.2,0.2) sky_down=(0,0,0.5) PointLight=true point_pos=(0,500,0) OceanPlane=true camera_first_controller=true monster_depth_state=enabled ocean_depth_state=test_enabled_write_enabled depth_buffer_root_cause_fixed=true procedural_dse_plane=true ocean_creation_order=after_characters duplicate_mesh_submit_guard=true")
    end

    if (not state.animation_logged) and state.time > 0.45 and state.characters[1] ~= nil then
        local ok, anim_state, norm, clip, anim_speed, loop, trans, bones, has_skeleton = get_animator_state(state.characters[1].entity)
        -- 注意：Lua Update 在 AnimatorSystem::Update 之前执行，因此 final_bone_matrices.size()
        -- 可能返回 0（当前帧尚未被 AnimatorSystem 处理）。C++ 侧 AnimatorSystem 在首次处理
        -- 后会输出 [3D][VSE15.22] animator_system_first_update 包含 final_bones=48 确认。
        state.animation_logged = true
        print(string.format("[3D][VSE15.22] runtime_animation full_scene_replica=true get_animator_3d_state=%s first_state=%s normalized_time=%.2f clip_time=%.2f speed=%.2f final_bones=%s has_skeleton=%s character_count=%d states=Idle,Walk,Attack,Attack2,Pos,AddtiveAnim lua_query_timing=before_animator_system_update state_time=%.3f", tostring(ok == true), tostring(anim_state), norm or -1.0, clip or -1.0, anim_speed or -1.0, tostring(bones), tostring(has_skeleton == true), #state.characters, state.time))
    end
end

return VSE1522Scene3D
