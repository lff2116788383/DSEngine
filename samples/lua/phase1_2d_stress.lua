local Runtime2DStress = {}

local initialized = false
local frame_counter = 0
local spawn_index = 0
local texture_handle = 0
local settings = {
    total_boxes = 1024,
    spawn_per_frame = 64,
    columns = 64,
    spacing = 0.55,
    start_y = 2.0,
    box_scale = 0.45,
    camera_ortho_size = 12.0
}

local function apply_config(config)
    if type(config) ~= "table" then
        return
    end
    if type(config.total_boxes) == "number" and config.total_boxes > 0 then
        settings.total_boxes = math.floor(config.total_boxes)
    end
    if type(config.spawn_per_frame) == "number" and config.spawn_per_frame > 0 then
        settings.spawn_per_frame = math.floor(config.spawn_per_frame)
    end
    if type(config.columns) == "number" and config.columns > 0 then
        settings.columns = math.floor(config.columns)
    end
    if type(config.spacing) == "number" and config.spacing > 0 then
        settings.spacing = config.spacing
    end
    if type(config.start_y) == "number" then
        settings.start_y = config.start_y
    end
    if type(config.box_scale) == "number" and config.box_scale > 0 then
        settings.box_scale = config.box_scale
    end
    if type(config.camera_ortho_size) == "number" and config.camera_ortho_size > 0 then
        settings.camera_ortho_size = config.camera_ortho_size
    end
end

local function spawn_one_box(i)
    local start_x = -0.5 * (settings.columns - 1) * settings.spacing
    local e = dse.ecs.create_entity()
    local x = start_x + (i % settings.columns) * settings.spacing
    local y = settings.start_y + math.floor(i / settings.columns) * settings.spacing
    dse.ecs.add_transform(e, x, y, 0.0, settings.box_scale, settings.box_scale, 1.0)
    dse.ecs.add_sprite(e, 0.9, 0.95, 1.0, 1.0, i, texture_handle)
    dse.ecs.add_rigid_body(e, 2, 1.0, 0)
    dse.ecs.add_box_collider(e, settings.box_scale, settings.box_scale, 1.0, 0.3, 0.5)
end

function Runtime2DStress.Setup(config)
    if initialized then
        return
    end
    apply_config(config)

    local camera = dse.ecs.create_entity()
    dse.ecs.add_transform(camera, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0)
    dse.ecs.add_camera(camera, settings.camera_ortho_size)

    texture_handle = dse.assets.load_texture("mirror_assets/Resources/item/1.png")
    if texture_handle == 0 then
        texture_handle = dse.assets.load_texture("data/mirror_assets/Resources/item/1.png")
    end

    local ground = dse.ecs.create_entity()
    dse.ecs.add_transform(ground, 0.0, -5.0, 0.0, 40.0, 1.0, 1.0)
    dse.ecs.add_sprite(ground, 0.3, 0.8, 0.3, 1.0, 0, texture_handle)
    dse.ecs.add_rigid_body(ground, 0, 0.0, 1)
    dse.ecs.add_box_collider(ground, 40.0, 1.0, 1.0, 0.4, 0.0)

    spawn_index = 0
    initialized = true
    print(string.format("[2D-Test] setup started: total_boxes=%d spawn_per_frame=%d", settings.total_boxes, settings.spawn_per_frame))
end

function Runtime2DStress.Update(delta_time)
    if spawn_index < settings.total_boxes then
        local remaining = settings.total_boxes - spawn_index
        local batch = settings.spawn_per_frame
        if batch > remaining then
            batch = remaining
        end
        for _ = 1, batch do
            spawn_one_box(spawn_index)
            spawn_index = spawn_index + 1
        end
        if spawn_index == settings.total_boxes then
            print(string.format("[2D-Test] setup finished: boxes=%d", settings.total_boxes))
        end
    end

    frame_counter = frame_counter + 1
    if frame_counter % 60 ~= 0 then
        return
    end

    local draw_calls = dse.metrics.get_draw_calls()
    local max_batch = dse.metrics.get_max_batch_sprites()
    local sprite_count = dse.metrics.get_sprite_count()
    local status = "PASS"
    if draw_calls > 9 then
        status = "FAIL"
    end
    print(string.format("[2D-Test] draw_calls=%d max_batch=%d sprites=%d spawned=%d/%d status=%s", draw_calls, max_batch, sprite_count, spawn_index, settings.total_boxes, status))
end

return Runtime2DStress
