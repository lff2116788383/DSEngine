-- ============================================================================
-- _sprite3d_many_lights_test.lua
-- M3 high-light-count smoke: DSE_LIT_COUNT point lights (default 80) around
-- one lit Sprite3D hero. This validates the 255-light ForwardShaded UBO path
-- that replaced the old 8+8 snapshot cap for lit sprites.
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
    add_box(0.0, -0.10, -5.0, 20.0, 0.20, 20.0, 0.18, 0.42, 0.20)

    local cam = dse.ecs.create_entity()
    dse.ecs.add_transform(cam, 0.0, 1.0, 0.0, 1, 1, 1)
    dse.ecs.add_camera_3d(cam, 30.0, 0)

    local count = tonumber(os.getenv("DSE_LIT_COUNT") or "80")
    if not count or count < 1 then count = 80 end
    for i = 0, count - 1 do
        local p = dse.ecs.create_entity()
        local a = (i / count) * math.pi * 2.0
        local radius = 1.4 + (i % 5) * 0.35
        dse.ecs.add_transform(p,
            math.cos(a) * radius,
            0.75 + (i % 3) * 0.15,
            -3.5 + math.sin(a) * radius,
            1, 1, 1)
        dse.ecs.add_point_light_3d(p, 1.0, 0.55, 0.28, 1.0, 6.0)
    end
    print(string.format("[m3-many] point_lights=%d", count))

    local tex = dse.assets.load_texture_ex(
        "templates/hd2d_wuxia/assets/char/hero/hero_d_idle_0.png", "nearest", "clamp")
    G.hero = dse.ecs.create_entity()
    dse.ecs.add_transform(G.hero, 0.0, 0.0, -3.5, 1, 1, 1)
    dse.ecs.add_sprite3d(G.hero, tex, 0.9, 1.3, {
        billboard = "yaw", anchor = 0.0, lit = true, receive_shadow = true})
    dse.ecs.set_sprite3d_lit(G.hero, true)
    dse.ecs.set_sprite3d_receive_shadow(G.hero, true)
    dse.ecs.set_sprite3d_color_tint(G.hero, 1.0, 1.0, 1.0, 1.0)
end

function Update(dt)
    local _ = dt
end
