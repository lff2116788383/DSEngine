-- ============================================================================
-- _sprite3d_lit_test.lua  HD-2D M3.1 minimal Sprite3D lit smoke test
--
-- Same camera/hero as the M1/M2 demos, but lit=true and one warm point light
-- near the hero. Set DSE_LIT_POINT=0 to disable the point light and compare
-- screenshot brightness; the lit shader must react to the point light.
-- ============================================================================

local G = { hero = 0 }

function Awake()
    local tex = dse.assets.load_texture_ex(
        "templates/hd2d_wuxia/assets/char/hero/hero_d_idle_0.png", "nearest", "clamp")

    G.hero = dse.ecs.create_entity()
    dse.ecs.add_transform(G.hero, 0.0, 0.0, -5.0, 1, 1, 1)
    dse.ecs.add_sprite3d(G.hero, tex, 0.90, 1.30, {
        billboard = "yaw", anchor = 0.0, lit = true})
    dse.ecs.set_sprite3d_lit(G.hero, true)
    dse.ecs.set_sprite3d_color_tint(G.hero, 1.0, 1.0, 1.0, 1.0)

    local cam = dse.ecs.create_entity()
    dse.ecs.add_transform(cam, 0.0, 0.9, 0.0, 1, 1, 1)
    dse.ecs.set_transform_rotation(cam, 0, 0, 0)
    dse.ecs.add_camera_3d(cam, 30.0, 0)

    local dir = dse.ecs.create_entity()
    dse.ecs.add_transform(dir, 0, 6, -5, 1, 1, 1)
    dse.ecs.add_directional_light_3d(dir, 0.3, -0.9, 0.2,
                                     1.0, 0.98, 0.95, 0.45, 0.18, 0.0)

    if os.getenv("DSE_LIT_POINT") ~= "0" then
        local p = dse.ecs.create_entity()
        dse.ecs.add_transform(p, 0.0, 1.0, -4.0, 1, 1, 1)
        dse.ecs.add_point_light_3d(p, 1.0, 0.78, 0.45, 4.0, 6.0)
        print("[m3-lit] point light ON")
    else
        print("[m3-lit] point light OFF")
    end
end

function Update(dt)
    local _ = dt
end