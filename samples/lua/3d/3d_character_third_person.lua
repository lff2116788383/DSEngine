-- 3D P2 sample: third-person character showcase
-- 目标：展示第三人称跟随相机、角色移动/转向/攻击状态；当前用 cube character rig 作为角色资源 fallback。
local CharacterThirdPerson3D = {}

local state = { camera = nil, character = nil, parts = {}, time = 0.0, mode = "run", logged_mode = "" }

local function cube_vertices()
    return { -0.5,-0.5,0.5, 0.5,-0.5,0.5, 0.5,0.5,0.5, -0.5,0.5,0.5, -0.5,-0.5,-0.5, 0.5,-0.5,-0.5, 0.5,0.5,-0.5, -0.5,0.5,-0.5 }
end

local function cube_indices()
    return { 0,1,2,2,3,0, 1,5,6,6,2,1, 5,4,7,7,6,5, 4,0,3,3,7,4, 3,2,6,6,7,3, 4,5,1,1,0,4 }
end

local function add_cube(name, x, y, z, sx, sy, sz, color, emissive)
    local e = dse.ecs.create_entity()
    dse.ecs.add_transform(e, x, y, z, sx, sy, sz)
    dse.ecs.add_mesh_renderer(e, color[1], color[2], color[3], color[4] or 1.0, cube_vertices(), cube_indices())
    dse.ecs.set_mesh_shader_variant(e, "MESH_LIT")
    dse.ecs.set_mesh_material(e, 0.0, 0.50, 1.0, emissive and emissive[1] or 0.0, emissive and emissive[2] or 0.0, emissive and emissive[3] or 0.0, 1.0, true, true)
    return e
end

local function add_part(name, lx, ly, lz, sx, sy, sz, color)
    local e = add_cube(name, lx, ly, lz, sx, sy, sz, color)
    table.insert(state.parts, { name = name, entity = e, lx = lx, ly = ly, lz = lz, sx = sx, sy = sy, sz = sz })
    return e
end

local function setup_camera(config)
    local camera = dse.ecs.create_entity()
    local distance = (type(config) == "table" and type(config.camera_distance) == "number") and config.camera_distance or 10.5
    dse.ecs.add_transform(camera, 0.0, 3.3, distance, 1.0, 1.0, 1.0)
    dse.ecs.set_transform_rotation(camera, -18.0, 0.0, 0.0)
    dse.ecs.add_camera_3d(camera, 60.0, 100)
    state.camera = camera
end

local function setup_scene(config)
    local light = dse.ecs.create_entity()
    dse.ecs.add_directional_light_3d(light, -0.42, -1.0, -0.25, 1.0, 0.94, 0.86, 1.2, 0.20, 0.35)
    add_cube("ground", 0.0, -0.58, 0.0, 11.0, 0.12, 7.0, {0.32, 0.38, 0.36, 1.0})
    add_cube("path_a", -2.8, -0.46, -1.6, 0.9, 0.04, 0.9, {0.18, 0.55, 0.30, 1.0}, {0.02, 0.16, 0.04})
    add_cube("path_b", 2.8, -0.46, 1.6, 0.9, 0.04, 0.9, {0.65, 0.44, 0.18, 1.0}, {0.16, 0.08, 0.02})

    local character = dse.ecs.create_entity()
    dse.ecs.add_transform(character, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0)
    dse.ecs.add_animator_3d(character, config.danim_path or "", config.dskel_path or "")
    dse.ecs.init_animator_3d_fsm(character)
    dse.ecs.add_animator_3d_state(character, "run", config.danim_path or "", true, 1.0)
    dse.ecs.add_animator_3d_state(character, "attack", config.danim_path or "", false, 1.4)
    dse.ecs.set_animator_3d_state(character, "run", 1.0, true)
    state.character = character

    add_part("body", 0.0, 0.85, 0.0, 0.56, 0.78, 0.34, {0.18, 0.42, 0.90, 1.0})
    add_part("head", 0.0, 1.48, 0.0, 0.34, 0.34, 0.34, {0.94, 0.78, 0.58, 1.0})
    add_part("arm_l", -0.44, 0.88, 0.0, 0.18, 0.64, 0.20, {0.72, 0.84, 1.0, 1.0})
    add_part("arm_r", 0.44, 0.88, 0.0, 0.18, 0.64, 0.20, {0.72, 0.84, 1.0, 1.0})
    add_part("leg_l", -0.18, 0.14, 0.0, 0.18, 0.64, 0.22, {0.12, 0.24, 0.58, 1.0})
    add_part("leg_r", 0.18, 0.14, 0.0, 0.18, 0.64, 0.22, {0.12, 0.24, 0.58, 1.0})
    add_part("weapon", 0.72, 0.95, 0.0, 0.12, 0.90, 0.12, {0.92, 0.86, 0.38, 1.0})
    print("[3D][Character] setup: third-person follow camera + cube character rig fallback. States: run/attack.")
end

local function apply_part_transform(root_x, root_y, root_z, root_yaw, phase)
    local attack = state.mode == "attack"
    for _, p in ipairs(state.parts) do
        local lx, ly, lz = p.lx, p.ly, p.lz
        local rx, ry, rz = 0.0, root_yaw, 0.0
        local swing = math.sin(phase)
        if p.name == "arm_l" then rz = attack and -70.0 or swing * 26.0 end
        if p.name == "arm_r" then rz = attack and 72.0 or -swing * 26.0 end
        if p.name == "weapon" then rz = attack and 82.0 or -swing * 18.0 end
        if p.name == "leg_l" then rz = -swing * 20.0 end
        if p.name == "leg_r" then rz = swing * 20.0 end
        if p.name == "head" then ry = root_yaw + math.sin(phase * 0.5) * 6.0 end
        dse.ecs.set_transform_position(p.entity, root_x + lx, root_y + ly, root_z + lz)
        dse.ecs.set_transform_rotation(p.entity, rx, ry, rz)
    end
end

function CharacterThirdPerson3D.Setup(config)
    setup_camera(config or {})
    setup_scene(config or {})
end

function CharacterThirdPerson3D.Update(delta_time)
    local dt = delta_time or 0.0
    if dt > 0.1 then dt = 0.1 end
    state.time = state.time + dt
    local mode_cycle = math.floor(state.time / 3.0) % 3
    state.mode = (mode_cycle == 2) and "attack" or "run"
    local radius = 2.25
    local root_x = math.cos(state.time * 0.55) * radius
    local root_z = math.sin(state.time * 0.55) * radius * 0.72
    local root_yaw = -state.time * 31.5 + 90.0
    if state.mode == "attack" then root_x = root_x * 0.55; root_z = root_z * 0.55 end
    if state.character ~= nil then
        dse.ecs.set_transform_position(state.character, root_x, 0.0, root_z)
        dse.ecs.set_transform_rotation(state.character, 0.0, root_yaw, 0.0)
        dse.ecs.set_animator_3d_state(state.character, state.mode, state.mode == "attack" and 1.4 or 1.0, state.mode ~= "attack")
    end
    apply_part_transform(root_x, 0.0, root_z, root_yaw, state.time * 5.5)
    if state.camera ~= nil then
        local cam_x = root_x - math.cos(state.time * 0.55) * 4.0
        local cam_z = root_z + 6.8
        dse.ecs.set_transform_position(state.camera, cam_x, 3.2, cam_z)
        dse.ecs.set_transform_rotation(state.camera, -19.0, math.sin(state.time * 0.35) * 5.0, 0.0)
    end
    if state.logged_mode ~= state.mode then
        state.logged_mode = state.mode
        print(string.format("[3D][Character] state=%s speed=%.2f pos=(%.2f,%.2f)", state.mode, state.mode == "attack" and 0.0 or 1.0, root_x, root_z))
    end
end

return CharacterThirdPerson3D
