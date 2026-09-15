-- ============================================================================
-- _sprite3d_perf_test.lua  HD-2D M2 sorting/batching probe
--
-- 1000 Sprite3D billboards sharing one texture + several 3D boxes.
-- The M2 sort key (depth_bucket, texture, blend) keeps all same-texture
-- sprites contiguous, so the shared SpriteBatchRenderer 3D path should need
-- far fewer than 100 draw calls. dse.metrics.get_draw_calls() is printed.
-- ============================================================================

local SPRITE_COUNT = 1000
local sprites = {}
local frame_count = 0

local function add_box(x, y, z, sx, sy, sz, r, g, b)
    local e = dse.ecs.create_entity()
    dse.ecs.add_transform(e, x, y, z, sx, sy, sz)
    local v = {
        -0.5,-0.5,-0.5,  0.5,-0.5,-0.5,  0.5,0.5,-0.5, -0.5,0.5,-0.5,
        -0.5,-0.5, 0.5,  0.5,-0.5, 0.5,  0.5,0.5, 0.5, -0.5,0.5, 0.5,
    }
    local i = {4,5,6, 4,6,7, 1,0,3, 1,3,2, 0,4,7, 0,7,3, 5,1,2, 5,2,6, 0,1,5, 0,5,4, 3,7,6, 3,6,2}
    dse.ecs.add_mesh_renderer(e, r, g, b, 1.0, v, i)
    dse.ecs.set_mesh_shader_variant(e, "MESH_LIT")
    dse.ecs.set_mesh_material(e, 0.0, 0.9, 1.0, 0.0, 0.0, 0.0, 1.0, true, true)
    return e
end

function Awake()
    add_box(0, -0.10, -10.0, 14, 0.20, 14, 0.20, 0.45, 0.22)

    -- A few occluders placed across the sprite grid.
    for ix = -1, 1 do
        for iz = -1, 1 do
            add_box(ix * 2.5, 0.55, -10.0 + iz * 2.0, 1.5, 1.1, 1.5,
                    0.15 + 0.05 * ix, 0.25, 0.70 - 0.05 * iz)
        end
    end

    local cam = dse.ecs.create_entity()
    dse.ecs.add_transform(cam, 0.0, 2.0, 0.0, 1, 1, 1)
    dse.ecs.set_transform_rotation(cam, 0.0, 0.0, 0.0)
    dse.ecs.add_camera_3d(cam, 38.0, 0)

    local dlight = dse.ecs.create_entity()
    dse.ecs.add_transform(dlight, 0, 8, -10, 1, 1, 1)
    dse.ecs.add_directional_light_3d(dlight, 0.3, -0.9, 0.2,
                                     1.0, 0.98, 0.92, 1.3, 0.30, 0.0)

    local tex = dse.assets.load_texture_ex(
        "templates/hd2d_wuxia/assets/char/hero/hero_d_idle_0.png", "nearest", "clamp")

    local n = 0
    for iz = 0, 31 do
        for ix = 0, 31 do
            if n >= SPRITE_COUNT then break end
            local x = -5.0 + ix * (10.0 / 31.0)
            local z = -13.0 + iz * (6.0 / 31.0)
            local e = dse.ecs.create_entity()
            dse.ecs.add_transform(e, x, 0.0, z, 1, 1, 1)
            -- Every 17th sprite gets a foreground sorting_bias, exercising M2.
            local bias = (n % 17 == 0) and -0.25 or 0.0
            dse.ecs.add_sprite3d(e, tex, 0.30, 0.42, {
                billboard = "yaw",
                anchor = 0.0,
                lit = false,
                sorting_bias = bias,
            })
            sprites[#sprites + 1] = e
            n = n + 1
        end
    end

    print(string.format("[sprite3d-perf] Awake sprites=%d boxes=10 draw_calls=%d metric_sprites=%d",
                        #sprites, dse.metrics.get_draw_calls(), dse.metrics.get_sprite_count()))
end

function Update(dt)
    local _ = dt
    frame_count = frame_count + 1
    if frame_count % 60 == 0 then
        print(string.format("[sprite3d-perf] frame=%d sprites=%d draw_calls=%d metric_sprites=%d",
                            frame_count, #sprites,
                            dse.metrics.get_draw_calls(),
                            dse.metrics.get_sprite_count()))
    end
end
