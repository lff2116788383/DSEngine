local Runtime2DMVP = {}

local initialized = false
local box_entity = 0
local audio_entity = 0
local frame_counter = 0

function Runtime2DMVP.Setup(config)
    if initialized then return end

    -- 1. Camera
    local camera = dse.ecs.create_entity()
    dse.ecs.add_transform(camera, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0)
    dse.ecs.add_camera(camera, 10.0)

    -- Load Textures
    local box_tex = dse.assets.load_texture("mirror_assets/Resources/item/1.png")
    if box_tex == 0 then box_tex = dse.assets.load_texture("data/mirror_assets/Resources/item/1.png") end

    local ground_tex = dse.assets.load_texture("mirror_assets/Resources/map/0.png")
    if ground_tex == 0 then ground_tex = dse.assets.load_texture("data/mirror_assets/Resources/map/0.png") end

    local ui_tex = dse.assets.load_texture("mirror_assets/Resources/ui/1.png")
    if ui_tex == 0 then ui_tex = dse.assets.load_texture("data/mirror_assets/Resources/ui/1.png") end
    local font_tex = dse.assets.load_texture("font/bitmap_font.png")
    if font_tex == 0 then font_tex = dse.assets.load_texture("data/font/bitmap_font.png") end
    local bgm_path = "audio/phase1_bgm.wav"

    -- 2. Tilemap (Ground)
    local tilemap = dse.ecs.create_entity()
    dse.ecs.add_transform(tilemap, 0.0, -4.0, 0.0, 1.0, 1.0, 1.0)
    dse.ecs.add_tilemap(tilemap, 10, 2, 1.0, ground_tex)
    for x = 0, 9 do
        for y = 0, 1 do
            dse.ecs.set_tile(tilemap, x, y, 0)
        end
    end

    -- Add a big static collider for the tilemap area so boxes don't fall through
    local ground_collider = dse.ecs.create_entity()
    dse.ecs.add_transform(ground_collider, 0.0, -3.5, 0.0, 1.0, 1.0, 1.0)
    dse.ecs.add_rigid_body(ground_collider, 0, 0.0, 1)
    dse.ecs.add_box_collider(ground_collider, 10.0, 2.0, 1.0, 0.5, 0.0)

    -- 3. Physics & Sprite & Audio (Dropping Box)
    box_entity = dse.ecs.create_entity()
    dse.ecs.add_transform(box_entity, 0.0, 5.0, 0.0, 1.0, 1.0, 1.0)
    dse.ecs.add_sprite(box_entity, 1.0, 1.0, 1.0, 1.0, 10, box_tex)
    dse.ecs.add_rigid_body(box_entity, 2, 1.0, 0)
    dse.ecs.add_box_collider(box_entity, 1.0, 1.0, 1.0, 0.3, 0.5)
    -- 4. UI
    local ui_entity = dse.ecs.create_entity()
    dse.ecs.add_transform(ui_entity, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0)
    dse.ui.add_renderer(ui_entity, ui_tex, 0.2, 0.2, 0.8, 0.5, 100)
    dse.ui.add_label(ui_entity, "Phase 1 MVP", font_tex, 1.0, 1.0, 1.0, 1.0, 8.0, 12.0, 0.0, 16, 6, 32, 0.0, 0.0)
    dse.ui.add_panel(ui_entity, true)
    dse.ui.add_button(ui_entity, 0.2, 0.2, 0.8, 1.0)

    audio_entity = dse.ecs.create_entity()
    dse.ecs.add_transform(audio_entity, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0)
    dse.audio.add_source(audio_entity, bgm_path, true, true, 0.5)
    dse.audio.set_playing(audio_entity, true)
    print(string.format("[2D-MVP] audio source initialized: %s", bgm_path))

    -- 5. Animation
    local anim_entity = dse.ecs.create_entity()
    dse.ecs.add_transform(anim_entity, -2.0, 0.0, 0.0, 2.0, 2.0, 1.0)
    dse.ecs.add_sprite(anim_entity, 1.0, 1.0, 1.0, 1.0, 5, 0)
    dse.ecs.add_animator(anim_entity)
    dse.ecs.add_animation_state(anim_entity, "idle", 2.0, true, {box_tex, ground_tex})
    dse.ecs.play_animation(anim_entity, "idle")

    initialized = true
    print("[2D-MVP] Setup completed successfully.")
end

function Runtime2DMVP.Update(delta_time)
    frame_counter = frame_counter + 1
    if frame_counter % 60 == 0 then
        local dc = dse.metrics.get_draw_calls()
        local sprites = dse.metrics.get_sprite_count()
        print(string.format("[2D-MVP] Update running... draw_calls=%d sprites=%d audio_entity=%d", dc, sprites, audio_entity))
    end
end

return Runtime2DMVP
