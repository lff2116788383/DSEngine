-- ============================================================================
-- _sprite3d_lit_test.lua  HD-2D M3 full lit-path smoke test
--
-- Scene: lit 3D ground + a shadow-casting house box + one lit billboard hero.
-- Warm point light (DSE_LIT_POINT=0 disables) and directional CSM light.
-- The hero also enables the lightweight grounding contact shadow.
--
-- Screenshot controls: DSE_MAX_FRAMES / DSE_SCREENSHOT_FRAME / DSE_SCREENSHOT_PATH
-- ============================================================================

local G = { hero = 0 }

local function add_box(x, y, z, sx, sy, sz, r, g, b, a)
    local e = dse.ecs.create_entity()
    dse.ecs.add_transform(e, x, y, z, sx, sy, sz)
    local hx, hy, hz = 0.5, 0.5, 0.5
    local v = {
        -hx,-hy,-hz,  hx,-hy,-hz,  hx, hy,-hz, -hx, hy,-hz,
        -hx,-hy, hz,  hx,-hy, hz,  hx, hy, hz, -hx, hy, hz,
    }
    local i = {
        4,5,6, 4,6,7,   -- +Z
        1,0,3, 1,3,2,   -- -Z
        0,4,7, 0,7,3,   -- -X
        5,1,2, 5,2,6,   -- +X
        0,1,5, 0,5,4,   -- -Y
        3,7,6, 3,6,2,   -- +Y
    }
    dse.ecs.add_mesh_renderer(e, r, g, b, a or 1.0, v, i)
    dse.ecs.set_mesh_shader_variant(e, "MESH_LIT")
    dse.ecs.set_mesh_material(e, 0.0, 0.9, 1.0, 0.0, 0.0, 0.0, 1.0, true, true)
    return e
end

function Awake()
    -- Ground receives both directional and point light.
    add_box(0.0, -0.10, -5.0, 20.0, 0.20, 20.0, 0.18, 0.42, 0.20, 1.0)

    -- Shadow caster: a simple house/box. Keep it off-center so the CSM
    -- shadow can fall across the hero/ground area.
    add_box(1.6, 0.75, -5.2, 1.8, 1.5, 1.8, 0.22, 0.18, 0.14, 1.0)

    local cam = dse.ecs.create_entity()
    dse.ecs.add_transform(cam, 0.0, 1.1, 0.0, 1, 1, 1)
    dse.ecs.set_transform_rotation(cam, 0.0, 0.0, 0.0)
    dse.ecs.add_camera_3d(cam, 32.0, 0)

    local dir = dse.ecs.create_entity()
    dse.ecs.add_transform(dir, 0, 6, -5, 1, 1, 1)
    dse.ecs.add_directional_light_3d(dir, 0.35, -0.9, 0.2,
                                     1.0, 0.98, 0.95, 1.15, 0.20, 0.0)

    if os.getenv("DSE_LIT_POINT") ~= "0" then
        local p = dse.ecs.create_entity()
        dse.ecs.add_transform(p, -0.35, 1.10, -3.9, 1, 1, 1)
        dse.ecs.add_point_light_3d(p, 1.0, 0.72, 0.38, 5.0, 6.0)
        print("[m3-lit] point light ON")
    else
        print("[m3-lit] point light OFF")
    end

    local tex = dse.assets.load_texture_ex(
        "templates/hd2d_wuxia/assets/char/hero/hero_d_idle_0.png", "nearest", "clamp")
    G.hero = dse.ecs.create_entity()
    dse.ecs.add_transform(G.hero, -0.1, 0.0, -3.9, 1, 1, 1)
    dse.ecs.add_sprite3d(G.hero, tex, 0.90, 1.30, {
        billboard = "yaw", anchor = 0.0, lit = true, receive_shadow = true})
    dse.ecs.set_sprite3d_lit(G.hero, true)
    dse.ecs.set_sprite3d_receive_shadow(G.hero, true)
    dse.ecs.set_sprite3d_emissive(G.hero, 0.10, 0.035, 0.015)
    dse.ecs.set_sprite3d_contact_shadow(G.hero, true, 0.45, 0.50)
    dse.ecs.set_sprite3d_color_tint(G.hero, 1.0, 1.0, 1.0, 1.0)
end

function Update(dt)
    local _ = dt
end
