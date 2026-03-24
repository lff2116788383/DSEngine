local Phase1MVP = {}

local initialized = false
local box_entity = 0
local frame_counter = 0

function Phase1MVP.Setup(config)
    if initialized then return end

    -- 1. Camera
    local camera = DSE_CreateEntity()
    DSE_AddTransform(camera, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0)
    DSE_AddCamera(camera, 10.0)

    -- Load Textures
    local box_tex = DSE_LoadTexture("mirror_assets/Resources/item/1.png")
    if box_tex == 0 then box_tex = DSE_LoadTexture("data/mirror_assets/Resources/item/1.png") end

    local ground_tex = DSE_LoadTexture("mirror_assets/Resources/map/0.png")
    if ground_tex == 0 then ground_tex = DSE_LoadTexture("data/mirror_assets/Resources/map/0.png") end

    local ui_tex = DSE_LoadTexture("mirror_assets/Resources/ui/1.png")
    if ui_tex == 0 then ui_tex = DSE_LoadTexture("data/mirror_assets/Resources/ui/1.png") end
    local font_tex = DSE_LoadTexture("font/bitmap_font.png")
    if font_tex == 0 then font_tex = DSE_LoadTexture("data/font/bitmap_font.png") end

    -- 2. Tilemap (Ground)
    local tilemap = DSE_CreateEntity()
    DSE_AddTransform(tilemap, 0.0, -4.0, 0.0, 1.0, 1.0, 1.0)
    DSE_AddTilemap(tilemap, 10, 2, 1.0, ground_tex)
    for x = 0, 9 do
        for y = 0, 1 do
            DSE_SetTile(tilemap, x, y, 0)
        end
    end

    -- Add a big static collider for the tilemap area so boxes don't fall through
    local ground_collider = DSE_CreateEntity()
    DSE_AddTransform(ground_collider, 0.0, -3.5, 0.0, 1.0, 1.0, 1.0)
    DSE_AddRigidBody(ground_collider, 0, 0.0, 1) -- Static
    DSE_AddBoxCollider(ground_collider, 10.0, 2.0, 1.0, 0.5, 0.0)

    -- 3. Physics & Sprite & Audio (Dropping Box)
    box_entity = DSE_CreateEntity()
    DSE_AddTransform(box_entity, 0.0, 5.0, 0.0, 1.0, 1.0, 1.0)
    DSE_AddSprite(box_entity, 1.0, 1.0, 1.0, 1.0, 10, box_tex)
    DSE_AddRigidBody(box_entity, 2, 1.0, 0) -- Dynamic
    DSE_AddBoxCollider(box_entity, 1.0, 1.0, 1.0, 0.3, 0.5)
    -- 4. UI
    local ui_entity = DSE_CreateEntity()
    DSE_AddTransform(ui_entity, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0)
    DSE_AddUIRenderer(ui_entity, ui_tex, 0.2, 0.2, 0.8, 0.5, 100)
    DSE_AddUILabel(ui_entity, "Phase 1 MVP", font_tex, 1.0, 1.0, 1.0, 1.0, 8.0, 12.0, 0.0, 16, 6, 32, 0.0, 0.0)

    -- 5. Animation
    local anim_entity = DSE_CreateEntity()
    DSE_AddTransform(anim_entity, -2.0, 0.0, 0.0, 2.0, 2.0, 1.0)
    DSE_AddSprite(anim_entity, 1.0, 1.0, 1.0, 1.0, 5, 0)
    DSE_AddAnimator(anim_entity)
    -- Provide a table of texture handles
    DSE_AddAnimationState(anim_entity, "idle", 2.0, true, {box_tex, ground_tex})
    DSE_PlayAnimation(anim_entity, "idle")

    initialized = true
    print("[Phase1-MVP] Setup completed successfully.")
end

function Phase1MVP.Update(delta_time)
    frame_counter = frame_counter + 1
    if frame_counter % 60 == 0 then
        local dc = DSE_GetDrawCalls()
        local sprites = DSE_GetSpriteCount()
        print(string.format("[Phase1-MVP] Update running... draw_calls=%d sprites=%d", dc, sprites))
    end
end

return Phase1MVP
