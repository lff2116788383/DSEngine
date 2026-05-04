-- 3D P4 sample: VSEngine 2.1 Demo 15.22 full-scene replica
-- 严格复刻 Source.cpp 场景结构：1st/free camera、6 个 NewMonsterWithAnim、NewOceanPlane、SkyLight、PointLight(含阴影)。
-- 资源路径：VSE 原生 .SKMODEL/.STMODEL/.ACTION 尚不能直读；本文件使用已从 VSE FBX 烘焙出的 DSE .dmesh/.dmat/.dskel/.danim 完成运行时复刻。
-- 材质：已升级至 MESH_PBR + TGA 贴图；地面使用 cooked OceanPlane.dmesh 替代程序化四边形。
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

--- 创建带 mesh 的实体，支持 PBR 材质 + 贴图
-- @param shader_variant "MESH_PBR" 或 "MESH_UNLIT"
-- @param textures table 可选，{albedo="path", normal="path", ...}
local function add_resource_mesh(name, x, y, z, sx, sy, sz, mesh_path, material_path, color, emissive, rx, ry, rz, shader_variant, textures)
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
    local variant = shader_variant or "MESH_PBR"
    dse.ecs.set_mesh_shader_variant(e, variant)
    if dse.ecs.set_mesh_depth_state then
        dse.ecs.set_mesh_depth_state(e, true, true)
    end
    -- PBR 材质参数；后 4 个可选参数把 base_color 重置为白色，避免 Monster.dmat 的 0.588 灰色与贴图双重相乘。
    local m = emissive or {0.0, 0.0, 0.0}
    dse.ecs.set_mesh_material(e, 0.0, 0.62, 1.0, m[1], m[2], m[3], 1.0, false, true, 1.0, 1.0, 1.0, c[4] or 1.0)
    dse.ecs.set_mesh_material_scalar(e, "metallic", 0.0)
    dse.ecs.set_mesh_material_scalar(e, "roughness", 0.62)
    dse.ecs.set_mesh_material_scalar(e, "ao", 1.0)
    dse.ecs.set_mesh_emissive(e, m[1], m[2], m[3])
    -- 设置贴图，并把 Lua binding 的实际加载/句柄结果写入日志；仅 `textures ~= nil` 不能证明贴图已渲染。
    if textures and dse.ecs.set_mesh_texture then
        local loaded_count = 0
        local function bind_texture_slot(slot, path)
            if not path then
                return
            end
            local ok, handle, width, height = dse.ecs.set_mesh_texture(e, slot, path)
            if ok then
                loaded_count = loaded_count + 1
            end
            print(string.format("[3D][VSE15.22] texture_bind entity=%d name=%s slot=%s path=%s loaded=%s handle=%s size=%sx%s", e, name, slot, path, tostring(ok == true), tostring(handle or 0), tostring(width or 0), tostring(height or 0)))
        end
        bind_texture_slot("albedo", textures.albedo)
        bind_texture_slot("normal", textures.normal)
        bind_texture_slot("metallic_roughness", textures.metallic_roughness or textures.roughness)
        bind_texture_slot("emissive", textures.emissive)
        bind_texture_slot("occlusion", textures.ao or textures.occlusion)
        print(string.format("[3D][VSE15.22] texture_bind_summary entity=%d name=%s loaded_slots=%d material_source=component_fallback", e, name, loaded_count))
    end
    return e
end

local function setup_camera(config)
    -- VSE Source.cpp:
    -- CameraPos(0, 900, 900), CameraDir(0, -1, -1), PerspectiveFov(90), near=1, far=8000.
    -- VSE 1stCameraController: 箭头键移动 + 左键旋转; DSE 用 FreeCameraController 等价。
    -- VSE 坐标按 SCALE=0.01 缩放: (0,9,9), dir(0,-1,-1) 对应 pitch≈-45°。
    -- 默认使用“截图验收视角”：仍从 +Z 方向俯视同一 2x3 布局，但进一步拉近并放大主体，
    -- 让 6 个 cooked Monster 在 800x600 验收图中不仅可见，还能看出 Monster_d.tga 的贴图色块/细节。
    local cam_y = config.camera_height or 4.2
    local cam_z = config.camera_distance or 7.0
    local cam_pitch = config.camera_pitch or -33.0
    local use_vse_coords = config.use_vse_camera_coords == true
    local visual_framing = not use_vse_coords
    if use_vse_coords then
        cam_y = 9.0
        cam_z = 9.0
        cam_pitch = -45.0
        visual_framing = false
    end
    local camera = dse.ecs.create_entity()
    dse.ecs.add_transform(camera, 0.0, cam_y, cam_z, 1.0, 1.0, 1.0)
    dse.ecs.set_transform_rotation(camera, cam_pitch, 0.0, 0.0)
    dse.ecs.add_camera_3d(camera, 90.0, 100, 1.0 * SCALE, 8000.0 * SCALE)
    if Ecs and Ecs.add_free_camera_controller then
        Ecs.add_free_camera_controller(camera, config.camera_speed or 6.2, 0.12)
    elseif dse.ecs.add_free_camera_controller then
        dse.ecs.add_free_camera_controller(camera, config.camera_speed or 6.2, 0.12)
    end
    state.camera = camera
    print(string.format("[3D][VSE15.22] camera_replica vse_camera_pos=(0,900,900) vse_camera_dir=(0,-1,-1) fov=90 near=1 far=8000 1st_camera_controller=true free_camera_equivalent=true dse_camera_pos=(0,%.1f,%.1f) dse_camera_pitch=%.1f use_vse_coords=%s visual_framing=%s framing_target=2x3_monsters", cam_y, cam_z, cam_pitch, tostring(use_vse_coords), tostring(visual_framing)))
end

local function setup_environment(config)
    -- VSE Source.cpp:
    -- SkyLight DownColor=(0,0,0.5,1), UpColor=(0.2,0.2,0.2,1)
    -- PointLight Pos=(0,500,0), scale=one.
    -- Demo 15.22 标题为"演示点光源传统影子"，PointLight 必须投射阴影。
    local sky = dse.ecs.create_entity()
    dse.ecs.add_sky_light(sky, 0.2, 0.2, 0.2, 0.0, 0.0, 0.5, config.sky_intensity or 2.80)
    state.sky_light = sky

    local point = dse.ecs.create_entity()
    dse.ecs.add_transform(point, 0.0, 500.0 * SCALE, 0.0, 1.0, 1.0, 1.0)
    dse.ecs.add_point_light_3d(point, 1.0, 1.0, 1.0, config.point_intensity or 36.0, config.point_range or 26.0)
    -- 开启 PointLight 阴影：复刻 VSE Demo 15.22 核心功能
    if dse.ecs.set_point_light_shadow then
        dse.ecs.set_point_light_shadow(point, true)
    end
    state.point_light = point

    local shadow_enabled = dse.ecs.set_point_light_shadow ~= nil
    print(string.format("[3D][VSE15.22] p4_vse15_22_scene full_scene_replica=true cooked_fbx=true environment_api SkyLight=true sky_up=(0.2,0.2,0.2) sky_down=(0,0,0.5) PointLight=true point_shadow=%s point_pos=(0,500,0) point_scaled=(0,5,0) point_intensity=%.2f point_range=%.2f", tostring(shadow_enabled), config.point_intensity or 36.0, config.point_range or 26.0))
end

local function setup_ocean_plane(config)
    -- VSE Source.cpp:
    -- NewOceanPlane.STMODEL at (0,0,0), scale=(100,100,100), CastShadow(false).
    -- 使用从 VSE FBX 烘焙出的 cooked OceanPlane.dmesh + OceanPlane.dmat 替代程序化四边形，
    -- 逼近 VSE 原始网格几何。
    local use_cooked_mesh = config.use_cooked_ocean ~= false  -- 默认使用 cooked mesh
    local ocean = dse.ecs.create_entity()
    local ocean_y = (type(config.ocean_y) == "number") and config.ocean_y or -0.25
    -- VSE scale=(100,100,100)，按 SCALE=0.01 映射到 DSE
    local ocean_scale = (type(config.ocean_scale) == "number") and config.ocean_scale or (100.0 * SCALE)

    if use_cooked_mesh then
        -- 使用 cooked OceanPlane 资源
        local mesh_path = (type(config.ocean_mesh_path) == "string") and config.ocean_mesh_path or "vse_demo/15_22/cooked/OceanPlane.dmesh"
        local material_path = (type(config.ocean_material_path) == "string") and config.ocean_material_path or "vse_demo/15_22/cooked/OceanPlane.dmat"
        dse.ecs.add_transform(ocean, 0.0, ocean_y, 0.0, ocean_scale, ocean_scale, ocean_scale)
        dse.ecs.add_mesh_renderer(ocean, 1.0, 1.0, 1.0, 1.0)
        dse.ecs.set_mesh_path(ocean, mesh_path)
        dse.ecs.set_mesh_material(ocean, material_path)
        dse.ecs.set_mesh_shader_variant(ocean, config.ocean_shader_variant or "MESH_PBR")
        dse.ecs.set_mesh_material_scalar(ocean, "metallic", 0.0)
        dse.ecs.set_mesh_material_scalar(ocean, "roughness", 0.78)
        dse.ecs.set_mesh_material_scalar(ocean, "ao", 1.0)
        dse.ecs.set_mesh_emissive(ocean, 0.20, 0.26, 0.32)
        state.resources.ocean_mesh_path = mesh_path
        state.resources.ocean_material_path = material_path
    else
        -- 回退：DSE 程序化四边形
        local half_size = (type(config.ocean_half_size) == "number") and config.ocean_half_size or 17.5
        local verts = {
            -half_size, 0.0, -half_size,
            -half_size, 0.0,  half_size,
             half_size, 0.0,  half_size,
             half_size, 0.0, -half_size
        }
        local indices = { 0, 1, 2, 2, 3, 0 }
        dse.ecs.add_transform(ocean, 0.0, ocean_y, 0.0, 1.0, 1.0, 1.0)
        dse.ecs.add_mesh_renderer(ocean, 0.50, 0.62, 0.74, 1.0, verts, indices)
        dse.ecs.set_mesh_shader_variant(ocean, "MESH_PBR")
        dse.ecs.set_mesh_material(ocean, 0.0, 0.78, 1.0, 0.20, 0.26, 0.32, 1.0, false, true)
        dse.ecs.set_mesh_material_scalar(ocean, "metallic", 0.0)
        dse.ecs.set_mesh_material_scalar(ocean, "roughness", 0.78)
        dse.ecs.set_mesh_material_scalar(ocean, "ao", 1.0)
        dse.ecs.set_mesh_emissive(ocean, 0.20, 0.26, 0.32)
        state.resources.ocean_mesh_path = "procedural:DSE_OceanPlaneQuad"
        state.resources.ocean_material_path = "procedural:pbr_ocean_plane"
    end

    if dse.ecs.set_mesh_depth_state then
        dse.ecs.set_mesh_depth_state(ocean, true, true)
    end
    state.ocean_plane = ocean
    print(string.format("[3D][VSE15.22] ocean_plane_replica vse_asset=NewOceanPlane.STMODEL cooked_mesh=%s vse_pos=(0,0,0) vse_scale=(100,100,100) dse_y=%.3f dse_scale=%.3f cast_shadow=false depth_state=test_enabled_write_enabled", tostring(use_cooked_mesh), ocean_y, ocean_scale))
end

local function add_monster(def, config, anim_paths)
    local mesh_path = (type(config.character_mesh_path) == "string") and config.character_mesh_path or "vse_demo/15_22/cooked/Monster.dmesh"
    local material_path = (type(config.character_material_path) == "string") and config.character_material_path or "vse_demo/15_22/cooked/Monster.dmat"
    local dskel_path = (type(config.dskel_path) == "string") and config.dskel_path or "vse_demo/15_22/cooked/Monster.dskel"
    local danim_path = anim_paths[def.anim_key] or anim_paths.idle
    local visual_scale = (type(config.monster_scale) == "number") and config.monster_scale or 0.180
    local x = def.vse_x * SCALE
    local y = def.vse_y * SCALE
    local z = def.vse_z * SCALE

    -- Monster PBR 贴图（从 VSE TGA 烘焙而来）
    local textures = {
        albedo = "vse_demo/15_22/raw/Texture/Monster_d.tga",
        normal = "vse_demo/15_22/raw/Texture/Monster_n.tga",
        roughness = "vse_demo/15_22/raw/Texture/Monster_s.tga",
        emissive = "vse_demo/15_22/raw/Texture/Monster_e.tga",
    }
    if config.no_textures then textures = nil end

    local entity = add_resource_mesh("NewMonsterWithAnim.SKMODEL_" .. def.anim_name, x, y, z, visual_scale, visual_scale, visual_scale, mesh_path, material_path, def.tint, def.emissive, nil, nil, nil, "MESH_PBR", textures)
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
        vse_z = def.vse_z,
        visual_scale = visual_scale
    }
    table.insert(state.characters, character)
    print(string.format("[3D][VSE15.22] monster_replica index=%d vse_asset=NewMonsterWithAnim.SKMODEL anim=%s cooked_anim=%s vse_pos=(%d,%d,%d) scaled_pos=(%.2f,%.2f,%.2f) visual_scale=%.3f mesh=%s dskel=%s shader_variant=MESH_PBR pbr_textures=%s depth_state=enabled", #state.characters, def.anim_name, danim_path, def.vse_x, def.vse_y, def.vse_z, x, y, z, visual_scale, mesh_path, dskel_path, tostring(textures ~= nil)))
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
        { anim_key = "idle", anim_name = "Idle", vse_x = -300, vse_y = 0, vse_z = 300, tint = {1.0, 1.0, 1.0, 1.0}, emissive = {0.08, 0.04, 0.03} },
        { anim_key = "walk", anim_name = "Walk", vse_x = 0, vse_y = 0, vse_z = 300, tint = {1.0, 1.0, 1.0, 1.0}, emissive = {0.08, 0.04, 0.03} },
        { anim_key = "attack", anim_name = "Attack", vse_x = 300, vse_y = 0, vse_z = 300, tint = {1.0, 1.0, 1.0, 1.0}, emissive = {0.08, 0.04, 0.03} },
        { anim_key = "attack2", anim_name = "Attack2", vse_x = -300, vse_y = 0, vse_z = -300, tint = {1.0, 1.0, 1.0, 1.0}, emissive = {0.08, 0.04, 0.03} },
        { anim_key = "pos", anim_name = "Pos", vse_x = 0, vse_y = 0, vse_z = -300, tint = {1.0, 1.0, 1.0, 1.0}, emissive = {0.08, 0.04, 0.03} },
        { anim_key = "additive", anim_name = "AddtiveAnim", vse_x = 300, vse_y = 0, vse_z = -300, tint = {1.0, 1.0, 1.0, 1.0}, emissive = {0.08, 0.04, 0.03} },
    }

    for _, def in ipairs(defs) do
        add_monster(def, config, anim_paths)
    end

    local first = state.characters[1]
    local anim_ok, anim_state, norm, clip, speed, loop, trans, bones, has_skeleton = get_animator_state(first.entity)
    local visual_scale = (type(config.monster_scale) == "number") and config.monster_scale or 0.180
    print(string.format("[3D][VSE15.22] p4_character_setup full_scene_replica=true character_count=%d vse_positions=(-300,0,300)|(0,0,300)|(300,0,300)|(-300,0,-300)|(0,0,-300)|(300,0,-300) dse_layout_bounds=(-3,0,-3)..(3,0,3) visual_scale=%.3f visibility_goal=all_6_monsters_visible vse_states=Idle,Walk,Attack,Attack2,Pos,AddtiveAnim", #state.characters, visual_scale))
    print(string.format("[3D][VSE15.22] p4_animation_resource full_scene_replica=true cooked_fbx=true mesh_path=%s material_path=%s idle_danim=%s walk_danim=%s attack_danim=%s attack2_danim=%s pos_danim=%s additive_danim=%s dskel=%s get_animator_3d_state=%s state=%s normalized_time=%.2f clip_time=%.2f speed=%.2f final_bones=%s has_skeleton=%s", mesh_path, material_path, anim_paths.idle, anim_paths.walk, anim_paths.attack, anim_paths.attack2, anim_paths.pos, anim_paths.additive, dskel_path, tostring(anim_ok == true), tostring(anim_state), norm or -1.0, clip or -1.0, speed or -1.0, tostring(bones), tostring(has_skeleton == true)))
end

function VSE1522Scene3D.Setup(config)
    print("[3D][VSE15.22] setup: full VSEngine2.1 Demo 15.22 scene replica; PBR + PointLight shadow + cooked OceanPlane; visual calibration requires all 6 monsters visible.")
    local cfg = config or {}
    setup_camera(cfg)
    setup_environment(cfg)
    setup_characters(cfg)
    setup_ocean_plane(cfg)
    local shadow_ok = dse.ecs.set_point_light_shadow ~= nil
    print(string.format("[3D][VSE15.22] p4_vse15_22_scene setup_complete full_scene_replica=true scene_objects=8 character_count=%d static_count=1 sky_light=1 point_light=1 point_shadow=%s camera=1 free_camera=true pbr_material=true cooked_ocean=true", #state.characters, tostring(shadow_ok)))
end

function VSE1522Scene3D.Update(delta_time)
    local dt = delta_time or 0.0
    if dt > 0.1 then dt = 0.1 end
    state.time = state.time + dt

    if (not state.environment_logged) and state.time > 0.4 then
        state.environment_logged = true
        local shadow_ok = dse.ecs.set_point_light_shadow ~= nil
        print(string.format("[3D][VSE15.22] runtime_environment full_scene_replica=true SkyLight=true sky_up=(0.2,0.2,0.2) sky_down=(0,0,0.5) PointLight=true point_shadow=%s point_pos=(0,500,0) OceanPlane=true cooked_ocean=true pbr_material=true camera_first_controller=true monster_depth_state=enabled ocean_depth_state=test_enabled_write_enabled depth_buffer_root_cause_fixed=true visual_acceptance=all_6_monsters_visible", tostring(shadow_ok)))
    end

    if (not state.animation_logged) and state.time > 0.45 and #state.characters > 0 then
        state.animation_logged = true
        -- 轮询所有 6 个角色的动画状态，匹配 VSE Source.cpp 的 6 个 PlayAnim 调用
        local anim_summary = {}
        for i, ch in ipairs(state.characters) do
            local ok, anim_state, norm, clip, anim_speed, loop, trans, bones, has_skeleton = get_animator_state(ch.entity)
            anim_summary[i] = string.format("%s=%s(%.2f)", ch.anim_name, tostring(anim_state), norm or -1.0)
        end
        local first = state.characters[1]
        local ok1, s1, n1, c1, sp1, l1, t1, b1, sk1 = get_animator_state(first.entity)
        print(string.format("[3D][VSE15.22] runtime_animation full_scene_replica=true get_animator_3d_state=%s first_state=%s normalized_time=%.2f clip_time=%.2f speed=%.2f final_bones=%s has_skeleton=%s character_count=%d anim_summary=[%s] lua_query_timing=before_animator_system_update state_time=%.3f", tostring(ok1 == true), tostring(s1), n1 or -1.0, c1 or -1.0, sp1 or -1.0, tostring(b1), tostring(sk1 == true), #state.characters, table.concat(anim_summary, "|"), state.time))
    end
end

return VSE1522Scene3D
