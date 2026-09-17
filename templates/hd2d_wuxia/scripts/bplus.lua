-- ============================================================================
-- bplus.lua  HD-2D B+ presentation layer
--
-- Keeps the original 2D gameplay, collision, AI, quest and save data intact and
-- mirrors selected ECS entities into a 3D presentation: a generated .dmesh
-- world, lit Sprite3D characters with .dsprite clip animation, contact
-- shadows, a 3D camera and clustered lights.  Enable with:
--     set DSE_HD2D_BPLUS=1
-- The layer is deliberately optional so all existing M1-M6 acceptance scripts
-- keep working without a 3D asset.
-- ============================================================================
local core = require("core")

local B = {}
B.enabled = (os and os.getenv and ((os.getenv("DSE_HD2D_BPLUS") or "") == "1"))
if not B.enabled then return B end

local A = require("assets")
local ecs = dse.ecs

B.camera = nil
B.terrain = nil
B.lights = {}
B.records = {}
B.fx_meta = {}
B.follow_source = nil
B.camera_offset = { x = 0.0, y = 2.6, z = 6.0, pitch = -18.0 }
B._atlas_cache = {}
B._missing_atlas = {}

B.ACTION_FPS = {
    idle = 8.0, walk = 10.0, attack = 16.0, dodge = 8.0, cast = 9.0,
    hurt = 8.0, die = 8.0, play = 20.0,
}
B.ACTION_LOOP = {
    idle = true, walk = true, attack = false, dodge = false, cast = false,
    hurt = false, die = false, play = false,
}

local function valid(e)
    return (dse.entity_valid and dse.entity_valid(e)) or (e and e ~= 0)
end

local function actor_atlas_path(kind, dir, action)
    if kind == "hero" then
        return "char/hero/hero_" .. dir .. "_" .. action .. "_atlas.dsprite.json"
    elseif kind == "bandit" then
        return "enemy/bandit/bandit_" .. dir .. "_" .. action .. "_atlas.dsprite.json"
    elseif kind == "boss" then
        return "enemy/boss/boss_" .. dir .. "_" .. action .. "_atlas.dsprite.json"
    elseif kind == "wolf" then
        return "enemy/wolf_" .. dir .. "_" .. action .. "_atlas.dsprite.json"
    elseif kind == "ghost" then
        return "enemy/ghost_" .. dir .. "_" .. action .. "_atlas.dsprite.json"
    elseif kind == "villager" or kind == "smith" or kind == "elder" then
        return "npc/" .. kind .. "/" .. kind .. "_" .. dir .. "_idle_atlas.dsprite.json"
    end
    return nil
end

local function fx_atlas_path(kind)
    return "fx/" .. kind .. "_atlas.dsprite.json"
end

local function load_atlas(path)
    if not path then return nil end
    if B._atlas_cache[path] ~= nil then return B._atlas_cache[path] end
    local resolved = core.resolve(path)
    local handle = dse.assets.load_sprite_atlas(resolved)
    if not handle or handle < 0 then
        if not B._missing_atlas[path] then
            B._missing_atlas[path] = true
            print("[bplus] missing sprite atlas: " .. tostring(resolved))
        end
        B._atlas_cache[path] = -1
        return nil
    end
    B._atlas_cache[path] = handle
    return handle
end

local function resolve_actor_atlas(kind, dir, action)
    local dirs = { dir, "r", "d", "l", "u" }
    local actions = { action, "idle", "walk" }
    for _, d in ipairs(dirs) do
        for _, a in ipairs(actions) do
            local h = load_atlas(actor_atlas_path(kind, d, a))
            if h then return h, d, a end
        end
    end
    return nil, dir, action
end

local function mapped_foot(source)
    local x, y, z = ecs.get_transform_position(source)
    local sx, sy, sz = ecs.get_transform_scale(source)
    local foot = y - math.abs(sy or 1.0) * 0.5
    return x, foot, z, sx or 1.0, sy or 1.0, sz or 1.0
end

local function add_proxy(source, w_px, h_px, opts)
    opts = opts or {}
    local x, foot = mapped_foot(source)
    local proxy = ecs.create_entity()
    ecs.add_transform(proxy, x, 0.0, -foot, 1, 1, 1)
    ecs.add_sprite3d(proxy, opts.texture or 0, w_px / 32.0, h_px / 32.0, {
        billboard = opts.billboard or "yaw",
        anchor = opts.anchor or 0.0,
        lit = opts.lit ~= false,
        receive_shadow = opts.receive_shadow ~= false,
    })
    if opts.lit ~= false then
        ecs.set_sprite3d_lit(proxy, true)
        ecs.set_sprite3d_receive_shadow(proxy, true)
        ecs.set_sprite3d_contact_shadow(proxy, true,
            opts.shadow_radius or (w_px / 96.0), opts.shadow_opacity or 0.45)
    end
    if opts.emissive then
        ecs.set_sprite3d_emissive(proxy, opts.emissive[1] or 0.0,
                                  opts.emissive[2] or 0.0, opts.emissive[3] or 0.0)
    end
    return proxy
end

function B.setup_camera(map)
    if B.camera and valid(B.camera) then return B.camera end
    local cam = ecs.create_entity()
    ecs.add_transform(cam, 0.0, B.camera_offset.y, B.camera_offset.z, 1, 1, 1)
    ecs.set_transform_rotation(cam, B.camera_offset.pitch or 0.0, 0.0, 0.0)
    ecs.add_camera_3d(cam, 26.0, 0)
    ecs.add_post_process(cam, true, 0.78, 0.70, 1.02)
    ecs.set_post_process_color_grading_enabled(cam, true)
    ecs.set_post_process_exposure(cam, 1.02)
    ecs.set_post_process_gamma(cam, 1.02)
    ecs.set_post_process_fxaa_enabled(cam, true)
    ecs.set_post_process_vignette_enabled(cam, true)
    ecs.set_post_process_vignette_intensity(cam, 0.45)
    ecs.set_post_process_vignette_radius(cam, 0.72)
    ecs.set_post_process_vignette_softness(cam, 0.55)
    ecs.set_post_process_film_grain_enabled(cam, true)
    ecs.set_post_process_film_grain_intensity(cam, 0.035)
    ecs.set_post_process_bloom_enabled(cam, true)
    ecs.set_post_process_bloom_threshold(cam, 0.80)
    ecs.set_post_process_bloom_intensity(cam, 0.95)
    ecs.set_post_process_tilt_shift(cam, true, 3.6, 1.6, 7.0)
    B.camera = cam
    return cam
end

function B.get_camera()
    return B.camera
end

function B.clear_map()
    if B.terrain and valid(B.terrain) then ecs.destroy_entity(B.terrain) end
    B.terrain = nil
    for _, e in ipairs(B.lights) do
        if valid(e) then ecs.destroy_entity(e) end
    end
    B.lights = {}
end

function B.clear()
    for src, rec in pairs(B.records) do
        if valid(rec.proxy) then ecs.destroy_entity(rec.proxy) end
        B.records[src] = nil
    end
    B.records = {}
    B.follow_source = nil
    B.clear_map()
end

function B.register_actor(source, kind, dir, action, w_px, h_px, opts)
    opts = opts or {}
    local proxy = add_proxy(source, w_px or 32, h_px or 48, opts)
    pcall(ecs.set_sprite_visible, source, false)
    local rec = {
        source = source, proxy = proxy, kind = kind, dir = dir or "d",
        clip = "", clip_dir = "", w = w_px or 32, h = h_px or 48,
    }
    B.records[source] = rec
    B.play_actor(source, kind, rec.dir, action or "idle")
    return proxy
end

function B.set_actor_dir(source, dir)
    local rec = B.records[source]
    if rec then rec.dir = dir or rec.dir end
end

function B.play_actor(source, kind, dir, action, fps, loop)
    local rec = B.records[source]
    if not rec then return end
    if kind then rec.kind = kind end
    rec.dir = dir or rec.dir or "d"
    action = action or "idle"
    local atlas, resolved_dir, resolved_action = resolve_actor_atlas(rec.kind, rec.dir, action)
    if not atlas then return end
    local clip_key = tostring(atlas) .. "|" .. tostring(resolved_action)
    if rec.clip == clip_key then return end
    ecs.set_sprite3d_atlas(rec.proxy, atlas, resolved_action)
    local use_fps = fps or B.ACTION_FPS[action] or B.ACTION_FPS.idle
    local use_loop = loop
    if use_loop == nil then use_loop = B.ACTION_LOOP[action] ~= false end
    ecs.set_sprite3d_anim(rec.proxy, resolved_action, { fps = use_fps, loop = use_loop })
    rec.clip = clip_key
    rec.dir = resolved_dir
end

function B.follow(source)
    B.follow_source = source
end

function B.add_pickup(source, texture, w_px, h_px)
    local proxy = add_proxy(source, w_px or 14, h_px or 14, {
        lit = true, anchor = 0.5, texture = texture,
        shadow_radius = 0.28, shadow_opacity = 0.4,
    })
    ecs.set_sprite3d_uv_rect(proxy, 0, 0, 1, 1)
    pcall(ecs.set_sprite_visible, source, false)
    B.records[source] = { source = source, proxy = proxy, kind = "pickup", dir = "d",
                          clip = "", clip_dir = "", w = w_px or 14, h = h_px or 14 }
    return proxy
end

function B.add_marker(source, texture, w_px, h_px)
    local proxy = add_proxy(source, w_px or 10, h_px or 10, {
        lit = true, anchor = 0.0, texture = texture,
        shadow_radius = 0.18, shadow_opacity = 0.0,
    })
    ecs.set_sprite3d_uv_rect(proxy, 0, 0, 1, 1)
    pcall(ecs.set_sprite_visible, source, false)
    B.records[source] = { source = source, proxy = proxy, kind = "marker", dir = "d",
                          clip = "", clip_dir = "", w = w_px or 10, h = h_px or 10 }
    return proxy
end

function B.register_fx_actor(source, kind, action, w_px, h_px, fps, loop)
    local proxy = add_proxy(source, w_px or 32, h_px or 32, {
        lit = false, anchor = 0.5, receive_shadow = false,
    })
    local atlas = load_atlas(fx_atlas_path(kind))
    if atlas then
        ecs.set_sprite3d_atlas(proxy, atlas, action or kind)
        ecs.set_sprite3d_anim(proxy, action or kind,
            { fps = fps or B.ACTION_FPS.play, loop = loop ~= false })
    end
    pcall(ecs.set_sprite_visible, source, false)
    B.records[source] = { source = source, proxy = proxy, kind = "fx:" .. kind,
                          dir = "d", clip = tostring(atlas or ""), clip_dir = "",
                          w = w_px or 32, h = h_px or 32 }
    return proxy
end

function B.add_fx(kind, x, y, cfg)
    cfg = cfg or {}
    local atlas = load_atlas(fx_atlas_path(kind))
    if not atlas then return nil end
    local e = ecs.create_entity()
    local base_w = cfg.base_w or 1.15
    local base_h = cfg.base_h or 1.15
    local scale = cfg.scale or 1.0
    ecs.add_transform(e, x, 0.45, -y, 1, 1, 1)
    ecs.add_sprite3d(e, 0, base_w * scale, base_h * scale, {
        billboard = "yaw", anchor = 0.5, lit = false, receive_shadow = false,
    })
    ecs.set_sprite3d_color_tint(e, cfg.r or 1, cfg.g or 1, cfg.b or 1, 1.0)
    ecs.set_sprite3d_atlas(e, atlas, kind)
    ecs.set_sprite3d_anim(e, kind, { fps = cfg.fps or B.ACTION_FPS.play, loop = false })
    ecs.set_sprite3d_opacity(e, 1.0)
    B.fx_meta[e] = { base_w = base_w, base_h = base_h }
    return e
end

function B.update_fx(e, x, y, alpha, spin, scale)
    if not valid(e) then return end
    local meta = B.fx_meta[e] or { base_w = 1.15, base_h = 1.15 }
    scale = scale or 1.0
    ecs.set_transform_position(e, x, 0.45, -y)
    ecs.set_sprite3d_size(e, meta.base_w * scale, meta.base_h * scale)
    ecs.set_sprite3d_opacity(e, core.clamp(alpha or 1.0, 0.0, 1.0))
end

function B.destroy_fx(e)
    if e and valid(e) then ecs.destroy_entity(e) end
    B.fx_meta[e] = nil
end

function B.load_map(map)
    B.clear_map()
    if not map or not map.mesh3d then return end
    local path = core.resolve("maps/" .. map.mesh3d)
    local terrain = ecs.create_entity()
    ecs.add_transform(terrain, 0.0, 0.0, 0.0, 1, 1, 1)
    ecs.mesh_renderer_add(terrain, path)
    ecs.set_mesh_shader_variant(terrain, "MESH_LIT")
    ecs.set_mesh_material(terrain, 0.0, 0.9, 1.0, 0.0, 0.0, 0.0, 1.0, true, true)
    B.terrain = terrain

    local cam = B.setup_camera(map)
    local spawn_x = map.spawn and map.spawn.x or (map.w * 0.5)
    local spawn_y = map.spawn and map.spawn.y or (map.h * 0.5)
    ecs.set_transform_position(cam, spawn_x, B.camera_offset.y, -spawn_y + B.camera_offset.z)

    local dir = ecs.create_entity()
    ecs.add_transform(dir, 0, 7, -5, 1, 1, 1)
    ecs.add_directional_light_3d(dir, 0.35, -0.9, 0.20,
                                 1.0, 0.96, 0.88, 1.15, 0.20, 0.0)
    B.lights[#B.lights + 1] = dir

    for _, l in ipairs(map.lights or {}) do
        local le = ecs.create_entity()
        ecs.add_transform(le, l.x, 0.55, -l.y, 1, 1, 1)
        ecs.add_point_light_3d(le, l.r or 1.0, l.g or 0.72, l.b or 0.36,
                               (l.size or 2.0) * 1.25, (l.size or 2.0) * 2.7)
        B.lights[#B.lights + 1] = le
    end
    print(string.format("[bplus] map=%s mesh=%s lights=%d", tostring(map.id), path, #B.lights))
end

function B.update(dt)
    if not B.enabled then return end
    for src, rec in pairs(B.records) do
        if not valid(src) then
            if valid(rec.proxy) then ecs.destroy_entity(rec.proxy) end
            B.records[src] = nil
        elseif valid(rec.proxy) then
            local x, foot, z, sx, sy, sz = mapped_foot(src)
            local flip = (sx < 0.0) and -1.0 or 1.0
            ecs.set_transform_position(rec.proxy, x, 0.0, -foot)
            if rec.kind ~= "pickup" and rec.kind ~= "marker" then
                ecs.set_sprite3d_size(rec.proxy, flip * math.abs(sx), math.abs(sy))
            end
        end
    end

    if B.camera and valid(B.camera) and B.follow_source and valid(B.follow_source) then
        local x, foot = mapped_foot(B.follow_source)
        local target_x = x
        local target_y = B.camera_offset.y
        local target_z = -foot + B.camera_offset.z
        local cx, cy, cz = ecs.get_transform_position(B.camera)
        local k = 1.0 - math.exp(-10.0 * (dt or 0.016))
        ecs.set_transform_position(B.camera,
            core.lerp(cx, target_x, k),
            core.lerp(cy, target_y, k),
            core.lerp(cz, target_z, k))
    end
end

function B.project(wx, wy)
    local sx, sy, visible = dse.ecs.world_to_screen(wx, 0.0, -wy)
    return sx, sy, visible
end

return B
