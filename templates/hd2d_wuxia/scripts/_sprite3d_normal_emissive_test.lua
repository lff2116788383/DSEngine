-- ============================================================================
-- _sprite3d_normal_emissive_test.lua
-- M3 additions: normal-map response and emissive -> Bloom evidence.
--   DSE_M3_NORMAL=0/1    disable/enable normal map strength 2.0
--   DSE_M3_EMISSIVE=0/1  disable/enable emissive HDR color
-- ============================================================================

local G = { hero = 0 }

local function add_box(x, y, z, sx, sy, sz, r, g, b)
    local e = dse.ecs.create_entity()
    dse.ecs.add_transform(e, x, y, z, sx, sy, sz)
    local h = 0.5
    local v = {-h,-h,-h, h,-h,-h, h,h,-h, -h,h,-h, -h,-h,h, h,-h,h, h,h,h, -h,h,h}
    local i = {4,5,6,4,6,7,1,0,3,1,3,2,0,4,7,0,7,3,5,1,2,5,2,6,0,1,5,0,5,4,3,7,6,3,6,2}
    dse.ecs.add_mesh_renderer(e, r, g, b, 1.0, v, i)
    dse.ecs.set_mesh_shader_variant(e, "MESH_LIT")
    dse.ecs.set_mesh_material(e, 0.0, 0.9, 1.0, 0.0, 0.0, 0.0, 1.0, true, true)
    return e
end

function Awake()
    add_box(0.0, -0.10, -4.5, 16.0, 0.20, 16.0, 0.18, 0.42, 0.20)

    local cam = dse.ecs.create_entity()
    dse.ecs.add_transform(cam, 0.0, 1.0, 0.0, 1, 1, 1)
    dse.ecs.add_camera_3d(cam, 30.0, 0)

    local dir = dse.ecs.create_entity()
    dse.ecs.add_transform(dir, 0, 6, -4, 1, 1, 1)
    dse.ecs.add_directional_light_3d(dir, -0.6, -0.65, 0.25,
                                     1.0, 0.98, 0.95, 0.55, 0.08, 0.0)

    local p = dse.ecs.create_entity()
    dse.ecs.add_transform(p, -2.2, 0.85, -3.2, 1, 1, 1)
    dse.ecs.add_point_light_3d(p, 1.0, 0.65, 0.35, 4.0, 6.0)

    local atlas = dse.assets.load_sprite_atlas(
        "templates/hd2d_wuxia/assets/char/hero/hero_d_walk_m3.dsprite.json")
    G.hero = dse.ecs.create_entity()
    dse.ecs.add_transform(G.hero, 0.0, 0.0, -3.2, 1, 1, 1)
    dse.ecs.add_sprite3d(G.hero, 0, 0.9, 1.3, {
        billboard = "yaw", anchor = 0.0, lit = true, receive_shadow = true})
    dse.ecs.set_sprite3d_lit(G.hero, true)
    dse.ecs.set_sprite3d_receive_shadow(G.hero, true)
    dse.ecs.set_sprite3d_atlas(G.hero, atlas, "walk")
    dse.ecs.set_sprite3d_anim(G.hero, "walk", {fps = 8.0, loop = true})

    local normal_on = os.getenv("DSE_M3_NORMAL") ~= "0"
    dse.ecs.set_sprite3d_normal(G.hero, 0, normal_on and 2.0 or 0.0)

    local emissive_on = os.getenv("DSE_M3_EMISSIVE") ~= "0"
    if emissive_on then
        dse.ecs.set_sprite3d_emissive(G.hero, 2.5, 0.9, 0.25)
    else
        dse.ecs.set_sprite3d_emissive(G.hero, 0.0, 0.0, 0.0)
    end

    dse.ecs.add_post_process(cam, true, 0.75, 0.8, 1.0)
    dse.ecs.set_post_process_bloom_enabled(cam, true)
    dse.ecs.set_post_process_bloom_threshold(cam, 0.85)
    dse.ecs.set_post_process_bloom_intensity(cam, 0.9)

    print(string.format("[m3-normal-emissive] normal=%s emissive=%s",
                        tostring(normal_on), tostring(emissive_on)))
end

function Update(dt)
    local _ = dt
end
