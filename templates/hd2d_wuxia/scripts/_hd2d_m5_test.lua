-- ============================================================================
-- _hd2d_m5_test.lua  HD-2D M5 .dsprite atlas + clip animation smoke
-- ============================================================================

local G = { hero = 0 }

local function add_box(x, y, z, sx, sy, sz, r, g, b, a)
    local e = dse.ecs.create_entity()
    dse.ecs.add_transform(e, x, y, z, sx, sy, sz)
    local h = 0.5
    local v = {-h,-h,-h, h,-h,-h, h,h,-h, -h,h,-h, -h,-h,h, h,-h,h, h,h,h, -h,h,h}
    local i = {4,5,6,4,6,7,1,0,3,1,3,2,0,4,7,0,7,3,5,1,2,5,2,6,0,1,5,0,5,4,3,7,6,3,6,2}
    dse.ecs.add_mesh_renderer(e, r, g, b, a or 1.0, v, i)
    dse.ecs.set_mesh_shader_variant(e, "MESH_LIT")
    dse.ecs.set_mesh_material(e, 0.0, 0.9, 1.0, 0.0, 0.0, 0.0, 1.0, true, true)
    return e
end

function Awake()
    add_box(0.0, -0.10, -5.0, 20.0, 0.20, 20.0, 0.18, 0.42, 0.20, 1.0)

    local cam = dse.ecs.create_entity()
    dse.ecs.add_transform(cam, 0.0, 0.9, 0.0, 1, 1, 1)
    dse.ecs.set_transform_rotation(cam, 0, 0, 0)
    dse.ecs.add_camera_3d(cam, 30.0, 0)

    local dir = dse.ecs.create_entity()
    dse.ecs.add_transform(dir, 0, 6, -5, 1, 1, 1)
    dse.ecs.add_directional_light_3d(dir, 0.35, -0.9, 0.2, 1.0, 0.98, 0.95, 1.0, 0.25, 0.0)

    local atlas = dse.assets.load_sprite_atlas(
        "templates/hd2d_wuxia/assets/char/hero/hero_d_walk.dsprite.json")
    print(string.format("[m5] atlas=%s", tostring(atlas)))
    if atlas < 0 then
        error("load_sprite_atlas failed")
    end

    G.hero = dse.ecs.create_entity()
    dse.ecs.add_transform(G.hero, 0.0, 0.0, -3.0, 1, 1, 1)
    dse.ecs.add_sprite3d(G.hero, 0, 0.9, 1.3, {
        billboard = "yaw", anchor = 0.0, lit = true, receive_shadow = true})
    dse.ecs.set_sprite3d_lit(G.hero, true)
    dse.ecs.set_sprite3d_receive_shadow(G.hero, true)
    dse.ecs.set_sprite3d_atlas(G.hero, atlas, "walk")
    dse.ecs.set_sprite3d_anim(G.hero, "walk", {fps = 10.0, loop = true})
    dse.ecs.set_sprite3d_contact_shadow(G.hero, true, 0.42, 0.45)
end

function Update(dt)
    local _ = dt
end
