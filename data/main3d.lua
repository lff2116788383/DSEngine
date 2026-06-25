-- DSEngine minimal 3D Web demo (A1 M5 "3D forward on Web", best-effort).
-- Self-contained: only the repo data/ dir is preloaded into the Web MEMFS, so
-- this entry must NOT require() the sample modules under samples/lua.
-- Lifecycle: the engine calls global Awake() once (via main()), then Update(dt).
--
-- What it proves: WebGL2 (GLES3.0, no Compute/SSBO) runs the forward 3D path —
-- a perspective Camera3D, a directional light and a lit MeshRenderer cube,
-- shaded by the UBO PBR program (the same non-SSBO path desktop GL<4.3 uses).
--
-- Controls (verify Web input wiring on the 3D path):
--   A / D (or Left/Right) : orbit the camera around the cube
--   W / S (or Up/Down)    : dolly the camera in / out
--   M                     : toggle the looping background music

local KEY_W, KEY_A, KEY_S, KEY_D = 87, 65, 83, 68
local KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT = 265, 264, 263, 262
local KEY_M = 77

local AUDIO_CLIP = "audio/spatial/spatial_ping.wav"

-- B-1: minimal two-bone skinned rig (real data/ asset, web-preloaded).
local SKIN_MESH  = "animation/minimal_rig/two_bone.dmesh"
local SKIN_DANIM = "animation/minimal_rig/two_bone_idle_walk.danim"
local SKIN_DSKEL = "animation/minimal_rig/two_bone.dskel"

local camera
local cube
local t = 0.0
local yaw = 0.0        -- camera orbit angle (radians)
local radius = 9.5     -- horizontal distance from the origin
local height = 3.4     -- camera height
local prev_m = false
local bgm_on = true

local function key(code)
    return dse.app.get_key and dse.app.get_key(code)
end

-- Place the camera on an orbit circle around the origin and aim it at the cube.
-- The engine's default camera forward is -Z, so yaw=0 sits on +Z looking toward
-- the origin; pitch tilts down to keep the cube centred as height changes.
local function update_camera()
    local px = radius * math.sin(yaw)
    local pz = radius * math.cos(yaw)
    dse.ecs.add_transform(camera, px, height, pz, 1.0, 1.0, 1.0)
    local pitch = -math.deg(math.atan(height, radius))
    dse.ecs.set_transform_rotation(camera, pitch, math.deg(yaw), 0.0)
end

local function setup_camera()
    camera = dse.ecs.create_entity()
    dse.ecs.add_camera_3d(camera, 60.0, 100, 0.1, 200.0)
    update_camera()
end

local function setup_light()
    local light = dse.ecs.create_entity()
    -- dir(x,y,z), color(r,g,b), intensity, ambient, shadow_strength.
    -- Keep the key light strong but the flat ambient low so the cube faces read
    -- as distinct planes (a high flat ambient washes the scene out to flat grey).
    dse.ecs.add_directional_light_3d(light, -0.5, -1.0, -0.4, 1.0, 0.96, 0.88, 2.4, 0.10, 0.35)
    -- Hemisphere ambient: cool sky from above, warm-dark bounce from below. This
    -- replaces the flat grey ambient with a directional tint so the form pops.
    local sky = dse.ecs.create_entity()
    dse.ecs.add_sky_light(sky, 0.34, 0.42, 0.55, 0.10, 0.09, 0.08, 1.0)
end

-- A unit cube: 8 shared corners, 6 faces, 12 triangles.
local function setup_cube()
    cube = dse.ecs.create_entity()
    dse.ecs.add_transform(cube, 0.0, 0.4, 0.0, 1.6, 1.6, 1.6)
    local v = {
        -0.5, -0.5,  0.5,   0.5, -0.5,  0.5,   0.5,  0.5,  0.5,  -0.5,  0.5,  0.5,
        -0.5, -0.5, -0.5,   0.5, -0.5, -0.5,   0.5,  0.5, -0.5,  -0.5,  0.5, -0.5,
    }
    local idx = {
        0, 1, 2, 2, 3, 0,  -- front
        1, 5, 6, 6, 2, 1,  -- right
        5, 4, 7, 7, 6, 5,  -- back
        4, 0, 3, 3, 7, 4,  -- left
        3, 2, 6, 6, 7, 3,  -- top
        4, 5, 1, 1, 0, 4,  -- bottom
    }
    dse.ecs.add_mesh_renderer(cube, 0.30, 0.62, 1.0, 1.0, v, idx)
    dse.ecs.set_mesh_shader_variant(cube, "MESH_LIT")
    -- metallic, roughness, ao, emissive(r,g,b), normal_strength, receive_shadow
    dse.ecs.set_mesh_material(cube, 0.05, 0.38, 1.0, 0.02, 0.02, 0.06, 1.0, false)
end

-- A large flat quad under the cube for depth/parallax context.
local function setup_ground()
    local ground = dse.ecs.create_entity()
    dse.ecs.add_transform(ground, 0.0, -0.6, 0.0, 1.0, 1.0, 1.0)
    local s = 6.0
    local v = { -s, 0.0, -s,   s, 0.0, -s,   s, 0.0,  s,  -s, 0.0,  s }
    local idx = { 0, 2, 1, 0, 3, 2 }
    dse.ecs.add_mesh_renderer(ground, 0.16, 0.17, 0.20, 1.0, v, idx)
    dse.ecs.set_mesh_shader_variant(ground, "MESH_LIT")
    dse.ecs.set_mesh_material(ground, 0.0, 0.9, 1.0, 0.0, 0.0, 0.0, 1.0, false)
end

-- Deterministic dark "shadow" marker quad on the ground beneath the cube.
-- The engine's forward CSM path does not produce a camera-visible ground
-- projection (this holds on every desktop backend too -- see
-- samples/lua/3d/3d_shadow_showcase.lua, whose canonical shadow demo uses the
-- same fixed dark fallback marker). This quad stands in for the caster's cast
-- shadow and, being static, keeps the dual-backend golden deterministic.
local function setup_shadow_marker()
    local marker = dse.ecs.create_entity()
    -- A flat quad on the ground just below the cube, offset toward the camera so
    -- it reads clearly in front of the cube's base. Lifted ~0.01 above the ground
    -- (ground y=-0.6) so it wins the depth test against the coplanar ground quad.
    dse.ecs.add_transform(marker, 0.0, -0.59, 0.7, 1.0, 1.0, 1.0)
    local sx, sz = 1.25, 0.95
    local v = { -sx, 0.0, -sz,   sx, 0.0, -sz,   sx, 0.0,  sz,  -sx, 0.0,  sz }
    local idx = { 0, 2, 1, 0, 3, 2 }
    -- MESH_UNLIT renders the vertex colour directly (no lighting, no PBR
    -- dielectric specular floor), so the near-black colour reads as an opaque
    -- dark shadow patch independent of scene lighting and backend.
    dse.ecs.add_mesh_renderer(marker, 0.0015, 0.0015, 0.002, 1.0, v, idx)
    dse.ecs.set_mesh_shader_variant(marker, "MESH_UNLIT")
    dse.ecs.set_mesh_material(marker, 0.0, 1.0, 1.0, 0.0, 0.0, 0.0, 1.0, false)
end

-- A visible GPU-compute-skinned mesh (B-1 "skinning visible activation").
-- On web the forward *skinned* shaded program is unavailable (WebGPU has no
-- per-draw skinning; WebGL2/GLES3.0 can't compile the SSBO skinned VS), so the
-- engine skins this two-bone rig on the GPU compute path (WebGPU, consumed via
-- the async double-buffer readback) or on the CPU (WebGL2 / warmup), bakes the
-- object-space result into a static mesh and draws it through the ordinary
-- forward-shaded path. The pose is frozen (speed 0 -> clip time pinned at 0) so
-- the dual-backend golden stays bit-stable.
local function setup_skinned()
    local skinned = dse.ecs.create_entity()
    dse.ecs.add_transform(skinned, -3.1, 0.25, 0.0, 1.7, 1.7, 1.7)
    dse.ecs.add_mesh_renderer(skinned, 1.0, 0.52, 0.16, 1.0)
    dse.ecs.set_mesh_path(skinned, SKIN_MESH)
    dse.ecs.set_mesh_shader_variant(skinned, "MESH_LIT")
    -- metallic, roughness, ao, emissive(r,g,b), normal_strength, receive_shadow
    dse.ecs.set_mesh_material(skinned, 0.05, 0.45, 1.0, 0.04, 0.02, 0.0, 1.0, false)
    dse.ecs.add_animator_3d(skinned, SKIN_DANIM, SKIN_DSKEL)
    if dse.ecs.init_animator_3d_fsm then dse.ecs.init_animator_3d_fsm(skinned) end
    if dse.ecs.add_animator_3d_state then
        -- state playback speed 0 -> the FSM never advances the clip clock
        -- (AdvanceClipTime: current += dt * speed), so the sampled pose is
        -- identical every frame and across backends (deterministic golden).
        dse.ecs.add_animator_3d_state(skinned, "idle", SKIN_DANIM, true, 0.0)
    end
    dse.ecs.set_animator_3d_state(skinned, "idle", 0.0, true)
end

-- Explicit tone-mapping control. Forward3D has no HDR post chain, so the
-- composite pass tone-maps with whatever exposure this component carries;
-- pinning it (plus a soft vignette) keeps the framing tidy and the midtones
-- from lifting to flat grey.
local function setup_post_process()
    local pp = dse.ecs.create_entity()
    -- entity, bloom_enabled, bloom_threshold, bloom_intensity, exposure
    dse.ecs.add_post_process(pp, false, 1.0, 1.0, 0.9)
    if dse.ecs.set_post_process_vignette then
        -- enabled, intensity, radius, softness
        dse.ecs.set_post_process_vignette(pp, true, 0.35, 0.85, 0.45)
    end
end

function Awake()
    dse.app.set_window_title("DSEngine Web - 3D Demo (A/D orbit, W/S dolly, M=BGM)")
    setup_camera()
    setup_light()
    setup_post_process()
    setup_ground()
    setup_shadow_marker()
    setup_cube()
    setup_skinned()
    if dse.audio and dse.audio.play_bgm then
        dse.audio.play_bgm(AUDIO_CLIP, 0.2, true)
    end
end

function Update(delta_time)
    local dt = delta_time or 0.0
    t = t + dt

    -- Spin the cube so the 3D shape and lighting read clearly.
    if cube then
        dse.ecs.set_transform_rotation(cube, t * 18.0, t * 28.0, t * 10.0)
    end

    -- Orbit / dolly the camera (verifies keyboard input on the 3D path).
    local orbit_speed = 1.4
    if key(KEY_A) or key(KEY_LEFT)  then yaw = yaw - orbit_speed * dt end
    if key(KEY_D) or key(KEY_RIGHT) then yaw = yaw + orbit_speed * dt end
    if key(KEY_W) or key(KEY_UP)    then radius = math.max(2.5, radius - 4.0 * dt) end
    if key(KEY_S) or key(KEY_DOWN)  then radius = math.min(20.0, radius + 4.0 * dt) end
    update_camera()

    -- 'M' (press edge) toggles the looping background music.
    local m_down = key(KEY_M)
    if m_down and not prev_m and dse.audio then
        bgm_on = not bgm_on
        if bgm_on then
            if dse.audio.resume_bgm then dse.audio.resume_bgm() end
        else
            if dse.audio.pause_bgm then dse.audio.pause_bgm() end
        end
    end
    prev_m = m_down and true or false
end

function exit() end

function main()
    Awake()
end
