-- ============================================================================
-- _hd2d_m4_test.lua  HD-2D M4 camera + post-process smoke
--
-- Weak-perspective (long lens) and optional ortho-3D camera, lit Sprite3D
-- characters at different depths, bloom emissive and tilt-shift DOF.
-- Set DSE_M4_ORTHO=1 to exercise ecs.set_camera_ortho_3d.
-- Screenshot controls: DSE_MAX_FRAMES / DSE_SCREENSHOT_FRAME / DSE_SCREENSHOT_PATH
-- ============================================================================

local G = { hero = 0, far_hero = 0 }

local function add_box(x, y, z, sx, sy, sz, r, g, b, a)
    local e = dse.ecs.create_entity()
    dse.ecs.add_transform(e, x, y, z, sx, sy, sz)
    local hx, hy, hz = 0.5, 0.5, 0.5
    local v = {
        -hx,-hy,-hz,  hx,-hy,-hz,  hx, hy,-hz, -hx, hy,-hz,
        -hx,-hy, hz,  hx,-hy, hz,  hx, hy, hz, -hx, hy, hz,
    }
    local i = {
        4,5,6, 4,6,7,  1,0,3, 1,3,2,  0,4,7, 0,7,3,
        5,1,2, 5,2,6,  0,1,5, 0,5,4,  3,7,6, 3,6,2,
    }
    dse.ecs.add_mesh_renderer(e, r, g, b, a or 1.0, v, i)
    dse.ecs.set_mesh_shader_variant(e, "MESH_LIT")
    dse.ecs.set_mesh_material(e, 0.0, 0.9, 1.0, 0.0, 0.0, 0.0, 1.0, true, true)
    return e
end

local function add_hero(x, y, z, emissive)
    local tex = dse.assets.load_texture_ex(
        "templates/hd2d_wuxia/assets/char/hero/hero_d_idle_0.png", "nearest", "clamp")
    local e = dse.ecs.create_entity()
    dse.ecs.add_transform(e, x, y, z, 1, 1, 1)
    dse.ecs.add_sprite3d(e, tex, 0.9, 1.3, {
        billboard = "yaw", anchor = 0.0, lit = true, receive_shadow = true})
    dse.ecs.set_sprite3d_lit(e, true)
    dse.ecs.set_sprite3d_receive_shadow(e, true)
    dse.ecs.set_sprite3d_emissive(e, emissive or 0.0, (emissive or 0.0) * 0.35, (emissive or 0.0) * 0.15)
    dse.ecs.set_sprite3d_contact_shadow(e, true, 0.42, 0.45)
    return e
end

function Awake()
    add_box(0.0, -0.10, -9.0, 30.0, 0.20, 30.0, 0.17, 0.40, 0.20, 1.0)
    -- Depth reference blocks: near/far silhouettes for DOF comparison.
    add_box(-2.3, 0.65, -3.5, 0.9, 1.3, 0.9, 0.55, 0.30, 0.20, 1.0)
    add_box(2.3, 0.65, -12.0, 0.9, 1.3, 0.9, 0.22, 0.30, 0.62, 1.0)

    local cam = dse.ecs.create_entity()
    dse.ecs.add_transform(cam, 0.0, 1.05, 0.0, 1, 1, 1)
    dse.ecs.set_transform_rotation(cam, 0.0, 0.0, 0.0)
    -- HD-2D weak perspective: long lens / small FOV by default.
    dse.ecs.add_camera_3d(cam, 16.0, 0)
    if os.getenv("DSE_M4_ORTHO") == "1" then
        dse.ecs.set_camera_ortho_3d(cam, 3.2, 10.0, 0.0)
        print("[m4] ortho-3d camera ON")
    else
        print("[m4] weak-perspective camera")
    end

    local dir = dse.ecs.create_entity()
    dse.ecs.add_transform(dir, 0, 7, -5, 1, 1, 1)
    dse.ecs.add_directional_light_3d(dir, 0.35, -0.9, 0.2,
                                     1.0, 0.98, 0.95, 1.1, 0.22, 0.0)

    local p = dse.ecs.create_entity()
    dse.ecs.add_transform(p, -0.5, 1.15, -3.2, 1, 1, 1)
    dse.ecs.add_point_light_3d(p, 1.0, 0.72, 0.38, 4.5, 6.0)

    G.hero = add_hero(0.0, 0.0, -3.0, 0.10)
    G.far_hero = add_hero(0.0, 0.0, -12.0, 0.0)

    -- Post-process on the camera: tilt-shift uses the existing depth DOF path.
    dse.ecs.add_post_process(cam, true, 0.75, 0.65, 1.0)
    dse.ecs.set_post_process_tilt_shift(cam, true, 3.2, 1.4, 6.0)
    dse.ecs.set_post_process_bloom_enabled(cam, true)
    dse.ecs.set_post_process_bloom_threshold(cam, 0.65)
    dse.ecs.set_post_process_bloom_intensity(cam, 0.9)
end

function Update(dt)
    local _ = dt
end
