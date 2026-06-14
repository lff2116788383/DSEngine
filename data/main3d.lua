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

local camera
local cube
local t = 0.0
local yaw = 0.0        -- camera orbit angle (radians)
local radius = 6.5     -- horizontal distance from the origin
local height = 2.2     -- camera height
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
    -- dir(x,y,z), color(r,g,b), intensity, ambient, sky-blend (mirrors samples/lua/3d/cube.lua)
    dse.ecs.add_directional_light_3d(light, -0.5, -1.0, -0.4, 1.0, 0.96, 0.88, 1.5, 0.25, 0.35)
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
    dse.ecs.set_mesh_material(cube, 0.05, 0.55, 1.0, 0.02, 0.02, 0.06, 1.0, false)
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

function Awake()
    dse.app.set_window_title("DSEngine Web - 3D Demo (A/D orbit, W/S dolly, M=BGM)")
    setup_camera()
    setup_light()
    setup_ground()
    setup_cube()
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
