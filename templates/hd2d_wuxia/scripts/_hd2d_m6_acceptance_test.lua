-- ============================================================================
-- _hd2d_m6_acceptance_test.lua
-- One-scene HD-2D B+ acceptance substrate:
--   * procedural 3D ground / steps / building / roof / tree canopy
--   * animated atlas Sprite3D hero / enemy / NPC
--   * directional CSM + warm point light, contact shadow, emissive bloom
--   * weak-perspective or ortho-3D camera + tilt-shift DOF
-- Screenshot: DSE_MAX_FRAMES / DSE_SCREENSHOT_FRAME / DSE_SCREENSHOT_PATH
-- ============================================================================

local G = { hero = 0 }

local function add_box(x, y, z, sx, sy, sz, r, g, b, a, lit)
    local e = dse.ecs.create_entity()
    dse.ecs.add_transform(e, x, y, z, sx, sy, sz)
    local h = 0.5
    local v = {-h,-h,-h, h,-h,-h, h,h,-h, -h,h,-h, -h,-h,h, h,-h,h, h,h,h, -h,h,h}
    local i = {4,5,6,4,6,7,1,0,3,1,3,2,0,4,7,0,7,3,5,1,2,5,2,6,0,1,5,0,5,4,3,7,6,3,6,2}
    dse.ecs.add_mesh_renderer(e, r, g, b, a or 1.0, v, i)
    dse.ecs.set_mesh_shader_variant(e, lit and "MESH_LIT" or "MESH_UNLIT")
    if lit then
        dse.ecs.set_mesh_material(e, 0.0, 0.9, 1.0, 0.0, 0.0, 0.0, 1.0, true, true)
    end
    return e
end

local function add_atlas_sprite(x, y, z, emissive)
    local atlas = dse.assets.load_sprite_atlas(
        "templates/hd2d_wuxia/assets/char/hero/hero_d_walk.dsprite.json")
    local e = dse.ecs.create_entity()
    dse.ecs.add_transform(e, x, y, z, 1, 1, 1)
    dse.ecs.add_sprite3d(e, 0, 0.9, 1.3, {
        billboard = "yaw", anchor = 0.0, lit = true, receive_shadow = true})
    dse.ecs.set_sprite3d_lit(e, true)
    dse.ecs.set_sprite3d_receive_shadow(e, true)
    dse.ecs.set_sprite3d_atlas(e, atlas, "walk")
    dse.ecs.set_sprite3d_anim(e, "walk", {fps = 8.0, loop = true})
    if emissive then
        dse.ecs.set_sprite3d_emissive(e, emissive[1], emissive[2], emissive[3])
    end
    dse.ecs.set_sprite3d_contact_shadow(e, true, 0.42, 0.48)
    return e
end

function Awake()
    -- 3D terrain and props
    add_box(0.0, -0.10, -8.0, 28.0, 0.20, 28.0, 0.17, 0.40, 0.18, 1.0, true)
    add_box(-1.4, 0.10, -5.2, 1.6, 0.20, 1.6, 0.28, 0.48, 0.24, 1.0, true)
    add_box(-1.4, 0.30, -5.2, 1.6, 0.20, 1.6, 0.28, 0.48, 0.24, 1.0, true)
    add_box(1.8, 0.75, -6.0, 2.2, 1.5, 2.0, 0.24, 0.18, 0.14, 1.0, true)
    add_box(1.8, 1.58, -6.0, 2.7, 0.18, 2.5, 0.42, 0.20, 0.14, 1.0, true)
    add_box(-2.8, 0.90, -8.5, 2.2, 1.8, 2.2, 0.12, 0.30, 0.14, 1.0, true)

    local cam = dse.ecs.create_entity()
    dse.ecs.add_transform(cam, -0.3, 1.0, 0.0, 1, 1, 1)
    dse.ecs.set_transform_rotation(cam, 0.0, 0.0, 0.0)
    dse.ecs.add_camera_3d(cam, 16.0, 0)
    if os.getenv("DSE_M6_ORTHO") == "1" then
        dse.ecs.set_camera_ortho_3d(cam, 3.4, 10.0, 0.0)
    end

    local dir = dse.ecs.create_entity()
    dse.ecs.add_transform(dir, 0, 7, -5, 1, 1, 1)
    dse.ecs.add_directional_light_3d(dir, 0.4, -0.9, 0.25,
                                     1.0, 0.97, 0.90, 1.2, 0.18, 0.0)

    local p = dse.ecs.create_entity()
    dse.ecs.add_transform(p, -0.35, 1.15, -3.4, 1, 1, 1)
    dse.ecs.add_point_light_3d(p, 1.0, 0.70, 0.36, 5.0, 6.0)

    G.hero = add_atlas_sprite(-0.15, 0.0, -3.4, {2.50, 0.90, 0.25})
    local _enemy = add_atlas_sprite(1.35, 0.0, -4.8, {0.0, 0.0, 0.0})
    local _npc = add_atlas_sprite(-1.55, 0.0, -6.2, {0.0, 0.0, 0.0})

    dse.ecs.add_post_process(cam, true, 0.72, 0.80, 1.0)
    dse.ecs.set_post_process_tilt_shift(cam, true, 3.4, 1.5, 5.5)
    dse.ecs.set_post_process_bloom_enabled(cam, true)
    dse.ecs.set_post_process_bloom_threshold(cam, 0.85)
    dse.ecs.set_post_process_bloom_intensity(cam, 0.95)

    print(string.format("[m6] hero=%s atlas=sprite3d ground=3d camera=ortho=%s",
                        tostring(G.hero), tostring(os.getenv("DSE_M6_ORTHO") == "1")))
end

function Update(dt)
    local _ = dt
end
