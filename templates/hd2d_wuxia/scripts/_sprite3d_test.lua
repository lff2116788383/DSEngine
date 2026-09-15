-- ============================================================================
-- _sprite3d_test.lua  HD-2D M1 minimal demo
--
-- Scene: a ground box + a low house box + a 3D perspective camera + one
-- billboard hero sprite. The hero is moved from in front of the house to
-- behind it using the demo clock. The low house height is intentional: when
-- the hero is behind, the lower red cloak is depth-occluded while the upper
-- cloak remains visible above the house rectangle, which is exactly the
-- M1 pixel gate (inside house rect = 0 character pixels, outside > 0).
--
-- Screenshot controls are the engine-standard env vars:
--   DSE_MAX_FRAMES, DSE_SCREENSHOT_FRAME, DSE_SCREENSHOT_PATH
-- Set DSE_SPRITE3D_FRONT=1 to pin the hero in front for the comparison image.
-- ============================================================================

local G = {
    elapsed = 0.0,
    frame = 0,
    hero = 0,
    hero_z = -3.0,
}

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
    -- Double-sided keeps the test robust to face winding; M1 is about depth,
    -- not culling.
    dse.ecs.set_mesh_material(e, 0.0, 0.9, 1.0, 0.0, 0.0, 0.0, 1.0, true, true)
    return e
end

function Awake()
    -- Ground: a thin box so either winding is visible.
    add_box(0, -0.10, -5.0, 20, 0.20, 20, 0.20, 0.45, 0.22, 1.0)

    -- Low house box. Height 0.65 is chosen against the 1.3-tall hero so the
    -- lower cloak is occluded while the upper cloak peeks above the roof.
    add_box(0, 0.325, -5.0, 2.6, 0.65, 2.6, 0.12, 0.20, 0.75, 1.0)

    -- Perspective camera looking down -Z (engine default camera forward).
    local cam = dse.ecs.create_entity()
    dse.ecs.add_transform(cam, 0.0, 0.90, 0.0, 1, 1, 1)
    dse.ecs.set_transform_rotation(cam, 0.0, 0.0, 0.0)
    dse.ecs.add_camera_3d(cam, 30.0, 0)

    -- Simple key + ambient lighting for MESH_LIT boxes. The Sprite3D path is
    -- unlit in M1, so the hero stays a crisp red for pixel counting.
    local dlight = dse.ecs.create_entity()
    dse.ecs.add_transform(dlight, 0, 6, -5, 1, 1, 1)
    dse.ecs.add_directional_light_3d(dlight, 0.3, -0.9, 0.2,
                                     1.0, 0.98, 0.92, 1.4, 0.35, 0.0)

    local ambient = dse.ecs.create_entity()
    dse.ecs.add_transform(ambient, 0, 3, -4, 1, 1, 1)
    dse.ecs.add_point_light_3d(ambient, 0.35, 0.35, 0.45, 0.8, 18.0)

    -- Billboard hero. Nearest sampling keeps the red cloak near (172,58,52)
    -- so the screenshot analysis can count it exactly.
    local tex = dse.assets.load_texture_ex(
        "templates/hd2d_wuxia/assets/char/hero/hero_d_idle_0.png", "nearest", "clamp")
    G.hero = dse.ecs.create_entity()
    dse.ecs.add_transform(G.hero, 0.0, 0.0, G.hero_z, 1, 1, 1)
    dse.ecs.add_sprite3d(G.hero, tex, 0.90, 1.30, {
        billboard = "yaw",
        anchor = 0.0,
        lit = false,
    })

    print(string.format("[sprite3d] Awake hero=%s tex=%d size=%.2fx%.2f",
                        tostring(G.hero), tex, 0.90, 1.30))
end

function Update(dt)
    G.frame = G.frame + 1
    G.elapsed = G.elapsed + dt

    local front_only = os.getenv("DSE_SPRITE3D_FRONT") == "1"
    local behind_only = os.getenv("DSE_SPRITE3D_BEHIND") == "1"
    if front_only then
        G.hero_z = -3.0
    elseif behind_only then
        G.hero_z = -7.0
    else
        -- Hold in front for ~1.5s, then walk behind over ~2.0s.
        local t = math.min(1.0, math.max(0.0, (G.elapsed - 1.5) / 2.0))
        G.hero_z = -3.0 + (-7.0 - -3.0) * t
    end
    dse.ecs.set_transform_position(G.hero, 0.0, 0.0, G.hero_z)

    if G.frame % 60 == 0 then
        print(string.format("[sprite3d] frame=%d z=%.2f draw_calls=%d sprites(metric)=%d",
                            G.frame, G.hero_z,
                            dse.metrics.get_draw_calls(),
                            dse.metrics.get_sprite_count()))
    end
end
