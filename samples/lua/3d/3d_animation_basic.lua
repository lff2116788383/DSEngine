-- 3D P2 sample: animation basic showcase
-- 目标：验证 Animator3D/FSM Lua 入口；优先加载 data/animation/minimal_rig 最小真实资源，分段 cube rig 保留为截图兜底。
local AnimationBasic3D = {}


AnimationBasic3D._meta = {
    name     = "animation basic showcase",
    category = "animation",
    config   = { camera_distance=8.5,
    mesh_path="animation/minimal_rig/two_bone.dmesh",
    material_path="animation/minimal_rig/two_bone.dmat",
    danim_path="animation/minimal_rig/two_bone_idle_walk.danim",
    dskel_path="animation/minimal_rig/two_bone.dskel" },
}

local state = { camera = nil, actor = nil, skinned_mesh = nil, bones = {}, time = 0.0, current_state = "idle", next_switch = 3.0, logged_state = "", resources = {} }

local function cube_vertices()
    return { -0.5,-0.5,0.5, 0.5,-0.5,0.5, 0.5,0.5,0.5, -0.5,0.5,0.5, -0.5,-0.5,-0.5, 0.5,-0.5,-0.5, 0.5,0.5,-0.5, -0.5,0.5,-0.5 }
end

local function cube_indices()
    return { 0,1,2,2,3,0, 1,5,6,6,2,1, 5,4,7,7,6,5, 4,0,3,3,7,4, 3,2,6,6,7,3, 4,5,1,1,0,4 }
end

local function add_part(name, x, y, z, sx, sy, sz, color, emissive)
    local e = dse.ecs.create_entity()
    dse.ecs.add_transform(e, x, y, z, sx, sy, sz)
    dse.ecs.add_mesh_renderer(e, color[1], color[2], color[3], color[4] or 1.0, cube_vertices(), cube_indices())
    dse.ecs.set_mesh_shader_variant(e, "MESH_LIT")
    dse.ecs.set_mesh_material(e, 0.0, 0.52, 1.0, emissive and emissive[1] or 0.0, emissive and emissive[2] or 0.0, emissive and emissive[3] or 0.0, 1.0, true, true)
    table.insert(state.bones, { name = name, entity = e, x = x, y = y, z = z, sx = sx, sy = sy, sz = sz })
    return e
end

local function setup_camera(config)
    local camera = dse.ecs.create_entity()
    local distance = (type(config) == "table" and type(config.camera_distance) == "number") and config.camera_distance or 8.5
    dse.ecs.add_transform(camera, 0.0, 2.8, distance, 1.0, 1.0, 1.0)
    dse.ecs.set_transform_rotation(camera, -18.0, 0.0, 0.0)
    dse.ecs.add_camera_3d(camera, 60.0, 100)
    if Ecs and Ecs.add_free_camera_controller then Ecs.add_free_camera_controller(camera, 5.5, 0.12) elseif dse.ecs.add_free_camera_controller then dse.ecs.add_free_camera_controller(camera, 5.5, 0.12) end
    state.camera = camera
end

local function setup_actor(config)
    local light = dse.ecs.create_entity()
    dse.ecs.add_directional_light_3d(light, -0.35, -1.0, -0.32, 1.0, 0.94, 0.86, 1.2, 0.18, 0.35)
    add_part("ground", 0.0, -0.58, 0.0, 7.0, 0.12, 4.5, {0.34, 0.38, 0.40, 1.0})

    local actor = dse.ecs.create_entity()
    dse.ecs.add_transform(actor, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0)
    local danim = (type(config) == "table" and type(config.danim_path) == "string") and config.danim_path or ""
    local dskel = (type(config) == "table" and type(config.dskel_path) == "string") and config.dskel_path or ""
    local mesh_path = (type(config) == "table" and type(config.mesh_path) == "string") and config.mesh_path or ""
    local material_path = (type(config) == "table" and type(config.material_path) == "string") and config.material_path or ""
    state.resources = { mesh_path = mesh_path, material_path = material_path, danim_path = danim, dskel_path = dskel }
    dse.ecs.add_animator_3d(actor, danim, dskel)
    dse.ecs.init_animator_3d_fsm(actor)
    dse.ecs.add_animator_3d_state(actor, "idle", danim, true, 1.0)
    dse.ecs.add_animator_3d_state(actor, "walk", danim, true, 1.15)
    dse.ecs.set_animator_3d_state(actor, "idle", 1.0, true)
    state.actor = actor

    if mesh_path ~= "" then
        local skinned = dse.ecs.create_entity()
        dse.ecs.add_transform(skinned, -1.25, -0.05, 0.0, 1.25, 1.25, 1.25)
        dse.ecs.add_mesh_renderer(skinned, 0.35, 0.75, 1.0, 1.0)
        dse.ecs.set_mesh_path(skinned, mesh_path)
        if material_path ~= "" then
            dse.ecs.set_mesh_material(skinned, material_path)
        else
            dse.ecs.set_mesh_material(skinned, 0.0, 0.45, 1.0, 0.02, 0.08, 0.12, 1.0, true, true)
        end
        dse.ecs.set_mesh_shader_variant(skinned, "MESH_LIT")
        dse.ecs.add_animator_3d(skinned, danim, dskel)
        dse.ecs.init_animator_3d_fsm(skinned)
        dse.ecs.add_animator_3d_state(skinned, "idle", danim, true, 1.0)
        dse.ecs.add_animator_3d_state(skinned, "walk", danim, true, 1.15)
        dse.ecs.set_animator_3d_state(skinned, "idle", 1.0, true)
        state.skinned_mesh = skinned
    end

    add_part("hips", 0.0, 0.52, 0.0, 0.62, 0.32, 0.38, {0.42, 0.70, 1.0, 1.0})
    add_part("spine", 0.0, 1.08, 0.0, 0.52, 0.72, 0.34, {0.32, 0.58, 0.95, 1.0})
    add_part("head", 0.0, 1.70, 0.0, 0.38, 0.38, 0.38, {0.94, 0.82, 0.62, 1.0})
    add_part("arm_l", -0.48, 1.12, 0.0, 0.20, 0.72, 0.22, {0.75, 0.88, 1.0, 1.0})
    add_part("arm_r", 0.48, 1.12, 0.0, 0.20, 0.72, 0.22, {0.75, 0.88, 1.0, 1.0})
    add_part("leg_l", -0.22, 0.05, 0.0, 0.22, 0.72, 0.24, {0.20, 0.36, 0.70, 1.0})
    add_part("leg_r", 0.22, 0.05, 0.0, 0.22, 0.72, 0.24, {0.20, 0.36, 0.70, 1.0})
    add_part("state_beacon", 1.6, 1.2, 0.0, 0.28, 0.28, 0.28, {0.2, 1.0, 0.45, 1.0}, {0.04, 0.45, 0.08})
    local state_ok, current_state, normalized_time, clip_time, speed, loop, transitioning, bone_count, has_skeleton = dse.ecs.get_animator_3d_state(actor)
    print(string.format("[3D][Animation] setup: animator_resource_chain real_animation_resource mesh_path=%s material_path=%s danim_path=%s dskel_path=%s resource_paths_configured=%s; animator_state_api get_animator_3d_state=%s state=%s normalized_time=%.2f clip_time=%.2f speed=%.2f loop=%s transitioning=%s final_bones=%s has_skeleton=%s; fallback cube rig retained", mesh_path, material_path, danim, dskel, tostring(danim ~= "" and dskel ~= ""), tostring(state_ok == true), tostring(current_state), normalized_time or -1.0, clip_time or -1.0, speed or -1.0, tostring(loop == true), tostring(transitioning == true), tostring(bone_count), tostring(has_skeleton == true)))
    if state.skinned_mesh ~= nil then
        local mesh_ok, mesh_state, mesh_norm, mesh_clip, mesh_speed, mesh_loop, mesh_transition, mesh_bones, mesh_has_skeleton = dse.ecs.get_animator_3d_state(state.skinned_mesh)
        print(string.format("[3D][Animation] skinned_mesh_resource animator_state_api get_animator_3d_state=%s state=%s normalized_time=%.2f clip_time=%.2f speed=%.2f loop=%s transitioning=%s final_bones=%s has_skeleton=%s mesh_path=%s material_path=%s", tostring(mesh_ok == true), tostring(mesh_state), mesh_norm or -1.0, mesh_clip or -1.0, mesh_speed or -1.0, tostring(mesh_loop == true), tostring(mesh_transition == true), tostring(mesh_bones), tostring(mesh_has_skeleton == true), mesh_path, material_path))
    end
end

local function switch_state(name)
    state.current_state = name
    state.next_switch = state.time + 3.0
    if state.actor ~= nil then
        dse.ecs.set_animator_3d_state(state.actor, name, name == "walk" and 1.15 or 1.0, true)
        if name == "walk" then dse.ecs.set_animator_3d_param_float(state.actor, "speed", 1.0) else dse.ecs.set_animator_3d_param_float(state.actor, "speed", 0.0) end
        if state.skinned_mesh ~= nil then
            dse.ecs.set_animator_3d_state(state.skinned_mesh, name, name == "walk" and 1.15 or 1.0, true)
            if name == "walk" then dse.ecs.set_animator_3d_param_float(state.skinned_mesh, "speed", 1.0) else dse.ecs.set_animator_3d_param_float(state.skinned_mesh, "speed", 0.0) end
        end
        local ok, actual_state, normalized_time, clip_time, speed, loop, transitioning, bone_count, has_skeleton = dse.ecs.get_animator_3d_state(state.actor)
        print(string.format("[3D][Animation] animator_state_api real_animation_resource get_animator_3d_state=%s state=%s normalized_time=%.2f clip_time=%.2f speed=%.2f loop=%s transitioning=%s final_bones=%s has_skeleton=%s danim_path=%s dskel_path=%s", tostring(ok == true), tostring(actual_state), normalized_time or -1.0, clip_time or -1.0, speed or -1.0, tostring(loop == true), tostring(transitioning == true), tostring(bone_count), tostring(has_skeleton == true), state.resources.danim_path or "", state.resources.dskel_path or ""))
        return
    end
    print(string.format("[3D][Animation] state=%s normalized_time=0.00", name))
end

function AnimationBasic3D.Setup(config)
    setup_camera(config or {})
    setup_actor(config or {})
    switch_state("idle")
end

function AnimationBasic3D.Update(delta_time)
    local dt = delta_time or 0.0
    if dt > 0.1 then dt = 0.1 end
    state.time = state.time + dt
    if state.time >= state.next_switch then
        switch_state(state.current_state == "idle" and "walk" or "idle")
    end
    local phase = state.time * (state.current_state == "walk" and 5.0 or 2.0)
    local stride = state.current_state == "walk" and math.sin(phase) or math.sin(phase) * 0.18
    for _, b in ipairs(state.bones) do
        local x, y, z = b.x, b.y, b.z
        local rx, ry, rz = 0.0, 0.0, 0.0
        if b.name == "spine" then y = y + math.sin(phase) * 0.035 end
        if b.name == "head" then y = y + math.sin(phase + 0.8) * 0.04; ry = math.sin(phase * 0.5) * 8.0 end
        if b.name == "arm_l" then rz = stride * 28.0 end
        if b.name == "arm_r" then rz = -stride * 28.0 end
        if b.name == "leg_l" then rz = -stride * 22.0 end
        if b.name == "leg_r" then rz = stride * 22.0 end
        if b.name == "state_beacon" then
            x = b.x; y = b.y + math.sin(state.time * 3.0) * 0.12
            rx = state.time * 45.0; ry = state.time * 90.0
        end
        dse.ecs.set_transform_position(b.entity, x, y, z)
        dse.ecs.set_transform_rotation(b.entity, rx, ry, rz)
    end
    if state.actor ~= nil and state.time > 1.0 and state.logged_state ~= state.current_state then
        state.logged_state = state.current_state
        local ok, actual_state, normalized_time, clip_time, speed, loop, transitioning, bone_count, has_skeleton = dse.ecs.get_animator_3d_state(state.actor)
        local mesh_ok, mesh_state, mesh_norm, mesh_clip, mesh_speed, mesh_loop, mesh_transition, mesh_bones, mesh_has_skeleton = false, "none", -1.0, -1.0, -1.0, false, false, 0, false
        if state.skinned_mesh ~= nil then
            mesh_ok, mesh_state, mesh_norm, mesh_clip, mesh_speed, mesh_loop, mesh_transition, mesh_bones, mesh_has_skeleton = dse.ecs.get_animator_3d_state(state.skinned_mesh)
        end
        print(string.format("[3D][Animation] runtime: animator_state_api real_animation_resource get_animator_3d_state=%s state=%s normalized_time=%.2f clip_time=%.2f speed=%.2f loop=%s transitioning=%s final_bones=%s has_skeleton=%s mesh_state=%s mesh_final_bones=%s mesh_has_skeleton=%s mesh_norm=%.2f mesh_clip=%.2f", tostring(ok == true), tostring(actual_state), normalized_time or -1.0, clip_time or -1.0, speed or -1.0, tostring(loop == true), tostring(transitioning == true), tostring(bone_count), tostring(has_skeleton == true), tostring(mesh_state), tostring(mesh_bones), tostring(mesh_has_skeleton == true), mesh_norm or -1.0, mesh_clip or -1.0))
    end
end

return AnimationBasic3D
